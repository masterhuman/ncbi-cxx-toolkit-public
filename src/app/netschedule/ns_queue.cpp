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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule queue structure and parameters
 */
#include <ncbi_pch.hpp>

#include <unistd.h>

#include "ns_queue.hpp"
#include "ns_queue_param_accessor.hpp"
#include "background_host.hpp"
#include "ns_util.hpp"
#include "ns_server.hpp"
#include "ns_precise_time.hpp"
#include "ns_rollback.hpp"
#include "queue_database.hpp"
#include "ns_handler.hpp"
#include "ns_ini_params.hpp"
#include "ns_perf_logging.hpp"
#include "ns_restore_state.hpp"
#include "ns_db_dump.hpp"

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/request_ctx.hpp>
#include <util/qparse/query_parse.hpp>
#include <util/qparse/query_exec.hpp>
#include <util/qparse/query_exec_bv.hpp>
#include <util/bitset/bmalgo.h>


BEGIN_NCBI_SCOPE


// Used together with m_SavedId. m_SavedId is saved in a DB and is used
// as to start from value for the restarted neschedule.
// s_ReserveDelta value is used to avoid to often DB updates
static const unsigned int       s_ReserveDelta = 10000;


CQueue::CQueue(const string &        queue_name,
               TQueueKind            queue_kind,
               CNetScheduleServer *  server,
               CQueueDataBase &      qdb) :
    m_Server(server),
    m_QueueDB(qdb),
    m_RunTimeLine(NULL),
    m_QueueName(queue_name),
    m_Kind(queue_kind),

    m_LastId(0),
    m_SavedId(s_ReserveDelta),

    m_JobsToDeleteOps(0),
    m_ReadJobsOps(0),

    m_Timeout(default_timeout),
    m_RunTimeout(default_run_timeout),
    m_ReadTimeout(default_read_timeout),
    m_FailedRetries(default_failed_retries),
    m_ReadFailedRetries(default_failed_retries),  // See CXX-5161, the same
                                                  // default as for
                                                  // failed_retries
    m_MaxJobsPerClient(default_max_jobs_per_client),
    m_BlacklistTime(default_blacklist_time),
    m_ReadBlacklistTime(default_blacklist_time),  // See CXX-4993, the same
                                                  // default as for
                                                  // blacklist_time
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize),
    m_WNodeTimeout(default_wnode_timeout),
    m_ReaderTimeout(default_reader_timeout),
    m_PendingTimeout(default_pending_timeout),
    m_KeyGenerator(server->GetHost(), server->GetPort(), queue_name),
    m_Log(server->IsLog()),
    m_LogBatchEachJob(server->IsLogBatchEachJob()),
    m_RefuseSubmits(false),
    m_StatisticsCounters(CStatisticsCounters::eQueueCounters),
    m_StatisticsCountersLastPrinted(CStatisticsCounters::eQueueCounters),
    m_StatisticsCountersLastPrintedTimestamp(0.0),
    m_NotificationsList(qdb, server->GetNodeID(), queue_name),
    m_NotifHifreqInterval(default_notif_hifreq_interval), // 0.1 sec
    m_NotifHifreqPeriod(default_notif_hifreq_period),
    m_NotifLofreqMult(default_notif_lofreq_mult),
    m_DumpBufferSize(default_dump_buffer_size),
    m_DumpClientBufferSize(default_dump_client_buffer_size),
    m_DumpAffBufferSize(default_dump_aff_buffer_size),
    m_DumpGroupBufferSize(default_dump_group_buffer_size),
    m_ScrambleJobKeys(default_scramble_job_keys),
    m_PauseStatus(eNoPause),
    m_ClientRegistryTimeoutWorkerNode(
                                default_client_registry_timeout_worker_node),
    m_ClientRegistryMinWorkerNodes(default_client_registry_min_worker_nodes),
    m_ClientRegistryTimeoutAdmin(default_client_registry_timeout_admin),
    m_ClientRegistryMinAdmins(default_client_registry_min_admins),
    m_ClientRegistryTimeoutSubmitter(default_client_registry_timeout_submitter),
    m_ClientRegistryMinSubmitters(default_client_registry_min_submitters),
    m_ClientRegistryTimeoutReader(default_client_registry_timeout_reader),
    m_ClientRegistryMinReaders(default_client_registry_min_readers),
    m_ClientRegistryTimeoutUnknown(default_client_registry_timeout_unknown),
    m_ClientRegistryMinUnknowns(default_client_registry_min_unknowns),
    m_ShouldPerfLogTransitions(false)
{
    _ASSERT(!queue_name.empty());
    m_ClientsRegistry.SetRegistries(&m_AffinityRegistry,
                                    &m_NotificationsList);

    m_StatesForRead.push_back(CNetScheduleAPI::eDone);
    m_StatesForRead.push_back(CNetScheduleAPI::eFailed);
    m_StatesForRead.push_back(CNetScheduleAPI::eCanceled);
}


CQueue::~CQueue()
{
    delete m_RunTimeLine;
}


void CQueue::Attach(void)
{
    // Here we have a db, so we can read the counter value we should start from
    m_LastId = m_Server->GetJobsStartID(m_QueueName);
    m_SavedId = m_LastId + s_ReserveDelta;
    if (m_SavedId < m_LastId) {
        // Overflow
        m_LastId = 0;
        m_SavedId = s_ReserveDelta;
    }
    m_Server->SetJobsStartID(m_QueueName, m_SavedId);
}


void CQueue::SetParameters(const SQueueParameters &  params)
{
    CFastMutexGuard     guard(m_ParamLock);

    m_Timeout    = params.timeout;
    m_RunTimeout = params.run_timeout;
    if (!m_RunTimeLine) {
        // One time only. Precision can not be reset.
        CNSPreciseTime  precision = params.CalculateRuntimePrecision();
        unsigned int    interval_sec = precision.Sec();
        if (interval_sec < 1)
            interval_sec = 1;
        m_RunTimeLine = new CJobTimeLine(interval_sec, 0);
    }

    m_ReadTimeout           = params.read_timeout;
    m_FailedRetries         = params.failed_retries;
    m_ReadFailedRetries     = params.read_failed_retries;
    m_MaxJobsPerClient      = params.max_jobs_per_client;
    m_BlacklistTime         = params.blacklist_time;
    m_ReadBlacklistTime     = params.read_blacklist_time;
    m_MaxInputSize          = params.max_input_size;
    m_MaxOutputSize         = params.max_output_size;
    m_WNodeTimeout          = params.wnode_timeout;
    m_ReaderTimeout         = params.reader_timeout;
    m_PendingTimeout        = params.pending_timeout;
    m_MaxPendingWaitTimeout = CNSPreciseTime(params.max_pending_wait_timeout);
    m_MaxPendingReadWaitTimeout =
                        CNSPreciseTime(params.max_pending_read_wait_timeout);
    m_NotifHifreqInterval   = params.notif_hifreq_interval;
    m_NotifHifreqPeriod     = params.notif_hifreq_period;
    m_NotifLofreqMult       = params.notif_lofreq_mult;
    m_HandicapTimeout       = CNSPreciseTime(params.notif_handicap);
    m_DumpBufferSize        = params.dump_buffer_size;
    m_DumpClientBufferSize  = params.dump_client_buffer_size;
    m_DumpAffBufferSize     = params.dump_aff_buffer_size;
    m_DumpGroupBufferSize   = params.dump_group_buffer_size;
    m_ScrambleJobKeys       = params.scramble_job_keys;
    m_LinkedSections        = params.linked_sections;

    m_ClientRegistryTimeoutWorkerNode =
                            params.client_registry_timeout_worker_node;
    m_ClientRegistryMinWorkerNodes = params.client_registry_min_worker_nodes;
    m_ClientRegistryTimeoutAdmin = params.client_registry_timeout_admin;
    m_ClientRegistryMinAdmins = params.client_registry_min_admins;
    m_ClientRegistryTimeoutSubmitter = params.client_registry_timeout_submitter;
    m_ClientRegistryMinSubmitters = params.client_registry_min_submitters;
    m_ClientRegistryTimeoutReader = params.client_registry_timeout_reader;
    m_ClientRegistryMinReaders = params.client_registry_min_readers;
    m_ClientRegistryTimeoutUnknown = params.client_registry_timeout_unknown;
    m_ClientRegistryMinUnknowns = params.client_registry_min_unknowns;

    m_ClientsRegistry.SetBlacklistTimeouts(m_BlacklistTime,
                                           m_ReadBlacklistTime);

    // program version control
    m_ProgramVersionList.Clear();
    if (!params.program_name.empty()) {
        m_ProgramVersionList.AddClientInfo(params.program_name);
    }
    m_SubmHosts.SetHosts(params.subm_hosts);
    m_WnodeHosts.SetHosts(params.wnode_hosts);
    m_ReaderHosts.SetHosts(params.reader_hosts);

    UpdatePerfLoggingSettings(params.qclass);
}


void CQueue::UpdatePerfLoggingSettings(const string &  qclass)
{
    m_ShouldPerfLogTransitions = m_Server->ShouldPerfLogTransitions(m_QueueName,
                                                                    qclass);
}


CQueue::TParameterList CQueue::GetParameters() const
{
    TParameterList          parameters;
    CQueueParamAccessor     qp(*this);
    unsigned                nParams = qp.GetNumParams();

    for (unsigned n = 0; n < nParams; ++n) {
        parameters.push_back(
            pair<string, string>(qp.GetParamName(n), qp.GetParamValue(n)));
    }
    return parameters;
}


void CQueue::GetMaxIOSizesAndLinkedSections(
                unsigned int &  max_input_size,
                unsigned int &  max_output_size,
                map< string, map<string, string> > & linked_sections) const
{
    CQueueParamAccessor     qp(*this);

    max_input_size = qp.GetMaxInputSize();
    max_output_size = qp.GetMaxOutputSize();
    GetLinkedSections(linked_sections);
}


void
CQueue::GetLinkedSections(map< string,
                               map<string, string> > &  linked_sections) const
{
    for (map<string, string>::const_iterator  k = m_LinkedSections.begin();
         k != m_LinkedSections.end(); ++k) {
        map< string, string >   values = m_QueueDB.GetLinkedSection(k->second);

        if (!values.empty())
            linked_sections[k->first] = values;
    }
}


// It is called only if there was no job for reading
bool CQueue::x_NoMoreReadJobs(const CNSClientId  &  client,
                              const TNSBitVector &  aff_ids,
                              bool                  reader_affinity,
                              bool                  any_affinity,
                              bool                  exclusive_new_affinity,
                              const TNSBitVector &  group_ids,
                              bool                  affinity_may_change,
                              bool                  group_may_change)
{
    // This certain condition guarantees that there will be no job given
    if (!reader_affinity &&
        !aff_ids.any() &&
        !exclusive_new_affinity &&
        !any_affinity)
        return true;

    // Used only in the GetJobForReadingOrWait().
    // Operation lock has to be already taken.
    // Provides true if there are no more jobs for reading.
    vector<CNetScheduleAPI::EJobStatus>     from_state;
    TNSBitVector                            pending_running_jobs;
    TNSBitVector                            other_jobs;
    string                                  scope = client.GetScope();

    from_state.push_back(CNetScheduleAPI::ePending);
    from_state.push_back(CNetScheduleAPI::eRunning);
    m_StatusTracker.GetJobs(from_state, pending_running_jobs);

    m_StatusTracker.GetJobs(m_StatesForRead, other_jobs);

    // Remove those which have been read or in process of reading. This cannot
    // affect the pending and running jobs
    other_jobs -= m_ReadJobs;
    // Add those which are in a process of reading.
    // This needs to be done after '- m_ReadJobs' because that vector holds
    // both jobs which have been read and jobs which are in a process of
    // reading. When calculating 'no_more_jobs' the only already read jobs must
    // be excluded.
    TNSBitVector        reading_jobs;
    m_StatusTracker.GetJobs(CNetScheduleAPI::eReading, reading_jobs);

    // Apply scope limitations to all participant jobs
    if (scope.empty() || scope == kNoScopeOnly) {
        // Both these cases should consider only the non-scope jobs
        TNSBitVector        all_jobs_in_scopes(bm::BM_GAP);
        all_jobs_in_scopes = m_ScopeRegistry.GetAllJobsInScopes();
        pending_running_jobs -= all_jobs_in_scopes;
        other_jobs -= all_jobs_in_scopes;
        reading_jobs -= all_jobs_in_scopes;
    } else {
        // Consider only the jobs in the particular scope
        TNSBitVector        scope_jobs(bm::BM_GAP);
        scope_jobs = m_ScopeRegistry.GetJobs(scope);
        pending_running_jobs &= scope_jobs;
        other_jobs &= scope_jobs;
        reading_jobs &= scope_jobs;
    }

    if (group_ids.any()) {
        // The pending and running jobs may change their group and or affinity
        // later via the RESCHEDULE command
        if (!group_may_change)
            m_GroupRegistry.RestrictByGroup(group_ids, pending_running_jobs);

        // The job group cannot be changed for the other job states
        other_jobs |= reading_jobs;
        m_GroupRegistry.RestrictByGroup(group_ids, other_jobs);
    } else
        other_jobs |= reading_jobs;

    TNSBitVector    candidates = pending_running_jobs | other_jobs;

    if (!candidates.any())
        return true;


    // Deal with affinities
    // The weakest condition is if any affinity is suitable
    if (any_affinity)
        return !candidates.any();

    TNSBitVector        suitable_affinities;
    TNSBitVector        all_aff;
    TNSBitVector        all_pref_affs;
    TNSBitVector        all_aff_jobs;           // All jobs with an affinity
    TNSBitVector        no_aff_jobs;            // Jobs without any affinity

    all_aff = m_AffinityRegistry.GetRegisteredAffinities();
    all_pref_affs = m_ClientsRegistry.GetAllPreferredAffinities(eRead);
    all_aff_jobs = m_AffinityRegistry.GetJobsWithAffinities(all_aff);
    no_aff_jobs = candidates - all_aff_jobs;
    if (exclusive_new_affinity && no_aff_jobs.any())
        return false;

    if (exclusive_new_affinity)
        suitable_affinities = all_aff - all_pref_affs;
    if (reader_affinity)
        suitable_affinities |= m_ClientsRegistry.
                                    GetPreferredAffinities(client, eRead);
    suitable_affinities |= aff_ids;

    TNSBitVector    suitable_aff_jobs =
                        m_AffinityRegistry.GetJobsWithAffinities(
                                                        suitable_affinities);
    if (affinity_may_change)
        candidates = pending_running_jobs |
                     (other_jobs & suitable_aff_jobs);
    else
        candidates &= suitable_aff_jobs;
    return !candidates.any();
}


// Used to log a single job
void CQueue::x_LogSubmit(const CJob &  job)
{
    CDiagContext_Extra  extra = GetDiagContext().Extra()
        .Print("job_key", MakeJobKey(job.GetId()));

    extra.Flush();
}


unsigned int  CQueue::Submit(const CNSClientId &        client,
                             CJob &                     job,
                             const string &             aff_token,
                             const string &             group,
                             bool                       logging,
                             CNSRollbackInterface * &   rollback_action)
{
    // the only config parameter used here is the max input size so there is no
    // need to have a safe parameters accessor.

    if (job.GetInput().size() > m_MaxInputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong, "Input is too long");

    unsigned int    aff_id = 0;
    unsigned int    group_id = 0;
    CNSPreciseTime  op_begin_time = CNSPreciseTime::Current();
    unsigned int    job_id = GetNextId();
    CJobEvent &     event = job.AppendEvent();

    job.SetId(job_id);
    job.SetPassport(rand());
    job.SetLastTouch(op_begin_time);

    event.SetNodeAddr(client.GetAddress());
    event.SetStatus(CNetScheduleAPI::ePending);
    event.SetEvent(CJobEvent::eSubmit);
    event.SetTimestamp(op_begin_time);
    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());

    // Special treatment for system job masks
    if (job.GetMask() & CNetScheduleAPI::eOutOfOrder)
    {
        // NOT IMPLEMENTED YET: put job id into OutOfOrder list.
        // The idea is that there can be an urgent job, which
        // should be executed before jobs which were submitted
        // earlier, e.g. for some administrative purposes. See
        // CNetScheduleAPI::EJobMask in file netschedule_api.hpp
    }

    // Take the queue lock and start the operation
    {{
        string              scope = client.GetScope();
        CFastMutexGuard     guard(m_OperationLock);


        if (!scope.empty()) {
            // Check the scope registry limits
            SNSRegistryParameters   params =
                                        m_Server->GetScopeRegistrySettings();
            if (!m_ScopeRegistry.CanAccept(scope, params.max_records))
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                           "No available slots in the queue scope registry");
        }
        if (!group.empty()) {
            // Check the group registry limits
            SNSRegistryParameters   params =
                                        m_Server->GetGroupRegistrySettings();
            if (!m_GroupRegistry.CanAccept(group, params.max_records))
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                           "No available slots in the queue group registry");
        }
        if (!aff_token.empty()) {
            // Check the affinity registry limits
            SNSRegistryParameters   params =
                                        m_Server->GetAffRegistrySettings();
            if (!m_AffinityRegistry.CanAccept(aff_token, params.max_records))
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                           "No available slots in the queue affinity registry");
        }


        if (!group.empty()) {
            group_id = m_GroupRegistry.AddJob(group, job_id);
            job.SetGroupId(group_id);
        }
        if (!aff_token.empty()) {
            aff_id = m_AffinityRegistry.ResolveAffinityToken(aff_token,
                                                    job_id, 0, eUndefined);
            job.SetAffinityId(aff_id);
        }

        m_Jobs[job_id] = job;

        m_StatusTracker.AddPendingJob(job_id);

        if (!scope.empty())
            m_ScopeRegistry.AddJob(scope, job_id);

        // Register the job with the client
        m_ClientsRegistry.AddToSubmitted(client, 1);

        // Make the decision whether to send or not a notification
        if (m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(job_id, aff_id, m_ClientsRegistry,
                                       m_AffinityRegistry, m_GroupRegistry,
                                       m_ScopeRegistry, m_NotifHifreqPeriod,
                                       m_HandicapTimeout, eGet);

        m_GCRegistry.RegisterJob(job_id, op_begin_time,
                                 aff_id, group_id,
                                 job.GetExpirationTime(m_Timeout,
                                                       m_RunTimeout,
                                                       m_ReadTimeout,
                                                       m_PendingTimeout,
                                                       op_begin_time));
    }}

    rollback_action = new CNSSubmitRollback(client, job_id,
                                            op_begin_time,
                                            CNSPreciseTime::Current());

    m_StatisticsCounters.CountSubmit(1);
    if (logging)
        x_LogSubmit(job);

    return job_id;
}


unsigned int
CQueue::SubmitBatch(const CNSClientId &             client,
                    vector< pair<CJob, string> > &  batch,
                    const string &                  group,
                    bool                            logging,
                    CNSRollbackInterface * &        rollback_action)
{
    unsigned int    batch_size = batch.size();
    unsigned int    job_id = GetNextJobIdForBatch(batch_size);
    TNSBitVector    affinities;
    CNSPreciseTime  curr_time = CNSPreciseTime::Current();

    {{
        unsigned int        job_id_cnt = job_id;
        unsigned int        group_id = 0;
        vector<string>      aff_tokens;
        string              scope = client.GetScope();

        // Count the number of affinities
        for (size_t  k = 0; k < batch_size; ++k) {
            const string &      aff_token = batch[k].second;
            if (!aff_token.empty())
                aff_tokens.push_back(aff_token);
        }


        CFastMutexGuard     guard(m_OperationLock);

        if (!scope.empty()) {
            // Check the scope registry limits
            SNSRegistryParameters   params =
                                        m_Server->GetScopeRegistrySettings();
            if (!m_ScopeRegistry.CanAccept(scope, params.max_records))
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                           "No available slots in the queue scope registry");
        }
        if (!group.empty()) {
            // Check the group registry limits
            SNSRegistryParameters   params =
                                        m_Server->GetGroupRegistrySettings();
            if (!m_GroupRegistry.CanAccept(group, params.max_records))
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                           "No available slots in the queue group registry");
        }
        if (!aff_tokens.empty()) {
            // Check the affinity registry limits
            SNSRegistryParameters   params =
                                        m_Server->GetAffRegistrySettings();
            if (!m_AffinityRegistry.CanAccept(aff_tokens, params.max_records))
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                           "No available slots in the queue affinity registry");
        }

        group_id = m_GroupRegistry.ResolveGroup(group);
        for (size_t  k = 0; k < batch_size; ++k) {

            CJob &              job = batch[k].first;
            const string &      aff_token = batch[k].second;
            CJobEvent &         event = job.AppendEvent();

            job.SetId(job_id_cnt);
            job.SetPassport(rand());
            job.SetGroupId(group_id);
            job.SetLastTouch(curr_time);

            event.SetNodeAddr(client.GetAddress());
            event.SetStatus(CNetScheduleAPI::ePending);
            event.SetEvent(CJobEvent::eBatchSubmit);
            event.SetTimestamp(curr_time);
            event.SetClientNode(client.GetNode());
            event.SetClientSession(client.GetSession());

            if (!aff_token.empty()) {
                unsigned int    aff_id = m_AffinityRegistry.
                                        ResolveAffinityToken(aff_token,
                                                             job_id_cnt,
                                                             0,
                                                             eUndefined);

                job.SetAffinityId(aff_id);
                affinities.set_bit(aff_id);
            }

            m_Jobs[job_id_cnt] = job;
            ++job_id_cnt;
        }

        m_GroupRegistry.AddJobs(group_id, job_id, batch_size);
        m_StatusTracker.AddPendingBatch(job_id, job_id + batch_size - 1);
        m_ClientsRegistry.AddToSubmitted(client, batch_size);

        if (!scope.empty())
            m_ScopeRegistry.AddJobs(scope, job_id, batch_size);

        // Make a decision whether to notify clients or not
        TNSBitVector        jobs;
        jobs.set_range(job_id, job_id + batch_size - 1);

        if (m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(jobs, affinities,
                                       batch_size != aff_tokens.size(),
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_ScopeRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eGet);

        for (size_t  k = 0; k < batch_size; ++k) {
            m_GCRegistry.RegisterJob(
                        batch[k].first.GetId(), curr_time,
                        batch[k].first.GetAffinityId(), group_id,
                        batch[k].first.GetExpirationTime(m_Timeout,
                                                         m_RunTimeout,
                                                         m_ReadTimeout,
                                                         m_PendingTimeout,
                                                         curr_time));
        }
    }}

    m_StatisticsCounters.CountSubmit(batch_size);
    if (m_LogBatchEachJob && logging)
        for (size_t  k = 0; k < batch_size; ++k)
            x_LogSubmit(batch[k].first);

    rollback_action = new CNSBatchSubmitRollback(client, job_id, batch_size);
    return job_id;
}


TJobStatus  CQueue::PutResult(const CNSClientId &     client,
                              const CNSPreciseTime &  curr,
                              unsigned int            job_id,
                              const string &          job_key,
                              CJob &                  job,
                              const string &          auth_token,
                              int                     ret_code,
                              const string &          output)
{
    // The only one parameter (max output size) is required for the put
    // operation so there is no need to use CQueueParamAccessor

    if (output.size() > m_MaxOutputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");

    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status == CNetScheduleAPI::eDone) {
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eDone,
                                             CNetScheduleAPI::eDone);
        return old_status;
    }

    if (old_status != CNetScheduleAPI::ePending &&
        old_status != CNetScheduleAPI::eRunning &&
        old_status != CNetScheduleAPI::eFailed)
        return old_status;

    x_UpdateDB_PutResultNoLock(job_id, auth_token, curr, ret_code, output,
                               job, client);

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eDone);
    m_StatisticsCounters.CountTransition(old_status,
                                         CNetScheduleAPI::eDone);
    g_DoPerfLogging(*this, job, 200);
    m_ClientsRegistry.UnregisterJob(job_id, eGet);

    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      curr));

    TimeLineRemove(job_id);

    x_NotifyJobChanges(job, job_key, eStatusChanged, curr);

    // Notify the readers if the job has not been given for reading yet
    if (!m_ReadJobs.get_bit(job_id)) {
        m_GCRegistry.UpdateReadVacantTime(job_id, curr);
        m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_GroupRegistry,
                                   m_ScopeRegistry,
                                   m_NotifHifreqPeriod,
                                   m_HandicapTimeout,
                                   eRead);
    }
    return old_status;
}


bool
CQueue::GetJobOrWait(const CNSClientId &       client,
                     unsigned short            port, // Port the client
                                                     // will wait on
                     unsigned int              timeout, // If timeout != 0 =>
                                                        // WGET
                     const list<string> *      aff_list,
                     bool                      wnode_affinity,
                     bool                      any_affinity,
                     bool                      exclusive_new_affinity,
                     bool                      prioritized_aff,
                     bool                      new_format,
                     const list<string> *      group_list,
                     CJob *                    new_job,
                     CNSRollbackInterface * &  rollback_action,
                     string &                  added_pref_aff)
{
    // We need exactly 1 parameter - m_RunTimeout, so we can access it without
    // CQueueParamAccessor

    CFastMutexGuard     guard(m_OperationLock);
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    // This is a worker node command, so mark the node type as a worker
    // node
    m_ClientsRegistry.AppendType(client, CNSClient::eWorkerNode);

    vector<unsigned int>    aff_ids;
    TNSBitVector            aff_ids_vector;
    TNSBitVector            group_ids_vector;
    bool                    has_groups = false;

    {{

        if (wnode_affinity) {
            // Check that the preferred affinities were not reset
            if (m_ClientsRegistry.GetAffinityReset(client, eGet))
                return false;

            // Check that the client was garbage collected with preferred affs
            if (m_ClientsRegistry.WasGarbageCollected(client, eGet))
                return false;
        }

        // Resolve affinities and groups. It is supposed that the client knows
        // better what affinities and groups to expect i.e. even if they do not
        // exist yet, they may appear soon.
        if (group_list != NULL) {
            m_GroupRegistry.ResolveGroups(*group_list, group_ids_vector);
            has_groups = !group_list->empty();
        }
        if (aff_list != NULL)
            m_AffinityRegistry.ResolveAffinities(*aff_list, aff_ids_vector,
                                                 aff_ids);

        x_UnregisterGetListener(client, port);
    }}

    for (;;) {
        // Old comment:
        // No lock here to make it possible to pick a job
        // simultaneously from many threads
        // Current state:
        // The lock is taken at the beginning. Now there is not much of a
        // concurrency so a bit of performance is not needed anymore.
        // The rest is left untouched to simplify the changes
        x_SJobPick  job_pick = x_FindVacantJob(client,
                                               aff_ids_vector, aff_ids,
                                               wnode_affinity,
                                               any_affinity,
                                               exclusive_new_affinity,
                                               prioritized_aff,
                                               group_ids_vector, has_groups,
                                               eGet);
        {{
            bool                outdated_job = false;

            if (job_pick.job_id == 0) {
                if (exclusive_new_affinity)
                    // Second try only if exclusive new aff is set on
                    job_pick = x_FindOutdatedPendingJob(client, 0,
                                                        group_ids_vector);

                if (job_pick.job_id == 0) {
                    if (timeout != 0 && port > 0)
                        // WGET: // There is no job, so the client might need to
                        // be registered in the waiting list
                        x_RegisterGetListener(client, port, timeout,
                                              aff_ids_vector,
                                              wnode_affinity, any_affinity,
                                              exclusive_new_affinity,
                                              new_format, group_ids_vector);
                    return true;
                }
                outdated_job = true;
            } else {
                // Check that the job is still Pending; it could be
                // grabbed by another WN or GC
                if (GetJobStatus(job_pick.job_id) != CNetScheduleAPI::ePending)
                    continue;   // Try to pick a job again

                if (exclusive_new_affinity) {
                    if (m_GCRegistry.IsOutdatedJob(
                                    job_pick.job_id, eGet,
                                    m_MaxPendingWaitTimeout) == false) {
                        x_SJobPick  outdated_pick =
                                        x_FindOutdatedPendingJob(
                                                    client, job_pick.job_id,
                                                    group_ids_vector);
                        if (outdated_pick.job_id != 0) {
                            job_pick = outdated_pick;
                            outdated_job = true;
                        }
                    }
                }
            }

            // The job is still pending, check if it was received as
            // with exclusive affinity
            if (job_pick.exclusive && job_pick.aff_id != 0 &&
                outdated_job == false) {
                if (m_ClientsRegistry.IsPreferredByAny(
                                                job_pick.aff_id, eGet))
                    continue;  // Other WN grabbed this affinity already

                string  aff_token = m_AffinityRegistry.GetTokenByID(
                                                            job_pick.aff_id);
                // CXX-8843: The '-' affinity must not be added to the list of
                // preferred affinities
                if (aff_token != k_NoAffinityToken) {
                    bool added = m_ClientsRegistry.
                                    UpdatePreferredAffinities(
                                        client, job_pick.aff_id, 0, eGet);
                    if (added)
                        added_pref_aff = aff_token;
                }
            }
            if (outdated_job && job_pick.aff_id != 0) {
                string  aff_token = m_AffinityRegistry.GetTokenByID(
                                                            job_pick.aff_id);
                // CXX-8843: The '-' affinity must not be added to the list of
                // preferred affinities
                if (aff_token != k_NoAffinityToken) {
                    bool added = m_ClientsRegistry.
                                    UpdatePreferredAffinities(
                                        client, job_pick.aff_id, 0, eGet);
                    if (added)
                        added_pref_aff = aff_token;
                }
            }

            x_UpdateDB_ProvideJobNoLock(client, curr, job_pick.job_id, eGet,
                                        *new_job);
            m_StatusTracker.SetStatus(job_pick.job_id,
                                      CNetScheduleAPI::eRunning);

            m_StatisticsCounters.CountTransition(
                                        CNetScheduleAPI::ePending,
                                        CNetScheduleAPI::eRunning);
            g_DoPerfLogging(*this, *new_job, 200);
            if (outdated_job)
                m_StatisticsCounters.CountOutdatedPick(eGet);

            m_GCRegistry.UpdateLifetime(
                        job_pick.job_id,
                        new_job->GetExpirationTime(m_Timeout,
                                                   m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout,
                                                   curr));
            TimeLineAdd(job_pick.job_id, curr + m_RunTimeout);
            m_ClientsRegistry.RegisterJob(client, job_pick.job_id, eGet);

            x_NotifyJobChanges(*new_job, MakeJobKey(job_pick.job_id),
                               eStatusChanged, curr);

            // If there are no more pending jobs, let's clear the
            // list of delayed exact notifications.
            if (!m_StatusTracker.AnyPending())
                m_NotificationsList.ClearExactGetNotifications();

            rollback_action = new CNSGetJobRollback(client, job_pick.job_id);
            return true;
        }}
    }
    return true;
}


void  CQueue::CancelWaitGet(const CNSClientId &  client)
{
    bool    result;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        result = x_UnregisterGetListener(client, 0);
    }}

    if (result == false)
        ERR_POST(Warning << "Attempt to cancel WGET for the client "
                            "which does not wait anything (node: "
                         << client.GetNode() << " session: "
                         << client.GetSession() << ")");
}


void  CQueue::CancelWaitRead(const CNSClientId &  client)
{
    bool    result;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        result = m_ClientsRegistry.CancelWaiting(client, eRead);
    }}

    if (result == false)
        ERR_POST(Warning << "Attempt to cancel waiting READ for the client "
                            "which does not wait anything (node: "
                         << client.GetNode() << " session: "
                         << client.GetSession() << ")");
}


list<string>
CQueue::ChangeAffinity(const CNSClientId &     client,
                       const list<string> &    aff_to_add,
                       const list<string> &    aff_to_del,
                       ECommandGroup           cmd_group)
{
    // It is guaranteed here that the client is a new style one.
    // I.e. it has both client_node and client_session.
    if (cmd_group == eGet)
        m_ClientsRegistry.AppendType(client, CNSClient::eWorkerNode);
    else
        m_ClientsRegistry.AppendType(client, CNSClient::eReader);


    list<string>    msgs;   // Warning messages for the socket
    unsigned int    client_id = client.GetID();
    TNSBitVector    current_affinities =
                         m_ClientsRegistry.GetPreferredAffinities(client,
                                                                  cmd_group);
    TNSBitVector    aff_id_to_add;
    TNSBitVector    aff_id_to_del;
    bool            any_to_add = false;
    bool            any_to_del = false;

    // Identify the affinities which should be deleted
    for (list<string>::const_iterator  k(aff_to_del.begin());
         k != aff_to_del.end(); ++k) {
        unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(*k);

        if (aff_id == 0) {
            // The affinity is not known for NS at all
            ERR_POST(Warning << "Client '" << client.GetNode()
                             << "' deletes unknown affinity '"
                             << *k << "'. Ignored.");
            msgs.push_back("eAffinityNotFound:"
                           "unknown affinity to delete: " + *k);
            continue;
        }

        if (!current_affinities.get_bit(aff_id)) {
            // This a try to delete something which has not been added or
            // deleted before.
            ERR_POST(Warning << "Client '" << client.GetNode()
                             << "' deletes affinity '" << *k
                             << "' which is not in the list of the "
                                "preferred client affinities. Ignored.");
            msgs.push_back("eAffinityNotPreferred:not registered affinity "
                           "to delete: " + *k);
            continue;
        }

        // The affinity will really be deleted
        aff_id_to_del.set_bit(aff_id);
        any_to_del = true;
    }


    // Check that the update of the affinities list will not exceed the limit
    // for the max number of affinities per client.
    // Note: this is not precise check. There could be non-unique affinities in
    // the add list or some of affinities to add could already be in the list.
    // The precise checking however requires more CPU and blocking so only an
    // approximate (but fast) checking is done.
    SNSRegistryParameters       aff_reg_settings =
                                        m_Server->GetAffRegistrySettings();
    if (current_affinities.count() + aff_to_add.size()
                                    - aff_id_to_del.count() >
                                         aff_reg_settings.max_records) {
        NCBI_THROW(CNetScheduleException, eTooManyPreferredAffinities,
                   "The client '" + client.GetNode() +
                   "' exceeds the limit (" +
                   to_string(aff_reg_settings.max_records) +
                   ") of the preferred affinities. Changed request ignored.");
    }

    // To avoid logging under the lock
    vector<string>  already_added_affinities;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        // Convert the aff_to_add to the affinity IDs
        for (list<string>::const_iterator  k(aff_to_add.begin());
             k != aff_to_add.end(); ++k ) {
            unsigned int    aff_id =
                                 m_AffinityRegistry.ResolveAffinityToken(*k,
                                                                 0, client_id,
                                                                 cmd_group);

            if (current_affinities.get_bit(aff_id)) {
                already_added_affinities.push_back(*k);
                continue;
            }

            aff_id_to_add.set_bit(aff_id);
            any_to_add = true;
        }
    }}

    // Log the warnings and add it to the warning message
    for (vector<string>::const_iterator  j(already_added_affinities.begin());
         j != already_added_affinities.end(); ++j) {
        // That was a try to add something which has already been added
        ERR_POST(Warning << "Client '" << client.GetNode()
                         << "' adds affinity '" << *j
                         << "' which is already in the list of the "
                            "preferred client affinities. Ignored.");
        msgs.push_back("eAffinityAlreadyPreferred:already registered "
                       "affinity to add: " + *j);
    }

    if (any_to_add || any_to_del)
        m_ClientsRegistry.UpdatePreferredAffinities(client,
                                                    aff_id_to_add,
                                                    aff_id_to_del,
                                                    cmd_group);

    if (m_ClientsRegistry.WasGarbageCollected(client, cmd_group)) {
        ERR_POST(Warning << "Client '" << client.GetNode()
                         << "' has been garbage collected and tries to "
                            "update its preferred affinities.");
        msgs.push_back("eClientGarbageCollected:the client had been "
                       "garbage collected");
    }
    return msgs;
}


void  CQueue::SetAffinity(const CNSClientId &     client,
                          const list<string> &    aff,
                          ECommandGroup           cmd_group)
{
    if (cmd_group == eGet)
        m_ClientsRegistry.AppendType(client, CNSClient::eWorkerNode);
    else
        m_ClientsRegistry.AppendType(client, CNSClient::eReader);

    SNSRegistryParameters   aff_reg_settings =
                                            m_Server->GetAffRegistrySettings();

    if (aff.size() > aff_reg_settings.max_records) {
        NCBI_THROW(CNetScheduleException, eTooManyPreferredAffinities,
                   "The client '" + client.GetNode() +
                   "' exceeds the limit (" +
                   to_string(aff_reg_settings.max_records) +
                   ") of the preferred affinities. Set request ignored.");
    }

    unsigned int    client_id = client.GetID();
    TNSBitVector    aff_id_to_set;
    TNSBitVector    already_added_aff_id;


    TNSBitVector    current_affinities =
                         m_ClientsRegistry.GetPreferredAffinities(client,
                                                                  cmd_group);
    {{
        CFastMutexGuard     guard(m_OperationLock);

        // Convert the aff to the affinity IDs
        for (list<string>::const_iterator  k(aff.begin());
             k != aff.end(); ++k ) {
            unsigned int    aff_id =
                                 m_AffinityRegistry.ResolveAffinityToken(*k,
                                                                 0, client_id,
                                                                 cmd_group);

            if (current_affinities.get_bit(aff_id))
                already_added_aff_id.set_bit(aff_id);

            aff_id_to_set.set_bit(aff_id);
        }
    }}

    m_ClientsRegistry.SetPreferredAffinities(client, aff_id_to_set, cmd_group);
}


int CQueue::SetClientData(const CNSClientId &  client,
                          const string &  data, int  data_version)
{
    return m_ClientsRegistry.SetClientData(client, data, data_version);
}


TJobStatus  CQueue::JobDelayExpiration(unsigned int            job_id,
                                       CJob &                  job,
                                       const CNSPreciseTime &  tm)
{
    CNSPreciseTime      queue_run_timeout = GetRunTimeout();
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          status = GetJobStatus(job_id);

    if (status != CNetScheduleAPI::eRunning)
        return status;

    CNSPreciseTime  time_start = kTimeZero;
    CNSPreciseTime  run_timeout = kTimeZero;

    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        return CNetScheduleAPI::eJobNotFound;

    time_start = job_iter->second.GetLastEvent()->GetTimestamp();
    run_timeout = job_iter->second.GetRunTimeout();
    if (run_timeout == kTimeZero)
        run_timeout = queue_run_timeout;

    if (time_start + run_timeout > curr + tm) {
        job = job_iter->second;
        return CNetScheduleAPI::eRunning;   // Old timeout is enough to cover
                                            // this request, so keep it.
    }

    job_iter->second.SetRunTimeout(curr + tm - time_start);
    job_iter->second.SetLastTouch(curr);

    // No need to update the GC registry because the running (and reading)
    // jobs are skipped by GC
    CNSPreciseTime      exp_time = kTimeZero;
    if (run_timeout != kTimeZero)
        exp_time = time_start + run_timeout;

    TimeLineMove(job_id, exp_time, curr + tm);

    job = job_iter->second;
    return CNetScheduleAPI::eRunning;
}


TJobStatus  CQueue::JobDelayReadExpiration(unsigned int            job_id,
                                           CJob &                  job,
                                           const CNSPreciseTime &  tm)
{
    CNSPreciseTime      queue_read_timeout = GetReadTimeout();
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          status = GetJobStatus(job_id);

    if (status != CNetScheduleAPI::eReading)
        return status;

    CNSPreciseTime  time_start = kTimeZero;
    CNSPreciseTime  read_timeout = kTimeZero;

    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        return CNetScheduleAPI::eJobNotFound;

    time_start = job_iter->second.GetLastEvent()->GetTimestamp();
    read_timeout = job_iter->second.GetReadTimeout();
    if (read_timeout == kTimeZero)
        read_timeout = queue_read_timeout;

    if (time_start + read_timeout > curr + tm) {
        job = job_iter->second;
        return CNetScheduleAPI::eReading;   // Old timeout is enough to
                                            // cover this request, so
                                            // keep it.
    }

    job_iter->second.SetReadTimeout(curr + tm - time_start);
    job_iter->second.SetLastTouch(curr);

    // No need to update the GC registry because the running (and reading)
    // jobs are skipped by GC
    CNSPreciseTime      exp_time = kTimeZero;
    if (read_timeout != kTimeZero)
        exp_time = time_start + read_timeout;

    TimeLineMove(job_id, exp_time, curr + tm);

    job = job_iter->second;
    return CNetScheduleAPI::eReading;
}



// This member is used for WST/WST2 which do not need to touch the job
TJobStatus  CQueue::GetStatusAndLifetime(unsigned int      job_id,
                                         string &          client_ip,
                                         string &          client_sid,
                                         string &          client_phid,
                                         string &          progress_msg,
                                         CNSPreciseTime *  lifetime)
{
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    auto    job_iter = m_Jobs.find(job_id);

    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    client_ip = job_iter->second.GetClientIP();
    client_sid = job_iter->second.GetClientSID();
    client_phid = job_iter->second.GetNCBIPHID();
    progress_msg = job_iter->second.GetProgressMsg();

    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    return status;
}


// This member is used for the SST/SST2 commands which also touch the job
TJobStatus  CQueue::GetStatusAndLifetimeAndTouch(unsigned int      job_id,
                                                 CJob &            job,
                                                 CNSPreciseTime *  lifetime)
{
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    CNSPreciseTime      curr = CNSPreciseTime::Current();
    auto                job_iter = m_Jobs.find(job_id);

    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    job_iter->second.SetLastTouch(curr);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout, curr));

    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    job = job_iter->second;
    return status;
}


TJobStatus  CQueue::SetJobListener(unsigned int            job_id,
                                   CJob &                  job,
                                   unsigned int            address,
                                   unsigned short          port,
                                   const CNSPreciseTime &  timeout,
                                   bool                    need_stolen,
                                   bool                    need_progress_msg,
                                   size_t *                last_event_index)
{
    CNSPreciseTime      curr = CNSPreciseTime::Current();
    TJobStatus          status = CNetScheduleAPI::eJobNotFound;
    CFastMutexGuard     guard(m_OperationLock);

    auto        job_iter = m_Jobs.find(job_id);

    if (job_iter == m_Jobs.end())
        return status;

    *last_event_index = job_iter->second.GetLastEventIndex();
    status = job_iter->second.GetStatus();

    unsigned int    old_listener_addr = job_iter->second.GetListenerNotifAddr();
    unsigned short  old_listener_port = job_iter->second.GetListenerNotifPort();

    if (job_iter->second.GetNeedStolenNotif() &&
        old_listener_addr != 0 && old_listener_port != 0) {
        if (old_listener_addr != address || old_listener_port != port) {
            // Send the stolen notification only if it is
            // really a new listener
            x_NotifyJobChanges(job_iter->second, MakeJobKey(job_id),
                               eNotificationStolen, curr);
        }
    }

    if (address == 0 || port == 0 || timeout == kTimeZero) {
        // If at least one of the values is 0 => no notifications
        // So to make the job properly dumped put zeros everywhere.
        job_iter->second.SetListenerNotifAddr(0);
        job_iter->second.SetListenerNotifPort(0);
        job_iter->second.SetListenerNotifAbsTime(kTimeZero);
    } else {
        job_iter->second.SetListenerNotifAddr(address);
        job_iter->second.SetListenerNotifPort(port);
        job_iter->second.SetListenerNotifAbsTime(curr + timeout);
    }

    job_iter->second.SetNeedLsnrProgressMsgNotif(need_progress_msg);
    job_iter->second.SetNeedStolenNotif(need_stolen);
    job_iter->second.SetLastTouch(curr);

    job = job_iter->second;
    return status;
}


bool CQueue::PutProgressMessage(unsigned int    job_id,
                                CJob &          job,
                                const string &  msg)
{
    CNSPreciseTime      curr = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);

    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        return false;

    job_iter->second.SetProgressMsg(msg);
    job_iter->second.SetLastTouch(curr);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout, curr));
    x_NotifyJobChanges(job_iter->second, MakeJobKey(job_id),
                       eProgressMessageChanged, curr);

    job = job_iter->second;
    return true;
}


TJobStatus  CQueue::ReturnJob(const CNSClientId &     client,
                              unsigned int            job_id,
                              const string &          job_key,
                              CJob &                  job,
                              const string &          auth_token,
                              string &                warning,
                              TJobReturnOption        how)
{
    CFastMutexGuard     guard(m_OperationLock);
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    if (!auth_token.empty()) {
        // Need to check authorization token first
        CJob::EAuthTokenCompareResult   token_compare_result =
                                job_iter->second.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");
        if (token_compare_result == CJob::ePassportOnlyMatch) {
            // That means the job has been given to another worker node
            // by whatever reason (expired/failed/returned before)
            ERR_POST(Warning << "Received RETURN2 with only "
                                "passport matched.");
            warning = "eJobPassportOnlyMatch:Only job passport matched. "
                      "Command is ignored.";
            job = job_iter->second;
            return old_status;
        }
        // Here: the authorization token is OK, we can continue
    }

    unsigned int    run_count = job_iter->second.GetRunCount();
    CJobEvent *     event = job_iter->second.GetLastEvent();

    if (!event)
        ERR_POST("No JobEvent for running job");

    event = &job_iter->second.AppendEvent();
    event->SetNodeAddr(client.GetAddress());
    event->SetStatus(CNetScheduleAPI::ePending);
    switch (how) {
        case eWithBlacklist:
            event->SetEvent(CJobEvent::eReturn);
            break;
        case eWithoutBlacklist:
            event->SetEvent(CJobEvent::eReturnNoBlacklist);
            break;
        case eRollback:
            event->SetEvent(CJobEvent::eNSGetRollback);
            break;
    }
    event->SetTimestamp(current_time);
    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());

    if (run_count)
        job_iter->second.SetRunCount(run_count - 1);

    job_iter->second.SetStatus(CNetScheduleAPI::ePending);
    job_iter->second.SetLastTouch(current_time);

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    switch (how) {
        case eWithBlacklist:
            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::ePending);
            break;
        case eWithoutBlacklist:
            m_StatisticsCounters.CountToPendingWithoutBlacklist(1);
            break;
        case eRollback:
            m_StatisticsCounters.CountNSGetRollback(1);
            break;
    }
    g_DoPerfLogging(*this, job_iter->second, 200);
    TimeLineRemove(job_id);
    m_ClientsRegistry.UnregisterJob(job_id, eGet);
    if (how == eWithBlacklist)
        m_ClientsRegistry.RegisterBlacklistedJob(client, job_id, eGet);
    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout, m_PendingTimeout,
                                                   current_time));

    x_NotifyJobChanges(job_iter->second, job_key, eStatusChanged, current_time);

    if (m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(
            job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
            m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
            m_NotifHifreqPeriod, m_HandicapTimeout, eGet);

    job = job_iter->second;
    return old_status;
}


TJobStatus  CQueue::RescheduleJob(const CNSClientId &     client,
                                  unsigned int            job_id,
                                  const string &          job_key,
                                  const string &          auth_token,
                                  const string &          aff_token,
                                  const string &          group,
                                  bool &                  auth_token_ok,
                                  CJob &                  job)
{
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);
    unsigned int        affinity_id = 0;
    unsigned int        group_id = 0;
    unsigned int        job_affinity_id;
    unsigned int        job_group_id;

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    // Resolve affinity and group in a separate transaction
    if (!aff_token.empty() || !group.empty()) {
        if (!aff_token.empty())
            affinity_id = m_AffinityRegistry.ResolveAffinity(aff_token);
        if (!group.empty())
            group_id = m_GroupRegistry.ResolveGroup(group);
    }

    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    // Need to check authorization token first
    CJob::EAuthTokenCompareResult   token_compare_result =
                                job_iter->second.CompareAuthToken(auth_token);

    if (token_compare_result == CJob::eInvalidTokenFormat)
        NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                   "Invalid authorization token format");

    if (token_compare_result != CJob::eCompleteMatch) {
        auth_token_ok = false;
        job = job_iter->second;
        return old_status;
    }

    // Here: the authorization token is OK, we can continue
    auth_token_ok = true;

    // Memorize the job group and affinity for the proper updates after
    // the transaction is finished
    job_affinity_id = job_iter->second.GetAffinityId();
    job_group_id = job_iter->second.GetGroupId();

    // Update the job affinity and group
    job_iter->second.SetAffinityId(affinity_id);
    job_iter->second.SetGroupId(group_id);

    unsigned int    run_count = job_iter->second.GetRunCount();
    CJobEvent *     event = job_iter->second.GetLastEvent();

    if (!event)
        ERR_POST("No JobEvent for running job");

    event = &job_iter->second.AppendEvent();
    event->SetNodeAddr(client.GetAddress());
    event->SetStatus(CNetScheduleAPI::ePending);
    event->SetEvent(CJobEvent::eReschedule);
    event->SetTimestamp(current_time);
    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());

    if (run_count)
        job_iter->second.SetRunCount(run_count - 1);

    job_iter->second.SetStatus(CNetScheduleAPI::ePending);
    job_iter->second.SetLastTouch(current_time);

    // Job has been updated in the DB. Update the affinity and group
    // registries as needed.
    if (job_affinity_id != affinity_id) {
        if (job_affinity_id != 0)
            m_AffinityRegistry.RemoveJobFromAffinity(job_id, job_affinity_id);
        if (affinity_id != 0)
            m_AffinityRegistry.AddJobToAffinity(job_id, affinity_id);
    }
    if (job_group_id != group_id) {
        if (job_group_id != 0)
            m_GroupRegistry.RemoveJob(job_group_id, job_id);
        if (group_id != 0)
            m_GroupRegistry.AddJob(group_id, job_id);
    }
    if (job_affinity_id != affinity_id || job_group_id != group_id)
        m_GCRegistry.ChangeAffinityAndGroup(job_id, affinity_id, group_id);

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    m_StatisticsCounters.CountToPendingRescheduled(1);
    g_DoPerfLogging(*this, job_iter->second, 200);

    TimeLineRemove(job_id);
    m_ClientsRegistry.UnregisterJob(job_id, eGet);
    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout,
                                                   current_time));

    x_NotifyJobChanges(job_iter->second, job_key, eStatusChanged, current_time);

    if (m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(
            job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
            m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
            m_NotifHifreqPeriod, m_HandicapTimeout, eGet);

    job = job_iter->second;
    return old_status;
}


TJobStatus  CQueue::RedoJob(const CNSClientId &     client,
                            unsigned int            job_id,
                            const string &          job_key,
                            CJob &                  job)
{
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status == CNetScheduleAPI::eJobNotFound ||
        old_status == CNetScheduleAPI::ePending ||
        old_status == CNetScheduleAPI::eRunning ||
        old_status == CNetScheduleAPI::eReading)
        return old_status;

    auto        job_iter = m_Jobs.find(job_id);

    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Error fetching job");

    CJobEvent *     event = job_iter->second.GetLastEvent();
    if (!event)
        ERR_POST("Inconsistency: a job has no events");

    event = &job_iter->second.AppendEvent();
    event->SetNodeAddr(client.GetAddress());
    event->SetStatus(CNetScheduleAPI::ePending);
    event->SetEvent(CJobEvent::eRedo);
    event->SetTimestamp(current_time);
    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());

    job_iter->second.SetStatus(CNetScheduleAPI::ePending);
    job_iter->second.SetLastTouch(current_time);

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    m_StatisticsCounters.CountRedo(old_status);
    g_DoPerfLogging(*this, job_iter->second, 200);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout,
                                                   current_time));

    x_NotifyJobChanges(job_iter->second, job_key, eStatusChanged, current_time);

    if (m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id,
                                   job_iter->second.GetAffinityId(),
                                   m_ClientsRegistry, m_AffinityRegistry,
                                   m_GroupRegistry, m_ScopeRegistry,
                                   m_NotifHifreqPeriod,
                                   m_HandicapTimeout, eGet);
    job = job_iter->second;
    return old_status;
}


TJobStatus  CQueue::ReadAndTouchJob(unsigned int      job_id,
                                    CJob &            job,
                                    CNSPreciseTime *  lifetime)
{
    CFastMutexGuard         guard(m_OperationLock);
    TJobStatus              status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    CNSPreciseTime          curr = CNSPreciseTime::Current();
    auto                    job_iter = m_Jobs.find(job_id);

    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    job_iter->second.SetLastTouch(curr);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout, curr));
    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    job = job_iter->second;
    return status;
}


TJobStatus  CQueue::Cancel(const CNSClientId &  client,
                           unsigned int         job_id,
                           const string &       job_key,
                           CJob &               job,
                           bool                 is_ns_rollback)
{
    TJobStatus          old_status;
    CNSPreciseTime      current_time = CNSPreciseTime::Current();

    CFastMutexGuard     guard(m_OperationLock);

    old_status = m_StatusTracker.GetStatus(job_id);
    if (old_status == CNetScheduleAPI::eJobNotFound)
        return CNetScheduleAPI::eJobNotFound;

    if (old_status == CNetScheduleAPI::eCanceled) {
        if (is_ns_rollback)
            m_StatisticsCounters.CountNSSubmitRollback(1);
        else
            m_StatisticsCounters.CountTransition(
                                        CNetScheduleAPI::eCanceled,
                                        CNetScheduleAPI::eCanceled);
        return CNetScheduleAPI::eCanceled;
    }

    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        return CNetScheduleAPI::eJobNotFound;

    CJobEvent *     event = &job_iter->second.AppendEvent();

    event->SetNodeAddr(client.GetAddress());
    event->SetStatus(CNetScheduleAPI::eCanceled);
    if (is_ns_rollback)
        event->SetEvent(CJobEvent::eNSSubmitRollback);
    else
        event->SetEvent(CJobEvent::eCancel);
    event->SetTimestamp(current_time);
    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());

    job_iter->second.SetStatus(CNetScheduleAPI::eCanceled);
    job_iter->second.SetLastTouch(current_time);

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eCanceled);
    if (is_ns_rollback) {
        m_StatisticsCounters.CountNSSubmitRollback(1);
    } else {
        m_StatisticsCounters.CountTransition(old_status,
                                             CNetScheduleAPI::eCanceled);
        g_DoPerfLogging(*this, job_iter->second, 200);
    }

    TimeLineRemove(job_id);
    if (old_status == CNetScheduleAPI::eRunning)
        m_ClientsRegistry.UnregisterJob(job_id, eGet);
    else if (old_status == CNetScheduleAPI::eReading)
        m_ClientsRegistry.UnregisterJob(job_id, eRead);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout,
                                                   current_time));

    x_NotifyJobChanges(job_iter->second, job_key,
                       eStatusChanged, current_time);

    // Notify the readers if the job has not been given for reading yet
    // and it was not a rollback due to a socket write error
    if (!m_ReadJobs.get_bit(job_id) && is_ns_rollback == false) {
        m_GCRegistry.UpdateReadVacantTime(job_id, current_time);
        m_NotificationsList.Notify(
            job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
            m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
            m_NotifHifreqPeriod, m_HandicapTimeout, eRead);
    }

    job = job_iter->second;
    return old_status;
}


unsigned int  CQueue::CancelAllJobs(const CNSClientId &  client,
                                    bool                 logging)
{
    vector<CNetScheduleAPI::EJobStatus> statuses;

    // All except cancelled
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    statuses.push_back(CNetScheduleAPI::eFailed);
    statuses.push_back(CNetScheduleAPI::eDone);
    statuses.push_back(CNetScheduleAPI::eReading);
    statuses.push_back(CNetScheduleAPI::eConfirmed);
    statuses.push_back(CNetScheduleAPI::eReadFailed);

    TNSBitVector        jobs;
    CFastMutexGuard     guard(m_OperationLock);
    m_StatusTracker.GetJobs(statuses, jobs);
    return x_CancelJobs(client, jobs, logging);
}


unsigned int CQueue::x_CancelJobs(const CNSClientId &   client,
                                  const TNSBitVector &  candidates_to_cancel,
                                  bool                  logging)
{
    CJob                        job;
    CNSPreciseTime              current_time = CNSPreciseTime::Current();
    TNSBitVector                jobs_to_cancel = candidates_to_cancel;

    // Filter the jobs basing on scope if so
    string                      scope = client.GetScope();
    if (scope.empty() || scope != kNoScopeOnly) {
        // Both these cases should consider only the non-scope jobs
        jobs_to_cancel -= m_ScopeRegistry.GetAllJobsInScopes();
    } else {
        // Consider only the jobs in the particular scope
        jobs_to_cancel &= m_ScopeRegistry.GetJobs(scope);
    }

    TNSBitVector::enumerator    en(jobs_to_cancel.first());
    unsigned int                count = 0;
    for (; en.valid(); ++en) {
        unsigned int    job_id = *en;
        TJobStatus      old_status = m_StatusTracker.GetStatus(job_id);
        auto            job_iter = m_Jobs.find(job_id);

        if (job_iter == m_Jobs.end()) {
            ERR_POST("Cannot fetch job " << DecorateJob(job_id) <<
                     " while cancelling jobs");
            continue;
        }

        CJobEvent *     event = &job_iter->second.AppendEvent();

        event->SetNodeAddr(client.GetAddress());
        event->SetStatus(CNetScheduleAPI::eCanceled);
        event->SetEvent(CJobEvent::eCancel);
        event->SetTimestamp(current_time);
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        job_iter->second.SetStatus(CNetScheduleAPI::eCanceled);
        job_iter->second.SetLastTouch(current_time);

        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eCanceled);
        m_StatisticsCounters.CountTransition(old_status,
                                             CNetScheduleAPI::eCanceled);
        g_DoPerfLogging(*this, job_iter->second, 200);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.UnregisterJob(job_id, eGet);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.UnregisterJob(job_id, eRead);

        m_GCRegistry.UpdateLifetime(
            job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                       m_ReadTimeout,
                                                       m_PendingTimeout,
                                                       current_time));

        x_NotifyJobChanges(job_iter->second, MakeJobKey(job_id),
                           eStatusChanged, current_time);

        // Notify the readers if the job has not been given for reading yet
        if (!m_ReadJobs.get_bit(job_id)) {
            m_GCRegistry.UpdateReadVacantTime(job_id, current_time);
            m_NotificationsList.Notify(
                job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
                m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
                m_NotifHifreqPeriod, m_HandicapTimeout, eRead);
        }

        if (logging)
            GetDiagContext().Extra()
                .Print("job_key", MakeJobKey(job_id))
                .Print("job_phid", job_iter->second.GetNCBIPHID());

        ++count;
    }
    return count;
}


// This function must be called under the operations lock.
// If called for not existing job then an exception is generated
CNSPreciseTime
CQueue::x_GetEstimatedJobLifetime(unsigned int   job_id,
                                  TJobStatus     status) const
{
    if (status == CNetScheduleAPI::eRunning ||
        status == CNetScheduleAPI::eReading)
        return CNSPreciseTime::Current() + GetTimeout();
    return m_GCRegistry.GetLifetime(job_id);
}


unsigned int
CQueue::CancelSelectedJobs(const CNSClientId &         client,
                           const string &              group,
                           const string &              aff_token,
                           const vector<TJobStatus> &  job_statuses,
                           bool                        logging,
                           vector<string> &            warnings)
{
    if (group.empty() && aff_token.empty() && job_statuses.empty()) {
        // This possible if there was only 'Canceled' status and
        // it was filtered out. A warning for this case is already produced
        return 0;
    }

    TNSBitVector        jobs_to_cancel;
    vector<TJobStatus>  statuses;

    if (job_statuses.empty()) {
        // All statuses
        statuses.push_back(CNetScheduleAPI::ePending);
        statuses.push_back(CNetScheduleAPI::eRunning);
        statuses.push_back(CNetScheduleAPI::eCanceled);
        statuses.push_back(CNetScheduleAPI::eFailed);
        statuses.push_back(CNetScheduleAPI::eDone);
        statuses.push_back(CNetScheduleAPI::eReading);
        statuses.push_back(CNetScheduleAPI::eConfirmed);
        statuses.push_back(CNetScheduleAPI::eReadFailed);
    }
    else {
        // The user specified statuses explicitly
        // The list validity is checked by the caller.
        statuses = job_statuses;
    }

    CFastMutexGuard     guard(m_OperationLock);
    m_StatusTracker.GetJobs(statuses, jobs_to_cancel);

    if (!group.empty()) {
        try {
            jobs_to_cancel &= m_GroupRegistry.GetJobs(group);
        } catch (...) {
            jobs_to_cancel.clear();
            warnings.push_back("eGroupNotFound:job group " + group +
                               " is not found");
            if (logging)
                ERR_POST(Warning << "Job group '" + group +
                                    "' is not found. No jobs are canceled.");
        }
    }

    if (!aff_token.empty()) {
        unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
        if (aff_id == 0) {
            jobs_to_cancel.clear();
            warnings.push_back("eAffinityNotFound:affinity " + aff_token +
                               " is not found");
            if (logging)
                ERR_POST(Warning << "Affinity '" + aff_token +
                                    "' is not found. No jobs are canceled.");
        }
        else
            jobs_to_cancel &= m_AffinityRegistry.GetJobsWithAffinity(aff_id);
    }

    return x_CancelJobs(client, jobs_to_cancel, logging);
}


TJobStatus  CQueue::GetJobStatus(unsigned int  job_id) const
{
    return m_StatusTracker.GetStatus(job_id);
}


bool CQueue::IsEmpty() const
{
    CFastMutexGuard     guard(m_OperationLock);
    return !m_StatusTracker.AnyJobs();
}


unsigned int CQueue::GetNextId()
{
    CFastMutexGuard     guard(m_LastIdLock);

    // Job indexes are expected to start from 1,
    // the m_LastId is 0 at the very beginning
    ++m_LastId;
    if (m_LastId >= m_SavedId) {
        m_SavedId += s_ReserveDelta;
        if (m_SavedId < m_LastId) {
            // Overflow for the saved ID
            m_LastId = 1;
            m_SavedId = s_ReserveDelta;
        }
        m_Server->SetJobsStartID(m_QueueName, m_SavedId);
    }
    return m_LastId;
}


// Reserves the given number of the job IDs
unsigned int CQueue::GetNextJobIdForBatch(unsigned int  count)
{
    CFastMutexGuard     guard(m_LastIdLock);

    // Job indexes are expected to start from 1 and be monotonously growing
    unsigned int    start_index = m_LastId;

    m_LastId += count;
    if (m_LastId < start_index ) {
        // Overflow
        m_LastId = count;
        m_SavedId = count + s_ReserveDelta;
        m_Server->SetJobsStartID(m_QueueName, m_SavedId);
    }

    // There were no overflow, check the reserved value
    if (m_LastId >= m_SavedId) {
        m_SavedId += s_ReserveDelta;
        if (m_SavedId < m_LastId) {
            // Overflow for the saved ID
            m_LastId = count;
            m_SavedId = count + s_ReserveDelta;
        }
        m_Server->SetJobsStartID(m_QueueName, m_SavedId);
    }

    return m_LastId - count + 1;
}


bool
CQueue::GetJobForReadingOrWait(const CNSClientId &       client,
                               unsigned int              port,
                               unsigned int              timeout,
                               const list<string> *      aff_list,
                               bool                      reader_affinity,
                               bool                      any_affinity,
                               bool                      exclusive_new_affinity,
                               bool                      prioritized_aff,
                               const list<string> *      group_list,
                               bool                      affinity_may_change,
                               bool                      group_may_change,
                               CJob *                    job,
                               bool *                    no_more_jobs,
                               CNSRollbackInterface * &  rollback_action,
                               string &                  added_pref_aff)
{
    CFastMutexGuard         guard(m_OperationLock);
    CNSPreciseTime          curr = CNSPreciseTime::Current();
    TNSBitVector            group_ids_vector;
    bool                    has_groups = false;
    TNSBitVector            aff_ids_vector;
    vector<unsigned int>    aff_ids;

    // This is a reader command, so mark the node type as a reader
    m_ClientsRegistry.AppendType(client, CNSClient::eReader);

    *no_more_jobs = false;

    {{

        if (reader_affinity) {
            // Check that the preferred affinities were not reset
            if (m_ClientsRegistry.GetAffinityReset(client, eRead))
                return false;

            // Check that the client was garbage collected with preferred affs
            if (m_ClientsRegistry.WasGarbageCollected(client, eRead))
                return false;
        }

        // Resolve affinities and groups. It is supposed that the client knows
        // better what affinities and groups to expect i.e. even if they do not
        // exist yet, they may appear soon.
        if (group_list != NULL) {
            m_GroupRegistry.ResolveGroups(*group_list, group_ids_vector);
            has_groups = !group_list->empty();
        }
        if (aff_list != NULL)
            m_AffinityRegistry.ResolveAffinities(*aff_list, aff_ids_vector,
                                                 aff_ids);

        m_ClientsRegistry.CancelWaiting(client, eRead);
    }}

    for (;;) {
        // Old comment:
        // No lock here to make it possible to pick a job
        // simultaneously from many threads
        // Current state:
        // The lock is taken at the beginning. Now there is not much of a
        // concurrency so a bit of performance is not needed anymore.
        // The rest is left untouched to simplify the changes
        x_SJobPick  job_pick = x_FindVacantJob(client,
                                               aff_ids_vector, aff_ids,
                                               reader_affinity,
                                               any_affinity,
                                               exclusive_new_affinity,
                                               prioritized_aff,
                                               group_ids_vector, has_groups,
                                               eRead);

        {{
            bool                outdated_job = false;
            TJobStatus          old_status;

            if (job_pick.job_id == 0) {
                if (exclusive_new_affinity)
                    job_pick = x_FindOutdatedJobForReading(client, 0,
                                                           group_ids_vector);

                if (job_pick.job_id == 0) {
                    *no_more_jobs = x_NoMoreReadJobs(client, aff_ids_vector,
                                                 reader_affinity, any_affinity,
                                                 exclusive_new_affinity,
                                                 group_ids_vector,
                                                 affinity_may_change,
                                                 group_may_change);
                    if (timeout != 0 && port > 0)
                        x_RegisterReadListener(client, port, timeout,
                                           aff_ids_vector,
                                           reader_affinity, any_affinity,
                                           exclusive_new_affinity,
                                           group_ids_vector);
                    return true;
                }
                outdated_job = true;
            } else {
                // Check that the job is still Done/Failed/Canceled
                // it could be grabbed by another reader or GC
                old_status = GetJobStatus(job_pick.job_id);
                if (old_status != CNetScheduleAPI::eDone &&
                    old_status != CNetScheduleAPI::eFailed &&
                    old_status != CNetScheduleAPI::eCanceled)
                    continue;   // try to pick another job

                if (exclusive_new_affinity) {
                    if (m_GCRegistry.IsOutdatedJob(
                                    job_pick.job_id, eRead,
                                    m_MaxPendingReadWaitTimeout) == false) {
                        x_SJobPick  outdated_pick =
                                        x_FindOutdatedJobForReading(
                                                client, job_pick.job_id,
                                                group_ids_vector);
                        if (outdated_pick.job_id != 0) {
                            job_pick = outdated_pick;
                            outdated_job = true;
                        }
                    }
                }
            }

            // The job is still in acceptable state. Check if it was received
            // with exclusive affinity
            if (job_pick.exclusive && job_pick.aff_id != 0 &&
                outdated_job == false) {
                if (m_ClientsRegistry.IsPreferredByAny(job_pick.aff_id, eRead))
                    continue;   // Other reader grabbed this affinity already

                string  aff_token = m_AffinityRegistry.GetTokenByID(
                                                            job_pick.aff_id);
                // CXX-8843: The '-' affinity must not be added to the list of
                // preferred affinities
                if (aff_token != k_NoAffinityToken) {
                    bool added = m_ClientsRegistry.UpdatePreferredAffinities(
                                            client, job_pick.aff_id, 0, eRead);
                    if (added)
                        added_pref_aff = aff_token;
                }
            }

            if (outdated_job && job_pick.aff_id != 0) {
                string  aff_token = m_AffinityRegistry.GetTokenByID(
                                                            job_pick.aff_id);
                // CXX-8843: The '-' affinity must not be added to the list of
                // preferred affinities
                if (aff_token != k_NoAffinityToken) {
                    bool added = m_ClientsRegistry.
                                    UpdatePreferredAffinities(
                                            client, job_pick.aff_id, 0, eRead);
                    if (added)
                        added_pref_aff = aff_token;
                }
            }

            old_status = GetJobStatus(job_pick.job_id);
            x_UpdateDB_ProvideJobNoLock(client, curr, job_pick.job_id,
                                        eRead, *job);
            m_StatusTracker.SetStatus(job_pick.job_id,
                                      CNetScheduleAPI::eReading);

            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::eReading);
            g_DoPerfLogging(*this, *job, 200);

            if (outdated_job)
                m_StatisticsCounters.CountOutdatedPick(eRead);

            m_GCRegistry.UpdateLifetime(job_pick.job_id,
                                        job->GetExpirationTime(m_Timeout,
                                                               m_RunTimeout,
                                                               m_ReadTimeout,
                                                               m_PendingTimeout,
                                                               curr));
            TimeLineAdd(job_pick.job_id, curr + m_ReadTimeout);
            m_ClientsRegistry.RegisterJob(client, job_pick.job_id, eRead);

            x_NotifyJobChanges(*job, MakeJobKey(job_pick.job_id),
                               eStatusChanged, curr);

            rollback_action = new CNSReadJobRollback(client, job_pick.job_id,
                                                     old_status);
            m_ReadJobs.set_bit(job_pick.job_id);
            ++m_ReadJobsOps;
            return true;
        }}
    }
    return true;    // unreachable
}


TJobStatus  CQueue::ConfirmReadingJob(const CNSClientId &  client,
                                      unsigned int         job_id,
                                      const string &       job_key,
                                      CJob &               job,
                                      const string &       auth_token)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, "",
                                                CNetScheduleAPI::eConfirmed,
                                                false, false);
    m_ClientsRegistry.UnregisterJob(client, job_id, eRead);
    return old_status;
}


TJobStatus  CQueue::FailReadingJob(const CNSClientId &  client,
                                   unsigned int         job_id,
                                   const string &       job_key,
                                   CJob &               job,
                                   const string &       auth_token,
                                   const string &       err_msg,
                                   bool                 no_retries)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, err_msg,
                                                CNetScheduleAPI::eReadFailed,
                                                false, no_retries);
    m_ClientsRegistry.MoveJobToBlacklist(job_id, eRead);
    return old_status;
}


TJobStatus  CQueue::ReturnReadingJob(const CNSClientId &  client,
                                     unsigned int         job_id,
                                     const string &       job_key,
                                     CJob &               job,
                                     const string &       auth_token,
                                     bool                 is_ns_rollback,
                                     bool                 blacklist,
                                     TJobStatus           target_status)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, "",
                                                target_status,
                                                is_ns_rollback,
                                                false);
    if (is_ns_rollback || blacklist == false)
        m_ClientsRegistry.UnregisterJob(job_id, eRead);
    else
        m_ClientsRegistry.MoveJobToBlacklist(job_id, eRead);
    return old_status;
}


TJobStatus  CQueue::RereadJob(const CNSClientId &     client,
                              unsigned int            job_id,
                              const string &          job_key,
                              CJob &                  job,
                              bool &                  no_op)
{
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status == CNetScheduleAPI::eJobNotFound ||
        old_status == CNetScheduleAPI::ePending ||
        old_status == CNetScheduleAPI::eRunning ||
        old_status == CNetScheduleAPI::eReading)
        return old_status;

    if (old_status == CNetScheduleAPI::eFailed ||
        old_status == CNetScheduleAPI::eDone) {
        no_op = true;
        return old_status;
    }

    // Check that the job has been read already
    if (!m_ReadJobs.get_bit(job_id)) {
        no_op = true;
        return old_status;
    }

    TJobStatus      state_before_read = CNetScheduleAPI::eJobNotFound;
    auto            job_iter = m_Jobs.find(job_id);

    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Error fetching job");

    const vector<CJobEvent>&    job_events = job_iter->second.GetEvents();
    if (job_events.empty())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Inconsistency: a job has no events");

    state_before_read = job_iter->second.GetStatusBeforeReading();

    CJobEvent *     event = &job_iter->second.AppendEvent();
    event->SetNodeAddr(client.GetAddress());
    event->SetStatus(state_before_read);
    event->SetEvent(CJobEvent::eReread);
    event->SetTimestamp(current_time);
    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());

    job_iter->second.SetStatus(state_before_read);
    job_iter->second.SetLastTouch(current_time);

    m_StatusTracker.SetStatus(job_id, state_before_read);
    m_StatisticsCounters.CountReread(old_status, state_before_read);
    g_DoPerfLogging(*this, job_iter->second, 200);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout,
                                                   current_time));

    x_NotifyJobChanges(job_iter->second, job_key, eStatusChanged, current_time);

    // Notify the readers
    m_NotificationsList.Notify(job_id, job_iter->second.GetAffinityId(),
                               m_ClientsRegistry,
                               m_AffinityRegistry,
                               m_GroupRegistry,
                               m_ScopeRegistry,
                               m_NotifHifreqPeriod,
                               m_HandicapTimeout,
                               eRead);

    m_ReadJobs.set_bit(job_id, false);
    ++m_ReadJobsOps;

    job = job_iter->second;
    return old_status;
}


TJobStatus  CQueue::x_ChangeReadingStatus(const CNSClientId &  client,
                                          unsigned int         job_id,
                                          const string &       job_key,
                                          CJob &               job,
                                          const string &       auth_token,
                                          const string &       err_msg,
                                          TJobStatus           target_status,
                                          bool                 is_ns_rollback,
                                          bool                 no_retries)
{
    CNSPreciseTime                              current_time =
                                                    CNSPreciseTime::Current();
    CStatisticsCounters::ETransitionPathOption  path_option =
                                                    CStatisticsCounters::eNone;
    CFastMutexGuard                             guard(m_OperationLock);
    TJobStatus                                  old_status =
                                                    GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eReading)
        return old_status;

    auto            job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    // Check that authorization token matches
    if (is_ns_rollback == false) {
        CJob::EAuthTokenCompareResult   token_compare_result =
                                job_iter->second.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");
    }

    // Sanity check of the current job state
    if (job_iter->second.GetStatus() != CNetScheduleAPI::eReading)
        NCBI_THROW(CNetScheduleException, eInternalError,
                "Internal inconsistency detected. The job state in memory is " +
                CNetScheduleAPI::StatusToString(CNetScheduleAPI::eReading) +
                " while in database it is " +
                CNetScheduleAPI::StatusToString(job_iter->second.GetStatus()));

    if (target_status == CNetScheduleAPI::eJobNotFound)
        target_status = job_iter->second.GetStatusBeforeReading();


    // Add an event
    CJobEvent &     event = job_iter->second.AppendEvent();
    event.SetTimestamp(current_time);
    event.SetNodeAddr(client.GetAddress());
    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());
    event.SetErrorMsg(err_msg);

    if (is_ns_rollback) {
        event.SetEvent(CJobEvent::eNSReadRollback);
        job_iter->second.SetReadCount(job_iter->second.GetReadCount() - 1);
    } else {
        switch (target_status) {
            case CNetScheduleAPI::eFailed:
            case CNetScheduleAPI::eDone:
            case CNetScheduleAPI::eCanceled:
                event.SetEvent(CJobEvent::eReadRollback);
                job_iter->second.SetReadCount(job_iter->second.GetReadCount() - 1);
                break;
            case CNetScheduleAPI::eReadFailed:
                if (no_retries) {
                    event.SetEvent(CJobEvent::eReadFinalFail);
                } else {
                    event.SetEvent(CJobEvent::eReadFail);
                    // Check the number of tries first
                    if (job_iter->second.GetReadCount() <= m_ReadFailedRetries) {
                        // The job needs to be re-scheduled for reading
                        target_status = CNetScheduleAPI::eDone;
                        path_option = CStatisticsCounters::eFail;
                    }
                }
                break;
            case CNetScheduleAPI::eConfirmed:
                event.SetEvent(CJobEvent::eReadDone);
                break;
            default:
                _ASSERT(0);
                break;
        }
    }

    event.SetStatus(target_status);
    job_iter->second.SetStatus(target_status);
    job_iter->second.SetLastTouch(current_time);

    if (target_status != CNetScheduleAPI::eConfirmed &&
        target_status != CNetScheduleAPI::eReadFailed) {
        m_ReadJobs.set_bit(job_id, false);
        ++m_ReadJobsOps;

        m_GCRegistry.UpdateReadVacantTime(job_id, current_time);

        // Notify the readers
        m_NotificationsList.Notify(
            job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
            m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
            m_NotifHifreqPeriod, m_HandicapTimeout, eRead);
    }

    TimeLineRemove(job_id);

    m_StatusTracker.SetStatus(job_id, target_status);
    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout, m_PendingTimeout,
                                                   current_time));
    if (is_ns_rollback)
        m_StatisticsCounters.CountNSReadRollback(1);
    else
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eReading,
                                             target_status,
                                             path_option);
    g_DoPerfLogging(*this, job_iter->second, 200);
    x_NotifyJobChanges(job_iter->second, job_key, eStatusChanged, current_time);

    job = job_iter->second;
    return CNetScheduleAPI::eReading;
}


// This function is called from places where the operations lock has been
// already taken. So there is no lock around memory status tracker
void CQueue::EraseJob(unsigned int  job_id, TJobStatus  status)
{
    m_StatusTracker.Erase(job_id);

    {{
        // Request delayed record delete
        CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

        m_JobsToDelete.set_bit(job_id);
        ++m_JobsToDeleteOps;
    }}
    TimeLineRemove(job_id);
    m_StatisticsCounters.CountTransitionToDeleted(status, 1);
}


// status - the job status from which the job was deleted
void CQueue::x_Erase(const TNSBitVector &  job_ids, TJobStatus  status)
{
    size_t              job_count = job_ids.count();
    if (job_count <= 0)
        return;

    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

    m_JobsToDelete |= job_ids;
    m_JobsToDeleteOps += job_count;
    m_StatisticsCounters.CountTransitionToDeleted(status, job_count);
}


void CQueue::OptimizeMem()
{
    m_StatusTracker.OptimizeMem();
}


CQueue::x_SJobPick
CQueue::x_FindVacantJob(const CNSClientId  &          client,
                        const TNSBitVector &          explicit_affs,
                        const vector<unsigned int> &  aff_ids,
                        bool                          use_pref_affinity,
                        bool                          any_affinity,
                        bool                          exclusive_new_affinity,
                        bool                          prioritized_aff,
                        const TNSBitVector &          group_ids,
                        bool                          has_groups,
                        ECommandGroup                 cmd_group)
{
    string          scope = client.GetScope();
    string          virtual_scope = client.GetVirtualScope();

    if (!virtual_scope.empty()) {
        // Try first this scope: see CXX-5324
        x_SJobPick  job_pick = x_FindVacantJob(client, explicit_affs,
                                               aff_ids, use_pref_affinity,
                                               any_affinity,
                                               exclusive_new_affinity,
                                               prioritized_aff,
                                               group_ids, has_groups,
                                               cmd_group, virtual_scope);
        if (job_pick.job_id != 0)
            return job_pick;

        // Fallback to a regular pick
    }

    return x_FindVacantJob(client, explicit_affs, aff_ids, use_pref_affinity,
                           any_affinity, exclusive_new_affinity,
                           prioritized_aff, group_ids, has_groups,
                           cmd_group, scope);
}


CQueue::x_SJobPick
CQueue::x_FindVacantJob(const CNSClientId  &          client,
                        const TNSBitVector &          explicit_affs,
                        const vector<unsigned int> &  aff_ids,
                        bool                          use_pref_affinity,
                        bool                          any_affinity,
                        bool                          exclusive_new_affinity,
                        bool                          prioritized_aff,
                        const TNSBitVector &          group_ids,
                        bool                          has_groups,
                        ECommandGroup                 cmd_group,
                        const string &                scope)
{
    bool            explicit_aff = !aff_ids.empty();
    bool            effective_use_pref_affinity = use_pref_affinity;
    TNSBitVector    pref_aff_candidate_jobs;
    TNSBitVector    exclusive_aff_candidate_jobs;

    // Jobs per client support: CXX-11138 (only for GET)
    map<string, size_t>     running_jobs_per_client;
    if (m_MaxJobsPerClient > 0 && cmd_group == eGet) {
        running_jobs_per_client = x_GetRunningJobsPerClientIP();
    }

    TNSBitVector    pref_aff = m_ClientsRegistry.GetPreferredAffinities(
                                                    client, cmd_group);
    if (use_pref_affinity)
        effective_use_pref_affinity = use_pref_affinity && pref_aff.any();

    if (explicit_aff || effective_use_pref_affinity || exclusive_new_affinity) {
        // Check all vacant jobs: pending jobs for eGet,
        //                        done/failed/cancel jobs for eRead
        TNSBitVector    vacant_jobs;
        if (cmd_group == eGet)
            m_StatusTracker.GetJobs(CNetScheduleAPI::ePending, vacant_jobs);
        else
            m_StatusTracker.GetJobs(m_StatesForRead, vacant_jobs);

        if (scope.empty() || scope == kNoScopeOnly) {
            // Both these cases should consider only the non-scope jobs
            vacant_jobs -= m_ScopeRegistry.GetAllJobsInScopes();
        } else {
            // Consider only the jobs in the particular scope
            vacant_jobs &= m_ScopeRegistry.GetJobs(scope);
        }

        // Exclude blacklisted jobs
        m_ClientsRegistry.SubtractBlacklistedJobs(client, cmd_group,
                                                  vacant_jobs);
        // Keep only the group jobs if the groups are provided
        if (has_groups)
            m_GroupRegistry.RestrictByGroup(group_ids, vacant_jobs);

        // Exclude jobs which have been read or in a process of reading
        if (cmd_group == eRead)
            vacant_jobs -= m_ReadJobs;

        if (prioritized_aff) {
            // The criteria here is a list of explicit affinities
            // (respecting their order) which may be followed by any affinity
            for (vector<unsigned int>::const_iterator  k = aff_ids.begin();
                    k != aff_ids.end(); ++k) {
                TNSBitVector    aff_jobs = m_AffinityRegistry.
                                                    GetJobsWithAffinity(*k);
                TNSBitVector    candidates = vacant_jobs & aff_jobs;
                if (candidates.any()) {
                    // Need to check the running jobs per client ip
                    TNSBitVector::enumerator    en(candidates.first());
                    for (; en.valid(); ++en) {
                        auto            job_id = *en;
                        if (x_ValidateMaxJobsPerClientIP(
                                    job_id, running_jobs_per_client)) {
                            return x_SJobPick(job_id, false, *k);
                        }
                    }
                }
            }
            if (any_affinity) {
                if (vacant_jobs.any()) {
                    // Need to check the running jobs per client ip
                    TNSBitVector::enumerator    en(vacant_jobs.first());
                    for (; en.valid(); ++en) {
                        auto            job_id = *en;
                        if (x_ValidateMaxJobsPerClientIP(
                                    job_id, running_jobs_per_client)) {
                            return x_SJobPick(job_id, false,
                                              m_GCRegistry.GetAffinityID(job_id));
                        }
                    }
                }
            }
            return x_SJobPick();
        }

        // HERE: no prioritized affinities
        TNSBitVector    all_pref_aff;
        if (exclusive_new_affinity)
            all_pref_aff = m_ClientsRegistry.GetAllPreferredAffinities(
                                                                    cmd_group);

        TNSBitVector::enumerator    en(vacant_jobs.first());
        for (; en.valid(); ++en) {
            unsigned int    job_id = *en;

            unsigned int    aff_id = m_GCRegistry.GetAffinityID(job_id);
            if (aff_id != 0 && explicit_aff) {
                if (explicit_affs.get_bit(aff_id)) {
                    if (x_ValidateMaxJobsPerClientIP(
                                job_id, running_jobs_per_client)) {
                        return x_SJobPick(job_id, false, aff_id);
                    }
                }
            }

            if (aff_id != 0 && effective_use_pref_affinity) {
                if (pref_aff.get_bit(aff_id)) {
                    if (explicit_aff == false) {
                        if (x_ValidateMaxJobsPerClientIP(
                                job_id, running_jobs_per_client)) {
                            return x_SJobPick(job_id, false, aff_id);
                        }
                    }

                    pref_aff_candidate_jobs.set_bit(job_id);
                    continue;
                }
            }

            if (exclusive_new_affinity) {
                if (aff_id == 0 || all_pref_aff.get_bit(aff_id) == false) {
                    if (explicit_aff == false &&
                        effective_use_pref_affinity == false) {
                        if (x_ValidateMaxJobsPerClientIP(
                                    job_id, running_jobs_per_client)) {
                            return x_SJobPick(job_id, true, aff_id);
                        }
                    }

                    exclusive_aff_candidate_jobs.set_bit(job_id);
                }
            }
        } // end for

        TNSBitVector::enumerator  en1(pref_aff_candidate_jobs.first());
        for (; en1.valid(); ++en1) {
            if (x_ValidateMaxJobsPerClientIP(*en1, running_jobs_per_client)) {
                return x_SJobPick(*en1, false, 0);
            }
        }

        TNSBitVector::enumerator  en2(exclusive_aff_candidate_jobs.first());
        for (; en2.valid(); ++en2) {
            if (x_ValidateMaxJobsPerClientIP(*en2, running_jobs_per_client)) {
                return x_SJobPick(*en2, true, m_GCRegistry.GetAffinityID(*en2));
            }
        }
    }

    // The second condition looks strange and it covers a very specific
    // scenario: some (older) worker nodes may originally come with the only
    // flag set - use preferred affinities - while they have nothing in the
    // list of preferred affinities yet. In this case a first pending job
    // should be provided.
    if (any_affinity ||
        (!explicit_aff &&
         use_pref_affinity && !effective_use_pref_affinity &&
         !exclusive_new_affinity &&
         cmd_group == eGet)) {

        TNSBitVector    jobs_in_scope;
        TNSBitVector    restricted_jobs;
        bool            no_scope_only = scope.empty() ||
                                        scope == kNoScopeOnly;
        unsigned int    job_id = 0;

        if (no_scope_only)
            jobs_in_scope = m_ScopeRegistry.GetAllJobsInScopes();
        else {
            restricted_jobs = m_ScopeRegistry.GetJobs(scope);
            if (has_groups)
                m_GroupRegistry.RestrictByGroup(group_ids, restricted_jobs);
        }

        if (cmd_group == eGet) {
            // NOTE: this only to avoid an expensive temporary bvector
            m_ClientsRegistry.AddBlacklistedJobs(client, cmd_group,
                                                 jobs_in_scope);

            TNSBitVector    pending_jobs;
            m_StatusTracker.GetJobs(CNetScheduleAPI::ePending, pending_jobs);
            TNSBitVector::enumerator    en = pending_jobs.first();

            if (no_scope_only) {
                // only the jobs which are not in the scope
                if (has_groups) {
                    TNSBitVector    group_jobs = m_GroupRegistry.GetJobs(group_ids);
                    for (; en.valid(); ++en) {
                        unsigned int    candidate_job_id = *en;
                        if (jobs_in_scope.get_bit(candidate_job_id))
                            continue;
                        if (!group_jobs.get_bit(candidate_job_id))
                            continue;
                        if (x_ValidateMaxJobsPerClientIP(candidate_job_id,
                                                         running_jobs_per_client)) {
                            job_id = candidate_job_id;
                            break;
                        }
                    }
                } else {
                    for (; en.valid(); ++en) {
                        unsigned int    candidate_job_id = *en;
                        if (jobs_in_scope.get_bit(candidate_job_id))
                            continue;
                        if (x_ValidateMaxJobsPerClientIP(candidate_job_id,
                                                         running_jobs_per_client)) {
                            job_id = candidate_job_id;
                            break;
                        }
                    }
                }
            } else {
                // only the specific scope jobs
                for (; en.valid(); ++en) {
                    unsigned int    candidate_job_id = *en;
                    if (jobs_in_scope.get_bit(candidate_job_id))
                        continue;
                    if (!restricted_jobs.get_bit(candidate_job_id))
                        continue;
                    if (x_ValidateMaxJobsPerClientIP(candidate_job_id,
                                                     running_jobs_per_client)) {
                        job_id = candidate_job_id;
                        break;
                    }
                }
            }
        } else {
            if (no_scope_only) {
                // only the jobs which are not in the scope

                // NOTE: this only to avoid an expensive temporary bvector
                jobs_in_scope |= m_ReadJobs;
                m_ClientsRegistry.AddBlacklistedJobs(client, cmd_group,
                                                     jobs_in_scope);
                if (has_groups)
                    job_id = m_StatusTracker.GetJobByStatus(
                                    m_StatesForRead,
                                    jobs_in_scope,
                                    m_GroupRegistry.GetJobs(group_ids),
                                    has_groups);
                else
                    job_id = m_StatusTracker.GetJobByStatus(
                                    m_StatesForRead,
                                    jobs_in_scope,
                                    kEmptyBitVector,
                                    false);
            } else {
                // only the specific scope jobs

                // NOTE: this only to avoid an expensive temporary bvector
                jobs_in_scope = m_ReadJobs;
                m_ClientsRegistry.AddBlacklistedJobs(client, cmd_group,
                                                     jobs_in_scope);
                job_id = m_StatusTracker.GetJobByStatus(
                                            m_StatesForRead,
                                            jobs_in_scope,
                                            restricted_jobs, true);
            }
        }
        return x_SJobPick(job_id, false, 0);
    }

    return x_SJobPick();
}

// Provides a map between the client IP and the number of running jobs
map<string, size_t> CQueue::x_GetRunningJobsPerClientIP(void)
{
    map<string, size_t>     ret;
    TNSBitVector            running_jobs;

    m_StatusTracker.GetJobs(CNetScheduleAPI::eRunning, running_jobs);
    TNSBitVector::enumerator    en(running_jobs.first());
    for (; en.valid(); ++en) {
        auto            job_iter = m_Jobs.find(*en);
        if (job_iter != m_Jobs.end()) {
            string  client_ip = job_iter->second.GetClientIP();
            auto    iter = ret.find(client_ip);
            if (iter == ret.end()) {
                ret[client_ip] = 1;
            } else {
                iter->second += 1;
            }
        }
    }
    return ret;
}


bool
CQueue::x_ValidateMaxJobsPerClientIP(
                        unsigned int  job_id,
                        const map<string, size_t> &  jobs_per_client_ip) const
{
    if (jobs_per_client_ip.empty())
        return true;

    auto    job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        return true;

    string  client_ip = job_iter->second.GetClientIP();
    auto    iter = jobs_per_client_ip.find(client_ip);
    if (iter == jobs_per_client_ip.end())
        return true;
    return iter->second < m_MaxJobsPerClient;
}


CQueue::x_SJobPick
CQueue::x_FindOutdatedPendingJob(const CNSClientId &   client,
                                 unsigned int          picked_earlier,
                                 const TNSBitVector &  group_ids)
{
    if (m_MaxPendingWaitTimeout == kTimeZero)
        return x_SJobPick();    // Not configured

    string      scope = client.GetScope();
    string      virtual_scope = client.GetVirtualScope();

    if (!virtual_scope.empty()) {
        // Try first this scope: see CXX-5324
        x_SJobPick  job_pick = x_FindOutdatedPendingJob(client, picked_earlier,
                                                        group_ids,
                                                        virtual_scope);
        if (job_pick.job_id != 0)
            return job_pick;

        // Fallback to a regular outdated pick
    }

    return x_FindOutdatedPendingJob(client, picked_earlier,
                                    group_ids, scope);
}


CQueue::x_SJobPick
CQueue::x_FindOutdatedPendingJob(const CNSClientId &   client,
                                 unsigned int          picked_earlier,
                                 const TNSBitVector &  group_ids,
                                 const string &        scope)
{
    TNSBitVector    outdated_pending =
                        m_StatusTracker.GetOutdatedPendingJobs(
                                m_MaxPendingWaitTimeout,
                                m_GCRegistry);
    if (picked_earlier != 0)
        outdated_pending.set_bit(picked_earlier, false);

    m_ClientsRegistry.SubtractBlacklistedJobs(client, eGet, outdated_pending);

    if (scope.empty() || scope == kNoScopeOnly)
        outdated_pending -= m_ScopeRegistry.GetAllJobsInScopes();
    else
        outdated_pending &= m_ScopeRegistry.GetJobs(scope);

    if (group_ids.any())
        m_GroupRegistry.RestrictByGroup(group_ids, outdated_pending);

    if (!outdated_pending.any())
        return x_SJobPick();


    x_SJobPick      job_pick;
    job_pick.job_id = *outdated_pending.first();
    job_pick.aff_id = m_GCRegistry.GetAffinityID(job_pick.job_id);
    job_pick.exclusive = job_pick.aff_id != 0;
    return job_pick;
}


CQueue::x_SJobPick
CQueue::x_FindOutdatedJobForReading(const CNSClientId &  client,
                                    unsigned int  picked_earlier,
                                    const TNSBitVector &  group_ids)
{
    if (m_MaxPendingReadWaitTimeout == kTimeZero)
        return x_SJobPick();    // Not configured

    string      scope = client.GetScope();
    string      virtual_scope = client.GetVirtualScope();

    if (!virtual_scope.empty()) {
        // Try first this scope: see CXX-5324
        x_SJobPick  job_pick = x_FindOutdatedJobForReading(client,
                                                           picked_earlier,
                                                           group_ids,
                                                           virtual_scope);
        if (job_pick.job_id != 0)
            return job_pick;

        // Fallback to a regular outdated pick
    }

    return x_FindOutdatedJobForReading(client, picked_earlier,
                                       group_ids, scope);
}


CQueue::x_SJobPick
CQueue::x_FindOutdatedJobForReading(const CNSClientId &  client,
                                    unsigned int  picked_earlier,
                                    const TNSBitVector &  group_ids,
                                    const string &        scope)
{
    TNSBitVector    outdated_read_jobs =
                        m_StatusTracker.GetOutdatedReadVacantJobs(
                                m_MaxPendingReadWaitTimeout,
                                m_ReadJobs, m_GCRegistry);
    if (picked_earlier != 0)
        outdated_read_jobs.set_bit(picked_earlier, false);

    m_ClientsRegistry.SubtractBlacklistedJobs(client, eRead,
                                              outdated_read_jobs);

    if (scope.empty() || scope == kNoScopeOnly)
        outdated_read_jobs -= m_ScopeRegistry.GetAllJobsInScopes();
    else
        outdated_read_jobs &= m_ScopeRegistry.GetJobs(scope);

    if (group_ids.any())
        m_GroupRegistry.RestrictByGroup(group_ids, outdated_read_jobs);

    if (!outdated_read_jobs.any())
        return x_SJobPick();

    unsigned int    job_id = *outdated_read_jobs.first();
    unsigned int    aff_id = m_GCRegistry.GetAffinityID(job_id);
    return x_SJobPick(job_id, aff_id != 0, aff_id);
}


TJobStatus CQueue::FailJob(const CNSClientId &    client,
                           unsigned int           job_id,
                           const string &         job_key,
                           CJob &                 job,
                           const string &         auth_token,
                           const string &         err_msg,
                           const string &         output,
                           int                    ret_code,
                           bool                   no_retries,
                           string                 warning)
{
    unsigned        failed_retries;
    unsigned        max_output_size;
    {{
        CQueueParamAccessor     qp(*this);
        failed_retries  = qp.GetFailedRetries();
        max_output_size = qp.GetMaxOutputSize();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");
    }

    CNSPreciseTime      curr = CNSPreciseTime::Current();
    bool                rescheduled = false;
    TJobStatus          old_status;

    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          new_status = CNetScheduleAPI::eFailed;

    old_status = GetJobStatus(job_id);
    if (old_status == CNetScheduleAPI::eFailed) {
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eFailed,
                                             CNetScheduleAPI::eFailed);
        return old_status;
    }

    if (old_status != CNetScheduleAPI::eRunning) {
        // No job state change
        return old_status;
    }

    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Error fetching job");

    if (!auth_token.empty()) {
        // Need to check authorization token first
        CJob::EAuthTokenCompareResult   token_compare_result =
                            job_iter->second.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");
        if (token_compare_result == CJob::ePassportOnlyMatch) {
            // That means the job has been given to another worker node
            // by whatever reason (expired/failed/returned before)
            ERR_POST(Warning << "Received FPUT2 with only "
                                "passport matched.");
            warning = "eJobPassportOnlyMatch:Only job passport "
                      "matched. Command is ignored.";
            job = job_iter->second;
            return old_status;
        }
        // Here: the authorization token is OK, we can continue
    }

    CJobEvent *     event = job_iter->second.GetLastEvent();
    if (!event)
        ERR_POST("No JobEvent for running job");

    event = &job_iter->second.AppendEvent();
    if (no_retries)
        event->SetEvent(CJobEvent::eFinalFail);
    else
        event->SetEvent(CJobEvent::eFail);
    event->SetStatus(CNetScheduleAPI::eFailed);
    event->SetTimestamp(curr);
    event->SetErrorMsg(err_msg);
    event->SetRetCode(ret_code);
    event->SetNodeAddr(client.GetAddress());
    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());

    if (no_retries) {
        job_iter->second.SetStatus(CNetScheduleAPI::eFailed);
        event->SetStatus(CNetScheduleAPI::eFailed);
        rescheduled = false;
        if (m_Log)
            ERR_POST(Warning << "Job failed "
                                "unconditionally, no_retries = 1");
    } else {
        unsigned                run_count = job_iter->second.GetRunCount();
        if (run_count <= failed_retries) {
            job_iter->second.SetStatus(CNetScheduleAPI::ePending);
            event->SetStatus(CNetScheduleAPI::ePending);

            new_status = CNetScheduleAPI::ePending;

            rescheduled = true;
        } else {
            job_iter->second.SetStatus(CNetScheduleAPI::eFailed);
            event->SetStatus(CNetScheduleAPI::eFailed);
            new_status = CNetScheduleAPI::eFailed;
            rescheduled = false;
            if (m_Log)
                ERR_POST(Warning << "Job failed, exceeded "
                                    "max number of retries ("
                                 << failed_retries << ")");
        }
    }

    job_iter->second.SetOutput(output);
    job_iter->second.SetLastTouch(curr);

    m_StatusTracker.SetStatus(job_id, new_status);
    if (new_status == CNetScheduleAPI::ePending)
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                             new_status,
                                             CStatisticsCounters::eFail);
    else
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                             new_status,
                                             CStatisticsCounters::eNone);
    g_DoPerfLogging(*this, job_iter->second, 200);

    TimeLineRemove(job_id);

    // Replace it with ClearExecuting(client, job_id) when all clients
    // provide their credentials and job passport is checked strictly
    m_ClientsRegistry.UnregisterJob(job_id, eGet);
    m_ClientsRegistry.RegisterBlacklistedJob(client, job_id, eGet);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout, curr));

    if (rescheduled && m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(
            job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
            m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
            m_NotifHifreqPeriod, m_HandicapTimeout, eGet);

    if (new_status == CNetScheduleAPI::eFailed)
        if (!m_ReadJobs.get_bit(job_id)) {
            m_GCRegistry.UpdateReadVacantTime(job_id, curr);
            m_NotificationsList.Notify(
                job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
                m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
                m_NotifHifreqPeriod, m_HandicapTimeout, eRead);
        }

    x_NotifyJobChanges(job_iter->second, job_key, eStatusChanged, curr);

    job = job_iter->second;
    return old_status;
}


string  CQueue::GetAffinityTokenByID(unsigned int  aff_id) const
{
    return m_AffinityRegistry.GetTokenByID(aff_id);
}


void CQueue::ClearWorkerNode(const CNSClientId &  client,
                             bool &               client_was_found,
                             string &             old_session,
                             bool &               had_wn_pref_affs,
                             bool &               had_reader_pref_affs)
{
    // Get the running and reading jobs and move them to the corresponding
    // states (pending and done)

    TNSBitVector    running_jobs;
    TNSBitVector    reading_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        m_ClientsRegistry.ClearClient(client, running_jobs, reading_jobs,
                                      client_was_found, old_session,
                                      had_wn_pref_affs, had_reader_pref_affs);

    }}

    if (running_jobs.any())
        x_ResetRunningDueToClear(client, running_jobs);
    if (reading_jobs.any())
        x_ResetReadingDueToClear(client, reading_jobs);
    return;
}


// Triggered from a notification thread only
void CQueue::NotifyListenersPeriodically(const CNSPreciseTime &  current_time)
{
    if (m_MaxPendingWaitTimeout != kTimeZero) {
        // Pending outdated timeout is configured, so check outdated jobs
        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        outdated_jobs =
                                    m_StatusTracker.GetOutdatedPendingJobs(
                                        m_MaxPendingWaitTimeout,
                                        m_GCRegistry);
        if (outdated_jobs.any())
            m_NotificationsList.CheckOutdatedJobs(outdated_jobs,
                                                  m_ClientsRegistry,
                                                  m_NotifHifreqPeriod,
                                                  eGet);
    }

    if (m_MaxPendingReadWaitTimeout != kTimeZero) {
        // Read pending timeout is configured, so check read outdated jobs
        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        outdated_jobs =
                                    m_StatusTracker.GetOutdatedReadVacantJobs(
                                        m_MaxPendingReadWaitTimeout,
                                        m_ReadJobs, m_GCRegistry);
        if (outdated_jobs.any())
            m_NotificationsList.CheckOutdatedJobs(outdated_jobs,
                                                  m_ClientsRegistry,
                                                  m_NotifHifreqPeriod,
                                                  eRead);
    }


    // Check the configured notification interval
    static CNSPreciseTime   last_notif_timeout = kTimeNever;
    static size_t           skip_limit = 0;
    static size_t           skip_count;

    if (m_NotifHifreqInterval != last_notif_timeout) {
        last_notif_timeout = m_NotifHifreqInterval;
        skip_count = 0;
        skip_limit = size_t(m_NotifHifreqInterval/0.1);
    }

    ++skip_count;
    if (skip_count < skip_limit)
        return;

    skip_count = 0;

    // The NotifyPeriodically() and CheckTimeout() calls may need to modify
    // the clients and affinity registry so it is safer to take the queue lock.
    CFastMutexGuard     guard(m_OperationLock);
    if (m_StatusTracker.AnyPending())
        m_NotificationsList.NotifyPeriodically(current_time,
                                               m_NotifLofreqMult,
                                               m_ClientsRegistry);
    else
        m_NotificationsList.CheckTimeout(current_time, m_ClientsRegistry, eGet);
}


CNSPreciseTime CQueue::NotifyExactListeners(void)
{
    return m_NotificationsList.NotifyExactListeners();
}


string CQueue::PrintClientsList(bool verbose) const
{
    CFastMutexGuard     guard(m_OperationLock);
    return m_ClientsRegistry.PrintClientsList(this,
                                              m_DumpClientBufferSize, verbose);
}


string CQueue::PrintNotificationsList(bool verbose) const
{
    CFastMutexGuard     guard(m_OperationLock);
    return m_NotificationsList.Print(m_ClientsRegistry, m_AffinityRegistry,
                                     m_GroupRegistry, verbose);
}


string CQueue::PrintAffinitiesList(const CNSClientId &  client,
                                   bool verbose) const
{
    TNSBitVector        scope_jobs;
    string              scope = client.GetScope();
    CFastMutexGuard     guard(m_OperationLock);

    if (scope == kNoScopeOnly)
        scope_jobs = m_ScopeRegistry.GetAllJobsInScopes();
    else if (!scope.empty())
        scope_jobs = m_ScopeRegistry.GetJobs(scope);

    return m_AffinityRegistry.Print(this, m_ClientsRegistry,
                                    scope_jobs, scope,
                                    m_DumpAffBufferSize, verbose);
}


string CQueue::PrintGroupsList(const CNSClientId &  client,
                               bool verbose) const
{
    TNSBitVector        scope_jobs;
    string              scope = client.GetScope();
    CFastMutexGuard     guard(m_OperationLock);

    if (scope == kNoScopeOnly)
        scope_jobs = m_ScopeRegistry.GetAllJobsInScopes();
    else if (!scope.empty())
        scope_jobs = m_ScopeRegistry.GetJobs(scope);

    return m_GroupRegistry.Print(this, scope_jobs, scope,
                                 m_DumpGroupBufferSize, verbose);
}


string CQueue::PrintScopesList(bool verbose) const
{
    CFastMutexGuard     guard(m_OperationLock);
    return m_ScopeRegistry.Print(this, 100, verbose);
}


void CQueue::CheckExecutionTimeout(bool  logging)
{
    if (!m_RunTimeLine)
        return;

    CNSPreciseTime  queue_run_timeout = GetRunTimeout();
    CNSPreciseTime  queue_read_timeout = GetReadTimeout();
    CNSPreciseTime  curr = CNSPreciseTime::Current();
    TNSBitVector    bv;
    {{
        CReadLockGuard  guard(m_RunTimeLineLock);
        m_RunTimeLine->ExtractObjects(curr.Sec(), &bv);
    }}

    TNSBitVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        x_CheckExecutionTimeout(queue_run_timeout, queue_read_timeout,
                                *en, curr, logging);
    }
}


void CQueue::x_CheckExecutionTimeout(const CNSPreciseTime &  queue_run_timeout,
                                     const CNSPreciseTime &  queue_read_timeout,
                                     unsigned int            job_id,
                                     const CNSPreciseTime &  curr_time,
                                     bool                    logging)
{
    CNSPreciseTime                      time_start = kTimeZero;
    CNSPreciseTime                      run_timeout = kTimeZero;
    CNSPreciseTime                      read_timeout = kTimeZero;
    CNSPreciseTime                      exp_time = kTimeZero;
    TJobStatus                          status;
    TJobStatus                          new_status;
    CJobEvent::EJobEvent                event_type;
    map<unsigned int, CJob>::iterator   job_iter;

    {{
        CFastMutexGuard         guard(m_OperationLock);

        status = GetJobStatus(job_id);
        if (status == CNetScheduleAPI::eRunning) {
            new_status = CNetScheduleAPI::ePending;
            event_type = CJobEvent::eTimeout;
        } else if (status == CNetScheduleAPI::eReading) {
            new_status = CNetScheduleAPI::eDone;
            event_type = CJobEvent::eReadTimeout;
        } else
            return; // Execution timeout is for Running and Reading jobs only

        job_iter = m_Jobs.find(job_id);
        if (job_iter == m_Jobs.end())
            return;

        CJobEvent *     event = job_iter->second.GetLastEvent();
        time_start = event->GetTimestamp();
        run_timeout = job_iter->second.GetRunTimeout();
        if (run_timeout == kTimeZero)
            run_timeout = queue_run_timeout;

        if (status == CNetScheduleAPI::eRunning &&
            run_timeout == kTimeZero)
            // 0 timeout means the job never fails
            return;

        read_timeout = job_iter->second.GetReadTimeout();
        if (read_timeout == kTimeZero)
            read_timeout = queue_read_timeout;

        if (status == CNetScheduleAPI::eReading &&
            read_timeout == kTimeZero)
            // 0 timeout means the job never fails
            return;

        // Calculate the expiration time
        if (status == CNetScheduleAPI::eRunning)
            exp_time = time_start + run_timeout;
        else
            exp_time = time_start + read_timeout;

        if (curr_time < exp_time) {
            // we need to register job in time line
            TimeLineAdd(job_id, exp_time);
            return;
        }

        // The job timeout (running or reading) is expired.
        // Check the try counter, we may need to fail the job.
        if (status == CNetScheduleAPI::eRunning) {
            // Running state
            if (job_iter->second.GetRunCount() > m_FailedRetries)
                new_status = CNetScheduleAPI::eFailed;
        } else {
            // Reading state
            if (job_iter->second.GetReadCount() > m_ReadFailedRetries)
                new_status = CNetScheduleAPI::eReadFailed;
            else
                new_status = job_iter->second.GetStatusBeforeReading();
            m_ReadJobs.set_bit(job_id, false);
            ++m_ReadJobsOps;
        }

        job_iter->second.SetStatus(new_status);
        job_iter->second.SetLastTouch(curr_time);

        event = &job_iter->second.AppendEvent();
        event->SetStatus(new_status);
        event->SetEvent(event_type);
        event->SetTimestamp(curr_time);

        m_StatusTracker.SetStatus(job_id, new_status);
        m_GCRegistry.UpdateLifetime(
            job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                       m_ReadTimeout,
                                                       m_PendingTimeout,
                                                       curr_time));

        if (status == CNetScheduleAPI::eRunning) {
            if (new_status == CNetScheduleAPI::ePending) {
                // Timeout and reschedule, put to blacklist as well
                m_ClientsRegistry.MoveJobToBlacklist(job_id, eGet);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            new_status,
                                            CStatisticsCounters::eTimeout);
            } else {
                m_ClientsRegistry.UnregisterJob(job_id, eGet);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            CNetScheduleAPI::eFailed,
                                            CStatisticsCounters::eTimeout);
            }
        } else {
            if (new_status == CNetScheduleAPI::eReadFailed) {
                m_ClientsRegistry.UnregisterJob(job_id, eRead);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            new_status,
                                            CStatisticsCounters::eTimeout);
            } else {
                // The target status could be Done, Failed, Canceled.
                // The job could be read again by another reader.
                m_ClientsRegistry.MoveJobToBlacklist(job_id, eRead);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            new_status,
                                            CStatisticsCounters::eTimeout);
            }
        }
        g_DoPerfLogging(*this, job_iter->second, 200);

        if (new_status == CNetScheduleAPI::ePending &&
            m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(
                job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
                m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
                m_NotifHifreqPeriod, m_HandicapTimeout, eGet);

        if (new_status == CNetScheduleAPI::eDone ||
            new_status == CNetScheduleAPI::eFailed ||
            new_status == CNetScheduleAPI::eCanceled)
            if (!m_ReadJobs.get_bit(job_id)) {
                m_GCRegistry.UpdateReadVacantTime(job_id, curr_time);
                m_NotificationsList.Notify(
                    job_id, job_iter->second.GetAffinityId(), m_ClientsRegistry,
                    m_AffinityRegistry, m_GroupRegistry, m_ScopeRegistry,
                    m_NotifHifreqPeriod, m_HandicapTimeout, eRead);
            }
    }}

    x_NotifyJobChanges(job_iter->second, MakeJobKey(job_id),
                       eStatusChanged, curr_time);

    if (logging) {
        string      purpose;
        if (status == CNetScheduleAPI::eRunning)
            purpose = "execution";
        else
            purpose = "reading";

        GetDiagContext().Extra()
                .Print("msg", "Timeout expired, rescheduled for " + purpose)
                .Print("msg_code", "410")   // The code is for
                                            // searching in applog
                .Print("job_key", MakeJobKey(job_id))
                .Print("queue", m_QueueName)
                .Print("run_counter", job_iter->second.GetRunCount())
                .Print("read_counter", job_iter->second.GetReadCount())
                .Print("time_start", NS_FormatPreciseTime(time_start))
                .Print("exp_time", NS_FormatPreciseTime(exp_time))
                .Print("run_timeout", run_timeout)
                .Print("read_timeout", read_timeout);
    }
}


// Checks up to given # of jobs at the given status for expiration and
// marks up to given # of jobs for deletion.
// Returns the # of performed scans, the # of jobs marked for deletion and
// the last scanned job id.
SPurgeAttributes
CQueue::CheckJobsExpiry(const CNSPreciseTime &  current_time,
                        SPurgeAttributes        attributes,
                        unsigned int            last_job,
                        TJobStatus              status)
{
    TNSBitVector        job_ids;
    SPurgeAttributes    result;
    unsigned int        job_id;
    unsigned int        aff;
    unsigned int        group;

    result.job_id = attributes.job_id;
    result.deleted = 0;
    {{
        CFastMutexGuard     guard(m_OperationLock);

        for (result.scans = 0;
             result.scans < attributes.scans; ++result.scans) {
            job_id = m_StatusTracker.GetNext(status, result.job_id);
            if (job_id == 0)
                break;  // No more jobs in the state
            if (last_job != 0 && job_id >= last_job)
                break;  // The job in the state is above the limit

            result.job_id = job_id;

            if (m_GCRegistry.DeleteIfTimedOut(job_id, current_time,
                                              &aff, &group))
            {
                // The job is expired and needs to be marked for deletion
                m_StatusTracker.Erase(job_id);
                job_ids.set_bit(job_id);
                ++result.deleted;

                // check if the affinity should also be updated
                if (aff != 0)
                    m_AffinityRegistry.RemoveJobFromAffinity(job_id, aff);

                // Check if the group registry should also be updated
                if (group != 0)
                    m_GroupRegistry.RemoveJob(group, job_id);

                // Remove the job from the scope registry if so
                m_ScopeRegistry.RemoveJob(job_id);

                if (result.deleted >= attributes.deleted)
                    break;
            }
        }
    }}

    if (result.deleted > 0) {
        TNSBitVector::enumerator    en(job_ids.first());
        CFastMutexGuard             guard(m_OperationLock);

        for (; en.valid(); ++en) {
            unsigned int    id = *en;
            auto            job_iter = m_Jobs.find(id);

            // if the job is deleted from the pending state then a performance
            // record should be produced. A job full information is also
            // required if the listener expects job notifications.
            if (job_iter != m_Jobs.end()) {
                x_NotifyJobChanges(job_iter->second, MakeJobKey(id),
                                   eJobDeleted, current_time);
                if (status == CNetScheduleAPI::ePending) {
                    g_DoErasePerfLogging(*this, job_iter->second);
                }
            }
        }

        if (!m_StatusTracker.AnyPending())
            m_NotificationsList.ClearExactGetNotifications();
    }

    x_Erase(job_ids, status);
    return result;
}


void CQueue::TimeLineMove(unsigned int            job_id,
                          const CNSPreciseTime &  old_time,
                          const CNSPreciseTime &  new_time)
{
    if (!job_id || !m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->MoveObject(old_time.Sec(), new_time.Sec(), job_id);
}


void CQueue::TimeLineAdd(unsigned int            job_id,
                         const CNSPreciseTime &  job_time)
{
    if (!job_id  ||  !m_RunTimeLine  ||  !job_time)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->AddObject(job_time.Sec(), job_id);
}


void CQueue::TimeLineRemove(unsigned int  job_id)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->RemoveObject(job_id);
}


void CQueue::TimeLineExchange(unsigned int            remove_job_id,
                              unsigned int            add_job_id,
                              const CNSPreciseTime &  new_time)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    if (remove_job_id)
        m_RunTimeLine->RemoveObject(remove_job_id);
    if (add_job_id)
        m_RunTimeLine->AddObject(new_time.Sec(), add_job_id);
}


unsigned int  CQueue::DeleteBatch(unsigned int  max_deleted)
{
    // Copy the vector with deleted jobs
    TNSBitVector    jobs_to_delete;

    {{
         CFastMutexGuard     guard(m_JobsToDeleteLock);
         jobs_to_delete = m_JobsToDelete;
    }}

    static const size_t         chunk_size = 100;
    unsigned int                del_rec = 0;
    TNSBitVector::enumerator    en = jobs_to_delete.first();
    TNSBitVector                deleted_jobs;

    while (en.valid() && del_rec < max_deleted) {
        {{
            CFastMutexGuard     guard(m_OperationLock);

            for (size_t n = 0;
                 en.valid() && n < chunk_size && del_rec < max_deleted;
                 ++en, ++n) {
                unsigned int    job_id = *en;
                size_t          del_count = m_Jobs.erase(job_id);

                if (del_count > 0) {
                    ++del_rec;
                    deleted_jobs.set_bit(job_id);
                }

                // The job might be the one which was given for reading
                // so the garbage should be collected
                m_ReadJobs.set_bit(job_id, false);
                ++m_ReadJobsOps;
            }
        }}
    }

    if (del_rec > 0) {
        m_StatisticsCounters.CountDBDeletion(del_rec);

        {{
            TNSBitVector::enumerator    en = deleted_jobs.first();
            CFastMutexGuard             guard(m_JobsToDeleteLock);
            for (; en.valid(); ++en) {
                m_JobsToDelete.set_bit(*en, false);
                ++m_JobsToDeleteOps;
            }

            if (m_JobsToDeleteOps >= 1000000) {
                m_JobsToDeleteOps = 0;
                m_JobsToDelete.optimize(0, TNSBitVector::opt_free_0);
            }
        }}

        CFastMutexGuard     guard(m_OperationLock);
        if (m_ReadJobsOps >= 1000000) {
            m_ReadJobsOps = 0;
            m_ReadJobs.optimize(0, TNSBitVector::opt_free_0);
        }
    }
    return del_rec;
}


// See CXX-2838 for the description of how affinities garbage collection is
// going  to work.
unsigned int  CQueue::PurgeAffinities(void)
{
    unsigned int            aff_dict_size = m_AffinityRegistry.size();
    SNSRegistryParameters   aff_reg_settings =
                                        m_Server->GetAffRegistrySettings();

    if (aff_dict_size < (aff_reg_settings.low_mark_percentage / 100.0) *
                         aff_reg_settings.max_records)
        // Did not reach the dictionary low mark
        return 0;

    unsigned int    del_limit = aff_reg_settings.high_removal;
    if (aff_dict_size <
            (aff_reg_settings.high_mark_percentage / 100.0) *
             aff_reg_settings.max_records) {
        // Here: check the percentage of the affinities that have no references
        // to them
        unsigned int    candidates_size =
                                    m_AffinityRegistry.CheckRemoveCandidates();

        if (candidates_size <
                (aff_reg_settings.dirt_percentage / 100.0) *
                 aff_reg_settings.max_records)
            // The number of candidates to be deleted is low
            return 0;

        del_limit = aff_reg_settings.low_removal;
    }


    // Here: need to delete affinities from the memory
    return m_AffinityRegistry.CollectGarbage(del_limit);
}


// See CQueue::PurgeAffinities - this one works similar
unsigned int  CQueue::PurgeGroups(void)
{
    unsigned int            group_dict_size = m_GroupRegistry.size();
    SNSRegistryParameters   group_reg_settings =
                                    m_Server->GetGroupRegistrySettings();

    if (group_dict_size < (group_reg_settings.low_mark_percentage / 100.0) *
                           group_reg_settings.max_records)
        // Did not reach the dictionary low mark
        return 0;

    unsigned int    del_limit = group_reg_settings.high_removal;
    if (group_dict_size <
            (group_reg_settings.high_mark_percentage / 100.0) *
             group_reg_settings.max_records) {
        // Here: check the percentage of the groups that have no references
        // to them
        unsigned int    candidates_size =
                                m_GroupRegistry.CheckRemoveCandidates();

        if (candidates_size <
                (group_reg_settings.dirt_percentage / 100.0) *
                 group_reg_settings.max_records)
            // The number of candidates to be deleted is low
            return 0;

        del_limit = group_reg_settings.low_removal;
    }

    // Here: need to delete groups from the memory
    return m_GroupRegistry.CollectGarbage(del_limit);
}


void  CQueue::StaleNodes(const CNSPreciseTime &  current_time)
{
    // Clears the worker nodes affinities if the workers are inactive for
    // the configured timeout
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.StaleNodes(current_time, 
                                 m_WNodeTimeout, m_ReaderTimeout, m_Log);
}


void CQueue::PurgeBlacklistedJobs(void)
{
    m_ClientsRegistry.GCBlacklistedJobs(m_StatusTracker, eGet);
    m_ClientsRegistry.GCBlacklistedJobs(m_StatusTracker, eRead);
}


void  CQueue::PurgeClientRegistry(const CNSPreciseTime &  current_time)
{
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.Purge(current_time,
                            m_ClientRegistryTimeoutWorkerNode,
                            m_ClientRegistryMinWorkerNodes,
                            m_ClientRegistryTimeoutAdmin,
                            m_ClientRegistryMinAdmins,
                            m_ClientRegistryTimeoutSubmitter,
                            m_ClientRegistryMinSubmitters,
                            m_ClientRegistryTimeoutReader,
                            m_ClientRegistryMinReaders,
                            m_ClientRegistryTimeoutUnknown,
                            m_ClientRegistryMinUnknowns, m_Log);
}


string CQueue::PrintJobDbStat(const CNSClientId &  client,
                              unsigned int         job_id,
                              TDumpFields          dump_fields)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    string      job_dump;

    // Check first that the job has not been deleted yet
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        if (m_JobsToDelete.get_bit(job_id))
            return job_dump;
    }}

    string              scope = client.GetScope();
    {{
        CFastMutexGuard     guard(m_OperationLock);

        // Check the scope restrictions
        if (scope == kNoScopeOnly) {
            if (m_ScopeRegistry.GetAllJobsInScopes()[job_id] == true)
                return job_dump;
        } else if (!scope.empty()) {
            if (m_ScopeRegistry.GetJobs(scope)[job_id] == false)
                return job_dump;
        }

        auto    job_iter = m_Jobs.find(job_id);
        if (job_iter == m_Jobs.end())
            return job_dump;

        job_dump.reserve(2048);
        try {
            // GC can remove the job from its registry while the
            // DUMP is in process. If so the job should not be dumped
            // and the exception from m_GCRegistry.GetLifetime() should
            // be suppressed.
            job_dump = job_iter->second.Print(dump_fields,
                                              *this, m_AffinityRegistry,
                                              m_GroupRegistry);
            if (dump_fields & eGCEraseTime)
                job_dump.append("OK:GC erase time: ")
                        .append(NS_FormatPreciseTime(m_GCRegistry.GetLifetime(job_id)))
                        .append(kNewLine);
            if (dump_fields & eScope)
                job_dump.append("OK:scope: '")
                        .append(m_ScopeRegistry.GetJobScope(job_id))
                        .append(1, '\'')
                        .append(kNewLine);
        } catch (...) {}

        return job_dump;
    }}
}


string CQueue::PrintAllJobDbStat(const CNSClientId &         client,
                                 const string &              group,
                                 const string &              aff_token,
                                 const vector<TJobStatus> &  job_statuses,
                                 unsigned int                start_after_job_id,
                                 unsigned int                count,
                                 bool                        order_first,
                                 TDumpFields                 dump_fields,
                                 bool                        logging)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    // Form a bit vector of all jobs to dump
    vector<TJobStatus>      statuses;
    TNSBitVector            jobs_to_dump;

    if (job_statuses.empty()) {
        // All statuses
        statuses.push_back(CNetScheduleAPI::ePending);
        statuses.push_back(CNetScheduleAPI::eRunning);
        statuses.push_back(CNetScheduleAPI::eCanceled);
        statuses.push_back(CNetScheduleAPI::eFailed);
        statuses.push_back(CNetScheduleAPI::eDone);
        statuses.push_back(CNetScheduleAPI::eReading);
        statuses.push_back(CNetScheduleAPI::eConfirmed);
        statuses.push_back(CNetScheduleAPI::eReadFailed);
    }
    else {
        // The user specified statuses explicitly
        // The list validity is checked by the caller.
        statuses = job_statuses;
    }


    {{
        string              scope = client.GetScope();
        CFastMutexGuard     guard(m_OperationLock);
        m_StatusTracker.GetJobs(statuses, jobs_to_dump);

        // Check if a certain group has been specified
        if (!group.empty()) {
            try {
                jobs_to_dump &= m_GroupRegistry.GetJobs(group);
            } catch (...) {
                jobs_to_dump.clear();
                if (logging)
                    ERR_POST(Warning << "Job group '" + group +
                                        "' is not found. No jobs to dump.");
            }
        }

        if (!aff_token.empty()) {
            unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
            if (aff_id == 0) {
                jobs_to_dump.clear();
                if (logging)
                    ERR_POST(Warning << "Affinity '" + aff_token +
                                        "' is not found. No jobs to dump.");
            } else
                jobs_to_dump &= m_AffinityRegistry.GetJobsWithAffinity(aff_id);
        }

        // Apply the scope limits
        if (scope == kNoScopeOnly) {
            jobs_to_dump -= m_ScopeRegistry.GetAllJobsInScopes();
        } else if (!scope.empty()) {
            // This is a specific scope
            jobs_to_dump &= m_ScopeRegistry.GetJobs(scope);
        }
    }}

    return x_DumpJobs(jobs_to_dump, start_after_job_id, count,
                      dump_fields, order_first);
}


string CQueue::x_DumpJobs(const TNSBitVector &    jobs_to_dump,
                          unsigned int            start_after_job_id,
                          unsigned int            count,
                          TDumpFields             dump_fields,
                          bool                    order_first)
{
    if (!jobs_to_dump.any())
        return kEmptyStr;

    // Skip the jobs which should not be dumped
    size_t                      skipped_jobs = 0;
    TNSBitVector::enumerator    en(jobs_to_dump.first());
    while (en.valid() && *en <= start_after_job_id) {
        ++en;
        ++skipped_jobs;
    }

    if (count > 0 && !order_first) {
        size_t      total_jobs = jobs_to_dump.count();
        size_t      jobs_left = total_jobs - skipped_jobs;
        while (jobs_left > count) {
            ++en;
            --jobs_left;
        }
    }

    // Identify the required buffer size for jobs
    size_t      buffer_size = m_DumpBufferSize;
    if (count != 0 && count < buffer_size)
        buffer_size = count;

    string  result;
    result.reserve(2048*buffer_size);

    {{
        vector<CJob>    buffer(buffer_size);
        size_t          read_jobs = 0;
        size_t          printed_count = 0;

        for ( ; en.valid(); ) {
            {{
                CFastMutexGuard     guard(m_OperationLock);

                for ( ; en.valid() && read_jobs < buffer_size; ++en ) {
                    auto        job_iter = m_Jobs.find(*en);
                    if (job_iter != m_Jobs.end()) {
                        buffer[read_jobs] = job_iter->second;
                        ++read_jobs;
                        ++printed_count;

                        if (count != 0)
                            if (printed_count >= count)
                                break;
                    }
                }
            }}

            // Print what was read
            string      one_job;
            one_job.reserve(2048);
            for (size_t  index = 0; index < read_jobs; ++index) {
                one_job.clear();
                try {
                    // GC can remove the job from its registry while the
                    // DUMP is in process. If so the job should not be dumped
                    // and the exception from m_GCRegistry.GetLifetime() should
                    // be suppressed.
                    unsigned int  job_id = buffer[index].GetId();
                    one_job.append(kNewLine)
                           .append(buffer[index].Print(dump_fields,
                                                       *this,
                                                       m_AffinityRegistry,
                                                       m_GroupRegistry));
                    if (dump_fields & eGCEraseTime)
                        one_job.append("OK:GC erase time: ")
                               .append(NS_FormatPreciseTime(
                                          m_GCRegistry.GetLifetime(job_id)))
                               .append(kNewLine);
                    if (dump_fields & eScope)
                        one_job.append("OK:scope: '")
                               .append(m_ScopeRegistry.GetJobScope(job_id))
                               .append(1, '\'')
                               .append(kNewLine);
                    result.append(one_job);
                } catch (...) {}
            }

            if (count != 0)
                if (printed_count >= count)
                    break;

            read_jobs = 0;
        }
    }}

    return result;
}


unsigned CQueue::CountStatus(TJobStatus st) const
{
    return m_StatusTracker.CountStatus(st);
}


void CQueue::StatusStatistics(TJobStatus                status,
                              TNSBitVector::statistics* st) const
{
    m_StatusTracker.StatusStatistics(status, st);
}


string CQueue::MakeJobKey(unsigned int  job_id) const
{
    if (m_ScrambleJobKeys)
        return m_KeyGenerator.GenerateCompoundID(job_id,
                                                 m_Server->GetCompoundIDPool());
    return m_KeyGenerator.Generate(job_id);
}


void CQueue::TouchClientsRegistry(CNSClientId &  client,
                                  bool &         client_was_found,
                                  bool &         session_was_reset,
                                  string &       old_session,
                                  bool &         had_wn_pref_affs,
                                  bool &         had_reader_pref_affs)
{
    TNSBitVector        running_jobs;
    TNSBitVector        reading_jobs;

    {{
        // The client registry may need to make changes in the notification
        // registry, i.e. two mutexes are to be locked. The other threads may
        // visit notifications first and then a client registry i.e. the very
        // same mutexes are locked in a reverse order.
        // To prevent it the operation lock is locked here.
        CFastMutexGuard     guard(m_OperationLock);
        m_ClientsRegistry.Touch(client, running_jobs, reading_jobs,
                                client_was_found, session_was_reset,
                                old_session, had_wn_pref_affs,
                                had_reader_pref_affs);
        guard.Release();
    }}

    if (session_was_reset) {
        if (running_jobs.any())
            x_ResetRunningDueToNewSession(client, running_jobs);
        if (reading_jobs.any())
            x_ResetReadingDueToNewSession(client, reading_jobs);
    }
}


void CQueue::MarkClientAsAdmin(const CNSClientId &  client)
{
    m_ClientsRegistry.MarkAsAdmin(client);
}


void CQueue::RegisterSocketWriteError(const CNSClientId &  client)
{
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.RegisterSocketWriteError(client);
}


void CQueue::SetClientScope(const CNSClientId &  client)
{
    // Memorize the last client scope
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.SetLastScope(client);
}


// Moves the job to Pending/Failed or to Done/ReadFailed
// when event event_type has come
TJobStatus  CQueue::x_ResetDueTo(const CNSClientId &     client,
                                 unsigned int            job_id,
                                 const CNSPreciseTime &  current_time,
                                 TJobStatus              status_from,
                                 CJobEvent::EJobEvent    event_type)
{
    TJobStatus          new_status;
    CFastMutexGuard     guard(m_OperationLock);
    auto                job_iter = m_Jobs.find(job_id);

    if (job_iter == m_Jobs.end()) {
        ERR_POST("Cannot fetch job to reset it due to " <<
                 CJobEvent::EventToString(event_type) <<
                 ". Job: " << DecorateJob(job_id));
        return CNetScheduleAPI::eJobNotFound;
    }

    if (status_from == CNetScheduleAPI::eRunning) {
        // The job was running
        if (job_iter->second.GetRunCount() > m_FailedRetries)
            new_status = CNetScheduleAPI::eFailed;
        else
            new_status = CNetScheduleAPI::ePending;
    } else {
        // The job was reading
        if (job_iter->second.GetReadCount() > m_ReadFailedRetries)
            new_status = CNetScheduleAPI::eReadFailed;
        else
            new_status = job_iter->second.GetStatusBeforeReading();
        m_ReadJobs.set_bit(job_id, false);
        ++m_ReadJobsOps;
    }

    job_iter->second.SetStatus(new_status);
    job_iter->second.SetLastTouch(current_time);

    CJobEvent *     event = &job_iter->second.AppendEvent();
    event->SetStatus(new_status);
    event->SetEvent(event_type);
    event->SetTimestamp(current_time);
    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());

    // Update the memory map
    m_StatusTracker.SetStatus(job_id, new_status);

    // Count the transition and do a performance logging
    if (event_type == CJobEvent::eClear)
        m_StatisticsCounters.CountTransition(status_from, new_status,
                                             CStatisticsCounters::eClear);
    else
        // It is a new session case
        m_StatisticsCounters.CountTransition(status_from, new_status,
                                             CStatisticsCounters::eNewSession);
    g_DoPerfLogging(*this, job_iter->second, 200);

    m_GCRegistry.UpdateLifetime(
        job_id, job_iter->second.GetExpirationTime(m_Timeout, m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout,
                                                   current_time));

    // remove the job from the time line
    TimeLineRemove(job_id);

    // Notify those who wait for the jobs if needed
    if (new_status == CNetScheduleAPI::ePending &&
        m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id, job_iter->second.GetAffinityId(),
                                   m_ClientsRegistry, m_AffinityRegistry,
                                   m_GroupRegistry, m_ScopeRegistry,
                                   m_NotifHifreqPeriod, m_HandicapTimeout,
                                   eGet);
    // Notify readers if they wait for jobs
    if (new_status == CNetScheduleAPI::eDone ||
        new_status == CNetScheduleAPI::eFailed ||
        new_status == CNetScheduleAPI::eCanceled)
        if (!m_ReadJobs.get_bit(job_id)) {
            m_GCRegistry.UpdateReadVacantTime(job_id, current_time);
            m_NotificationsList.Notify(job_id, job_iter->second.GetAffinityId(),
                                       m_ClientsRegistry, m_AffinityRegistry,
                                       m_GroupRegistry, m_ScopeRegistry,
                                       m_NotifHifreqPeriod, m_HandicapTimeout,
                                       eRead);
        }

    x_NotifyJobChanges(job_iter->second, MakeJobKey(job_id),
                       eStatusChanged, current_time);
    return new_status;
}


void CQueue::x_ResetRunningDueToClear(const CNSClientId &   client,
                                      const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node is "
                     "cleared. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetReadingDueToClear(const CNSClientId &   client,
                                      const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node is "
                     "cleared. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetRunningDueToNewSession(const CNSClientId &   client,
                                           const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node "
                     "changed session. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetReadingDueToNewSession(const CNSClientId &   client,
                                           const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node "
                     "changed session. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_RegisterGetListener(const CNSClientId &   client,
                                   unsigned short        port,
                                   unsigned int          timeout,
                                   const TNSBitVector &  aff_ids,
                                   bool                  wnode_aff,
                                   bool                  any_aff,
                                   bool                  exclusive_new_affinity,
                                   bool                  new_format,
                                   const TNSBitVector &  group_ids)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         wnode_aff, any_aff,
                                         exclusive_new_affinity, new_format,
                                         group_ids, eGet);
    if (client.IsComplete())
        m_ClientsRegistry.SetNodeWaiting(client, port,
                                         aff_ids, eGet);
    return;
}


void
CQueue::x_RegisterReadListener(const CNSClientId &   client,
                               unsigned short        port,
                               unsigned int          timeout,
                               const TNSBitVector &  aff_ids,
                               bool                  reader_aff,
                               bool                  any_aff,
                               bool                  exclusive_new_affinity,
                               const TNSBitVector &  group_ids)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         reader_aff, any_aff,
                                         exclusive_new_affinity, true,
                                         group_ids, eRead);
    m_ClientsRegistry.SetNodeWaiting(client, port, aff_ids, eRead);
}


bool CQueue::x_UnregisterGetListener(const CNSClientId &  client,
                                     unsigned short       port)
{
    if (client.IsComplete())
        return m_ClientsRegistry.CancelWaiting(client, eGet);

    if (port > 0) {
        m_NotificationsList.UnregisterListener(client, port, eGet);
        return true;
    }
    return false;
}


void CQueue::PrintStatistics(size_t &  aff_count) const
{
    CStatisticsCounters counters_copy = m_StatisticsCounters;

    // Do not print the server wide statistics the very first time
    CNSPreciseTime      current = CNSPreciseTime::Current();

    if (double(m_StatisticsCountersLastPrintedTimestamp) == 0.0) {
        m_StatisticsCountersLastPrinted = counters_copy;
        m_StatisticsCountersLastPrintedTimestamp = current;
        return;
    }

    // Calculate the delta since the last time
    CNSPreciseTime  delta = current - m_StatisticsCountersLastPrintedTimestamp;

    CRef<CRequestContext>   ctx;
    ctx.Reset(new CRequestContext());
    ctx->SetRequestID();


    CDiagContext &      diag_context = GetDiagContext();

    diag_context.SetRequestContext(ctx);
    CDiagContext_Extra      extra = diag_context.PrintRequestStart();

    size_t      affinities = m_AffinityRegistry.size();
    aff_count += affinities;

    // The member is called only if there is a request context
    extra.Print("_type", "statistics_thread")
         .Print("_queue", GetQueueName())
         .Print("time_interval", NS_FormatPreciseTimeAsSec(delta))
         .Print("affinities", affinities)
         .Print("pending", CountStatus(CNetScheduleAPI::ePending))
         .Print("running", CountStatus(CNetScheduleAPI::eRunning))
         .Print("canceled", CountStatus(CNetScheduleAPI::eCanceled))
         .Print("failed", CountStatus(CNetScheduleAPI::eFailed))
         .Print("done", CountStatus(CNetScheduleAPI::eDone))
         .Print("reading", CountStatus(CNetScheduleAPI::eReading))
         .Print("confirmed", CountStatus(CNetScheduleAPI::eConfirmed))
         .Print("readfailed", CountStatus(CNetScheduleAPI::eReadFailed));
    counters_copy.PrintTransitions(extra);
    counters_copy.PrintDelta(extra, m_StatisticsCountersLastPrinted);
    extra.Flush();

    ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    diag_context.PrintRequestStop();
    ctx.Reset();
    diag_context.SetRequestContext(NULL);

    m_StatisticsCountersLastPrinted = counters_copy;
    m_StatisticsCountersLastPrintedTimestamp = current;
}


void CQueue::PrintJobCounters(void) const
{
    vector<TJobStatus>      statuses;
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    statuses.push_back(CNetScheduleAPI::eCanceled);
    statuses.push_back(CNetScheduleAPI::eFailed);
    statuses.push_back(CNetScheduleAPI::eDone);
    statuses.push_back(CNetScheduleAPI::eReading);
    statuses.push_back(CNetScheduleAPI::eConfirmed);
    statuses.push_back(CNetScheduleAPI::eReadFailed);

    vector<unsigned int>    counters = m_StatusTracker.GetJobCounters(statuses);
    g_DoPerfLogging(*this, statuses, counters);
}


unsigned int CQueue::GetJobsToDeleteCount(void) const
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);
    return m_JobsToDelete.count();
}


string CQueue::PrintTransitionCounters(void) const
{
    string      output;
    output.reserve(4096);
    output.append(m_StatisticsCounters.PrintTransitions())
          .append("OK:garbage_jobs: ")
          .append(to_string(GetJobsToDeleteCount()))
          .append(kNewLine)
          .append("OK:affinity_registry_size: ")
          .append(to_string(m_AffinityRegistry.size()))
          .append(kNewLine)
          .append("OK:client_registry_size: ")
          .append(to_string(m_ClientsRegistry.size()))
          .append(kNewLine);
    return output;
}


void CQueue::GetJobsPerState(const CNSClientId &  client,
                             const string &       group_token,
                             const string &       aff_token,
                             size_t *             jobs,
                             vector<string> &     warnings) const
{
    TNSBitVector        group_jobs;
    TNSBitVector        aff_jobs;
    CFastMutexGuard     guard(m_OperationLock);

    if (!group_token.empty()) {
        try {
            group_jobs = m_GroupRegistry.GetJobs(group_token);
        } catch (...) {
            warnings.push_back("eGroupNotFound:job group " + group_token +
                               " is not found");
        }
    }
    if (!aff_token.empty()) {
        unsigned int  aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
        if (aff_id == 0)
            warnings.push_back("eAffinityNotFound:affinity " + aff_token +
                               " is not found");
        else
            aff_jobs = m_AffinityRegistry.GetJobsWithAffinity(aff_id);
    }

    if (!warnings.empty()) {
        // This is just in case; theoretically the caller should not use the
        // results if warnings are present.
        for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
            jobs[index] = 0;
        }
        return;
    }


    string          scope = client.GetScope();
    TNSBitVector    candidates;
    for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
        candidates.clear();
        m_StatusTracker.GetJobs(g_ValidJobStatuses[index], candidates);

        if (!group_token.empty())
            candidates &= group_jobs;
        if (!aff_token.empty())
            candidates &= aff_jobs;

        // Apply the scope limitations. Empty scope means that all the jobs
        // must be provided
        if (scope == kNoScopeOnly) {
            // Exclude all scoped jobs
            candidates -= m_ScopeRegistry.GetAllJobsInScopes();
        } else if (!scope.empty()) {
            // Specific scope
            candidates &= m_ScopeRegistry.GetJobs(scope);
        }

        jobs[index] = candidates.count();
    }
}


string CQueue::PrintJobsStat(const CNSClientId &  client,
                             const string &    group_token,
                             const string &    aff_token,
                             vector<string> &  warnings) const
{
    size_t              total = 0;
    string              result;
    size_t              jobs_per_state[g_ValidJobStatusesSize];

    GetJobsPerState(client, group_token, aff_token, jobs_per_state, warnings);

    // Warnings could be about non existing affinity or group. If so there are
    // no jobs to be printed.
    if (warnings.empty()) {
        for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
            result += "OK:" +
                      CNetScheduleAPI::StatusToString(g_ValidJobStatuses[index]) +
                      ": " + to_string(jobs_per_state[index]) + "\n";
            total += jobs_per_state[index];
        }
        result += "OK:Total: " + to_string(total) + "\n";
    }
    return result;
}


unsigned int CQueue::CountActiveJobs(void) const
{
    vector<CNetScheduleAPI::EJobStatus>     statuses;

    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    return m_StatusTracker.CountStatus(statuses);
}


void CQueue::SetPauseStatus(const CNSClientId &  client, TPauseStatus  status)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    bool        need_notifications = (status == eNoPause &&
                                      m_PauseStatus != eNoPause);

    m_PauseStatus = status;
    if (need_notifications)
        m_NotificationsList.onQueueResumed(m_StatusTracker.AnyPending());

    SerializePauseState(m_Server);
}


void CQueue::RegisterQueueResumeNotification(unsigned int  address,
                                             unsigned short  port,
                                             bool  new_format)
{
    m_NotificationsList.AddToQueueResumedNotifications(address, port,
                                                       new_format);
}


void CQueue::x_UpdateDB_PutResultNoLock(unsigned                job_id,
                                        const string &          auth_token,
                                        const CNSPreciseTime &  curr,
                                        int                     ret_code,
                                        const string &          output,
                                        CJob &                  job,
                                        const CNSClientId &     client)
{
    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    if (!auth_token.empty()) {
        // Need to check authorization token first
        CJob::EAuthTokenCompareResult   token_compare_result =
                                job_iter->second.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");
        if (token_compare_result == CJob::ePassportOnlyMatch) {
            // That means that the job has been executing by another worker
            // node at the moment, but we can accept the results anyway
            ERR_POST(Warning << "Received PUT2 with only "
                                "passport matched.");
        }
        // Here: the authorization token is OK, we can continue
    }

    // Append the event
    CJobEvent *     event = &job_iter->second.AppendEvent();
    event->SetStatus(CNetScheduleAPI::eDone);
    event->SetEvent(CJobEvent::eDone);
    event->SetTimestamp(curr);
    event->SetRetCode(ret_code);

    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());
    event->SetNodeAddr(client.GetAddress());

    job_iter->second.SetStatus(CNetScheduleAPI::eDone);
    job_iter->second.SetOutput(output);
    job_iter->second.SetLastTouch(curr);

    job = job_iter->second;
}


// If the job.job_id != 0 => the job has been read successfully
// Exception => DB errors
void CQueue::x_UpdateDB_ProvideJobNoLock(const CNSClientId &     client,
                                         const CNSPreciseTime &  curr,
                                         unsigned int            job_id,
                                         ECommandGroup           cmd_group,
                                         CJob &                  job)
{
    auto        job_iter = m_Jobs.find(job_id);
    if (job_iter == m_Jobs.end())
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    CJobEvent &     event = job_iter->second.AppendEvent();
    event.SetTimestamp(curr);
    event.SetNodeAddr(client.GetAddress());
    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());

    if (cmd_group == eGet) {
        event.SetStatus(CNetScheduleAPI::eRunning);
        event.SetEvent(CJobEvent::eRequest);
    } else {
        event.SetStatus(CNetScheduleAPI::eReading);
        event.SetEvent(CJobEvent::eRead);
    }

    job_iter->second.SetLastTouch(curr);
    if (cmd_group == eGet) {
        job_iter->second.SetStatus(CNetScheduleAPI::eRunning);
        job_iter->second.SetRunTimeout(kTimeZero);
        job_iter->second.SetRunCount(job_iter->second.GetRunCount() + 1);
    } else {
        job_iter->second.SetStatus(CNetScheduleAPI::eReading);
        job_iter->second.SetReadTimeout(kTimeZero);
        job_iter->second.SetReadCount(job_iter->second.GetReadCount() + 1);
    }

    job = job_iter->second;
}


// Dumps all the jobs into a flat file at the time of shutdown
void CQueue::Dump(const string &  dump_dname)
{
    // Form a bit vector of all jobs to dump
    vector<TJobStatus>      statuses;
    TNSBitVector            jobs_to_dump;

    // All statuses
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    statuses.push_back(CNetScheduleAPI::eCanceled);
    statuses.push_back(CNetScheduleAPI::eFailed);
    statuses.push_back(CNetScheduleAPI::eDone);
    statuses.push_back(CNetScheduleAPI::eReading);
    statuses.push_back(CNetScheduleAPI::eConfirmed);
    statuses.push_back(CNetScheduleAPI::eReadFailed);

    m_StatusTracker.GetJobs(statuses, jobs_to_dump);

    // Exclude all the jobs which belong to a certain scope. There is no
    // need to save them
    jobs_to_dump -= m_ScopeRegistry.GetAllJobsInScopes();

    if (!jobs_to_dump.any())
        return;     // Nothing to dump


    string      jobs_file_name = x_GetJobsDumpFileName(dump_dname);
    FILE *      jobs_file = NULL;

    try {
        // Dump the affinity registry
        m_AffinityRegistry.Dump(dump_dname, m_QueueName);

        // Dump the group registry
        m_GroupRegistry.Dump(dump_dname, m_QueueName);

        jobs_file = fopen(jobs_file_name.c_str(), "wb");
        if (jobs_file == NULL)
            throw runtime_error("Cannot open file " + jobs_file_name +
                                " to dump jobs");

        // Disable buffering to detect errors right away
        setbuf(jobs_file, NULL);

        // Write a header
        SJobDumpHeader      header;
        header.Write(jobs_file);

        TNSBitVector::enumerator    en(jobs_to_dump.first());
        for ( ; en.valid(); ++en) {
            auto        job_iter = m_Jobs.find(*en);
            if (job_iter == m_Jobs.end()) {
                ERR_POST("Dump at SHUTDOWN: error fetching job " <<
                         DecorateJob(*en) << ". Skip and continue.");
                continue;
            }

            job_iter->second.Dump(jobs_file);
        }
    } catch (const exception &  ex) {
        if (jobs_file != NULL)
            fclose(jobs_file);
        RemoveDump(dump_dname);
        throw runtime_error("Error dumping queue " + m_QueueName +
                            ": " + string(ex.what()));
    }

    fclose(jobs_file);
}


void CQueue::RemoveDump(const string &  dump_dname)
{
    m_AffinityRegistry.RemoveDump(dump_dname, m_QueueName);
    m_GroupRegistry.RemoveDump(dump_dname, m_QueueName);

    string      jobs_file_name = x_GetJobsDumpFileName(dump_dname);

    if (access(jobs_file_name.c_str(), F_OK) != -1)
        remove(jobs_file_name.c_str());
}


string CQueue::x_GetJobsDumpFileName(const string &  dump_dname) const
{
    string      upper_queue_name = m_QueueName;
    NStr::ToUpper(upper_queue_name);
    return dump_dname + kJobsFileName + "." + upper_queue_name;
}


unsigned int  CQueue::LoadFromDump(const string &  dump_dname)
{
    unsigned int    recs = 0;
    string          jobs_file_name = x_GetJobsDumpFileName(dump_dname);
    FILE *          jobs_file = NULL;

    if (!CDir(dump_dname).Exists())
        return 0;
    if (!CFile(jobs_file_name).Exists())
        return 0;

    try {
        m_AffinityRegistry.LoadFromDump(dump_dname, m_QueueName);
        m_GroupRegistry.LoadFromDump(dump_dname, m_QueueName);

        jobs_file = fopen(jobs_file_name.c_str(), "rb");
        if (jobs_file == NULL)
            throw runtime_error("Cannot open file " + jobs_file_name +
                                " to load dumped jobs");

        SJobDumpHeader      header;
        header.Read(jobs_file);

        CJob                job;
        AutoArray<char>     input_buf(new char[kNetScheduleMaxOverflowSize]);
        AutoArray<char>     output_buf(new char[kNetScheduleMaxOverflowSize]);
        while (job.LoadFromDump(jobs_file,
                                input_buf.get(), output_buf.get(),
                                header)) {
            unsigned int    job_id = job.GetId();
            unsigned int    group_id = job.GetGroupId();
            unsigned int    aff_id = job.GetAffinityId();
            TJobStatus      status = job.GetStatus();

            m_Jobs[job_id] = job;
            m_StatusTracker.SetExactStatusNoLock(job_id, status, true);

            if ((status == CNetScheduleAPI::eRunning ||
                 status == CNetScheduleAPI::eReading) &&
                m_RunTimeLine) {
                // Add object to the first available slot;
                // it is going to be rescheduled or dropped
                // in the background control thread
                // We can use time line without lock here because
                // the queue is still in single-use mode while
                // being loaded.
                m_RunTimeLine->AddObject(m_RunTimeLine->GetHead(), job_id);
            }

            // Register the job for the affinity if so
            if (aff_id != 0)
                m_AffinityRegistry.AddJobToAffinity(job_id, aff_id);

            // Register the job in the group registry
            if (group_id != 0)
                m_GroupRegistry.AddJobToGroup(group_id, job_id);

            // Register the loaded job with the garbage collector
            CNSPreciseTime  submit_time = job.GetSubmitTime();
            CNSPreciseTime  expiration =
                    GetJobExpirationTime(job.GetLastTouch(), status,
                                         submit_time, job.GetTimeout(),
                                         job.GetRunTimeout(),
                                         job.GetReadTimeout(),
                                         m_Timeout, m_RunTimeout, m_ReadTimeout,
                                         m_PendingTimeout, kTimeZero);
            m_GCRegistry.RegisterJob(job_id, job.GetSubmitTime(),
                                     aff_id, group_id, expiration);
            ++recs;
        }

        // Make sure that there are no affinity IDs in the registry for which
        // there are no jobs and initialize the next affinity ID counter.
        m_AffinityRegistry.FinalizeAffinityDictionaryLoading();

        // Make sure that there are no group IDs in the registry for which there
        // are no jobs and initialize the next group ID counter.
        m_GroupRegistry.FinalizeGroupDictionaryLoading();
    } catch (const exception &  ex) {
        if (jobs_file != NULL)
            fclose(jobs_file);

        x_ClearQueue();
        throw runtime_error("Error loading queue " + m_QueueName +
                            " from its dump: " + string(ex.what()));
    } catch (...) {
        if (jobs_file != NULL)
        fclose(jobs_file);

        x_ClearQueue();
        throw runtime_error("Unknown error loading queue " + m_QueueName +
                            " from its dump");
    }

    fclose(jobs_file);
    return recs;
}


// The member does not grab the operational lock.
// The member is used at the time of loading jobs from dump and at that time
// there is no concurrent access.
void CQueue::x_ClearQueue(void)
{
    m_StatusTracker.ClearAll();
    m_RunTimeLine->ReInit();
    m_JobsToDelete.clear(true);
    m_ReadJobs.clear(true);

    m_AffinityRegistry.Clear();
    m_GroupRegistry.Clear();
    m_GCRegistry.Clear();
    m_ScopeRegistry.Clear();

    m_Jobs.clear();
}


void CQueue::x_NotifyJobChanges(const CJob &            job,
                                const string &          job_key,
                                ENotificationReason     reason,
                                const CNSPreciseTime &  current_time)
{
    string      notification;
    TJobStatus  job_status = job.GetStatus();

    if (reason == eJobDeleted)
        job_status = CNetScheduleAPI::eDeleted;

    if (reason != eProgressMessageChanged || job.GetLsnrNeedProgressMsgNotif()) {
        if (job.ShouldNotifyListener(current_time)) {
            notification = m_NotificationsList.BuildJobChangedNotification(
                    job, job_key, job_status, reason);
            m_NotificationsList.NotifyJobChanges(job.GetListenerNotifAddr(),
                                                 job.GetListenerNotifPort(),
                                                 notification);
        }
    }

    if (reason != eNotificationStolen) {
        if (reason != eProgressMessageChanged || job.GetSubmNeedProgressMsgNotif()) {
            if (job.ShouldNotifySubmitter(current_time)) {
                if (notification.empty())
                    notification = m_NotificationsList.BuildJobChangedNotification(
                            job, job_key, job_status, reason);
                m_NotificationsList.NotifyJobChanges(job.GetSubmAddr(),
                                                     job.GetSubmNotifPort(),
                                                     notification);
            }
        }
    }
}


END_NCBI_SCOPE
