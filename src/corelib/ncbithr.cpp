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
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 * File Description:
 *   Multi-threading -- classes, functions, and features.
 *
 *    TLS:
 *      CTlsBase         -- TLS implementation (base class for CTls<>)
 *
 *    THREAD:
 *      CThread          -- thread wrapper class
 *
 *    RW-LOCK:
 *      CInternalRWLock  -- platform-dependent RW-lock structure (fwd-decl)
 *      CRWLock          -- Read/Write lock related  data and methods
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/error_codes.hpp>
#ifdef NCBI_POSIX_THREADS
#  include <sys/time.h> // for gettimeofday()
#endif
#ifdef NCBI_OS_LINUX
#  include <sys/prctl.h>
#endif

#include "ncbidbg_p.hpp"


#define NCBI_USE_ERRCODE_X   Corelib_Threads

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CTlsBase::
//


DEFINE_STATIC_MUTEX(s_TlsCleanupMutex);


CUsedTlsBases::CUsedTlsBases(void)
{
}


CUsedTlsBases::~CUsedTlsBases(void)
{
}


void CUsedTlsBases::ClearAll(CTlsBase::ECleanupMode mode)
{
    CMutexGuard tls_cleanup_guard(s_TlsCleanupMutex);
    // Prevent double-destruction
    CTlsBase* used_tls = NULL;
    NON_CONST_ITERATE(TTlsSet, it, m_UsedTls) {
        CTlsBase* tls = *it;
        // Do not cleanup it now - this will cause infinite recursion
        if (tls == &sm_UsedTlsBases.Get()) {
            used_tls = tls;
            continue;
        }
        // Prevent double-destruction
        tls->x_DeleteTlsData(mode);
        if (tls->m_AutoDestroy  &&  tls->Referenced()) {
            tls->RemoveReference();
        }
    }
    m_UsedTls.clear();

    if (used_tls) {
        used_tls->x_DeleteTlsData(mode);
        if (used_tls->m_AutoDestroy  &&  used_tls->Referenced()) {
            used_tls->RemoveReference();
        }
    }
}


void CUsedTlsBases::Register(CTlsBase* tls)
{
    CMutexGuard tls_cleanup_guard(s_TlsCleanupMutex);
    if ( m_UsedTls.insert(tls).second ) {
        if (tls->m_AutoDestroy) {
            tls->AddReference();
        }
    }
}


void CUsedTlsBases::Deregister(CTlsBase* tls)
{
    CMutexGuard tls_cleanup_guard(s_TlsCleanupMutex);
    xncbi_VerifyAndErrorReport(m_UsedTls.erase(tls));
    if (tls->m_AutoDestroy) {
        tls->RemoveReference();
    }
}


static void s_CleanupUsedTlsBases(CUsedTlsBases* tls, void*)
{
    delete tls;
}

static void s_CleanupMainUsedTlsBases(CUsedTlsBases& tls)
{
    tls.ClearAll();
}

// Storage for used TLS sets
CStaticTls<CUsedTlsBases>
    CUsedTlsBases::sm_UsedTlsBases(0, CSafeStaticLifeSpan::eLifeSpan_Long);
// Main thread needs a usual safe-static-ref for proper cleanup --
// there's no thread which can do it on destruction.
static CSafeStatic<CUsedTlsBases>
s_MainUsedTlsBases(0, s_CleanupMainUsedTlsBases,
CSafeStaticLifeSpan::eLifeSpan_Long);

CUsedTlsBases& CUsedTlsBases::GetUsedTlsBases(void)
{
    if ( CThread::IsMain() )
    {
        return *s_MainUsedTlsBases;
    }

    CUsedTlsBases* tls = sm_UsedTlsBases.GetValue();
    if ( !tls )
    {
        tls = new CUsedTlsBases();
        sm_UsedTlsBases.SetValue(tls, s_CleanupUsedTlsBases, nullptr, CTlsBase::eDoCleanup);
    }
    return *tls;
}


void CUsedTlsBases::Init(void)
{
    sm_UsedTlsBases.Get();
}


void CUsedTlsBases::ClearAllCurrentThread(CTlsBase::ECleanupMode mode)
{
    if ( CUsedTlsBases* tls = sm_UsedTlsBases.GetValue() ) {
        tls->ClearAll(mode);
    }
}


struct SNativeThreadTlsCleanup
{
    ~SNativeThreadTlsCleanup(void) {
        if ( CThread::IsMain() ) return;
        CUsedTlsBases::ClearAllCurrentThread(CTlsBase::eCleanup_Native);
    }
};


void CTlsBase::CleanupTlsData(void* data, ECleanupMode mode)
{
    if (!data) return;
    STlsData* tls_data = static_cast<STlsData*>(data);
    if (!tls_data->m_Value || !tls_data->m_CleanupFunc) return;
    if (mode == eCleanup_Native && tls_data->m_Native == eSkipCleanup) return;
    tls_data->m_CleanupFunc(tls_data->m_Value, tls_data->m_CleanupData);
}


void CTlsBase::CleanupAndDeleteTlsData(void* data, ECleanupMode mode)
{
    if (!data) return;
    STlsData* tls_data = static_cast<STlsData*>(data);
    CleanupTlsData(data, mode);
    delete tls_data;
}


void CTlsBase::x_CleanupThreadCallback(void* ptr)
{
    CTlsBase::CleanupAndDeleteTlsData(ptr);
}


void CTlsBase::x_Init(void)
{
    // Create platform-dependent TLS key (index)
#if defined(NCBI_WIN32_THREADS)
    xncbi_VerifyAndErrorReport((m_Key = TlsAlloc()) != DWORD(-1));
#elif defined(NCBI_POSIX_THREADS)
    xncbi_VerifyAndErrorReport(pthread_key_create(&m_Key, x_CleanupThreadCallback) == 0);
    // pthread_key_create does not reset the value to 0 if the key has been
    // used and deleted.
    xncbi_VerifyAndErrorReport(pthread_setspecific(m_Key, 0) == 0);
#else
    m_Key = 0;
#endif

    m_Initialized = true;
}


void CTlsBase::x_Destroy(void)
{
    x_Reset();
    m_Initialized = false;

    // Destroy system TLS key
#if defined(NCBI_WIN32_THREADS)
    if ( TlsFree(m_Key) ) {
        m_Key = 0;
        return;
    }
    assert(0);
#elif defined(NCBI_POSIX_THREADS)
    if (pthread_key_delete(m_Key) == 0) {
        m_Key = 0;
        return;
    }
    assert(0);
#else
    m_Key = 0;
    return;
#endif
}


// Platform-specific TLS data storing
inline
void s_TlsSetValue(TTlsKey& key, void* data, const char* err_message)
{
#if defined(NCBI_WIN32_THREADS)
    xncbi_Validate(TlsSetValue(key, data), err_message);
#elif defined(NCBI_POSIX_THREADS)
    xncbi_ValidatePthread(pthread_setspecific(key, data), 0, err_message);
#else
    key = data;
    assert(err_message);  // to get rid of the "unused variable" warning
#endif
}


void CTlsBase::x_SetValue(void*        value,
                          FCleanupBase cleanup,
                          void*        cleanup_data,
                          ENativeThreadCleanup native)
{
    if ( !m_Initialized ) {
        return;
    }

    // Get previously stored data
    STlsData* tls_data = static_cast<STlsData*> (x_GetTlsData());

    // Create and initialize TLS structure, if it was not present
    if ( !tls_data ) {
        tls_data = new STlsData;
        xncbi_Validate(tls_data != 0,
                       "CTlsBase::x_SetValue() -- cannot allocate "
                       "memory for TLS data");
        tls_data->m_Value       = 0;
        tls_data->m_CleanupFunc = 0;
        tls_data->m_CleanupData = 0;
        tls_data->m_Native = eSkipCleanup;

#ifdef NCBI_WIN32_THREADS
        static thread_local SNativeThreadTlsCleanup s_NativeThreadTlsCleanup;
#endif
    }

    // Cleanup
    if (tls_data->m_Value != value) {
        CleanupTlsData(tls_data);
    }

    // Store the values
    tls_data->m_Value       = value;
    tls_data->m_CleanupFunc = cleanup;
    tls_data->m_CleanupData = cleanup_data;
    tls_data->m_Native = native;

    // Store the structure in the TLS
    s_TlsSetValue(m_Key, tls_data,
                  "CTlsBase::x_SetValue() -- error setting value");

    // Add to the used TLS list to cleanup data in the thread Exit()
    CUsedTlsBases::GetUsedTlsBases().Register(this);
}


bool CTlsBase::x_DeleteTlsData(ECleanupMode mode)
{
    if ( !m_Initialized ) {
        return false;
    }

    // Get previously stored data
    STlsData* tls_data = static_cast<STlsData*> (x_GetTlsData());
    if ( !tls_data ) {
        return false;
    }

    // Cleanup & destroy
    CleanupAndDeleteTlsData(tls_data, mode);

    // Store NULL in the TLS
    s_TlsSetValue(m_Key, 0,
                  "CTlsBase::x_Reset() -- error cleaning-up TLS");

    return true;
}


void CTlsBase::x_Reset(void)
{
    if ( x_DeleteTlsData() ) {
        // Deregister this TLS from the current thread
        CUsedTlsBases::GetUsedTlsBases().Deregister(this);
    }
}


/////////////////////////////////////////////////////////////////////////////
//  CExitThreadException::
//
//    Exception used to terminate threads safely, cleaning up
//    all the resources allocated.
//


class CExitThreadException
{
public:
    // Create new exception object, initialize counter.
    CExitThreadException(void);

    // Create a copy of exception object, increase counter.
    CExitThreadException(const CExitThreadException& prev);

    // Destroy the object, decrease counter. If the counter is
    // zero outside of CThread::Wrapper(), rethrow exception.
    ~CExitThreadException(void);

    // Inform the object it has reached CThread::Wrapper().
    void EnterWrapper(void)
    {
        *m_InWrapper = true;
    }
private:
    int* m_RefCount;
    bool* m_InWrapper;
};


CExitThreadException::CExitThreadException(void)
    : m_RefCount(new int),
      m_InWrapper(new bool)
{
    *m_RefCount = 1;
    *m_InWrapper = false;
}


CExitThreadException::CExitThreadException(const CExitThreadException& prev)
    : m_RefCount(prev.m_RefCount),
      m_InWrapper(prev.m_InWrapper)
{
    (*m_RefCount)++;
}


CExitThreadException::~CExitThreadException(void)
{
    if (--(*m_RefCount) > 0) {
        // Not the last object - continue to handle exceptions
        return;
    }

    bool tmp_in_wrapper = *m_InWrapper; // save the flag
    delete m_RefCount;
    delete m_InWrapper;

    if ( !tmp_in_wrapper ) {
        // Something is wrong - terminate the thread
        assert(((void)("CThread::Exit() -- cannot exit thread"), 0));
#if defined(NCBI_WIN32_THREADS)
        ExitThread(0);
#elif defined(NCBI_POSIX_THREADS)
        pthread_exit(0);
#endif
    }

}



/////////////////////////////////////////////////////////////////////////////
//  CThread::
//

// Mutex to protect CThread members and to make sure that Wrapper() function
// will not proceed until after the appropriate Run() is finished.
DEFINE_STATIC_FAST_MUTEX(s_ThreadMutex);

atomic<unsigned int> CThread::sm_ThreadsCount(0);


// Internal storage for thread objects and related variables/functions
static DECLARE_TLS_VAR(CThread*, sx_ThreadPtr);
static DECLARE_TLS_VAR(CThread::TID, sx_ThreadId);
static bool sm_MainThreadIdInitialized = false;
static const CThread::TID kMainThreadId = ~CThread::TID(0);
static CThread::TID sx_MainThreadId = kMainThreadId;


DEFINE_STATIC_FAST_MUTEX(s_MainThreadIdMutex);

CThread::TID sx_GetMainThreadId()
{
    CFastMutexGuard guard(s_MainThreadIdMutex);
    return sx_MainThreadId;
}


void sx_SetMainThreadId(CThread::TID id)
{
    CFastMutexGuard guard(s_MainThreadIdMutex);
    sx_MainThreadId = id;
}


static int sx_GetNextThreadId(void)
{
    CFastMutexGuard guard(s_ThreadMutex);
    static int s_ThreadCount = 0;
    return ++s_ThreadCount;
}


void CThread::x_InitializeThreadId(void)
{
#if defined(NCBI_THREADS)
    _ASSERT(!sx_ThreadPtr);
    _ASSERT(!sx_ThreadId);
#endif
    sx_ThreadPtr = this;
    sx_ThreadId = sx_GetNextThreadId();
}


void CThread::InitializeMainThreadId(void)
{
    // mark main thread
#if defined(NCBI_THREADS)
    CFastMutexGuard guard(s_MainThreadIdMutex);
#endif
    if ( sm_MainThreadIdInitialized ) {
        if (sx_ThreadId != sx_MainThreadId) {
            ERR_POST("Can not change main thread ID");
        }
        return;
    }
#if defined(NCBI_THREADS)
    _ASSERT(!sx_ThreadPtr);
    _ASSERT(sx_MainThreadId == kMainThreadId);
#endif
    if ( !sx_ThreadId ) {
        // Not yet assigned - use the default value.
        sx_ThreadId = kMainThreadId;
    }
    sx_MainThreadId = sx_ThreadId;
    sx_ThreadPtr = 0;
    sm_MainThreadIdInitialized = true;
}


CThread* CThread::GetCurrentThread(void)
{
    // Get pointer to the current thread object
    return sx_ThreadPtr;
}


CThread::TID CThread::GetSelf(void)
{
    TID id = sx_ThreadId;
    if ( !id ) {
        // If main thread has not been set, consider current thread is the main one.
        // Since sx_ThreadId is still zero, InitializeMainThreadId() will set it to
        // kMainThreadId, so that the value returned by GetSelf() will be zero.
        if (!sm_MainThreadIdInitialized) {
            InitializeMainThreadId();
            id = sx_ThreadId;
        }
        else {
            sx_ThreadId = id = sx_GetNextThreadId();
        }
    }
    // kMainThreadId is usually marker for main thread, but when using native threads
    // and InitializeMainThreadId() to set main thread, the main thread id may be
    // different and it's more reliable to use IsMain() rather than GetSelf() == 0.
    return id == kMainThreadId ? 0 : id;
}


bool CThread::IsMain(void)
{
    if (!sm_MainThreadIdInitialized) {
        InitializeMainThreadId();
    }
    return sx_ThreadId == sx_GetMainThreadId();
}


NCBI_PARAM_DECL(bool, Thread, Catch_Unhandled_Exceptions);
NCBI_PARAM_DEF_EX(bool, Thread, Catch_Unhandled_Exceptions, true, 0,
                  THREAD_CATCH_UNHANDLED_EXCEPTIONS);
typedef NCBI_PARAM_TYPE(Thread, Catch_Unhandled_Exceptions) TParamThreadCatchExceptions;


TWrapperRes CThread::Wrapper(TWrapperArg arg)
{
    // Get thread object and self ID
    CThread* thread_obj = static_cast<CThread*>(arg);

    // Set Toolkit thread ID.
    thread_obj->x_InitializeThreadId();
    xncbi_Validate(!IsMain(),
                   "CThread::Wrapper() -- error assigning thread ID");

#if defined NCBI_THREAD_PID_WORKAROUND
    // Store this thread's PID. Changed PID means forking of the thread.
    thread_obj->m_ThreadPID =
        CProcess::sx_GetPid(CProcess::ePID_GetThread);
#endif

    bool catch_all = TParamThreadCatchExceptions::GetDefault();

    // Check if parent request context should be used.
    if ( thread_obj->m_ParentRequestContext ) {
        CDiagContext::SetRequestContext(thread_obj->m_ParentRequestContext);
    }

    // Run user-provided thread main function here
    if ( catch_all ) {
        try {
            thread_obj->m_ExitData = thread_obj->Main();
        }
        catch (CExitThreadException& e) {
            e.EnterWrapper();
        }
#if defined(NCBI_COMPILER_MSVC)  &&  defined(_DEBUG)
        // Microsoft promotes many common application errors to exceptions.
        // This includes occurrences such as dereference of a NULL pointer and
        // walking off of a dangling pointer.  The catch-all is lifted only in
        // debug mode to permit easy inspection of such error conditions, while
        // maintaining safety of production, release-mode applications.
        NCBI_CATCH_X(1, "CThread::Wrapper: CThread::Main() failed");
#else
        NCBI_CATCH_ALL_X(2, "CThread::Wrapper: CThread::Main() failed");
#endif
    }
    else {
        try {
            thread_obj->m_ExitData = thread_obj->Main();
        }
        catch (CExitThreadException& e) {
            e.EnterWrapper();
        }
    }

    // Call user-provided OnExit()
    if ( catch_all ) {
        try {
            thread_obj->OnExit();
        }
#if defined(NCBI_COMPILER_MSVC)  &&  defined(_DEBUG)
        // Microsoft promotes many common application errors to exceptions.
        // This includes occurrences such as dereference of a NULL pointer and
        // walking off of a dangling pointer.  The catch-all is lifted only in
        // debug mode to permit easy inspection of such error conditions, while
        // maintaining safety of production, release-mode applications.
        NCBI_CATCH_X(3, "CThread::Wrapper: CThread::OnExit() failed");
#else
        NCBI_CATCH_ALL_X(4, "CThread::Wrapper: CThread::OnExit() failed");
#endif
    }
    else {
        thread_obj->OnExit();
    }

    // Cleanup local storages used by this thread
    CUsedTlsBases::ClearAllCurrentThread();

    {{
        CFastMutexGuard state_guard(s_ThreadMutex);

        // Thread is terminated - decrement counter under mutex
        --sm_ThreadsCount;

        // Indicate the thread is terminated
        thread_obj->m_IsTerminated = true;

        // Schedule the thread object for destruction, if detached
        if ( thread_obj->m_IsDetached ) {
            thread_obj->m_SelfRef.Reset();
        }
    }}

    return 0;
}


CThread::CThread(void)
    : m_Handle(0), m_IsRun(false),
      m_IsDetached(false),
      m_IsJoined(false),
      m_IsTerminated(false),
      m_ExitData(0)
#if defined NCBI_THREAD_PID_WORKAROUND
      , m_ThreadPID(0)
#endif
{
    DoDeleteThisObject();
}


CThread::~CThread(void)
{
#if defined(NCBI_WIN32_THREADS)
    // close handle if it's not yet closed
//    CFastMutexGuard state_guard(s_ThreadMutex);
    if ( m_IsRun && m_Handle != NULL ) {
        CloseHandle(m_Handle);
        m_Handle = NULL;
    }
#endif
}



inline TWrapperRes ThreadWrapperCaller(TWrapperArg arg) {
    return CThread::Wrapper(arg);
}

#if defined(NCBI_POSIX_THREADS)
extern "C" {
    typedef TWrapperRes (*FSystemWrapper)(TWrapperArg);

    static TWrapperRes ThreadWrapperCallerImpl(TWrapperArg arg) {
        return ThreadWrapperCaller(arg);
    }
}
#elif defined(NCBI_WIN32_THREADS)
extern "C" {
    typedef TWrapperRes (WINAPI *FSystemWrapper)(TWrapperArg);

    static TWrapperRes WINAPI ThreadWrapperCallerImpl(TWrapperArg arg) {
        return ThreadWrapperCaller(arg);
    }
}
#endif


#if defined NCBI_THREAD_PID_WORKAROUND
TPid CThread::sx_GetThreadPid(void)
{
    CThread* thread_ptr = GetCurrentThread();
    return thread_ptr ? thread_ptr->m_ThreadPID : 0;
}


void CThread::sx_SetThreadPid(TPid pid)
{
    CThread* thread_ptr = GetCurrentThread();
    if ( thread_ptr ) {
        thread_ptr->m_ThreadPID = pid;
    }
}
#endif


#define NCBI_THREAD_VALIDATE(cond, error_code, message) \
    if ( !(cond) ) NCBI_THROW(CThreadException, error_code, message)


// Stack size parameter, 2M by default.
NCBI_PARAM_DECL(size_t, Thread, StackSize);
NCBI_PARAM_DEF_EX(size_t, Thread, StackSize, 2048*1024, eParam_NoThread, THREAD_STACK_SIZE);
typedef NCBI_PARAM_TYPE(Thread, StackSize) TParamThreadStackSize;


bool CThread::Run(TRunMode flags)
{
    CUsedTlsBases::Init();

    // Do not allow the new thread to run until m_Handle is set
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Check
    NCBI_THREAD_VALIDATE(!m_IsRun, eRunError,
        "CThread::Run() -- called for already started thread");

    m_IsDetached = (flags & fRunDetached) != 0;

#if defined NCBI_THREAD_PID_WORKAROUND
    CProcess::sx_GetPid(CProcess::ePID_GetCurrent);
#endif

    // Thread will run - increment counter under mutex
    ++sm_ThreadsCount;
    try {

        if (flags & fRunCloneRequestContext) {
            m_ParentRequestContext = CDiagContext::GetRequestContext().Clone();
        }

#if defined(NCBI_WIN32_THREADS)
        // We need this parameter in WinNT - can not use NULL instead!
        DWORD thread_id;
        // Suspend thread to adjust its priority
        DWORD creation_flags = (flags & fRunNice) == 0 ? 0 : CREATE_SUSPENDED;
        m_Handle = CreateThread(NULL, 0, ThreadWrapperCallerImpl,
                                this, creation_flags, &thread_id);
        NCBI_THREAD_VALIDATE(m_Handle != NULL, eRunError,
            "CThread::Run() -- error creating thread");
        if (flags & fRunNice) {
            // Adjust priority and resume the thread
            SetThreadPriority(m_Handle, THREAD_PRIORITY_BELOW_NORMAL);
            ResumeThread(m_Handle);
        }
        if ( m_IsDetached ) {
            CloseHandle(m_Handle);
            m_Handle = NULL;
        }
        else {
            // duplicate handle to adjust security attributes
            HANDLE oldHandle = m_Handle;
            NCBI_THREAD_VALIDATE(DuplicateHandle(GetCurrentProcess(), oldHandle,
                GetCurrentProcess(), &m_Handle,
                0, FALSE, DUPLICATE_SAME_ACCESS),
                eRunError, "CThread::Run() -- error getting thread handle");
            NCBI_THREAD_VALIDATE(CloseHandle(oldHandle),
                eRunError, "CThread::Run() -- error closing thread handle");
        }
#elif defined(NCBI_POSIX_THREADS)
        pthread_attr_t attr;
        NCBI_THREAD_VALIDATE(pthread_attr_init(&attr) == 0, eRunError,
            "CThread::Run() - error initializing thread attributes");
        if ( ! (flags & fRunUnbound) ) {
#if defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_CYGWIN)  ||  defined(NCBI_OS_IRIX)
            NCBI_THREAD_VALIDATE(pthread_attr_setscope(&attr,
                PTHREAD_SCOPE_PROCESS) == 0, eRunError,
                "CThread::Run() - error setting thread scope");
#else
            NCBI_THREAD_VALIDATE(pthread_attr_setscope(&attr,
                PTHREAD_SCOPE_SYSTEM) == 0, eRunError,
                "CThread::Run() - error setting thread scope");
#endif
        }
        if ( m_IsDetached ) {
            NCBI_THREAD_VALIDATE(pthread_attr_setdetachstate(&attr,
                PTHREAD_CREATE_DETACHED) == 0, eRunError,
                "CThread::Run() - error setting thread detach state");
        }
        NCBI_THREAD_VALIDATE(pthread_attr_setstacksize(&attr,
            TParamThreadStackSize::GetDefault()) == 0, eRunError,
            "Thread::Run() -- error setting stack size");
        NCBI_THREAD_VALIDATE(pthread_create(&m_Handle, &attr,
            ThreadWrapperCallerImpl, this) == 0, eRunError,
            "CThread::Run() -- error creating thread");

        NCBI_THREAD_VALIDATE(pthread_attr_destroy(&attr) == 0, eRunError,
            "CThread::Run() - error destroying thread attributes");

#else
        if (flags & fRunAllowST) {
            Wrapper(this);
        }
        else {
            NCBI_THREAD_VALIDATE(0, eRunError,
                "CThread::Run() -- system does not support threads");
        }
#endif

        // prevent deletion of CThread until thread is finished
        m_SelfRef.Reset(this);

    }
    catch (...) {
        // In case of any error we need to decrement threads count
        --sm_ThreadsCount;
        throw;
    }

    // Indicate that the thread is run
    m_IsRun = true;
    return true;
}


void CThread::Detach(void)
{
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Check the thread state: it must be run, but not detached yet
    NCBI_THREAD_VALIDATE(m_IsRun, eControlError,
        "CThread::Detach() -- called for not yet started thread");
    NCBI_THREAD_VALIDATE(!m_IsDetached, eControlError,
        "CThread::Detach() -- called for already detached thread");

    // Detach the thread
#if defined(NCBI_WIN32_THREADS)
    NCBI_THREAD_VALIDATE(CloseHandle(m_Handle), eControlError,
        "CThread::Detach() -- error closing thread handle");
    m_Handle = NULL;
#elif defined(NCBI_POSIX_THREADS)
    NCBI_THREAD_VALIDATE(pthread_detach(m_Handle) == 0, eControlError,
        "CThread::Detach() -- error detaching thread");
#endif

    // Indicate the thread is detached
    m_IsDetached = true;

    // Schedule the thread object for destruction, if already terminated
    if ( m_IsTerminated ) {
        m_SelfRef.Reset();
    }
}


void CThread::Join(void** exit_data)
{
    // Check the thread state: it must be run, but not detached yet
    {{
        CFastMutexGuard state_guard(s_ThreadMutex);
        NCBI_THREAD_VALIDATE(m_IsRun, eControlError,
            "CThread::Join() -- called for not yet started thread");
        NCBI_THREAD_VALIDATE(!m_IsDetached, eControlError,
            "CThread::Join() -- called for detached thread");
        NCBI_THREAD_VALIDATE(!m_IsJoined, eControlError,
            "CThread::Join() -- called for already joined thread");
        m_IsJoined = true;
    }}

    // Join (wait for) and destroy
#if defined(NCBI_WIN32_THREADS)
    NCBI_THREAD_VALIDATE(WaitForSingleObject(m_Handle, INFINITE) == WAIT_OBJECT_0,
        eControlError, "CThread::Join() -- can not join thread");
    DWORD status;
    NCBI_THREAD_VALIDATE(GetExitCodeThread(m_Handle, &status) &&
        status != DWORD(STILL_ACTIVE), eControlError,
        "CThread::Join() -- thread is still running after join");
    NCBI_THREAD_VALIDATE(CloseHandle(m_Handle), eControlError,
        "CThread::Join() -- can not close thread handle");
    m_Handle = NULL;
#elif defined(NCBI_POSIX_THREADS)
    NCBI_THREAD_VALIDATE(pthread_join(m_Handle, 0) == 0, eControlError,
        "CThread::Join() -- can not join thread");
#endif

    // Set exit_data value
    if ( exit_data ) {
        *exit_data = m_ExitData;
    }

    // Schedule the thread object for destruction
    {{
        CFastMutexGuard state_guard(s_ThreadMutex);
        m_SelfRef.Reset();
    }}
}


void CThread::Exit(void* exit_data)
{
    // Don't exit from the main thread
    CThread* x_this = GetCurrentThread();
    NCBI_THREAD_VALIDATE(x_this != 0, eControlError,
        "CThread::Exit() -- attempt to call for the main thread");

    {{
        CFastMutexGuard state_guard(s_ThreadMutex);
        x_this->m_ExitData = exit_data;
    }}

    // Throw the exception to be caught by Wrapper()
    throw CExitThreadException();
}


bool CThread::Discard(void)
{
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Do not discard after Run()
    if ( m_IsRun ) {
        return false;
    }

    // Schedule for destruction (or, destroy it right now if there is no
    // other CRef<>-based references to this object left).
    m_SelfRef.Reset(this);
    m_SelfRef.Reset();
    return true;
}


void CThread::OnExit(void)
{
    return;
}


void CThread::GetSystemID(TThreadSystemID* id)
{
    *id = GetCurrentThreadSystemID();
}


#if defined(NCBI_OS_LINUX) && defined(PR_SET_NAME)
void CThread::SetCurrentThreadName(const CTempString& name)
{
    prctl(PR_SET_NAME, (unsigned long)name.data(), 0, 0, 0);
}
#elif defined(NCBI_POSIX_THREADS)
void CThread::SetCurrentThreadName(const CTempString& name)
{
# if defined(NCBI_OS_DARWIN)
    pthread_setname_np(name.data());
# else
    pthread_setname_np(pthread_self(), name.data());
# endif
}
#else
void CThread::SetCurrentThreadName(const CTempString&)
{
}
#endif


bool CThread::sm_IsExiting = false;
CTimeout CThread::sm_WaitForThreadsTimeout = CTimeout(0.1);


void CThread::SetWaitForAllThreadsTimeout(const CTimeout& timeout)
{
    sm_WaitForThreadsTimeout = timeout;
}


bool CThread::WaitForAllThreads(void)
{
    if (sm_ThreadsCount == 0) return true;
    if ( !IsMain() ) return false;

    CStopWatch sw(CStopWatch::eStart);
    bool infinite = sm_WaitForThreadsTimeout.IsInfinite();
    unsigned long to = 0;
    unsigned long q = 10;
    if ( !infinite ) {
        to = sm_WaitForThreadsTimeout.GetAsMilliSeconds();
        if (to < q) q = to;
    }
    while (sm_ThreadsCount > 0  &&  (infinite || sw.Elapsed()*1000 < to)) {
        SleepMilliSec(q);
    }
    return sm_ThreadsCount == 0;
}


const char* CThreadException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eRunError:     return "eRunError";
    case eControlError: return "eControlError";
    case eOther:        return "eOther";
    default:            return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
