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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <math.h>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/plugin_manager.hpp>
#include <util/random_gen.hpp>

#include <google/protobuf/stubs/common.h>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
#include "shutdown_data.hpp"


USING_NCBI_SCOPE;

const unsigned short    kWorkersMin = 1;
const unsigned short    kWorkersMax = 100;
const unsigned short    kWorkersDefault = 32;
const unsigned short    kHttpPortMin = 1;
const unsigned short    kHttpPortMax = 65534;
const unsigned int      kListenerBacklogMin = 5;
const unsigned int      kListenerBacklogMax = 2048;
const unsigned int      kListenerBacklogDefault = 256;
const unsigned short    kTcpMaxConnMax = 65000;
const unsigned short    kTcpMaxConnMin = 5;
const unsigned short    kTcpMaxConnDefault = 4096;
const unsigned int      kTimeoutMsMin = 0;
const unsigned int      kTimeoutMsMax = UINT_MAX;
const unsigned int      kTimeoutDefault = 30000;
const unsigned int      kMaxRetriesDefault = 1;
const unsigned int      kMaxRetriesMin = 0;
const unsigned int      kMaxRetriesMax = UINT_MAX;
const bool              kDefaultLog = true;
const string            kDefaultRootKeyspace = "sat_info";
const unsigned int      kDefaultExcludeCacheMaxSize = 1000;
const unsigned int      kDefaultExcludeCachePurgePercentage = 20;
const unsigned int      kDefaultExcludeCacheInactivityPurge = 60;
const string            kDefaultAuthToken = "";
const bool              kDefaultAllowIOTest = false;
const unsigned int      kDefaultSlimMaxBlobSize = 10 * 1024;

static const string     kDaemonizeArgName = "daemonize";


// Memorize the configured severity level to check before using ERR_POST.
// Otherwise some expensive operations are executed without a real need.
EDiagSev                g_ConfiguredSeverity = eDiag_Critical;

// Memorize the configured log on/off flag.
// It is used in the context resetter to avoid unnecessary context resets
bool                    g_Log = kDefaultLog;

// Create the shutdown related data. It is used in a few places:
// a URL handler, signal handlers, watchdog handlers
SShutdownData           g_ShutdownData;


CPubseqGatewayApp *     CPubseqGatewayApp::sm_PubseqApp = nullptr;


CPubseqGatewayApp::CPubseqGatewayApp() :
    m_HttpPort(0),
    m_HttpWorkers(kWorkersDefault),
    m_ListenerBacklog(kListenerBacklogDefault),
    m_TcpMaxConn(kTcpMaxConnDefault),
    m_CassConnectionFactory(CCassConnectionFactory::s_Create()),
    m_TimeoutMs(kTimeoutDefault),
    m_MaxRetries(kMaxRetriesDefault),
    m_ExcludeCacheMaxSize(kDefaultExcludeCacheMaxSize),
    m_ExcludeCachePurgePercentage(kDefaultExcludeCachePurgePercentage),
    m_ExcludeCacheInactivityPurge(kDefaultExcludeCacheInactivityPurge),
    m_StartTime(GetFastLocalTime()),
    m_AllowIOTest(kDefaultAllowIOTest),
    m_SlimMaxBlobSize(kDefaultSlimMaxBlobSize),
    m_ExcludeBlobCache(nullptr)
{
    sm_PubseqApp = this;
}


CPubseqGatewayApp::~CPubseqGatewayApp()
{}


void CPubseqGatewayApp::Init(void)
{
    unique_ptr<CArgDescriptions>    argdesc(new CArgDescriptions());

    argdesc->AddFlag(kDaemonizeArgName,
                     "Turn on daemonization of Pubseq Gateway at the start.");

    argdesc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Daemon to service Accession.Version Cache requests");
    SetupArgDescriptions(argdesc.release());

    // Memorize the configured severity
    g_ConfiguredSeverity = GetDiagPostLevel();
}


void CPubseqGatewayApp::ParseArgs(void)
{
    const CArgs &           args = GetArgs();
    const CNcbiRegistry &   registry = GetConfig();

    if (!registry.HasEntry("SERVER", "port"))
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "[SERVER]/port value is not fond in the configuration "
                   "file. The port must be provided to run the server. "
                   "Exiting.");

    m_Si2csiDbFile = registry.GetString("LMDB_CACHE", "dbfile_si2csi", "");
    m_BioseqInfoDbFile = registry.GetString("LMDB_CACHE", "dbfile_bioseq_info", "");
    m_BlobPropDbFile = registry.GetString("LMDB_CACHE", "dbfile_blob_prop", "");
    m_HttpPort = registry.GetInt("SERVER", "port", 0);
    m_HttpWorkers = registry.GetInt("SERVER", "workers",
                                    kWorkersDefault);
    m_ListenerBacklog = registry.GetInt("SERVER", "backlog",
                                        kListenerBacklogDefault);
    m_TcpMaxConn = registry.GetInt("SERVER", "maxconn",
                                   kTcpMaxConnDefault);
    m_TimeoutMs = registry.GetInt("SERVER", "optimeout",
                                  kTimeoutDefault);
    m_MaxRetries = registry.GetInt("SERVER", "maxretries",
                                   kMaxRetriesDefault);
    g_Log = registry.GetBool("SERVER", "log",
                             kDefaultLog);
    m_RootKeyspace = registry.GetString("SERVER", "root_keyspace",
                                        kDefaultRootKeyspace);

    m_ExcludeCacheMaxSize = registry.GetInt("AUTO_EXCLUDE", "max_cache_size",
                                            kDefaultExcludeCacheMaxSize);
    m_ExcludeCachePurgePercentage = registry.GetInt("AUTO_EXCLUDE",
                                                    "purge_percentage",
                                                    kDefaultExcludeCachePurgePercentage);
    m_ExcludeCacheInactivityPurge = registry.GetInt("AUTO_EXCLUDE",
                                                    "inactivity_purge_timeout",
                                                    kDefaultExcludeCacheInactivityPurge);
    m_AllowIOTest = registry.GetBool("DEBUG", "psg_allow_io_test",
                                     kDefaultAllowIOTest);

    m_SlimMaxBlobSize = x_GetDataSize(registry, "SERVER", "slim_max_blob_size",
                                      kDefaultSlimMaxBlobSize);

    try {
        m_AuthToken = registry.GetEncryptedString("ADMIN", "auth_token",
                                                  IRegistry::fPlaintextAllowed);
    } catch (const CRegistryException &  ex) {
        ERR_POST("Decrypting error detected while reading "
                 "[ADMIN]/auth_token value: " << ex.what());

        // Treat the value as a clear text
        m_AuthToken = registry.GetString("ADMIN", "auth_token",
                                         kDefaultAuthToken);
    } catch (...) {
        ERR_POST("Unknown decrypting error detected while reading "
                 "[ADMIN]/auth_token value");

        // Treat the value as a clear text
        m_AuthToken = registry.GetString("ADMIN", "auth_token",
                                         kDefaultAuthToken);
    }

    m_CassConnectionFactory->AppParseArgs(args);
    m_CassConnectionFactory->LoadConfig(registry, "");
    m_CassConnectionFactory->SetLogging(GetDiagPostLevel());

    // It throws an exception in case of inability to start
    x_ValidateArgs();
}


void CPubseqGatewayApp::OpenCache(void)
{
    // NB. It was decided that the configuration may ommit the cache file paths.
    // In this case the server should not use the corresponding cache at all.
    m_LookupCache.reset(new CPubseqGatewayCache(m_BioseqInfoDbFile,
                                                m_Si2csiDbFile,
                                                m_BlobPropDbFile));
    m_LookupCache->Open(m_SatNames);
}


void CPubseqGatewayApp::OpenCass(void)
{
    m_CassConnection = m_CassConnectionFactory->CreateInstance();
    m_CassConnection->Connect();
}


void CPubseqGatewayApp::CloseCass(void)
{
    m_CassConnection = nullptr;
    m_CassConnectionFactory = nullptr;
}


bool CPubseqGatewayApp::SatToSatName(size_t  sat, string &  sat_name)
{
    if (sat < m_SatNames.size()) {
        sat_name = m_SatNames[sat];
        return !sat_name.empty();
    }
    return false;
}


int CPubseqGatewayApp::Run(void)
{
    srand(time(NULL));

    try {
        ParseArgs();
    } catch (const exception &  exc) {
        PSG_CRITICAL(exc.what());
        return 1;
    } catch (...) {
        PSG_CRITICAL("Unknown argument parsing error");
        return 1;
    }

    if (GetArgs()[kDaemonizeArgName]) {
        bool    is_good = CCurrentProcess::Daemonize(kEmptyCStr,
                                                     CCurrentProcess::fDF_KeepCWD);
        if (!is_good)
            NCBI_THROW(CPubseqGatewayException, eDaemonizationFailed,
                       "Error during daemonization");
    }

    try {
        OpenCass();
    } catch (const exception &  exc) {
        PSG_CRITICAL(exc.what());
        return 1;
    } catch (...) {
        PSG_CRITICAL("Unknown opening Cassandra error");
        return 1;
    }

    int     ret = x_PopulateSatToKeyspaceMap();
    if (ret != 0)
        return ret;

    try {
        OpenCache();
    } catch (const exception &  exc) {
        PSG_CRITICAL(exc.what());
        return 1;
    } catch (...) {
        PSG_CRITICAL("Unknown opening LMDB cache error");
        return 1;
    }

    auto purge_size = round(float(m_ExcludeCacheMaxSize) *
                            float(m_ExcludeCachePurgePercentage) / 100.0);
    m_ExcludeBlobCache.reset(
        new CExcludeBlobCache(m_ExcludeCacheInactivityPurge,
                              m_ExcludeCacheMaxSize,
                              m_ExcludeCacheMaxSize - static_cast<size_t>(purge_size)));


    vector<HST::CHttpHandler<CPendingOperation>>    http_handler;
    HST::CHttpGetParser                             get_parser;

    http_handler.emplace_back(
            "/ID/getblob",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnGetBlob(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnGet(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/resolve",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnResolve(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_na",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnGetNA(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/config",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnConfig(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/info",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnInfo(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/status",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnStatus(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/shutdown",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnShutdown(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/favicon.ico",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                // It's a browser, most probably admin request
                resp.Send404("Not Found", "Not found");
                return 0;
            }, &get_parser, nullptr);

    if (m_AllowIOTest) {
        m_IOTestBuffer.reset(new char[kMaxTestIOSize]);
        CRandom     random;
        char *      current = m_IOTestBuffer.get();
        for (size_t  k = 0; k < kMaxTestIOSize; k += 8) {
            Uint8   random_val = random.GetRandUint8();
            memcpy(current, &random_val, 8);
            current += 8;
        }

        http_handler.emplace_back(
                "/TEST/io",
                [this](HST::CHttpRequest &  req,
                       HST::CHttpReply<CPendingOperation> &  resp)->int
                {
                    return OnTestIO(req, resp);
                }, &get_parser, nullptr);
    }

    http_handler.emplace_back(
            "",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnBadURL(req, resp);
            }, &get_parser, nullptr);


    m_TcpDaemon.reset(
            new HST::CHttpDaemon<CPendingOperation>(http_handler, "0.0.0.0",
                                                    m_HttpPort,
                                                    m_HttpWorkers,
                                                    m_ListenerBacklog,
                                                    m_TcpMaxConn));


    m_TcpDaemon->Run([this](TSL::CTcpDaemon<HST::CHttpProto<CPendingOperation>,
                       HST::CHttpConnection<CPendingOperation>,
                       HST::CHttpDaemon<CPendingOperation>> &  tcp_daemon)
            {
                // This lambda is called once per second.
                // Earlier implementations printed counters on stdout.
            });
    CloseCass();
    return 0;
}



CPubseqGatewayApp *  CPubseqGatewayApp::GetInstance(void)
{
    return sm_PubseqApp;
}


CPubseqGatewayErrorCounters &  CPubseqGatewayApp::GetErrorCounters(void)
{
    return m_ErrorCounters;
}


CPubseqGatewayRequestCounters &  CPubseqGatewayApp::GetRequestCounters(void)
{
    return m_RequestCounters;
}


CPubseqGatewayCacheCounters &  CPubseqGatewayApp::GetCacheCounters(void)
{
    return m_CacheCounters;
}


CPubseqGatewayDBCounters &  CPubseqGatewayApp::GetDBCounters(void)
{
    return m_DBCounters;
}


void CPubseqGatewayApp::x_ValidateArgs(void)
{
    if (m_HttpPort < kHttpPortMin || m_HttpPort > kHttpPortMax) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "[SERVER]/port value is out of range. Allowed range: " +
                   NStr::NumericToString(kHttpPortMin) + "..." +
                   NStr::NumericToString(kHttpPortMax) + ". Received: " +
                   NStr::NumericToString(m_HttpPort));
    }

    if (m_Si2csiDbFile.empty()) {
        PSG_WARNING("[LMDB_CACHE]/dbfile_si2csi is not found "
                    "in the ini file. No si2csi cache will be used.");
        // NCBI_THROW(CPubseqGatewayException, eConfigurationError,
        //            "[LMDB_CACHE]/dbfile_si2csi is not found. It must "
        //            "be supplied for the server to start");
    } else if (!CFile(m_Si2csiDbFile).Exists()) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "dbfile_si2csi is not found on the disk. It must "
                   "be supplied for the server to start");
    }

    if (m_BioseqInfoDbFile.empty()) {
        PSG_WARNING("[LMDB_CACHE]/dbfile_bioseq_info is not found "
                    "in the ini file. No bioseq_info cache will be used.");
        // NCBI_THROW(CPubseqGatewayException, eConfigurationError,
        //            "[LMDB_CACHE]/dbfile_bioseq_info is not found. It must "
        //            "be supplied for the server to start");
    } else if (!CFile(m_BioseqInfoDbFile).Exists()) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "dbfile_bioseq_info is not found on the disk. It must "
                   "be supplied for the server to start");
    }

    if (m_BlobPropDbFile.empty()) {
        PSG_WARNING("[LMDB_CACHE]/dbfile_blob_prop is not found "
                    "in the ini file. No blob_prop cache will be used.");
        // NCBI_THROW(CPubseqGatewayException, eConfigurationError,
        //            "[LMDB_CACHE]/dbfile_blob_prop is not found. It must "
        //            "be supplied for the server to start");
    } else if (!CFile(m_BlobPropDbFile).Exists()) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "dbfile_blob_prop is not found on the disk. It must "
                   "be supplied for the server to start");
    }

    if (m_HttpWorkers < kWorkersMin || m_HttpWorkers > kWorkersMax) {
        string  err_msg =
            "The number of HTTP workers is out of range. Allowed "
            "range: " + NStr::NumericToString(kWorkersMin) + "..." +
            NStr::NumericToString(kWorkersMax) + ". Received: " +
            NStr::NumericToString(m_HttpWorkers) + ". Reset to "
            "default: " + NStr::NumericToString(kWorkersDefault);
        PSG_ERROR(err_msg);
        m_HttpWorkers = kWorkersDefault;
    }

    if (m_ListenerBacklog < kListenerBacklogMin ||
        m_ListenerBacklog > kListenerBacklogMax) {
        string  err_msg =
            "The listener backlog is out of range. Allowed "
            "range: " + NStr::NumericToString(kListenerBacklogMin) + "..." +
            NStr::NumericToString(kListenerBacklogMax) + ". Received: " +
            NStr::NumericToString(m_ListenerBacklog) + ". Reset to "
            "default: " + NStr::NumericToString(kListenerBacklogDefault);
        PSG_ERROR(err_msg);
        m_ListenerBacklog = kListenerBacklogDefault;
    }

    if (m_TcpMaxConn < kTcpMaxConnMin || m_TcpMaxConn > kTcpMaxConnMax) {
        string  err_msg =
            "The max number of connections is out of range. Allowed "
            "range: " + NStr::NumericToString(kTcpMaxConnMin) + "..." +
            NStr::NumericToString(kTcpMaxConnMax) + ". Received: " +
            NStr::NumericToString(m_TcpMaxConn) + ". Reset to "
            "default: " + NStr::NumericToString(kTcpMaxConnDefault);
        PSG_ERROR(err_msg);
        m_TcpMaxConn = kTcpMaxConnDefault;
    }

    if (m_TimeoutMs < kTimeoutMsMin || m_TimeoutMs > kTimeoutMsMax) {
        string  err_msg =
            "The operation timeout is out of range. Allowed "
            "range: " + NStr::NumericToString(kTimeoutMsMin) + "..." +
            NStr::NumericToString(kTimeoutMsMax) + ". Received: " +
            NStr::NumericToString(m_TimeoutMs) + ". Reset to "
            "default: " + NStr::NumericToString(kTimeoutDefault);
        PSG_ERROR(err_msg);
        m_TimeoutMs = kTimeoutDefault;
    }

    if (m_MaxRetries < kMaxRetriesMin || m_MaxRetries > kMaxRetriesMax) {
        string  err_msg =
            "The max retries is out of range. Allowed "
            "range: " + NStr::NumericToString(kMaxRetriesMin) + "..." +
            NStr::NumericToString(kMaxRetriesMax) + ". Received: " +
            NStr::NumericToString(m_MaxRetries) + ". Reset to "
            "default: " + NStr::NumericToString(kMaxRetriesDefault);
        PSG_ERROR(err_msg);
        m_MaxRetries = kMaxRetriesDefault;
    }

    if (m_ExcludeCacheMaxSize < 0) {
        string  err_msg =
            "The max exclude cache size must be a positive integer. "
            "Received: " + NStr::NumericToString(m_ExcludeCacheMaxSize) + ". "
            "Reset to 0 (exclude blobs cache is disabled)";
        PSG_ERROR(err_msg);
        m_ExcludeCacheMaxSize = 0;
    }

    if (m_ExcludeCachePurgePercentage < 0 || m_ExcludeCachePurgePercentage > 100) {
        string  err_msg = "The exclude cache purge percentage is out of range. "
            "Allowed: 0...100. Received: " +
            NStr::NumericToString(m_ExcludeCachePurgePercentage) + ". ";
        if (m_ExcludeCacheMaxSize > 0) {
            err_msg += "Reset to " +
                NStr::NumericToString(kDefaultExcludeCachePurgePercentage);
            PSG_ERROR(err_msg);
        } else {
            err_msg += "The provided value has no effect "
                "because the exclude cache is disabled.";
            PSG_WARNING(err_msg);
        }
        m_ExcludeCachePurgePercentage = kDefaultExcludeCachePurgePercentage;
    }

    if (m_ExcludeCacheInactivityPurge <= 0) {
        string  err_msg = "The exclude cache inactivity purge timeout must be "
            "a positive integer greater than zero. Received: " +
            NStr::NumericToString(m_ExcludeCacheInactivityPurge) + ". ";
        if (m_ExcludeCacheMaxSize > 0) {
            err_msg += "Reset to " +
                NStr::NumericToString(kDefaultExcludeCacheInactivityPurge);
            PSG_ERROR(err_msg);
        } else {
            err_msg += "The provided value has no effect "
                "because the exclude cache is disabled.";
            PSG_WARNING(err_msg);
        }
        m_ExcludeCacheInactivityPurge = kDefaultExcludeCacheInactivityPurge;
    }
}


string CPubseqGatewayApp::x_GetCmdLineArguments(void) const
{
    const CNcbiArguments &  arguments = CNcbiApplication::Instance()->
                                                            GetArguments();
    size_t                  args_size = arguments.Size();
    string                  cmdline_args;

    for (size_t index = 0; index < args_size; ++index) {
        if (index != 0)
            cmdline_args += " ";
        cmdline_args += arguments[index];
    }
    return cmdline_args;
}


CRef<CRequestContext> CPubseqGatewayApp::x_CreateRequestContext(
                                                HST::CHttpRequest &  req) const
{
    CRef<CRequestContext>   context;
    if (g_Log) {
        context.Reset(new CRequestContext());
        context->SetRequestID();

        // NCBI SID may come from the header
        string      sid = req.GetHeaderValue("HTTP_NCBI_SID");
        if (!sid.empty())
            context->SetSessionID(sid);
        else
            context->SetSessionID();

        // NCBI PHID may come from the header
        string      phid = req.GetHeaderValue("HTTP_NCBI_PHID");
        if (!phid.empty())
            context->SetHitID(phid);
        else
            context->SetHitID();

        // Client IP may come from the headers
        TNCBI_IPv6Addr  client_address = req.GetClientIP();
        if (!NcbiIsEmptyIPv6(&client_address)) {
            char        buf[256];
            if (NcbiIPv6ToString(buf, sizeof(buf), &client_address) != 0) {
                context->SetClientIP(buf);
            }
        }

        CDiagContext::SetRequestContext(context);
        CDiagContext_Extra  extra = GetDiagContext().PrintRequestStart();

        // This is the URL path
        extra.Print("request_path", req.GetPath());
        req.PrintParams(extra);

        // If extra is not flushed then it picks read-only even though it is
        // done after...
        extra.Flush();

        // Just in case, avoid to have 0
        context->SetRequestStatus(CRequestStatus::e200_Ok);
        context->SetReadOnly(true);
    }
    return context;
}


void CPubseqGatewayApp::x_PrintRequestStop(CRef<CRequestContext>   context,
                                           int  status)
{
    if (context.NotNull()) {
        CDiagContext::SetRequestContext(context);
        context->SetReadOnly(false);
        context->SetRequestStatus(status);
        GetDiagContext().PrintRequestStop();
        context.Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


CPubseqGatewayApp::SRequestParameter
CPubseqGatewayApp::x_GetParam(HST::CHttpRequest &  req,
                              const string &  name) const
{
    SRequestParameter       param;
    const char *            value;
    size_t                  value_size;

    param.m_Found = req.GetParam(name.data(), name.size(),
                                 true, &value, &value_size);
    if (param.m_Found)
        param.m_Value.assign(value, value_size);
    return param;
}


bool CPubseqGatewayApp::x_IsBoolParamValid(const string &  param_name,
                                           const CTempString &  param_value,
                                           string &  err_msg) const
{
    static string   yes = "yes";
    static string   no = "no";

    if (param_value != yes && param_value != no) {
        err_msg = "Malformed '" + param_name + "' parameter. "
                  "Acceptable values are '" + yes + "' and '" + no + "'.";
        return false;
    }
    return true;
}


EOutputFormat
CPubseqGatewayApp::x_GetOutputFormat(const string &  param_name,
                                     const CTempString &  param_value,
                                     string &  err_msg) const
{
    static string   protobuf = "protobuf";
    static string   json = "json";
    static string   native = "native";

    if (param_value == protobuf)
        return eProtobufFormat;
    if (param_value == json)
        return eJsonFormat;
    if (param_value == native)
        return eNativeFormat;

    err_msg = "Malformed '" + param_name + "' parameter. "
              "Acceptable values are '" +
              protobuf + "' and '" +
              json + "' and '" +
              native + "'.";
    return eUnknownFormat;
}


ETSEOption
CPubseqGatewayApp::x_GetTSEOption(const string &  param_name,
                                  const CTempString &  param_value,
                                  string &  err_msg) const
{
    static string   none = "none";
    static string   whole = "whole";
    static string   orig = "orig";
    static string   smart = "smart";
    static string   slim = "slim";

    if (param_value == none)
        return eNoneTSE;
    if (param_value == whole)
        return eWholeTSE;
    if (param_value == orig)
        return eOrigTSE;
    if (param_value == smart)
        return eSmartTSE;
    if (param_value == slim)
        return eSlimTSE;

    err_msg = "Malformed '" + param_name + "' parameter. "
              "Acceptable values are '" +
              none + "', '" +
              whole + "', '" +
              orig + "', '" +
              smart + "' and '" +
              slim + "'.";
    return eUnknownTSE;
}


vector<SBlobId> CPubseqGatewayApp::x_GetExcludeBlobs(const string &  param_name,
                                                     const CTempString &  param_value,
                                                     string &  err_msg) const
{
    vector<SBlobId>             result;
    vector<CTempString>         blob_ids;
    NStr::Split(param_value, ",", blob_ids);

    for (const auto &  item : blob_ids) {
        SBlobId     blob_id(item);
        if (!blob_id.IsValid()) {
            err_msg = "Invalid blob id in the '" + param_name +
                      "' parameter comma separated list. Received: '" +
                      string(item.data(), item.size()) +
                      "'. Expected format 'sat.sat_key' where both "
                      "'sat' and 'sat_key' are integers.";
            result.clear();
            break;
        }
        result.push_back(blob_id);
    }
    return result;
}


bool CPubseqGatewayApp::x_ConvertIntParameter(const string &  param_name,
                                              const CTempString &  param_value,
                                              int &  converted,
                                              string &  err_msg) const
{
    try {
        converted = NStr::StringToInt(param_value);
    } catch (...) {
        err_msg = "Error converting " + param_name + " parameter "
                  "to integer (received value: '" + string(param_value) + "')";
        return false;
    }
    return true;
}


unsigned int
CPubseqGatewayApp::x_GetDataSize(const IRegistry &  reg,
                                 const string &  section,
                                 const string &  entry,
                                 unsigned int  default_val)
{
    CConfig                         conf(reg);
    const CConfig::TParamTree *     param_tree = conf.GetTree();
    const TPluginManagerParamTree * section_tree =
                                        param_tree->FindSubNode(section);

    if (!section_tree)
        return default_val;

    CConfig     c((CConfig::TParamTree*)section_tree, eNoOwnership);
    return c.GetDataSize("psg", entry, CConfig::eErr_NoThrow,
                         default_val);
}


int CPubseqGatewayApp::x_PopulateSatToKeyspaceMap(void)
{
    try {
        string      err_msg;
        if (!FetchSatToKeyspaceMapping(m_RootKeyspace, m_CassConnection,
                                       m_SatNames, eBlobVer2Schema,
                                       m_BioseqKeyspace, eResolverSchema,
                                       m_BioseqNAKeyspaces, eNamedAnnotationsSchema,
                                       err_msg)) {
            PSG_CRITICAL(err_msg);
            return 1;
        }

    } catch (const exception &  exc) {
        PSG_CRITICAL(exc);
        PSG_CRITICAL("Cannot populate the sat to keyspace mapping. "
                     "PSG server cannot start.");
        return 1;
    } catch (...) {
        PSG_CRITICAL("Unknown error while populating the sat to "
                     "keyspace mapping. PSG server cannot start.");
        return 1;
    }

    if (m_BioseqKeyspace.empty()) {
        PSG_CRITICAL("Cannot find the resolver keyspace "
                     "(where SI2CSI and BIOSEQ_INFO tables reside) "
                     "in the " + m_RootKeyspace + ".SAT2KEYSPACE table. "
                     "PSG server cannot start.");
        return 1;
    }

    if (m_SatNames.empty()) {
        PSG_CRITICAL("No sat to keyspace resolutions found in the " +
                     m_RootKeyspace + " keyspace. PSG server cannot start.");
        return 1;
    }

    if (m_BioseqNAKeyspaces.empty()) {
        PSG_CRITICAL("No bioseq named annotation keyspaces found in the " +
                     m_RootKeyspace + " keyspace. PSG server cannot start.");
        return 1;
    }

    return 0;
}


bool CPubseqGatewayApp::x_IsResolutionParamValid(const string &  param_name,
                                                 const CTempString &  param_value,
                                                 string &  err_msg) const
{
    static string   fast = "fast";
    static string   full = "full";

    if (param_value != fast && param_value != full) {
        err_msg = "Malformed '" + param_name + "' parameter. "
                  "Acceptable values are '" + fast + "' and '" + full + "'.";
        return false;
    }
    return true;
}


// Prepares the chunks for the case when it is a client error so only two
// chunks are required:
// - a message chunk
// - a reply completion chunk
void  CPubseqGatewayApp::x_SendMessageAndCompletionChunks(
        HST::CHttpReply<CPendingOperation> &  resp,  const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity)
{
    vector<h2o_iovec_t>     chunks;
    string                  header = GetReplyMessageHeader(message.size(),
                                                           status, code,
                                                           severity);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));

    // Add the error message
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(message.data()), message.size()));

    // Add reply completion
    string  reply_completion = GetReplyCompletionHeader(2);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

    resp.SetContentType(ePSGMime);
    resp.Send(chunks, true);
}


// Sends an unknown satellite error for the case when the satellite is provided
// by the user in the incoming URL. I.e. this error is treated as a client
// error (opposite to a server data inconsistency)
void CPubseqGatewayApp::x_SendUnknownClientSatelliteError(
        HST::CHttpReply<CPendingOperation> &  resp,
        const SBlobId &  blob_id,
        const string &  message)
{
    vector<h2o_iovec_t>     chunks;

    // Add header
    string      header = GetBlobMessageHeader(1, blob_id, message.size(),
                                              CRequestStatus::e404_NotFound,
                                              eUnknownResolvedSatellite,
                                              eDiag_Error);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));

    // Add the error message
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(message.data()), message.size()));

    // Add meta with n_chunks
    string      meta = GetBlobCompletionHeader(1, blob_id, 2);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(meta.data()), meta.size()));

    // Add reply completion
    string  reply_completion = GetReplyCompletionHeader(3);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

    resp.SetContentType(ePSGMime);
    resp.Send(chunks, true);
}


void CPubseqGatewayApp::x_MalformedArguments(
                                HST::CHttpReply<CPendingOperation> &  resp,
                                CRef<CRequestContext> &  context,
                                const string &  err_msg)
{
    m_ErrorCounters.IncMalformedArguments();
    x_SendMessageAndCompletionChunks(resp, err_msg,
                                     CRequestStatus::e400_BadRequest,
                                     eMalformedParameter, eDiag_Error);
    PSG_WARNING(err_msg);
    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
}


int main(int argc, const char* argv[])
{
    srand(time(NULL));
    CThread::InitializeMainThreadId();


    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetAllowedSessionIDFormat(CRequestContext::eSID_Other);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);


    int ret = CPubseqGatewayApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}


void CollectGarbage(void)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    app->GetExcludeBlobCache()->Purge();
}

