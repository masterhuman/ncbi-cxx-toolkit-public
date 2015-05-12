/*$Id$
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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *      Tests for NCBI C Logging API (clog.lib)
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <util/xregexp/regexp_template_tester.hpp>
#include "../ncbi_c_log_p.h"

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;



// Directory to store test templates
const char* kTemplatesDir = "clog-test-templates";


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
public:
    typedef void(*FTestCase)(void);
    void RunTest(CTempString name, FTestCase func);
private:
    unsigned int m_Passed;  // number of passed test cases
    unsigned int m_Failed;  // number of failed test cases
};


void CTestApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test program for the C Logging API");
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    // Reset environment variables that can affect test output
    putenv((char*)"SERVER_PORT=");
    putenv((char*)"HTTP_NCBI_SID=");
    putenv((char*)"HTTP_NCBI_PHID=");
    putenv((char*)"NCBI_LOG_SESSION_ID=");
    putenv((char*)"NCBI_LOG_HIT_ID=");
    putenv((char*)"NCBI_CONFIG__LOG__FILE=");
}


#define GETSUBHIT \
    subhitid = NcbiLog_GetNextSubHitID(); \
    NcbiLog_Info(subhitid); \
    NcbiLog_FreeMemory(subhitid)


// Default start/stop, nothing else.
//
void s_TestCase_EmptyApp(void)
{
    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    NcbiLog_AppStop(0);
}


// Default start/stop + 2 empty requests inside.
//
void s_TestCase_EmptyRequests(void)
{
    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    // request 1
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    NcbiLog_ReqStop(200, 1, 2);
    // request 2
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    NcbiLog_ReqStop(200, 1, 2);
    // stop
    NcbiLog_AppStop(0);
}


// PHID: set global app-wide PHID.
//
void s_TestCase_PHID_App(void)
{
    NcbiLog_AppSetHitID("APP-HIT-ID");
    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    NcbiLog_AppStop(0);
}


// PHID: use autogenerated phid and GetNextSubHitID() on app-wide level
//
void s_TestCase_PHID_App_subhit(void)
{
    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    NcbiLog_GetNextSubHitID();
    NcbiLog_AppStop(0);
}


// PHID: use GetNextSubHitID() for each request
//
void s_TestCase_PHID_Req_subhit_1(void)
{
    char* subhitid;
    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    // request 1
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    GETSUBHIT;
    NcbiLog_ReqStop(200, 1, 2);
    // request 2
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    GETSUBHIT;
    NcbiLog_ReqStop(200, 1, 2);
    // stop
    NcbiLog_AppStop(0);
}


// PHID: use GetNextSubHitID() for app and each request.
// Use implicit request name for one of the requests.
//
void s_TestCase_PHID_Req_subhit_2(void)
{
    char* subhitid;
    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    // app-level sub-hit-id
    GETSUBHIT;
    // request 1
    NcbiLog_SetHitID("REQUEST1");
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    GETSUBHIT;
    NcbiLog_ReqStop(200, 1, 2);
    // app-level in-between requests sub-hit-id
    GETSUBHIT;
    // request 2
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    GETSUBHIT;
    NcbiLog_ReqStop(200, 1, 2);
    // stop
    GETSUBHIT;
    NcbiLog_AppStop(0);
}


// PHID: same as s_TestCase_PHID_Req_subhit_2(), but with PHID set in the environment.
//
void s_TestCase_PHID_Req_subhit_3(void)
{
    char* subhitid;
    putenv((char*)"HTTP_NCBI_PHID=SOME_ENV_PHID");

    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    // app-level sub-hit-id
    GETSUBHIT;
    // request 1
    NcbiLog_SetHitID("REQUEST1");
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    GETSUBHIT;
    NcbiLog_ReqStop(200, 1, 2);
    // app-level in-between requests sub-hit-id
    GETSUBHIT;
    // request 2
    NcbiLog_ReqStart(NULL);
    NcbiLog_ReqRun();
    GETSUBHIT;
    NcbiLog_ReqStop(200, 1, 2);
    // stop
    GETSUBHIT;
    NcbiLog_AppStop(0);

    putenv((char*)"HTTP_NCBI_PHID=");
}


int CTestApplication::Run(void)
{
    m_Passed = 0;
    m_Failed = 0;

    RunTest( "empty-app",         &s_TestCase_EmptyApp );
    RunTest( "empty-requests",    &s_TestCase_EmptyRequests );
    RunTest( "phid-app",          &s_TestCase_PHID_App );
    RunTest( "phid-app-subhit",   &s_TestCase_PHID_App_subhit );
    RunTest( "phid-req-subhit-1", &s_TestCase_PHID_Req_subhit_1 );
    RunTest( "phid-req-subhit-2", &s_TestCase_PHID_Req_subhit_2 );
    RunTest( "phid-req-subhit-3", &s_TestCase_PHID_Req_subhit_3 );

    cout << endl;
    cout << "Passed: " << m_Passed << endl;
    cout << "Failed: " << m_Failed << endl;

    return m_Failed;
}


void CTestApplication::RunTest(CTempString name, FTestCase testcase)
{
    cout << name;

    string basename = "clog-test." + (string)name;
    string log_path = basename + ".out";

    // Redirect stderr to file
    ::fflush(stderr);
    int saved_stderr = ::dup(fileno(stderr));
    if (!::freopen(log_path.c_str(), "w", stderr)) {
        _TROUBLE;
    }

    // Initialize API (single-threaded mode)
    NcbiLogP_ReInit();
    NcbiLog_InitST(GetAppName().c_str());
    // Set CLog to use stderr for output, that will be redirected
    // to the file specified above.
    NcbiLog_SetDestination(eNcbiLog_Stderr);
    NcbiLog_SetPostLevel(eNcbiLog_Info);

    // Default initialization
    NcbiLog_SetHost("TESTHOST");

    // Run specified test case
    testcase();

    // Done logging
    NcbiLog_Destroy();
    
    // Restore original stderr
    ::fflush(stderr);
    if (::dup2(saved_stderr, fileno(stderr)) < 0) {
        _TROUBLE;
    }
    close(saved_stderr);
    clearerr(stderr);

    // Compare output
    string template_path = CDir::ConcatPath(kTemplatesDir, basename + ".tpl");
    try {
        CRegexpTemplateTester tester(CRegexpTemplateTester::fSkipEmptyTemplateLines);
        tester.Compare(log_path, template_path);
        cout << ": OK" << endl;
        m_Passed++;
    }
    catch (CRegexpTemplateTesterException& e) {
        cout << e.what() << endl;
        m_Failed++;
    }
    return;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv, 0, eDS_User /* do not redefine */);
}
