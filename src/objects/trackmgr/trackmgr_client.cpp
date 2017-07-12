/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'trackmgr.asn'.
 */

#include <ncbi_pch.hpp>
#include <serial/serialimpl.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/trackmgr/trackmgr_client.hpp>
#include <objects/trackmgr/TMgr_DisplayTrackRequest.hpp>
#include <objects/trackmgr/TMgr_DisplayTrackReply.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


CTrackMgrClient::CTrackMgrClient(const string& service)
    : m_HostType(eNamed_service)
{
    SetService(service);
    SetFormat(eSerial_AsnBinary);
    x_Init();
}

CTrackMgrClient::CTrackMgrClient(const string& host, unsigned int port)
    : m_HostType(eHost_port),
      m_Host(host),
      m_Port(port)
{
    SetFormat(eSerial_AsnBinary);
    x_Init();
}

CTrackMgrClient::~CTrackMgrClient(void)
{
}

void
CTrackMgrClient::x_Init(void)
{
    auto_ptr<STimeout> to(new STimeout());
    to->sec = 40;
    SetTimeout(to.release());
}

// override x_Connect to allow for connection to host:port
void
CTrackMgrClient::x_Connect(void)
{
    switch (m_HostType) {
    case eHost_port:
        _ASSERT(!m_Host.empty());
        x_SetStream(new CConn_SocketStream(m_Host, m_Port));
        break;

    case eNamed_service:
        CTrackMgrClient_Base::x_Connect();
        break;
    }
}

CRef<CTMgr_DisplayTrackReply>
CTrackMgrClient::AskDefault_display_tracks(
    const CTMgr_DisplayTrackRequest& req, TReply* reply)
{
    return AskDisplay_tracks(req, reply);
}

CRef<CTMgr_DisplayTrackReply>
CTrackMgrClient::s_Ask(const CTMgr_DisplayTrackRequest& request)
{
    CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
    static const CNcbiApplication* app = CNcbiApplication::Instance();
    const CNcbiRegistry& cfg = app->GetConfig();
    const string type = cfg.GetString("TrackMgr", "type", "service");
    const string name = cfg.GetString("TrackMgr", "name", "TrackMgr");
    const string port_str = cfg.GetString("TrackMgr", "port", "47228");
    const unsigned int port(NStr::StringToInt(port_str));

    CRef<CTrackMgrClient> client;

    if (NStr::EqualNocase(type, "service")) {
        client.Reset(new CTrackMgrClient(name));
    }
    else if (!NStr::EqualNocase(type, "sock")) {
        NCBI_THROW(CException, eUnknown, "Invalid connection type");
    }
    else {
        client.Reset(new CTrackMgrClient(name, port));
    }

    return client.NotNull()
        ? client->AskDefault_display_tracks(request)
        : CRef<CTMgr_DisplayTrackReply>();
}

bool CTrackMgrClient::ParseAlignId (const string& external_id, TAlignIDs& parsed_ids) noexcept
{
    using TStrIDs = vector<string>;

    string delim[3] = {";", ":", ","};
    TStrIDs vec1;

    NStr::Split(external_id, delim[0], vec1, 0, NULL);
    for (auto tok1 : vec1)
    {
        parsed_ids.push_back(SAlignIds());

        int batch_id = NStr::StringToNumeric<int>(tok1, NStr::fConvErr_NoThrow, 10);
        if (batch_id != 0) {
            parsed_ids.back().batch_id = batch_id;
            continue;
        }

        TStrIDs vec2;
        NStr::Split(tok1, delim[1], vec2, 0, NULL);

        int i = -1;
        for (auto tok2 : vec2)
        {
            ++i;
            if (i % 2 == 0)
            {
                int id = NStr::StringToNumeric<int>(tok2, NStr::fConvErr_NoThrow, 10);
                if (id != 0) {
                    parsed_ids.back().batch_id = id;
                    continue;
                }
                else if (i % 2 == 0) {
                    return false;
                }
            }

            TStrIDs vec3;
            NStr::Split(tok2, delim[2], vec3, 0, NULL);
            for (auto tok3 : vec3)
            {
                int align_id = NStr::StringToNumeric<int>(tok3, NStr::fConvErr_NoThrow, 10);
                if (align_id != 0)
                {
                    parsed_ids.back().align_ids.push_back(align_id);
                }
                else
                {
                    return false;
                }
            }
        }

    }
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE

