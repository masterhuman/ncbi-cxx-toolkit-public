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
* Authors:  Vsevolod Sandomirskiy, Denis Vakatov
*
* File Description:
*   CNcbiApplication -- a generic NCBI application class
*   CCgiApplication  -- a NCBI CGI-application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.34  2002/01/10 16:52:20  ivanov
* Changed LoadConfig() -- new method to search the config file
*
* Revision 1.33  2001/08/20 14:58:10  vakatov
* Get rid of some compilation warnings
*
* Revision 1.32  2001/08/10 18:26:07  vakatov
* Allow user to throw "CArgException" (and thus force USAGE printout
* with user-provided explanation) not from any of Init(), Run() or Exit().
*
* Revision 1.31  2001/07/07 01:19:28  juran
* Use "ncbi" for app name on Mac OS (if argv[0] is null).
*
* Revision 1.30  2001/03/26 20:07:40  vakatov
* [NCBI_OS_MAC]  Use argv[0] (if available) as basename for ".args"
*
* Revision 1.29  2001/02/02 16:19:27  vasilche
* Fixed reading program arguments on Mac
*
* Revision 1.28  2001/02/01 19:53:26  vasilche
* Reading program arguments from file moved to CNcbiApplication::AppMain.
*
* Revision 1.27  2000/12/23 05:50:53  vakatov
* AppMain() -- check m_ArgDesc for NULL
*
* Revision 1.26  2000/11/29 17:00:25  vakatov
* Use LOG_POST instead of ERR_POST to print cmd.-line arg usage
*
* Revision 1.25  2000/11/24 23:33:12  vakatov
* CNcbiApplication::  added SetupArgDescriptions() and GetArgs() to
* setup cmd.-line argument description, and then to retrieve their
* values, respectively. Also implements internal error handling and
* printout of USAGE for the described arguments.
*
* Revision 1.24  2000/11/01 20:37:15  vasilche
* Fixed detection of heap objects.
* Removed ECanDelete enum and related constructors.
* Disabled sync_with_stdio ad the beginning of AppMain.
*
* Revision 1.23  2000/06/09 18:41:04  vakatov
* FlushDiag() -- check for empty diag.buffer
*
* Revision 1.22  2000/04/04 22:33:35  vakatov
* Auto-set the tracing and the "abort-on-throw" debugging features
* basing on the application environment and/or registry
*
* Revision 1.21  2000/01/20 17:51:18  vakatov
* Major redesign and expansion of the "CNcbiApplication" class to
*  - embed application arguments   "CNcbiArguments"
*  - embed application environment "CNcbiEnvironment"
*  - allow auto-setup or "by choice" (see enum EAppDiagStream) of diagnostics
*  - allow memory-resided "per application" temp. diagnostic buffer
*  - allow one to specify exact name of the config.-file to load, or to
*    ignore the config.file (via constructor's "conf" arg)
*  - added detailed comments
*
* Revision 1.20  1999/12/29 21:20:18  vakatov
* More intelligent lookup for the default config.file. -- try to strip off
* file extensions if cannot find an exact match;  more comments and tracing
*
* Revision 1.19  1999/11/15 15:54:59  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.18  1999/06/11 20:30:37  vasilche
* We should catch exception by reference, because catching by value
* doesn't preserve comment string.
*
* Revision 1.17  1999/05/04 00:03:12  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.16  1999/04/30 19:21:03  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.15  1999/04/27 14:50:06  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.14  1999/02/22 21:12:38  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.13  1998/12/28 23:29:06  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.12  1998/12/28 15:43:12  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.11  1998/12/14 15:30:07  sandomir
* minor fixes in CNcbiApplication; command handling fixed
*
* Revision 1.10  1998/12/09 22:59:35  lewisg
* use new cgiapp class
*
* Revision 1.7  1998/12/09 16:49:56  sandomir
* CCgiApplication added
*
* Revision 1.4  1998/12/03 21:24:23  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.3  1998/12/01 19:12:08  lewisg
* added CCgiApplication
*
* Revision 1.2  1998/11/05 21:45:14  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 22:10:13  sandomir
* CNcbiApplication added; netest sample updated
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
// CNcbiApplication
//

CNcbiApplication* CNcbiApplication::m_Instance;


CNcbiApplication* CNcbiApplication::Instance(void)
{ 
    return m_Instance; 
}


CNcbiApplication::CNcbiApplication(void)
{
    // Register the app. instance
    if ( m_Instance ) {
        THROW1_TRACE(logic_error, "\
CNcbiApplication::CNcbiApplication() -- cannot create second instance");
    }
    m_Instance = this;

    // Create empty application arguments & name
    m_Arguments.reset(new CNcbiArguments(0,0));

    // Create empty application environment
    m_Environ.reset(new CNcbiEnvironment);

    // Create an empty registry
    m_Config.reset(new CNcbiRegistry);
}


CNcbiApplication::~CNcbiApplication(void)
{
    m_Instance = 0;
    FlushDiag(0, true);
}


void CNcbiApplication::Init(void)
{
    return;
}


void CNcbiApplication::Exit(void)
{
    return;
}


SIZE_TYPE CNcbiApplication::FlushDiag(CNcbiOstream* os, bool close_diag)
{
    // dyn.cast to CNcbiOstrstream
    CNcbiOstrstream* ostr = dynamic_cast<CNcbiOstrstream*>(m_DiagStream.get());
    if ( !ostr ) {
        _ASSERT( !m_DiagStream.get() );
        return 0;
    }

    // dump all content to "os"
    SIZE_TYPE n_write = 0;
    if ( os )
        n_write = ostr->pcount();

    if ( n_write ) {
        os->write(ostr->str(), n_write);
        ostr->rdbuf()->freeze(0);
    }

    // reset output buffer or destroy
    if ( close_diag ) {
        if ( IsDiagStream(m_DiagStream.get()) ) {
            SetDiagStream(0);
        }
        m_DiagStream.reset(0);
    } else {
        ostr->rdbuf()->SEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
    }

    // return # of bytes dumped to "os"
    return (os  &&  os->good()) ? n_write : 0;
}


int CNcbiApplication::AppMain
(int                argc,
 const char* const* argv,
 const char* const* envp,
 EAppDiagStream     diag,
 const char*        conf,
 const string&      name)
{
    // SUN WorkShop STL stream library has significant performance loss when
    // sync_with_stdio is true (default)
    // So we turn off sync_with_stdio here:
    IOS_BASE::sync_with_stdio(false);

    // We do not know standard way of passing arguments to C++ program on Mac,
    // so we will read arguments from special file having extension ".args"
    // and name equal to name of program (name argument of AppMain).
#if defined(NCBI_OS_MAC)
#  define MAX_ARGC 256
#  define MAX_ARG_LEN 1024
    if (argc <= 1) {
        string fileName = name;
        if (fileName.empty()) {
            if (argc > 0  &&  argv[0] != NULL  &&  *argv[0] != '\0') {
                fileName = argv[0];
            } else {
                fileName = "ncbi";
            }
        }
        string argsName = fileName + ".args";

        CNcbiIfstream in(argsName.c_str());
        if ( in ) {
            int c = 1;
            const char** v = new const char*[MAX_ARGC];
            v[0] = strdup(fileName.c_str()); // program name
            char arg[MAX_ARG_LEN];
            while (in.getline(arg, sizeof(arg))  ||  in.gcount()) {
                if ( in.eof() ) {
                    ERR_POST(Warning << argsName << ", line " << c << ": " <<
                             "unfinished last line");
                } else if ( in.fail() ) {
                    ERR_POST(Fatal << argsName << ", line " << c << ": " <<
                             "too long argument: " << arg);
                }
                if (c >= MAX_ARGC) {
                    ERR_POST(Fatal << argsName << ", line " << c << ": " <<
                             "too many arguments");
                }
                v[c++] = strdup(arg);
            }
            argc = c;
            argv = v;
        }
        else {
            ERR_POST(argsName << ": file not found");
        }
    }
#endif

    // Reset command-line args and application name
    m_Arguments->Reset(argc, argv, name);

    // Reset application environment
    m_Environ->Reset(envp);

    // Setup some debugging features
    if ( !m_Environ->Get(DIAG_TRACE).empty() ) {
        SetDiagTrace(eDT_Enable, eDT_Enable);
    }
    if ( !m_Environ->Get(ABORT_ON_THROW).empty() ) {
        SetThrowTraceAbort(true);
    }

    // Clear registry content
    m_Config->Clear();

    // Setup for diagnostics
    try {
        if ( !SetupDiag(diag) ) {
            ERR_POST("CNcbiApplication::SetupDiag() returned FALSE");
        }
    } catch (exception& e) {
        ERR_POST("CNcbiApplication::SetupDiag() failed: " << e.what());
        throw runtime_error("CNcbiApplication::SetupDiag() failed");
    }

    // Load registry from the config file
    try {
        if ( conf ) {
            string x_conf(conf);
            LoadConfig(*m_Config, &x_conf);
        } else {
            LoadConfig(*m_Config, 0);
        }
    } catch (exception& e) {
        ERR_POST("CNcbiApplication::LoadConfig() failed: " << e.what());
        throw runtime_error("CNcbiApplication::LoadConfig() failed");
    }

    // Setup some debugging features
    if ( !m_Config->Get("DEBUG", DIAG_TRACE).empty() ) {
        SetDiagTrace(eDT_Enable, eDT_Enable);
    }
    if ( !m_Config->Get("DEBUG", ABORT_ON_THROW).empty() ) {
        SetThrowTraceAbort(true);
    }

    // Call:  Init() + Run() + Exit()
    int exit_code = 1;

    try {
        // Init application
        try {
            Init();
        }
        catch (CArgHelpException&) {
            // Print USAGE
            string str;
            LOG_POST(string(72, '='));
            LOG_POST(m_ArgDesc->PrintUsage(str));
            exit_code = 0;
        }
        catch (CArgException&) {
            throw;
        }
        catch (exception& e) {
            ERR_POST("CNcbiApplication::Init() failed: " << e.what());
            exit_code = -2;
        }

        // Run application
        if (exit_code == 1) {
            try {
                exit_code = Run();
            }
            catch (CArgException&) {
                throw;
            }
            catch (exception& e) {
                ERR_POST("CNcbiApplication::Run() failed: " << e.what());
                exit_code = -3;
            }
        }

        // Close application
        try {
            Exit();
        }
        catch (CArgException&) {
            throw;
        }
        catch (exception& e) {
            ERR_POST("CNcbiApplication::Exit() failed: " << e.what());
        }
    }
    catch (CArgException& e) {
        // Print USAGE and the exception error message
        string str;
        LOG_POST(string(72, '='));
        if ( m_ArgDesc.get() ) {
            LOG_POST(m_ArgDesc->PrintUsage(str) << string(72, '='));
        }
        LOG_POST(" ERROR:  " << e.what());
        exit_code = -1;
    }

    // Exit
    return exit_code;
}


static void s_DiagToStdlog_Cleanup(void* data)
{  // SetupDiag(eDS_ToStdlog)
    CNcbiOfstream* os_log = static_cast<CNcbiOfstream*>(data);
    delete os_log;
}


void CNcbiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    m_ArgDesc.reset(arg_desc);

    if ( arg_desc ) {
        m_Args.reset(arg_desc->CreateArgs(GetArguments()));
    } else {
        m_Args.reset();
    }
}


bool CNcbiApplication::SetupDiag(EAppDiagStream diag)
{
    // Setup diagnostic stream
    switch ( diag ) {
    case eDS_ToStdout: {
        SetDiagStream(&NcbiCout);
        break;
    }
    case eDS_ToStderr: {
        SetDiagStream(&NcbiCerr);
        break;
    }
    case eDS_ToStdlog: {
        // open log.file
        string log = m_Arguments->GetProgramName() + ".log";
        auto_ptr<CNcbiOfstream> os(new CNcbiOfstream(log.c_str()));
        if ( !os->good() ) {
            _TRACE("CNcbiApplication() -- cannot open log file: " << log);
            return false;
        }
        _TRACE("CNcbiApplication() -- opened log file: " << log);

        // (re)direct the global diagnostics to the log.file
        CNcbiOfstream* os_log = os.release();
        SetDiagStream(os_log, true, s_DiagToStdlog_Cleanup, (void*) os_log);
        break;
    }
    case eDS_ToMemory: {
        // direct global diagnostics to the memory-resident output stream
        if ( !m_DiagStream.get() ) {
            m_DiagStream.reset(new CNcbiOstrstream);
        }
        SetDiagStream(m_DiagStream.get());
        break;
    }
    case eDS_Disable: {
        SetDiagStream(0);
        break;
    }
    case eDS_User: {
        // dont change current diag.stream
        break;
    }
    case eDS_AppSpecific: {
        return SetupDiag_AppSpecific();
    }
    case eDS_Default: {
        if ( !IsSetDiagHandler() ) {
            return CNcbiApplication::SetupDiag(eDS_AppSpecific);
        }
        // else eDS_User -- dont change current diag.stream
        break;
    }
    default: {
        _ASSERT(0);
        break;
    }
    } // switch ( diag )

    return true;
}


bool CNcbiApplication::SetupDiag_AppSpecific(void)
{
    return SetupDiag(eDS_ToStderr);
}


// for the exclusive use by CNcbiApplication::LoadConfig()
static bool s_LoadConfig(CNcbiRegistry& reg, const string& conf)
{
    CNcbiIfstream is(conf.c_str());
    if ( !is.good() ) {
        _TRACE("CNcbiApplication::LoadConfig() -- cannot open registry file: "
               << conf);
        return false;
    }

    _TRACE("CNcbiApplication::LoadConfig() -- reading registry file: " <<conf);
    reg.Read(is);
    return true;
}


// for the exclusive use by CNcbiApplication::LoadConfig()
static bool s_LoadConfigTryPath(CNcbiRegistry& reg, const string& path, 
                                const string& conf, const string& basename)
{
    if ( !conf.empty() ) {
        return s_LoadConfig(reg, path + conf);
    }

    // try to derive conf.file name from the application name (for empty"conf")
    string fileName = basename;

    for (;;) {
        // try the next variant -- with added ".ini" file extension
        fileName += ".ini";
        
        // current directory
        if ( s_LoadConfig(reg, path + fileName) )
            return true;  // success!

        // strip ".ini" file extension (the one added above) 
        _ASSERT( fileName.length() > 4 );
        fileName.resize(fileName.length() - 4);

        // strip next file extension, if any left
        SIZE_TYPE dot_pos = fileName.find_last_of(".");
        if ( dot_pos == NPOS ) {
            break;
        }
        fileName.resize(dot_pos);
    }
    return false;
}


bool CNcbiApplication::LoadConfig(CNcbiRegistry& reg, const string* conf)
{
    // don't load at all
    if ( !conf ) {
        return false;
    }

    string x_conf;
    string cnf = CDirEntry::ConvertToOSPath(*conf);

    // load from the specified file name only (with path)
    if (cnf.find_first_of("/\\:") != NPOS) {
        // detect if it is a absolute path
        if ( CDirEntry::IsAbsolutePath(cnf) ) {
            // absolute path
            x_conf = cnf;
        } else {
            // path is relative to the program location
            x_conf = CDirEntry::ConcatPath(m_Arguments->GetProgramDirname(), cnf);
        }
        // do load
        x_conf = NStr::TruncateSpaces(x_conf);
        if ( !s_LoadConfig(reg, x_conf) ) {
            THROW1_TRACE(runtime_error, "\
CNcbiApplication::LoadConfig() -- cannot open registry file: " + x_conf);
        }
        return true;
    }

    string basename = m_Arguments->GetProgramBasename();

    // current (working) directory
    if ( s_LoadConfigTryPath(reg, kEmptyStr, cnf, basename) ) {
        return true;
    }
    // path from environment variable "NCBI"
    char *ptr = 0;
    if ( ptr = getenv("NCBI") ) {
        if ( s_LoadConfigTryPath(reg, CDirEntry::AddTrailingPathSeparator(ptr), 
                                 cnf, basename) ) {
            return true;
        }
    }
    // home directory
    if ( s_LoadConfigTryPath(reg, CDir::GetHome(), cnf, basename) ) {
        return true;
    }
    // program directory
    string dirname = m_Arguments->GetProgramDirname();
    if (!dirname.empty()  &&
        s_LoadConfigTryPath(reg, dirname, cnf, basename)) {
        return true;
    }

    ERR_POST(Warning << 
             "CNcbiApplication::LoadConfig() -- cannot find registry "
             "file for application: " << m_Arguments->GetProgramBasename());

    return false;
}


END_NCBI_SCOPE
