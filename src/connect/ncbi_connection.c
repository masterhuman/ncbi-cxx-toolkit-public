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
 * Authors:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Generic API to open and handle connection to an abstract service.
 *   For more detail, see in "ncbi_connection.h".
 *
 */

#include "ncbi_priv.h"
#include "ncbi_socketp.h"
#include <connect/ncbi_connection.h>
#include <stdlib.h>
#include <string.h>

#define NCBI_USE_ERRCODE_X   Connect_Conn


#define CONNECTION_MAGIC     0xEFCDAB09


/***********************************************************************
 *  INTERNAL
 ***********************************************************************/

/* Standard logging message
 */
#define CONN_LOG_EX(subcode, func_name, level, message, status)         \
  do {                                                                  \
      const char* ststr = ((EIO_Status) status != eIO_Success           \
                           ? IO_StatusStr((EIO_Status) status)          \
                           : "");                                       \
      const char* ctype = (conn  &&  conn->meta.get_type                \
                           ? conn->meta.get_type(conn->meta.c_get_type) \
                           : 0);                                        \
      char* descr = (conn  &&  conn->meta.descr                         \
                     ? conn->meta.descr(conn->meta.c_descr)             \
                     : 0);                                              \
      char stbuf[80];                                                   \
      if ((EIO_Status) status == eIO_Timeout  &&  timeout) {            \
          assert(timeout != kDefaultTimeout);                           \
          sprintf(stbuf, "%s[%u.%06u]", ststr,                          \
                  timeout->usec / 1000000 + timeout->sec,               \
                  timeout->usec % 1000000);                             \
          assert(strlen(stbuf) < sizeof(stbuf));                        \
          ststr = stbuf;                                                \
      }                                                                 \
      CORE_LOGF_X(subcode, level,                                       \
                  ("[CONN_" #func_name "(%s%s%s)]  %s%s%s",             \
                   ctype  &&  *ctype ? ctype : "UNDEF",                 \
                   descr  &&  *descr ? "; "  : "", descr ? descr : "",  \
                   message,                                             \
                   ststr  &&  *ststr ? ": "  : "",                      \
                   ststr             ? ststr : ""));                    \
      if (descr)                                                        \
          free(descr);                                                  \
  } while (0)

#define CONN_LOG(s_c, f_n, lvl, msg)  CONN_LOG_EX(s_c, f_n, lvl, msg, status)

#if 0
#  define CONN_CALLTRACE(func_name)                             \
  do {                                                          \
      static const STimeout* timeout = 0/*dummy*/;              \
      char x_handle[80];                                        \
      sprintf(x_handle, "0x%p", conn);                          \
      CONN_LOG_EX(0, func_name, eLOG_Trace, x_handle, 0);       \
  } while (0)
#else
#  define CONN_CALLTRACE(func_name)  /*((void) 0)*/
#endif /*0*/


/* Standard macros to verify that the connection handle is not NULL and valid.
 * NB: "retval" must be either a valid EIO_Status or 0 (no status logged)
 */
#define CONN_NOT_NULL_EX(subcode, func_name, retval)                    \
  do {                                                                  \
      CONN_CALLTRACE(func_name);                                        \
      if (!conn) {                                                      \
          static const STimeout* timeout = 0/*dummy*/;                  \
          CONN_LOG_EX(subcode, func_name, eLOG_Error,                   \
                      "NULL connection handle", retval);                \
          assert(conn);                                                 \
          return retval;                                                \
      }                                                                 \
      if (conn->magic != CONNECTION_MAGIC) {                            \
          static const STimeout* timeout = 0/*dummy*/;                  \
          char x_errmsg[80];                                            \
          sprintf(x_errmsg, "Corrupt connection handle 0x%p", conn);    \
          CONN_LOG_EX(subcode, func_name, eLOG_Critical, x_errmsg, 0);  \
          assert(0);                                                    \
          return retval;                                                \
      }                                                                 \
  } while (0)

#define CONN_NOT_NULL(s_c, f_n)  CONN_NOT_NULL_EX(s_c, f_n, eIO_InvalidArg)

#ifdef _DEBUG
#  define CONN_TRACE(f_n, msg)   CONN_LOG(0, f_n, eLOG_Trace, msg)
#else
#  define CONN_TRACE(f_n, msg)   /*((void) 0)*/
#endif /*_DEBUG*/


/* Private flags, MUST NOT cross with ECONN_Flags defined in the header
 */
enum ECONN_InternalFlag {
    fCONN_Flush = 1024           /* auto-flush was successful                */
};


/* Connection state
 */
typedef enum {
    eCONN_Unusable = -1,         /* iff !conn->meta.list (uninited state)    */
    eCONN_Closed   =  0,         /* "Open" can be attempted                  */
    eCONN_Open     =  1,         /* operational state (I/O allowed)          */
    eCONN_Bad      =  2,         /* non-operational (I/O not allowed)        */
    eCONN_Corrupt  =  3,         /* NB: |= eCONN_Open|eCONN_Bad, no more I/O */
    eCONN_Canceled =  5          /* NB: |= eCONN_Open, user-canceled         */
} ECONN_State;


/* Connection internal data.
 *
 * NOTE:  "meta" *must* come first!  (see ncbi_conn_streambuf.cpp)
 */
typedef struct SConnectionTag {
    SMetaConnector  meta;        /* VTable of operations and list            */

    ECONN_State     state;       /* connection state                         */
    TCONN_Flags     flags;       /* connection flags                         */
    EIO_Status      r_status;    /* I/O status of last read                  */
    EIO_Status      w_status;    /* I/O status of last write                 */

    BUF             buf;         /* storage for peek/pushback data           */

    void*           data;        /* user data pointer                        */

    /* "[o|r|w|c]_timeout" is either 0 (kInfiniteTimeout), kDefaultTimeout
       (to use connector-specific one), or points to "[oo|rr|ww|cc]_timeout";
       timeouts are "as is" (may be not normalized) for performance reasons  */
    const STimeout*  o_timeout;  /* timeout on open                          */
    const STimeout*  r_timeout;  /* timeout on read                          */
    const STimeout*  w_timeout;  /* timeout on write                         */
    const STimeout*  c_timeout;  /* timeout on close                         */
    STimeout        oo_timeout;  /* storage for "o_timeout"                  */
    STimeout        rr_timeout;  /* storage for "r_timeout"                  */
    STimeout        ww_timeout;  /* storage for "w_timeout"                  */
    STimeout        cc_timeout;  /* storage for "c_timeout"                  */

    TNCBI_BigCount  r_pos;       /* read and ...                             */
    TNCBI_BigCount  w_pos;       /*          ... write positions             */

    SCONN_Callback  cb[CONN_N_CALLBACKS+1]; /* +1 includes catchall callback */

    unsigned int    magic;       /* magic cookie for integrity checks        */
} SConnection;


/*ARGSUSED*/
static EIO_Status x_CatchallCallback(CONN conn,TCONN_Callback type,void* data)
{
    static const STimeout* timeout = 0/*dummy*/;
    char errmsg[80];
    sprintf(errmsg, "Unknown callback #%u for 0x%p, assume corruption",
            type, conn);
    CONN_LOG_EX(37, CALLBACK, eLOG_Critical, errmsg, 0);
    assert(0);
    conn->state = eCONN_Corrupt;
    return eIO_InvalidArg;
}


static size_t x_CB2IDX(ECONN_Callback type)
{
    size_t idx;
    switch (type) {
    case eCONN_OnClose:
        idx = 0;
        break;
    case eCONN_OnRead:
        idx = 1;
        break;
    case eCONN_OnWrite:
        idx = 2;
        break;
    case eCONN_OnFlush:
        idx = 3;
        break;
    case eCONN_OnTimeout:
        idx = 4;
        break;
    case eCONN_OnOpen:
        idx = 5;
        break;
    default:
        /* to flag an API error via x_CatchallCallback() */
        idx = CONN_N_CALLBACKS;
        break;
    }
    return idx;
}


static EIO_Status x_Callback(CONN conn, ECONN_Callback type, unsigned int flag)
{
    int/*bool*/    io = (type | flag) & eIO_ReadWrite;
    size_t         idx = x_CB2IDX(type);
    FCONN_Callback func;
    void*          data;
    EIO_Status     status;

    assert(!flag  ||  (type == eCONN_OnTimeout
                       &&  (flag | eIO_ReadWrite) == eIO_ReadWrite));
    assert(conn  &&  idx < CONN_N_CALLBACKS);
    if (conn->state != eCONN_Open  &&  io)
        return eIO_Unknown;
    if (!(func = conn->cb[idx].func))
        return type == eCONN_OnTimeout ? eIO_Timeout : eIO_Success;
    data = conn->cb[idx].data;
    status = (*func)(conn, (TCONN_Callback) type | flag, data);
    if (status == eIO_Success) {
        if (type == eCONN_OnOpen)
            status = eIO_Reserved;
        else if (conn->state != eCONN_Open  &&  io)
            status = eIO_Unknown;
    } else if (status == eIO_Interrupt)
        conn->state = eCONN_Canceled;
    return status;
}


static EIO_Status x_Flush(CONN conn, const STimeout* timeout,
                          int/*bool*/ isflush)
{
    assert(isflush  ||  !(conn->flags & fCONN_Flush));
    for (;;) {
        EIO_Status status;
        if ((status = x_Callback(conn, eCONN_OnFlush, 0)) != eIO_Success)
            return status;

        if (!conn->meta.flush)
            break;
        /* call current connector's "FLUSH" method */
        if (timeout == kDefaultTimeout)
            timeout  = conn->meta.default_timeout;
        assert(timeout != kDefaultTimeout);
        status = conn->meta.flush(conn->meta.c_flush, timeout);
        if (isflush)
            conn->w_status = status;
        if (status == eIO_Success)
            break;

        if (status == eIO_Timeout)
            status  = x_Callback(conn, eCONN_OnTimeout, eIO_ReadWrite);
        if (status != eIO_Success)
            return status;
    }
    conn->flags |= fCONN_Flush;
    return eIO_Success;
}


static EIO_Status x_ReInit(CONN conn, CONNECTOR connector, int/*bool*/ close)
{
    const STimeout* timeout = 0;
    EIO_Status status;
    CONNECTOR  x_conn;

    assert(!close  ||  !connector);
    assert(!conn->meta.list == !(conn->state != eCONN_Unusable));

    /* flush connection first, if open & not flushed */
    status = conn->meta.list 
        &&  conn->state == eCONN_Open
        &&  !(conn->flags & fCONN_Flush)
        ? x_Flush(conn, conn->c_timeout, 0/*no-flush*/) : eIO_Success;

    for (x_conn = conn->meta.list;  x_conn;  x_conn = x_conn->next) {
        assert(conn->state != eCONN_Unusable);
        if (x_conn == connector) {
            assert(!close);
            /* re-init with the same and the only connector is allowed */
            if (!x_conn->next  &&  x_conn == conn->meta.list)
                break;
            status = eIO_NotSupported;
            CONN_LOG(4, ReInit, eLOG_Critical, "Partial re-init not allowed");
            conn->state = eCONN_Corrupt;
            return status;
        }
    }

    if (conn->meta.list) {
        assert(conn->state != eCONN_Unusable);
        /* erase unread data */
        BUF_Erase(conn->buf);

        if (!x_conn)
            status = x_Callback(conn, eCONN_OnClose, 0);
        /* else: re-init with same connector does not cause the callback */

        if (conn->state & eCONN_Open) {
            /* call current connector's "CLOSE" method */
            if (conn->meta.close) {
                EIO_Status closed;
                timeout = (conn->c_timeout == kDefaultTimeout
                           ? conn->meta.default_timeout
                           : conn->c_timeout);
                assert(timeout != kDefaultTimeout);
                closed = conn->meta.close(conn->meta.c_close, timeout);
                if (closed != eIO_Success)
                    status  = closed;
            }
            if (status != eIO_Success
                &&  (status != eIO_Closed  ||  connector)) {
                if (close) {
                    CONN_LOG(3, Close,
                             connector ? eLOG_Error : eLOG_Warning,
                             "Connection failed to close properly");
                } else {
                    CONN_LOG(3, ReInit, eLOG_Error,
                             "Connection failed to close properly");
                }
            }
        }
        /* HERE: conn->state = eCONN_Closed; */

        if (!x_conn) {
            /* entirely new connector - remove the old connector stack first */
            METACONN_Remove(&conn->meta, 0); /* NB: always succeeds with "0" */
            assert(!conn->meta.list);
            memset(&conn->meta, 0, sizeof(conn->meta));
            conn->state = eCONN_Unusable;
        } else if (status != eIO_Success) {
            /* the incumbent connector's re-init failed */
            conn->state = eCONN_Bad;
            assert(!close);
            return status;
        } else
            conn->state = eCONN_Closed;
    } else if (!connector)
        status = eIO_Closed;  /*NB: conn->state == eCONN_Unusable */

    if (!x_conn  &&  connector) {
        assert(conn->state == eCONN_Unusable);
        /* setup the new connector */
        if ((status = METACONN_Insert(&conn->meta, connector)) != eIO_Success)
            return status;
        assert(conn->meta.list);
        conn->state = eCONN_Closed;
        assert(conn->meta.default_timeout != kDefaultTimeout);
    }

    assert(conn->state == eCONN_Unusable  ||  conn->state == eCONN_Closed);
    return status;
}


static EIO_Status s_Open(CONN conn)
{
    const STimeout* timeout = 0;
    EIO_Status      status;

    switch (conn->state) {
    case eCONN_Unusable:
        return eIO_InvalidArg;
    case eCONN_Canceled:
        return eIO_Interrupt;
    case eCONN_Corrupt:
        return eIO_Unknown;
    case eCONN_Bad:
        return eIO_Closed;
    default:
        break;
    }
    assert(conn->state == eCONN_Closed  &&  conn->meta.list);
    conn->r_pos = 0;
    conn->w_pos = 0;

    if (conn->meta.open) {
        for (;;) {
            int/*bool*/ nocb = 1/*true = no callback*/;
            if ((status = x_Callback(conn, eCONN_OnOpen, 0)) == eIO_Reserved)
                nocb = 0/*false*/;
            else if (status != eIO_Success)
                break;

            /* call current connector's "OPEN" method */
            timeout = (conn->o_timeout == kDefaultTimeout
                       ? conn->meta.default_timeout
                       : conn->o_timeout);
            assert(timeout != kDefaultTimeout);
            status = conn->meta.open(conn->meta.c_open, timeout);
            if (status != eIO_Timeout  ||  nocb)
                break;

            /* eCONN_OnTimeout gets called only if eCONN_OnOpen was there */
            status = x_Callback(conn, eCONN_OnTimeout, eIO_Open);
            if (status != eIO_Success)
                break;
        }
    } else
        status  = eIO_NotSupported;
    if (status == eIO_Success) {
        conn->flags   |= fCONN_Flush;
        conn->r_status = eIO_Success;
        conn->w_status = eIO_Success;
        conn->state    = eCONN_Open;
    } else {
        CONN_LOG(5, Open, eLOG_Error, "Failed to open connection");
        if (conn->state == eCONN_Closed)
            conn->state  = eCONN_Bad;
    }
    return status;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern EIO_Status CONN_CreateEx
(CONNECTOR   connector,
 TCONN_Flags flags,
 CONN*       connection)
{
    EIO_Status status;
    CONN       conn;

    if (connector) {
        conn = (SConnection*) calloc(1, sizeof(SConnection));

        if (conn) {
            conn->flags     = flags & (TCONN_Flags)(~fCONN_Flush);
            conn->state     = eCONN_Unusable;
            conn->o_timeout = kDefaultTimeout;
            conn->r_timeout = kDefaultTimeout;
            conn->w_timeout = kDefaultTimeout;
            conn->c_timeout = kDefaultTimeout;
            conn->magic     = CONNECTION_MAGIC;
            conn->cb[CONN_N_CALLBACKS].func = x_CatchallCallback;
            if ((status = x_ReInit(conn, connector, 0)) != eIO_Success) {
                conn->magic = (unsigned int)(-1);
                free(conn);
                conn = 0;
            }
        } else
            status = eIO_Unknown;
    } else {
        static const STimeout* timeout = 0/*dummy*/;
        conn = 0;
        status = eIO_InvalidArg;
        CONN_LOG(2, Create, eLOG_Error, "NULL connector");
    }

    assert(!conn == !(status == eIO_Success));
    CONN_CALLTRACE(Create);
    *connection = conn;
    return status;
}


extern EIO_Status CONN_Create
(CONNECTOR connector,
 CONN*     conn)
{
    return CONN_CreateEx(connector, 0, conn);
}


extern EIO_Status CONN_ReInit
(CONN      conn,
 CONNECTOR connector)
{
    CONN_NOT_NULL(1, ReInit);

    return x_ReInit(conn, connector, 0/*reinit*/);
}


extern const char* CONN_GetType(CONN conn)
{
    CONN_NOT_NULL_EX(6, GetType, 0);

    assert(!conn->meta.list == !(conn->state != eCONN_Unusable));
    return !conn->meta.list  ||  !conn->meta.get_type
        ? 0 : conn->meta.get_type(conn->meta.c_get_type);
}


extern char* CONN_Description(CONN conn)
{
    CONN_NOT_NULL_EX(7, Description, 0);

    assert(!conn->meta.list == !(conn->state != eCONN_Unusable));
    return !conn->meta.list  ||  !conn->meta.descr
        ? 0 : conn->meta.descr(conn->meta.c_descr);
}


extern TNCBI_BigCount CONN_GetPosition(CONN conn, EIO_Event event)
{
    static const STimeout* timeout = 0/*dummy*/;
    TNCBI_BigCount pos;
    char errbuf[80];

    CONN_NOT_NULL_EX(30, GetPosition, 0);

    switch (event) {
    case eIO_Open:
        conn->r_pos = 0;
        conn->w_pos = 0;
        pos = 0;
        break;
    case eIO_Read:
        pos = conn->r_pos;
        break;
    case eIO_Write:
        pos = conn->w_pos;
        break;
    default:
        sprintf(errbuf, "Unknown direction #%u", (unsigned int) event);
        CONN_LOG_EX(31, GetPosition, eLOG_Error, errbuf, 0);
        assert(0);
        pos = 0;
        break;
    }
    return conn->state == eCONN_Unusable ? 0 : pos;
}


extern EIO_Status CONN_SetTimeout
(CONN            conn,
 EIO_Event       event,
 const STimeout* timeout)
{
    char errbuf[80];

    CONN_NOT_NULL(8, SetTimeout);

    switch (event) {
    case eIO_Open:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->oo_timeout != timeout)
                conn->oo_timeout = *timeout;
            conn->o_timeout  = &conn->oo_timeout;
        } else
            conn->o_timeout  = timeout;
        break;
    case eIO_Read:
    case eIO_ReadWrite:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->rr_timeout != timeout)
                conn->rr_timeout = *timeout;
            conn->r_timeout  = &conn->rr_timeout;
        } else
            conn->r_timeout  = timeout;
        if (event != eIO_ReadWrite)
            break;
        /*FALLTHRU*/
    case eIO_Write:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->ww_timeout != timeout)
                conn->ww_timeout = *timeout;
            conn->w_timeout  = &conn->ww_timeout;
        } else
            conn->w_timeout  = timeout;
        break;
    case eIO_Close:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->cc_timeout != timeout)
                conn->cc_timeout = *timeout;
            conn->c_timeout  = &conn->cc_timeout;
        } else
            conn->c_timeout  = timeout;
        break;
    default:
        sprintf(errbuf, "Unknown event #%u", (unsigned int) event);
        CONN_LOG_EX(9, SetTimeout, eLOG_Error, errbuf, eIO_InvalidArg);
        assert(0);
        return eIO_InvalidArg;
    }

    return eIO_Success;
}


extern const STimeout* CONN_GetTimeout
(CONN      conn,
 EIO_Event event)
{
    const STimeout* timeout;
    char errbuf[80];

    CONN_NOT_NULL_EX(10, GetTimeout, 0);

    switch (event) {
    case eIO_Open:
        timeout = conn->o_timeout;
        break;
    case eIO_ReadWrite:
        timeout = 0/*dummy*/;
        CONN_LOG_EX(11, GetTimeout, eLOG_Warning,
                    "ReadWrite timeout requested", 0);
        /*FALLTHRU*/
    case eIO_Read:
        timeout = conn->r_timeout;
        break;
    case eIO_Write:
        timeout = conn->w_timeout;
        break;
    case eIO_Close:
        timeout = conn->c_timeout;
        break;
    default:
        timeout = 0;
        sprintf(errbuf, "Unknown event #%u", (unsigned int) event);
        CONN_LOG_EX(12, GetTimeout, eLOG_Error, errbuf, 0);
        assert(0);
        break;
    }

    return timeout;
}


extern EIO_Status CONN_Wait
(CONN            conn,
 EIO_Event       event,
 const STimeout* timeout)
{
    EIO_Status status;

    CONN_NOT_NULL(13, Wait);

    if (event != eIO_Read  &&  event != eIO_Write) {
        assert(0);
        return eIO_InvalidArg;
    }

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list);

    /* check if there is a PEEK'ed data in the input */
    if (event == eIO_Read  &&  BUF_Size(conn->buf))
        return eIO_Success;

    /* call current connector's "WAIT" method */
    if (timeout == kDefaultTimeout) {
        timeout  = conn->meta.default_timeout;
        assert(timeout != kDefaultTimeout);
    }
    status = conn->meta.wait
        ? conn->meta.wait(conn->meta.c_wait, event, timeout)
        : eIO_NotSupported;

    if (status != eIO_Success) {
        static const char* kErrMsg[] = { "Read event failed",
                                         "Write event failed" };
        ELOG_Level level;
        switch (status) {
        case eIO_Timeout:
            if (!timeout/*==kInfiniteTimeout,impossible*/)
                level = eLOG_Critical;
            else if (timeout->sec | timeout->usec)
                level = eLOG_Trace;
            else
                return status;
            break;
        case eIO_Closed:
            level = event == eIO_Read ? eLOG_Trace : eLOG_Error;
            break;
        case eIO_Interrupt:
            level = eLOG_Warning;
            break;
        default:
            level = eLOG_Error;
            break;
        }
        CONN_LOG(event != eIO_Read ? 15 : 14, Wait, level,
                 kErrMsg[event != eIO_Read]);
    }
    return status;
}


static EIO_Status s_CONN_Write
(CONN         conn,
 const void*  data,
 const size_t size,
 size_t*      n_written)
{
    const STimeout* timeout;
    EIO_Status status;

    assert(*n_written == 0);

    for (timeout = 0; ; timeout = 0) {
        FConnectorWrite writefunc;

        if ((status = x_Callback(conn, eCONN_OnWrite, 0)) != eIO_Success)
            break;

        /* call current connector's "WRITE" method */
        if (!(writefunc = conn->meta.write)) {
            status = eIO_NotSupported;
            CONN_LOG(16, Write, eLOG_Critical, "Cannot write data");
            return status;
        }
        timeout = (conn->w_timeout == kDefaultTimeout
                   ? conn->meta.default_timeout
                   : conn->w_timeout);
        assert(timeout != kDefaultTimeout);
        status = writefunc(conn->meta.c_write, data, size,
                           n_written, timeout);
        assert(status != eIO_Success  ||  *n_written  ||  !size);
        assert(*n_written <= size);
        conn->w_status = status;

        if (*n_written) {
            conn->w_pos += *n_written;
            conn->flags &= (TCONN_Flags)(~fCONN_Flush);
            break;
        }
        if (!size  ||  status != eIO_Timeout)
            break;

        status = x_Callback(conn, eCONN_OnTimeout, eIO_Write);
        if (status != eIO_Success)
            break;
    }

    if (status != eIO_Success) {
        if (*n_written) {
            CONN_TRACE(Write, "Write error");
            /*status = eIO_Success;*/
        } else if (size) {
            ELOG_Level level;
            if (status != eIO_Timeout  ||  conn->w_timeout == kDefaultTimeout)
                level = eLOG_Error;
            else if (!timeout/*impossible*/ || (timeout->sec | timeout->usec))
                level = eLOG_Warning;
            else
                level = eLOG_Trace;
            CONN_LOG(17, Write, level, "Unable to write data");
        }
    }
    return status;
}


static EIO_Status s_CONN_WritePersist
(CONN         conn,
 const void*  data,
 const size_t size,
 size_t*      n_written)
{
    EIO_Status status;

    assert(*n_written == 0);

    do {
        size_t x_written = 0;
        status = s_CONN_Write(conn, (char*) data + *n_written,
                              size - *n_written, &x_written);
        *n_written += x_written;
        assert(*n_written <= size);
        if (!size)
            break;
        if (*n_written == size)
            return conn->flags & fCONN_Supplement ? status : eIO_Success;
    } while (status == eIO_Success);

    return status;
}


extern EIO_Status CONN_Write
(CONN            conn,
 const void*     data,
 size_t          size,
 size_t*         n_written,
 EIO_WriteMethod how)
{
    EIO_Status status;

    if (!n_written) {
        assert(0);
        return eIO_InvalidArg;
    }
    *n_written = 0;
    if (size  &&  !data)
        return eIO_InvalidArg;

    CONN_NOT_NULL(18, Write);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list);

    switch (how) {
    case eIO_WritePlain:
        status = s_CONN_Write(conn, data, size, n_written);
        break;
    case eIO_WritePersist:
        return s_CONN_WritePersist(conn, data, size, n_written);
    default:
        assert(0);
        return eIO_NotSupported;
    }

    if (conn->flags & fCONN_Supplement)
        return status;
    return *n_written ? eIO_Success : status;
}


extern EIO_Status CONN_Pushback
(CONN        conn,
 const void* data,
 size_t      size)
{
    if (size  &&  !data) {
        assert(0);
        return eIO_InvalidArg;
    }

    CONN_NOT_NULL(19, Pushback);

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Canceled)
        return eIO_Interrupt;

    if (conn->state != eCONN_Open)
        return eIO_Closed;

    return BUF_Pushback(&conn->buf, data, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status CONN_Flush
(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL(20, Flush);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list);

    status = x_Flush(conn, conn->w_timeout, 1/*flush*/);
    if (status != eIO_Success) {
        /* this is only for the log message */
        const STimeout* timeout = status != eIO_Timeout ? 0
            : (conn->w_timeout == kDefaultTimeout
               ? conn->meta.default_timeout
               : conn->w_timeout);
        assert(timeout != kDefaultTimeout);
        CONN_LOG(21, Flush, status == eIO_Timeout ? eLOG_Trace : eLOG_Warning,
                 "Failed to flush");
    }
    return status;
}


/* Read or peek data from the input queue, see CONN_Read()
 */
static EIO_Status s_CONN_Read
(CONN         conn,
 void*        buf,
 const size_t size,
 size_t*      n_read,
 int/*bool*/  peek)
{
    const STimeout* timeout;
    EIO_Status status;

    assert(*n_read == 0);

    for (timeout = 0; ; timeout = 0) {
        FConnectorRead readfunc;
        size_t x_read;

        /* keep flushing pending unwritten output data, if any */
        if (!(conn->flags & (fCONN_Untie | fCONN_Flush))) {
            status = x_Flush(conn, conn->r_timeout, 0/*no-flush*/);
            if (status == eIO_Interrupt)
                break;
            /* else ignore any other status here (success or error) */
        }

        /* read data from the internal peek/pushback buffer, if any */
        if (size) {
            x_read = (peek
                      ? BUF_Peek(conn->buf, buf, size - *n_read)
                      : BUF_Read(conn->buf, buf, size - *n_read));
            *n_read += x_read;
            if (x_read  &&  (*n_read == size  ||  !peek)) {
                status = eIO_Success;
                break;
            }
            buf = (char*) buf + x_read;
        }

        if ((status = x_Callback(conn, eCONN_OnRead, 0)) != eIO_Success)
            break;

        /* call current connector's "READ" method */
        if (!(readfunc = conn->meta.read)) {
            if (!*n_read) {
                status = eIO_NotSupported;
                CONN_LOG(22, Read, eLOG_Critical, "Cannot read data");
            } else {
                status = eIO_Closed;
                assert(peek);
            }
            break;
        }
        timeout = (conn->r_timeout == kDefaultTimeout
                   ? conn->meta.default_timeout
                   : conn->r_timeout);
        assert(timeout != kDefaultTimeout);
        x_read = 0;
        status = readfunc(conn->meta.c_read, buf, size - *n_read,
                          &x_read, timeout);
        assert(status != eIO_Success  ||  x_read  ||  !size);
        assert(x_read <= size - *n_read);
        conn->r_status = status;

        if (x_read) {
            *n_read     += x_read;
            conn->r_pos += x_read;
            /* save data in the internal peek buffer */
            if (conn->state == eCONN_Open  &&  peek
                &&  !BUF_Write(&conn->buf, buf, x_read)) {
                status = eIO_Unknown;
                CONN_LOG_EX(32, Read, eLOG_Critical,
                            "Cannot save peek data", 0);
                conn->state  = eCONN_Corrupt;
            }
            break;
        }
        if (!size  ||  *n_read  ||  status != eIO_Timeout)
            break;

        status = x_Callback(conn, eCONN_OnTimeout, eIO_Read);
        if (status != eIO_Success)
            break;
    }

    if (status != eIO_Success) {
        if (*n_read) {
            CONN_TRACE(Read, "Read error");
            /*status = eIO_Success;*/
        } else if (size  &&  status != eIO_Closed) {
            ELOG_Level level;
            if (status != eIO_Timeout  ||  conn->r_timeout == kDefaultTimeout)
                level = eLOG_Error;
            else if (!timeout/*impossible*/ || (timeout->sec | timeout->usec))
                level = eLOG_Warning;
            else
                level = eLOG_Trace;
            CONN_LOG(23, Read, level, "Unable to read data");
        }
    }
    return status;
}


/* Persistently read data from the input queue, see CONN_Read()
 */
static EIO_Status s_CONN_ReadPersist
(CONN         conn,
 void*        buf,
 const size_t size,
 size_t*      n_read)
{
    EIO_Status status;

    assert(*n_read == 0);

    do {
        size_t x_read = 0;
        status = s_CONN_Read(conn, (char*) buf + *n_read,
                             size - *n_read, &x_read, 0/*read*/);
        *n_read += x_read;
        assert(*n_read <= size);
        if (!size)
            break;
        if (*n_read == size)
            return conn->flags & fCONN_Supplement ? status : eIO_Success;
    } while (status == eIO_Success);

    return status;
}


extern EIO_Status CONN_Read
(CONN           conn,
 void*          buf,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how)
{
    EIO_Status status;

    if (!n_read) {
        assert(0);
        return eIO_InvalidArg;
    }
    *n_read = 0;
    if (size  &&  !buf)
        return eIO_InvalidArg;

    CONN_NOT_NULL(24, Read);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status == eIO_Closed ? eIO_Unknown : status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list);

    /* now do read */
    switch (how) {
    case eIO_ReadPeek:
        status = s_CONN_Read(conn, buf, size, n_read, 1/*peek*/);
        break;
    case eIO_ReadPlain:
        status = s_CONN_Read(conn, buf, size, n_read, 0/*read*/);
        break;
    case eIO_ReadPersist:
        return s_CONN_ReadPersist(conn, buf, size, n_read);
    default:
        assert(0);
        return eIO_NotSupported;
    }

    if (conn->flags & fCONN_Supplement)
        return status;
    return *n_read ? eIO_Success : status;
}


extern EIO_Status CONN_ReadLine
(CONN    conn,
 char*   line,
 size_t  size,
 size_t* n_read
 )
{
    EIO_Status  status;
    int/*bool*/ done;
    size_t      len;

    if (!n_read) {
        assert(0);
        return eIO_InvalidArg;
    }
    *n_read = 0;
    if (!size  ||  !line)
        return eIO_InvalidArg;

    CONN_NOT_NULL(25, ReadLine);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status == eIO_Closed ? eIO_Unknown : status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list);

    len = 0;
    done = 0/*false*/;
    do {
        char   w[1024];
        size_t i, x_size, x_read = 0;
        char*  x_buf = size - len < sizeof(w) ? w : line + len;

        if (!(x_size = BUF_Size(conn->buf))  ||  x_size > sizeof(w))
            x_size = sizeof(w);
        status = s_CONN_Read(conn, x_buf, x_size, &x_read, 0);
        assert(x_read <= x_size);

        i = 0;
        while (i < x_read  &&  len < size) {
            char c = x_buf[i++];
            if (c == '\n') {
                status = eIO_Success;
                done = 1/*true*/;
                break;
            }
            if (x_buf == w)
                line[len] = c;
            ++len;
        }
        if (len >= size) {
            /* out of room */
            assert(!done  &&  len);
            done = 1/*true*/;
        }
        if (i < x_read) {
            /* pushback excess */
            assert(done);
            if (conn->state == eCONN_Open
                &&  !BUF_Pushback(&conn->buf, x_buf + i, x_read - i)) {
                static const STimeout* timeout = 0/*dummy*/;
                status = eIO_Unknown;
                CONN_LOG_EX(35, ReadLine, eLOG_Critical,
                            "Cannot pushback extra data", 0);
                conn->state  = eCONN_Corrupt;
            }
            break;
        }
    } while (!done  &&  status == eIO_Success);

    if (len < size)
        line[len] = '\0';
    *n_read = len;

    return done  &&  !(conn->flags & fCONN_Supplement) ? eIO_Success : status;
}


extern EIO_Status CONN_Status(CONN conn, EIO_Event dir)
{
    CONN_NOT_NULL(26, Status);

    if (dir != eIO_Open  &&  (dir & ~eIO_ReadWrite))
        return eIO_InvalidArg;

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Canceled)
        return eIO_Interrupt;

    if (conn->state == eCONN_Corrupt)
        return eIO_Unknown;

    if (conn->state != eCONN_Open)
        return dir == eIO_Read ? eIO_Unknown : eIO_Closed;

    switch (dir) {
    case eIO_ReadWrite:
        conn->r_status = eIO_Success;
        conn->w_status = eIO_Success;
        /*FALLTHRU*/
    case eIO_Open:
        return eIO_Success;
    case eIO_Read:
        if (conn->r_status != eIO_Success)
            return conn->r_status;
        break;
    case eIO_Write:
        if (conn->w_status != eIO_Success)
            return conn->w_status;
        break;
    default:
        assert(0);
        return eIO_NotSupported;
    }
    return conn->meta.status
        ? conn->meta.status(conn->meta.c_status, dir)
        : eIO_Success;
}


extern EIO_Status CONN_Close(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL(27, Close);

    status = x_ReInit(conn, 0, 1/*close*/);
    BUF_Destroy(conn->buf);
    conn->magic = 0;
    conn->data = 0;
    conn->buf = 0;
    free(conn);
    return status == eIO_Closed ? eIO_Success : status;
}


extern EIO_Status CONN_SetCallback
(CONN                  conn,
 ECONN_Callback        type,
 const SCONN_Callback* newcb,
 SCONN_Callback*       oldcb)
{
    size_t idx;

    CONN_NOT_NULL(28, SetCallback);
    
    if ((idx = x_CB2IDX(type)) >= CONN_N_CALLBACKS) {
        static const STimeout* timeout = 0/*dummy*/;
        char errbuf[80];
        sprintf(errbuf, "Unknown callback type #%u", (unsigned int) type);
        CONN_LOG_EX(29, SetCallback, eLOG_Critical, errbuf, eIO_InvalidArg);
        assert(0);
        return eIO_InvalidArg;
    }

    /* NB: oldcb and newcb may point to the same address */
    if (newcb  ||  oldcb) {
        SCONN_Callback cb = conn->cb[idx];
        if (newcb)
            conn->cb[idx] = *newcb;
        if (oldcb)
            *oldcb = cb;
    }
    return eIO_Success;
}


extern EIO_Status CONN_GetSOCK(CONN conn, SOCK* sock)
{
    EIO_Status status;
    CONNECTOR  x_conn;

    if (!sock)
        return eIO_InvalidArg;
    *sock = 0;

    CONN_NOT_NULL(36, GetSOCK);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list);

    x_conn = conn->meta.list;
    if (x_conn->meta  &&  x_conn->meta->get_type) {
        /* check to see if it's SOCK-based at the lowest level */
        const char* type = x_conn->meta->get_type(x_conn->meta->c_get_type);
        if (type == g_kNcbiSockNameAbbr
            ||  ((type = strrchr(type, '/')) != 0
                 &&  strcmp(++type, g_kNcbiSockNameAbbr) == 0)) {
            /* HACK * HACK * HACK */
            SOCK* x_sock = (SOCK*) x_conn->handle;
            if (x_sock) {
                *sock = *x_sock;
                return eIO_Success;
            }
        }
    }
    return eIO_Closed;
}


extern EIO_Status CONN_SetFlags(CONN conn, TCONN_Flags flags)
{
    CONN_CALLTRACE(SetFlags);

    if (!conn)
        return eIO_InvalidArg;

    flags &= (TCONN_Flags)(~fCONN_Flush);
    flags |=  conn->flags & fCONN_Flush;
    conn->flags = flags;
    return eIO_Success;
}


extern TCONN_Flags CONN_GetFlags(CONN conn)
{
    CONN_CALLTRACE(GetFlags);

    return conn ? conn->flags & (TCONN_Flags)(~fCONN_Flush) : 0;
}


extern EIO_Status CONN_SetUserData(CONN conn, void* data)
{
    CONN_CALLTRACE(SetUserData);

    if (!conn)
        return eIO_InvalidArg;

    conn->data = data;
    return eIO_Success;
}


extern void* CONN_GetUserData(CONN conn)
{
    CONN_CALLTRACE(GetUserData);

    return conn ? conn->data : 0;
}
