/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2008-2010  Frediano Ziglio
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

/**
 * \file
 * \brief Handle bulk copy
 */

#include <config.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <ctype.h>

#include <assert.h>

#include <freetds/tds.h>
#include <freetds/checks.h>
#include <freetds/bytes.h>
#include <freetds/iconv.h>
#include <freetds/stream.h>
#include <freetds/convert.h>
#include <freetds/utils/string.h>
#include <freetds/replacements.h>

/** \cond HIDDEN_SYMBOLS */
#ifndef MAX
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#endif

#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif
/** \endcond */

/**
 * Holds clause buffer
 */
typedef struct tds_pbcb
{
	/** buffer */
	char *pb;
	/** buffer length */
	unsigned int cb;
	/** true is buffer came from malloc */
	unsigned int from_malloc;
} TDSPBCB;

static TDSRET tds7_bcp_send_colmetadata(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
static TDSRET tds_bcp_start_insert_stmt(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
static int tds5_bcp_add_fixed_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
				      int offset, unsigned char * rowbuffer, int start);
static int tds5_bcp_add_variable_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
					 int offset, TDS_UCHAR *rowbuffer, int start, int *pncols);
static void tds_bcp_row_free(TDSRESULTINFO* result, unsigned char *row);
static TDSRET tds5_process_insert_bulk_reply(TDSSOCKET * tds, TDSBCPINFO *bcpinfo);
static TDSRET tds5_get_col_data_or_dflt(tds_bcp_get_col_data get_col_data,
                                        TDSBCPINFO *bulk, TDSCOLUMN *bcpcol,
                                        int offset, int colnum);
static int tds_bcp_is_bound(TDSBCPINFO *bcpinfo, TDSCOLUMN *colinfo);

/**
 * Initialize BCP information.
 * Query structure of the table to server.
 * \tds
 * \param bcpinfo BCP information to initialize. Structure should be allocate
 *        and table name and direction should be already set.
 */
TDSRET
tds_bcp_init(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSRESULTINFO *resinfo;
	TDSRESULTINFO *bindinfo = NULL;
	TDSCOLUMN *curcol;
	TDS_INT result_type;
	int i;
	TDSRET rc;
	const char *fmt;

	/* FIXME don't leave state in processing state */

	/* TODO quote tablename if needed */
	if (bcpinfo->direction != TDS_BCP_QUERYOUT)
		fmt = "SET FMTONLY ON select * from %s SET FMTONLY OFF";
	else
		fmt = "SET FMTONLY ON %s SET FMTONLY OFF";

	if (TDS_FAILED(rc=tds_submit_queryf(tds, fmt, tds_dstr_cstr(&bcpinfo->tablename))))
		/* TODO return an error ?? */
		/* Attempt to use Bulk Copy with a non-existent Server table (might be why ...) */
		return rc;

	/* TODO possibly stop at ROWFMT and copy before going to idle */
	/* TODO check what happen if table is not present, cleanup on error */
	while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
		   == TDS_SUCCESS)
		continue;
	if (TDS_FAILED(rc))
		return rc;

	/* copy the results info from the TDS socket */
	if (!tds->res_info)
		return TDS_FAIL;

	resinfo = tds->res_info;
	if ((bindinfo = tds_alloc_results(resinfo->num_cols)) == NULL) {
		rc = TDS_FAIL;
		goto cleanup;
	}

	bindinfo->row_size = resinfo->row_size;

	/* Copy the column metadata */
	rc = TDS_FAIL;
	for (i = 0; i < bindinfo->num_cols; i++) {

		curcol = bindinfo->columns[i];
		
		/*
		 * TODO use memcpy ??
		 * curcol and resinfo->columns[i] are both TDSCOLUMN.  
		 * Why not "curcol = resinfo->columns[i];"?  Because the rest of TDSCOLUMN (below column_timestamp)
		 * isn't being used.  Perhaps this "upper" part of TDSCOLUMN should be a substructure.
		 * Or, see if the "lower" part is unused (and zeroed out) at this point, and just do one assignment.
		 */
		curcol->funcs = resinfo->columns[i]->funcs;
		curcol->column_type = resinfo->columns[i]->column_type;
		curcol->column_usertype = resinfo->columns[i]->column_usertype;
		curcol->column_flags = resinfo->columns[i]->column_flags;
		if (curcol->column_varint_size == 0)
			curcol->column_cur_size = resinfo->columns[i]->column_cur_size;
		else
			curcol->column_cur_size = -1;
		curcol->column_size = resinfo->columns[i]->column_size;
		curcol->column_varint_size = resinfo->columns[i]->column_varint_size;
		curcol->column_prec = resinfo->columns[i]->column_prec;
		curcol->column_scale = resinfo->columns[i]->column_scale;
		curcol->on_server = resinfo->columns[i]->on_server;
		curcol->char_conv = resinfo->columns[i]->char_conv;
		if (!tds_dstr_dup(&curcol->column_name, &resinfo->columns[i]->column_name))
			goto cleanup;
		if (!tds_dstr_dup(&curcol->table_column_name, &resinfo->columns[i]->table_column_name))
			goto cleanup;
		curcol->column_nullable = resinfo->columns[i]->column_nullable;
		curcol->column_identity = resinfo->columns[i]->column_identity;
		curcol->column_timestamp = resinfo->columns[i]->column_timestamp;
		curcol->column_computed = resinfo->columns[i]->column_computed;
		
		memcpy(curcol->column_collation, resinfo->columns[i]->column_collation, 5);
		
		if (is_numeric_type(curcol->column_type)) {
			curcol->bcp_column_data = tds_alloc_bcp_column_data(sizeof(TDS_NUMERIC));
			((TDS_NUMERIC *) curcol->bcp_column_data->data)->precision = curcol->column_prec;
			((TDS_NUMERIC *) curcol->bcp_column_data->data)->scale = curcol->column_scale;
                } else if (bcpinfo->bind_count != 0 /* ctlib */
                           &&  is_blob_col(curcol)) {
                        curcol->bcp_column_data = tds_alloc_bcp_column_data(0);
		} else {
			curcol->bcp_column_data = 
				tds_alloc_bcp_column_data(MAX(curcol->column_size,curcol->on_server.column_size));
		}
		if (!curcol->bcp_column_data)
			goto cleanup;
	}

	if (!IS_TDS7_PLUS(tds->conn)) {
		bindinfo->current_row = tds_new(unsigned char, bindinfo->row_size);
		if (!bindinfo->current_row)
			goto cleanup;
		bindinfo->row_free = tds_bcp_row_free;
	}

	if (bcpinfo->identity_insert_on) {

		rc = tds_submit_queryf(tds, "set identity_insert %s on", tds_dstr_cstr(&bcpinfo->tablename));
		if (TDS_FAILED(rc))
			goto cleanup;

		/* TODO use tds_process_simple_query */
		while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
			   == TDS_SUCCESS) {
		}
		if (rc != TDS_NO_MORE_RESULTS)
			goto cleanup;
	}

	bcpinfo->bindinfo = bindinfo;
	bcpinfo->bind_count = 0;
	return TDS_SUCCESS;

cleanup:
	tds_free_results(bindinfo);
	return rc;
}

/**
 * Help to build query to be sent to server.
 * Append column declaration to the query.
 * Only for TDS 7.0+.
 * \tds
 * \param[out] clause output string
 * \param bcpcol column to append
 * \param first  true if column is the first
 * \return TDS_SUCCESS or TDS_FAIL.
 */
static TDSRET
tds7_build_bulk_insert_stmt(TDSSOCKET * tds, TDSPBCB * clause, TDSCOLUMN * bcpcol, int first)
{
	char column_type[40];

	tdsdump_log(TDS_DBG_FUNC, "tds7_build_bulk_insert_stmt(%p, %p, %p, %d)\n", tds, clause, bcpcol, first);

	if (TDS_FAILED(tds_get_column_declaration(tds, bcpcol, column_type))) {
		tdserror(tds_get_ctx(tds), tds, TDSEBPROBADTYP, errno);
		tdsdump_log(TDS_DBG_FUNC, "error: cannot build bulk insert statement. unrecognized server datatype %d\n",
			    bcpcol->on_server.column_type);
		return TDS_FAIL;
	}

	if (clause->cb < strlen(clause->pb)
	    + tds_quote_id(tds, NULL, tds_dstr_cstr(&bcpcol->column_name), tds_dstr_len(&bcpcol->column_name))
	    + strlen(column_type)
	    + ((first) ? 2u : 4u)) {
		char *temp = tds_new(char, 2 * clause->cb);

		if (!temp) {
			tdserror(tds_get_ctx(tds), tds, TDSEMEM, errno);
			return TDS_FAIL;
		}
		strcpy(temp, clause->pb);
		if (clause->from_malloc)
			free(clause->pb);
		clause->from_malloc = 1;
		clause->pb = temp;
		clause->cb *= 2;
	}

	if (!first)
		strcat(clause->pb, ", ");

	tds_quote_id(tds, strchr(clause->pb, 0), tds_dstr_cstr(&bcpcol->column_name), tds_dstr_len(&bcpcol->column_name));
	strcat(clause->pb, " ");
	strcat(clause->pb, column_type);

	return TDS_SUCCESS;
}

static const char * uc_str(const char * s)
{
        /* return strupper(strdup(s)); */
        size_t n = strcspn(s, "abcdefghijklmnopqrstuvwxyz");
        if (s[n] == '\0') {
                return s; /* already all uppercase */
        } else {
                char * result = malloc(n + 1);
                size_t i;
                for (i = 0; i < n; i++) {
                        result[i] = toupper(s[i]);
                }
                result[n] = '\0';
                return result;
        }
}

/**
 * Prepare the query to be sent to server to request BCP information
 * \tds
 * \param bcpinfo BCP information
 */
static TDSRET
tds_bcp_start_insert_stmt(TDSSOCKET * tds, TDSBCPINFO * bcpinfo)
{
	char *query;

	if (IS_TDS7_PLUS(tds->conn)) {
		int i, firstcol, erc;
		char *hint;
		TDSCOLUMN *bcpcol;
		TDSPBCB colclause;
		char clause_buffer[4096] = { 0 };
                bool triggers_checked = !bcpinfo->hint; /* Done on demand */

		colclause.pb = clause_buffer;
		colclause.cb = sizeof(clause_buffer);
		colclause.from_malloc = 0;

		/* TODO avoid asprintf, use always malloc-ed buffer */
		firstcol = 1;

		for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
			bcpcol = bcpinfo->bindinfo->columns[i];

                        if (bcpcol->column_timestamp
                            ||  !tds_bcp_is_bound(bcpinfo, bcpcol))
				continue;
			if (!bcpinfo->identity_insert_on && bcpcol->column_identity)
				continue;
                        if (bcpcol->column_computed) {
                                if ( !triggers_checked ) {
                                        const char * uc_hint
                                                = uc_str(bcpinfo->hint);
                                        if (strstr(uc_hint, "FIRE_TRIGGERS")) {
                                                bcpinfo->with_triggers = true;
                                        }
                                        if (uc_hint != bcpinfo->hint) {
                                                free((char *)uc_hint);
                                        }
                                        triggers_checked = true;
                                }
                                if ( !bcpinfo->with_triggers ) {
                                        continue;
                                }
                        }
			tds7_build_bulk_insert_stmt(tds, &colclause, bcpcol, firstcol);
			firstcol = 0;
		}

		if (bcpinfo->hint) {
			if (asprintf(&hint, " with (%s)", bcpinfo->hint) < 0)
				hint = NULL;
		} else {
			hint = strdup("");
		}
		if (!hint) {
			if (colclause.from_malloc)
				TDS_ZERO_FREE(colclause.pb);
			return TDS_FAIL;
		}

		erc = asprintf(&query, "insert bulk %s (%s)%s", tds_dstr_cstr(&bcpinfo->tablename), colclause.pb, hint);

		free(hint);
		if (colclause.from_malloc)
			TDS_ZERO_FREE(colclause.pb);	/* just for good measure; not used beyond this point */

		if (erc < 0)
			return TDS_FAIL;
	} else {
		/* NOTE: if we use "with nodescribe" for following inserts server do not send describe */
		if (asprintf(&query, "insert bulk %s", tds_dstr_cstr(&bcpinfo->tablename)) < 0)
			return TDS_FAIL;
	}

	/* save the statement for later... */
	bcpinfo->insert_stmt = query;

	return TDS_SUCCESS;
}

static TDSRET
tds7_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo,
                 tds_bcp_get_col_data get_col_data,
                 tds_bcp_null_error null_error, int offset, int start_col)
{
	int i;

        if (start_col == 0) {
                tds_put_byte(tds, TDS_ROW_TOKEN);   /* 0xd1 */
        }
        for (i = start_col; i < bcpinfo->bindinfo->num_cols; i++) {

		TDS_INT save_size;
		unsigned char *save_data;
		TDSBLOB blob;
		TDSCOLUMN  *bindcol;
		TDSRET rc;
                bool has_text = false;

		bindcol = bcpinfo->bindinfo->columns[i];

		/*
		 * Don't send the (meta)data for timestamp columns or
		 * identity columns unless indentity_insert is enabled.
		 */

		if ((!bcpinfo->identity_insert_on && bindcol->column_identity) ||
			bindcol->column_timestamp ||
                        (bindcol->column_computed
                         && !bcpinfo->with_triggers) ||
                        !tds_bcp_is_bound(bcpinfo, bindcol)) {
			continue;
		}

		rc = get_col_data(bcpinfo, bindcol, offset);
		if (TDS_FAILED(rc)) {
			tdsdump_log(TDS_DBG_INFO1, "get_col_data (column %d) failed\n", i + 1);
			return rc;
                } else if (rc == TDS_NO_MORE_RESULTS) {
                        has_text = true;
		}
		tdsdump_log(TDS_DBG_INFO1, "gotten column %d length %d null %d\n",
				i + 1, bindcol->bcp_column_data->datalen, bindcol->bcp_column_data->is_null);

		save_size = bindcol->column_cur_size;
		save_data = bindcol->column_data;
		assert(bindcol->column_data == NULL);
		if (bindcol->bcp_column_data->is_null) {
                        if ( !bindcol->column_nullable
                            &&  !is_nullable_type(bindcol->on_server
                                                  .column_type) ) {
                                if (null_error) {
                                        null_error(bcpinfo, i, offset);
                                }
                                return TDS_FAIL;
                        }
			bindcol->column_cur_size = -1;
                } else if (has_text) {
                        bindcol->column_cur_size
                                = bindcol->bcp_column_data->datalen;
		} else if (is_blob_col(bindcol)) {
			bindcol->column_cur_size = bindcol->bcp_column_data->datalen;
			memset(&blob, 0, sizeof(blob));
			blob.textvalue = (TDS_CHAR *) bindcol->bcp_column_data->data;
			bindcol->column_data = (unsigned char *) &blob;
		} else {
			bindcol->column_cur_size = bindcol->bcp_column_data->datalen;
			bindcol->column_data = bindcol->bcp_column_data->data;
		}
		rc = bindcol->funcs->put_data(tds, bindcol, 1);
		bindcol->column_cur_size = save_size;
		bindcol->column_data = save_data;

		if (TDS_FAILED(rc))
			return rc;
                else if (has_text) {
                        bcpinfo->next_col = i + 1;
                        /* bcpinfo->text_sent = 0; */
                        return TDS_NO_MORE_RESULTS;
                }
	}
	return TDS_SUCCESS;
}

static TDSRET
tds5_send_non_blobs(TDSSOCKET *tds, TDSBCPINFO *bcpinfo,
                    tds_bcp_get_col_data get_col_data,
                    tds_bcp_null_error null_error, int offset)
{
	int row_pos;
	int row_sz_pos;
	int var_cols_written = 0;
	TDS_INT	 old_record_size = bcpinfo->bindinfo->row_size;
	unsigned char *record = bcpinfo->bindinfo->current_row;

	memset(record, '\0', old_record_size);	/* zero the rowbuffer */

	/*
	 * offset 0 = number of var columns
	 * offset 1 = row number.  zeroed (datasever assigns)
	 */
	row_pos = 2;

	if ((row_pos = tds5_bcp_add_fixed_columns(bcpinfo, get_col_data, null_error, offset, record, row_pos)) < 0)
		return TDS_FAIL;

	row_sz_pos = row_pos;

	/* potential variable columns to write */

	row_pos = tds5_bcp_add_variable_columns(bcpinfo, get_col_data, null_error, offset, record, row_pos, &var_cols_written);
	if (row_pos < 0)
		return TDS_FAIL;


	if (var_cols_written) {
		TDS_PUT_UA2LE(&record[row_sz_pos], row_pos);
		record[0] = var_cols_written;
	}

	tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n", old_record_size, row_pos);

	tds_put_smallint(tds, row_pos);
	tds_put_n(tds, record, row_pos);
        return TDS_SUCCESS;
}

static TDSRET
tds5_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo,
                 tds_bcp_get_col_data get_col_data,
                 tds_bcp_null_error null_error, int offset, int start_col)
{
        int i;
        if (start_col == 0) {
                TDS_PROPAGATE(tds5_send_non_blobs(tds, bcpinfo, get_col_data,
                                                  null_error, offset));
        }
	/* row is done, now handle any text/image data */

        bcpinfo->blob_cols = 0;

        for (i = start_col; i < bcpinfo->bindinfo->num_cols; i++) {
		TDSCOLUMN  *bindcol = bcpinfo->bindinfo->columns[i];
		if (is_blob_type(bindcol->on_server.column_type)) {
                        /* Elide trailing NULLs */
                        if (bindcol->bcp_column_data->is_null) {
                                int j;
                                for (j = i + 1;
                                     j < bcpinfo->bindinfo->num_cols; ++j) {
                                        TDSCOLUMN *bindcol2
                                                = bcpinfo->bindinfo
                                                ->columns[j];
                                        if (is_blob_type(bindcol2->column_type)
                                            &&  !(bindcol2->bcp_column_data
                                                  ->is_null)) {
                                                break;
                                        }
                                }
                                if (j == bcpinfo->bindinfo->num_cols) {
                                        i = j;
                                        break;
                                }
                        }

                        TDSRET rc = tds5_get_col_data_or_dflt(get_col_data,
                                                              bcpinfo, bindcol,
                                                              offset, i);
			if (TDS_FAILED(rc))
				return rc;
			/* unknown but zero */
			tds_put_smallint(tds, 0);
                        TDS_PUT_BYTE(tds, bindcol->on_server.column_type);
                        tds_put_byte(tds, 0xff - bcpinfo->blob_cols);
			/*
			 * offset of txptr we stashed during variable
			 * column processing
			 */
			tds_put_smallint(tds, bindcol->column_textpos);
			tds_put_int(tds, bindcol->bcp_column_data->datalen);
                        if (rc == TDS_NO_MORE_RESULTS) {
                                bcpinfo->next_col = i + 1;
                                /* bcpinfo->text_sent = 0; */
                                return rc;
                        }
			tds_put_n(tds, bindcol->bcp_column_data->data, bindcol->bcp_column_data->datalen);
                        bcpinfo->blob_cols++;

		}
	}
	return TDS_SUCCESS;
}

/**
 * Send one row of data to server
 * \tds
 * \param bcpinfo BCP information
 * \param get_col_data function to call to retrieve data to be sent
 * \param ignored function to call if we try to send NULL if not allowed (not used)
 * \param offset passed to get_col_data and null_error to specify the row to get
 * \return TDS_SUCCESS or TDS_FAIL.
 */
TDSRET
tds_bcp_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo,
		    tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset)
{
	TDSRET rc;
        int start_col = bcpinfo->next_col;

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_send_bcp_record(%p, %p, %p, %p, %d)\n",
		    tds, bcpinfo, get_col_data, null_error, offset);

	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

        if (start_col > 0) {
                TDSCOLUMN *bindcol = bcpinfo->bindinfo->columns[start_col - 1];
                *bindcol->column_lenbind
                        = MIN((TDS_INT) bindcol->column_bindlen
                              - bcpinfo->text_sent,
                              *bindcol->column_lenbind);
                if (IS_TDS7_PLUS(tds->conn)
                    &&  bindcol->column_varint_size == 8
                    &&  *bindcol->column_lenbind > 0) {
                        /* Put PLP chunk length. */
                        tds_put_int(tds, *bindcol->column_lenbind);
                }
                tds_put_n(tds, bindcol->column_varaddr,
                          *bindcol->column_lenbind);
                bcpinfo->text_sent += *bindcol->column_lenbind;
                if ((TDS_UINT) bcpinfo->text_sent < bindcol->column_bindlen) {
                        return TDS_SUCCESS; /* That's all for now. */
                } else if (!IS_TDS7_PLUS(tds->conn)) {
                        bcpinfo->blob_cols++;
                } else if (bindcol->column_varint_size == 8) {
                        tds_put_int(tds, 0); /* Put PLP terminator. */
                }
                bcpinfo->next_col  = 0;
                bcpinfo->text_sent = 0;
        }

	if (IS_TDS7_PLUS(tds->conn))
                rc = tds7_send_record(tds, bcpinfo, get_col_data, null_error,
                                      offset, start_col);
	else
                rc = tds5_send_record(tds, bcpinfo, get_col_data, null_error,
                                      offset, start_col);

        if (rc == TDS_NO_MORE_RESULTS)
                return TDS_SUCCESS;
	tds_set_state(tds, TDS_SENDING);
        bcpinfo->next_col = 0;
        bcpinfo->rows_sent++;
	return rc;
}

static inline void
tds5_swap_data(const TDSCOLUMN *col, void *p)
{
#ifdef WORDS_BIGENDIAN
	tds_swap_datatype(tds_get_conversion_type(col->on_server.column_type, col->column_size), p);
#endif
}

/**
 * Add fixed size columns to the row
 * \param bcpinfo BCP information
 * \param get_col_data function to call to retrieve data to be sent
 * \param ignored function to call if we try to send NULL if not allowed (not used)
 * \param offset passed to get_col_data and null_error to specify the row to get
 * \param rowbuffer row buffer to write to
 * \param start row buffer last end position
 * \returns new row length or -1 on error.
 */
static int
tds5_bcp_add_fixed_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
			   int offset, unsigned char * rowbuffer, int start)
{
	TDS_NUMERIC *num;
	int row_pos = start;
	int cpbytes;
	int i;
	int bitleft = 0, bitpos = 0;

	assert(bcpinfo);
	assert(rowbuffer);

	tdsdump_log(TDS_DBG_FUNC, "tds5_bcp_add_fixed_columns(%p, %p, %p, %d, %p, %d)\n",
		    bcpinfo, get_col_data, null_error, offset, rowbuffer, start);

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {

		TDSCOLUMN *const bcpcol = bcpinfo->bindinfo->columns[i];
		const TDS_INT column_size = bcpcol->on_server.column_size;

		/* if possible check information from server */
		if (bcpinfo->sybase_count > i) {
			if (bcpinfo->sybase_colinfo[i].offset < 0)
				continue;
		} else {
			if (is_nullable_type(bcpcol->on_server.column_type) || bcpcol->column_nullable)
				continue;
		}

		tdsdump_log(TDS_DBG_FUNC, "tds5_bcp_add_fixed_columns column %d (%s) is a fixed column\n", i + 1, tds_dstr_cstr(&bcpcol->column_name));

                if (TDS_FAILED(tds5_get_col_data_or_dflt(get_col_data, bcpinfo,
                                                         bcpcol, offset, i))) {
			tdsdump_log(TDS_DBG_INFO1, "get_col_data (column %d) failed\n", i + 1);
			return -1;
		}

		/* We have no way to send a NULL at this point, return error to client */
		if (bcpcol->bcp_column_data->is_null) {
			tdsdump_log(TDS_DBG_ERROR, "tds5_bcp_add_fixed_columns column %d is a null column\n", i + 1);
			/* No value or default value available and NULL not allowed. */
			if (null_error)
				null_error(bcpinfo, i, offset);
			return -1;
		}

		if (is_numeric_type(bcpcol->on_server.column_type)) {
			num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
			cpbytes = tds_numeric_bytes_per_prec[num->precision];
			memcpy(&rowbuffer[row_pos], num->array, cpbytes);
		} else if (bcpcol->column_type == SYBBIT) {
			/* all bit are collapsed together */
			if (!bitleft) {
				bitpos = row_pos++;
				bitleft = 8;
				rowbuffer[bitpos] = 0;
			}
			if (bcpcol->bcp_column_data->data[0])
				rowbuffer[bitpos] |= 256 >> bitleft;
			--bitleft;
			continue;
		} else {
			cpbytes = bcpcol->bcp_column_data->datalen > column_size ?
				  column_size : bcpcol->bcp_column_data->datalen;
			memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
			tds5_swap_data(bcpcol, &rowbuffer[row_pos]);

			/* CHAR data may need padding out to the database length with blanks */
			/* TODO check binary !!! */
			if (bcpcol->column_type == SYBCHAR && cpbytes < column_size)
				memset(rowbuffer + row_pos + cpbytes, ' ', column_size - cpbytes);
		}

		row_pos += column_size;
	}
	return row_pos;
}

/**
 * Add variable size columns to the row
 *
 * \param bcpinfo BCP information already prepared
 * \param get_col_data function to call to retrieve data to be sent
 * \param null_error function to call if we try to send NULL if not allowed
 * \param offset passed to get_col_data and null_error to specify the row to get
 * \param rowbuffer The row image that will be sent to the server. 
 * \param start Where to begin copying data into the rowbuffer. 
 * \param pncols Address of output variable holding the count of columns added to the rowbuffer.  
 * 
 * \return length of (potentially modified) rowbuffer, or -1.
 */
static int
tds5_bcp_add_variable_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
			      int offset, TDS_UCHAR* rowbuffer, int start, int *pncols)
{
	TDS_USMALLINT offsets[256];
	unsigned int i, row_pos;
	unsigned int ncols = 0;

	assert(bcpinfo);
	assert(rowbuffer);
	assert(pncols);

	tdsdump_log(TDS_DBG_FUNC, "%4s %8s %18s %18s %8s\n", 	"col", 
								"type", 
								"is_nullable_type", 
								"column_nullable", 
								"is null" );
	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		TDSCOLUMN *bcpcol = bcpinfo->bindinfo->columns[i];
		tdsdump_log(TDS_DBG_FUNC, "%4d %8d %18s %18s %8s\n", 	i,
									bcpcol->on_server.column_type,
									is_nullable_type(bcpcol->on_server.column_type)? "yes" : "no",
									bcpcol->column_nullable? "yes" : "no",
									bcpcol->bcp_column_data->is_null? "yes" : "no" );
	}

	/* the first two bytes of the rowbuffer are reserved to hold the entire record length */
	row_pos = start + 2;
	offsets[0] = row_pos;

	tdsdump_log(TDS_DBG_FUNC, "%4s %8s %8s %8s\n", "col", "ncols", "row_pos", "cpbytes");

        for (i = bcpinfo->next_col; i < bcpinfo->bindinfo->num_cols; i++) {
		unsigned int cpbytes = 0;
		TDSCOLUMN *bcpcol = bcpinfo->bindinfo->columns[i];
                TDSRET rc;

		/*
		 * Is this column of "variable" type, i.e. NULLable
		 * or naturally variable length e.g. VARCHAR
		 */
                if (bcpinfo->sybase_count > (TDS_INT) i) {
			if (bcpinfo->sybase_colinfo[i].offset >= 0)
				continue;
		} else {
			if (!is_nullable_type(bcpcol->on_server.column_type) && !bcpcol->column_nullable)
				continue;
		}

		tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d %8d\n", i, ncols, row_pos, cpbytes);

                rc = tds5_get_col_data_or_dflt(get_col_data, bcpinfo, bcpcol,
                                               offset, i);
                if (TDS_FAILED(rc)) {
			return -1;
                } else if (rc == TDS_NO_MORE_RESULTS) {
                        bcpinfo->next_col = i + 1;
                }

		/* If it's a NOT NULL column, and we have no data, throw an error.
		 * This is the behavior for Sybase, this function is only used for Sybase */
		if (!bcpcol->column_nullable && bcpcol->bcp_column_data->is_null) {
			/* No value or default value available and NULL not allowed. */
			if (null_error)
				null_error(bcpinfo, i, offset);
			return -1;
		}

		/* move the column buffer into the rowbuffer */
		if (!bcpcol->bcp_column_data->is_null) {
			if (is_blob_type(bcpcol->on_server.column_type)) {
				cpbytes = 16;
				bcpcol->column_textpos = row_pos;               /* save for data write */
			} else if (is_numeric_type(bcpcol->on_server.column_type)) {
				TDS_NUMERIC *num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
				cpbytes = tds_numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
                        } else if ((bcpcol->column_type == SYBVARCHAR
                                    ||  bcpcol->column_type == SYBCHAR)
                                   &&  bcpcol->bcp_column_data->datalen == 0) {
                                cpbytes = 1;
                                rowbuffer[row_pos] = ' ';
			} else {
				cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ?
				bcpcol->column_size : bcpcol->bcp_column_data->datalen;
				memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
				tds5_swap_data(bcpcol, &rowbuffer[row_pos]);
			}
                } else if (is_blob_type(bcpcol->column_type)) {
                        bcpcol->column_textpos = row_pos;
		}

		row_pos += cpbytes;
		offsets[++ncols] = row_pos;
		tdsdump_dump_buf(TDS_DBG_NETWORK, "BCP row buffer so far", rowbuffer,  row_pos);
	}

	tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d\n", i, ncols, row_pos);

	/*
	 * The rowbuffer ends with an offset table and, optionally, an adjustment table.  
	 * The offset table has 1-byte elements that describe the locations of the start of each column in
	 * the rowbuffer.  If the largest offset is greater than 255, another table -- the adjustment table --
	 * is inserted just before the offset table.  It holds the high bytes. 
	 * 
	 * Both tables are laid out in reverse:
	 * 	#elements, offset N+1, offset N, offset N-1, ... offset 0
	 * E.g. for 2 columns you have 4 data points:
	 *	1.  How many elements (4)
	 *	2.  Start of column 3 (non-existent, "one off the end")
	 *	3.  Start of column 2
	 *	4.  Start of column 1
	 *  The length of each column is computed by subtracting its start from the its successor's start. 
	 *
	 * The algorithm below computes both tables. If the adjustment table isn't needed, the 
	 * effect is to overwrite it with the offset table.  
	 */
	while (ncols && offsets[ncols] == offsets[ncols-1])
		ncols--;	/* trailing NULL columns are not sent and are not included in the offset table */

	if (ncols) {
		TDS_UCHAR *poff = rowbuffer + row_pos;
		unsigned int pfx_top = offsets[ncols] / 256;

		tdsdump_log(TDS_DBG_FUNC, "ncols=%u poff=%p [%u]\n", ncols, poff, offsets[ncols]);

                if (offsets[ncols] / 256  ==  offsets[ncols-1] / 256) {
                        *poff++ = ncols + 1;
                }
		/* this is some kind of run-length-prefix encoding */
		while (pfx_top) {
			unsigned int n_pfx = 1;

			for (i = 0; i <= ncols ; ++i)
                                if ((offsets[i] / 256u) < pfx_top)
					++n_pfx;
			*poff++ = n_pfx;
			--pfx_top;
		}
   
		tdsdump_log(TDS_DBG_FUNC, "poff=%p\n", poff);

		for (i=0; i <= ncols; i++)
			*poff++ = offsets[ncols-i] & 0xFF;
		row_pos = (unsigned int)(poff - rowbuffer);
	}

	tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d\n", i, ncols, row_pos);
	tdsdump_dump_buf(TDS_DBG_NETWORK, "BCP row buffer", rowbuffer,  row_pos);

	*pncols = ncols;

	return ncols == 0? start : row_pos;
}

/**
 * Send BCP metadata to server.
 * Only for TDS 7.0+.
 * \tds
 * \param bcpinfo BCP information
 * \return TDS_SUCCESS or TDS_FAIL.
 */
static TDSRET
tds7_bcp_send_colmetadata(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSCOLUMN *bcpcol;
	int i, num_cols;

	tdsdump_log(TDS_DBG_FUNC, "tds7_bcp_send_colmetadata(%p, %p)\n", tds, bcpinfo);
	assert(tds && bcpinfo);

	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	/* 
	 * Deep joy! For TDS 7 we have to send a colmetadata message followed by row data
	 */
	tds_put_byte(tds, TDS7_RESULT_TOKEN);	/* 0x81 */

	num_cols = 0;
	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		bcpcol = bcpinfo->bindinfo->columns[i];
		if ((!bcpinfo->identity_insert_on && bcpcol->column_identity) || 
			bcpcol->column_timestamp ||
                        (bcpcol->column_computed
                         && !bcpinfo->with_triggers) ||
                        !tds_bcp_is_bound(bcpinfo, bcpcol)) {
			continue;
		}
		num_cols++;
	}

	tds_put_smallint(tds, num_cols);

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		size_t converted_len;
		const char *converted_name;

		bcpcol = bcpinfo->bindinfo->columns[i];

		/*
		 * dont send the (meta)data for timestamp columns, or
		 * identity columns (unless indentity_insert is enabled
		 */

		if ((!bcpinfo->identity_insert_on && bcpcol->column_identity) || 
			bcpcol->column_timestamp ||
                        (bcpcol->column_computed 
                         && !bcpinfo->with_triggers) ||
                        !tds_bcp_is_bound(bcpinfo, bcpcol)) {
			continue;
		}

		if (IS_TDS72_PLUS(tds->conn))
			tds_put_int(tds, bcpcol->column_usertype);
		else
			tds_put_smallint(tds, bcpcol->column_usertype);
		tds_put_smallint(tds, bcpcol->column_flags);
                TDS_PUT_BYTE(tds, bcpcol->on_server.column_type);

		assert(bcpcol->funcs);
		bcpcol->funcs->put_info(tds, bcpcol);

		/* TODO put this in put_info. It seems that parameter format is
		 * different from BCP format
		 */
		if (is_blob_type(bcpcol->on_server.column_type)) {
			converted_name = tds_convert_string(tds, tds->conn->char_convs[client2ucs2],
							    tds_dstr_cstr(&bcpinfo->tablename),
							    (int) tds_dstr_len(&bcpinfo->tablename), &converted_len);
			if (!converted_name) {
				tds_connection_close(tds->conn);
				return TDS_FAIL;
			}

			/* UTF-16 length is always size / 2 even for 4 byte letters (yes, 1 letter of length 2) */
			TDS_PUT_SMALLINT(tds, converted_len / 2);
			tds_put_n(tds, converted_name, converted_len);

			tds_convert_string_free(tds_dstr_cstr(&bcpinfo->tablename), converted_name);
		}

		converted_name = tds_convert_string(tds, tds->conn->char_convs[client2ucs2],
						    tds_dstr_cstr(&bcpcol->column_name),
						    (int) tds_dstr_len(&bcpcol->column_name), &converted_len);
		if (!converted_name) {
			tds_connection_close(tds->conn);
			return TDS_FAIL;
		}

		/* UTF-16 length is always size / 2 even for 4 byte letters (yes, 1 letter of length 2) */
		TDS_PUT_BYTE(tds, converted_len / 2);
		tds_put_n(tds, converted_name, converted_len);

		tds_convert_string_free(tds_dstr_cstr(&bcpcol->column_name), converted_name);
	}

	tds_set_state(tds, TDS_SENDING);
	return TDS_SUCCESS;
}

/**
 * Tell we finished sending BCP data to server
 * \tds
 * \param[out] rows_copied number of rows copied to server
 */
TDSRET
tds_bcp_done(TDSSOCKET *tds, int *rows_copied)
{
	TDSRET rc;

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_done(%p, %p)\n", tds, rows_copied);

	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

	rc = tds_process_simple_query(tds);
	if (TDS_FAILED(rc))
		return rc;

	if (rows_copied)
		*rows_copied = tds->rows_affected;

	return TDS_SUCCESS;
}

/**
 * Start sending BCP data to server.
 * Initialize stream to accept data.
 * \tds
 * \param bcpinfo BCP information already prepared
 */
TDSRET
tds_bcp_start(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSRET rc;

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_start(%p, %p)\n", tds, bcpinfo);

	if (!IS_TDS50_PLUS(tds->conn))
		return TDS_FAIL;

	rc = tds_submit_query(tds, bcpinfo->insert_stmt);
	if (TDS_FAILED(rc))
		return rc;

	/* set we want to switch to bulk state */
	tds->bulk_query = true;

	/*
	 * In TDS 5 we get the column information as a result set from the "insert bulk" command.
	 */
	if (IS_TDS50(tds->conn))
		rc = tds5_process_insert_bulk_reply(tds, bcpinfo);
	else
		rc = tds_process_simple_query(tds);
	if (TDS_FAILED(rc))
		return rc;

	tds->out_flag = TDS_BULK;
	if (tds_set_state(tds, TDS_SENDING) != TDS_SENDING)
		return TDS_FAIL;

	if (IS_TDS7_PLUS(tds->conn))
		tds7_bcp_send_colmetadata(tds, bcpinfo);
	
	return TDS_SUCCESS;
}

enum {
	/* list of columns we need, 0-nnn */
	BULKCOL_colcnt,
	BULKCOL_colid,
	BULKCOL_type,
	BULKCOL_length,
	BULKCOL_status,
	BULKCOL_offset,
        BULKCOL_dflt,

	/* number of columns needed */
	BULKCOL_COUNT,

	/* bitmask to have them all */
	BULKCOL_ALL = (1 << BULKCOL_COUNT) -1,
};

static int
tds5_bulk_insert_column(const char *name)
{
#define BULKCOL(n) do {\
	if (strcmp(name, #n) == 0) \
		return BULKCOL_ ## n; \
} while(0)

	switch (name[0]) {
	case 'c':
		BULKCOL(colcnt);
		BULKCOL(colid);
		break;
        case 'd':
                BULKCOL(dflt);
                break;
	case 't':
		BULKCOL(type);
		break;
	case 'l':
		BULKCOL(length);
		break;
	case 's':
		BULKCOL(status);
		break;
	case 'o':
		BULKCOL(offset);
		break;
	}
#undef BULKCOL
	return -1;
}

static void
tds5_read_bulk_defaults(TDSRESULTINFO *res_info, TDSBCPINFO *bcpinfo)
{
        int i;
        TDS5COLINFO *syb_info = bcpinfo->sybase_colinfo;
        for (i = 0;  i < res_info->num_cols;  ++i, ++syb_info) {
                TDSCOLUMN *col = res_info->columns[i];
                TDS_UCHAR* src = col->column_data;
                TDS_INT len = col->column_cur_size;
                if (is_blob_type(col->column_type)) {
                        src = (unsigned char*) ((TDSBLOB*)src)->textvalue;
                }
                while ( !syb_info->dflt ) {
                        ++syb_info;
                }
                syb_info->dflt_size = len;
                tds_realloc((void**)&syb_info->dflt_value, len);
                memcpy(syb_info->dflt_value, src, len);
        }
}

static TDSRET
tds5_process_insert_bulk_reply(TDSSOCKET * tds, TDSBCPINFO *bcpinfo)
{
	TDS_INT res_type;
	TDS_INT done_flags;
	TDSRET  rc;
	TDSRET  ret = TDS_SUCCESS;
	bool row_match = false;
	TDSRESULTINFO *res_info;
	int icol;
	unsigned col_flags;
	/* position of the columns in the row */
	int cols_pos[BULKCOL_COUNT];
	int cols_values[BULKCOL_COUNT];
	TDS5COLINFO *colinfo;
        int num_defs = 0;

	CHECK_TDS_EXTRA(tds);

	while ((rc = tds_process_tokens(tds, &res_type, &done_flags, TDS_RETURN_DONE|TDS_RETURN_ROWFMT|TDS_RETURN_ROW)) == TDS_SUCCESS) {
		switch (res_type) {
		case TDS_ROWFMT_RESULT:
			/* check if it's the resultset with column information and save column positions */
			row_match = false;
			col_flags = 0;
			res_info = tds->current_results;
			if (!res_info)
				continue;
			for (icol = 0; icol < res_info->num_cols; ++icol) {
				const TDSCOLUMN *col = res_info->columns[icol];
				int scol = tds5_bulk_insert_column(tds_dstr_cstr(&col->column_name));
				if (scol < 0)
					continue;
				cols_pos[scol] = icol;
				col_flags |= 1 << scol;
			}
			if (col_flags == BULKCOL_ALL)
				row_match = true;
			break;
		case TDS_ROW_RESULT:
			/* get the results */
			col_flags = 0;
			res_info = tds->current_results;
                        if (!row_match) {
#if ENABLE_EXTRA_CHECKS
                                assert(res_info->num_cols == num_defs);
#endif
                                tds5_read_bulk_defaults(res_info, bcpinfo);
                                continue;
                        }
			if (!res_info)
				continue;
			for (icol = 0; icol < BULKCOL_COUNT; ++icol) {
				const TDSCOLUMN *col = res_info->columns[cols_pos[icol]];
				int ctype = tds_get_conversion_type(col->on_server.column_type, col->column_size);
				unsigned char *src = col->column_data;
				int srclen = col->column_cur_size;
				CONV_RESULT dres;

				if (tds_convert(tds_get_ctx(tds), ctype, src, srclen, SYBINT4, &dres) < 0)
					break;
				col_flags |= 1 << icol;
				cols_values[icol] = dres.i;
			}
			/* save informations */
			if (col_flags != BULKCOL_ALL ||
				cols_values[BULKCOL_colcnt] < 1 ||
				cols_values[BULKCOL_colcnt] > 4096 || /* limit of columns accepted */
				cols_values[BULKCOL_colid] < 1 ||
				cols_values[BULKCOL_colid] > cols_values[BULKCOL_colcnt]) {
				rc = TDS_FAIL;
				break;
			}
			if (bcpinfo->sybase_colinfo == NULL) {
				bcpinfo->sybase_colinfo = calloc(cols_values[BULKCOL_colcnt], sizeof(*bcpinfo->sybase_colinfo));
				if (bcpinfo->sybase_colinfo == NULL) {
					rc = TDS_FAIL;
					break;
				}
				bcpinfo->sybase_count = cols_values[BULKCOL_colcnt];
			}
			/* bound check, colcnt could have changed from row to row */
			if (cols_values[BULKCOL_colid] > bcpinfo->sybase_count) {
				rc = TDS_FAIL;
				break;
			}
			colinfo = &bcpinfo->sybase_colinfo[cols_values[BULKCOL_colid] - 1];
			colinfo->type = cols_values[BULKCOL_type];
			colinfo->status = cols_values[BULKCOL_status];
			colinfo->offset = cols_values[BULKCOL_offset];
			colinfo->length = cols_values[BULKCOL_length];
                        colinfo->dflt = cols_values[BULKCOL_dflt];
                        if (colinfo->dflt) {
                                ++num_defs;
                        }
			tdsdump_log(TDS_DBG_INFO1, "gotten row information %d type %d length %d status %d offset %d\n",
					cols_values[BULKCOL_colid],
					colinfo->type,
					colinfo->length,
					colinfo->status,
					colinfo->offset);
			break;
		case TDS_DONE_RESULT:
		case TDS_DONEPROC_RESULT:
		case TDS_DONEINPROC_RESULT:
			if ((done_flags & TDS_DONE_ERROR) != 0)
				ret = TDS_FAIL;
		default:
			break;
		}
	}
	if (TDS_FAILED(rc))
		ret = rc;

	return ret;
}

static TDSRET tds5_get_col_data_or_dflt(tds_bcp_get_col_data get_col_data,
                                        TDSBCPINFO *bulk, TDSCOLUMN *bcpcol,
                                        int offset, int colnum)
{
        TDSRET ret;
        if (bcpcol->column_lenbind == NULL) {
                bcpcol->column_lenbind = (TDS_INT *)&bcpcol->column_bindlen;
        }
        ret = get_col_data(bulk, bcpcol, offset);
        BCPCOLDATA *coldata = bcpcol->bcp_column_data;
        if (bcpcol->column_varaddr == NULL  &&  coldata->datalen == 0
            &&  bulk->sybase_colinfo != NULL
            &&  ( !is_blob_type(bcpcol->column_type)
                 ||  bcpcol->column_lenbind == NULL
                 ||  bcpcol->column_lenbind[offset] == 0)) {
                const TDS5COLINFO *syb_info = &bulk->sybase_colinfo[colnum];
                const TDS_SMALLINT *nullind = bcpcol->column_nullbind;
                if ((nullind != NULL  &&  nullind[offset] == -1)
                    ||  !syb_info->dflt) {
                        if ( !bcpcol->column_nullable ) {
                                return TDS_FAIL;
                        }
                        coldata->datalen = 0;
                        coldata->is_null = true;
                } else {
                        if (syb_info->dflt_size > 4096) {
                                tds_realloc((void**)&coldata->data,
                                            syb_info->dflt_size);
                        }
                        memcpy(coldata->data, syb_info->dflt_value,
                               syb_info->dflt_size);
                        coldata->datalen = syb_info->dflt_size;
                        coldata->is_null = false;
                }
                return TDS_SUCCESS;
        }
        return ret;
}

/**
 * Free row data allocated in the result set.
 */
static void 
tds_bcp_row_free(TDSRESULTINFO* result, unsigned char *row)
{
	result->row_size = 0;
	TDS_ZERO_FREE(result->current_row);
}

/**
 * Start bulk copy to server
 * \tds
 * \param bcpinfo BCP information already prepared
 */
TDSRET
tds_bcp_start_copy_in(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSCOLUMN *bcpcol;
	int i;
	int fixed_col_len_tot     = 0;
	int variable_col_len_tot  = 0;
	int column_bcp_data_size  = 0;
	int bcp_record_size       = 0;
	TDSRET rc;
	TDS_INT var_cols;
	
	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_start_copy_in(%p, %p)\n", tds, bcpinfo);

	rc = tds_bcp_start_insert_stmt(tds, bcpinfo);
	if (TDS_FAILED(rc))
		return rc;

	rc = tds_bcp_start(tds, bcpinfo);
	if (TDS_FAILED(rc)) {
		/* TODO, in CTLib was _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 5, 1, 140, ""); */
		return rc;
	}

	/* 
	 * Work out the number of "variable" columns.  These are either nullable or of 
	 * varying length type e.g. varchar.   
	 */
	var_cols = 0;

	if (IS_TDS50(tds->conn)) {
		for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
	
			bcpcol = bcpinfo->bindinfo->columns[i];

			/*
			 * work out storage required for this datatype
			 * blobs always require 16, numerics vary, the
			 * rest can be taken from the server
			 */

			if (is_blob_type(bcpcol->on_server.column_type))
				column_bcp_data_size  = 16;
			else if (is_numeric_type(bcpcol->on_server.column_type))
				column_bcp_data_size  = tds_numeric_bytes_per_prec[bcpcol->column_prec];
			else
				column_bcp_data_size  = bcpcol->column_size;

			/*
			 * now add that size into either fixed or variable
			 * column totals...
			 */

			if (is_nullable_type(bcpcol->on_server.column_type) || bcpcol->column_nullable) {
				var_cols++;
				variable_col_len_tot += column_bcp_data_size;
			}
			else {
				fixed_col_len_tot += column_bcp_data_size;
			}
		}

		/* this formula taken from sybase manual... */

		bcp_record_size =  	4 +
							fixed_col_len_tot +
							variable_col_len_tot +
							( (int)(variable_col_len_tot / 256 ) + 1 ) +
							(var_cols + 1) +
							2;

		tdsdump_log(TDS_DBG_FUNC, "current_record_size = %d\n", bcpinfo->bindinfo->row_size);
		tdsdump_log(TDS_DBG_FUNC, "bcp_record_size     = %d\n", bcp_record_size);

		if (bcp_record_size > bcpinfo->bindinfo->row_size) {
			if (!TDS_RESIZE(bcpinfo->bindinfo->current_row, bcp_record_size)) {
				tdsdump_log(TDS_DBG_FUNC, "could not realloc current_row\n");
				return TDS_FAIL;
			}
			bcpinfo->bindinfo->row_free = tds_bcp_row_free;
			bcpinfo->bindinfo->row_size = bcp_record_size;
		}
	}

	return TDS_SUCCESS;
}

/** input stream to read a file */
typedef struct tds_file_stream {
	/** common fields, must be the first field */
	TDSINSTREAM stream;
	/** file to read from */
	FILE *f;

	/** terminator */
	const char *terminator;
	/** terminator length in bytes */
	size_t term_len;

	/** buffer for store bytes readed that could be the terminator */
	char *left;
	size_t left_pos;
} TDSFILESTREAM;

/** \cond HIDDEN_SYMBOLS */
#if defined(_WIN32) && defined(HAVE__LOCK_FILE) && defined(HAVE__UNLOCK_FILE)
#define TDS_HAVE_STDIO_LOCKED 1
#define flockfile(s) _lock_file(s)
#define funlockfile(s) _unlock_file(s)
#define getc_unlocked(s) _getc_nolock(s)
#define feof_unlocked(s) _feof_nolock(s)
#endif

#ifndef TDS_HAVE_STDIO_LOCKED
#undef getc_unlocked
#undef feof_unlocked
#undef flockfile
#undef funlockfile
#define getc_unlocked(s) getc(s)
#define feof_unlocked(s) feof(s)
#define flockfile(s) do { } while(0)
#define funlockfile(s) do { } while(0)
#endif
/** \endcond */

/**
 * Reads a chunk of data from file stream checking for terminator
 * \param stream file stream
 * \param ptr buffer where to read data
 * \param len length of buffer
 */
static int
tds_file_stream_read(TDSINSTREAM *stream, void *ptr, size_t len)
{
	TDSFILESTREAM *s = (TDSFILESTREAM *) stream;
	int c;
	char *p = (char *) ptr;

	while (len) {
		if (memcmp(s->left, s->terminator - s->left_pos, s->term_len) == 0)
			return p - (char *) ptr;

		c = getc_unlocked(s->f);
		if (c == EOF)
			return -1;

		*p++ = s->left[s->left_pos];
		--len;

		s->left[s->left_pos++] = c;
		s->left_pos %= s->term_len;
	}
	return p - (char *) ptr;
}

/**
 * Read a data file, passing the data through iconv().
 * \retval TDS_SUCCESS  success
 * \retval TDS_FAIL     error reading the column
 * \retval TDS_NO_MORE_RESULTS end of file detected
 */
TDSRET
tds_bcp_fread(TDSSOCKET * tds, TDSICONV * char_conv, FILE * stream, const char *terminator, size_t term_len, char **outbuf, size_t * outbytes)
{
	TDSRET res;
	TDSFILESTREAM r;
	TDSDYNAMICSTREAM w;
	size_t readed;

	/* prepare streams */
	r.stream.read = tds_file_stream_read;
	r.f = stream;
	r.term_len = term_len;
	r.left = tds_new0(char, term_len*3);
	r.left_pos = 0;
	if (!r.left) return TDS_FAIL;

	/* copy terminator twice, let terminator points to second copy */
	memcpy(r.left + term_len, terminator, term_len);
	memcpy(r.left + term_len*2u, terminator, term_len);
	r.terminator = r.left + term_len*2u;

	/* read initial buffer to test with terminator */
	readed = fread(r.left, 1, term_len, stream);
	if (readed != term_len) {
		free(r.left);
		if (readed == 0 && feof(stream))
			return TDS_NO_MORE_RESULTS;
		return TDS_FAIL;
	}

	res = tds_dynamic_stream_init(&w, (void**) outbuf, 0);
	if (TDS_FAILED(res)) {
		free(r.left);
		return res;
	}

	/* convert/copy from input stream to output one */
	flockfile(stream);
	if (char_conv == NULL)
		res = tds_copy_stream(&r.stream, &w.stream);
	else
		res = tds_convert_stream(tds, char_conv, to_server, &r.stream, &w.stream);
	funlockfile(stream);
	free(r.left);

	if (TDS_FAILED(res))
		return res;

	*outbytes = w.size;

	/* terminate buffer */
	if (!w.stream.buf_len)
		return TDS_FAIL;

	((char *) w.stream.buffer)[0] = 0;
	w.stream.write(&w.stream, 1);

	return res;
}

/**
 * Start writing writetext request.
 * This request start a bulk session.
 * \tds
 * \param objname table name
 * \param textptr TEXTPTR (see sql documentation)
 * \param timestamp data timestamp
 * \param with_log is log is enabled during insert
 * \param size bytes to be inserted
 */
TDSRET
tds_writetext_start(TDSSOCKET *tds, const char *objname, const char *textptr, const char *timestamp, int with_log, TDS_UINT size)
{
	TDSRET rc;

	/* TODO mssql does not like timestamp */
	rc = tds_submit_queryf(tds,
			      "writetext bulk %s 0x%s timestamp = 0x%s%s",
			      objname, textptr, timestamp, with_log ? " with log" : "");
	if (TDS_FAILED(rc))
		return rc;

	/* set we want to switch to bulk state */
	tds->bulk_query = true;

	/* read the end token */
	rc = tds_process_simple_query(tds);
	if (TDS_FAILED(rc))
		return rc;

	tds->out_flag = TDS_BULK;
	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_put_int(tds, size);

	tds_set_state(tds, TDS_SENDING);
	return TDS_SUCCESS;
}

/**
 * Send some data in the writetext request started by tds_writetext_start.
 * You should write in total (with multiple calls to this function) all
 * bytes declared calling tds_writetext_start.
 * \tds
 * \param text data to write
 * \param size data size in bytes
 */
TDSRET
tds_writetext_continue(TDSSOCKET *tds, const TDS_UCHAR *text, TDS_UINT size)
{
	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	/* TODO check size left */
	tds_put_n(tds, text, size);

	tds_set_state(tds, TDS_SENDING);
	return TDS_SUCCESS;
}

/**
 * Finish sending writetext data.
 * \tds
 */
TDSRET
tds_writetext_end(TDSSOCKET *tds)
{
	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_flush_packet(tds);
	tds_set_state(tds, TDS_PENDING);
	return TDS_SUCCESS;
}


static int tds_bcp_is_bound(TDSBCPINFO *bcpinfo, TDSCOLUMN *colinfo)
{
        return (bcpinfo  &&  colinfo  &&
                /* Don't interfere with dblib bulk insertion from files. */
                (bcpinfo->xfer_init == 0
                 ||  colinfo->column_varaddr != NULL
                 ||  (colinfo->column_lenbind != NULL
                      &&  (*colinfo->column_lenbind != 0
                           ||  (colinfo->column_nullbind != NULL
                                /* null-value for blk_textxfer ... */
                                /* &&  *colinfo->column_nullbind == -1 */)))));
}
