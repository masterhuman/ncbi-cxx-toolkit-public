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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 * Cassandra fetch blob status history record
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/status_history/fetch.hpp>

#include <memory>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE

CCassStatusHistoryTaskFetch::CCassStatusHistoryTaskFetch(
    shared_ptr<CCassConnection> connection,
    const string &keyspace,
    int32_t sat_key,
    int64_t done_when,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(move(connection), keyspace, sat_key, true, move(data_error_cb))
    , m_DoneWhen(done_when)
{}

void CCassStatusHistoryTaskFetch::Wait1()
{
    bool need_repeat{false};
        do {
            need_repeat = false;
            switch (m_State) {
                case eError:
                case eDone:
                    return;

                case eInit: {
                    m_QueryArr.resize(1);
                    m_QueryArr[0] = {m_Conn->NewQuery(), 0};
                    auto query = m_QueryArr[0].query;
                    string sql =
                        "SELECT comment, public_comment, flags, replaces, username, replaces_ids, replaced_by_ids "
                        "FROM " + GetKeySpace() + ".blob_status_history "
                        "WHERE sat_key = ? and done_when = ?";
                    query->SetSQL(sql, 2);
                    query->BindInt32(0, GetKey());
                    query->BindInt64(1, m_DoneWhen);

                    SetupQueryCB3(m_QueryArr[0].query);
                    m_QueryArr[0].query->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async, true);
                    m_State = eFetchStarted;
                    break;
                }

                case eFetchStarted: {
                    auto query = m_QueryArr[0].query;
                    if (CheckReady(m_QueryArr[0])) {
                        if (query->NextRow() == ar_dataready) {
                            m_Record = make_unique<CBlobStatusHistoryRecord>();
                            (*m_Record)
                                .SetKey(GetKey())
                                .SetDoneWhen(m_DoneWhen)
                                .SetComment(query->FieldGetStrValueDef(0, ""))
                                .SetPublicComment(query->FieldGetStrValueDef(1, ""))
                                .SetFlags(query->FieldGetInt64Value(2, 0))
                                .SetReplaces(query->FieldGetInt32Value(3, 0))
                                .SetUserName(query->FieldGetStrValueDef(4, ""));
                            vector<int32_t> replaces_ids, replaced_by_ids;
                            query->FieldGetContainerValue(5, back_inserter(replaces_ids));
                            query->FieldGetContainerValue(6, back_inserter(replaced_by_ids));
                            m_Record->SetReplacesIds(move(replaces_ids));
                            m_Record->SetReplacedByIds(move(replaced_by_ids));
                            m_State = eDone;
                            CloseAll();
                            need_repeat = true;
                        }
                    }
                    if (query->IsEOF()) {
                        m_State = eDone;
                        CloseAll();
                        need_repeat = true;
                    }
                    break;
                }

                default: {
                    char msg[1024];
                    snprintf(msg, sizeof(msg), "Failed to fetch blob status history (key=%i.%ld) unexpected state (%d)",
                        GetKey(), m_DoneWhen, static_cast<int>(m_State));
                    Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
                }
            }
        } while(need_repeat);
}

unique_ptr<CBlobStatusHistoryRecord> CCassStatusHistoryTaskFetch::Consume()
{
    unique_ptr<CBlobStatusHistoryRecord> tmp;
    swap(tmp, m_Record);
    return tmp;
}

END_IDBLOB_SCOPE
