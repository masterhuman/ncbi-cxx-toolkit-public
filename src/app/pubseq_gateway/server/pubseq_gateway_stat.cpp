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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "pubseq_gateway_stat.hpp"
#include "pubseq_gateway_logging.hpp"

USING_NCBI_SCOPE;

static const string     kValue("value");
static const string     kName("name");
static const string     kDescription("description");


CPSGSCounters::CPSGSCounters()
{
    m_Counters[ePSGS_BadUrlPath] =
        new SCounterInfo(
            "BadUrlPathCount", "Unknown URL counter",
            "Number of times clients requested a path "
            "which is not served by the server",
            true, true, false);
    m_Counters[ePSGS_InsufficientArgs] =
        new SCounterInfo(
            "InsufficientArgumentsCount", "Insufficient arguments counter",
            "Number of times clients did not supply all the requiried arguments",
            true, true, false);
    m_Counters[ePSGS_MalformedArgs] =
        new SCounterInfo(
            "MalformedArgumentsCount", "Malformed arguments counter",
            "Number of times clients supplied malformed arguments",
            true, true, false);
    m_Counters[ePSGS_GetBlobNotFound] =
        new SCounterInfo(
            "GetBlobNotFoundCount", "Blob not found counter",
            "Number of times clients requested a blob which was not found",
            true, true, false);
    m_Counters[ePSGS_UnknownError] =
        new SCounterInfo(
            "UnknownErrorCount", "Unknown error counter",
            "Number of times an unknown error has been detected",
            true, true, false);
    m_Counters[ePSGS_ClientSatToSatNameError] =
        new SCounterInfo(
            "ClientSatToSatNameErrorCount",
            "Client provided sat to sat name mapping error counter",
            "Number of times a client provided sat could not be mapped to a sat name",
            true, true, false);
    m_Counters[ePSGS_ServerSatToSatNameError] =
        new SCounterInfo(
            "ServerSatToSatNameErrorCount",
            "Server data sat to sat name mapping error counter",
            "Number of times a server data sat could not be mapped to a sat name",
            true, true, false);
    m_Counters[ePSGS_BlobPropsNotFoundError] =
        new SCounterInfo(
            "BlobPropsNotFoundErrorCount", "Blob properties not found counter",
            "Number of times blob properties were not found",
            true, true, false);
    m_Counters[ePSGS_LMDBError] =
        new SCounterInfo(
            "LMDBErrorCount", "LMDB cache error count",
            "Number of times an error was detected while searching in the LMDB cache",
            true, true, false);
    m_Counters[ePSGS_CassQueryTimeoutError] =
        new SCounterInfo(
            "CassQueryTimeoutErrorCount", "Cassandra query timeout error counter",
            "Number of times a timeout error was detected while executing a Cassandra query",
            true, true, false);
    m_Counters[ePSGS_InvalidId2InfoError] =
        new SCounterInfo(
            "InvalidId2InfoErrorCount", "Invalid bioseq info ID2 field error counter",
            "Number of times a malformed bioseq info ID2 field was detected",
            true, true, false);
    m_Counters[ePSGS_SplitHistoryNotFoundError] =
        new SCounterInfo(
            "SplitHistoryNotFoundErrorCount", "Split history not found error count",
            "Number of times a split history was not found",
            true, true, false);
    m_Counters[ePSGS_MaxHopsExceededError] =
        new SCounterInfo(
            "MaxHopsExceededErrorCount", "Max hops exceeded error count",
            "Number of times the max number of hops was exceeded",
            true, true, false);
    m_Counters[ePSGS_InputSeqIdNotResolved] =
        new SCounterInfo(
            "InputSeqIdNotResolved", "Seq id not resolved counter",
            "Number of times a client provided seq id could not be resolved",
            true, false, false);
    m_Counters[ePSGS_TSEChunkSplitVersionCacheMatched] =
        new SCounterInfo(
            "TSEChunkSplitVersionCacheMatched",
            "Requested TSE chunk split version matched the cached one counter",
            "Number of times a client requested TSE chunk split version "
            "matched the cached version",
            true, false, false);
    m_Counters[ePSGS_TSEChunkSplitVersionCacheNotMatched] =
        new SCounterInfo(
            "TSEChunkSplitVersionCacheNotMatched",
            "Requested TSE chunk split version did not match the cached one counter",
            "Number of times a client requested TSE chunk split version "
            "did not match the cached version",
            true, false, false);
    m_Counters[ePSGS_AdminRequest] =
        new SCounterInfo(
            "AdminRequestCount", "Administrative requests counter",
            "Number of time a client requested administrative functionality",
            true, false, true);
    m_Counters[ePSGS_ResolveRequest] =
        new SCounterInfo(
            "ResolveRequestCount", "Resolve requests counter",
            "Number of times a client requested resolve functionality",
            true, false, true);
    m_Counters[ePSGS_GetBlobBySeqIdRequest] =
        new SCounterInfo(
            "GetBlobBySeqIdRequestCount", "Blob requests (by seq id) counter",
            "Number of times a client requested a blob by seq id",
            true, false, true);
    m_Counters[ePSGS_GetBlobBySatSatKeyRequest] =
        new SCounterInfo(
            "GetBlobBySatSatKeyRequestCount", "Blob requests (by sat and sat key) counter",
            "Number of times a client requested a blob by sat and sat key",
            true, false, true);
    m_Counters[ePSGS_GetNamedAnnotations] =
        new SCounterInfo(
            "GetNamedAnnotationsCount", "Named annotations requests counter",
            "Number of times a client requested named annotations",
            true, false, true);
    m_Counters[ePSGS_AccessionVersionHistory] =
        new SCounterInfo(
            "AccessionVersionHistoryCount", "Accession version history requests counter",
            "Number of times a client requested accession version history",
            true, false, true);
    m_Counters[ePSGS_TestIORequest] =
        new SCounterInfo(
            "TestIORequestCount", "Test input/output requests counter",
            "Number of times a client requested an input/output test",
            true, false, true);
    m_Counters[ePSGS_GetTSEChunk] =
        new SCounterInfo(
            "GetTSEChunkCount", "TSE chunk requests counter",
            "Number of times a client requested a TSE chunk",
            true, false, true);
    m_Counters[ePSGS_HealthRequest] =
        new SCounterInfo(
            "HealthRequestCount", "Health requests counter",
            "Number of times a client requested health or deep-health status",
            true, false, true);
    m_Counters[ePSGS_Si2csiCacheHit] =
        new SCounterInfo(
            "Si2csiCacheHit", "si2csi cache hit counter",
            "Number of times a si2csi LMDB cache lookup found a record",
            true, false, false);
    m_Counters[ePSGS_Si2csiCacheMiss] =
        new SCounterInfo(
            "Si2csiCacheMiss", "si2csi cache miss counter",
            "Number of times a si2csi LMDB cache lookup did not find a record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoCacheHit] =
        new SCounterInfo(
            "BioseqInfoCacheHit", "bioseq info cache hit counter",
            "Number of times a bioseq info LMDB cache lookup found a record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoCacheMiss] =
        new SCounterInfo(
            "BioseqInfoCacheMiss", "bioseq info cache miss counter",
            "Number of times a bioseq info LMDB cache lookup did not find a record",
            true, false, false);
    m_Counters[ePSGS_BlobPropCacheHit] =
        new SCounterInfo(
            "BlobPropCacheHit", "Blob properties cache hit counter",
            "Number of times a blob properties LMDB cache lookup found a record",
            true, false, false);
    m_Counters[ePSGS_BlobPropCacheMiss] =
        new SCounterInfo(
            "BlobPropCacheMiss", "Blob properties cache miss counter",
            "Number of times a blob properties LMDB cache lookup did not find a record",
            true, false, false);
    m_Counters[ePSGS_Si2csiNotFound] =
        new SCounterInfo(
            "Si2csiNotFound", "si2csi not found in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in no records",
            true, false, false);
    m_Counters[ePSGS_Si2csiFoundOne] =
        new SCounterInfo(
            "Si2csiFoundOne", "si2csi found one record in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in exactly one record",
            true, false, false);
    m_Counters[ePSGS_Si2csiFoundMany] =
        new SCounterInfo(
            "Si2csiFoundMany", "si2csi found more than one record in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in more than one record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoNotFound] =
        new SCounterInfo(
            "BioseqInfoNotFound", "bioseq info not found in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in no records",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoFoundOne] =
        new SCounterInfo(
            "BioseqInfoFoundOne", "bioseq info found one record in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in exactly one record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoFoundMany] =
        new SCounterInfo(
            "BioseqInfoFoundMany", "bioseq info found more than one record in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in more than one record",
            true, false, false);
    m_Counters[ePSGS_Si2csiError] =
        new SCounterInfo(
            "Si2csiError", "si2csi Cassandra query execution error counter",
            "Number of time a Cassandra si2csi query resulted in an error",
            true, true, false);
    m_Counters[ePSGS_BioseqInfoError] =
        new SCounterInfo(
            "BioseqInfoError", "bioseq info Cassandra query execution error counter",
            "Number of times a Cassandra bioseq info query resulted in an error",
            true, true, false);
    m_Counters[ePSGS_100] =
        new SCounterInfo(
            "RequestStop100", "Request stop counter with status 100",
            "Number of times a request finished with status 100",
            true, false, false);
    m_Counters[ePSGS_101] =
        new SCounterInfo(
            "RequestStop101", "Request stop counter with status 101",
            "Number of times a request finished with status 101",
            true, false, false);
    m_Counters[ePSGS_200] =
        new SCounterInfo(
            "RequestStop200", "Request stop counter with status 200",
            "Number of times a request finished with status 200",
            true, false, false);
    m_Counters[ePSGS_201] =
        new SCounterInfo(
            "RequestStop201", "Request stop counter with status 201",
            "Number of times a request finished with status 201",
            true, false, false);
    m_Counters[ePSGS_202] =
        new SCounterInfo(
            "RequestStop202", "Request stop counter with status 202",
            "Number of times a request finished with status 202",
            true, false, false);
    m_Counters[ePSGS_203] =
        new SCounterInfo(
            "RequestStop203", "Request stop counter with status 203",
            "Number of times a request finished with status 203",
            true, false, false);
    m_Counters[ePSGS_204] =
        new SCounterInfo(
            "RequestStop204", "Request stop counter with status 204",
            "Number of times a request finished with status 204",
            true, false, false);
    m_Counters[ePSGS_205] =
        new SCounterInfo(
            "RequestStop205", "Request stop counter with status 205",
            "Number of times a request finished with status 205",
            true, false, false);
    m_Counters[ePSGS_206] =
        new SCounterInfo(
            "RequestStop206", "Request stop counter with status 206",
            "Number of times a request finished with status 206",
            true, false, false);
    m_Counters[ePSGS_299] =
        new SCounterInfo(
            "RequestStop299", "Request stop counter with status 299",
            "Number of times a request finished with status 299",
            true, false, false);
    m_Counters[ePSGS_300] =
        new SCounterInfo(
            "RequestStop300", "Request stop counter with status 300",
            "Number of times a request finished with status 300",
            true, false, false);
    m_Counters[ePSGS_301] =
        new SCounterInfo(
            "RequestStop301", "Request stop counter with status 301",
            "Number of times a request finished with status 301",
            true, false, false);
    m_Counters[ePSGS_302] =
        new SCounterInfo(
            "RequestStop302", "Request stop counter with status 302",
            "Number of times a request finished with status 302",
            true, false, false);
    m_Counters[ePSGS_303] =
        new SCounterInfo(
            "RequestStop303", "Request stop counter with status 303",
            "Number of times a request finished with status 303",
            true, false, false);
    m_Counters[ePSGS_304] =
        new SCounterInfo(
            "RequestStop304", "Request stop counter with status 304",
            "Number of times a request finished with status 304",
            true, false, false);
    m_Counters[ePSGS_305] =
        new SCounterInfo(
            "RequestStop305", "Request stop counter with status 305",
            "Number of times a request finished with status 305",
            true, false, false);
    m_Counters[ePSGS_307] =
        new SCounterInfo(
            "RequestStop307", "Request stop counter with status 307",
            "Number of times a request finished with status 307",
            true, false, false);
    m_Counters[ePSGS_400] =
        new SCounterInfo(
            "RequestStop400", "Request stop counter with status 400",
            "Number of times a request finished with status 400",
            true, false, false);
    m_Counters[ePSGS_401] =
        new SCounterInfo(
            "RequestStop401", "Request stop counter with status 401",
            "Number of times a request finished with status 401",
            true, false, false);
    m_Counters[ePSGS_402] =
        new SCounterInfo(
            "RequestStop402", "Request stop counter with status 402",
            "Number of times a request finished with status 402",
            true, false, false);
    m_Counters[ePSGS_403] =
        new SCounterInfo(
            "RequestStop403", "Request stop counter with status 403",
            "Number of times a request finished with status 403",
            true, false, false);
    m_Counters[ePSGS_404] =
        new SCounterInfo(
            "RequestStop404", "Request stop counter with status 404",
            "Number of times a request finished with status 404",
            true, false, false);
    m_Counters[ePSGS_405] =
        new SCounterInfo(
            "RequestStop405", "Request stop counter with status 405",
            "Number of times a request finished with status 405",
            true, false, false);
    m_Counters[ePSGS_406] =
        new SCounterInfo(
            "RequestStop406", "Request stop counter with status 406",
            "Number of times a request finished with status 406",
            true, false, false);
    m_Counters[ePSGS_407] =
        new SCounterInfo(
            "RequestStop407", "Request stop counter with status 407",
            "Number of times a request finished with status 407",
            true, false, false);
    m_Counters[ePSGS_408] =
        new SCounterInfo(
            "RequestStop408", "Request stop counter with status 408",
            "Number of times a request finished with status 408",
            true, false, false);
    m_Counters[ePSGS_409] =
        new SCounterInfo(
            "RequestStop409", "Request stop counter with status 409",
            "Number of times a request finished with status 409",
            true, false, false);
    m_Counters[ePSGS_410] =
        new SCounterInfo(
            "RequestStop410", "Request stop counter with status 410",
            "Number of times a request finished with status 410",
            true, false, false);
    m_Counters[ePSGS_411] =
        new SCounterInfo(
            "RequestStop411", "Request stop counter with status 411",
            "Number of times a request finished with status 411",
            true, false, false);
    m_Counters[ePSGS_412] =
        new SCounterInfo(
            "RequestStop412", "Request stop counter with status 412",
            "Number of times a request finished with status 412",
            true, false, false);
    m_Counters[ePSGS_413] =
        new SCounterInfo(
            "RequestStop413", "Request stop counter with status 413",
            "Number of times a request finished with status 413",
            true, false, false);
    m_Counters[ePSGS_414] =
        new SCounterInfo(
            "RequestStop414", "Request stop counter with status 414",
            "Number of times a request finished with status 414",
            true, false, false);
    m_Counters[ePSGS_415] =
        new SCounterInfo(
            "RequestStop415", "Request stop counter with status 415",
            "Number of times a request finished with status 415",
            true, false, false);
    m_Counters[ePSGS_416] =
        new SCounterInfo(
            "RequestStop416", "Request stop counter with status 416",
            "Number of times a request finished with status 416",
            true, false, false);
    m_Counters[ePSGS_417] =
        new SCounterInfo(
            "RequestStop417", "Request stop counter with status 417",
            "Number of times a request finished with status 417",
            true, false, false);
    m_Counters[ePSGS_422] =
        new SCounterInfo(
            "RequestStop422", "Request stop counter with status 422",
            "Number of times a request finished with status 422",
            true, false, false);
    m_Counters[ePSGS_499] =
        new SCounterInfo(
            "RequestStop499", "Request stop counter with status 499",
            "Number of times a request finished with status 499",
            true, false, false);
    m_Counters[ePSGS_500] =
        new SCounterInfo(
            "RequestStop500", "Request stop counter with status 500",
            "Number of times a request finished with status 500",
            true, false, false);
    m_Counters[ePSGS_501] =
        new SCounterInfo(
            "RequestStop501", "Request stop counter with status 501",
            "Number of times a request finished with status 501",
            true, false, false);
    m_Counters[ePSGS_502] =
        new SCounterInfo(
            "RequestStop502", "Request stop counter with status 502",
            "Number of times a request finished with status 502",
            true, false, false);
    m_Counters[ePSGS_503] =
        new SCounterInfo(
            "RequestStop503", "Request stop counter with status 503",
            "Number of times a request finished with status 503",
            true, false, false);
    m_Counters[ePSGS_504] =
        new SCounterInfo(
            "RequestStop504", "Request stop counter with status 504",
            "Number of times a request finished with status 504",
            true, false, false);
    m_Counters[ePSGS_505] =
        new SCounterInfo(
            "RequestStop505", "Request stop counter with status 505",
            "Number of times a request finished with status 505",
            true, false, false);
    m_Counters[ePSGS_xxx] =
        new SCounterInfo(
            "RequestStopXXX", "Request stop counter with unknown status",
            "Number of times a request finished with unknown status",
            true, false, false);

    // The counters below are for the sake of an identifier, name and
    // description. The name and description can be overwritten by the
    // configuration values

    m_Counters[ePSGS_TotalRequest] =
        new SCounterInfo(
            "TotalRequestCount", "Total number of requests",
            "Total number of requests",
            false, false, false);
    m_Counters[ePSGS_TotalError] =
        new SCounterInfo(
            "TotalErrorCount", "Total number of errors",
            "Total number of errors",
            false, false, false);
    m_Counters[ePSGS_CassandraActiveStatements] =
        new SCounterInfo(
            "CassandraActiveStatementsCount", "Cassandra active statements counter",
            "Number of the currently active Cassandra queries",
            false, false, false);
    m_Counters[ePSGS_NumberOfConnections] =
        new SCounterInfo(
            "NumberOfConnections", "Cassandra connections counter",
            "Number of the connections to Cassandra",
            false, false, false);
    m_Counters[ePSGS_ActiveRequest] =
        new SCounterInfo(
            "ActiveRequestCount", "Active requests counter",
            "Number of the currently active client requests",
            false, false, false);
    m_Counters[ePSGS_SplitInfoCacheSize] =
        new SCounterInfo(
            "SplitInfoCacheSize", "Split info cache size",
            "Number of records in the split info cache",
            false, false, false);
    m_Counters[ePSGS_ShutdownRequested] =
        new SCounterInfo(
            "ShutdownRequested", "Shutdown requested flag",
            "Shutdown requested flag",
            false, false, false);
    m_Counters[ePSGS_GracefulShutdownExpiredInSec] =
        new SCounterInfo(
            "GracefulShutdownExpiredInSec", "Graceful shutdown expiration",
            "Graceful shutdown expiration in seconds from now",
            false, false, false);
}


CPSGSCounters::~CPSGSCounters()
{
    for (auto & item: m_Counters) {
        delete item.second;
    }
}


void CPSGSCounters::Increment(EPSGS_CounterType  counter)
{
    auto    it = m_Counters.find(counter);
    if (it == m_Counters.end()) {
        PSG_ERROR("There is no information about the counter with id " +
                  to_string(counter) + ". Nothing was incremented.");
        return;
    }

    ++(it->second->m_Value);
}


CPSGSCounters::EPSGS_CounterType
CPSGSCounters::StatusToCounterType(int  status)
{
    switch (status) {
        case 100:   return ePSGS_100;
        case 101:   return ePSGS_101;
        case 200:   return ePSGS_200;
        case 201:   return ePSGS_201;
        case 202:   return ePSGS_202;
        case 203:   return ePSGS_203;
        case 204:   return ePSGS_204;
        case 205:   return ePSGS_205;
        case 206:   return ePSGS_206;
        case 299:   return ePSGS_299;
        case 300:   return ePSGS_300;
        case 301:   return ePSGS_301;
        case 302:   return ePSGS_302;
        case 303:   return ePSGS_303;
        case 304:   return ePSGS_304;
        case 305:   return ePSGS_305;
        case 307:   return ePSGS_307;
        case 400:   return ePSGS_400;
        case 401:   return ePSGS_401;
        case 402:   return ePSGS_402;
        case 403:   return ePSGS_403;
        case 404:   return ePSGS_404;
        case 405:   return ePSGS_405;
        case 406:   return ePSGS_406;
        case 407:   return ePSGS_407;
        case 408:   return ePSGS_408;
        case 409:   return ePSGS_409;
        case 410:   return ePSGS_410;
        case 411:   return ePSGS_411;
        case 412:   return ePSGS_412;
        case 413:   return ePSGS_413;
        case 414:   return ePSGS_414;
        case 415:   return ePSGS_415;
        case 416:   return ePSGS_416;
        case 417:   return ePSGS_417;
        case 422:   return ePSGS_422;
        case 499:   return ePSGS_499;
        case 500:   return ePSGS_500;
        case 501:   return ePSGS_501;
        case 502:   return ePSGS_502;
        case 503:   return ePSGS_503;
        case 504:   return ePSGS_504;
        case 505:   return ePSGS_505;
    }
    return ePSGS_xxx;
}


void CPSGSCounters::IncrementRequestStopCounter(int  status)
{
    Increment(StatusToCounterType(status));
}


void CPSGSCounters::UpdateConfiguredNameDescription(
                            const map<string, tuple<string, string>> &  conf)
{
    for (auto const & conf_item : conf) {
        for (auto & counter: m_Counters) {
            if (counter.second->m_Identifier == conf_item.first) {
                counter.second->m_Name = get<0>(conf_item.second);
                counter.second->m_Description = get<1>(conf_item.second);
                break;
            }
        }
    }
}


void CPSGSCounters::PopulateDictionary(CJsonNode &  dict)
{
    uint64_t    err_sum(0);
    uint64_t    req_sum(0);
    uint64_t    value(0);

    for (auto const & item: m_Counters) {
        if (!item.second->m_IsMonotonicCounter)
            continue;

        value = item.second->m_Value;
        if (item.second->m_IsErrorCounter) {
            err_sum += value;
        } else {
            if (item.second->m_IsRequestCounter) {
                req_sum += value;
            }
        }
        AppendValueNode(dict, item.second->m_Identifier,
                        item.second->m_Name, item.second->m_Description,
                        value);
    }

    AppendValueNode(dict,
                    m_Counters[ePSGS_TotalRequest]->m_Identifier,
                    m_Counters[ePSGS_TotalRequest]->m_Name,
                    m_Counters[ePSGS_TotalRequest]->m_Description,
                    req_sum);
    AppendValueNode(dict,
                    m_Counters[ePSGS_TotalError]->m_Identifier,
                    m_Counters[ePSGS_TotalError]->m_Name,
                    m_Counters[ePSGS_TotalError]->m_Description,
                    err_sum);
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict, const string &  id,
                                    const string &  name, const string &  description,
                                    uint64_t  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());

    value_dict.SetInteger(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);
    dict.SetByKey(id, value_dict);
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict, const string &  id,
                                    const string &  name, const string &  description,
                                    bool  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());

    value_dict.SetBoolean(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);
    dict.SetByKey(id, value_dict);

}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict, const string &  id,
                                    const string &  name, const string &  description,
                                    const string &  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());

    value_dict.SetString(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);
    dict.SetByKey(id, value_dict);

}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict,
                                    EPSGS_CounterType  counter_type,
                                    uint64_t  value)
{
    AppendValueNode(dict,
                    m_Counters[counter_type]->m_Identifier,
                    m_Counters[counter_type]->m_Name,
                    m_Counters[counter_type]->m_Description,
                    value);
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict,
                                    EPSGS_CounterType  counter_type,
                                    bool  value)
{
    AppendValueNode(dict,
                    m_Counters[counter_type]->m_Identifier,
                    m_Counters[counter_type]->m_Name,
                    m_Counters[counter_type]->m_Description,
                    value);
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict,
                                    EPSGS_CounterType  counter_type,
                                    const string &  value)
{
    AppendValueNode(dict,
                    m_Counters[counter_type]->m_Identifier,
                    m_Counters[counter_type]->m_Name,
                    m_Counters[counter_type]->m_Description,
                    value);
}

