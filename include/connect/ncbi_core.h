#ifndef NCBI_CORE__H
#define NCBI_CORE__H

/*  $Id$
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
 *   Types and code shared by all "ncbi_*.[ch]" modules.
 *
 *********************************
 * Timeout:
 *    struct STimeout
 *
 * I/O status and direction:
 *    enum:       EIO_ReadMethod
 *    enum:       EIO_Status,  verbal: IO_StatusStr()
 *    enum:       EIO_Event
 *
 * Critical section (basic multi-thread synchronization):
 *    handle:     MT_LOCK
 *    enum:       EMT_Lock
 *    callbacks:  (*FMT_LOCK_Handler)(),  (*FMT_LOCK_Cleanup)()
 *    methods:    MT_LOCK_Create(),  MT_LOCK_AddRef(),  MT_LOCK_Delete(),
 *                MT_LOCK_Do()
 *
 * Tracing and logging:
 *    handle:     LOG
 *    enum:       ELOG_Level,  verbal: LOG_LevelStr()
 *    flags:      TLOG_FormatFlags, ELOG_FormatFlags
 *    callbacks:  (*FLOG_Handler)(),  (*FLOG_Cleanup)()
 *    methods:    LOG_Greate(),  LOG_Reset(),  LOG_AddRef(),  LOG_Delete(),
 *                LOG_Write(), LOG_Data(),
 *    macro:      LOG_WRITE(), LOG_DATA(),  THIS_FILE, THIS_MODULE
 *
 * Registry:
 *    handle:     REG
 *    enum:       EREG_Storage
 *    callbacks:  (*FREG_Get)(),  (*FREG_Set)(),  (*FREG_Cleanup)()
 *    methods:    REG_Greate(),  REG_Reset(),  REG_AddRef(),  REG_Delete(),
 *                REG_Get(),  REG_Set()
 *
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  2000/06/23 19:34:41  vakatov
 * Added means to log binary data
 *
 * Revision 6.3  2000/04/07 19:55:14  vakatov
 * Standard identation
 *
 * Revision 6.2  2000/03/24 23:12:03  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.1  2000/02/23 22:30:40  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <stddef.h>

#include <assert.h>
#if defined(NDEBUG)
#  define verify(expr)  (void)(expr)
#else
#  define verify(expr)  assert(expr)
#endif


#ifdef __cplusplus
extern "C" {
#endif


/* Timeout structure
 */
typedef struct {
    unsigned int sec;  /* seconds (truncated to the platf.-dep. max. limit) */
    unsigned int usec; /* microseconds (always truncated by mod. 1,000,000) */
} STimeout;


/* Aux. enum to set/unset/default various features
 */
typedef enum {
    eOff = 0,
    eOn,
    eDefault
} ESwitch;



/******************************************************************************
 *  I/O
 */


/* I/O read method
 */
typedef enum {
    eIO_Plain,  /* read the presently available data only */
    eIO_Peek,   /* eREAD_Plain, but dont discard the data from input queue */
    eIO_Persist /* try to read exactly "size" bytes;  wait for enough data */
} EIO_ReadMethod;


/* I/O event (or direction)
 */
typedef enum {
    eIO_Open,
    eIO_Read,
    eIO_Write,
    eIO_ReadWrite,
    eIO_Close
} EIO_Event;


/* I/O status
 */
typedef enum {
    eIO_Success = 0,  /* everything is fine, no errors occured          */
    eIO_Timeout,      /* timeout expired before the data could be i/o'd */
    eIO_Closed,       /* peer has closed the connection                 */
    eIO_InvalidArg,   /* bad argument value(s)                          */
    eIO_NotSupported, /* the requested operation is not supported       */

    eIO_Unknown       /* unknown (most probably -- fatal) error         */
} EIO_Status;


/* Return verbal description of the I/O status
 */
extern const char* IO_StatusStr(EIO_Status status);



/******************************************************************************
 *  MT locking
 */


/* Lock handle -- keeps all data needed for the locking and for the cleanup
 */
struct MT_LOCK_tag;
typedef struct MT_LOCK_tag* MT_LOCK;


/* Set the lock/unlock callback function and its data for MT critical section.
 * TIP: If the RW-lock functionality is not provided by the callback, then:
 *   eMT_LockRead <==> eMT_Lock
 */
typedef enum {
    eMT_Lock,      /* lock    critical section             */
    eMT_LockRead,  /* lock    critical section for reading */
    eMT_Unlock     /* unlock  critical section             */
} EMT_Lock;


/* MT locking function (operates like Mutex or RW-lock)
 * Return non-zero value if the requested operation was successful.
 * NOTE:  the "-1" value is reserved for unset handler;  you also
 *   may want to return "-1" if your locking function does no locking, and
 *   you dont consider it as an error, but still want the caller to be
 *   aware of this "rightful non-doing" as opposed to the "rightful doing".
 */
typedef int/*bool*/ (*FMT_LOCK_Handler)
(void*    user_data,  /* see "user_data" in MT_LOCK_Create() */
 EMT_Lock how         /* as passed to MT_LOCK_Do() */
 );

/* MT lock cleanup function;  see "MT_LOCK_Delete()" for more details
 */
typedef void (*FMT_LOCK_Cleanup)
(void* user_data  /* see "user_data" in MT_LOCK_Create() */
 );


/* Create new MT locking object (with reference counter := 1)
 */
extern MT_LOCK MT_LOCK_Create
(void*            user_data, /* to call "handler" and "cleanup" with */
 FMT_LOCK_Handler handler,   /* locking function */
 FMT_LOCK_Cleanup cleanup    /* cleanup function */
 );


/* Increment ref.counter by 1,  then return "lk"
 */
extern MT_LOCK MT_LOCK_AddRef(MT_LOCK lk);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * destroy the handle, call "lk->cleanup(lk->user_data)", and return NULL.
 * Otherwise (if ref.counter is still > 0), return "lk".
 */
extern MT_LOCK MT_LOCK_Delete(MT_LOCK lk);


/* Call "lk->handler(lk->user_data, how)".
 * Return value returned by the lock handler ("handler" in MT_LOCK_Create()).
 * If lock handler is not specified then always return "-1".
 * NOTE:  use MT_LOCK_Do() to avoid overhead!
 */
#define MT_LOCK_Do(lk,how)  (lk ? MT_LOCK_DoInternal(lk, how) : -1)
extern int/*bool*/ MT_LOCK_DoInternal
(MT_LOCK  lk,
 EMT_Lock how
 );


/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


/* Log handle -- keeps all data needed for the logging and for the cleanup
 */
struct LOG_tag;
typedef struct LOG_tag* LOG;


/* Log severity level
 */
typedef enum {
    eLOG_Trace = 0,
    eLOG_Note,
    eLOG_Warning,
    eLOG_Error,
    eLOG_Critical,

    eLOG_Fatal
} ELOG_Level;


/* Return verbal description of the log level
 */
extern const char* LOG_LevelStr(ELOG_Level level);


/* Message and miscellaneous data to pass to the log post callback FLOG_Handler
 * For more details, see LOG_Write().
 */
typedef struct {
    const char* message;   /* can be NULL */
    ELOG_Level  level;
    const char* module;    /* can be NULL */
    const char* file;      /* can be NULL */
    int         line;
    const void* raw_data;  /* raw data to log (usually NULL)*/
    size_t      raw_size;  /* size of the raw data (usually zero)*/
} SLOG_Handler;


/* Log post callback.
 */
typedef void (*FLOG_Handler)
(void*         user_data,  /* see "user_data" in LOG_Create() or LOG_Reset() */
 SLOG_Handler* call_data   /* composed from LOG_Write() args */
 );


/* Log cleanup function;  see "LOG_Reset()" for more details
 */
typedef void (*FLOG_Cleanup)
(void* user_data  /* see "user_data" in LOG_Create() or LOG_Reset() */
 );


/* Create new LOG (with reference counter := 1).
 * ATTENTION:  if non-NULL "lk" is specified then MT_LOCK_Delete() will be
 *             called when this LOG is destroyed -- be aware of it!
 */
extern LOG LOG_Create
(void*        user_data, /* the data to call "handler" and "cleanup" with */
 FLOG_Handler handler,   /* handler */
 FLOG_Cleanup cleanup,   /* cleanup */
 MT_LOCK      mt_lock    /* protective MT lock (can be NULL) */
 );


/* Reset the "lg" to use the new "user_data", "handler" and "cleanup".
 * NOTE:  it does not change ref.counter.
 */
extern void LOG_Reset
(LOG          lg,         /* created by LOG_Create() */
 void*        user_data,  /* new user data */
 FLOG_Handler handler,    /* new handler */
 FLOG_Cleanup cleanup,    /* new cleanup */
 int/*bool*/  do_cleanup  /* call cleanup(if any specified) for the old data */
 );


/* Increment ref.counter by 1,  then return "lg"
 */
extern LOG LOG_AddRef(LOG lg);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * call "lg->cleanup(lg->user_data)", destroy the handle, and return NULL.
 * Otherwise (if ref.counter is still > 0), return "lg".
 */
extern LOG LOG_Delete(LOG lg);


/* Write message (maybe, with raw data attached) to the log -- e.g. call:
 *   "lg->handler(lg->user_data, SLOG_Handler*)"
 * NOTE:  use LOG_WRITE() and LOG_DATA() macros if possible!
 */
extern void LOG_WriteInternal
(LOG         lg,        /* created by LOG_Create() */
 ELOG_Level  level,     /* severity */
 const char* module,    /* module name */
 const char* file,      /* source file */
 int         line,      /* source line */
 const char* message,   /* message content */
 const void* raw_data,  /* raw data to log (can be NULL)*/
 size_t      raw_size   /* size of the raw data (can be zero)*/
 );

#define LOG_Write(lg,level,module,file,line,message) (void) (lg ? \
  (LOG_WriteInternal(lg,level,module,file,line,message,0,0), 0) : 1)
#define LOG_Data(lg,level,module,file,line,data,size,message) (void) (lg ? \
  (LOG_WriteInternal(lg,level,module,file,line,message,data,size), 0) : 1)


/* Auxiliary plain macro to write message (maybe, with raw data) to the log
 */
#define LOG_WRITE(lg, level, message) \
  LOG_Write(lg, level, THIS_MODULE, THIS_FILE, __LINE__, message)

#define LOG_DATA(lg, data, size, message) \
  LOG_Data(lg, eLOG_Trace, 0, 0, 0, data, size, message)


/* Defaults for the THIS_FILE and THIS_MODULE macros (used by LOG_WRITE)
 */
#if !defined(THIS_FILE)
#  define THIS_FILE __FILE__
#endif

#if !defined(THIS_MODULE)
#  define THIS_MODULE 0
#endif


/******************************************************************************
 *  REGISTRY
 */


/* Registry handle (keeps all data needed for the registry get/set/cleanup)
 */
struct REG_tag;
typedef struct REG_tag* REG;


/* Transient/Persistent storage
 */
typedef enum {
    eREG_Transient = 0,
    eREG_Persistent
} EREG_Storage;


/* Copy the registry value stored in "section" under name "name"
 * to buffer "value". Look for the matching entry first in the transient
 * storage, and then in the persistent storage.
 * If the specified entry is not found in the registry then just copy '\0'.
 * Note:  always terminate value by '\0'.
 * Note:  dont put more than "value_size" bytes to "value".
 */
typedef void (*FREG_Get)
(const char* section,
 const char* name,
 char*       value,      /* always passed in as non-NULL, empty string */
 size_t      value_size  /* always > 0 */
 );


/* Store the "value" to  the registry section "section" under name "name",
 * in storage "storage".
 */
typedef void (*FREG_Set)
(const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage
 );


/* Registry cleanup function;  see "LOG_Reset()" for more details
 */
typedef void (*FREG_Cleanup)
(void* user_data  /* see "user_data" in REG_Create() or REG_Reset() */
 );


/* Create new REG (with reference counter := 1).
 * ATTENTION:  if non-NULL "lk" is specified then MT_LOCK_Delete() will be
 *             called when this REG is destroyed -- be aware of it!
 */
extern REG REG_Create
(void*        user_data, /* the data to call "handler" and "cleanup" with */
 FREG_Get     get,       /* the get method */
 FREG_Set     set,       /* the set method */
 FREG_Cleanup cleanup,   /* cleanup */
 MT_LOCK      mt_lock    /* protective MT lock (can be NULL) */
 );


/* Reset the "lg" to use the new "user_data", "handler" and "cleanup".
 * NOTE:  it does not change ref.counter.
 */
extern void REG_Reset
(REG          rg,         /* created by REG_Create() */
 void*        user_data,  /* new user data */
 FREG_Get     get,        /* the get method */
 FREG_Set     set,        /* the set method */
 FREG_Cleanup cleanup,    /* cleanup */
 int/*bool*/  do_cleanup  /* call cleanup(if any specified) for the old data */
 );


/* Increment ref.counter by 1,  then return "reg"
 */
extern REG REG_AddRef(REG rg);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * call "reg->cleanup(reg->user_data)", destroy the handle, and return NULL.
 * Otherwise (if ref.counter is still > 0), return "reg".
 */
extern REG REG_Delete(REG rg);


/* Copy the registry value stored in "section" under name "name"
 * to buffer "value";  if the entry is found in both transient and persistent
 * storages, then copy the one from the transient storage.
 * If the specified entry is not found in the registry, and "def_value"
 * is not NULL, then copy "def_value" to "value".
 * Return "value" (however, if "value_size" is zero, then return NULL).
 * Note:  always terminate "value" by '\0'.
 * Note:  dont put more than "value_size" bytes to "value".
 */
extern char* REG_Get
(REG         rg,         /* created by REG_Create() */
 const char* section,    /* registry section name */
 const char* name,       /* registry entry name  */
 char*       value,      /* buffer to put the value of the requsted entry to */
 size_t      value_size, /* max. size of buffer "value" */
 const char* def_value   /* default value (none if passed NULL) */
 );


/* Store the "value" to  the registry section "section" under name "name",
 * in storage "storage".
 */
extern void REG_Set
(REG          rg,        /* created by REG_Create() */
 const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_CORE__H */
