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
 * Author:  Vinay Kumar
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'gencoll_client.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/genomecoll/genomic_collections_cli.hpp>
#include <objects/genomecoll/GCClient_AttributeFlags.hpp>
#include <objects/genomecoll/GCClient_GetAssemblyReques.hpp>
#include <objects/genomecoll/GCClient_GetAssemblyReques.hpp>
#include <objects/genomecoll/GCClient_Error.hpp>
#include <objects/genomecoll/GC_Assembly.hpp>
#include <sstream>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

const string GENCOLL_URL("http://www.ncbi.nlm.nih.gov/projects/r_gencoll/access/gc_get_assembly.cgi");


// destructor
CGenomicCollectionsService::~CGenomicCollectionsService(void)
{
}


string CGenomicCollectionsService::x_GetURL()
{
    return GENCOLL_URL;
}

void CGenomicCollectionsService::x_Connect()
{
    LOG_POST("Connecting to url:" << x_GetURL().c_str());
    STimeout to5Min;
    to5Min.sec=600;
    to5Min.usec=0;
    SetTimeout(&to5Min);

    x_ConnectURL(x_GetURL());
}


CRef<CGC_Assembly> CGenomicCollectionsService::GetAssembly(string acc, 
                                            int level, 
                                            int asmAttrFlags, 
                                            int chrAttrFlags, 
                                            int scafAttrFlags, 
                                            int compAttrFlags)
{
    CGCClient_GetAssemblyRequest req;
    CGCClientResponse reply;
    
    req.SetAccession(acc);
    req.SetLevel(level);
    req.SetAssm_flags(asmAttrFlags);
    req.SetChrom_flags(chrAttrFlags);
    req.SetScaf_flags(scafAttrFlags);
    req.SetComponent_flags(compAttrFlags);
    
    ostringstream ostrstrm;
    ostrstrm << "Making request - " << MSerial_AsnText << req;
    LOG_POST(ostrstrm.str());
    
    try {
        return AskGet_assembly(req, &reply);
    } catch (CException& ex) {
        if(reply.Which() == CGCClientResponse::e_Srvr_error) {
            ERR_POST(Error << " at Server  side (will be propagated) ...\nErrId:" 
                            << reply.GetSrvr_error().GetError_id() << ": "
                            << reply.GetSrvr_error().GetDescription());
            NCBI_THROW(CException, eUnknown, reply.GetSrvr_error().GetDescription());
        }
        throw;
    }
}


CRef<CGC_Assembly> CGenomicCollectionsService::GetAssembly(int releaseId, 
                                            int level, 
                                            int asmAttrFlags, 
                                            int chrAttrFlags, 
                                            int scafAttrFlags, 
                                            int compAttrFlags)
{
    CGCClient_GetAssemblyRequest req;
    CGCClientResponse reply;
    
    req.SetRelease_id(releaseId);
    req.SetLevel(level);
    req.SetAssm_flags(asmAttrFlags);
    req.SetChrom_flags(chrAttrFlags);
    req.SetScaf_flags(scafAttrFlags);
    req.SetComponent_flags(compAttrFlags);
    
    ostringstream ostrstrm;
    ostrstrm << "Making request -" << MSerial_AsnText << req;
    LOG_POST(ostrstrm.str());
    
    try {
        return AskGet_assembly(req, &reply);
    } catch (CException& ex) {
        if(reply.Which() == CGCClientResponse::e_Srvr_error) {
            ERR_POST(Error << " at Server side (will be propagated) ...\n" 
                            << reply.GetSrvr_error().GetError_id() << ": "
                            << reply.GetSrvr_error().GetDescription());
            NCBI_THROW(CException, eUnknown, reply.GetSrvr_error().GetDescription());
        }
        throw;
    }
}



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1793, CRC32: a9ae6ff4 */
