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
#include <objtools/data_loaders/genbank/id2/reader_id2.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <dbapi/driver/drivers.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <algorithm>

//#define RUN_MT_TESTS
#if defined(RUN_MT_TESTS) && defined(NCBI_THREADS)
# include <objtools/simple/simple_om.hpp>
# include <future>
#endif

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seq/seq__.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/iterator.hpp>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBI_PARAM_DECL(bool, ID_UNIT_TEST, PSG2_LOADER);
NCBI_PARAM_DEF(bool, ID_UNIT_TEST, PSG2_LOADER, false);

static
bool IsUsingPSG2Loader(void)
{
    return NCBI_PARAM_TYPE(ID_UNIT_TEST, PSG2_LOADER)::GetDefault();
}

static
bool NeedsOtherLoaders()
{
    return CGBDataLoader::IsUsingPSGLoader() && !IsUsingPSG2Loader();
}


static CRef<CScope> s_InitScope(bool reset_loader = true)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    if ( reset_loader ) {
        CDataLoader* loader =
            om->FindDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
        if ( loader ) {
            if ( NeedsOtherLoaders() ) {
                om->RevokeAllDataLoaders();
            }
            else {
                BOOST_CHECK(om->RevokeDataLoader(*loader));
            }
        }
    }
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
    GenBankReaders_Register_Pubseq2();
#endif
    CGBDataLoader::RegisterInObjectManager(*om);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    if ( NeedsOtherLoaders() ) {
        // Add SNP/CDD/WGS loaders.
        scope->AddDataLoader(om->RegisterDataLoader(0, "cdd")->GetName());
        scope->AddDataLoader(om->RegisterDataLoader(0, "snp")->GetName());
        scope->AddDataLoader(om->RegisterDataLoader(0, "wgs")->GetName());
    }
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

enum EIncludePubseqos2 {
    eIncludePubseqos2,
    eExcludePubseqos2
};

bool s_HaveID2(EIncludePubseqos2 include_pubseqos2 = eIncludePubseqos2)
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        return true;
    }
    if ( NStr::EndsWith(env, "id1", NStr::eNocase) ||
         NStr::EndsWith(env, "pubseqos", NStr::eNocase) ) {
        // non-ID2 based readers
        return false;
    }
    if ( NStr::EndsWith(env, "pubseqos2", NStr::eNocase) ) {
        // only if pubseqos2 reader is allowed
        return include_pubseqos2 == eIncludePubseqos2;
    }
    if ( NStr::EndsWith(env, "id2", NStr::eNocase) ) {
        // real id2 reader
        return true;
    }
    return false;
}


bool s_HaveID1(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        return false;
    }
    return NStr::EndsWith(env, "id1", NStr::eNocase);
}


bool s_HaveCache(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD");
    if ( !env ) {
        // assume default ID2
        return false;
    }
    return NStr::StartsWith(env, "cache", NStr::eNocase);
}


bool s_HaveNA()
{
    // NA are available in PSG and ID2
    return CGBDataLoader::IsUsingPSGLoader() || s_HaveID2();
}


bool s_HaveSplit()
{
    // split data are available in PSG and ID2
    return CGBDataLoader::IsUsingPSGLoader() || s_HaveID2();
}


bool s_HaveMongoDBCDD()
{
    // MongoDB plugin is available in PSG and OSG
    return CGBDataLoader::IsUsingPSGLoader() || (s_HaveID2(eExcludePubseqos2) && CId2Reader::GetVDB_CDD_Enabled());
}


bool s_HaveVDBWGS()
{
    // WGS VDB plugin is available in PSG and OSG
    return CGBDataLoader::IsUsingPSGLoader() || s_HaveID2(eExcludePubseqos2);
}


bool s_HaveVDBSNP()
{
    // VDB SNP plugin is available in PSG and OSG
    return CGBDataLoader::IsUsingPSGLoader() || s_HaveID2(eExcludePubseqos2);
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
        "JAJALB010000060",
        "gb|JAJALB010000060.1;gi|2209563612;gnl|WGS:JAJALB01|270_pilon",
        57616,
        57616
    },
    {
        "2209563612",
        "gb|JAJALB010000060.1;gi|2209563612;gnl|WGS:JAJALB01|270_pilon",
        57616,
        57616
    },
    {
        "2209563628",
        "gb|KAI1044389.1;gi|2209563628;gnl|WGS:JAJALB|LB504_013148-RA",
        758,
        758
    },
    {
        "KAI1044389.1",
        "gb|KAI1044389.1;gi|2209563628;gnl|WGS:JAJALB|LB504_013148-RA",
        758,
        758
    },
};


void s_CheckIds(const SBioseqInfo& info, CScope* scope)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(info.m_RequestId);
    vector<CSeq_id_Handle> req_ids;
    {
        vector<string> req_ids_str;
        NStr::Split(info.m_RequiredIds, ";", req_ids_str);
        ITERATE ( vector<string>, it, req_ids_str ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*it);
            req_ids.push_back(idh);
        }
    }

    CScope::TIds ids = s_Normalize(scope->GetIds(idh));
    s_CompareIdSets("expected with get-ids",
                    req_ids, ids, true);

    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    //s_Print("Actual ", bh.GetId());
    s_CompareIdSets("get-ids with Bioseq",
                    ids, s_Normalize(bh.GetId()), true);
}


void s_CheckSequence(const SBioseqInfo& info, CScope* scope)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(info.m_RequestId);
    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
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


size_t s_CheckFeat(CRef<CScope> scope,
                   const SAnnotSelector& sel,
                   const string& str_id,
                   CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    size_t ret = 0;
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
    {
        CFeat_CI it(*scope, *loc, sel);
        ret += it.GetSize();
        BOOST_CHECK(it);
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    {
        CFeat_CI it(bh, range, sel);
        ret += it.GetSize();
        BOOST_CHECK(it);
    }
    return ret;
}


void s_CheckNoFeat(CRef<CScope> scope,
                   const SAnnotSelector& sel,
                   const string& str_id,
                   CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
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
    {
        CFeat_CI it(*scope, *loc, sel);
        BOOST_CHECK(!it);
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    {
        CFeat_CI it(bh, range, sel);
        BOOST_CHECK(!it);
    }
}


void s_CheckFeat(const SAnnotSelector& sel,
                 const string& str_id,
                 CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    s_CheckFeat(s_InitScope(), sel, str_id, range);
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
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    s_CheckFeat(sel, "2150811132", CRange<TSeqPos>::GetWhole());
}


BOOST_AUTO_TEST_CASE(CheckExtSNPGraph)
{
    LOG_POST("Checking ExtAnnot SNP graph");
    SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    s_CheckGraph(sel, "2150811132", CRange<TSeqPos>::GetWhole());
}

#if 0
BOOST_AUTO_TEST_CASE(CheckExtSNPEdit)
{
    LOG_POST("Checking ExtAnnot SNP for edited sequence");
    string id = "NM_004006.2";
    
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    CRef<CScope> scope = s_InitScope();
    scope->SetKeepExternalAnnotsForEdit();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
    BOOST_REQUIRE(bh);
    CBioseq_EditHandle bhe = bh.GetEditHandle();
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE(bhe);
    s_CheckFeat(scope, sel, id);
}


BOOST_AUTO_TEST_CASE(CheckExtSNPEditChangeId)
{
    LOG_POST("Checking ExtAnnot SNP for sequence with changed ids");
    string id = "NM_004006.2";
    string dummy_id = "lcl|dummy";
    
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    CRef<CScope> scope = s_InitScope();
    scope->SetKeepExternalAnnotsForEdit();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
    BOOST_REQUIRE(bh);
    CBioseq_EditHandle bhe = bh.GetEditHandle();
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE(bhe);
    size_t count = s_CheckFeat(scope, sel, id);
    vector<CSeq_id_Handle> ids = bhe.GetId();
    bhe.ResetId();
    bhe.AddId(CSeq_id_Handle::GetHandle(dummy_id));
    s_CheckNoFeat(scope, sel, dummy_id);
    for ( auto idh : ids ) {
        bhe.AddId(idh);
    }
    BOOST_CHECK_EQUAL(s_CheckFeat(scope, sel, dummy_id), count);
}


template<class TObject>
void s_CheckFeatData(const SAnnotSelector& sel,
                     const string& str_id,
                     CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    CRef<CScope> scope = s_InitScope(false);
    CRef<CSeq_id> seq_id(new CSeq_id("NC_000001.10"));
    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);

    vector< CConstRef<TObject> > objs, objs_copy;
    CFeat_CI fit(bh, range, sel);
    BOOST_REQUIRE(fit);
    for ( ; fit; ++fit ) {
        CConstRef<CSeq_feat> feat = fit->GetSeq_feat();
        bool old_data = true;
        for ( size_t i = 0; i < objs.size(); ++i ) {
            BOOST_REQUIRE(objs[i]->Equals(*objs_copy[i]));
        }
        for ( CTypeConstIterator<TObject> it(Begin(*feat)); it; ++it ) {
            if ( old_data ) {
                objs.clear();
                objs_copy.clear();
                old_data = false;
            }
            CConstRef<TObject> obj(&*it);
            objs.push_back(obj);
            objs_copy.push_back(Ref(SerialClone(*obj)));
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_feat)
{
    LOG_POST("Checking ExtAnnot SNP Seq-feat generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_feat>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_SeqFeatData)
{
    LOG_POST("Checking ExtAnnot SNP SeqFeatData generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeqFeatData>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Imp_feat)
{
    LOG_POST("Checking ExtAnnot SNP Imp-feat generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CImp_feat>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_loc)
{
    LOG_POST("Checking ExtAnnot SNP Seq-loc generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_loc>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_id)
{
    LOG_POST("Checking ExtAnnot SNP Seq-id generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_id>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_point)
{
    LOG_POST("Checking ExtAnnot SNP Seq-point generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_point>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_interval)
{
    LOG_POST("Checking ExtAnnot SNP Seq-interval generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_interval>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Gb_qual)
{
    LOG_POST("Checking ExtAnnot SNP Gb-qual generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CGb_qual>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Dbtag)
{
    LOG_POST("Checking ExtAnnot SNP Dbtag generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CDbtag>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Object_id)
{
    LOG_POST("Checking ExtAnnot SNP Object-id generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CObject_id>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_User_object)
{
    LOG_POST("Checking ExtAnnot SNP User-object generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CUser_object>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_User_field)
{
    LOG_POST("Checking ExtAnnot SNP User-field generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CUser_field>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}
#endif


BOOST_AUTO_TEST_CASE(CheckExtSNPNAGraph5000)
{
    LOG_POST("Checking ExtAnnot SNP NA graph @@5000");
    SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("NA000146873.19#1@@5000");
    sel.AddNamedAnnots("NA000146873.19#1@@5000");
    s_CheckGraph(sel, "NM_001397487.1", CRange<TSeqPos>::GetWhole());
}


BOOST_AUTO_TEST_CASE(CheckExtSNPNAGraph100)
{
    LOG_POST("Checking ExtAnnot SNP NA graph @@100");
    SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("NA000146873.19#1@@100");
    sel.AddNamedAnnots("NA000146873.19#1@@100");
    s_CheckGraph(sel, "2150811132", CRange<TSeqPos>::GetWhole());
}


BOOST_AUTO_TEST_CASE(CheckExtSNPNA)
{
    LOG_POST("Checking ExtAnnot SNP NA");
    SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("NA000146873.19#1");
    sel.AddNamedAnnots("NA000146873.19#1");
    s_CheckFeat(sel, "NM_001397487", CRange<TSeqPos>::GetWhole());
}


static string s_GetVDB_CDD_Source()
{
    return CId2Reader::GetVDB_CDD_Enabled()? "CGI": "ID";
}


struct SInvertVDB_CDD {
    void s_Invert()
        {
            CId2Reader::SetVDB_CDD_Enabled(!CId2Reader::GetVDB_CDD_Enabled());
        }
    SInvertVDB_CDD()
        {
            s_Invert();
        }
    ~SInvertVDB_CDD()
        {
            s_Invert();
        }
    SInvertVDB_CDD(const SInvertVDB_CDD&) = delete;
    void operator=(const SInvertVDB_CDD&) = delete;


    static bool IsPossible()
        {
            // Only OSG can switch CDD source
            return !CGBDataLoader::IsUsingPSGLoader() && s_HaveID2(eExcludePubseqos2);
        }
};

BOOST_AUTO_TEST_CASE(CheckExtCDD)
{
    if (!s_HaveID2() || CId2Reader::GetVDB_CDD_Enabled()) return;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "NP_001385356.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDD2)
{
    if (!s_HaveID2() || !CId2Reader::GetVDB_CDD_Enabled()) return;
    SInvertVDB_CDD invert;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "NP_001385356.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDDonWGS)
{
    if (!s_HaveID2() || CId2Reader::GetVDB_CDD_Enabled()) return;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD on WGS sequence");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "RLL67630.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDD2onWGS)
{
    if (!s_HaveID2() || !CId2Reader::GetVDB_CDD_Enabled()) return;
    SInvertVDB_CDD invert;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD on WGS sequence");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "RLL67630.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDDonPDB)
{
    LOG_POST("Checking ExtAnnot CDD on PDB sequence");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "pdb|4XNUA");
}

#if 0
BOOST_AUTO_TEST_CASE(CheckExtMGC)
{
    LOG_POST("Checking ExtAnnot MGC");
    SAnnotSelector sel(CSeqFeatData::eSubtype_misc_difference);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("MGC");
    s_CheckFeat(sel, "BC103747.1");
}


BOOST_AUTO_TEST_CASE(CheckExtHPRD)
{
    LOG_POST("Checking ExtAnnot HPRD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_site);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("HPRD");
    s_CheckFeat(sel, "NP_003933.1");
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


BOOST_AUTO_TEST_CASE(CheckExtTRNAEdit)
{
    LOG_POST("Checking ExtAnnot tRNA edited");
    SAnnotSelector sel(CSeqFeatData::eSubtype_tRNA);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("tRNA");
    CRef<CScope> scope = s_InitScope();
    scope->SetKeepExternalAnnotsForEdit();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("NT_026437.11"));
    BOOST_REQUIRE(bh);
    CBioseq_EditHandle bhe = bh.GetEditHandle();
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE(bhe);
    s_CheckFeat(scope, sel, "NT_026437.11");
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
#endif

BOOST_AUTO_TEST_CASE(CheckNAZoom)
{
    bool have_na = s_HaveNA();
    LOG_POST("Checking NA Tracks");
    string id = "NC_059425.1";
    string na_acc = "NA000358513.1";

    for ( int t = 0; t < 4; ++t ) {
        SAnnotSelector sel;
        sel.IncludeNamedAnnotAccession(na_acc, -1);
        if ( t&1 ) {
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, -1));
        }
        if ( t&2 ) {
            sel.AddNamedAnnots(na_acc);
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, 100));
        }
        sel.SetCollectNames();

        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetWhole().Set(id);
        set<int> tracks;
        CRef<CScope> scope = s_InitScope();
        CGraph_CI it(*scope, *loc, sel);
        if ( !have_na ) {
            BOOST_CHECK(!it);
            continue;
        }
        ITERATE ( CGraph_CI::TAnnotNames, i, it.GetAnnotNames() ) {
            if ( !i->IsNamed() ) {
                continue;
            }
            int zoom;
            string acc;
            if ( !ExtractZoomLevel(i->GetName(), &acc, &zoom) ) {
                continue;
            }
            if ( acc != na_acc ) {
                continue;
            }
            tracks.insert(zoom);
        }
        BOOST_CHECK(tracks.count(100));
    }
}


BOOST_AUTO_TEST_CASE(CheckNAZoom10)
{
    LOG_POST("Checking NA Graph Track");
    string id = "2231916682";
    string na_acc = "NA000358513.1";

    for ( int t = 0; t < 2; ++t ) {
        SAnnotSelector sel;
        sel.IncludeNamedAnnotAccession(na_acc, 10);
        if ( t&1 ) {
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, -1));
        }
        else {
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, 10));
        }
        s_CheckGraph(sel, id);
    }
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr)
{
    LOG_POST("Checking WGS master sequence descriptors 1");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("JAJALB010000060"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 8);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr2)
{
    LOG_POST("Checking WGS master sequence descriptors 2");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("2209563612"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
}


BOOST_AUTO_TEST_CASE(CheckWGSScaffold)
{
    LOG_POST("Checking WGS scaffold");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("MU279811"));
    //auto core = SerialClone(*bh.GetObjectCore());
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Genbank));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 3u);
    BOOST_CHECK_EQUAL(user_count["AutodefOptions"], 1);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(total_count, 13);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 161u);
    BOOST_CHECK_EQUAL(CFeat_CI(bh, SAnnotSelector().SetResolveAll()).GetSize(), 161u);
    TSeqPos len = bh.GetBioseqLength();
    BOOST_CHECK(bh.GetSeqVector().CanGetRange(0, len));
    string data;
    bh.GetSeqVector().GetSeqData(0, len, data);
    BOOST_CHECK_EQUAL(data.size(), len);
}

BOOST_AUTO_TEST_CASE(CheckWGSProt1)
{
    LOG_POST("Checking VDB WGS nuc-prot set by GI");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetGiHandle(2209563628));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 14u);
    BOOST_CHECK_EQUAL(CFeat_CI(bh, SAnnotSelector().SetResolveAll()).GetSize(), 14u);
    TSeqPos len = bh.GetBioseqLength();
    BOOST_CHECK(bh.GetSeqVector().CanGetRange(0, len));
    string data;
    bh.GetSeqVector().GetSeqData(0, len, data);
    BOOST_CHECK_EQUAL(data.size(), len);
}

BOOST_AUTO_TEST_CASE(CheckWGSProt2)
{
    LOG_POST("Checking VDB WGS nuc-prot set by acc");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("KAI1044389"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 14u);
    BOOST_CHECK_EQUAL(CFeat_CI(bh, SAnnotSelector().SetResolveAll()).GetSize(), 14u);
    TSeqPos len = bh.GetBioseqLength();
    BOOST_CHECK(bh.GetSeqVector().CanGetRange(0, len));
    string data;
    bh.GetSeqVector().GetSeqData(0, len, data);
    BOOST_CHECK_EQUAL(data.size(), len);
}


enum EInstType {
    eInst_ext, // inst.ext is set, inst.seq-data is not set
    eInst_data, // inst.ext is not set, inst.seq-data is set
    eInst_split_data // inst.ext is not set, inst.seq-data is split
};

static void s_CheckSplitSeqData(CScope& scope, const string& seq_id, EInstType type)
{
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type == eInst_data);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Seq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Ext(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type == eInst_data);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
    bh.GetInst();
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Seq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Ext(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
}


BOOST_AUTO_TEST_CASE(CheckSplitSeqData)
{
    LOG_POST("Checking split Seq-data access");
    CRef<CScope> scope = s_InitScope();
    s_CheckSplitSeqData(*scope, "JAJALB010000061", eInst_ext);
}


template<class ObjectType, class Root>
size_t s_CountObjects(const Root& root)
{
    size_t count = 0;
    for ( CTypeConstIterator<ObjectType> it(Begin(root)); it; ++it ) {
        ++count;
    }
    return count;
}


BOOST_AUTO_TEST_CASE(CheckSplitNucProtSet)
{
    LOG_POST("Checking split nuc-prot set");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("CP043588"));
    BOOST_REQUIRE(bh);
    auto core = bh.GetTopLevelEntry().GetObjectCore();
    BOOST_CHECK_LT(s_CountObjects<CSeqdesc>(*core), 14704u);
    BOOST_CHECK_EQUAL(s_CountObjects<CSeq_feat>(*core), 0u);
    auto complete = bh.GetTopLevelEntry().GetCompleteObject();
    BOOST_CHECK_EQUAL(s_CountObjects<CSeqdesc>(*complete), 14704u);
    BOOST_CHECK_EQUAL(s_CountObjects<CSeq_feat>(*complete), 15112u);
}


BOOST_AUTO_TEST_CASE(TestGetBlobById)
{
    bool is_psg = CGBDataLoader::IsUsingPSGLoader();
    if ( is_psg ) {
        // test PSG
    }
    else {
        if ( !s_HaveID2(eExcludePubseqos2) ) {
            LOG_POST("Skipping CDataLoader::GetBlobById() test without OSG or PSG");
            return;
        }
        if ( s_HaveCache() ) {
            LOG_POST("Skipping CDataLoader::GetBlobById() with GenBank loader cache enabled");
            return;
        }
        // test ID2
    }    
    LOG_POST("Testing CDataLoader::GetBlobById()");
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    om->RevokeAllDataLoaders();
    CRef<CReader> reader;
    if ( is_psg ) {
        CGBDataLoader::RegisterInObjectManager(*om);
    }
    else {
        CGBLoaderParams params;
        reader = new CId2Reader();
        params.SetReaderPtr(reader);
        CGBDataLoader::RegisterInObjectManager(*om, params);
    }
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    map<int, set<CConstRef<CSeq_entry>>> entries;
    map<int, CBlobIdKey> blob_ids;
    for ( int t = 0; t < 4; ++t ) {
        int gi_start = 2;
        int gi_end = 100;
        int gi_stop_check = gi_end / 2;
        bool no_connection = false;
        if ( t == 0 ) {
            LOG_POST("Collecting entries");
        }
        else if ( t == 1 ) {
            LOG_POST("Re-loading entries");
        }
        else if ( t == 2 ) {
            LOG_POST("Re-loading entries with errors, exceptions are expected");
            if ( is_psg ) {
#ifdef HAVE_PSG_LOADER
                CPSGDataLoader_Impl::SetGetBlobByIdShouldFail(true);
                no_connection = true;
#endif
            }
            else {
                reader->SetMaximumConnections(0);
                no_connection = true;
            }
        }
        else {
            LOG_POST("Re-loading entries without errors");
            if ( is_psg ) {
#ifdef HAVE_PSG_LOADER
                CPSGDataLoader_Impl::SetGetBlobByIdShouldFail(false);
#endif
            }
            else {
                reader->SetMaximumConnections(1);
            }
        }
        if ( no_connection ) {
            LOG_POST("Multiple exception messages may appear below.");
        }
        for ( int gi = gi_start; gi <= gi_end; ++gi ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetGiHandle(GI_FROM(int, gi));
            CBioseq_Handle bh;
            if ( no_connection ) {
                if ( gi < gi_stop_check ) {
                    BOOST_CHECK_THROW(scope->GetBioseqHandle(idh), CException);
                }
                continue;
            }
            else {
                bh = scope->GetBioseqHandle(idh);
            }
            BOOST_REQUIRE(bh);
            if ( t == 0 ) {
                blob_ids[gi] = bh.GetTSE_Handle().GetBlobId();
            }
            auto entry = bh.GetTSE_Handle().GetTSECore();
            if ( gi < gi_stop_check ) {
                BOOST_CHECK(entries[gi].insert(entry).second);
            }
        }
    }
    /*
    for ( auto& s : blob_ids ) {
    }
    */
}


NCBITEST_INIT_CMDLINE(arg_descrs)
{
    arg_descrs->AddFlag("psg",
        "Update all test cases to current reader code (dangerous).",
        true );
    arg_descrs->AddOptionalKey("id2-service", "ID2Service",
                               "Service name for ID2 connection.",
                               CArgDescriptions::eString);
}


NCBITEST_INIT_TREE()
{
#ifdef NCBI_INT4_GI
    LOG_POST("Skipping all tests without 64-bit GIs enabled");
    NcbiTestSetGlobalDisabled();
    return;
#endif
    auto app = CNcbiApplication::Instance();
    const CArgs& args = app->GetArgs();
    if ( args["psg"] ) {
#if !defined(HAVE_PSG_LOADER)
        LOG_POST("Skipping -psg tests without PSG loader");
        NcbiTestSetGlobalDisabled();
        return;
#endif
        app->GetConfig().Set("genbank", "loader_psg", "1");
    }
    if ( args["id2-service"] ) {
        app->GetConfig().Set("genbank/id2", "service", args["id2-service"].AsString());
    }

    NCBITEST_DISABLE(CheckAll);
    NCBITEST_DISABLE(CheckExtCDDonWGS); // TODO
    NCBITEST_DISABLE(CheckExtCDD2onWGS); // TODO
    NCBITEST_DISABLE(CheckExtCDDonPDB); // TODO

    if ( !SInvertVDB_CDD::IsPossible() ) {
        NCBITEST_DISABLE(CheckExtCDD2);
        NCBITEST_DISABLE(CheckExtCDD2onWGS);
    }
    if ( !s_HaveMongoDBCDD() ) {
        NCBITEST_DISABLE(CheckExtCDDonPDB);
    }
    if (CGBDataLoader::IsUsingPSGLoader()) {
        //NCBITEST_DISABLE(CheckExtSTS);
    }
    if ( !s_HaveNA() ) {
        NCBITEST_DISABLE(CheckNAZoom);
        NCBITEST_DISABLE(CheckNAZoom10);
    }
    if ( !s_HaveSplit() ) {
        NCBITEST_DISABLE(CheckSplitSeqData);
        NCBITEST_DISABLE(CheckSplitNucProtSet);
    }
    if ( !s_HaveVDBSNP() ) {
        NCBITEST_DISABLE(CheckExtSNP);
        NCBITEST_DISABLE(CheckExtSNPGraph);
        NCBITEST_DISABLE(CheckExtSNPNAGraph5000);
        NCBITEST_DISABLE(CheckExtSNPNAGraph100);
        NCBITEST_DISABLE(CheckExtSNPNA);
    }
    if ( !s_HaveVDBWGS() ) {
        NCBITEST_DISABLE(CheckWGSProt1);
        NCBITEST_DISABLE(CheckWGSProt2);
    }
    if ( !CGBDataLoader::IsUsingPSGLoader() &&
         (!s_HaveID2(eExcludePubseqos2) || s_HaveCache()) ) {
        NCBITEST_DISABLE(TestGetBlobById);
    }    
    /*
    NCBITEST_DISABLE(CheckExtHPRD);
    NCBITEST_DISABLE(CheckExtMGC);
    NCBITEST_DISABLE(CheckExtTRNA);
    NCBITEST_DISABLE(CheckExtTRNAEdit);
    NCBITEST_DISABLE(CheckExtMicroRNA);
    NCBITEST_DISABLE(CheckExtExon);
#if !defined(HAVE_PUBSEQ_OS) || (defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT))
    // HUP test needs multiple PubSeqOS readers
    NCBITEST_DISABLE(Test_HUP);
    // GBLoader name test needs multiple PubSeqOS readers
    NCBITEST_DISABLE(TestGBLoaderName);
#endif
    NCBITEST_DISABLE(TestGetBlobById);
    */
}
