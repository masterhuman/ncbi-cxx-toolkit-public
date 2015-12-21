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
* Authors:  Eugene Vasilchenko
*
* File Description:
*   Unit test for data loading from ID.
*/

#define NCBI_TEST_APPLICATION

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>

#include <corelib/ncbi_system.hpp>
#include <dbapi/driver/drivers.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <algorithm>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBI_PARAM_DECL(Int8, GENBANK, GI_OFFSET);
NCBI_PARAM_DEF_EX(Int8, GENBANK, GI_OFFSET, 0,
                  eParam_NoThread, GENBANK_GI_OFFSET);

static
TIntId GetGiOffset(void)
{
    static volatile TIntId gi_offset;
    static volatile bool initialized;
    if ( !initialized ) {
        gi_offset = NCBI_PARAM_TYPE(GENBANK, GI_OFFSET)::GetDefault();
        initialized = true;
    }
    return gi_offset;
}


static CRef<CScope> s_InitScope(void)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CDataLoader* loader =
        om->FindDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
    if ( loader ) {
        BOOST_CHECK(om->RevokeDataLoader(*loader));
    }
    else {
#ifdef HAVE_PUBSEQ_OS
        DBAPI_RegisterDriver_FTDS();
        GenBankReaders_Register_Pubseq();
#endif
    }
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    return scope;
}


string s_AsString(const vector<CSeq_id_Handle>& ids)
{
    CNcbiOstrstream out;
    out << '{';
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        out << ' ' <<  *it;
    }
    out << " }";
    return CNcbiOstrstreamToString(out);
}


bool s_RelaxGpipeCheck(void)
{
    return true;
}


bool s_ContainsId(const CSeq_id_Handle& id, const vector<CSeq_id_Handle>& ids)
{
    if ( find(ids.begin(), ids.end(), id) != ids.end() ) {
        return true;
    }
    if ( id.Which() == CSeq_id::e_Gpipe && s_RelaxGpipeCheck() ) {
        return true;
    }
    return false;
}


void s_CompareIdSets(const char* type,
                     const vector<CSeq_id_Handle>& req_ids,
                     const vector<CSeq_id_Handle>& ids,
                     bool exact)
{
    LOG_POST("CompareIdSets: "<<type);
    LOG_POST("req_ids: "<<s_AsString(req_ids));
    LOG_POST("    ids: "<<s_AsString(ids));
    if ( exact ) {
        BOOST_CHECK_EQUAL(ids.size(), req_ids.size());
    }
    else {
        BOOST_CHECK(ids.size() >= req_ids.size());
    }
    ITERATE ( vector<CSeq_id_Handle>, it, req_ids ) {
        LOG_POST("checking "<<*it);
        BOOST_CHECK(s_ContainsId(*it, ids));
    }
}


CSeq_id_Handle s_Normalize(const CSeq_id_Handle& id)
{
    return CSeq_id_Handle::GetHandle(id.AsString());
}


vector<CSeq_id_Handle> s_Normalize(const vector<CSeq_id_Handle>& ids)
{
    vector<CSeq_id_Handle> ret;
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        ret.push_back(s_Normalize(*it));
    }
    return ret;
}


struct SBioseqInfo
{
    string m_RequestId;
    string m_RequiredIds;
    TSeqPos m_MinLength, m_MaxLength;
};


static const SBioseqInfo s_BioseqInfos[] = {
    {
        "NC_000001.10",
        "gi|224589800;ref|NC_000001.10|;gpp|GPC_000001293.1|;gnl|NCBI_GENOMES|1;gnl|ASM:GCF_000001305|1",
        249250621,
        249250621
    }
};


void s_CheckIds(const SBioseqInfo& info, CScope* scope)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(info.m_RequestId);
    vector<CSeq_id_Handle> req_ids;
    {
        vector<string> req_ids_str;
        NStr::Tokenize(info.m_RequiredIds, ";", req_ids_str);
        TIntId gi_offset = GetGiOffset();
        ITERATE ( vector<string>, it, req_ids_str ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*it);
            if ( gi_offset && idh.IsGi() ) {
                idh = CSeq_id_Handle::GetGiHandle(idh.GetGi() + gi_offset);
            }
            req_ids.push_back(idh);
        }
    }

    CScope::TIds ids = s_Normalize(scope->GetIds(idh));
    s_CompareIdSets("expected with get-ids",
                    req_ids, ids, true);

    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_CHECK(bh);
    //s_Print("Actual ", bh.GetId());
    s_CompareIdSets("get-ids with Bioseq",
                    ids, s_Normalize(bh.GetId()), true);
}


void s_CheckSequence(const SBioseqInfo& info, CScope* scope)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(info.m_RequestId);
    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_CHECK(bh);
    TSeqPos len = bh.GetBioseqLength();
    BOOST_CHECK(len >= info.m_MinLength);
    BOOST_CHECK(len <= info.m_MaxLength);
    if ( 0 ) {
        BOOST_CHECK(bh.GetSeqVector().CanGetRange(len/2, len));
        BOOST_CHECK(bh.GetSeqVector().CanGetRange(0, len/2));
        BOOST_CHECK(bh.GetSeqVector().CanGetRange(0, len));
    }
}


void s_CheckAll(const SBioseqInfo& info, CScope* scope)
{
    s_CheckIds(info, scope);
    s_CheckSequence(info, scope);
}


SAnnotSelector s_GetSelector(CSeqFeatData::ESubtype subtype,
                             const char* name = 0)
{
    SAnnotSelector sel(subtype);
    sel.SetResolveAll().SetAdaptiveDepth();
    if ( name ) {
        sel.AddNamedAnnots(name);
    }
    return sel;
}


void s_CheckFeat(const SAnnotSelector& sel,
                 const string& str_id,
                 CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    CRef<CScope> scope = s_InitScope();
    CRef<CSeq_id> seq_id(new CSeq_id(str_id));
    CRef<CSeq_loc> loc(new CSeq_loc);
    if ( range == CRange<TSeqPos>::GetWhole() ) {
        loc->SetWhole(*seq_id);
    }
    else {
        CSeq_interval& interval = loc->SetInt();
        interval.SetId(*seq_id);
        interval.SetFrom(range.GetFrom());
        interval.SetTo(range.GetTo());
    }
    BOOST_CHECK(CFeat_CI(*scope, *loc, sel));

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    BOOST_CHECK(CFeat_CI(bh, range, sel));
}


void s_CheckGraph(const SAnnotSelector& sel,
                  const string& str_id,
                  CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    CRef<CScope> scope = s_InitScope();
    CRef<CSeq_id> seq_id(new CSeq_id(str_id));
    CRef<CSeq_loc> loc(new CSeq_loc);
    if ( range == CRange<TSeqPos>::GetWhole() ) {
        loc->SetWhole(*seq_id);
    }
    else {
        CSeq_interval& interval = loc->SetInt();
        interval.SetId(*seq_id);
        interval.SetFrom(range.GetFrom());
        interval.SetTo(range.GetTo());
    }
    BOOST_CHECK(CGraph_CI(*scope, *loc, sel));

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    BOOST_CHECK(CGraph_CI(bh, range, sel));
}


BOOST_AUTO_TEST_CASE(CheckIds)
{
    for ( size_t i = 0; i < ArraySize(s_BioseqInfos); ++i ) {
        CRef<CScope> scope = s_InitScope();
        s_CheckIds(s_BioseqInfos[i], scope);
    }
}


BOOST_AUTO_TEST_CASE(CheckSequence)
{
    for ( size_t i = 0; i < ArraySize(s_BioseqInfos); ++i ) {
        CRef<CScope> scope = s_InitScope();
        s_CheckSequence(s_BioseqInfos[i], scope);
    }
}


BOOST_AUTO_TEST_CASE(CheckAll)
{
    for ( size_t i = 0; i < ArraySize(s_BioseqInfos); ++i ) {
        CRef<CScope> scope = s_InitScope();
        s_CheckAll(s_BioseqInfos[i], scope);
    }
}


BOOST_AUTO_TEST_CASE(CheckExtSNP)
{
    LOG_POST("Checking ExtAnnot SNP");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("SNP");
    s_CheckFeat(sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNPGraph)
{
    LOG_POST("Checking ExtAnnot SNP graph");
    SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("SNP");
    s_CheckGraph(sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtCDD)
{
    LOG_POST("Checking ExtAnnot CDD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "AAC73113.1");
}


BOOST_AUTO_TEST_CASE(CheckExtMGC)
{
    LOG_POST("Checking ExtAnnot MGC");
    SAnnotSelector sel(CSeqFeatData::eSubtype_misc_difference);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("MGC");
    //s_CheckFeat(sel, "NC_000001.10");
}


BOOST_AUTO_TEST_CASE(CheckExtHPRD)
{
    LOG_POST("Checking ExtAnnot HPRD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_site);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("HPRD");
    //s_CheckFeat(sel, "NC_000001.10");
}


BOOST_AUTO_TEST_CASE(CheckExtSTS)
{
    LOG_POST("Checking ExtAnnot STS");
    SAnnotSelector sel(CSeqFeatData::eSubtype_STS);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("STS");
    s_CheckFeat(sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249220000));
}


BOOST_AUTO_TEST_CASE(CheckExtTRNA)
{
    LOG_POST("Checking ExtAnnot tRNA");
    SAnnotSelector sel(CSeqFeatData::eSubtype_tRNA);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("tRNA");
    s_CheckFeat(sel, "NT_026437.11");
}


BOOST_AUTO_TEST_CASE(CheckExtMicroRNA)
{
    LOG_POST("Checking ExtAnnot microRNA");
    SAnnotSelector sel(CSeqFeatData::eSubtype_otherRNA);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("other");
    s_CheckFeat(sel, "NT_026437.11");
}


BOOST_AUTO_TEST_CASE(CheckExtExon)
{
    LOG_POST("Checking ExtAnnot Exon");
    SAnnotSelector sel(CSeqFeatData::eSubtype_exon);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("Exon");
    s_CheckFeat(sel, "NR_003287.2");
}


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(CheckAll);
}
