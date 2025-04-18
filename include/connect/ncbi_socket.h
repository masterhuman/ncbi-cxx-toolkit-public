#ifndef CONNECT___NCBI_SOCKET__H
#define CONNECT___NCBI_SOCKET__H

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
 * @file
 * File Description:
 *   Plain portable TCP/IP socket API for:  UNIX, MS-Win, MacOS
 *   Platform-specific library requirements:
 *     [UNIX ]   -DNCBI_OS_UNIX     -lresolv -lsocket -lnsl
 *     [MSWIN]   -DNCBI_OS_MSWIN    ws2_32.lib
 *
 *********************************
 * Generic:
 *
 *  SOCK_InitializeAPI
 *  SOCK_ShutdownAPI
 *  SOCK_OSHandleSize
 *  SOCK_AllowSigPipeAPI
 *  SOCK_SetIPv6API
 *  SOCK_SetApproveHookAPI
 *
 * Event trigger (handle TRIGGER):
 *
 *  TRIGGER_Create
 *  TRIGGER_Close
 *  TRIGGER_Set
 *  TRIGGER_IsSet
 *  TRIGGER_Reset
 *
 * Listening socket (handle LSOCK):
 *
 *  LSOCK_Create[Ex][6]
 *  LSOCK_Accept[Ex]
 *  LSOCK_Close
 *  LSOCK_GetOSHandle[Ex]
 *  LSOCK_GetPort
 *  LSOCK_GetListeningAddress[6]
 *  LSOCK_GetListeningAddressString[Ex]
 *
 * I/O Socket (handle SOCK):
 *
 *  SOCK_Create[Ex] (see also LSOCK_Accept)
 *  SOCK_CreateOnTop[Ex]
 *  SOCK_Reconnect
 *  SOCK_Shutdown
 *  SOCK_Close[Ex]
 *  SOCK_Destroy
 *  SOCK_GetOSHandle[Ex]
 *  SOCK_CloseOSHandle
 *  SOCK_Wait
 *  SOCK_Poll
 *  SOCK_SetTimeout
 *  SOCK_GetTimeout
 *  SOCK_Read (including "peek" and "persistent read")
 *  SOCK_ReadLine
 *  SOCK_Pushback
 *  SOCK_Status
 *  SOCK_Write
 *  SOCK_Abort
 *  SOCK_GetLocalPort[Ex]
 *  SOCK_GetRemotePort
 *  SOCK_GetPeerAddress[6]
 *  SOCK_GetPeerAddressString[Ex]
 *  SOCK_SetReadOnWriteAPI
 *  SOCK_SetReadOnWrite
 *  SOCK_DisableOSSendDelay
 *  SOCK_SetCork
 *
 * Datagram Socket (handle SOCK):
 *
 *  DSOCK_Create[Ex]
 *  DSOCK_Bind[6]
 *  DSOCK_Connect
 *  DSOCK_WaitMsg
 *  DSOCK_RecvMsg[6]
 *  DSOCK_SendMsg
 *  DSOCK_WipeMsg
 *  DSOCK_SetBroadcast
 *  DSOCK_GetMessageCount
 *
 * Settings:
 *
 *  SOCK_SetInterruptOnSignalAPI
 *  SOCK_SetInterruptOnSignal
 *  SOCK_SetReuseAddressAPI
 *  SOCK_SetReuseAddress
 *
 * Data logging / error reporting:
 *
 *  SOCK_SetDataLoggingAPI
 *  SOCK_SetDataLogging
 *  SOCK_SetErrHookAPI
 *
 * Socket classification & statistics:
 *
 *  SOCK_IsDatagram
 *  SOCK_IsClientSide
 *  SOCK_IsServerSide
 *  SOCK_IsUNIX
 *  SOCK_IsSecure
 *  SOCK_GetPosition
 *  SOCK_GetCount
 *  SOCK_GetTotalCount
 *
 * Generic POLLABLE API:
 *
 *  POLLABLE_Poll
 *  POLLABLE_What
 *  POLLABLE_FromTrigger
 *  POLLABLE_FromLSOCK
 *  POLLABLE_FromSOCK
 *  POLLABLE_ToTrigger
 *  POLLABLE_ToLSOCK
 *  POLLABLE_ToSOCK
 *
 * Auxiliary:
 *
 *  SOCK_ntoa
 *  SOCK_isip[Ex][6]
 *  SOCK_HostToNetShort
 *  SOCK_HostToNetLong
 *  SOCK_NetToHostShort
 *  SOCK_NetToHostLong
 *  SOCK_gethostname[Ex]
 *  SOCK_gethostbyname[Ex][6]
 *  SOCK_gethostbyaddr[Ex][6]
 *  SOCK_GetLocalHostAddress[6]
 *  SOCK_GetLoopbackAddress[6]
 *  SOCK_IsLoopbackAddress[6]
 *
 *  SOCK_StringToHostPort[6]
 *  SOCK_HostPortToString[6]
 *
 * Utility:
 *
 *  SOCK_SetSelectInternalRestartTimeout
 *  SOCK_SetIOWaitSysAPI
 *
 * Secure Socket Layer (SSL):
 *
 *  SOCK_SetupSSL[Ex]
 *  SOCK_SSLName
 *
 */

#include <connect/ncbi_core.h>
#include <connect/ncbi_ipv6.h>

#ifdef NCBI_DEPRECATED
#  define NCBI_SOCK_DEPRECATED NCBI_DEPRECATED
#else
#  define NCBI_SOCK_DEPRECATED
#endif /*NCBI_DEPRECATED*/


/** @addtogroup Sockets
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  TYPEDEFS & MACROS
 */

typedef unsigned EBIO_Event;
typedef unsigned EBIO_Status;


/** Network and host byte order enumeration type
 */
typedef enum {
    eNH_HostByteOrder,
    eNH_NetworkByteOrder
} ENH_ByteOrder;


/** Socket type (internal)
 */
typedef enum {
    eSOCK_Listening = 0,
    eSOCK_Trigger   = 1,
    eSOCK_Socket    = 2,
    eSOCK_Datagram  = 3/*2|1*/
} ESOCK_Type;


/** Sides of socket
 */
typedef enum {
    eSOCK_Server = 0,
    eSOCK_Client = 1
} ESOCK_Side;


/** Forward declarations of the hidden socket internal structures, and
 *  their upper-level handles to use by the SOCK API.
 */
struct LSOCK_tag;                     /* listening socket:  internal storage */
typedef struct LSOCK_tag*   LSOCK;    /* listening socket:  handle, opaque   */

struct SOCK_tag;                      /* socket:  internal storage           */
typedef struct SOCK_tag*    SOCK;     /* socket:  handle, opaque             */

struct TRIGGER_tag;                   /* trigger: internal storage           */
typedef struct TRIGGER_tag* TRIGGER;  /* trigger: handle, opaque             */



/******************************************************************************
 *                       Multi-Thread safety NOTICE
 *
 * If you are using this API in a multi-threaded application, and there is
 * more than one thread using this API, it is safe to call SOCK_InitializeAPI()
 * explicitly at the beginning of your main thread, before you run any other
 * threads, and to call SOCK_ShutdownAPI() after all threads have exited.
 *
 * As soon as the API is initialized it becomes relatively MT-safe, however
 * you still must not operate on same LSOCK or SOCK objects from different
 * threads simultaneously.  Any entry point of this API will attempt to
 * initialize the API implicitly if that has not yet been previously done.
 * However, the implicit initialization gets disabled by SOCK_ShutdownAPI()
 * (explicit re-init with SOCK_InitializeAPI() is always allowed).
 *
 * A MUCH BETTER WAY of dealing with this issue is to provide your own MT
 * locking callback (see CORE_SetLOCK() in "ncbi_core.h").  This will also
 * ensure proper MT protection should some SOCK functions start accessing
 * any intrinsic static data (such as in case of SSL).
 *
 * The MT lock as well as other library-wide settings are also provided
 * (in most cases automatically) by CONNECT_Init() API:  for C Toolkit it gets
 * always called before [Nlm_]Main();  in C++ Toolkit it gets called by
 * most of C++ classes' ctors, except for sockets;  so if your application
 * does not use any C++ classes besides sockets, it has to set CORE_SetLOCK
 * explicitly, as described above.
 *
 * @sa
 *  CORE_SetLOCK, CONNECT_Init
 */



/******************************************************************************
 *  API INITIALIZATION, SHUTDOWN/CLEANUP, AND UTILITY
 */

/** Initialize all internal/system data & resources to be used by the SOCK API.
 * @note
 *  You can safely call it more than once; just, all calls after the first
 *  one will have no result.
 * @note
 *  Usually, SOCK API does not require an explicit initialization -- as it is
 *  guaranteed to initialize itself automagically, in one of API functions,
 *  when necessary.  Yet, see the "Multi Thread safety" remark above.
 * @note
 *  This call, when used for the very first time in the application, enqueues
 *  SOCK_ShutdownAPI() to be called upon application exit on plaftorms that
 *  provide this functionality.  In any case, the application can opt for
 *  explicit SOCK_ShutdownAPI() call when it is done with all sockets.
 * @sa
 *  SOCK_ShutdownAPI
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_InitializeAPI(void);


/** Cleanup; destroy all internal/system data & resources used by the SOCK API.
 * @attention  No function from the SOCK API should be called after this call!
 * @note
 *  You can safely call it more than once;  just, all calls after the first
 *  one will have no effect.
 * @warning
 *  Once the API has been shut down with this call, it cannot be reactivated
 *  with SOCK_InitializeAPI() -- it will always return eIO_NotSupported.
 * @sa
 *  SOCK_InitializeAPI
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_ShutdownAPI(void);


/** Get size of OS-dependent native socket handle.
 * @return
 *  OS-dependent handle size or 0 in case of an error
 * @sa
 *  SOCK_GetOSHandle
 */
extern NCBI_XCONNECT_EXPORT size_t SOCK_OSHandleSize(void);


/** By default (on UNIX platforms) the SOCK API functions automagically call
 * "signal(SIGPIPE, SIG_IGN)" on initialization.  To prohibit this feature you
 * must call SOCK_AllowSigPipeAPI() before you call any other function from the
 * SOCK API.
 */
extern NCBI_XCONNECT_EXPORT void SOCK_AllowSigPipeAPI(void);


/** Select IPv6 as the underlying IP protocol.
* @param ipv6
*  eOn sets IPv6 throughout; eOff sets IPv4 throughout; eDefault allows both
* @return
*  Previous value of the IP selection
* @note
*  IPv4-specific functions (such as name resolution to IPv4 addresses) fail
*  when IPv6 is selected;  and conversely, IPv6-specific functions fail when
*  IPv4 is selected.  Default is to allow both (depending on the addresses),
*  but both IP versions are only fully functional on a dual-stack OS (which
*  has both IPv4 and IPv6 interfaces configured and running).
*/
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetIPv6API(ESwitch ipv6);


/** User-level connection approval hook.
 * The hook (when installed) gets called for any attempt to establish an
 * outgoing(eSOCK_Client) or incoming(eSOCK_Server) connection(eSOCK_Socket),
 * or to send(eSOCK_Client) or receive(eSOCK_Server) a message packet in a
 * datagram socket(eSOCK_Datagram).  The peer end is always identified by the
 * "addr:port" pair (which is non-0 in both parts).  Optionally for outgoing
 * transaction, a textual hostname("host") can be provided (to which "addr"
 * corresponds).  If the action was requested by a bare IP address, the "host"
 * field is set to NULL.
 *
 * The hook is expected to return eIO_Success to approve the action, or any
 * other status code to deny.  Note that either eIO_Timeout or eIO_Closed
 * returned from the hook, will get converted to eIO_Unknown;  and other codes
 * will get passed through to the caller "as is".
 * @warning
 *   Although returning eIO_Interrupt may be helpful to prevent the upper-lever
 *   connection (CONN) layer from processing further I/O on this communication
 *   channel, it may also be just an undesireable side-effect -- so it should
 *   be used with caution.
 * @note
 *   When the hook is not installed, all the actions described above are
 *   treated as allowed (as if eIO_Success was returned).
 * @sa
 *   SOCK_Create, SOCK_Reconnect, LSOCK_Accept, DSOCK_SendMsg, DSOCK_RecvMsg
 */

typedef struct {
    const char*    host;  /**< Textual hostname if provided for outgoing     */
    TNCBI_IPv6Addr addr;  /**< IP (or -1 if unknown / broadcast), never 0    */
    unsigned short port;  /**< Port number, host byte order, never 0         */
    ESOCK_Side     side;  /**< eSOCK_Client (out)  / eSOCK_Server (in)       */
    ESOCK_Type     type;  /**< eSOCK_Socket (conn) / eSOCK_Datagram (packet) */
} SSOCK_ApproveInfo;


/** The approve hook is global per API and gets called with an arbitrary
 * user "data", if so desired as a second argument, which was specified when
 * the hook was installed with the SOCK_SetApproveHookAPI() call.
 */
typedef EIO_Status (*FSOCK_ApproveHook)(const SSOCK_ApproveInfo* info,
                                        void*                    data);


/** The hook is installed when non-NULL, and de-installed otherwise. */
extern NCBI_XCONNECT_EXPORT
void SOCK_SetApproveHookAPI(FSOCK_ApproveHook hook,  /**< the hook function  */
                            void*             data); /**< optional user data */


/** This is a helper call that can improve I/O behavior.
 * @param timeslice
 *  [in]  Break down long waits for I/O into smaller intervals of at most
 *  "timeslice" duration each.  This can help recover "hanging" sockets from
 *  indefinite wait and allow them to report an exceptional I/O condition.
 * @return
 *  Previous value of the timeslice
 * @sa
 *  SOCK_Wait, SOCK_Poll
 */
extern NCBI_XCONNECT_EXPORT const STimeout*SOCK_SetSelectInternalRestartTimeout
(const STimeout* timeslice
 );


/** Selector of I/O wait system API:  auto, poll(), or select().
 * @sa
 *  SOCK_SetIOWaitSysAPI
 */
typedef enum {
    eSOCK_IOWaitSysAPIAuto,   /**< default; use some heuristics to choose API*/
    eSOCK_IOWaitSysAPIPoll,   /**< always use poll()                         */
    eSOCK_IOWaitSysAPISelect  /**< always use select()                       */
} ESOCK_IOWaitSysAPI;


/** This is a helper call that can improve I/O performance (ignored for MSVC).
 * @param api
 *  [in]  Default behavior is to wait for I/O such a way that accomodates the
 *  requested sockets accordingly.  There is a known limitation of the select()
 *  API that requires all sockets to have numeric values of their low-level I/O
 *  handles less than (as little as) 1024, but works faster than the poll() API
 *  that does not have limits on the number or numeric values of the handles.
 *  Either API can be enforced here.
 * @return
 *  Previous value of the API selector
 * @sa
 *  SOCK_Wait, SOCK_Poll
 */
extern NCBI_XCONNECT_EXPORT ESOCK_IOWaitSysAPI SOCK_SetIOWaitSysAPI
(ESOCK_IOWaitSysAPI api
 );



/******************************************************************************
 *  EVENT TRIGGER
 */

/** Create an event trigger.
 * @param trigger
 *  [in|out]  a pointer to a location where to store handle of the new trigger
 * @return
 *  eIO_Success on success; other status on error
 * @sa
 *  TRIGGER_Close, TRIGGER_Set
 */
extern NCBI_XCONNECT_EXPORT EIO_Status TRIGGER_Create
(TRIGGER* trigger,
 ESwitch  log
 );


/** Close an event trigger.
 * @param trigger
 *  [in]  a handle returned by TRIGGER_Create()
 * @return
 *   eIO_Success on success; other status on error
 * @sa
 *  TRIGGER_Create
 */
extern NCBI_XCONNECT_EXPORT EIO_Status TRIGGER_Close
(TRIGGER trigger
 );


/** Set an event trigger.  Can be used from many threads concurrently.
 * @param trigger
 *  [in]  a handle returned by TRIGGER_Create()
 * @return
 *   eIO_Success on success; other status on error
 * @sa
 *  TRIGGER_Create, TRIGGER_IsSet
 */
extern NCBI_XCONNECT_EXPORT EIO_Status TRIGGER_Set
(TRIGGER trigger
 );


/** Check whether the trigger has been set.  Should not be used from multiple
 * threads concurrently at a time, including along with TRIGGER_Reset().
 * @param trigger
 *  [in]  a handle returned by TRIGGER_Create()
 * @return
 *  eIO_Success if the trigger has been set;
 *  eIO_Closed  if the trigger has not yet been set;
 *  other status on error
 * @sa
 *  TRIGGER_Create, TRIGGER_Set, TRIGGER_Reset
 */
extern NCBI_XCONNECT_EXPORT EIO_Status TRIGGER_IsSet
(TRIGGER trigger
 );


/** Reset the trigger.  Should not be used from multiple threads concurrently,
 * including along with TRIGGER_IsSet().
 * @param trigger
 *  [in]  a handle returned by TRIGGER_Create()
 * @return
 *  eIO_Success if the trigger has been reset;  other status on error
 * @sa
 *  TRIGGER_Create, TRIGGER_Set, TRIGGER_IsSet
 */
extern NCBI_XCONNECT_EXPORT EIO_Status TRIGGER_Reset
(TRIGGER trigger
 );



/******************************************************************************
 *  SOCKET FLAGS
 */

typedef enum {
    fSOCK_LogOff       = eOff, /**< NB: logging is inherited in accepted SOCK*/
    fSOCK_LogOn        = eOn,
    fSOCK_LogDefault   = eDefault,
    fSOCK_KeepAlive    = 8,    /**< keep socket alive (if supported by OS)   */
    fSOCK_BindAny      = 0,    /**< bind to 0.0.0.0 (i.e. any), default      */
    fSOCK_BindLocal    = 0x10, /**< bind to 127.0.0.1 only                   */
    fSOCK_KeepOnExec   = 0x20, /**< can be applied to all sockets            */
    fSOCK_CloseOnExec  = 0,    /**< can be applied to all sockets, default   */
    fSOCK_Secure       = 0x40, /**< subsumes CloseOnExec regardless of Keep  */
    fSOCK_KeepOnClose  = 0x80, /**< retain OS handle in SOCK_Close[Ex]()     */
    fSOCK_CloseOnClose = 0,    /**< close  OS handle in SOCK_Close[Ex]()     */
    fSOCK_ReadOnWrite       = 0x100,
    fSOCK_InterruptOnSignal = 0x200
} ESOCK_Flags;
typedef unsigned int TSOCK_Flags;  /**< bitwise "OR" of ESOCK_Flags */


/* Address format string flags */
typedef enum {
    eSAF_Full = 0,  /**< address in full, native form                      */
    eSAF_Port,      /**< only numeric port if INET socket, empty otherwise */
    eSAF_IP         /**< only numeric IP if INET socket,   empty otherwise */
} ESOCK_AddressFormat;



/******************************************************************************
 *  LISTENING SOCKET [SERVER-side]
 */

/** [SERVER-side]  Create and initialize the server-side(listening) socket
 * (socket() + bind() + listen())
 * @param port
 *  [in]  the port to listen at (0 to choose first available)
 * @param backlog
 *  [in]  maximal # of pending connections
 *  @note  On some systems, "backlog" may be silently limited down to 128
           (or even 5), or completely ignored whatsoever.
 * @param lsock
 *  [out] handle of the created listening socket
 * @param flags
 *  [in]  special modifiers
 * @param ipv6
 *  [in]  eOff to use IPv4, eOn to use IPv6, eDefault to allow system default
 * @sa
 *  LSOCK_Create6, LSOCK_Close, LSOCK_GetPort
 */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_CreateEx6
(unsigned short port,
 unsigned short backlog,
 LSOCK*         lsock,
 TSOCK_Flags    flags,
 ESwitch        ipv6
 );


/** [SERVER-side]  Create and initialize the server-side(listening) socket
 * Same as LSOCK_CreateEx6() called with its "flags" argument provided as
 * fSOCK_LogDefault.
 * @param port
 *  [in]  the port to listen at
 * @param backlog
 *  [in]  maximal # of pending connections
 *  @note  On some systems, "backlog" can be silently limited down to 128
 *         (or even 5), or completely ignored whatsoever.
 * @param lsock
 *  [out] handle of the created listening socket
 * @param ipv6
 *  [in]  eOff to use IPv4, eOn to use IPv6, eDefault to allow system default
 * @sa
 *  LSOCK_CreateEx6, LSOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_Create6
(unsigned short port,
 unsigned short backlog,
 LSOCK*         lsock,
 ESwitch        ipv6
 );


/** Same as LSOCK_CreateEx6(,,,, eOff) */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_CreateEx
(unsigned short port,
 unsigned short backlog,
 LSOCK*         lsock,
 TSOCK_Flags    flags
 );


/** Same as LSOCK_Create6(,,, eOff) or LSOCK_CreateEx(,,, eDefault) */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_Create
(unsigned short port,
 unsigned short backlog,
 LSOCK*         lsock
 );


/** [SERVER-side]  Accept connection from a client.
 * @param lsock
 *  [in]  handle of a listening socket
 * @param timeout
 *  [in]  timeout (infinite if NULL)
 * @param sock
 *  [out] handle of the accepted socket
 * @param flags
 *  [in]  properties for the accepted socket to have
 * @note
 *  The provided "timeout" is for this Accept() only.  To set I/O timeout on
 *  the resulted socket use SOCK_SetTimeout();
 *  all I/O timeouts are infinite by default.
 * @sa
 *  SOCK_Create, SOCK_Close, TSOCK_Flags
 */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_AcceptEx
(LSOCK           lsock,
 const STimeout* timeout,
 SOCK*           sock,
 TSOCK_Flags     flags
 );


/** [SERVER-side]  Accept connection from a client.
 * Same as LSOCK_AcceptEx(.,.,.,fSOCK_LogDefault)
 * @sa
 *  LSOCK_AcceptEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_Accept
(LSOCK           lsock,
 const STimeout* timeout,
 SOCK*           sock
 );


/** [SERVER-side]  Close the listening socket, destroy relevant internal data.
 * @param lsock
 *  [in]  listening socket handle to close
 * The call invalidates the handle, so its further is not allowed.
 * @sa
 *  LSOCK_Create
 */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_Close(LSOCK lsock);


/** Get an OS-dependent native socket handle to use by platform-specific API.
 * FYI: on MS-Windows it will be "SOCKET", on other platforms -- "int".
 * @param lsock
 *  [in]  socket handle
 * @param handle_buf
 *  [in]  pointer to a memory location to store the OS-dependent handle at
 * @param handle_size
 *  The exact(!) size of the expected OS handle
 * @param ownership
 *  eTakeOwnership removes the handle from LSOCK;  eNoOwnership retains it
 * @note
 *  If handle ownership is taken, all operations with this LSOCK (except for
 *  LSOCK_Close()) will fail.
 * @sa
 *  SOCK_OSHandleSize, SOCK_GetOSHandle, SOCK_CloseOSHandle, LSOCK_GetOSHandle,
 *  LSOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_GetOSHandleEx
(LSOCK      lsock,
 void*      handle_buf,
 size_t     handle_size,
 EOwnership owndership
 );


/** Same as LSOCK_GetOSHandleEx(lsock, handle_buf, handle_size, eNoOwnership).
 * @sa
 *  LSOCK_GetOSHandleEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status LSOCK_GetOSHandle
(LSOCK  lsock,
 void*  handle_buf,
 size_t handle_size
 );


/** Get socket port number, which it listens on.
 * The returned port is either one specified when the socket was created, or
 * an automatically assigned number if LSOCK_Create() provided the port as 0.
 * @param lsock
 *  [in]  socket handle
 * @param byte_order
 *  [in]  byte order for port on return
 * @return
 *  Listening port number in requested byte order, or 0 in case of an error.
 * @sa
 *  LSOCK_Create
 */
extern NCBI_XCONNECT_EXPORT unsigned short LSOCK_GetPort
(LSOCK         lsock,
 ENH_ByteOrder byte_order
 );


/* @sa SOCK_GetPeerAddress */
extern NCBI_XCONNECT_EXPORT void LSOCK_GetListeningAddress
(LSOCK           lsock,
 unsigned int*   host,
 unsigned short* port,
 ENH_ByteOrder   byte_order
 );


/* @sa SOCK_GetPeerAddress6 */
extern NCBI_XCONNECT_EXPORT void LSOCK_GetListeningAddress6
(LSOCK           lsock,
 TNCBI_IPv6Addr* addr,
 unsigned short* port,
 ENH_ByteOrder   byte_order
 );


/* @sa SOCK_GetPeerAddressStringEx */
extern NCBI_XCONNECT_EXPORT char* LSOCK_GetListeningAddressStringEx
(LSOCK               lsock,
 char*               buf,
 size_t              bufsize,
 ESOCK_AddressFormat format
 );


/** Equivalent to LSOCK_GetListeningAddressStringEx(.,.,.,eSAF_Full) */
extern NCBI_XCONNECT_EXPORT char* LSOCK_GetListeningAddressString
(LSOCK  lsock,
 char*  buf,
 size_t bufsize
 );



/******************************************************************************
 *  SOCKET (connection-oriented)
 */

/** [CLIENT-side]  Connect client to another(server-side, listening) socket
 * (socket() + connect() [+ select()])
 * @note
 *  SOCK_Close[Ex]() will not close the underlying OS handle if
 *  fSOCK_KeepOnClose is set in "flags".
 * @param host
 *  [in]  host to connect to
 * @param port
 *  [in]  port to connect to
 * @param timeout
 *  [in]  the connect timeout (infinite if NULL)
 * @param sock
 *  [out] handle of socket created
 * @param data
 *  [in]  initial output data block (may be NULL)
 * @param size
 *  [in]  size of the initial data block (may be 0)
 * @param flags
 *  [in]  additional socket requirements
 * @sa
 *  SOCK_Create, SOCK_Reconnect, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_CreateEx
(const char*     host,
 unsigned short  port,
 const STimeout* timeout,
 SOCK*           sock,
 const void*     data,
 size_t          size,
 TSOCK_Flags     flags
 );


/** [CLIENT-side]  Connect client to another(server-side, listening) socket
 * (socket() + connect() [+ select()])
 * Equivalent to
 * SOCK_CreateEx(host, port, timeout, sock, 0, 0, fSOCK_LogDefault).
 *
 * @param host
 *  [in]  host to connect to
 * @param port
 *  [in]  port to connect to
 * @param timeout
 *  [in]  the connect timeout (infinite if NULL)
 * @param sock
 *  [out] handle of the created socket
 * @sa
 *  SOCK_CreateEx, SOCK_Reconnect, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Create
(const char*     host,
 unsigned short  port,
 const STimeout* timeout,
 SOCK*           sock
 );


/** [SERVER-side]  Create a SOCKet on top of either an OS-dependent "handle"
 * (file descriptor on Unix, SOCKET on MS-Windows) or an existing SOCK object.
 * In the former case, the returned socket is not reopenable to its default
 * peer (SOCK_Reconnect() may not specify zeros for the connection point).
 * In the latter case, the returned socket is reopenable to its default peer
 * only if the original existing socket is a client-side one (connected to a
 * remote server end), not a server-side socket (accepted at the server end).
 * All timeouts are set to default [infinite] values.
 * The call does *not* destroy either the OS handle or the SOCK passed in the
 * arguments, regardless of the return status code.
 * When a socket is being created on top of a SOCK handle, the original SOCK
 * gets emptied (and the underlying OS handle removed from it) upon the call
 * return (whether successfully or not), yet it will still need SOCK_Destroy()
 * in the caller's code to free up the memory it occupies.
 * Any secure session that may have existed in the original SOCK will have
 * been migrated to the new socket iff "flags" indicate fSOCK_Secure (if no
 * session existed, a new session may have been initiated in the new SOCK).
 * Otherwise, the original secure session (if any) will have been closed.
 * Any pending output will have been flushed (if switching secure / insecure
 * contexts;  otherwise, it will have simply migrated to the created SOCK), and
 * any pending input still in the original SOCK will migrate to the new socket
 * object returned.  If this behavior is undesireable, one can use
 * SOCK_GetOSHandleEx() on the original socket, taking the ownership of the
 * underlying OS handle, and then create a SOCK on top of the bare "handle",
 * specifying its (non-zero) "handle_size".
 * @warning
 *  It is not recommended to use this call on not fully connected sockets
 *  (either bare OS handle or SOCK) in native MS-Windows builds (e.g. Cygwin is
 *  okay).
 * @note
 *  SOCK_Close[Ex]() on the resultant socket will not close the underlying OS
 *  handle if fSOCK_KeepOnClose is set in "flags".
 * @note
 *  (SOCK_IsClientSide() | SOCK_IsServerSide()) can be used to determine if the
 *  original SOCK was stripped off the underlying handle:  the expression would
 *  evaluate to 0(false) iff the SOCK object does no longer have the OS handle.
 * @warning
 *  If a new SOCK is being built from an existing SOCK, then it is assumed that
 *  the underlying OS handle's properies have not been modified outside that
 *  old SOCK object (as all system calls to adjust the handle will be skipped).
 * @param handle
 *  [in]  OS-dependent "handle" or SOCK to be converted
 * @param handle_size
 *  [in]  "handle" size (or 0 if a SOCK passed in "handle")
 * @param sock
 *  [out] SOCK built on top of the "handle"
 * @param data
 *  [in]  initial output data block (may be NULL)
 * @param size
 *  [in]  size of the initial data block (may be 0)
 * @param flags
 *  [in]  additional socket requirements
 * @return
 *  Return eIO_Success on success;  otherwise: eIO_InvalidArg if the "handle"
 *  does not refer to an open socket [but e.g. to a normal file or a pipe];
 *  eIO_Closed when the original socket is not connected;  other error codes
 *  in case of other errors.
 * @sa
 *  SOCK_GetOSHandleEx, SOCK_CreateOnTop, SOCK_Reconnect, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_CreateOnTopEx
(const void* handle,
 size_t      handle_size,
 SOCK*       sock,
 const void* data,
 size_t      size,
 TSOCK_Flags flags
 );


/** [SERVER-side]  Create a socket on top of a "handle".
 * Equivalent to SOCK_CreateOnTopEx(handle, handle_size, sock,
 *                                  0, 0, fSOCK_LogDefault|fSOCK_CloseOnClose).
 * @param handle
 *  [in]  OS-dependent "handle" or "SOCK" to be converted
 * @param handle_size
 *  [in]  "handle" size (0 if a "SOCK" passed in "handle")
 * @param sock
 *  [out] SOCK built on top of the OS "handle"
 * @sa
 *  SOCK_GetOSHandleEx, SOCK_CreateOnTopEx, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_CreateOnTop
(const void* handle,
 size_t      handle_size,
 SOCK*       sock
 );


/** [CLIENT-side]  Close the socket referred to by "sock" and then connect
 * it to another "host:port";  fail if it takes more than "timeout"
 * (close() + connect() [+ select()])
 *
 * HINT:  if "host" is NULL then connect to the same host address as before;
 *        if "port" is zero then connect to the same port # as before.
 *
 * @note  The "new" socket inherits old I/O timeouts;
 * @note  The call is only applicable to stream [not datagram] sockets.
 * @note  "timeout"==NULL is infinite (also as kInfiniteTimeout);
 *        "timeout"=={0,0} causes no wait for connection to be established,
 *        and to return immediately.
 * @note  UNIX sockets can only be reconnected to the same file,
 *        thus both host and port have to be passed as 0s.
 * @param sock
 *  [in]  handle of the socket to reconnect
 * @param host
 *  [in]  host to connect to  (can be NULL)
 * @param port
 *  [in]  port to connect to  (can be 0)
 * @param timeout
 *  [in]  the connect timeout (infinite if NULL)
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Reconnect
(SOCK            sock,
 const char*     host,
 unsigned short  port,
 const STimeout* timeout
 );


/** Shutdown the connection in only one direction (specified by "direction").
 * Later attempts to I/O (or to wait) in the shutdown direction will
 * do nothing, and immediately return with "eIO_Closed" status.
 * Pending data output can cause data transfer to the remote end (subject
 * for eIO_Close timeout as previously set by SOCK_SetTimeout()).
 * Cannot be applied to datagram sockets (eIO_InvalidArg results).
 * @param sock
 *  [in]  handle of the socket to shutdown
 * @param how
 *  [in]  one of:  eIO_Read, eIO_Write, eIO_ReadWrite
 * @sa
 *  SOCK_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Shutdown
(SOCK      sock,
 EIO_Event how
 );


/** Close the SOCK handle, and destroy all relevant internal data.
 * The "sock" handle goes invalid after this function call, regardless of
 * whether the call was successful or not.  If eIO_Close timeout was specified
 * (or NULL) then it blocks until either all unsent data are sent, an error
 * flagged, or the timeout expires;  if there was any output pending, that
 * output will be flushed.
 * Connection may remain in the system if SOCK was created with the
 * fSOCK_KeepOnClose flag set;  otherwise, it gets closed as well.
 * @warning
 *  On MS-Windows closing a SOCK whose OS handle has been used elsewhere
 *  (e.g. in SOCK_CreateOnTop()) may render the OS handle unresponsive, so
 *  always make sure to close the current SOCK first (assuming
 *  fSOCK_KeepOnClose), and only then use the extracted handle to build
 *  another socket.
 * @param sock
 *  [in]  socket handle to close(if not yet closed) and destroy(always)
 * @sa
 *  SOCK_Create, SOCK_CreateOnTop, DSOCK_Create, SOCK_SetTimeout, SOCK_CloseEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Close(SOCK sock);
#define SOCK_Destroy(s)  SOCK_Close(s)


/** Close the SOCK handle, and conditionally destroy relevant internal data.
 * If eIO_Close timeout was specified (or NULL) then it blocks until either all
 * unsent data are sent, an error flagged, or the timeout expires;  if there is
 * any output pending, that output will be flushed.
 * Connection may remain in the system if the socket was created with the
 * fSOCK_KeepOnClose flag set;  otherwise, it gets closed as well.
 * @note
 *  On MS-Windows closing a SOCK whose OS handle has been used elsewhere
 *  (e.g. in SOCK_CreateOnTop()) may render the OS handle unresponsive, so
 *  always make sure to close the current SOCK first (assuming
 *  fSOCK_KeepOnClose), and only then use the extracted handle to build
 *  another socket.
 * @param sock
 *  [in]  handle of the socket to close
 * @param destroy
 *  [in]  =1 to destroy the SOCK handle; =0 to keep the handle
 * @note
 *  The kept SOCK handle can be freed/destroyed by SOCK_Destroy().
 * @note
 *  SOCK_CloseEx(sock, 1) is equivalent to SOCK_Destroy(sock).
 * @sa
 *  SOCK_Close, SOCK_Create, SOCK_CreateOnTop, DSOCK_Create, SOCK_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_CloseEx
(SOCK        sock,
 int/*bool*/ destroy
 );


/** Get an OS-dependent native socket handle to use by platform-specific API.
 * FYI:  on MS-Windows it will be "SOCKET", on other platforms -- "int".
 * @param sock
 *  [in]  socket handle
 * @param handle_buf
 *  [out] pointer to a memory area to put the OS handle at
 * @param handle_size
 *  [in]  the exact(!) size of the expected OS handle
 * @param ownership
 *  eTakeOwnership removes the handle from SOCK;  eNoOwnership retains it
 * @note
 *  If handle ownership is taken, all operations with this SOCK (except for
 *  SOCK_Close[Ex]()) will fail.
 * @sa
 *  SOCK_OSHandleSize, SOCK_GetOSHandle, SOCK_CloseOSHandle, SOCK_CloseEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_GetOSHandleEx
(SOCK       sock,
 void*      handle_buf,
 size_t     handle_size,
 EOwnership ownership
 );


/** Same as SOCK_GetOSHandleEx(sock, handle_buf, handle_size, eNoOwnership).
 * @sa
 *  SOCK_GetOSHandleEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_GetOSHandle
(SOCK   sock,
 void*  handle_buf,
 size_t handle_size
 );


/** Close socket OS handle (ungracefully aborting the connection if necessary).
 * The call retries repeatedly if interrupted by a singal (so no eIO_Interrupt
 * should be expected).  Return eIO_Success when the handle has been closed
 * successfully, eIO_Closed if the handle has been passed already closed,
 * eIO_InvalidArg if passed arguments are not valid, eIO_Unknown if the
 * handle cannot be closed (per an error returned by the system, see errno).
 * @warning
 *  Using this call on a handle that belongs to an active [LD]SOCK object is
 *  undefined.
 * @sa
 *  SOCK_GetOSHandleEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_CloseOSHandle
(const void* handle,
 size_t      handle_size
);


/** Block on the socket until either the specified "event" is available or
 * "timeout" expires (if "timeout" is NULL then assume it infinite).
 * eIO_Open (as "event") can be used to check whether the socket has been
 * connected.  When eIO_Read is requested as an "event" for a datagram socket,
 * then eIO_Closed results if the internally latched message has been entirely
 * read out.  Either of eIO_Open, eIO_Write and eIO_ReadWrite always succeeds
 * immediately for a datagram socket.
 * @param sock
 *  [in]  socket handle
 * @param event
 *  [in]  one of:  eIO_Open, eIO_Read, eIO_Write, eIO_ReadWrite
 * @param timeout
 *  [in]  maximal time to wait for the event to occur (not kDefaultTimeout!)
 * @return
 *  eIO_Closed     -- if the socket has been closed (in the specified
 *                    direction when a read/write "event" requested);
 *  eIO_Success    -- if the socket is ready;
 *  eIO_Unknown    -- if partially closed with eIO_Open requested,
 *                    or an I/O error occurred;
 *  eIO_Timeout    -- if socket is not ready for the "event" and the allotted
 *                    timeout has expired (for eIO_Open means the socket is
 *                    still connecting);
 *  eIO_Interrupt  -- if the call had been interrupted by a signal before
 *                    any other verifiable status was available;
 *  eIO_InvalidArg -- if the "event" is not one of those mentioned above.
 * @note  It is allowed to use a non-NULL zeroed STimeout to poll
 *        on the socket for the immediately available event and
 *        return it (or eIO_Timeout, otherwise) without blocking.
 * @sa
 *  SOCK_Poll, POLLABLE_Poll
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Wait
(SOCK            sock,
 EIO_Event       event,
 const STimeout* timeout
 );


/** I/O polling structure.
 * @sa SOCK_Poll()
 */
typedef struct {
    SOCK      sock;   /** [in]          SOCK to poll (NULL if not to poll)  */
    EIO_Event event;  /** [in]  one of: eIO_Open/Read/Write/ReadWrite       */
    EIO_Event revent; /** [out] one of: eIO_Open/Read/Write/ReadWrite/Close */
} SSOCK_Poll;


/** Block until at least one of the sockets enlisted in "polls" array
 * (of size "n") becomes available for requested operation (SSOCK_Poll::event),
 * or until timeout expires (wait indefinitely if timeout is passed as NULL).
 *
 * @note To lower overhead, use SOCK_Wait() to wait for I/O on a single socket.
 *
 * Return eIO_Success if at least one socket was found ready;  eIO_Timeout
 * if timeout expired;  eIO_Unknown if underlying system call(s) failed.
 *
 * @note  NULL sockets (without any verification for the contents of
 *        the "event" field) as well as non-NULL sockets with eIO_Open
 *        requested in their "event" do not get polled (yet the corresponding
 *        "revent" gets updated to indicate eIO_Open, for no I/O event ready);
 * @note  For a socket found not ready for an operation, eIO_Open gets returned
 *        in its "revent";  for a failing / closed socket -- eIO_Close;
 * @note  This call may return eIO_InvalidArg if:
 *        - parameters to the call are incomplete / inconsistent;
 *        - a non-NULL socket polled with a bad "event" (e.g. eIO_Close).
 *        With this return code, the caller cannot rely on "revent" fields in
 *        the "polls" array as they might not have been updated properly.
 * @note  If either both "n" and "polls" are NULL, or all sockets in the
 *        "polls" array are either NULL or without any events requested
 *        (eIO_Open), then the returned status is either:
 *        - eIO_Timeout (after the specified amount of time was spent idle), or
 *        - eIO_Interrupt (if a signal came while the waiting was in progress).
 * @note  For datagram sockets, the readiness for reading is determined by the
 *        message data latched since the last message receive call,
 *        DSOCK_RecvMsg().
 * @note  This call allows the intermixture of stream, datagram, and listening
 *        sockets (cast to SOCK), as well as triggers (also cast to SOCK), but
 *        for the sake of readability, it is recommended to use POLLABLE_Poll()
 *        in such circumstances.
 * @note  This call may cause some socket I/O in those sockets marked for
 *        read-on-write and those with pending connection or output data.
 * @param n
 *  [in]  # of SSOCK_Poll elems in "polls"
 * @param polls[]
 *  [in|out] array of query/result structures
 * @param timeout
 *  [in]  max time to wait (infinite if NULL)
 * @param n_ready
 *  [out] # of ready sockets  (may be NULL)
 * @sa
 *  SOCK_Wait, POLLABLE_Poll
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Poll
(size_t          n,
 SSOCK_Poll      polls[],
 const STimeout* timeout,
 size_t*         n_ready
 );


/** Specify timeout for the connection I/O (see SOCK_[Read|Write|Close]()).
 * If "timeout" is NULL then set the timeout to be infinite;
 * @note  The default timeout is infinite (to wait indefinitely).
 * @param sock
 *  [in]  socket handle
 * @param event
 *  [in]  one of:  eIO_[Read/Write/ReadWrite/Close]
 * @param timeout
 *  [in]  new timeout value to set (kDefaultTimeout is not accepted)
 * @sa
 *  SOCK_Read, SOCK_Write, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_SetTimeout
(SOCK            sock,
 EIO_Event       event,
 const STimeout* timeout
 );


/** Get the connection's i/o timeout (or NULL, if the timeout is infinite).
 * @note  The returned timeout is guaranteed to be pointing to
 *        a valid structure in memory at least until the SOCK is closed
 *        or SOCK_SetTimeout() is called for this "sock".
 * @note  eIO_ReadWrite timeout is the least of eIO_Read and eIO_Write ones.
 * @param sock
 *  [in]  socket handle
 * @param event
 *  [in]  one of:  eIO_[Read/Write/Close]
 * @sa
 *  SOCK_Read, SOCK_Write, SOCK_Close
 */
extern NCBI_XCONNECT_EXPORT const STimeout* SOCK_GetTimeout
(SOCK      sock,
 EIO_Event event
 );


/** Read/peek up to "size" bytes from "sock" to a buffer pointed to by "buf".
 * In "*n_read", return the number of successfully read bytes.
 * Read method "how" can be either of the following:
 * eIO_ReadPlain   -- read as many as "size" bytes and return (eIO_Success);
 *                    if no data are readily available then wait at most
 *                    read timeout and return (eIO_Timeout) if no data still
 *                    could be got; eIO_Success if some data were obtained.
 * eIO_ReadPeek    -- same as "eIO_ReadPlain" but do not extract the data from
 *                    the socket (so that the next read operation will see the
 *                    data again), with one important exception noted below.
 * eIO_ReadPersist -- read exactly "size" bytes and return eIO_Success; if less
 *                    data received then return an error condition (including
 *                    eIO_Timeout).
 *
 * If there is no data available to read (also, if eIO_ReadPersist and cannot
 * read exactly "size" bytes) and the timeout(see SOCK_SetTimeout()) is expired
 * then return eIO_Timeout.
 *
 * Both eIO_ReadPlain and eIO_ReadPeek return eIO_Success iff some data have
 * been actually read (within the time allotted by the eIO_Read timeout).
 * Both methods return any other code when no data at all were available.
 * In that respect, eIO_ReadPersist differs from the other two methods as it
 * can return an error condition even if some (but not all) data were actually
 * obtained from the socket.
 *
 * Hence, as the *rule of thumb*, an application should always check the number
 * of read bytes BEFORE checking the return status, which merely advises as to
 * whether it is okay to read again.
 *
 * As a special case, "buf" may passed as NULL:
 *   eIO_ReadPeek      -- read up to "size" bytes and store them in internal
 *                        buffer;
 *   eIO_Read[Persist] -- discard up to "size" bytes from internal buffer
 *                        and socket (check "*n_read" to know how many).
 *
 * @note  "Read" and "peek" methods differ:  when "read" is performed and not
 *        enough but only some data are available immediately from the internal
 *        buffer, then the call still completes with the eIO_Success status.
 *        For "peek", if not all requested data were available right away, the
 *        real I/O occurs to try to pick up additional data from the system.
 *        Keep this difference in mind when programming loops that heavily use
 *        "peek"s without "read"s.
 * @note  If on input "size" == 0, then "*n_read" is set to 0 on return,
 *        and the return status value can be either of eIO_Success, eIO_Closed,
 *        eIO_Timeout (if still connecting), or eIO_Unknown depending on the
 *        _current_ state of the socket regardless of any pending read data.
 * @param sock
 *  [in]  socket handle
 * @param buf
 *  [out] data buffer to read to
 * @param size
 *  [in]  max # of bytes to read to "buf"
 * @param n_read
 *  [out] # of bytes read  (can be NULL)
 * @param how
 *  [in]  how to read the data
 * @sa
 *  SOCK_SetTimeout, SOCK_PushBack
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Read
(SOCK           sock,
 void*          buf,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how
 );


/** Read a line from SOCK.  A line is terminated by either '\n' (with an
 * optional preceding '\r') or '\0', and is stored in the buffer "line" of
 * "size" characters long.  "*n_read" (if "n_read" passed non-NULL) receives
 * the number of characters written into "line", not counting the terminating
 * '\0'.  Returned result is '\0'-terminated (and has '\r'(if any)'\n'
 * stripped) but only if "size" is big enough to contain the entire line.
 * Otherwise, all "size" bytes are filled, and "*n_read" == "size" upon return,
 * and there is no terminating '\0' in this (and the only!) case, which the
 * caller can easily distinguish.  The remainder of the line can be read with
 * successive call(s) to SOCK_ReadLine() until the '\0' terminator is received
 * in the buffer.
 * @param sock
 *  [in]  socket handle
 * @param line
 *  [out] line buffer to read to, non-NULL
 * @param size
 *  [in]  max # of bytes to read to "line", non-0
 * @param n_read
 *  [out] # of bytes read  (optional, can be NULL)
 * @return
 *  Return eIO_Success upon successful completion:  either a line separator is
 *  encountered or the buffer is filled up completely, and any excess read has
 *  been successfully saved for further I/O.  Other code, otherwise.
 * @note that "*n_read" should be analyzed prior to the return status, because
 *  the buffer could have received some contents before the indicated error
 *  occurred (e.g. when the connection closes before a line separator is seen).
 * @sa
 *  SOCK_SetTimeout, SOCK_PushBack
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_ReadLine
(SOCK    sock,
 char*   line,
 size_t  size,
 size_t* n_read
 );


/** Push the specified data back to the socket's input queue (in the socket's
 * internal read buffer).  These can be any data, not necessarily the data
 * previously read from the socket.  The most recently pushed back data are the
 * data that will be read from the socket first (by either SOCK_ReadLine() or
 * SOCK_Read()).  The presence of pushed back data makes the socket ready
 * for read immediately (SOCK_Wait(sock, eIO_Read) succeeds without waiting).
 * @param sock
 *  [in]  socket handle
 * @param data
 *  [in]  data to push back to the socket's local buffer
 * @param size
 *  [in]  # of bytes (starting at "data") to push back
 * @sa
 *   SOCK_Read, SOCK_ReadLine, SOCK_Wait
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Pushback
(SOCK        sock,
 const void* data,
 size_t      size
 );


/** Return low-level socket I/O status of *last* socket operation.
 * This call does not perform any I/O or socket-related system calls.
 * @param sock
 *  [in]  socket handle
 * @param direction
 *  [in]  one of:  eIO_Open, eIO_Read, eIO_Write
 * @return
 *   - eIO_Closed     - if either the connection has been closed / shut down
 *                      (in corresponding direction for eIO_Read or eIO_Write),
 *                      or the socket does not exist (for eIO_Open);
 *   - eIO_Timeout    - if connection request has been submitted but not
 *                      completed (i.e. it was still pending during last I/O);
 *   - eIO_Interrupt  - if last data I/O was interrupted by a signal
 *                      (applicable only to eIO_Read or eIO_Write);
 *   - eIO_Unknown    - if an error has been detected during last data I/O
 *                      (applicable only to eIO_Read or eIO_Write);
 *   - eIO_InvalidArg - if "direction" is not one of the allowed above;
 *   - eIO_Success    - otherwise.
 *
 * @note  eIO_Open merely checks whether the socket still exists (i.e. open as
 *        a system object), and that SOCK_CloseEx() has not been called on it.
 *
 * @note  SOCK_Read(eIO_ReadPeek) and SOCK_Wait(eIO_Read) will not
 *        return any error as long as there is unread buffered
 *        data left.  Thus, when you are "peeking" data (instead of
 *        reading it out), then the only "non-destructive" way to
 *        check whether an EOF or an error has actually occurred,
 *        it is to use this call.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Status
(SOCK      sock,
 EIO_Event direction
 );


/** Write "size" bytes of "data" to "sock".
 * @param sock
 *  [in]  socket handle
 * @param data
 *  [in]  data to write to the socket
 * @param size
 *  [in]  # of bytes (starting at "data") to write
 * @param n_written
 *  [out] # of written bytes (can be NULL)
 * @param how
 *  [in]  either eIO_WritePlain or eIO_WritePersist
 * @return
 *  In "*n_written", return the number of bytes actually written.
 *  eIO_WritePlain   -- write as many bytes as possible at once and return
 *                      immediately;  if no bytes can be written then wait
 *                      at most WRITE timeout, try again and return.
 *  eIO_WritePersist -- write all data (doing an internal retry loop
 *                      if necessary);  if any single write attempt times out
 *                      or fails then stop writing and return (error code).
 *  Return status: eIO_Success -- some bytes were written successfully  [Plain]
 *                             -- all bytes were written successfully [Persist]
 *                 other code denotes an error, but some bytes might have
 *                 been sent nevertheless (always check "*n_written" to know).
 *
 * @note  With eIO_WritePlain the call returns eIO_Success if and only if
 *        some data were actually written to the socket.  If no data could
 *        be written (and perhaps timeout expired) this call always returns
 *        an error.
 * @note  eIO_WritePlain and eIO_WritePersist differ that
 *        the latter can flag an error condition even if some data were
 *        actually written
 *        (see "the rule of thumb" in the comments for SOCK_Read() above).
 * @note  If "size"==0, return value can be eIO_Success if no
 *        pending data left in the socket, or eIO_Timeout if there are still
 *        data pending.  In either case, "*n_written" is set to 0 on return.
 * @sa
 *  SOCK_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Write
(SOCK            sock,
 const void*     data,
 size_t          size,
 size_t*         n_written,
 EIO_WriteMethod how
 );


/** If there is outstanding connection or output data pending, cancel it.
 * Mark the socket as if it has been shut down for both reading and writing.
 * Break actual connection if any was established.
 * Do not attempt to send anything upon SOCK_Close().
 * This call is available for stream sockets only.
 * @note  Even though the underlying OS socket handle may have been marked for
 *        preservation via fSOCK_KeepOnClose, this call always and
 *        unconditionally closes and destroys the actual OS handle.
 * @param sock
 *  [in] socket handle
 */
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_Abort
(SOCK sock
 );


/** Get local port of the socket (true or cached / stored).
 * For most users, a simpler SOCK_GetLocalPort() call is going to be the
 * most suitable.  This call allows to inquire the network level about
 * a temporary port number assigned when a socket was created as a result
 * of accepting the connection (otherwise, the listening socket port number
 * gets returned as the local port).  For connecting sockets, both "trueport"
 * and no "trueport" results are identical (with the exception that "trueport"
 * causes an additional system call, and the result is not stored).
 * @param sock
 *  [in]  socket handle
 * @param trueport
 *  [in]  non-zero causes to refetch / no-cache port from the network layer
 * @param byte_order
 *  [in]  byte order for port on return
 * @return
 *  The port number in requested byte order, or 0 in case of an error.
 * @sa
 *  SOCK_GetLocalPort
 */
extern NCBI_XCONNECT_EXPORT unsigned short SOCK_GetLocalPortEx
(SOCK          sock,
 int/*bool*/   trueport,
 ENH_ByteOrder byte_order
 );


/** Get local port of the socket.
 * The returned port number is also cached within "sock" so any further
 * inquires for the local port do not cause any system calls to occur.
 * The call is exactly equivalent to SOCK_GetLocalPortEx(sock, 0, byte_order).
 * @param sock
 *  [in]  socket handle
 * @param byte_order
 *  [in]  byte order for port on return
 * @return
 *  Local port number in requested byte order, or 0 in case of an error.
 * @sa
 *  SOCK_GetLocalPortEx
 */
extern NCBI_XCONNECT_EXPORT unsigned short SOCK_GetLocalPort
(SOCK          sock,
 ENH_ByteOrder byte_order
 );


/** Get remote port of the socket (the port it is connected to).
 * The call is provided as a counterpart for SOCK_GetLocalPort(), and is
 * equivalent to calling SOCK_GetPeerAddress(sock, 0, &port, byte_order).
 * @param sock
 *  [in]  socket handle
 * @param byte_order
 *  [in]  byte order for port on return
 * @return
 *  Remote port number in requested byte order, or 0 in case of an error.
 * @sa
 *  SOCK_GetPeerAddress, SOCK_GetLocalPort
 */
extern NCBI_XCONNECT_EXPORT unsigned short SOCK_GetRemotePort
(SOCK          sock,
 ENH_ByteOrder byte_order
 );


/** Get host and port of the socket's peer (remote end).
 * @param sock
 *  [in]  socket handle
 * @param host
 *  [out] the peer's host (can be NULL, then not filled in)
 * @param port
 *  [out] the peer's port (can be NULL, then not filled in)
 * @param byte_order
 *  [in]  byte order for either host or port, or both, on return
 * @return
 *  Host/port addresses in requested byte order, or 0 in case of an error.
 * @sa
 *  SOCK_GetLocalPort
 */
extern NCBI_XCONNECT_EXPORT void SOCK_GetPeerAddress
(SOCK            sock,
 unsigned int*   host,
 unsigned short* port,
 ENH_ByteOrder   byte_order
 );


/** Get host and port of the socket's peer (remote end).
 * @param sock
 *  [in]  socket handle
 * @param addr
 *  [out] the peer's address (can be NULL, then not filled in)
 * @param port
 *  [out] the peer's port (can be NULL, then not filled in)
 * @param byte_order
 *  [in]  byte order for either host or port, or both, on return
 * @return
 *  Host/port addresses in requested byte order, or 0 in case of an error.
 * @sa
 *  SOCK_GetLocalPort
 */
extern NCBI_XCONNECT_EXPORT void SOCK_GetPeerAddress6
(SOCK            sock,
 TNCBI_IPv6Addr* addr,
 unsigned short* port,
 ENH_ByteOrder   byte_order
 );


/** Get textual representation of the socket's peer.
 * For INET domain sockets, the result is one of the forms
 * "aaa.bbb.ccc.ddd:ppppp" (IPv4) or "[a:b:c:d::e:f]:ppppp" (IPv6);
 * for UNIX domain socket, the result is the name of the socket's file.
 * @param sock
 *  [in]  socket handle
 * @param buf
 *  [out] pointer to provided buffer to store the text to
 * @param bufsize
 *  [in]  usable size of the buffer above
 * @param format
 *  [in]  what parts of address to include
 * @return
 *  On success, return its "buf" argument; return 0 on error.
 * @sa
 *  SOCK_HostPortToString6, ESOCK_AddressFormat
 */
extern NCBI_XCONNECT_EXPORT char* SOCK_GetPeerAddressStringEx
(SOCK                sock,
 char*               buf,
 size_t              bufsize,
 ESOCK_AddressFormat format
 );


/** Equivalent to SOCK_GetPeerAddressStringEx(.,.,.,eSAF_Full) */
extern NCBI_XCONNECT_EXPORT char* SOCK_GetPeerAddressString
(SOCK   sock,
 char*  buf,
 size_t bufsize
 );


/** By default, sockets will not try to read data from inside SOCK_Write().
 * If you want to automagically upread the data (and cache it in the internal
 * socket buffer) when the write operation is not immediately available,
 * call this func with "on_off" == eOn.
 * Pass "on_off" as eDefault to get current setting.
 * @param on_off
 *
 * @return
 *  Prior setting
 */
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetReadOnWriteAPI
(ESwitch on_off
 );


/** Control the reading-while-writing feature for socket "sock" individually.
 * To reset to the global default behavior (as set by
 * SOCK_SetReadOnWriteAPI()), call this function with "on_off" == eDefault.
 * @param sock
 *  [in]  socket handle
 * @param on_off
 *  [in]  R-on-W setting for this socket
 * @return
 *  Prior setting
 * @sa
 *  SOCK_Read
 */
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetReadOnWrite
(SOCK    sock,
 ESwitch on_off
 );


/** Control OS-defined send strategy by disabling/enabling the TCP Nagle
 * algorithm (which is on by default) that packs multiple requests into a
 * single packet and thus transfers the data in fewer transactions, miminizing
 * the network traffic and generally bursting the throughput.  However, some
 * applications may find it useful to disable this default behavior for the
 * sake of their performance increase (like in case of short transactions
 * otherwise held off by the system in anticipation of coalescing into larger
 * chunks -- a typical example is an interactive transmission of keystrokes).
 * Disabling the Nagle algorithm causes all internally pending yet
 * untransmitted data to flush down to the hardware.
 * @note The setting is overridden by SOCK_SetCork() but it still performs the
 *       flush, if set to disable.
 * @param sock
 *  [in]  socket handle [stream socket only]
 * @param on_off
 *  [in]  1 to disable the send delay; 0 to enable the send delay
 * @sa
 *  SOCK_Write, SOCK_SetCork
 */
extern NCBI_XCONNECT_EXPORT void SOCK_DisableOSSendDelay
(SOCK        sock,
 int/*bool*/ on_off
 );


/** Control OS-defined send strategy by disabling/enabling the TCP layer to
 * send incomplete network frames (packets).  With the "cork" set on, data gets
 * always buffered until a complete hardware packet is full (or connection is
 * about to close), and only then is sent out to the wire.
 * @note The setting cancels the effects of SOCK_DisableOSSendDelay().
 * @param sock
 *  [in]  socket handle [stream socket only]
 * @param on_off
 *  [in]  1 to set the cork; 0 to remove the cork
 * @sa
 *  SOCK_Write, SOCK_DisableOSSendDelay
 */
extern NCBI_XCONNECT_EXPORT void SOCK_SetCork
(SOCK        sock,
 int/*bool*/ on_off
 );



/******************************************************************************
 *  DATAGRAM SOCKETS (connectionless)
 *
 *  How the datagram exchange API works:
 *
 *  Datagram socket is created with special DSOCK_Create[Ex]() calls but the
 *  resulting object is still a SOCK handle.  That is, almost all SOCK routines
 *  may be applied to the handle.  There are few exceptions, though.
 *  In datagram sockets the I/O differs from how it is done in stream sockets:
 *
 *  SOCK_Write() writes data into an internal message buffer, appending new
 *  data as they come with each SOCK_Write().  When the message is complete,
 *  SOCK_SendMsg() should be called (optionally with an additional last,
 *  or the only [if no SOCK_Write() preceded the call] message fragment)
 *  to actually send the message down the wire.  If successful, SOCK_SendMsg()
 *  clears the internal buffer, and the process may repeat.  If unsuccessful,
 *  SOCK_SendMsg() can be repeated with restiction that no additional data are
 *  provided in the call.  This way, the entire message will be attempted to
 *  be sent again.  On the other hand, if after any SOCK_SendMsg() new data
 *  are added [regardless of whether previous data were successfully sent
 *  or not], all previously written [and kept in the internal send buffer]
 *  data get dropped and replaced with the new data.
 *
 *  DSOCK_WaitMsg() can be used to learn whether there is a new message
 *  available for read by DSOCK_RecvMsg() immediately.
 *
 *  SOCK_RecvMsg() receives the message into an internal receive buffer,
 *  and optionally can return the initial datagram fragment via provided
 *  buffer [this initial fragment is then stripped from what remains unread
 *  in the internal buffer].  Optimized version can supply a maximal message
 *  size (if known in advance), or 0 to get a message of any allowed size.
 *  The actual size of the received message can be obtained via a
 *  pointer-type argument "msgsize".  The message kept in the internal buffer
 *  can be read out in several SOCK_Read() calls, last returning eIO_Closed,
 *  when all data have been taken out.  SOCK_Wait() returns eIO_Success while
 *  there are data in the internal message buffer that SOCK_Read() can read.
 *
 *  SOCK_WipeMsg() can be used to clear the internal message buffers in
 *  either eIO_Read or eIO_Write directions, meaning receive and send
 *  buffers correspondingly.
 */

/** Create a datagram socket.
 * @param sock
 *  [out] socket created
 * @param flags
 *  [in]  additional socket properties
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_CreateEx
(SOCK*       sock,
 TSOCK_Flags flags
 );


/** Same as DSOCK_CreateEx(, fSOCK_LogDefault)
 * @param sock
 *  [out] socket created
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_Create
(SOCK* sock
 );


/** Assosiate a datagram socket with a local port.
 * All other attempts to use the same port will result in eIO_Closed (for "port
 * busy") unless SOCK_SetReuseAddress() is called, which then allows multiple
 * sockets to bind to the same port, and receive messages, in undefined order,
 * arriving at that port.
 * Passing 0 will ask the OS to automatically choose an unused port, which then
 * can be obtained via SOCK_GetLocalPort().
 * @param sock
 *  [in]  SOCK from DSOCK_Create[Ex]()
 * @param port
 *  [in]  port to bind to (0 to auto-choose)
 * @param ipv6
 *  [in]  eOff to use IPv4, eOn to use IPv6, eDefault to allow system default
 * @sa
 *  SOCK_SetReuseAddress, SOCK_GetLocalPort
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_Bind6
(SOCK           sock,
 unsigned short port,
 ESwitch        ipv6
 );


/** Same as DSOCK_Bind6(,, eOff) */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_Bind
(SOCK           sock,
 unsigned short port
 );


/** Associate a datagram socket with a destination address.
 * @param sock
 *  [in]  SOCK from DSOCK_Create[Ex]()
 * @param host
 *  [in]  peer host
 * @param port
 *  [in]  peer port
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_Connect
(SOCK           sock,
 const char*    host,
 unsigned short port
 );


/** Wait for a datagram in a datagram socket.
 * @param sock
 *  [in]  SOCK from DSOCK_Create[Ex]()
 * @param timeout
 *  [in]  time to wait for message
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_WaitMsg
(SOCK            sock,
 const STimeout* timeout
 );


/** Receive a datagram from a datagram socket.
 * @param sock
 *  [in]  SOCK from DSOCK_Create[Ex]()
 * @param buf
 *  [in]  buf to store msg at, may be NULL
 * @param bufsize
 *  [in]  buf length provided
 * @param maxmsglen
 *  [in]  maximal expected message len
 * @param msglen
 *  [out] actual msg size, may be NULL
 * @param sender_addr
 *  [out] sender address, may be NULL
 * @param sender_port
 *  [out] host byte order, may be NULL
 * @sa
 *  SOCK_Read, SOCK_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_RecvMsg6
(SOCK            sock,
 void*           buf,
 size_t          bufsize,
 size_t          maxmsglen,
 size_t*         msglen,
 TNCBI_IPv6Addr* sender_addr,
 unsigned short* sender_port
 );


/** Same as DSOCK_RecvMsg6() but only suitable for IPv4 sender address */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_RecvMsg
(SOCK            sock,
 void*           buf,
 size_t          bufsize,
 size_t          maxmsglen,
 size_t*         msglen,
 unsigned int*   sender_addr,
 unsigned short* sender_port
 );


/** Send a datagram to a datagram socket.
 * @param sock
 *  [in]  SOCK from DSOCK_Create[Ex]()
 * @param host
 *  [in]  hostname or dotted IP
 * @param port
 *  [in]  port number, host byte order
 * @param data
 *  [in]  additional data to send
 * @param datalen
 *  [in]  size of additional data (bytes)
 * @sa
 *  SOCK_Write, SOCK_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_SendMsg
(SOCK           sock,
 const char*    host,
 unsigned short port,
 const void*    data,
 size_t         datalen
 );


/** Clear message froma datagram socket.
 * @param sock
 *  [in]  SOCK from DSOCK_Create[Ex]()
 * @param direction
 *  [in]  either of eIO_Read (incoming), eIO_Write (outgoing)
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_WipeMsg
(SOCK      sock,
 EIO_Event direction
 );


/** Set a datagram socket for broadcast.
 * @param sock
 *  [in]  SOCK from DSOCK_Create[Ex]()
 * @param broadcast
 *  [in]  set(1)/unset(0) broadcast capability
 */
extern NCBI_XCONNECT_EXPORT EIO_Status DSOCK_SetBroadcast
(SOCK        sock,
 int/*bool*/ broadcast
 );


/** Get message count.
 * @param sock
 *  [in]  socket handle (datagram socket only)
 * @param direction
 *  [in]  either eIO_Read (in) or eIO_Write (out)
 * @return
 *  Total number of messages sent or received through this datagram socket.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_BigCount DSOCK_GetMessageCount
(SOCK      sock,
 EIO_Event direction
 );



/******************************************************************************
 *  I/O RESTART ON SIGNALS
 */

/** Control restartability of I/O interrupted by signals.
 * By default I/O is restartable if interrupted.
 * Pass "on_off" as eDefault to get the current setting.
 * @param on_off
 *  [in]  eOn to cancel I/O on signals;  eOff to restart
 * @return
 *  Prior setting
 * @sa
 *  SOCK_SetInterruptOnSignal
 */
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetInterruptOnSignalAPI
(ESwitch on_off
 );


/** Control restartability of I/O interrupted by signals on a per-socket basis.
 * eDefault causes the use of global API flag.
 * @param sock
 *  [in]  socket handle
 * @param on_off
 *  [in]  per-socket I/O restart behavior on signals
 * @return
 *  Prior setting
 * @sa
 *  SOCK_SetInterruptOnSignalAPI, SOCK_Create, DSOCK_Create
 */
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetInterruptOnSignal
(SOCK    sock,
 ESwitch on_off
 );



/******************************************************************************
 *  ADDRESS REUSE
 */

/** Control address reuse for socket addresses taken by the API.
 * By default address is not marked for reuse in either SOCK or DSOCK,
 * but is always reused for LSOCK (upon socket closure).
 * Pass "on_off" as eDefault to get the current setting.
 * @param on_off
 *  [in]  whether to turn on (eOn), turn off (eOff) or get current (eDefault)
 * @return
 *  Prior setting
 * @sa
 *  SOCK_SetReuseAddress
 */
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetReuseAddressAPI
(ESwitch on_off
 );


/** Control reuse of socket addresses on per-socket basis
 * Note: only a boolean parameter value is can be used here.
 * @param sock
 *  [in]  socket handle
 * @param on_off
 *  [in]  whether to reuse the address (true, non-zero) or not (false, zero)
 * @sa
 *  SOCK_SetReuseAddressAPI, SOCK_Create, DSOCK_Create
 */
extern NCBI_XCONNECT_EXPORT void SOCK_SetReuseAddress
(SOCK        sock,
 int/*bool*/ on_off
 );



/******************************************************************************
 *  DATA LOGGING & ERROR REPORTING
 *
 * @note  Use CORE_SetLOG() from "ncbi_core.h" to set log handler.
 *
 * @sa
 *  CORE_SetLOG
 */

/** By default data are not logged.
 * @param log
 *  To start logging the data, call this func with "log" == eOn.
 *  To stop  logging the data, call this func with "log" == eOff.
 *  To get current log switch, call this func with "log" == eDefault.
 * @return
 *  Prior (or current, if "log" is eDefault) setting
 * @sa
 *  SOCK_SetDataLogging
 */
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetDataLoggingAPI
(ESwitch log
 );


/** Control the data logging for socket "sock" individually.
 * @param sock
 *  [in]  socket handle
 * @param log
 *  [in]  requested data logging
 *  To reset to the global default behavior (as set by SOCK_SetDataLoggingAPI),
 *  call this function with "log" == eDefault.
 * @return
 *  Prior setting
 * @sa
 *  SOCK_SetDataLoggingAPI, SOCK_Create, DSOCK_Create
 */
extern NCBI_XCONNECT_EXPORT ESwitch SOCK_SetDataLogging
(SOCK    sock,
 ESwitch log
 );


/** User-level error hook.
 */

typedef enum {
    eSOCK_ErrInit = 1,      /**< Socket layer initialization error           */
    eSOCK_ErrDns,           /**< DNS-related error (unresolvable hostname)   */
    eSOCK_ErrIO             /**< I/O-related error                           */
} ESOCK_ErrType;


typedef struct {
    ESOCK_ErrType  type;    /**< See ESOCK_ErrType                           */
    SOCK           sock;    /**< Non-NULL when SOCK-related                  */
    const char*    host;    /**< Host name/IP (or path for non-IP SOCK)      */
    unsigned short port;    /**< Port (host byte order), 0 for non-IP SOCK   */
    EIO_Event      event;   /**< Meaningful only for the eSOCK_ErrIO type    */
    EIO_Status     status;  /**< Status code about to be returned (if known) */
} SSOCK_ErrInfo;


typedef void (*FSOCK_ErrHook)(const SSOCK_ErrInfo* info,
                              void*                data);

extern NCBI_XCONNECT_EXPORT
void SOCK_SetErrHookAPI(FSOCK_ErrHook hook,
                        void*         data);



/******************************************************************************
 *  TYPE & STATISTICS INFORMATION FOR [D]SOCK SOCKETS
 */

/** Check whether a socket is a datagram one.
 * @param sock
 *  [in]  socket handle
 * @return
 *  Non-zero value if socket "sock" was created by DSOCK_Create[Ex]().
 *  Return zero otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsDatagram(SOCK sock);


/** Check whether a socket is client-side.
 * @param sock
 *  [in]  socket handle
 * @return
 *  Non-zero value if socket "sock" was created by SOCK_Create[Ex]().
 *  Return zero otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsClientSide(SOCK sock);


/** Check whether a socket is server-side.
 * @param sock
 *  [in]  socket handle
 * @return
 *  Non-zero value if socket "sock" was created by LSOCK_Accept().
 *  Return zero otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsServerSide(SOCK sock);


/** Check whether a socket is UNIX type.
 * @param sock
 *  [in]  socket handle
 * @return
 *  Non-zero value if socket "sock" is a UNIX family named socket
 *  Return zero otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsUNIX(SOCK sock);


/** Check whether a socket is using SSL (Secure Socket Layer).
 * @param sock
 *  [in]  socket handle
 * @return
 *  Non-zero value if socket "sock" is using Secure Socket Layer (SSL).
 *  Return zero otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsSecure(SOCK sock);


/** Get current read or write position within a socket.
 * @param sock
 *  [in]  socket handle
 * @param direction
 *  [in]  either eIO_Read or eIO_Write
 * @return
 *  Current read or write logical position, which takes any pending (i.e. still
 *  unwritten or yet unread) data into account, by reporting / not reporting
 *  that, respectively, in the position returned.  For datagram sockets, the
 *  position is always relative to the beginning of the current message, and
 *  gets reset with every new message (i.e. does not span across the messages).
 */
extern NCBI_XCONNECT_EXPORT TNCBI_BigCount SOCK_GetPosition
(SOCK      sock,
 EIO_Event direction
 );


/** Get counts of read or written bytes.
 * @param sock
 *  [in]  socket handle
 * @param direction
 *  [in]  either eIO_Read or eIO_Write
 * @return
 *  Count of bytes actually read or written through this socket in the current
 *  session. For datagram sockets the count applies for the last message only;
 *  for stream sockets it counts only since last accept or connect event.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_BigCount SOCK_GetCount
(SOCK      sock,
 EIO_Event direction
 );


/** Get the total volume of data transferred by a socket.
 * @param sock
 *  [in]  socket handle
 * @param direction
 *  [in]  either eIO_Read or eIO_Write
 * @return
 *  Total number of bytes transferred through the socket in its lifetime.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_BigCount SOCK_GetTotalCount
(SOCK      sock,
 EIO_Event direction
 );



/******************************************************************************
 *  GENERIC POLLABLE INTERFACE (please see SOCK_Poll() above for explanations)
 */

/*fwdecl; opaque*/
struct SPOLLABLE_tag;
typedef struct SPOLLABLE_tag* POLLABLE;


typedef struct {
    POLLABLE  poll;
    EIO_Event event;
    EIO_Event revent;
} SPOLLABLE_Poll;


/** Poll for I/O readiness.
 * @param n
 *  [in]      how many elements to scan in the "polls" array parameter
 * @param polls[]
 *  [in/out]  array of handles and event masks to check for I/O, and return
 * @param timeout
 *  [in]      how long to wait for at least one handle to get ready.
 * @param n_ready
 *  [out]     how many elements of the "polls" array returned with their
 *            I/O marked ready
 * @return
 *  eIO_Success if at least one element was found ready, eIO_Timeout if none
 *  and the specified time interval had elapsed, other error code for some
 *  other error condition (in which case the "revent" fields in the array may
 *  not have been updated with valid values).
 * @sa
 *   SOCK_Wait, SOCK_Poll
 */
extern NCBI_XCONNECT_EXPORT EIO_Status POLLABLE_Poll
(size_t          n,
 SPOLLABLE_Poll  polls[],
 const STimeout* timeout,
 size_t*         n_ready
 );


/** Identifies a non-NULL POLLABLE. */
extern NCBI_XCONNECT_EXPORT ESOCK_Type POLLABLE_What(POLLABLE);


/** Conversion utilities from handles to POLLABLEs, and back.
 * @return
 *  Return 0 if conversion cannot be made; otherwise the converted handle.
 */
extern NCBI_XCONNECT_EXPORT POLLABLE POLLABLE_FromTRIGGER(TRIGGER);
extern NCBI_XCONNECT_EXPORT POLLABLE POLLABLE_FromLSOCK  (LSOCK);
extern NCBI_XCONNECT_EXPORT POLLABLE POLLABLE_FromSOCK   (SOCK);
extern NCBI_XCONNECT_EXPORT TRIGGER  POLLABLE_ToTRIGGER(POLLABLE);
extern NCBI_XCONNECT_EXPORT LSOCK    POLLABLE_ToLSOCK  (POLLABLE);
extern NCBI_XCONNECT_EXPORT SOCK     POLLABLE_ToSOCK   (POLLABLE);



/******************************************************************************
 *  AUXILIARY NETWORK-SPECIFIC FUNCTIONS (added for the portability reasons)
 */

/** Convert IP address to a string in dotted notation.
 * @param addr
 *  [in]  must be in the network byte-order
 * @param buf
 *  [out] to be filled by smth. like "123.45.67.89\0"
 * @param bufsize
 *  [in]  max # of bytes to put into "buf" (including the terminating '\0')
 * @return
 *  Zero on success, non-zero on error.  Vaguely related to BSD's inet_ntoa().
 *  On error (including insufficient room in "buf" to store the result) "buf"
 *  is returned empty (buf[0] == '\0').
 */
extern NCBI_XCONNECT_EXPORT int SOCK_ntoa
(unsigned int addr,
 char*        buf,
 size_t       bufsize
 );


/** Check whether a given string represents a valid bare IPv4 address.
 * @param host
 *  [in]  '\0'-terminated string to check against being a plain IPv4 address
 * @param fullquad
 *  [in]  non-zero to only accept "host" if it is a full-quad IPv4 notation
 * @return
 *  Non-zero (true) if given string is an IPv4 address, zero (false) otherwise.
 * @note
 *  Unless "fullquad" is requested, valid IPv4 addresses may have various
 *  formats (e.g. legal to be 32-bit integers, or have fewer than 3 dots).
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_isipEx
(const char* host,
 int/*bool*/ fullquad
 );


/** Equivalent of SOCK_isip(host, 0)
 * @param host
 *  [in]  '\0'-terminated string to check against being a plain IPv4 address
 * @return
 *  Non-zero (true) if given string is an IPv4 address, zero (false) otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_isip
(const char* host
 );


/** Check whether a given string represents a valid bare IPv6 address.
 * @param host
 *  [in]  '\0'-terminated string to check against being a plain IPv6 address
 * @return
 *  Non-zero (true) if given string is an IPv6 address, zero (false) otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_isip6
(const char* host
 );


/** Check whether a given string represents a bare IP address.
* @param host
*  [in]  '\0'-terminated string to check against being a bare address
* @return
*  Non-zero (true) if given string is an IPv6 address, zero (false) otherwise.
* @note
*  Equivalent to "SOCK_isip(host) || SOCK_isip6(host)".
* @sa
*  SOCK_isip, SOCK_isip6
*/
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsAddress
(const char* host
);


/** See man for the BSDisms, htonl() and htons().
 * @param value
 *  The value to convert from host to network byte order.
 */
extern NCBI_XCONNECT_EXPORT unsigned int SOCK_HostToNetLong
(unsigned int value
 );

#define SOCK_NetToHostLong SOCK_HostToNetLong

/** See man for the BSDisms, htonl() and htons().
 * @param value
 *  The value to convert from host to network byte order.
 */
extern NCBI_XCONNECT_EXPORT unsigned short SOCK_HostToNetShort
(unsigned short value
 );

#define SOCK_NetToHostShort SOCK_HostToNetShort


/* Deprecated:  Use SOCK_{Host|Net}To{Net|Host}{Long|Short}() instead */
extern NCBI_XCONNECT_EXPORT NCBI_SOCK_DEPRECATED
unsigned int   SOCK_htonl(unsigned int);
#define        SOCK_ntohl SOCK_htonl

extern NCBI_XCONNECT_EXPORT NCBI_SOCK_DEPRECATED
unsigned short SOCK_htons(unsigned short);
#define        SOCK_ntohs SOCK_htons


/** Get the local host name.
 * @param name
 *  [out] (guaranteed to be '\0'-terminated)
 * @param namelen
 *  [in]   max # of bytes allowed to put into "name"
 * @param log
 *  [in]   whether to log failures
 * @return
 *  Zero on success, non-zero on error.  See BSD gethostname().
 *  On error (incl. insufficient room) "name" returned empty (name[0] == '\0').
 * @sa
 *  SOCK_gethostname, SOCK_gethostbyaddr
 */
extern NCBI_XCONNECT_EXPORT int SOCK_gethostnameEx
(char*   name,
 size_t  namelen,
 ESwitch log
 );


/** Same as SOCK_gethostnameEx(,,<current API data logging>)
 * @sa
 *  SOCK_gethostnameEx, SOCK_gethostbyaddrEx, SOCK_SetDataLoggingAPI
 */
extern NCBI_XCONNECT_EXPORT int SOCK_gethostname
(char*  name,
 size_t namelen
 );


/** Find and return an Internet address of a named host.  The call also accepts
 * numeric IPv4/v6 notations, in which case the conversion is done without
 * consulting the name resolver (DNS).
 * @param addr
 *  [out] IPv6 (including IPv4) address returned
 * @param hostname
 *  [in]  specified host, or the current host if hostname is either 0 or ""
 * @param log
 *  [in]  whether to log failures
 * @return
 *  "addr" when successful and NULL when an error occurred
 * @note Both "::" and "0.0.0.0" result in an error (NULL returned)
 * @sa
 *  SOCK_gethostbyname6, SOCK_gethostname, NcbiStringToIPv4
 */
extern NCBI_XCONNECT_EXPORT TNCBI_IPv6Addr* SOCK_gethostbynameEx6
(TNCBI_IPv6Addr* addr,
 const char*     hostname,
 ESwitch         log
 );


/** Same as SOCK_gethostbynameEx6(,,<current API data logging>)
 * @sa
 *  SOCK_gethostbynameEx6, SOCK_SetDataLoggingAPI
 */
extern NCBI_XCONNECT_EXPORT TNCBI_IPv6Addr* SOCK_gethostbyname6
(TNCBI_IPv6Addr* addr,
 const char*     hostname
 );


/** Find and return IPv4 address of a named host.  The call also accepts dotted
 * IPv4 notation, in which case the conversion is done without consulting the
 * name resolver (DNS).
 * @param hostname
 *  [in]  specified host, or the current host if hostname is either 0 or ""
 * @param log
 *  [in]  whether to log failures
 * @return
 *  IPv4 host address (in network byte order) of the specified host (or of the
 *  local host, if hostname is passed as NULL / empty), which can be specified
 *  as either a domain name or an IPv4 address in the dotted notation (e.g.
 *  "123.45.67.89\0").  Return 0 on error.
 * @note "0.0.0.0" result in an error (0) returned
 * @sa
 *  SOCK_gethostbyname, SOCK_gethostname, NcbiStringToIPv4
 */
extern NCBI_XCONNECT_EXPORT unsigned int SOCK_gethostbynameEx
(const char* hostname,
 ESwitch     log
 );


/** Same as SOCK_gethostbynameEx(,<current API data logging>)
 * @sa
 *  SOCK_gethostbynameEx, SOCK_SetDataLoggingAPI
 */
extern NCBI_XCONNECT_EXPORT unsigned int SOCK_gethostbyname
(const char* hostname
 );


/** Take an Internet address (IPv6 or IPv4) [or 0 (or empty) for current host],
 * and fill in the provided buffer with a name, which the address corresponds
 * to (in case of multiple names the primary name is used).
 * If no name is found in DNS, a text representation of "addr" gets returned.
 * @param addr
 *  [in]  host address (NULL/empty means current host) in network byte order
 * @param buf
 *  [out] buffer to store the name to
 * @param buflen
 *  [in]  size (bytes) of the buffer above
 * @param log
 *  [in]  whether to log failures
 * @return
 *  Value 0 means error;  success is denoted by the 'buf' argument returned.
 *  Note that on error the name gets emptied (buf[0] == '\0').
 * @sa
 *  SOCK_gethostbyaddr6, SOCK_gethostname
 */
extern NCBI_XCONNECT_EXPORT const char* SOCK_gethostbyaddrEx6
(const TNCBI_IPv6Addr* addr,
 char*                 buf,
 size_t                buflen,
 ESwitch               log
 );


/** Same as SOCK_gethostbyaddrEx6(,,<current API data logging>)
 * @sa
 *  SOCK_gethostbyaddrEx6, SOCK_gethostnameEx6, SOCK_SetDataLoggingAPI
 */
extern NCBI_XCONNECT_EXPORT const char* SOCK_gethostbyaddr6
(const TNCBI_IPv6Addr* addr,
 char*                 buf,
 size_t                buflen
 );


/** Take an IPv4 address (in network byte order) [or 0 for current host], and
 * fill in the provided buffer with a name, which the address corresponds to
 * (in case of multiple names the primary name is used).
 * If no name is found in DNS, a text representation of "addr" gets returned.
 * @param addr
 *  [in]  IPv4 host address (0 means current host) in network byte order
 * @param buf
 *  [out] buffer to store the name in
 * @param buflen
 *  [in]  size (bytes) of the buffer above
 * @param log
 *  [in]  whether to log failures
 * @return
 *  Value 0 means error;  success is denoted by the 'buf' argument returned.
 *  Note that on error the name gets emptied (buf[0] == '\0').
 * @sa
 *  SOCK_gethostbyaddr, SOCK_gethostname
 */
extern NCBI_XCONNECT_EXPORT const char* SOCK_gethostbyaddrEx
(unsigned int addr,
 char*        buf,
 size_t       buflen,
 ESwitch      log
 );


/** Same as SOCK_gethostbyaddrEx(,,<current API data logging>)
 * @sa
 *  SOCK_gethostbyaddrEx, SOCK_gethostnameEx, SOCK_SetDataLoggingAPI
 */
extern NCBI_XCONNECT_EXPORT const char* SOCK_gethostbyaddr
(unsigned int addr,
 char*        buf,
 size_t       buflen
 );


/** Get (and cache for faster follow-up retrievals) IP address of local host
 * @param addr
 *  a pointer to the storage area to fill in with the local host address
 * @param reget
 *  eOn      to forcibly re-cache and return the address;
 *  eDefault to re-cache only if unknown, return the [newly] cached value;
 *  eOff     not to re-cache even if unknown, return whatever is available.
 * @return
 *  "addr" if successful; NULL otherwise.
 * @sa
 *  SOCK_gethostname6
 */
extern NCBI_XCONNECT_EXPORT TNCBI_IPv6Addr* SOCK_GetLocalHostAddress6
(TNCBI_IPv6Addr* addr,
 ESwitch         reget
 );


/** Same as SOCK_GetLocalHostAddress6() only strictly for IPv4 */
extern NCBI_XCONNECT_EXPORT unsigned int SOCK_GetLocalHostAddress
(ESwitch         reget
 );


/** Get loopback address.
 * @return
 *  "addr" filled with loopback address; NULL on error
 */
extern NCBI_XCONNECT_EXPORT TNCBI_IPv6Addr* SOCK_GetLoopbackAddress6
(TNCBI_IPv6Addr* addr
 );


/** Get IPv4 loopback address.
* @return
*  Loopback address (in network byte order)
*/
extern NCBI_XCONNECT_EXPORT unsigned int SOCK_GetLoopbackAddress(void);


/** Check whether an address is a loopback one.
 * Return non-zero (true) if the IPv4 address (in network byte order) given
 * in the agrument, is a loopback one;  zero otherwise.
 * @sa
 *  SOCK_IsLoopbackAddress6
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsLoopbackAddress
(unsigned int ip
 );


/** Check whether an address is a loopback one.
* Return non-zero (true) if the the address given in the agrument,
* is a loopback one;  zero otherwise.
* @sa
*  SOCK_IsLoopbackAddress
*/
extern NCBI_XCONNECT_EXPORT int/*bool*/ SOCK_IsLoopbackAddress6
(const TNCBI_IPv6Addr* addr
);


/** Read (skipping leading blanks) "[host][:port]" from a string stopping
 * at either EOL or a blank character.
 * @param str
 *  must not be NULL
 * @param host
 *  may be NULL for no assignment
 * @param port
 *  may be NULL for no assignment
 * @return
 *  On success, return the advanced pointer past the host/port read.
 *  If no host/port detected, return 'str'.  On format error, return 0.
 *  If either host or port fragment is missing, then the corresponding 'host'/
 *  'port' parameters get a value of 0.
 * @note  "0.0.0.0" for the host part gets the host returned as 0 as well.
 * @note  'host' gets returned in network byte order, unlike 'port', which
 *        always comes out in host (native) byte order.
 * @note  ":0" is accepted to denote no-host:zero-port.  
 * @sa
 *   SOCK_HostPortToString, SOCK_isip, SOCK_gethostbyname
 */
extern NCBI_XCONNECT_EXPORT const char* SOCK_StringToHostPort
(const char*     str,
 unsigned int*   host,
 unsigned short* port
 );


/* If an IPv6 address is followed by a port, the address must be enclosed in
 * square brackets.
 */
extern NCBI_XCONNECT_EXPORT const char* SOCK_StringToHostPort6
(const char*     str,
 TNCBI_IPv6Addr* addr,
 unsigned short* port
 );


/** Print numeric string "host:port" into a buffer provided, not to exceed
 * 'bufsize' bytes (including the teminating '\0' character).  Suppress
 * printing the "host" if the 'host' parameter is zero.  Suppress printing the
 * ":port" part if 'port' passed as zero, but if both the 'host' and the 'port'
 * parameters are zero, the output is the literal ":0".
 * @param host
 *  IPv4 in network byte order
 * @param port
 *  in host byte order
 * @param buf
 *  must not be NULL
 * @param bufsize
 *  must be large enough
 * @return
 *  Number of bytes printed, or 0 on error (e.g. buffer is too small).
 * @sa
 *  SOCK_StringToHostPort, SOCK_ntoa, SOCK_gethostbyaddr
 */
extern NCBI_XCONNECT_EXPORT size_t SOCK_HostPortToString
(unsigned int   host,
 unsigned short port,
 char*          buf,
 size_t         bufsize
 );

/** IPv6-aware version of SOCK_HostPortToString, which can handle both
 * IPv4 and IPv6.  IPv6 address, which is followed by a port, gets
 * enclosed into square brackets.  No brackets are used for IPv4 or
 * for IPv6 address, which is not followed by a port ("port"==0).
 * An empty IPv6 "addr" with zero "port" gets output as "::" if API
 * was not set to IPv4-only mode.  Otherwise, the result is ":0".
 * NULL "addr" suppresses the host part output, unconditionally.
 */
extern NCBI_XCONNECT_EXPORT size_t SOCK_HostPortToString6
(const TNCBI_IPv6Addr* addr,
 unsigned short        port,
 char*                 buf,
 size_t                bufsize
 );



/******************************************************************************
 *  SECURE SOCKET LAYER (SSL) SUPPORT
 */

/*fwdecl*/
struct SNcbiCred;
/** Opaque type for credentials */
typedef struct SNcbiCred* NCBI_CRED;

/*fwdecl*/
struct SOCKSSL_struct;
/** Opaque type for SSL implementation */
typedef const struct SOCKSSL_struct* SOCKSSL;


/** SSL setup callback */
typedef SOCKSSL (*FSSLSetup)(void);


/** Store SSL setup callback until actual initialization.
 * @param setup
 *  non-NULL SSL setup routine, or NULL to shut the SSL down
 * @warning
 *  Do _not_ use this function unless you know what you're doing.
 *  Use other means of initialization such as CONNECT_Init() or CConnIniter.
 * @sa
 *  SOCK_SetupSSLEx, CONNECT_Init, CConnIniter
 */
extern NCBI_XCONNECT_EXPORT void SOCK_SetupSSL(FSSLSetup setup);


/** Take SSL setup callback, and then init the entire API (SOCK + SSL).
 * @param setup
 *  non-NULL SSL setup routine, or NULL to shut the SSL down
 * @return
 *  eIO_Success if successful, other code on error.
 * @warning
 *  Do _not_ use this function unless you know what you're doing.
 *  Use other means of initialization such as CONNECT_Init() or CConnIniter.
 * @sa
 *  SOCK_SetupSSL, CONNECT_Init, CConnIniter
 */ 
extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_SetupSSLEx(FSSLSetup setup);


/** Return an SSL provider name (in a const static storage), with special
 *  considerations:  return NULL if SSL has not been set up (yet), and return
 *  "" if SSL has not been (properly) initialized for use.
 * @note
 *  "NONE" is returned as a name if SSL has been explicitly disabled.
 * @sa
 *   SOCK_SetupSSL, SOCK_SetupSSLEx
 */
extern NCBI_XCONNECT_EXPORT const char* SOCK_SSLName(void);


#ifdef __cplusplus
} /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SOCKET__H */
