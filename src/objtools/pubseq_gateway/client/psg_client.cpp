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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#ifdef HAVE_PSG_CLIENT

#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_base64.h>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_service.h>
#include <connect/ncbi_connutil.h>

#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>
#undef ThrowError // unfortunately

#include "psg_client_impl.hpp"

BEGIN_NCBI_SCOPE


const char* CPSG_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
        case eTimeout:          return "eTimeout";
        case eServerError:      return "eServerError";
        case eInternalError:    return "eInternalError";
        case eParameterMissing: return "eParameterMissing";
        default:                return CException::GetErrCodeString();
    }
}


SPSG_BlobReader::SPSG_BlobReader(SPSG_Reply::SItem::TTS& src, TStats stats)
    : m_Src(src),
      m_Stats(stats)
{
}

ERW_Result SPSG_BlobReader::x_Read(void* buf, size_t count, size_t* bytes_read)
{
    assert(bytes_read);

    *bytes_read = 0;

    CheckForNewChunks();

    for (; m_Chunk < m_Data.size(); ++m_Chunk) {
        auto& data = m_Data[m_Chunk];

        // Chunk has not been received yet
        if (data.empty()) return eRW_Success;

        auto available = data.size() - m_Index;
        auto to_copy = min(count, available);

        memcpy(buf, data.data() + m_Index, to_copy);
        buf = (char*)buf + to_copy;
        count -= to_copy;
        *bytes_read += to_copy;
        m_Index += to_copy;

        if (!count) return eRW_Success;

        m_Index = 0;
    }

    auto src_locked = m_Src.GetLock();
    return src_locked->expected.Cmp<equal_to>(src_locked->received) ? eRW_Eof : eRW_Success;
}

ERW_Result SPSG_BlobReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    size_t read;
    const auto kSeconds = TPSG_ReaderTimeout::GetDefault();
    CDeadline deadline(kSeconds);

    do {
        auto rv = x_Read(buf, count, &read);

        if ((rv != eRW_Success) || (read != 0)) {
            if (bytes_read) *bytes_read = read;
            return rv;
        }
    }
    while (m_Src.WaitUntil(deadline));

    NCBI_THROW_FMT(CPSG_Exception, eTimeout, "Timeout on reading (after " << kSeconds << " seconds)");
    return eRW_Error;
}

ERW_Result SPSG_BlobReader::PendingCount(size_t* count)
{
    assert(count);

    *count = 0;

    CheckForNewChunks();

    auto k = m_Index;

    for (auto i = m_Chunk; i < m_Data.size(); ++i) {
        auto& data = m_Data[i];

        // Chunk has not been received yet
        if (data.empty()) return eRW_Success;

        *count += data.size() - k;
        k = 0;
    }

    return eRW_Success;
}

void SPSG_BlobReader::CheckForNewChunks()
{
    if (m_Src->state.Empty()) return;

    auto src_locked = m_Src.GetLock();
    auto& src = *src_locked;
    auto& chunks = src.chunks;

    if (m_Data.size() < chunks.size()) m_Data.resize(chunks.size());

    for (size_t i = 0; i < chunks.size(); ++i) {
        if (auto size = chunks[i].size()) {
            m_Data[i].swap(chunks[i]);
            if (auto stats = m_Stats.second.lock()) stats->AddData(m_Stats.first, SPSG_Stats::eRead, size);
        }
    }
}


const char* s_GetRequestTypeName(CPSG_Request::EType type)
{
    switch (type) {
        case CPSG_Request::eBiodata:        return "biodata";
        case CPSG_Request::eResolve:        return "resolve";
        case CPSG_Request::eBlob:           return "blob";
        case CPSG_Request::eNamedAnnotInfo: return "annot";
        case CPSG_Request::eChunk:          return "chunk";
    }

    // Should not happen
    _TROUBLE;
    return "unknown";
}

CPSG_Request::SType::operator string() const
{
    return s_GetRequestTypeName(m_Type);
}

ostream& operator<<(ostream& os, const CPSG_Request::SType& type)
{
    return os << s_GetRequestTypeName(type.m_Type);
}


static_assert(is_nothrow_move_constructible<CPSG_BioId>::value, "CPSG_BioId move constructor must be noexcept");

string CPSG_BioId::Repr() const
{
    return m_Type == TType::e_not_set ? m_Id : m_Id + '~' + to_string(m_Type);
}

CPSG_BioId s_GetBioId(const CJsonNode& data)
{
    auto type = static_cast<CPSG_BioId::TType>(data.GetInteger("seq_id_type"));
    auto accession = data.GetString("accession");
    auto name_node = data.GetByKeyOrNull("name");
    auto name = name_node && name_node.IsString() ? name_node.AsString() : string();
    auto version = static_cast<int>(data.GetInteger("version"));
    return { objects::CSeq_id(type, accession, name, version).AsFastaString(), type };
};

ostream& operator<<(ostream& os, const CPSG_BioId& bio_id)
{
    if (bio_id.GetType()) os << "seq_id_type=" << bio_id.GetType() << '&';
    return os << "seq_id=" << bio_id.GetId();
}


static_assert(is_nothrow_move_constructible<CPSG_BlobId>::value, "CPSG_BlobId move constructor must be noexcept");

string CPSG_BlobId::Repr() const
{
    return m_LastModified.IsNull() ? m_Id : m_Id + '~' + to_string(m_LastModified.GetValue());
}

CPSG_BlobId s_GetBlobId(const CJsonNode& data)
{
    CPSG_BlobId::TLastModified last_modified;

    if (data.HasKey("last_modified")) {
        last_modified = data.GetInteger("last_modified");
    }

    if (data.HasKey("blob_id")) {
        return { data.GetString("blob_id"), last_modified };
    }

    auto sat = static_cast<int>(data.GetInteger("sat"));
    auto sat_key = static_cast<int>(data.GetInteger("sat_key"));
    return { sat, sat_key, last_modified };
}

ostream& operator<<(ostream& os, const CPSG_BlobId& blob_id)
{
    if (!blob_id.GetLastModified().IsNull()) os << "last_modified=" << blob_id.GetLastModified().GetValue() << '&';
    return os << "blob_id=" << blob_id.GetId();
}


string CPSG_ChunkId::Repr() const
{
    return to_string(m_Id2Chunk) + '~' + m_Id2Info;
}

ostream& operator<<(ostream& os, const CPSG_ChunkId& chunk_id)
{
    return os << "id2_chunk=" << chunk_id.GetId2Chunk() << "&id2_info=" << chunk_id.GetId2Info();
}


struct SDataId
{
    enum ETypePriority { eBlobIdPriority, eChunkIdPriority };

    SDataId(const SPSG_Args& args) : m_Args(args) {}

    template <ETypePriority = eBlobIdPriority>
    bool HasBlobId() const { return !m_Args.GetValue<SPSG_Args::eBlobId>().get().empty(); }

    template <ETypePriority type_priority = eBlobIdPriority>
    static unique_ptr<CPSG_DataId> Get(SDataId data_id);

    template <class TRequestedId = CPSG_DataId>
    unique_ptr<CPSG_DataId> Get(shared_ptr<SPSG_Stats>& stats);

private:
    template <class TRequestedId> unique_ptr<TRequestedId> x_Get() const;

    template <class TRequestedId, class TAllowedId>
    unique_ptr<TRequestedId> Get() const;

    const SPSG_Args& m_Args;
};

template <>
bool SDataId::HasBlobId<SDataId::eChunkIdPriority>() const
{
    return m_Args.GetValue<SPSG_Args::eId2Chunk>().get().empty();
}

template <>
unique_ptr<CPSG_BlobId> SDataId::x_Get<CPSG_BlobId>() const
{
    CPSG_BlobId::TLastModified last_modified;
    const auto& blob_id = m_Args.GetValue<SPSG_Args::eBlobId>();
    const auto& last_modified_str = m_Args.GetValue("last_modified");

    if (last_modified_str.empty()) {
        return make_unique<CPSG_BlobId>(blob_id);
    }

    last_modified = NStr::StringToNumeric<Int8>(last_modified_str);
    return make_unique<CPSG_BlobId>(blob_id, move(last_modified));
}

template <>
unique_ptr<CPSG_ChunkId> SDataId::x_Get<CPSG_ChunkId>() const
{
    auto id2_chunk = NStr::StringToNumeric<int>(m_Args.GetValue<SPSG_Args::eId2Chunk>().get());
    return make_unique<CPSG_ChunkId>(id2_chunk, m_Args.GetValue("id2_info"));
}

template <class TRequestedId, class TAllowedId>
unique_ptr<TRequestedId> SDataId::Get() const
{
    try {
        return x_Get<TAllowedId>();
    }
    catch (...) {
        NCBI_THROW_FMT(CPSG_Exception, eServerError,
                "Both blob_id[+last_modified] and id2_chunk+id2_info pairs are missing/corrupted in server response: " <<
                m_Args.GetQueryString(CUrlArgs::eAmp_Char));
    }
}

template <SDataId::ETypePriority type_priority>
unique_ptr<CPSG_DataId> SDataId::Get(SDataId data_id)
{
    return data_id.HasBlobId<type_priority>() ? data_id.Get<CPSG_DataId, CPSG_BlobId>() : data_id.Get<CPSG_DataId, CPSG_ChunkId>();
}

template <class TRequestedId>
unique_ptr<CPSG_DataId> SDataId::Get(shared_ptr<SPSG_Stats>& stats)
{
    auto id = Get<TRequestedId, TRequestedId>();
    if (stats) stats->AddId(*id);
    return id;
}

template <>
unique_ptr<CPSG_DataId> SDataId::Get<CPSG_DataId>(shared_ptr<SPSG_Stats>& stats)
{
    return HasBlobId() ? Get<CPSG_BlobId>(stats) : Get<CPSG_ChunkId>(stats);
}

template <class TReplyItem>
CPSG_ReplyItem* CPSG_Reply::SImpl::CreateImpl(TReplyItem* item, const vector<SPSG_Chunk>& chunks)
{
    if (chunks.empty()) return item;

    unique_ptr<TReplyItem> rv(item);
    rv->m_Data = CJsonNode::ParseJSON(chunks.front(), CJsonNode::fStandardJson);

    return rv.release();
}

CPSG_ReplyItem* CPSG_Reply::SImpl::CreateImpl(SPSG_Reply::SItem::TTS& item_ts, const SPSG_Args& args, shared_ptr<SPSG_Stats>& stats)
{
    SDataId data_id(args);
    unique_ptr<CPSG_BlobData> blob_data(new CPSG_BlobData(data_id.Get(stats)));
    blob_data->m_Stream.reset(new SPSG_RStream(item_ts, make_pair(data_id.HasBlobId(), reply->stats)));
    return blob_data.release();
}

CPSG_SkippedBlob::TSeconds s_GetSeconds(const SPSG_Args& args, const string& name)
{
    const auto& value = args.GetValue(name);

    // Do not use ternary operator below, 'null' will be become '0.0' otherwise
    if (value.empty()) return null;
    return NStr::StringToNumeric<double>(value);
}

CPSG_ReplyItem* CPSG_Reply::SImpl::CreateImpl(CPSG_SkippedBlob::EReason reason, const SPSG_Args& args, shared_ptr<SPSG_Stats>& stats)
{
    auto id = SDataId::Get<SDataId::eChunkIdPriority>(args);
    auto sent_seconds_ago = s_GetSeconds(args, "sent_seconds_ago");
    auto time_until_resend = s_GetSeconds(args, "time_until_resend");

    if (stats) {
        stats->IncCounter(SPSG_Stats::eSkippedBlob, reason);

        if (!sent_seconds_ago.IsNull()) stats->AddTime(SPSG_Stats::eSentSecondsAgo, sent_seconds_ago);
        if (!time_until_resend.IsNull()) stats->AddTime(SPSG_Stats::eTimeUntilResend, time_until_resend);
    }

    return new CPSG_SkippedBlob(move(id), reason, move(sent_seconds_ago), move(time_until_resend));
}

CPSG_Processor::EProgressStatus s_GetProgressStatus(const SPSG_Args& args)
{
    const auto& progress = args.GetValue("progress");

    if (progress == "start")      return CPSG_Processor::eStart;
    if (progress == "done")       return CPSG_Processor::eDone;
    if (progress == "not_found")  return CPSG_Processor::eNotFound;
    if (progress == "canceled")   return CPSG_Processor::eCanceled;
    if (progress == "timeout")    return CPSG_Processor::eTimeout;
    if (progress == "error")      return CPSG_Processor::eError;

    // Should not happen, new server?
    return CPSG_Processor::eUnknown;
}

CPSG_ReplyItem* CPSG_Reply::SImpl::CreateImpl(SPSG_Reply::SItem::TTS& item_ts, SPSG_Reply::SItem& item, CPSG_ReplyItem::EType type, CPSG_SkippedBlob::EReason reason)
{
    auto stats = reply->stats.lock();
    if (stats) stats->IncCounter(SPSG_Stats::eReplyItem, type);

    auto status = item.state.GetStatus();
    auto& args = item.args;
    auto& chunks = item.chunks;

    if ((status == EPSG_Status::eSuccess) || (status == EPSG_Status::eInProgress)) {
        switch (type) {
            case CPSG_ReplyItem::eBlobData:         return CreateImpl(item_ts, args, stats);
            case CPSG_ReplyItem::eSkippedBlob:      return CreateImpl(reason, args, stats);
            case CPSG_ReplyItem::eBioseqInfo:       return CreateImpl(new CPSG_BioseqInfo, chunks);
            case CPSG_ReplyItem::eBlobInfo:         return CreateImpl(new CPSG_BlobInfo(SDataId::Get(args)), chunks);
            case CPSG_ReplyItem::eNamedAnnotInfo:   return CreateImpl(new CPSG_NamedAnnotInfo(args.GetValue("na")), chunks);
            case CPSG_ReplyItem::ePublicComment:    return new CPSG_PublicComment(SDataId::Get(args), chunks.empty() ? string() : chunks.front());
            case CPSG_ReplyItem::eProcessor:        return new CPSG_Processor(s_GetProgressStatus(args));
            case CPSG_ReplyItem::eEndOfReply:       return nullptr;
        }

        // Should not happen
        _TROUBLE;

    } else if (type != CPSG_ReplyItem::eEndOfReply) {
        if (stats) stats->IncCounter(SPSG_Stats::eReplyItemStatus, static_cast<size_t>(status));
        return new CPSG_ReplyItem(type);
    }

    return nullptr;
}

struct SItemTypeAndReason : pair<CPSG_ReplyItem::EType, CPSG_SkippedBlob::EReason>
{
    static SItemTypeAndReason Get(const SPSG_Args& args);

private:
    using TBase = pair<CPSG_ReplyItem::EType, CPSG_SkippedBlob::EReason>;
    using TBase::TBase;
    SItemTypeAndReason(CPSG_ReplyItem::EType type) : TBase(type, CPSG_SkippedBlob::eUnknown) {}
    static SItemTypeAndReason GetIfBlob(const SPSG_Args& args);
};

// Item type cannot be determined by "item_type" alone, for blobs "reason" has to be used as well.
SItemTypeAndReason SItemTypeAndReason::GetIfBlob(const SPSG_Args& args)
{
    const auto reason = args.GetValue("reason");

    if (reason.empty()) {
        return CPSG_ReplyItem::eBlobData;

    } else if (reason == "excluded") {
        return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eExcluded };

    } else if (reason == "inprogress") {
        return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eInProgress };

    } else if (reason == "sent") {
        return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eSent };

    } else {
        return { CPSG_ReplyItem::eSkippedBlob, CPSG_SkippedBlob::eUnknown };
    }
}

SItemTypeAndReason SItemTypeAndReason::Get(const SPSG_Args& args)
{
    const auto item_type = args.GetValue<SPSG_Args::eItemType>();

    switch (item_type.first) {
        case SPSG_Args::eBioseqInfo:     return CPSG_ReplyItem::eBioseqInfo;
        case SPSG_Args::eBlobProp:       return CPSG_ReplyItem::eBlobInfo;
        case SPSG_Args::eBlob:           return GetIfBlob(args);
        case SPSG_Args::eReply:          break;
        case SPSG_Args::eBioseqNa:       return CPSG_ReplyItem::eNamedAnnotInfo;
        case SPSG_Args::ePublicComment:  return CPSG_ReplyItem::ePublicComment;
        case SPSG_Args::eProcessor:      return CPSG_ReplyItem::eProcessor;
        case SPSG_Args::eUnknownItem:    break;
    }

    if (TPSG_FailOnUnknownItems::GetDefault()) {
        NCBI_THROW_FMT(CPSG_Exception, eServerError, "Received unknown item type: " << item_type.second.get());
    }

    static atomic_bool reported(false);

    if (!reported.exchange(true)) {
        ERR_POST("Received unknown item type: " << item_type.second.get());
    }

    return CPSG_ReplyItem::eEndOfReply;
}

shared_ptr<CPSG_ReplyItem> CPSG_Reply::SImpl::Create(SPSG_Reply::SItem::TTS& item_ts)
{
    auto item_locked = item_ts.GetLock();
    item_locked->state.SetReturned();

    const auto& args = item_locked->args;
    const auto itar = SItemTypeAndReason::Get(args);

    shared_ptr<CPSG_ReplyItem> rv(CreateImpl(item_ts, *item_locked, itar.first, itar.second));

    if (rv) {
        rv->m_Impl.reset(new CPSG_ReplyItem::SImpl(item_ts));
        rv->m_Reply = user_reply.lock();
        rv->m_ProcessorId = args.GetValue("processor_id");
        _ASSERT(rv->m_Reply);
    }

    return rv;
}


pair<mutex, weak_ptr<CPSG_Queue::SImpl::CService::TMap>> CPSG_Queue::SImpl::CService::sm_Instance;

SPSG_IoCoordinator& CPSG_Queue::SImpl::CService::GetIoC(const string& service)
{
    if (service.empty()) {
        NCBI_THROW(CPSG_Exception, eParameterMissing, "Service name is empty");
    }

    unique_lock<mutex> lock(sm_Instance.first);

    auto found = m_Map->find(service);

    if (found != m_Map->end()) {
        return *found->second;
    }

    auto created = m_Map->emplace(service, unique_ptr<SPSG_IoCoordinator>(new SPSG_IoCoordinator(service)));
    return *created.first->second;
}

shared_ptr<CPSG_Queue::SImpl::CService::TMap> CPSG_Queue::SImpl::CService::GetMap()
{
    unique_lock<mutex> lock(sm_Instance.first);

    auto rv = sm_Instance.second.lock();

    if (!rv) {
        rv = make_shared<TMap>();
        sm_Instance.second = rv;
    }

    return rv;
}


SPSG_UserArgs::SPSG_UserArgs(const CUrlArgs& url_args)
{
    for (const auto& p : url_args.GetArgs()) {
        operator[](p.name).emplace(p.value);
    }
}


// This class is used to split a complex merge function into four,
// much simpler members without a need to pass many parameters around
struct SPSG_UserArgsBuilder::MergeValues
{
    using TValues = SPSG_UserArgs::mapped_type;

    MergeValues(const string& name, SPSG_UserArgs& to, const TValues& from_values) :
        m_Name(name),
        m_To(to),
        m_ToValues(m_To[name]),
        m_FromValues(from_values),
        m_ToValuesSizeBefore(m_ToValues.size())
    {}

    // The rvalue ref-qualifier allows using the class like a function and also prevents
    // the operator (doing the merge) from being accidentally called more than once
    explicit operator bool() &&;

private:
    bool AddNoMerge();
    void AddCorrelated(const string& correlated_name);
    void AddAll() { m_ToValues.insert(m_FromValues.begin(), m_FromValues.end()); }

    const string& m_Name;
    SPSG_UserArgs& m_To;
    TValues& m_ToValues;
    const TValues& m_FromValues;
    TValues::size_type m_ToValuesSizeBefore;
};

SPSG_UserArgsBuilder::MergeValues::operator bool() &&
{
    static const unordered_map<string, string> correlations{
        { "enable_processor", "disable_processor" },
        { "disable_processor", "enable_processor" },
    };

    if (!AddNoMerge()) {
        auto found = correlations.find(m_Name);

        if (found == correlations.end()) {
            AddAll();
        } else {
            AddCorrelated(found->second);
        }
    }

    return m_ToValues.size() > m_ToValuesSizeBefore;
}

bool SPSG_UserArgsBuilder::MergeValues::AddNoMerge()
{
    static const unordered_set<string> no_merge{
        "hops",
    };

    if (no_merge.find(m_Name) == no_merge.end()) {
        return false;
    }

    if (m_ToValues.empty()) {
        AddAll();
    }

    return true;
}

void SPSG_UserArgsBuilder::MergeValues::AddCorrelated(const string& correlated_name)
{
    auto found = m_To.find(correlated_name);

    if (found == m_To.end()) {
        AddAll();
    } else {
        const auto& correlated = found->second;
        set_difference(m_FromValues.begin(), m_FromValues.end(), correlated.begin(), correlated.end(), inserter(m_ToValues, m_ToValues.end()));
    }
}

bool SPSG_UserArgsBuilder::Merge(SPSG_UserArgs& higher_priority, const SPSG_UserArgs& lower_priority)
{
    bool added_something = false;

    for (const auto& p : lower_priority) {
        if (MergeValues(p.first, higher_priority, p.second)) {
            added_something = true;
        }
    }

    return added_something;
}

void SPSG_UserArgsBuilder::Build(ostream& os, const SPSG_UserArgs& request_args)
{
    if (!request_args.empty()) {
        auto combined_args = s_GetIniArgs();

        // We can use cached args unless request_args are actually adding something
        if (Merge(combined_args, request_args)) {
            Merge(combined_args, m_QueueArgs);
            os << combined_args;
            return;
        }
    }

    os << m_CachedArgs;
}

void SPSG_UserArgsBuilder::x_UpdateCache()
{
    auto combined_args = s_GetIniArgs();
    Merge(combined_args, m_QueueArgs);

    ostringstream os;
    os << combined_args;
    m_CachedArgs = os.str();
}

const SPSG_UserArgs& SPSG_UserArgsBuilder::s_GetIniArgs()
{
    static SPSG_UserArgs instance(TPSG_RequestUserArgs::GetDefault());
    return instance;
}


CPSG_Queue::SImpl::SImpl(const string& service) :
    queue(make_shared<TPSG_Queue>()),
    m_Service(service)
{
}

const char* s_GetTSE(CPSG_Request_Biodata::EIncludeData include_data)
{
    switch (include_data) {
        case CPSG_Request_Biodata::eDefault:  return nullptr;
        case CPSG_Request_Biodata::eNoTSE:    return "none";
        case CPSG_Request_Biodata::eSlimTSE:  return "slim";
        case CPSG_Request_Biodata::eSmartTSE: return "smart";
        case CPSG_Request_Biodata::eWholeTSE: return "whole";
        case CPSG_Request_Biodata::eOrigTSE:  return "orig";
    }

    return nullptr;
}

string s_GetOtherArgs()
{
    ostringstream os;
    TPSG_UseCache use_cache(TPSG_UseCache::eGetDefault);

    switch (use_cache) {
        case EPSG_UseCache::eDefault:                         break;
        case EPSG_UseCache::eNo:      os << "&use_cache=no";  break;
        case EPSG_UseCache::eYes:     os << "&use_cache=yes"; break;
    }

    os << "&client_id=" << GetDiagContext().GetStringUID();

    return os.str();
}

string CPSG_Queue::SImpl::x_GetAbsPathRef(shared_ptr<const CPSG_Request> user_request)
{
    static const string other_args(s_GetOtherArgs());

    _ASSERT(user_request);
    ostringstream os;
    user_request->x_GetAbsPathRef(os);

    os << other_args;
    m_UserArgsBuilder.GetLock()->Build(os, user_request->m_UserArgs);
    return os.str();
}

const char* s_GetAccSubstitution(EPSG_AccSubstitution acc_substitution)
{
    switch (acc_substitution) {
        case EPSG_AccSubstitution::Default: break;
        case EPSG_AccSubstitution::Limited: return "&acc_substitution=limited";
        case EPSG_AccSubstitution::Never:   return "&acc_substitution=never";
    }

    return "";
}


const char* s_GetAutoBlobSkipping(ESwitch value)
{
    switch (value) {
        case eDefault: break;
        case eOn:      return "&auto_blob_skipping=yes";
        case eOff:     return "&auto_blob_skipping=no";
    }

    return "";
}


template <typename TIterator, class TGet>
void s_DelimitedOutput(TIterator from, TIterator to, ostream& os, const char* prefix, char delimiter, TGet get)
{
    if (from != to) {
        os << prefix << get(*from++);

        while (from != to) {
            os << delimiter << get(*from++);
        }
    }
}

template <class TValues, class... TArgs>
void s_DelimitedOutput(const TValues& values, TArgs&&... args)
{
    return s_DelimitedOutput(begin(values), end(values), forward<TArgs>(args)...);
}

void CPSG_Request_Biodata::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/get?" << m_BioId;

    if (const auto tse = s_GetTSE(m_IncludeData)) os << "&tse=" << tse;

    s_DelimitedOutput(m_ExcludeTSEs, os, "&exclude_blobs=", ',', [](const auto& blob_id) { return blob_id.GetId(); });
    os << s_GetAccSubstitution(m_AccSubstitution);

    if (m_ResendTimeout.IsInfinite()) {
        NCBI_THROW(CPSG_Exception, eParameterMissing, "Infinite resend timeout is not supported");
    } else if (!m_ResendTimeout.IsDefault()) {
        os << "&resend_timeout=" << m_ResendTimeout.GetAsDouble();
    }
}

void CPSG_Request_Resolve::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/resolve?" << m_BioId << "&fmt=json";

    auto value = "yes";
    auto include_info = m_IncludeInfo;
    const auto max_bit = (numeric_limits<unsigned>::max() >> 1) + 1;

    if (include_info & CPSG_Request_Resolve::TIncludeInfo(max_bit)) {
        os << "&all_info=yes";
        value = "no";
        include_info = ~include_info;
    }

    if (include_info & CPSG_Request_Resolve::fCanonicalId)  os << "&canon_id=" << value;
    if (include_info & CPSG_Request_Resolve::fName)         os << "&name=" << value;
    if (include_info & CPSG_Request_Resolve::fOtherIds)     os << "&seq_ids=" << value;
    if (include_info & CPSG_Request_Resolve::fMoleculeType) os << "&mol_type=" << value;
    if (include_info & CPSG_Request_Resolve::fLength)       os << "&length=" << value;
    if (include_info & CPSG_Request_Resolve::fChainState)   os << "&seq_state=" << value;
    if (include_info & CPSG_Request_Resolve::fState)        os << "&state=" << value;
    if (include_info & CPSG_Request_Resolve::fBlobId)       os << "&blob_id=" << value;
    if (include_info & CPSG_Request_Resolve::fTaxId)        os << "&tax_id=" << value;
    if (include_info & CPSG_Request_Resolve::fHash)         os << "&hash=" << value;
    if (include_info & CPSG_Request_Resolve::fDateChanged)  os << "&date_changed=" << value;
    if (include_info & CPSG_Request_Resolve::fGi)           os << "&gi=" << value;

    os << s_GetAccSubstitution(m_AccSubstitution);
}

void CPSG_Request_Blob::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/getblob?" << m_BlobId;

    if (const auto tse = s_GetTSE(m_IncludeData)) os << "&tse=" << tse;
}

string s_GetFastaString(const CPSG_BioId& bio_id)
{
    const auto& id = bio_id.GetId();
    auto type = bio_id.GetType();

    try {
        return type ? objects::CSeq_id(type, id).AsFastaString() : id;
    }
    catch (objects::CSeqIdException&) {
        return id;
    }
}

void CPSG_Request_NamedAnnotInfo::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/get_na?";

    s_DelimitedOutput(m_BioIds, os, "seq_ids=", ' ', s_GetFastaString);
    s_DelimitedOutput(m_AnnotNames, os, "&names=", ',', [](const auto& name) { return name; });

    if (const auto tse = s_GetTSE(m_IncludeData)) os << "&tse=" << tse;

    os << s_GetAccSubstitution(m_AccSubstitution);
}

void CPSG_Request_Chunk::x_GetAbsPathRef(ostream& os) const
{
    os << "/ID/get_tse_chunk?" << m_ChunkId;
}

shared_ptr<CPSG_Reply> CPSG_Queue::SImpl::SendRequestAndGetReply(shared_ptr<CPSG_Request> r, CDeadline deadline)
{
    _ASSERT(queue);

    if (!r) {
        NCBI_THROW(CPSG_Exception, eParameterMissing, "request cannot be empty");
    }

    auto user_request = const_pointer_cast<const CPSG_Request>(r);
    auto& ioc = m_Service.ioc;
    auto& params = ioc.params;
    auto& stats = ioc.stats;

    auto user_context = params.user_request_ids ? user_request->GetUserContext<string>() : nullptr;
    const auto request_id = user_context ? *user_context : ioc.GetNewRequestId();
    auto reply = make_shared<SPSG_Reply>(move(request_id), params, queue, stats);
    auto abs_path_ref = x_GetAbsPathRef(user_request);
    const auto& request_context = user_request->m_RequestContext;

    _ASSERT(request_context);

    auto request = make_shared<SPSG_Request>(move(abs_path_ref), reply, request_context->Clone(), params);

    if (ioc.AddRequest(request, queue->Stopped(), deadline)) {
        if (stats) stats->IncCounter(SPSG_Stats::eRequest, user_request->GetType());
        shared_ptr<CPSG_Reply> user_reply(new CPSG_Reply);
        user_reply->m_Impl->reply = move(reply);
        user_reply->m_Impl->user_reply = user_reply;
        user_reply->m_Request = move(user_request);
        return user_reply;
    }

    return {};
}

bool CPSG_Queue::SImpl::SendRequest(shared_ptr<CPSG_Request> request, CDeadline deadline)
{
    _ASSERT(queue);

    if (auto user_reply = SendRequestAndGetReply(move(request), move(deadline))) {
        queue->Push(move(user_reply));
        return true;
    }

    return false;
}

bool CPSG_Queue::SImpl::WaitForEvents(CDeadline deadline)
{
    _ASSERT(queue);

    if (queue->CV().WaitUntil(queue->Stopped(), move(deadline), false, true)) {
        queue->CV().Reset();
        return true;
    }

    return false;
}

EPSG_Status s_GetStatus(SPSG_Reply::SItem::TTS& ts, const CDeadline& deadline)
{
    auto& state = ts->state;

    do {
        auto status = state.GetStatus();

        if (status != EPSG_Status::eInProgress) {
            return status;
        }
    }
    while (state.change.WaitUntil(deadline));

    return EPSG_Status::eInProgress;
}

EPSG_Status CPSG_ReplyItem::GetStatus(CDeadline deadline) const
{
    assert(m_Impl);

    return s_GetStatus(m_Impl->item, deadline);
}

string CPSG_ReplyItem::GetNextMessage() const
{
    assert(m_Impl);

    return m_Impl->item.GetLock()->state.GetError();
}

CPSG_ReplyItem::~CPSG_ReplyItem()
{
}

CPSG_ReplyItem::CPSG_ReplyItem(EType type) :
    m_Type(type)
{
}


CPSG_BlobData::CPSG_BlobData(unique_ptr<CPSG_DataId> id) :
    CPSG_ReplyItem(eBlobData),
    m_Id(move(id))
{
}


CPSG_BlobInfo::CPSG_BlobInfo(unique_ptr<CPSG_DataId> id) :
    CPSG_ReplyItem(eBlobInfo),
    m_Id(move(id))
{
}

enum EPSG_BlobInfo_Flags
{
    fPSGBI_CheckFailed = 1 << 0,
    fPSGBI_Gzip        = 1 << 1,
    fPSGBI_Not4Gbu     = 1 << 2,
    fPSGBI_Withdrawn   = 1 << 3,
    fPSGBI_Suppress    = 1 << 4,
    fPSGBI_Dead        = 1 << 5,
};

string CPSG_BlobInfo::GetCompression() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Gzip ? "gzip" : "";
}

string CPSG_BlobInfo::GetFormat() const
{
    return "asn.1";
}

Uint8 CPSG_BlobInfo::GetStorageSize() const
{
    return static_cast<Uint8>(m_Data.GetInteger("size"));
}

Uint8 CPSG_BlobInfo::GetSize() const
{
    return static_cast<Uint8>(m_Data.GetInteger("size_unpacked"));
}

bool CPSG_BlobInfo::IsDead() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Dead;
}

bool CPSG_BlobInfo::IsSuppressed() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Suppress;
}

bool CPSG_BlobInfo::IsWithdrawn() const
{
    return m_Data.GetInteger("flags") & fPSGBI_Withdrawn;
}

CTime s_GetTime(Int8 milliseconds)
{
    return milliseconds > 0 ? CTime(static_cast<time_t>(milliseconds / kMilliSecondsPerSecond)) : CTime();
}

CTime CPSG_BlobInfo::GetHupReleaseDate() const
{
    return s_GetTime(m_Data.GetInteger("hup_date"));
}

Uint8 CPSG_BlobInfo::GetOwner() const
{
    return static_cast<Uint8>(m_Data.GetInteger("owner"));
}

CTime CPSG_BlobInfo::GetOriginalLoadDate() const
{
    return s_GetTime(m_Data.GetInteger("date_asn1"));
}

objects::CBioseq_set::EClass CPSG_BlobInfo::GetClass() const
{
    return static_cast<objects::CBioseq_set::EClass>(m_Data.GetInteger("class"));
}

string CPSG_BlobInfo::GetDivision() const
{
    return m_Data.GetString("div");
}

string CPSG_BlobInfo::GetUsername() const
{
    return m_Data.GetString("username");
}

string CPSG_BlobInfo::GetId2Info() const
{
    return m_Data.GetString("id2_info");
}

Uint8 CPSG_BlobInfo::GetNChunks() const
{
    return static_cast<Uint8>(m_Data.GetInteger("n_chunks"));
}


CPSG_SkippedBlob::CPSG_SkippedBlob(unique_ptr<CPSG_DataId> id, EReason reason, TSeconds sent_seconds_ago, TSeconds time_until_resend) :
    CPSG_ReplyItem(eSkippedBlob),
    m_Id(move(id)),
    m_Reason(reason),
    m_SentSecondsAgo(move(sent_seconds_ago)),
    m_TimeUntilResend(move(time_until_resend))
{
}


CPSG_BioseqInfo::CPSG_BioseqInfo()
    : CPSG_ReplyItem(eBioseqInfo)
{
}

CPSG_BioId CPSG_BioseqInfo::GetCanonicalId() const
{
    return s_GetBioId(m_Data);
};

vector<CPSG_BioId> CPSG_BioseqInfo::GetOtherIds() const
{
    auto seq_ids = m_Data.GetByKey("seq_ids");
    vector<CPSG_BioId> rv;
    bool error = !seq_ids.IsArray();

    for (CJsonIterator it = seq_ids.Iterate(); !error && it.IsValid(); it.Next()) {
        auto seq_id = it.GetNode();
        error = !seq_id.IsArray() || (seq_id.GetSize() != 2);

        if (!error) {
            auto type = static_cast<CPSG_BioId::TType>(seq_id.GetAt(0).AsInteger());
            auto content = seq_id.GetAt(1).AsString();
            rv.emplace_back(string(objects::CSeq_id::WhichFastaTag(type))
                            + '|' + content, type);
        }
    }

    if (error) {
        auto reply = GetReply();
        _ASSERT(reply);

        auto request = reply->GetRequest().get();
        _ASSERT(request);

        NCBI_THROW_FMT(CPSG_Exception, eServerError, "Wrong seq_ids format: '" << seq_ids.Repr() <<
                "' for " << s_GetRequestTypeName(request->GetType()) << " request '" << request->GetId() << '\'');
    }

    return rv;
}

objects::CSeq_inst::TMol CPSG_BioseqInfo::GetMoleculeType() const
{
    return static_cast<objects::CSeq_inst::TMol>(m_Data.GetInteger("mol"));
}

Uint8 CPSG_BioseqInfo::GetLength() const
{
    return m_Data.GetInteger("length");
}

CPSG_BioseqInfo::TState CPSG_BioseqInfo::GetChainState() const
{
    return static_cast<TState>(m_Data.GetInteger("seq_state"));
}

CPSG_BioseqInfo::TState CPSG_BioseqInfo::GetState() const
{
    return static_cast<TState>(m_Data.GetInteger("state"));
}

CPSG_BlobId CPSG_BioseqInfo::GetBlobId() const
{
    return s_GetBlobId(m_Data);
}

TTaxId CPSG_BioseqInfo::GetTaxId() const
{
    return TAX_ID_FROM(Int8, m_Data.GetInteger("tax_id"));
}

int CPSG_BioseqInfo::GetHash() const
{
    return static_cast<int>(m_Data.GetInteger("hash"));
}

CTime CPSG_BioseqInfo::GetDateChanged() const
{
    return s_GetTime(m_Data.GetInteger("date_changed"));
}

TGi CPSG_BioseqInfo::GetGi() const
{
    return static_cast<TGi>(m_Data.GetInteger("gi"));
}

CPSG_Request_Resolve::TIncludeInfo CPSG_BioseqInfo::IncludedInfo() const
{
    CPSG_Request_Resolve::TIncludeInfo rv = {};

    if (m_Data.HasKey("accession") && m_Data.HasKey("seq_id_type"))       rv |= CPSG_Request_Resolve::fCanonicalId;
    if (m_Data.HasKey("name"))                                            rv |= CPSG_Request_Resolve::fName;
    if (m_Data.HasKey("seq_ids") && m_Data.GetByKey("seq_ids").GetSize()) rv |= CPSG_Request_Resolve::fOtherIds;
    if (m_Data.HasKey("mol"))                                             rv |= CPSG_Request_Resolve::fMoleculeType;
    if (m_Data.HasKey("length"))                                          rv |= CPSG_Request_Resolve::fLength;
    if (m_Data.HasKey("seq_state"))                                       rv |= CPSG_Request_Resolve::fChainState;
    if (m_Data.HasKey("state"))                                           rv |= CPSG_Request_Resolve::fState;
    if (m_Data.HasKey("blob_id") ||
        (m_Data.HasKey("sat") && m_Data.HasKey("sat_key")))               rv |= CPSG_Request_Resolve::fBlobId;
    if (m_Data.HasKey("tax_id"))                                          rv |= CPSG_Request_Resolve::fTaxId;
    if (m_Data.HasKey("hash"))                                            rv |= CPSG_Request_Resolve::fHash;
    if (m_Data.HasKey("date_changed"))                                    rv |= CPSG_Request_Resolve::fDateChanged;
    if (m_Data.HasKey("gi"))                                              rv |= CPSG_Request_Resolve::fGi;

    return rv;
}


CPSG_NamedAnnotInfo::CPSG_NamedAnnotInfo(string name) :
    CPSG_ReplyItem(eNamedAnnotInfo),
    m_Name(move(name))
{
}


string CPSG_NamedAnnotInfo::GetId2AnnotInfo() const
{
    auto node = m_Data.GetByKeyOrNull("seq_annot_info");
    return node && node.IsString() ? node.AsString() : string();
}


CPSG_NamedAnnotInfo::TId2AnnotInfoList CPSG_NamedAnnotInfo::GetId2AnnotInfoList() const
{
    TId2AnnotInfoList ret;
    auto info_str = GetId2AnnotInfo();
    if (!info_str.empty()) {
        auto in_string = NStr::Base64Decode(info_str);
        istringstream in_stream(in_string);
        CObjectIStreamAsnBinary in(in_stream);
        while ( in.HaveMoreData() ) {
            CRef<TId2AnnotInfo> info(new TId2AnnotInfo);
            in >> *info;
            ret.push_back(info);
        }
    }
    return ret;
}


CPSG_BlobId CPSG_NamedAnnotInfo::GetBlobId() const
{
    return s_GetBlobId(m_Data);
}


CPSG_PublicComment::CPSG_PublicComment(unique_ptr<CPSG_DataId> id, string text) :
    CPSG_ReplyItem(ePublicComment),
    m_Id(move(id)),
    m_Text(move(text))
{
}


CPSG_Processor::CPSG_Processor(EProgressStatus progress_status) :
    CPSG_ReplyItem(eProcessor),
    m_ProgressStatus(progress_status)
{
}


CPSG_Reply::CPSG_Reply() :
    m_Impl(new SImpl)
{
}

EPSG_Status CPSG_Reply::GetStatus(CDeadline deadline) const
{
    assert(m_Impl);

    return s_GetStatus(m_Impl->reply->reply_item, deadline);
}

string CPSG_Reply::GetNextMessage() const
{
    assert(m_Impl);
    assert(m_Impl->reply);

    return m_Impl->reply->reply_item.GetLock()->state.GetError();
}

shared_ptr<CPSG_ReplyItem> CPSG_Reply::GetNextItem(CDeadline deadline)
{
    assert(m_Impl);
    assert(m_Impl->reply);

    auto& reply_item = m_Impl->reply->reply_item;
    auto& reply_state = reply_item->state;

    do {
        bool was_in_progress = reply_state.InProgress();

        if (auto items_locked = m_Impl->reply->items.GetLock()) {
            auto& items = *items_locked;

            for (auto& item_ts : items) {
                const auto& item_state = item_ts->state;

                if (item_state.Returned()) continue;

                if (item_state.Empty()) {
                    auto item_locked = item_ts.GetLock();
                    auto& item = *item_locked;

                    // Wait for more chunks on this item
                    if (!item.expected.Cmp<less_equal>(item.received)) continue;
                }

                // Do not hold lock on item_ts around this call!
                if (auto rv = m_Impl->Create(item_ts)) {
                    return rv;
                }

                continue;
            }
        }

        // No more reply items
        if (!was_in_progress) {
            return shared_ptr<CPSG_ReplyItem>(new CPSG_ReplyItem(CPSG_ReplyItem::eEndOfReply));
        }
    }
    while (reply_item.WaitUntil(reply_state.GetState(), deadline, SPSG_Reply::SState::eInProgress, true));

    return {};
}

CPSG_Reply::~CPSG_Reply()
{
}


CPSG_Queue::CPSG_Queue() = default;
CPSG_Queue::CPSG_Queue(CPSG_Queue&&) = default;
CPSG_Queue& CPSG_Queue::operator=(CPSG_Queue&&) = default;
CPSG_Queue::~CPSG_Queue() = default;

CPSG_Queue::CPSG_Queue(const string& service) :
    m_Impl(new SImpl(service))
{
}

bool CPSG_Queue::SendRequest(shared_ptr<CPSG_Request> request, CDeadline deadline)
{
    _ASSERT(m_Impl);
    return m_Impl->SendRequest(move(request), move(deadline));
}

shared_ptr<CPSG_Reply> CPSG_Queue::GetNextReply(CDeadline deadline)
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    shared_ptr<CPSG_Reply> rv;
    m_Impl->queue->Pop(rv, deadline);
    return rv;
}

shared_ptr<CPSG_Reply> CPSG_Queue::SendRequestAndGetReply(shared_ptr<CPSG_Request> request, CDeadline deadline)
{
    _ASSERT(m_Impl);
    return m_Impl->SendRequestAndGetReply(move(request), move(deadline));
}

void CPSG_Queue::Stop()
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    m_Impl->queue->Stop(m_Impl->queue->eDrain);
}

bool CPSG_Queue::WaitForEvents(CDeadline deadline)
{
    _ASSERT(m_Impl);
    return m_Impl->WaitForEvents(move(deadline));
}

void CPSG_Queue::Reset()
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    m_Impl->queue->Stop(m_Impl->queue->eClear);
}

bool CPSG_Queue::IsEmpty() const
{
    _ASSERT(m_Impl);
    _ASSERT(m_Impl->queue);
    return m_Impl->queue->Empty();
}

bool CPSG_Queue::RejectsRequests() const
{
    _ASSERT(m_Impl);
    return m_Impl->RejectsRequests();
}


void CPSG_Queue::SetUserArgs(SPSG_UserArgs user_args)
{
    m_Impl->SetUserArgs(move(user_args));
}


CPSG_Queue::TApiLock CPSG_Queue::GetApiLock()
{
    return SImpl::GetApiLock();
}


CPSG_EventLoop::CPSG_EventLoop() = default;
CPSG_EventLoop::CPSG_EventLoop(CPSG_EventLoop&&) = default;
CPSG_EventLoop& CPSG_EventLoop::operator=(CPSG_EventLoop&&) = default;

CPSG_EventLoop::CPSG_EventLoop(const string&  service, TItemComplete item_complete, TReplyComplete reply_complete, TNewItem new_item) :
    CPSG_Queue(service),
    m_ItemComplete(move(item_complete)),
    m_ReplyComplete(move(reply_complete)),
    m_NewItem(move(new_item))
{
    if (!m_ItemComplete) {
        NCBI_THROW(CPSG_Exception, eParameterMissing, "item_complete cannot be empty");
    }

    if (!m_ReplyComplete) {
        NCBI_THROW(CPSG_Exception, eParameterMissing, "reply_complete cannot be empty");
    }
}

bool CPSG_EventLoop::RunOnce(CDeadline deadline)
{
    if (!WaitForEvents(deadline)) {
        return false;
    }

    while (auto reply = GetNextReply(CDeadline::eNoWait)) {
        m_Replies.emplace_back(move(reply), 0);
    }

    for (auto i = m_Replies.begin(); i != m_Replies.end();) {
        auto& reply = i->first;
        auto& items = i->second;
        bool end_of_reply = false;

        while (auto item = reply->GetNextItem(CDeadline::eNoWait)) {
            if (item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                // No need to store this end_of_reply in m_Replies,
                // as it means this reply's status is not eInProgress already (see GetNextItem)
                // and this reply will be completed (and removed from m_Replies) in this iteration
                end_of_reply = true;
                break;
            }

            if (m_NewItem) {
                m_NewItem(item);
            }

            items.emplace_back(move(item));
        }

        for (auto j = items.begin(); j != items.end();) {
            auto& item = *j;
            auto status = item->GetStatus(CDeadline::eNoWait);

            if (status == EPSG_Status::eInProgress) {
                ++j;
            } else {
                m_ItemComplete(status, item);
                j = items.erase(j);
            }
        }

        // Allow reply complete event only after all of its items are complete
        if (end_of_reply && items.empty()) {
            auto status = reply->GetStatus(CDeadline::eNoWait);

            if (status != EPSG_Status::eInProgress) {
                m_ReplyComplete(status, reply);
                i = m_Replies.erase(i);
                continue;
            }
        }

        ++i;
    }

    return true;
}


END_NCBI_SCOPE

#endif
