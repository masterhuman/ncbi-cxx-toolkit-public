#ifndef CORELIB___NCBIAPP_API__HPP
#define CORELIB___NCBIAPP_API__HPP

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
 * Authors:  Denis Vakatov, Vsevolod Sandomirskiy
 *
 *
 */

/// @file ncbiapp.hpp
/// Defines the CNcbiApplication and CAppException classes for creating
/// NCBI applications.
///
/// The CNcbiApplicationAPI class defines the application framework and the high
/// high level behavior of an application, and the CAppException class is used
/// for the exceptions generated by CNcbiApplicationAPI.


#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/metareg.hpp>
#include <corelib/version_api.hpp>

/// Avoid preprocessor name clash with the NCBI C Toolkit.
#if !defined(NCBI_OS_UNIX)  ||  defined(HAVE_NCBI_C)
#  if defined(GetArgs)
#    undef GetArgs
#  endif
#  define GetArgs GetArgs
#endif


/** @addtogroup AppFramework
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CAppException --
///
/// Define exceptions generated by CNcbiApplicationAPI.
///
/// CAppException inherits its basic functionality from CCoreException
/// and defines additional error codes for applications.

class NCBI_XNCBI_EXPORT CAppException : public CCoreException
{
public:
    /// Error types that an application can generate.
    ///
    /// These error conditions are checked for and caught inside AppMain().
    enum EErrCode {
        eUnsetArgs,     ///< Command-line argument description not found
        eSetupDiag,     ///< Application diagnostic stream setup failed
        eLoadConfig,    ///< Registry data failed to load from config file
        eSecond,        ///< Second instance of CNcbiApplicationAPI is prohibited
        eNoRegistry     ///< Registry file cannot be opened
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const override;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CAppException, CCoreException);
};



///////////////////////////////////////////////////////
// CNcbiApplicationAPI
//


/////////////////////////////////////////////////////////////////////////////
///
/// CNcbiApplicationAPI --
///
/// Basic (abstract) NCBI application class.
///
/// Defines the high level behavior of an NCBI application.
/// A new application is written by deriving a class from the CNcbiApplicationAPI
/// and writing an implementation of the Run() and maybe some other (like
/// Init(), Exit(), etc.) methods.

class NCBI_XNCBI_EXPORT CNcbiApplicationAPI
{
public:
    /// Singleton method.
    ///
    /// Track the instance of CNcbiApplicationAPI, and throw an exception
    /// if an attempt is made to create another instance of the application.
    /// @return
    ///   Current application instance.
    /// @sa
    ///   GetInstanceMutex()
    static CNcbiApplicationAPI* Instance(void);

    /// Mutex for application singleton object.
    ///
    /// Lock this mutex when calling Instance()
    /// @return
    ///   Reference to application instance mutex.
    /// @sa
    ///   Instance()
    static SSystemMutex& GetInstanceMutex(void);

    /// Constructor.
    ///
    /// Register the application instance, and reset important
    /// application-specific settings to empty values that will
    /// be set later.
    explicit
    CNcbiApplicationAPI(const SBuildInfo& build_info);

    /// Destructor.
    ///
    /// Clean up the application settings, flush the diagnostic stream.
    virtual ~CNcbiApplicationAPI(void);

    /// Main function (entry point) for the NCBI application.
    ///
    /// You can specify where to write the diagnostics to (EAppDiagStream),
    /// and where to get the configuration file (LoadConfig()) to load
    /// to the application registry (accessible via GetConfig()).
    ///
    /// Throw exception if:
    ///  - not-only instance
    ///  - cannot load explicitly specified config.file
    ///  - SetupDiag() throws an exception
    ///
    /// If application name is not specified a default of "ncbi" is used.
    /// Certain flags such as -logfile, -conffile and -version are special so
    /// AppMain() processes them separately.
    /// @param argc
    ///   Argument count [argc in a regular main(argc, argv)].
    /// @param argv
    ///   Argument vector [argv in a regular main(argc, argv)].
    /// @param envp
    ///   Environment pointer [envp in a regular main(argc, argv, envp)];
    ///   a null pointer (the default) corresponds to the standard system
    ///   array (environ on most Unix platforms).
    /// @param diag
    ///   Specify diagnostic stream.
    /// @param conf
    ///   Specify registry to load, as per LoadConfig().  The default is an
    ///   empty string, requesting automatic detection; it is also possible
    ///   to pass a specific name (optionally qualified) or a NULL pointer
    ///   (disabling application-specific registry lookup altogether).
    /// @param name
    ///   Specify application name, used in diagnostics and automatic 
    ///   registry searching.
    /// @return
    ///   Exit code from Run(). Can also return non-zero value if application
    ///   threw an exception.
    /// @sa
    ///   LoadConfig(), Init(), Run(), Exit()
    int AppMain
    (int                argc,
     const char* const* argv,
     const char* const* envp = 0,
     EAppDiagStream     diag = eDS_Default,
     const char*        conf = NcbiEmptyCStr,
     const string&      name = NcbiEmptyString
     );

#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)
    int AppMain
    (int                  argc,
     const TXChar* const* argv,
     const TXChar* const* envp = 0,
     EAppDiagStream       diag = eDS_Default,
     const TXChar*        conf = NcbiEmptyXCStr,
     const TXString&      name = NcbiEmptyXString
     );
#endif

    /// Initialize the application.
    ///
    /// The default behavior of this is "do nothing". If you have special
    /// initialization logic that needs to be peformed, then you must override
    /// this method with your own logic.
    virtual void Init(void);

    /// Run the application.
    ///
    /// It is defined as a pure virtual method -- so you must(!) supply the
    /// Run() method to implement the application-specific logic.
    /// @return
    ///   Exit code.
    virtual int Run(void) = 0;

    /// Test run the application.
    ///
    /// It is only supposed to test if the Run() is possible,
    /// or makes sense: that is, test all preconditions etc.
    /// @return
    ///   Exit code.
    virtual int DryRun(void);

    /// Cleanup on application exit.
    ///
    /// Perform cleanup before exiting. The default behavior of this is
    /// "do nothing". If you have special cleanup logic that needs to be
    /// performed, then you must override this method with your own logic.
    virtual void Exit(void);

    /// Get the application's cached unprocessed command-line arguments.
    const CNcbiArguments& GetArguments(void) const;

    /// Get parsed command line arguments.
    ///
    /// Get command line arguments parsed according to the arg descriptions
    /// set by SetupArgDescriptions(). Throw exception if no descriptions
    /// have been set.
    /// @return
    ///   The CArgs object containing parsed cmd.-line arguments.
    /// @sa
    ///   SetupArgDescriptions().
    virtual const CArgs& GetArgs(void) const;

    /// Get the application's cached environment.
    const CNcbiEnvironment& GetEnvironment(void) const;

    /// Get a non-const copy of the application's cached environment.
    CNcbiEnvironment& SetEnvironment(void);

    /// Set a specified environment variable by name
    void SetEnvironment(const string& name, const string& value);

    /// Check if the config file has been loaded
    bool HasLoadedConfig(void) const;

    /// Check if the application has finished loading config file
    /// (successfully or not).
    bool FinishedLoadingConfig(void) const;

    /// Get the application's cached configuration parameters (read-only).
    ///
    /// Application also can use protected GetRWConfig() to get read-write
    //  access to the configuration parameters.
    /// @sa 
    ///   GetRWConfig
    const CNcbiRegistry& GetConfig(void) const;
    
    /// @deprecated Please use const version of GetConfig() or protected GetRWConfig()
    //NCBI_DEPRECATED 
    CNcbiRegistry& GetConfig(void);

    /// Get the full path to the configuration file (if any) we ended
    /// up using.
    const string& GetConfigPath(void) const;

    /// Reload the configuration file.  By default, does nothing if
    /// the file has the same size and date as before.
    ///
    /// Note that this may lose other data stored in the registry!
    ///
    /// @param flags
    ///   Controls how aggressively to reload.
    /// @param reg_flags
    ///   Flags to use when parsing the registry; ignored if the registry
    ///   was already cached (as it should normally have been).
    /// @return
    ///   TRUE if a reload actually occurred.
    bool ReloadConfig(CMetaRegistry::TFlags flags
                      = CMetaRegistry::fReloadIfChanged,
                      IRegistry::TFlags reg_flags = IRegistry::fWithNcbirc);

    /// Flush the in-memory diagnostic stream (for "eDS_ToMemory" case only).
    ///
    /// In case of "eDS_ToMemory", the diagnostics is stored in
    /// the internal application memory buffer ("m_DiagStream").
    /// Call this function to dump all the diagnostics to stream "os" and
    /// purge the buffer.
    /// @param  os
    ///   Output stream to dump diagnostics to. If it is NULL, then
    ///   nothing will be written to it (but the buffer will still be purged).
    /// @param  close_diag
    ///   If "close_diag" is TRUE, then also destroy "m_DiagStream".
    /// @return
    ///   Total number of bytes actually written to "os".
    SIZE_TYPE FlushDiag(CNcbiOstream* os, bool close_diag = false);

    /// Get the application's "display" name.
    ///
    /// Get name of this application, suitable for displaying
    /// or for using as the base name for other files.
    /// Will be the 'name' argument of AppMain if given.
    /// Otherwise will be taken from the actual name of the application file
    /// or argv[0].
    const string& GetProgramDisplayName(void) const;

    /// Get the application's executable path.
    ///
    /// The path to executable file of this application. 
    /// Return empty string if not possible to determine this path.
    const string& GetProgramExecutablePath(EFollowLinks follow_links
                                           = eIgnoreLinks)
        const;

    enum EAppNameType {
        eBaseName, ///< per GetProgramDisplayName
        eFullName, ///< per GetProgramExecutablePath(eIgnoreLinks)
        eRealName  ///< per GetProgramExecutablePath(eFollowLinks)
    };
    static string GetAppName(EAppNameType name_type = eBaseName,
                             int argc = 0, const char* const* argv = NULL);

    /// Get the program version information.
    ///
    /// @sa SetVersion, SetFullVersion
    CVersionInfo GetVersion(void) const;

    /// Get the program version information.
    const CVersionAPI& GetFullVersion(void) const;
    
    /// Check if it is a test run.
    bool IsDryRun(void) const;

    /// Setup application specific diagnostic stream.
    ///
    /// Called from SetupDiag when it is passed the eDS_AppSpecific parameter.
    /// Currently, this calls SetupDiag(eDS_ToStderr) to setup diagonistic
    /// stream to the std error channel.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @deprecated
    NCBI_DEPRECATED virtual bool SetupDiag_AppSpecific(void);

    /// Add callback to be executed from CNcbiApplicationAPI destructor.
    /// @sa CNcbiActionGuard
    template<class TFunc> void AddOnExitAction(TFunc func)
    {
        m_OnExitActions.AddAction(func);
    }

protected:
    /// Result of PreparseArgs()
    enum EPreparseArgs {
        ePreparse_Continue,  ///< Continue application execution
        ePreparse_Exit       ///< Exit the application with zero exit code
    };

    /// Check the command line arguments before parsing them.
    /// @sa EPreparseArgs
    virtual EPreparseArgs PreparseArgs(int                argc,
                                       const char* const* argv);

    /// Disable argument descriptions.
    ///
    /// Call with a bit flag set if you do not want std AND user
    /// cmd.line args to be parsed.
    /// Note that by default the parsing of cmd.line args are enabled.
    enum EDisableArgDesc {
        fDisableStdArgs     = 0x01   ///<  (-logfile, -conffile, -version etc)
    };
    typedef int TDisableArgDesc;  ///< Binary OR of "EDisableArgDesc"
    void DisableArgDescriptions(TDisableArgDesc disable = fDisableStdArgs);

    /// Which standard flag's descriptions should not be displayed in
    /// the usage message.
    ///
    /// Do not display descriptions of the standard flags such as the
    ///    -h, -logfile, -conffile, -version
    /// flags in the usage message. Note that you still can pass them in
    /// the command line.
    enum EHideStdArgs {
        fHideLogfile     = 0x01,  ///< Hide log file description
        fHideConffile    = 0x02,  ///< Hide configuration file description
        fHideVersion     = 0x04,  ///< Hide version description
        fHideFullVersion = 0x08,  ///< Hide full version description
        fHideDryRun      = 0x10,  ///< Hide dryrun description
        fHideHelp        = 0x20,  ///< Hide help description
        fHideFullHelp    = 0x40,  ///< Hide full help description
        fHideXmlHelp     = 0x80,  ///< Hide XML help description
        fHideAll         = 0xFF   ///< Hide all standard argument descriptions
    };
    typedef int THideStdArgs;  ///< Binary OR of "EHideStdArgs"

    /// Set the hide mask for the Hide Std Flags.
    void HideStdArgs(THideStdArgs hide_mask);

    /// Flags to adjust standard I/O streams' behaviour.
    enum EStdioSetup {
        fNoSyncWithStdio  = 0x01,
        ///< Turn off synchronizing of "C++" cin/cout/cerr streams with
        ///< their "C" counterparts, possibly making the former not thread-safe.

        fDefault_CinBufferSize  = 0x02,
        ///< Use compiler-specific default of Cin buffer size.
        fBinaryCin   = 0x04,  ///< treat standard  input as binary
        fBinaryCout  = 0x08,  ///< treat standard output as binary

        fDefault_SyncWithStdio  = 0x00, ///< @deprecated @sa fNoSyncWithStdio
    };
    typedef int TStdioSetupFlags;  ///< Binary OR of "EStdioSetup"

    /// Adjust the behavior of standard I/O streams.
    ///
    /// Unless this function is called, the behaviour of "C++" Cin/Cout/Cerr
    /// streams will be the same regardless of the compiler used.
    ///
    /// IMPLEMENTATION NOTE: Do not call this function more than once
    /// and from places other than App's constructor.
    void SetStdioFlags(TStdioSetupFlags stdio_flags);

    /// Set the version number for the program.
    ///
    /// If not set, a default of 0.0.teamcity_build_number is used.
    /// @note
    ///   This function should be used from constructor of CNcbiApplicationAPI
    ///   derived class, otherwise command-like arguments "-version" and 
    ///   "-version-full" will not work as expected.
    /// @sa GetVersion, NCBI_APP_SET_VERSION, NCBI_APP_SET_VERSION_AUTO
    void SetVersion(const CVersionInfo& version);
    void SetVersion(const CVersionInfo& version, const SBuildInfo& build_info);
    NCBI_DEPRECATED void SetVersionByBuild(int major);

    /// Set version data for the program.
    ///
    /// @note
    ///   This function should be used from constructor of CNcbiApplicationAPI
    ///   derived class, otherwise command-like arguments "-version" and 
    ///   "-version-full" will not work as expected.
    /// @sa GetVersion
    void SetFullVersion( CRef<CVersionAPI> version);

    /// Setup the command line argument descriptions.
    ///
    /// Call from the Init() method. The passed "arg_desc" will be owned
    /// by this class, and it will be deleted by ~CNcbiApplicationAPI(),
    /// or if SetupArgDescriptions() is called again.
    virtual void SetupArgDescriptions(CArgDescriptions* arg_desc);

    /// Get argument descriptions (set by SetupArgDescriptions)
    const CArgDescriptions* GetArgDescriptions(void) const;

    /// Setup the application diagnostic stream.
    /// @return
    ///   TRUE if successful,  FALSE otherwise.
    /// @deprecated
    NCBI_DEPRECATED bool SetupDiag(EAppDiagStream diag);

    /// Load settings from the configuration file to the registry.
    ///
    /// This method is called from inside AppMain() to load (add) registry
    /// settings from the configuration file specified as the "conf" arg
    /// passed to AppMain(). The "conf" argument has the following special
    /// meanings:
    ///  - NULL      -- don't try to load an application-specific registry
    ///                 from any file at all.
    ///  - non-empty -- if "conf" contains a path, then try to load from the
    ///                 conf.file of name "conf" (only!). Else - see NOTE.
    ///                 TIP: if the path is not fully qualified then:
    ///                      if it starts from "../" or "./" -- look starting
    ///                      from the current working dir.
    ///  - empty     -- compose conf.file name from the application name
    ///                 plus ".ini". If it does not match an existing
    ///                 file, then try to strip file extensions, e.g. for
    ///                 "my_app.cgi.exe" -- try subsequently:
    ///                   "my_app.cgi.exe.ini", "my_app.cgi.ini", "my_app.ini".
    ///
    /// Regardless, this method normally loads global settings
    /// from .ncbirc or ncbi.ini when reg_flags contains fWithNcbirc
    /// (as it typically does), even if conf is NULL.
    ///
    /// NOTE:
    /// If "conf" arg is empty or non-empty, but without path, then
    /// the Toolkit will try to look for it in several potentially
    /// relevant directories, as described in <corelib/metareg.hpp>.
    ///
    /// Throw an exception if "conf" is non-empty, and cannot open file.
    /// Throw an exception if file exists, but contains invalid entries.
    /// @param reg
    ///   The loaded registry is returned via the reg parameter.
    /// @param conf
    ///   The configuration file to loaded the registry entries from.
    /// @param reg_flags
    ///   Flags for loading the registry
    /// @return
    ///   TRUE only if the file was non-NULL, found and successfully read.
    /// @sa
    ///   CMetaRegistry::GetDefaultSearchPath
    virtual bool LoadConfig(CNcbiRegistry& reg, const string* conf,
                            CNcbiRegistry::TFlags reg_flags);

    /// Load settings from the configuration file to the registry.
    ///
    /// CNcbiApplicationAPI::LoadConfig(reg, conf) just calls
    /// LoadConfig(reg, conf, IRegistry::fWithNcbirc).
    virtual bool LoadConfig(CNcbiRegistry& reg, const string* conf);

    /// Get the application's cached configuration parameters, 
    /// accessible to read-write for an application only.
    /// @sa 
    ///   GetConfig
    CNcbiRegistry& GetRWConfig(void);

    /// Set program's display name.
    ///
    /// Set up application name suitable for display or as a basename for
    /// other files. It can also be set by the user when calling AppMain().
    void SetProgramDisplayName(const string& app_name);

    /// Find the application's executable file.
    ///
    /// Find the path and name of the executable file that this application is
    /// running from. Will be accessible by GetArguments().GetProgramName().
    /// @param argc
    ///   Standard argument count "argc".
    /// @param argv
    ///   Standard argument vector "argv".
    /// @param real_path
    ///   If non-NULL, will get the fully resolved path to the executable.
    /// @return
    ///   Name of application's executable file (may involve symlinks).
    static string FindProgramExecutablePath(int argc, const char* const* argv,
                                            string* real_path = 0);

    /// Method to be called before application start.
    /// Can be used to set DiagContext properties to be printed
    /// in the application start message (e.g. host|host_ip_addr,
    /// client_ip and session_id for CGI applications).
    virtual void AppStart(void);

    /// Method to be called before application exit.
    /// Can be used to set DiagContext properties to be printed
    /// in the application stop message (exit_status, exit_signal,
    /// exit_code).
    virtual void AppStop(int exit_code);

    /// When to return a user-set exit code
    enum EExitMode {
        eNoExits,          ///< never (stick to existing logic)
        eExceptionalExits, ///< when an (uncaught) exception occurs
        eAllExits          ///< always (ignoring Run's return value)
    };
    /// Force the program to return a specific exit code later, either
    /// when it exits due to an exception or unconditionally.  In the
    /// latter case, Run's return value will be ignored.
    void SetExitCode(int exit_code, EExitMode when = eExceptionalExits);

private:
    /// Read standard NCBI application configuration settings.
    ///
    /// [NCBI]:   HeapSizeLimit, CpuTimeLimit
    /// [DEBUG]:  ABORT_ON_THROW, DIAG_POST_LEVEL, MessageFile
    /// @param reg
    ///   Registry to read from. If NULL, use the current registry setting.
    void x_HonorStandardSettings(IRegistry* reg = 0);

    /// Read switches that are stored in m_LogOptions from registry and 
    /// environment
    void x_ReadLogOptions();

    /// Log environment, registry, command arguments, path
    void x_LogOptions(int event);
    
    /// Setup C++ standard I/O streams' behaviour.
    ///
    /// Called from AppMain() to do compiler-specific optimization
    /// for C++ I/O streams. For example, since SUN WorkShop STL stream
    /// library has significant performance loss when sync_with_stdio is
    /// TRUE (default), so we turn it off. Another, for GCC version greater
    /// than 3.00 we forcibly set cin stream buffer size to 4096 bytes -- which
    /// boosts the performance dramatically.
    void x_SetupStdio(void);
    
    void x_AddDefaultArgs(void);

    // Wrappers for parts of AppMain() called with or without try/catch
    // depending on settings.
    void x_TryInit(EAppDiagStream diag, const char* conf);
    void x_TryMain(EAppDiagStream diag,
                   const char*    conf,
                   int*           exit_code,
                   bool*          got_exception);

    static CNcbiApplicationAPI*m_Instance;   ///< Current app. instance
    CRef<CVersionAPI>          m_Version;    ///< Program version
    unique_ptr<CNcbiEnvironment> m_Environ;    ///< Cached application env.
    CRef<CNcbiRegistry>        m_Config;     ///< Guaranteed to be non-NULL
    unique_ptr<CNcbiOstream>     m_DiagStream; ///< Opt., aux., see eDS_ToMemory
    unique_ptr<CNcbiArguments>   m_Arguments;  ///< Command-line arguments
    unique_ptr<CArgDescriptions> m_ArgDesc;    ///< Cmd.-line arg descriptions
    unique_ptr<CArgs>            m_Args;       ///< Parsed cmd.-line args
    TDisableArgDesc            m_DisableArgDesc;  ///< Arg desc. disabled
    THideStdArgs               m_HideArgs;   ///< Std cmd.-line flags to hide
    TStdioSetupFlags           m_StdioFlags; ///< Std C++ I/O adjustments
    char*                      m_CinBuffer;  ///< Cin buffer if changed
    string                     m_ProgramDisplayName;  ///< Display name of app
    string                     m_ExePath;    ///< Program executable path
    string                     m_RealExePath; ///< Symlink-free executable path
    mutable string             m_LogFileName; ///< Log file name
    string                     m_ConfigPath;  ///< Path to .ini file used
    int                        m_ExitCode;    ///< Exit code to force
    EExitMode                  m_ExitCodeCond; ///< When to force it (if ever)
    bool                       m_DryRun;       ///< Dry run
    string                     m_DefaultConfig; ///< conf parameter to AppMain
    bool                       m_ConfigLoaded;  ///< Finished loading config
    const char*                m_LogFile;     ///< Logfile if set in the command line
    int                        m_LogOptions; ///<  logging of env, reg, args, path
    CNcbiActionGuard           m_OnExitActions; ///< Actions executed on app destruction
};

/// Interface for application idler.
class NCBI_XNCBI_EXPORT INcbiIdler {
public:
    virtual ~INcbiIdler(void) {}

    // Perform any actions. Called by RunIdle().
    virtual void Idle(void) = 0;
};


/// Default idler.
class NCBI_XNCBI_EXPORT CDefaultIdler : public INcbiIdler
{
public:
    CDefaultIdler(void) {}
    virtual ~CDefaultIdler(void) {}

    virtual void Idle(void);
};


/// Return currently installed idler or NULL. Update idler ownership
/// according to the flag.
NCBI_XNCBI_EXPORT INcbiIdler* GetIdler(EOwnership ownership = eNoOwnership);

/// Set new idler and ownership.
NCBI_XNCBI_EXPORT void SetIdler(INcbiIdler* idler,
                                EOwnership ownership = eTakeOwnership);

/// Execute currently installed idler if any.
NCBI_XNCBI_EXPORT void RunIdler(void);


/* @} */



/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


inline const CNcbiArguments& CNcbiApplicationAPI::GetArguments(void) const
{
    return *m_Arguments;
}

inline const CNcbiEnvironment& CNcbiApplicationAPI::GetEnvironment(void) const
{
    return *m_Environ;
}

inline CNcbiEnvironment& CNcbiApplicationAPI::SetEnvironment(void)
{
    return *m_Environ;
}

inline const CNcbiRegistry& CNcbiApplicationAPI::GetConfig(void) const
{
    return *m_Config;
}

/// @deprecated
inline CNcbiRegistry& CNcbiApplicationAPI::GetConfig(void)
{
    return *m_Config;
}

inline CNcbiRegistry& CNcbiApplicationAPI::GetRWConfig(void)
{
    return *m_Config;
}

inline const string& CNcbiApplicationAPI::GetConfigPath(void) const
{
    return m_ConfigPath;
}

inline bool CNcbiApplicationAPI::HasLoadedConfig(void) const
{
    return !m_ConfigPath.empty();
}

inline bool CNcbiApplicationAPI::FinishedLoadingConfig(void) const
{
    return m_ConfigLoaded;
}

inline bool CNcbiApplicationAPI::ReloadConfig(CMetaRegistry::TFlags flags, IRegistry::TFlags reg_flags)
{
    return CMetaRegistry::Reload(GetConfigPath(), GetRWConfig(), flags, reg_flags);
}

inline const string& CNcbiApplicationAPI::GetProgramDisplayName(void) const
{
    return m_ProgramDisplayName;
}

inline const string&
CNcbiApplicationAPI::GetProgramExecutablePath(EFollowLinks follow_links) const
{
    return follow_links == eFollowLinks ? m_RealExePath : m_ExePath;
}


inline const CArgDescriptions* CNcbiApplicationAPI::GetArgDescriptions(void) const
{
    return m_ArgDesc.get();
}


inline bool CNcbiApplicationAPI::IsDryRun(void) const
{
    return m_DryRun;
}


#ifndef CORELIB___NCBIAPP__HPP
# define CNcbiApplication CNcbiApplicationAPI
#endif

END_NCBI_SCOPE

#endif  /* CORELIB___NCBIAPP_API__HPP */
