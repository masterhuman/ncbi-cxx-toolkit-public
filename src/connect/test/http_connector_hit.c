/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Hit an arbitrary URL using HTTP-based CONNECTOR
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_tls.h>
#include <stdlib.h>

#include "test_assert.h"  /* This header must go last */


/* Holder for the cmd.-line arg values describing the URL to hit
 * (used by the pseudo-registry getter "s_REG_Get")
 */
static struct {
    const char* host;
    const char* port;
    const char* path;
    const char* args;
} s_Args;


/* Getter for the pseudo-registry
 */
#if defined(__cplusplus)
extern "C" {
    static int s_REG_Get(void* user_data, const char* section,
                         const char* name, char* value, size_t value_size);
}
#endif /* __cplusplus */

/*ARGSUSED*/
static int s_REG_Get
(void*       unused,
 const char* section,
 const char* name,
 char*       value,
 size_t      value_size)
{
    if (strcmp(section, DEF_CONN_REG_SECTION) != 0) {
        assert(0);
        return 0;
    }

#define X_GET_VALUE(x_name, x_value)                \
    if (strcmp(name, x_name) == 0) {                \
        strncpy0(value, x_value, value_size - 1);   \
        return 1;                                   \
    }

    X_GET_VALUE(REG_CONN_HOST,           s_Args.host);
    X_GET_VALUE(REG_CONN_PORT,           s_Args.port);
    X_GET_VALUE(REG_CONN_PATH,           s_Args.path);
    X_GET_VALUE(REG_CONN_ARGS,           s_Args.args);
    X_GET_VALUE(REG_CONN_DEBUG_PRINTOUT, "yes");

    return -1;
}



/*****************************************************************************
 *  MAIN
 */

int main(int argc, const char* argv[])
{
    const char* inp_file    = (argc > 5) ? argv[5] : "";
    const char* user_header = (argc > 6) ? argv[6] : "";

    SConnNetInfo* net_info;
    CONNECTOR     connector;
    CONN          conn;
    EIO_Status    status;

    char   buffer[100];
    size_t n_read, n_written;

    /* Prepare to connect:  parse and check cmd.-line args, etc. */
    s_Args.host         = (argc > 1) ? argv[1] : "";
    s_Args.port         = (argc > 2) ? argv[2] : "";
    s_Args.path         = (argc > 3) ? argv[3] : "";
    s_Args.args         = (argc > 4) ? argv[4] : "";

    fprintf(stderr, "Running '%s':\n"
            "  URL host:        '%s'\n"
            "  URL port:        '%s'\n"
            "  URL path:        '%s'\n"
            "  URL args:        '%s'\n"
            "  Input data file: '%s'\n"
            "  User header:     '%s'\n"
            "Reply(if any) from the hit URL goes to the standard output.\n\n",
            argv[0],
            s_Args.host, s_Args.port, s_Args.path, s_Args.args,
            inp_file, user_header);

    /* Log stream */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    /* Tune to the test URL using hard-coded pseudo-registry */
    CORE_SetREG( REG_Create(0, s_REG_Get, 0, 0, 0) );

    SOCK_SetupSSL(NcbiSetupTls);

    /* Usage */
    if (argc < 4) {
        fprintf(stderr,
                "Usage:   %s host port path [args] [inp_file] [user_header]\n"
                "Example: %s www.ncbi.nlm.nih.gov 80 "
                "/Service/bounce.cgi 'arg1+arg2+arg3'\n",
                argv[0], argv[0]);
        CORE_LOG(eLOG_Fatal, "Too few arguments");
        /*NOTREACHED*/
        return 1;
    }

    /* Connect */
    if (atoi(s_Args.port) == CONN_PORT_HTTPS) {
        verify((net_info = ConnNetInfo_Create(0)) != 0);
        net_info->scheme = eURL_Https;
    } else
        net_info = 0;

    connector = HTTP_CreateConnector(net_info, user_header, 0);
    assert(connector);
    verify(CONN_Create(connector, &conn) == eIO_Success);

    ConnNetInfo_Destroy(net_info);

    /* If input file specified, then send its content (as HTTP request body) */
    if (*inp_file) {
        FILE* inp_fp;

        if (strcmp(inp_file, "-") != 0) {
            static const char kDevNull[] =
#ifdef NCBI_OS_MSWIN
                "NUL"
#else
                "/dev/null"
#endif /*NCBI_OS_MSWIN*/
                ;
            if (strcmp(inp_file, "+") == 0)
                inp_file = kDevNull;
            if (!(inp_fp = fopen(inp_file, "rb"))) {
                CORE_LOGF(eLOG_Fatal,
                          ("Cannot open file '%s' for reading", inp_file));
                /*NOTREACHED*/
                return 1;
            }
        } else
            inp_fp = stdin;

        do {
            n_read = fread(buffer, 1, sizeof(buffer), inp_fp);
            if (n_read <= 0)
                break; /* EOF */

            status = CONN_Write(conn, buffer, n_read,
                                &n_written, eIO_WritePersist);
            if (status != eIO_Success) {
                CORE_LOGF(eLOG_Fatal,
                          ("Unable to write to URL: %s",IO_StatusStr(status)));
            }
            assert(n_written == n_read);
        } while (n_read == sizeof(buffer));
        assert(feof(inp_fp));
        fclose(inp_fp);
    }

    /* Read reply from connection, write it to standard output */
    for (;;) {
        status = CONN_Read(conn,buffer,sizeof(buffer),&n_read,eIO_ReadPlain);
        if (status != eIO_Success)
            break;
        if (connector)
            puts("----- [BEGIN] HTTP Content -----");
        fwrite(buffer, 1, n_read, stdout);
        fflush(stdout);
        connector = 0;
    }
    if (!connector) {
        puts("\n----- [END] HTTP Content -----");
        fclose(stdout);
    }

    if (status != eIO_Closed) {
        CORE_LOGF(eLOG_Fatal,
                  ("Unable to read from URL: %s", IO_StatusStr(status)));
    }

    /* Success:  close the connection, cleanup, and exit */
    CONN_Close(conn);
    CORE_SetREG(0);
    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
