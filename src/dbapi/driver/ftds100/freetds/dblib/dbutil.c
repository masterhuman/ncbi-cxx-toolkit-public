/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <freetds/tds.h>
#include <freetds/convert.h>
#include <sybdb.h>
#include <syberror.h>
#include <dblib.h>

/*
 * test include consistency 
 * I don't think all compiler are able to compile this code... if not comment it
 */
#if ENABLE_EXTRA_CHECKS

/* TODO test SYBxxx consistency */

#define TEST_ATTRIBUTE(t,sa,fa,sb,fb) \
	TDS_COMPILE_CHECK(t,sizeof(((sa*)0)->fa) == sizeof(((sb*)0)->fb) && TDS_OFFSET(sa,fa) == TDS_OFFSET(sb,fb))

TEST_ATTRIBUTE(t21,TDS_MONEY4,mny4,DBMONEY4,mny4);
TEST_ATTRIBUTE(t22,TDS_OLD_MONEY,mnyhigh,DBMONEY,mnyhigh);
TEST_ATTRIBUTE(t23,TDS_OLD_MONEY,mnylow,DBMONEY,mnylow);
TEST_ATTRIBUTE(t24,TDS_DATETIME,dtdays,DBDATETIME,dtdays);
TEST_ATTRIBUTE(t25,TDS_DATETIME,dttime,DBDATETIME,dttime);
TEST_ATTRIBUTE(t26,TDS_DATETIME4,days,DBDATETIME4,days);
TEST_ATTRIBUTE(t27,TDS_DATETIME4,minutes,DBDATETIME4,minutes);
TEST_ATTRIBUTE(t28,TDS_NUMERIC,precision,DBNUMERIC,precision);
TEST_ATTRIBUTE(t29,TDS_NUMERIC,scale,DBNUMERIC,scale);
TEST_ATTRIBUTE(t30,TDS_NUMERIC,array,DBNUMERIC,array);
#endif

/* 
 * The next 2 functions receive the info and error messages that come from the TDS layer.  
 * The address of this function is passed to the TDS layer in dbinit().  
 * It takes a pointer to a DBPROCESS, it's just that the TDS layer didn't 
 * know what it really was.  
 */
int
_dblib_handle_info_message(const TDSCONTEXT * tds_ctx, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	DBPROCESS *dbproc = (tds && tds_get_parent(tds))? (DBPROCESS *) tds_get_parent(tds) : NULL;

	tdsdump_log(TDS_DBG_FUNC, "_dblib_handle_info_message(%p, %p, %p)\n", tds_ctx, tds, msg);
	tdsdump_log(TDS_DBG_FUNC, "msgno %d: \"%s\"\n", msg->msgno, msg->message);

	/* 
	 * Check to see if the user supplied a function, else ignore the message. 
	 */
	if (_dblib_msg_handler) {
		_dblib_msg_handler(dbproc,
				   msg->msgno,
				   msg->state,
				   msg->severity, msg->message, msg->server, msg->proc_name, msg->line_number);
	}

	if (msg->severity > 10 && _dblib_err_handler) {	/* call the application's error handler, if installed. */
		/*
		 * Sybase docs say SYBESMSG is generated only in specific
		 * cases (severity greater than 16, or deadlock occurred, or
		 * a syntax error occurred.)  However, actual observed
		 * behavior is that SYBESMSG is always generated for
		 * server messages with severity greater than 10.
		 */
		/* Cannot call dbperror() here because server messsage numbers (and text) are not in its lookup table. */
		static const char message[] = "General SQL Server error: Check messages from the SQL Server";
		(*_dblib_err_handler)(dbproc, msg->severity, SYBESMSG, DBNOERR, (char *) message, NULL);
	}
	return TDS_SUCCESS;
}

/** \internal
 *  \dblib_internal
 *  \brief handle errors generated by libtds
 *  \param tds_ctx actually a dbproc: contains all information needed by db-lib to manage communications with the server.
 *  \param tds contains all information needed by libtds to manage communications with the server.
 *  \param msg the message to send
 *  \returns 
 *  \remarks This function is called by libtds via tds_ctx->err_handler.  It exists to convert the db-lib 
 *	error handler's return code into one that libtds will know  how to handle.  
 * 	libtds conveniently issues msgno's with the same values and meanings as db-lib and ODBC use.  
 *	(N.B. ODBC actually uses db-lib msgno numbers for its driver-specific "server errors".    
 */
/* 
 * Stack:
 *		libtds
 *			tds_ctx->err_handler (pointer to _dblib_handle_err_message)
 *			_dblib_handle_err_message
 *				dbperror
 *					client (or default) error handler
 *					returns db-lib defined return code INT_something.
 *				returns db-lib return code with Sybase semantics (adjusting client's code if need be)
 *			returns libtds return code
 *		decides what to do based on the universal libtds return code, thank you. 
 */	
int
_dblib_handle_err_message(const TDSCONTEXT * tds_ctx, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	DBPROCESS *dbproc = (tds && tds_get_parent(tds))? (DBPROCESS *) tds_get_parent(tds) : NULL;
	int rc = INT_CANCEL;

	assert(_dblib_err_handler);
	assert(msg);

	rc = dbperror(dbproc, msg->msgno, msg->oserr);

	/*
	 * Preprocess the return code to handle INT_TIMEOUT/INT_CONTINUE
	 * for non-SYBETIME errors in the strange and different ways as
	 * specified by Sybase and Microsoft.
	 */
	if (msg->msgno != SYBETIME) {
		switch (rc) {
		case INT_TIMEOUT:
			rc = INT_EXIT;
			break;
		case INT_CONTINUE:
			if (!dbproc || !dbproc->msdblib) {
				/* Sybase behavior */
				assert(0);  /* dbperror() should prevent */
				rc = INT_EXIT;
			} else {
				/* Microsoft behavior */
				rc = INT_CANCEL;
			}
			break;
		default:
			break;
		}
	}

	/*
	 * Convert db-lib return code to libtds return code, and return.
	 */
	switch (rc) {
	case INT_CANCEL:	return TDS_INT_CANCEL;
	case INT_TIMEOUT:	return TDS_INT_TIMEOUT;
	case INT_CONTINUE:	return TDS_INT_CONTINUE;
	
	case INT_EXIT:
		assert(0);  /* dbperror() should prevent */
	default:
		/* unknown return code from error handler */
		exit(EXIT_FAILURE);
		break;
	}

	/* not reached */
	assert(0);
	return TDS_INT_CANCEL;
}

/**
 * \ingroup dblib_internal
 * \brief check interrupts for libtds.
 * 
 * \param vdbproc a DBPROCESS pointer, contains all information needed by db-lib to manage communications with the server.
 * \sa DBDEAD(), dbsetinterrupt().
 */
int
_dblib_check_and_handle_interrupt(void * vdbproc)
{
	DBPROCESS * dbproc = (DBPROCESS*) vdbproc;
	int ret = INT_CONTINUE;

	assert( dbproc != NULL );

	if (dbproc->chkintr == NULL || dbproc->hndlintr == NULL)
		return INT_CONTINUE;
		
	tdsdump_log(TDS_DBG_FUNC, "_dblib_check_and_handle_interrupt %p [%p, %p]\n", dbproc, dbproc->chkintr, dbproc->hndlintr);

	if (dbproc->chkintr(dbproc)){
		switch (ret = dbproc->hndlintr(dbproc)) {
		case INT_EXIT:
			tdsdump_log(TDS_DBG_FUNC, "dbproc->hndlintr returned INT_EXIT, goodbye!\n");
			exit(1);
		case INT_CANCEL:
			tdsdump_log(TDS_DBG_FUNC, "dbproc->hndlintr returned INT_CANCEL\n");
			break;
		case INT_CONTINUE:
			tdsdump_log(TDS_DBG_FUNC, "dbproc->hndlintr returned INT_CONTINUE\n");
			break;
		default:
			tdsdump_log(TDS_DBG_FUNC, "dbproc->hndlintr returned an invalid value (%d), returning INT_CONTINUE\n", ret);
			ret = INT_CONTINUE;
			break;
		}
	}
	return ret;
}


void
_dblib_setTDS_version(TDSLOGIN * tds_login, DBINT version)
{
	switch (version) {
	case DBVERSION_42:
		tds_set_version(tds_login, 4, 2);
		break;
	case DBVERSION_46:
		tds_set_version(tds_login, 4, 6);
		break;
	case DBVERSION_100:
		tds_set_version(tds_login, 5, 0);
		break;
	}
}

void
_dblib_convert_err(DBPROCESS * dbproc, TDS_INT len)
{
	switch (len) {
	case TDS_CONVERT_NOAVAIL:
		dbperror(dbproc, SYBERDCN, 0);
		break;
	case TDS_CONVERT_SYNTAX:
		dbperror(dbproc, SYBECSYN, 0);
		break;
	case TDS_CONVERT_NOMEM:
		dbperror(dbproc, SYBEMEM, ENOMEM);
		break;
	case TDS_CONVERT_OVERFLOW:
		dbperror(dbproc, SYBECOFL, 0);
		break;
	default:
	case TDS_CONVERT_FAIL:
		dbperror(dbproc, SYBECINTERNAL, 0);
		break;
	}
}

