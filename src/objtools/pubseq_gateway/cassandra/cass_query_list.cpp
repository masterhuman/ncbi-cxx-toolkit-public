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
 * CCassQueryList class that maintains list of async queries against Cass*
 */

#include <ncbi_pch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_query_list.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

constexpr const unsigned int CCassQueryList::k_dflt_max_stmt;

static string AllParams(shared_ptr<CCassQuery> qry) {
    string rv;
    for (size_t i = 0; i < qry->ParamCount(); ++i) {
        if (i > 0)
            rv.append(", ");
        rv.append(qry->ParamAsStr(i));
    }
    return rv;
}

void CCassQueryList::Execute(ICassQueryListConsumer* consumer, int retry_count, bool post_async) {
    unique_ptr<ICassQueryListConsumer> lconsumer(consumer);
    SQrySlot* slot;
    do {
        if (m_query_arr.size() < m_max_queries) {
            if (m_query_arr.empty() && m_notification_arr.empty()) {
                SetMaxQueries(m_max_queries);
            }
            size_t index = m_query_arr.size();
            while (m_query_arr.size() < m_max_queries)
                m_query_arr.push_back({nullptr, nullptr, 0, ssAvailable, m_query_arr.size()});
            slot = &m_query_arr[index];
        }
        else {
            slot = WaitAny(false);
        }
        if (post_async && !slot) {
            m_pending_arr.push_back({move(lconsumer), retry_count});
            Tick();
            return;
        }
    }
    while (!slot);

    AttachSlot(slot, {move(lconsumer), retry_count});
}

void CCassQueryList::DetachSlot(SQrySlot* slot) {
    slot->m_qry->Close();
    slot->m_state = ssAvailable;
}

void CCassQueryList::AttachSlot(SQrySlot* slot, SPendingSlot&& pending_slot) {
    assert(slot->m_state == ssAvailable);
    slot->m_state = ssAttached;
    slot->m_retry_count = pending_slot.m_retry_count;
    slot->m_consumer = move(pending_slot.m_consumer);

    if (!slot->m_qry)
        slot->m_qry = m_cass_conn->NewQuery();
    else
        slot->m_qry->Close();
    assert(slot->m_index < m_notification_arr.size());
    slot->m_qry->SetOnData(SOnDataCB, &m_notification_arr[slot->m_index]);
    if (slot->m_consumer) {
        bool b = slot->m_consumer->Start(*slot->m_qry, *this);
        if (!b) {
            ERR_POST(Info << "Consumer refused to start, detaching...");
            assert(!slot->m_qry->IsActive());
            DetachSlot(slot);
        }
    }
    Tick();
}

void CCassQueryList::CheckPending(SQrySlot* slot) {
    assert(slot->m_state == ssAvailable);    
    if (!m_pending_arr.empty()) {
        AttachSlot(slot, move(m_pending_arr.back()));
        m_pending_arr.pop_back();
    }
}

void CCassQueryList::Release(SQrySlot* slot) {
    bool release = true;
    assert(slot->m_state != ssReleasing);
    assert(slot->m_state != ssAvailable);
    slot->m_state = ssReleasing;
    if (slot->m_consumer) {
        release = slot->m_consumer->Finish(*slot->m_qry, *this);
        if (release)
            slot->m_consumer = nullptr;
    }
    if (release) {
        DetachSlot(slot);
        CheckPending(slot);
    }
    else {
        assert(slot->m_qry);
        assert(slot->m_qry->IsActive());
        slot->m_state = ssAttached;
    }
}

void CCassQueryList::ReadRows(SQrySlot* slot) {
    bool do_continue = true;
    assert(slot->m_state == ssAttached);

    async_rslt_t state = slot->m_qry->NextRow();
    while (do_continue) {
        do_continue = false;
        switch (state) {
            case ar_done: {
                if (slot->m_state != ssReleasing)
                    Release(slot);
                break;
            }
            case ar_wait:
                break;
            case ar_dataready:
                assert(slot->m_state != ssReadingRows);
                if (slot->m_state != ssReadingRows) {
                    slot->m_state = ssReadingRows;
                    do_continue = !slot->m_consumer || slot->m_consumer->ProcessRow(*slot->m_qry, *this);
                    slot->m_state = ssAttached;
                }

                if (do_continue)
                    state = slot->m_qry->NextRow();
                else if (slot->m_state != ssReleasing)
                    Release(slot);
                break;
            default:;
        }
    }
}

void CCassQueryList::Cancel(const exception* e) {
    m_pending_arr.clear();
    for (ssize_t i = 0; i < static_cast<ssize_t>(m_query_arr.size()); ++i) {
        auto slot = &m_query_arr[i];
        if (slot->m_state != ssAvailable) {
            m_has_error = true;
            if (slot->m_consumer)
                slot->m_consumer->Failed(*slot->m_qry, *this, e);
            DetachSlot(slot);
        }
        slot->m_qry = nullptr;
        Tick();
    }
    m_query_arr.clear();
    m_pending_arr.clear();
}

void CCassQueryList::Wait() {
    while (!m_pending_arr.empty()) {
        SQrySlot *slot = WaitAny(false);
        if (slot)
            CheckPending(slot);
    }
    bool any;
    do {
            
        any = false;
        for (auto&& slot : m_query_arr) {
            if (slot.m_qry) {
                any = true;
                break;
            }
        }
        if (any)
            WaitAny(true);
        if (SSignalHandler::s_CtrlCPressed())
            RAISE_ERROR(eUserCancelled, "SIGINT delivered");
    } while (any);
    Tick();
    m_query_arr.clear();
}

CCassQueryList::SQrySlot* CCassQueryList::WaitAny(bool discard) {
    if (discard) {
        while (!m_query_arr.empty()) {
            size_t index;
            if (!m_ready.pop(&index))
                index = m_query_arr.size() - 1;
            while (m_query_arr[index].m_qry == nullptr && index > 0)
                --index;
            if (index == 0 && m_query_arr[index].m_qry == nullptr)
                return nullptr;

            SQrySlot *slot = WaitOne(index, discard);
            if (slot)
                return slot;
        }
    }
    else {
        for (size_t index = 0; index < m_query_arr.size(); ++index) {
            SQrySlot *slot = &m_query_arr[index];
            if (slot->m_state == ssAvailable)
                return slot;
        }
        size_t index = numeric_limits<size_t>::max();
        while (m_ready.pop_wait(&index, k_ready_wait_timeout)) {
            SQrySlot *slot = WaitOne(index, discard);
            if (slot)
                return slot;
        }
    }
    return nullptr;
}

CCassQueryList::SQrySlot* CCassQueryList::WaitOne(size_t index, bool discard) {
    assert(index < m_query_arr.size());
    if (index >= m_query_arr.size())
        return nullptr;
    SQrySlot *slot = &m_query_arr[index];
    if (slot->m_qry) {
        bool need_restart = false;
        try {
            if (slot->m_qry->IsActive()) {
                switch (slot->m_qry->WaitAsync(0)) {
                    case ar_done:
                        if (slot->m_state != ssReleasing)
                            Release(slot);
                        break;
                    case ar_dataready:
                        if (slot->m_state != ssReadingRows)
                            ReadRows(slot);
                        break;
                    default:;
                }
            }
        }
        catch (const CCassandraException& e) {
            if ((e.GetErrCode() == CCassandraException::eQueryTimeout || e.GetErrCode() == CCassandraException::eQueryFailedRestartable) &&
                --slot->m_retry_count > 0) 
            {
                need_restart = true;
                if (slot->m_state != ssReseting && slot->m_consumer) {
                    slot->m_state = ssReseting;
                    slot->m_consumer->Reset(*slot->m_qry, *this);
                    slot->m_state = ssAttached;
                }
                else {
                    slot->m_state = ssAttached;
                }
                    
                ERR_POST(Error << "CCassQueryList::WaitAny: exception (IGNORING & RESTARTING) [" << index << "]: " << e.what() <<  "\nparams: " << AllParams(slot->m_qry).c_str());
            }
            else {
                m_has_error = true;
                if (slot->m_consumer)
                    slot->m_consumer->Failed(*slot->m_qry, *this, &e);
                DetachSlot(slot);
                throw;
            }
        }
        if (need_restart)
            slot->m_qry->Restart();
        if (slot->m_state == ssAvailable) {
            if (discard) {
                slot->m_qry = m_query_arr[index].m_qry = nullptr;
            }
            else if (slot->m_qry && slot->m_qry->IsAsync()) {
                slot->m_qry->Close();
            }
            return slot;
        }
    }
    else if (!discard && slot->m_state == ssAvailable) {
        return slot;
    }

    if (SSignalHandler::s_CtrlCPressed())
        RAISE_ERROR(eUserCancelled, "SIGINT delivered");
    return nullptr;
}

END_IDBLOB_SCOPE

