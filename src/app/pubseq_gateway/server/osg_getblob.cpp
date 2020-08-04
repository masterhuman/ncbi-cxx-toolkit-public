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
 * Authors: Eugene Vasilchenko
 *
 * File Description: processor for data from OSG
 *
 */

#include <ncbi_pch.hpp>

#include "osg_getblob.hpp"
#include "osg_fetch.hpp"

#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


CPSGS_OSGGetBlob::CPSGS_OSGGetBlob(const CRef<COSGConnectionPool>& pool,
                                   const shared_ptr<CPSGS_Request>& request,
                                   const shared_ptr<CPSGS_Reply>& reply,
                                   TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(pool, request, reply, priority)
{
}


string CPSGS_OSGGetBlob::GetName() const
{
    return "OSG processor: get blob";
}


void CPSGS_OSGGetBlob::CreateRequests()
{
    CRef<CID2_Request> osg_req(new CID2_Request);
    auto& psg_req = GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    auto& req = osg_req->SetRequest().SetGet_blob_info();
    req.SetBlob_id().SetBlob_id(*GetOSGBlobId(psg_req.m_BlobId));
    req.SetGet_data();
    AddRequest(osg_req);
}


void CPSGS_OSGGetBlob::ProcessReplies()
{
    for ( auto& f : GetFetches() ) {
        for ( auto& r : f->GetReplies() ) {
            switch ( r->GetReply().Which() ) {
            case CID2_Reply::TReply::e_Init:
            case CID2_Reply::TReply::e_Empty:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_blob:
                ProcessBlobReply(*r);
                break;
            case CID2_Reply::TReply::e_Get_split_info:
                ProcessBlobReply(*r);
                break;
            default:
                ERR_POST(GetName()<<": "
                         "Unknown reply to "<<MSerial_AsnText<<*f->GetRequest()<<"\n"<<*r);
                break;
            }
        }
    }
    SendBlob();
    FinalizeResult();
}


CPSGS_OSGGetChunks::CPSGS_OSGGetChunks(const CRef<COSGConnectionPool>& pool,
                                       const shared_ptr<CPSGS_Request>& request,
                                       const shared_ptr<CPSGS_Reply>& reply,
                                       TProcessorPriority priority)
    : CPSGS_OSGProcessorBase(pool, request, reply, priority)
{
}


string CPSGS_OSGGetChunks::GetName() const
{
    return "OSG processor: get chunks";
}


void CPSGS_OSGGetChunks::CreateRequests()
{
    CRef<CID2_Request> osg_req(new CID2_Request);
    auto& psg_req = GetRequest()->GetRequest<SPSGS_TSEChunkRequest>();
    auto& req = osg_req->SetRequest().SetGet_chunks();
    req.SetBlob_id(*GetOSGBlobId(psg_req.m_TSEId));
    req.SetSplit_version(psg_req.m_SplitVersion);
    // TODO: multiple chunks in request
    CID2S_Chunk_Id chunk_id;
    chunk_id.Set(psg_req.m_Chunk);
    req.SetChunks().push_back(chunk_id);
    AddRequest(osg_req);
}


void CPSGS_OSGGetChunks::ProcessReplies()
{
    for ( auto& f : GetFetches() ) {
        for ( auto& r : f->GetReplies() ) {
            switch ( r->GetReply().Which() ) {
            case CID2_Reply::TReply::e_Init:
            case CID2_Reply::TReply::e_Empty:
                // do nothing
                break;
            case CID2_Reply::TReply::e_Get_chunk:
                ProcessBlobReply(*r);
                break;
            default:
                ERR_POST(GetName()<<": "
                         "Unknown reply to "<<MSerial_AsnText<<*f->GetRequest()<<"\n"<<*r);
                break;
            }
        }
    }
    SendBlob();
    FinalizeResult();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
