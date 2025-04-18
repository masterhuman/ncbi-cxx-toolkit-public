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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for WGS data loader
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <corelib/ncbi_system.hpp>
#include <objtools/readers/idmapper.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define PILEUP_NAME_SUFFIX " pileup graphs"

// 0 - never, 1 - every 5th day, 2 - always
#define DEFAULT_REPORT_GENERAL_ID_ERROR 2
#define DEFAULT_REPORT_SEQ_STATE_ERROR 1

NCBI_PARAM_DECL(int, WGS, REPORT_GENERAL_ID_ERROR);
NCBI_PARAM_DEF_EX(int, WGS, REPORT_GENERAL_ID_ERROR,
                  DEFAULT_REPORT_GENERAL_ID_ERROR,
                  eParam_NoThread, WGS_REPORT_GENERAL_ID_ERROR);
NCBI_PARAM_DECL(int, WGS, REPORT_SEQ_STATE_ERROR);
NCBI_PARAM_DEF_EX(int, WGS, REPORT_SEQ_STATE_ERROR,
                  DEFAULT_REPORT_SEQ_STATE_ERROR,
                  eParam_NoThread, WGS_REPORT_SEQ_STATE_ERROR);

static bool GetReportError(int report_level, int sub_day)
{
    // optionally report error only when day of month is divisible by 5
    return ( (report_level >= 2) ||
             (report_level == 1 && CTime(CTime::eCurrent).Day() % 5 == sub_day) );
}

static bool GetReportGeneralIdError(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, REPORT_GENERAL_ID_ERROR)> s_Value;
    return GetReportError(s_Value->Get(), 2);
}

static bool GetReportSeqStateError(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, REPORT_SEQ_STATE_ERROR)> s_Value;
    return GetReportError(s_Value->Get(), 0);
}

NCBI_PARAM_DECL(bool, WGS, ADD_GB_FOR_MASTER_DESCR);
NCBI_PARAM_DEF_EX(bool, WGS, ADD_GB_FOR_MASTER_DESCR, false, eParam_NoThread, WGS_ADD_GB_FOR_MASTER_DESCR);


static bool AddGBForMasterDescr()
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, ADD_GB_FOR_MASTER_DESCR)> s_Value;
    return s_Value->Get();
}


template<class Call>
static
typename std::invoke_result<Call>::type
s_CallWithRetry(Call&& call,
                const char* name,
                int retry_count)
{
    for ( int t = 1; t < retry_count; ++ t ) {
        try {
            return call();
        }
        catch ( CBlobStateException& ) {
            // no retry
            throw;
        }
        catch ( CException& exc ) {
            LOG_POST(Warning<<name<<"() try "<<t<<" exception: "<<exc);
        }
        catch ( exception& exc ) {
            LOG_POST(Warning<<name<<"() try "<<t<<" exception: "<<exc.what());
        }
        catch ( ... ) {
            LOG_POST(Warning<<name<<"() try "<<t<<" exception");
        }
        if ( t >= 2 ) {
            //double wait_sec = m_WaitTime.GetTime(t-2);
            double wait_sec = 1;
            LOG_POST(Warning<<name<<"(): waiting "<<wait_sec<<"s before retry");
            SleepMilliSec(Uint4(wait_sec*1000));
        }
    }
    return call();
}


enum EMasterDescrType
{
    eWithoutMasterDescr,
    eWithMasterDescr
};

static EMasterDescrType s_master_descr_type = eWithoutMasterDescr;

void sx_InitGBLoader(CObjectManager& om)
{
    CGBDataLoader* gbloader = dynamic_cast<CGBDataLoader*>
        (CGBDataLoader::RegisterInObjectManager(om, "id1", om.eNonDefault).GetLoader());
    _ASSERT(gbloader);
    gbloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
}

CRef<CObjectManager> sx_GetEmptyOM(void)
{
    SetDiagPostLevel(eDiag_Info);
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames names;
    om->GetRegisteredNames(names);
    ITERATE ( CObjectManager::TRegisteredNames, it, names ) {
        om->RevokeDataLoader(*it);
    }
    return om;
}

CRef<CObjectManager> sx_InitOM(EMasterDescrType master_descr_type)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    s_master_descr_type = master_descr_type;
    CWGSDataLoader* wgsloader = dynamic_cast<CWGSDataLoader*>
        (CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault).GetLoader());
    wgsloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
    if ( master_descr_type == eWithMasterDescr && AddGBForMasterDescr() ) {
        sx_InitGBLoader(*om);
    }
    return om;
}

CRef<CObjectManager> sx_InitOM(EMasterDescrType master_descr_type,
                               const string& wgs_file)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    s_master_descr_type = master_descr_type;
    CWGSDataLoader* wgsloader = dynamic_cast<CWGSDataLoader*>
        (CWGSDataLoader::RegisterInObjectManager(*om,
                                                 "",
                                                 vector<string>(1, wgs_file),
                                                 CObjectManager::eDefault).GetLoader());
    wgsloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
    return om;
}

bool sx_CanOpen(const string& acc)
{
    CVDBMgr mgr;
    try {
        CVDB(mgr, acc);
        return true;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ) {
            return false;
        }
        throw;
    }
}

bool sx_HasNewWGSRepositoryOnce()
{
    static bool new_rep = sx_CanOpen("AIDX01.2");
    return new_rep;
}

bool sx_HasNewWGSRepository()
{
    return s_CallWithRetry(&sx_HasNewWGSRepositoryOnce,
                           "sx_HasNewWGSRepository", 3);
}


CBioseq_Handle sx_LoadFromGB(const CBioseq_Handle& bh)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    sx_InitGBLoader(*om);
    CScope scope(*om);
    scope.AddDataLoader("GBLOADER");
    return scope.GetBioseqHandle(*bh.GetSeqId());
}

string sx_GetASN(const CBioseq_Handle& bh)
{
    CNcbiOstrstream str;
    if ( bh ) {
        str << MSerial_AsnText << *bh.GetCompleteBioseq();
    }
    return CNcbiOstrstreamToString(str);
}

string sx_GetGeneralIdStr(const vector<CSeq_id_Handle>& ids)
{
    for ( auto& id : ids ) {
        if ( id.Which() == CSeq_id::e_General ) {
            string s = id.AsString();
            string r = s.substr(4);
            s = s.substr(0, 4) + NStr::ToUpper(r);
            return s;
        }
    }
    return "";
}

CRef<CSeq_id> sx_ExtractGeneralId(CBioseq& seq)
{
    NON_CONST_ITERATE ( CBioseq::TId, it, seq.SetId() ) {
        CRef<CSeq_id> id = *it;
        if ( id->Which() == CSeq_id::e_General ) {
            seq.SetId().erase(it);
            return id;
        }
    }
    return null;
}

CRef<CDelta_ext> sx_ExtractDelta(CBioseq& seq)
{
    CRef<CDelta_ext> delta;
    CSeq_inst& inst = seq.SetInst();
    if ( inst.IsSetExt() ) {
        CSeq_ext& ext = inst.SetExt();
        if ( ext.IsDelta() ) {
            delta = &ext.SetDelta();
            ext.Reset();
        }
    }
    else if ( inst.IsSetSeq_data() ) {
        CRef<CDelta_seq> ext(new CDelta_seq);
        ext->SetLiteral().SetLength(inst.GetLength());
        ext->SetLiteral().SetSeq_data(inst.SetSeq_data());
        delta = new CDelta_ext();
        delta->Set().push_back(ext);
        inst.ResetSeq_data();
        if ( inst.GetRepr() == CSeq_inst::eRepr_raw ) {
            inst.SetRepr(CSeq_inst::eRepr_delta);
            inst.SetExt();
        }
    }
    if ( inst.IsSetStrand() && inst.GetStrand() == CSeq_inst::eStrand_ds ) {
        inst.ResetStrand();
    }
    return delta;
}

bool sx_EqualGeneralId(const CSeq_id& gen1, const CSeq_id& gen2)
{
    if ( gen1.Equals(gen2) ) {
        return true;
    }
    // allow partial match in Dbtag.db like "WGS:ABBA" vs "WGS:ABBA01"
    if ( !gen1.IsGeneral() || !gen2.IsGeneral() ) {
        return false;
    }
    const CDbtag& id1 = gen1.GetGeneral();
    const CDbtag& id2 = gen2.GetGeneral();
    if ( !id1.GetTag().Equals(id2.GetTag()) ) {
        CObject_id::TId8 value1, value2;
        if ( !id1.GetTag().GetId8(value1) ||
             !id2.GetTag().GetId8(value2) ||
             value1 != value2 ) {
            return false;
        }
    }
    const string& db1 = id1.GetDb();
    const string& db2 = id2.GetDb();
    if ( db1 == db2 ) {
        return true;
    }
    return false;
    size_t len = min(db1.size(), db2.size());
    if ( db1.substr(0, len) != db2.substr(0, len) ) {
        return false;
    }
    if ( db1.size() <= len+2 && db2.size() <= len+2 ) {
        return true;
    }
    return false;
}

bool sx_EqualDelta(const CDelta_ext& delta1, const CDelta_ext& delta2,
                   bool report_error = false)
{
    if ( delta1.Equals(delta2) ) {
        return true;
    }
    // slow check for possible different representation of delta sequence
    CScope scope(*CObjectManager::GetInstance());
    CRef<CBioseq> seq1(new CBioseq);
    CRef<CBioseq> seq2(new CBioseq);
    seq1->SetId().push_back(Ref(new CSeq_id("lcl|1")));
    seq2->SetId().push_back(Ref(new CSeq_id("lcl|2")));
    seq1->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    seq2->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    seq1->SetInst().SetMol(CSeq_inst::eMol_na);
    seq2->SetInst().SetMol(CSeq_inst::eMol_na);
    seq1->SetInst().SetExt().SetDelta(const_cast<CDelta_ext&>(delta1));
    seq2->SetInst().SetExt().SetDelta(const_cast<CDelta_ext&>(delta2));
    CBioseq_Handle bh1 = scope.AddBioseq(*seq1);
    CBioseq_Handle bh2 = scope.AddBioseq(*seq2);
    CSeqVector sv1 = bh1.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
    CSeqVector sv2 = bh2.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
    if ( sv1.size() != sv2.size() ) {
        if ( report_error ) {
            NcbiCout << "ERROR: Lengths are different: "
                     << "WGS: " << sv1.size() << " != "
                     << "GB: " << sv2.size()
                     << NcbiEndl;
        }
        return false;
    }
    for ( CSeqVector_CI it1 = sv1.begin(), it2 = sv2.begin();
          it1 && it2; ++it1, ++it2 ) {
        if ( it1.IsInGap() != it2.IsInGap() ) {
            if ( report_error ) {
                NcbiCout << "ERROR: Gaps are different @ "<<it1.GetPos()<<": "
                         << "WGS: " << it1.IsInGap() << " != "
                         << "GB: " << it2.IsInGap()
                         << NcbiEndl;
            }
            return false;
        }
        if ( *it1 != *it2 ) {
            if ( report_error ) {
                NcbiCout << "ERROR: Bases are different @ "<<it1.GetPos()<<": "
                         << "WGS: " << int(*it1) << " != "
                         << "GB: " << int(*it2)
                         << NcbiEndl;
            }
            return false;
        }
    }
    return true;
}

string sx_BinaryASN(const CSerialObject& obj)
{
    CNcbiOstrstream str;
    str << MSerial_AsnBinary << obj;
    return CNcbiOstrstreamToString(str);
}

struct PDescLess {
    bool operator()(const CRef<CSeqdesc>& r1,
                    const CRef<CSeqdesc>& r2) const
        {
            const CSeqdesc& d1 = *r1;
            const CSeqdesc& d2 = *r2;
            if ( d1.Which() != d2.Which() ) {
                return d1.Which() < d2.Which();
            }
            return sx_BinaryASN(d1) < sx_BinaryASN(d2);
        }
};

CRef<CSeq_descr> sx_ExtractDescr(const CBioseq_Handle& bh)
{
    CRef<CSeq_descr> descr(new CSeq_descr);
    const int kSingleMask =
        (1 << CSeqdesc::e_Source) |
        (1 << CSeqdesc::e_Molinfo) |
        (1 << CSeqdesc::e_Create_date) |
        (1 << CSeqdesc::e_Update_date);
    int found = 0;
    CSeq_descr::Tdata& dst = descr->Set();
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        int bit = 1 << it->Which();
        if ( bit & kSingleMask ) {
            if ( bit & found ) {
                continue;
            }
            found |= bit;
        }
        dst.push_back(Ref(SerialClone(*it)));
    }
    dst.sort(PDescLess());
    return descr;
}

class CEntryAnnots
{
public:
    explicit CEntryAnnots(CRef<CSeq_entry> entry)
        : m_Entry(entry)
        {
        }
    CSeq_annot& Select(const CSeq_annot_Handle& src_annot)
        {
            if ( m_CurSrcAnnot != src_annot ) {
                CRef<CSeq_annot> tmp_annot(new CSeq_annot);
                tmp_annot->Assign(*src_annot.GetObjectCore(), eShallow);
                tmp_annot->SetData(*new CSeq_annot::TData);
                tmp_annot->SetData().SetLocs();
                string key = sx_BinaryASN(*tmp_annot);
                if ( !m_Annots.count(key) ) {
                    m_Annots[key] = tmp_annot;
                    m_Entry->SetSet().SetAnnot().push_back(tmp_annot);
                }
                m_CurSrcAnnot = src_annot;
                m_CurDstAnnot = m_Annots[key];
            }
            return *m_CurDstAnnot;
        }

private:
    CRef<CSeq_entry> m_Entry;
    CSeq_annot_Handle m_CurSrcAnnot;
    CRef<CSeq_annot> m_CurDstAnnot;
    map<string, CRef<CSeq_annot> > m_Annots;
};

CRef<CSeq_entry> sx_ExtractAnnot(const CBioseq_Handle& bh)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();

    CFeat_CI feat_it(bh);
    if ( feat_it ) {
        CEntryAnnots annots(entry);
        for ( ; feat_it; ++feat_it ) {
            CSeq_annot& dst_annot = annots.Select(feat_it.GetAnnot());
            dst_annot.SetData().SetFtable()
                .push_back(Ref(SerialClone(feat_it->GetOriginalFeature())));
        }
    }
    return entry;
}

struct PStateFlags
{
    CBioseq_Handle::TBioseqStateFlags state;
    
    explicit PStateFlags(CBioseq_Handle::TBioseqStateFlags state) : state(state) {}
};
ostream& operator<<(ostream& out, PStateFlags p_state)
{
    CBioseq_Handle::TBioseqStateFlags state = p_state.state;
    if ( state & CBioseq_Handle::fState_dead ) {
        out << " dead";
    }
    if ( state & CBioseq_Handle::fState_suppress ) {
        out << " supp";
        if ( state & CBioseq_Handle::fState_suppress_temp ) {
            out << " temp";
        }
        if ( state & CBioseq_Handle::fState_suppress_perm ) {
            out << " perm";
        }
    }
    if ( state & CBioseq_Handle::fState_confidential ) {
        out << " confidential";
    }
    if ( state & CBioseq_Handle::fState_withdrawn ) {
        out << " withdrawn";
    }
    return out;
}

bool sx_StatesMatch(CBioseq_Handle::TBioseqStateFlags wgs_state,
                    CBioseq_Handle::TBioseqStateFlags gb_state)
{
    if ( wgs_state == gb_state ) {
        return true;
    }
    const CBioseq_Handle::TBioseqStateFlags kOverrideMask =
        CBioseq_Handle::fState_suppress |
        CBioseq_Handle::fState_dead |
        CBioseq_Handle::fState_withdrawn;
    if ( (wgs_state & !kOverrideMask) !=
         (gb_state & !kOverrideMask) ) {
        return false;
    }
    if ( (wgs_state & CBioseq_Handle::fState_withdrawn) !=
         (gb_state & CBioseq_Handle::fState_withdrawn) ) {
        return false;
    }
    if ( wgs_state & CBioseq_Handle::fState_withdrawn ) {
        return true;
    }
    if ( (wgs_state & CBioseq_Handle::fState_dead) !=
         (gb_state & CBioseq_Handle::fState_dead) ) {
        return false;
    }
    if ( wgs_state & CBioseq_Handle::fState_dead ) {
        return true;
    }
    if ( !(wgs_state & CBioseq_Handle::fState_suppress) !=
         !(gb_state & CBioseq_Handle::fState_suppress) ) {
        return false;
    }
    return true;
}

bool sx_Equal(const CBioseq_Handle& bh1, const CBioseq_Handle& bh2)
{
    CRef<CSeq_descr> descr1 = sx_ExtractDescr(bh1);
    CRef<CSeq_descr> descr2 = sx_ExtractDescr(bh2);
    CRef<CSeq_entry> annot1 = sx_ExtractAnnot(bh1);
    CRef<CSeq_entry> annot2 = sx_ExtractAnnot(bh2);
    CConstRef<CBioseq> seq1 = bh1.GetCompleteBioseq();
    CConstRef<CBioseq> seq2 = bh2.GetCompleteBioseq();
    CRef<CBioseq> nseq1(SerialClone(*seq1));
    CRef<CBioseq> nseq2(SerialClone(*seq2));
    CRef<CSeq_id> gen1 = sx_ExtractGeneralId(*nseq1);
    CRef<CSeq_id> gen2 = sx_ExtractGeneralId(*nseq2);
    CRef<CDelta_ext> delta1 = sx_ExtractDelta(*nseq1);
    CRef<CDelta_ext> delta2 = sx_ExtractDelta(*nseq2);
    nseq1->ResetDescr();
    nseq2->ResetDescr();
    nseq1->ResetAnnot();
    nseq2->ResetAnnot();
    if ( !nseq1->Equals(*nseq2) ) {
        NcbiCout << "Seq-id: " << bh1.GetAccessSeq_id_Handle() << NcbiEndl;
        NcbiCout << "ERROR: Sequences do not match:\n"
                 << "WGS: " << MSerial_AsnText << *seq1
                 << "GB: " << MSerial_AsnText << *seq2;
        return false;
    }
    bool has_delta_error = false;
    bool has_state_error = false;
    bool report_state_error = false;
    bool has_id_error = false;
    bool report_id_error = false;
    bool has_descr_error = false;
    bool has_annot_error = false;
    if ( !delta1 != !delta2 ) {
        has_delta_error = true;
    }
    else if ( delta1 && !sx_EqualDelta(*delta1, *delta2) ) {
        has_delta_error = true;
    }
    if ( !gen1 && gen2 ) {
        // GB has general id but VDB hasn't
        if ( gen2->GetGeneral().GetTag().IsId() ) {
            // it might be artificial general id
            has_id_error = report_id_error = true;
        }
        else {
            has_id_error = report_id_error = true;
        }
    }
    else if ( gen1 && !gen2 ) {
        // VDB has general id but GB hasn't
        // it's possible and shouldn't be considered as an error
        has_id_error = true;
        report_id_error = GetReportGeneralIdError();
    }
    else if ( gen1 && !sx_EqualGeneralId(*gen1, *gen2) ) {
        has_id_error = true;
        report_id_error = GetReportGeneralIdError();
    }
    if ( 0 ) {
        if ( !descr1->Equals(*descr2) ) {
            has_descr_error = true;
        }
    }
    if ( 0 ) {
        if ( !annot1->Equals(*annot2) ) {
            has_annot_error = true;
        }
    }
    if ( !sx_StatesMatch(bh1.GetState(), bh2.GetState()) ) {
        has_state_error = true;
        report_state_error = GetReportSeqStateError();
    }
    if ( has_state_error || has_id_error ||
         has_descr_error || has_descr_error || has_annot_error ) {
        NcbiCout << "Seq-id: " << bh1.GetAccessSeq_id_Handle() << NcbiEndl;
    }
    if ( has_state_error ) {
        NcbiCout << (report_state_error? "ERROR": "WARNING")
                 << ": States do not match:"
                 << " WGS: " << bh1.GetState() << PStateFlags(bh1.GetState())
                 << " GB: " << bh2.GetState() << PStateFlags(bh2.GetState())
                 << NcbiEndl;
    }
    if ( has_id_error ) {
        NcbiCout << (report_id_error? "ERROR": "WARNING")
                 << ": General ids do not match:\n";
        NcbiCout << "Id1: ";
        if ( !gen1 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *gen1;
        }
        NcbiCout << "Id2: ";
        if ( !gen2 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *gen2;
        }
    }
    if ( has_descr_error ) {
        NcbiCout << "ERROR: Descriptors do not match:\n";
        NcbiCout << "WGS: " << MSerial_AsnText << *descr1;
        NcbiCout << "GB: " << MSerial_AsnText << *descr2;
    }
    if ( has_delta_error ) {
        NcbiCout << "ERROR: Delta sequences do not match:\n";
        NcbiCout << "WGS: ";
        if ( delta1 && delta2 ) {
            // report errors
            sx_EqualDelta(*delta1, *delta2, true);
        }
        if ( !delta1 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *delta1;
        }
        NcbiCout << "GB: ";
        if ( !delta2 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *delta2;
        }
    }
    if ( has_annot_error ) {
        NcbiCout << "ERROR: Annotations do not match:\n";
        NcbiCout << "WGS: " << MSerial_AsnText << *annot1;
        NcbiCout << "GB: " << MSerial_AsnText << *annot2;
    }
    return !report_id_error && !report_state_error &&
        !has_delta_error && !has_descr_error && !has_annot_error;
}

bool sx_EqualToGB(const CBioseq_Handle& bh)
{
    BOOST_REQUIRE(bh);
    CBioseq_Handle gb_bh = sx_LoadFromGB(bh);
    BOOST_REQUIRE(gb_bh);
    return sx_Equal(bh, gb_bh);
}

void sx_CheckNames(CScope& scope, const CSeq_loc& loc,
                   const string& name)
{
    SAnnotSelector sel;
    sel.SetSearchUnresolved();
    sel.SetCollectNames();
    CAnnotTypes_CI it(CSeq_annot::C_Data::e_not_set, scope, loc, &sel);
    CAnnotTypes_CI::TAnnotNames names = it.GetAnnotNames();
    ITERATE ( CAnnotTypes_CI::TAnnotNames, i, names ) {
        if ( i->IsNamed() ) {
            NcbiCout << "Named annot: " << i->GetName()
                     << NcbiEndl;
        }
        else {
            NcbiCout << "Unnamed annot"
                     << NcbiEndl;
        }
    }
    //NcbiCout << "Checking for name: " << name << NcbiEndl;
    BOOST_CHECK_EQUAL(names.count(name), 1u);
    if ( names.size() == 2 ) {
        BOOST_CHECK_EQUAL(names.count(name+PILEUP_NAME_SUFFIX), 1u);
    }
    else {
        BOOST_CHECK_EQUAL(names.size(), 1u);
    }
}

void sx_CheckSeq(CScope& scope,
                 const CSeq_id_Handle& main_idh,
                 const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( idh == main_idh ) {
        return;
    }
    const CBioseq_Handle::TId& ids = scope.GetIds(idh);
    BOOST_REQUIRE_EQUAL(ids.size(), 1u);
    BOOST_CHECK_EQUAL(ids[0], idh);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
}

void sx_ReportState(const CBioseq_Handle& bsh, const CSeq_id_Handle& idh)
{
    if ( !bsh ) {
        cerr << "no sequence: " << idh << endl;
        CBioseq_Handle::TBioseqStateFlags flags = bsh.GetState();
        if (flags & CBioseq_Handle::fState_suppress_temp) {
            cerr << "  suppressed temporarily" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress_perm) {
            cerr << "  suppressed permanently" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress) {
            cerr << "  suppressed" << endl;
        }
        if (flags & CBioseq_Handle::fState_dead) {
            cerr << "  dead" << endl;
        }
        if (flags & CBioseq_Handle::fState_confidential) {
            cerr << "  confidential" << endl;
        }
        if (flags & CBioseq_Handle::fState_withdrawn) {
            cerr << "  withdrawn" << endl;
        }
        if (flags & CBioseq_Handle::fState_no_data) {
            cerr << "  no data" << endl;
        }
        if (flags & CBioseq_Handle::fState_conflict) {
            cerr << "  conflict" << endl;
        }
        if (flags & CBioseq_Handle::fState_not_found) {
            cerr << "  not found" << endl;
        }
        if (flags & CBioseq_Handle::fState_other_error) {
            cerr << "  other error" << endl;
        }
    }
    else {
        cerr << "found sequence: " << idh << endl;
        //cerr << MSerial_AsnText << *bsh.GetCompleteBioseq();
    }
}

int sx_GetDescCount(const CBioseq_Handle& bh, CSeqdesc::E_Choice type)
{
    int ret = 0;
    for ( CSeqdesc_CI it(bh, type); it; ++it ) {
        ++ret;
    }
    return ret;
}

void sx_CheckTestDirectory(const string& dir)
{
    LOG_POST(boost::unit_test::framework::current_test_case().p_name.get()<<
             ": directory "<<dir<<" "<<(CDirEntry(dir).Exists()? "exists": "is inaccessible"));
}

BOOST_AUTO_TEST_CASE(FetchSeq1)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA01000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bh));
}

BOOST_AUTO_TEST_CASE(FetchSeq2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA02000102.1";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bh));
}

BOOST_AUTO_TEST_CASE(FetchSeq3)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|AAAA01000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

BOOST_AUTO_TEST_CASE(FetchSeq4)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|AAAA010000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq5)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|AAAA0100000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq6)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|ZZZZ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq7)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "AAAA01010001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK(sx_EqualToGB(bh));
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}


BOOST_AUTO_TEST_CASE(FetchSeq8)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ01S31652451";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 1);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

BOOST_AUTO_TEST_CASE(FetchSeq8_2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ02S4870253";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_not_set), 10);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 3);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

BOOST_AUTO_TEST_CASE(FetchSeq9)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ0100000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    //BOOST_CHECK_EQUAL(bh.GetState(), bh.fState_dead);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 1);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
    //BOOST_CHECK(sx_EqualToGB(bh)); no in GB
}

BOOST_AUTO_TEST_CASE(FetchSeq9_2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ020000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    //BOOST_CHECK_EQUAL(bh.GetState(), bh.fState_dead);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_not_set), 10);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 3);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
    //BOOST_CHECK(sx_EqualToGB(bh)); no in GB
}

BOOST_AUTO_TEST_CASE(FetchSeq10)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ010000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq11)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ACUJ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bh));
}


BOOST_AUTO_TEST_CASE(FetchSeq12)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ACUJ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bh));
}


BOOST_AUTO_TEST_CASE(FetchSeq13)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ACUJ01000001.1";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}


BOOST_AUTO_TEST_CASE(FetchSeq14)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ACUJ01000001.3";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq15)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    sx_InitGBLoader(*om);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    scope->AddDataLoader("GBLOADER");

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("gb|ABBA01000001.1|");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq16)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("JRAS01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq17)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("AVCP010000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Source), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Create_date), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Update_date), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_User), 2);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_dna);
}


BOOST_AUTO_TEST_CASE(FetchSeq18)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("CCMW01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchSeq19)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("CCMW01000002");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq20)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("GAAA01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_rna);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq21)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("JELW01000003");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq22)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("JXBK01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq23)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "JANG01000027";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK(sx_GetDescCount(bh, CSeqdesc::e_Molinfo) >= 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
    BOOST_CHECK(sx_EqualToGB(bh));
}


BOOST_AUTO_TEST_CASE(FetchSeq24)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("hAaa01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_rna);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq25)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("HAhq01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(bsh.GetSequenceType(), CSeq_inst::eMol_rna);
}


BOOST_AUTO_TEST_CASE(FetchSeqGnl1)
{
    // have standard gnl id
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "JAAA01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(scope.GetIds(idh)), "gnl|WGS:JAAA01|CONTIG1");
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK(sx_EqualToGB(bh));
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bh.GetId()), "gnl|WGS:JAAA01|CONTIG1");
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

BOOST_AUTO_TEST_CASE(FetchSeqGnl2)
{
    // have GNL column, but the id is standard
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "JAAO01000002";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(scope.GetIds(idh)), "gnl|WGS:JAAO01|CONTIGID02");
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK(sx_EqualToGB(bh));
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bh.GetId()), "gnl|WGS:JAAO01|CONTIGID02");
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

BOOST_AUTO_TEST_CASE(FetchSeqGnl3)
{
    // have non-standard gnl id
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAK03000003";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(scope.GetIds(idh)), "gnl|WGS:AAAK|CTG655");
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 4);
    BOOST_CHECK(sx_EqualToGB(bh));
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bh.GetId()), "gnl|WGS:AAAK|CTG655");
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

BOOST_AUTO_TEST_CASE(FetchSeqGnl4)
{
    // have no gnl id
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "CAAF010000004";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(scope.GetIds(idh)), "");
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 1);
    BOOST_CHECK(sx_EqualToGB(bh));
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bh.GetId()), "");
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

BOOST_AUTO_TEST_CASE(FetchSeqGnl5)
{
    // ID-6054 : Has gnl id with tag identical to row number
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "VOLU01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(scope.GetIds(idh)), "gnl|WGS:VOLU01|1");
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK(sx_EqualToGB(bh));
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bh.GetId()), "gnl|WGS:VOLU01|1");
    BOOST_CHECK_EQUAL(bh.GetSequenceType(), CSeq_inst::eMol_dna);
}

const string s_NewWGSPath = NCBI_TRACES04_PATH "/wgs03";

#if 0
const string s_ProteinFile = 
                    "x/home/dondosha/trunk/internal/c++/src/internal/ID/WGS/JTED01";
const string s_ProteinVDBAcc     = "JTED01";
const string s_ProteinContigId   = "JTED01000001";
const string s_ProteinScaffoldId = "JTED01S000001";
const string s_ProteinProteinId  = "JTED01P000001";
const int s_ProteinContigDescCount = 11;
const int s_ProteinContigPubCount = 2;
const string s_ProteinProteinAcc = "EDT30481.1";
const int s_ProteinProteinDescCount = 13;
const int s_ProteinProteinPubCount = 2;

BOOST_AUTO_TEST_CASE(FetchProt1)
{
    // WGS VDB with proteins: access contig
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    NcbiCout << "Testing protein file: "<<s_ProteinFile<<NcbiEndl;
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(s_ProteinContigId);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set),
                      s_ProteinContigDescCount);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub),
                      s_ProteinContigPubCount);
}


BOOST_AUTO_TEST_CASE(FetchProt2)
{
    // WGS VDB with proteins: check non-existend scaffold
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(s_ProteinScaffoldId);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchProt3)
{
    // WGS VDB with proteins: access protein by name
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string name_id = "gb||"+s_ProteinProteinId;
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(name_id);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set),
                      s_ProteinProteinDescCount);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub),
                      s_ProteinProteinPubCount);
    BOOST_CHECK_EQUAL(CFeat_CI(bsh.GetParentEntry()).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(FetchProt4)
{
    // WGS VDB with proteins: access protein by GB accession
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(s_ProteinProteinAcc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set),
                      s_ProteinProteinDescCount);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub),
                      s_ProteinProteinPubCount);
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bsh.GetId()), "gnl|WGS:ABKN|OCAR_2138");
    BOOST_CHECK_EQUAL(CFeat_CI(bsh.GetParentEntry()).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(FetchProt5)
{
    // WGS VDB with proteins: access protein by GB accession with wrong version
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(s_ProteinProteinAcc+"1");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchProt6)
{
    // WGS VDB with proteins: access protein by wrong GB accession with
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = s_ProteinProteinAcc;
    acc[3] += 1;
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchProt11)
{
    // WGS VDB with proteins: access contig
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("KMU40310");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set), 9);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub), 1);
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bsh.GetId()), "gnl|WGS:APHE|SEEN3RVX_05134");
    //BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchProt12)
{
    // WGS VDB with proteins: check non-existent scaffold
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(s_ProteinScaffoldId);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchProt13)
{
    // WGS VDB with proteins: access protein by name
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string name_id = "gb||"+s_ProteinProteinId;
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(name_id);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set),
                      s_ProteinProteinDescCount);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub),
                      s_ProteinProteinPubCount);
    BOOST_CHECK_EQUAL(CFeat_CI(bsh.GetParentEntry()).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(FetchProt14)
{
    // WGS VDB with proteins: access protein by GB accession
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(s_ProteinProteinAcc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set),
                      s_ProteinProteinDescCount);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub),
                      s_ProteinProteinPubCount);
    BOOST_CHECK_EQUAL(CFeat_CI(bsh.GetParentEntry()).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(FetchProt15)
{
    // WGS VDB with proteins: access protein by GB accession with wrong version
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(s_ProteinProteinAcc+"1");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchProt16)
{
    // WGS VDB with proteins: access protein by wrong GB accession with
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = s_ProteinProteinAcc;
    acc[3] += 1;
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}
#endif

BOOST_AUTO_TEST_CASE(FetchProt17)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om =
        sx_InitOM(eWithMasterDescr, s_NewWGSPath+"/WGS/AI/DX/AIDX01.1");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "AIDX01000002.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    {
        CFeat_CI feat_it(bsh);
        BOOST_CHECK_EQUAL(feat_it.GetSize(), 1859u);
    }

    CSeq_id_Handle prot_id = CSeq_id_Handle::GetHandle("EIQ82083");
    CBioseq_Handle prot_bh = bsh.GetTSE_Handle().GetBioseqHandle(prot_id);
    BOOST_REQUIRE(prot_bh);
    {
        CFeat_CI feat_it(prot_bh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(scope->GetBioseqHandle(feat_it->GetLocation()), bsh);
    }
}


BOOST_AUTO_TEST_CASE(FetchProt17a)
{
    // WGS VDB with proteins with new WGS repository
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, "AIDX01.1");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "AIDX01000002.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    {
        CFeat_CI feat_it(bsh);
        BOOST_CHECK_EQUAL(feat_it.GetSize(), 1859u);
    }

    CSeq_id_Handle prot_id = CSeq_id_Handle::GetHandle("EIQ82083");
    CBioseq_Handle prot_bh = bsh.GetTSE_Handle().GetBioseqHandle(prot_id);
    BOOST_REQUIRE(prot_bh);
    {
        CFeat_CI feat_it(prot_bh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(scope->GetBioseqHandle(feat_it->GetLocation()), bsh);
    }
}


BOOST_AUTO_TEST_CASE(FetchProt18)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om =
        sx_InitOM(eWithMasterDescr, s_NewWGSPath+"/WGS/AI/DX/AIDX01.2");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "AIDX01000002.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    {
        CFeat_CI feat_it(bsh);
        BOOST_CHECK_EQUAL(feat_it.GetSize(), 1859u);
    }

    CSeq_id_Handle prot_id = CSeq_id_Handle::GetHandle("EIQ82083");
    CBioseq_Handle prot_bh = bsh.GetTSE_Handle().GetBioseqHandle(prot_id);
    BOOST_REQUIRE(prot_bh);
    {
        CFeat_CI feat_it(prot_bh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(scope->GetBioseqHandle(feat_it->GetLocation()), bsh);
    }
}


BOOST_AUTO_TEST_CASE(FetchProt18a)
{
    // WGS VDB with proteins with new WGS repository
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, "AIDX01.2");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "AIDX01000002.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    {
        CFeat_CI feat_it(bsh);
        BOOST_CHECK_EQUAL(feat_it.GetSize(), 1859u);
    }

    CSeq_id_Handle prot_id = CSeq_id_Handle::GetHandle("EIQ82083");
    CBioseq_Handle prot_bh = bsh.GetTSE_Handle().GetBioseqHandle(prot_id);
    BOOST_REQUIRE(prot_bh);
    {
        CFeat_CI feat_it(prot_bh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(scope->GetBioseqHandle(feat_it->GetLocation()), bsh);
    }
}


BOOST_AUTO_TEST_CASE(FetchProt19)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om =
        sx_InitOM(eWithMasterDescr, s_NewWGSPath+"/WGS/AI/DX/AIDX01.1");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);

    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|AIDX01000001|");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt19a)
{
    // WGS VDB with proteins with new WGS repository
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, "AIDX01.1");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);

    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|AIDX01000001|");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt19b)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om =
        sx_InitOM(eWithMasterDescr, s_NewWGSPath+"/WGS/AI/DX/AIDX01.1");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);

    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|AIDX01000002|");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt19c)
{
    // WGS VDB with proteins with new WGS repository
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, "AIDX01.1");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);

    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|AIDX01000002|");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt20)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om =
        sx_InitOM(eWithMasterDescr, s_NewWGSPath+"/WGS/AI/DX/AIDX01.2");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|AIDX01000002|");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt20a)
{
    // WGS VDB with proteins with new WGS repository
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, "AIDX01.2");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|AIDX01000002|");
    }
    // Also check presence of gnl Seq-id
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bsh.GetId()), "gnl|WGS:AIDX|SCAZ3_06900");
}


BOOST_AUTO_TEST_CASE(FetchProt21)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om =
        sx_InitOM(eWithMasterDescr, s_NewWGSPath+"/WGS/AI/DX/AIDX01.3");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gi|391419271");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt21a)
{
    // WGS VDB with proteins with new WGS repository
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, "AIDX01.3");

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "EIQ82083";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gi|391419271");
    }
    // Also check presence of gnl Seq-id
    BOOST_CHECK_EQUAL(sx_GetGeneralIdStr(bsh.GetId()), "gnl|WGS:AIDX|SCAZ3_06900");
}


BOOST_AUTO_TEST_CASE(FetchProt22)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "MBA2057862.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 0u);
    }
}


BOOST_AUTO_TEST_CASE(FetchProt22a)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "MBA2057862";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|JACDXZ010000005.1|");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt22b)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "MBA2057862.2";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gb|JACDXZ010000005.1|");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt22c)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "MBA2057862.3";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchProt23)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "GAA10774.1";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchProt23a)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "GAA10774";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gi|343765796");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt23b)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "GAA10774.2";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);


    {
        CFeat_CI feat_it(bsh);
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Prot);
    }
    {
        CFeat_CI feat_it(bsh, SAnnotSelector().SetByProduct());
        BOOST_REQUIRE_EQUAL(feat_it.GetSize(), 1u);
        BOOST_CHECK_EQUAL(feat_it->GetFeatType(), CSeqFeatData::e_Cdregion);
        const CSeq_id* contig_id = feat_it->GetLocation().GetId();
        BOOST_REQUIRE(contig_id);
        BOOST_CHECK_EQUAL(contig_id->AsFastaString(), "gi|343765796");
    }
}


BOOST_AUTO_TEST_CASE(FetchProt23c)
{
    // WGS VDB with proteins with new WGS repository
    if ( !CDirEntry(s_NewWGSPath).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    string acc = "GAA10774.3";
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(acc);
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


static CScope::TIds s_LoadScaffoldIdsOnce(const string& name)
{
    CScope::TIds ids;
    {{
        CVDBMgr mgr;
        CWGSDb wgs_db(mgr, name);
        size_t limit_count = 30000, start_row = 1, count = 0;
        for ( CWGSScaffoldIterator it(wgs_db, start_row); it; ++it ) {
            ++count;
            if (limit_count > 0 && count > limit_count)
                break;
            ids.push_back(CSeq_id_Handle::GetHandle(*it.GetAccSeq_id()));
        }
    }}
    return ids;
}


static CScope::TIds s_LoadScaffoldIds(const string& name)
{
    return s_CallWithRetry(bind(&s_LoadScaffoldIdsOnce,
                                name),
                           "s_LoadScaffoldIds", 3);
}


static const bool s_MakeFasta = 0;


BOOST_AUTO_TEST_CASE(Scaffold2Fasta)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    CStopWatch sw(CStopWatch::eStart);
    CScope::TIds ids = s_LoadScaffoldIds("ALWZ01");

    CScope scope(*om);
    scope.AddDefaults();

    unique_ptr<CNcbiOstream> out;
    unique_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    ITERATE ( CScope::TIds, it, ids ) {
        //scope.ResetHistory();
        CBioseq_Handle scaffold = scope.GetBioseqHandle(*it);
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }
    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(StateTest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();

    CBioseq_Handle bh;

    if ( 0 ) {
        // CDBB01000001 is suppressed temporarily in ID.
        // It's marked as suppressed permanently by WGS VDB reader
        // because there is no distinction between these suppressions.
        bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CDBB01000001"));
        BOOST_REQUIRE(bh);
        /*
          NcbiCout << bh.GetAccessSeq_id_Handle() << ": "
          << bh.GetState() << " vs " << sx_LoadFromGB(bh).GetState()
          << NcbiEndl;
        */
        BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));
    }

    // AFFP01000011 is dead and suppressed permanently in ID.
    // It's marked as only dead by WGS VDB reader currently (2/1/2019)
    // because there's no way to store both 'dead' and 'suppressed' bits together.
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000011"));
    BOOST_REQUIRE(bh);
    /*
      NcbiCout << bh.GetAccessSeq_id_Handle() << ": "
      << bh.GetState() << " vs " << sx_LoadFromGB(bh).GetState()
      << NcbiEndl;
    */
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    if ( 0 ) {
        // JPNT01000001 is dead and suppressed permanently in ID.
        // It's marked as only suppressed by WGS VDB reader currently (2/1/2019)
        // because there's no way to store both 'dead' and 'suppressed' bits together.
        bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("JPNT01000001"));
        BOOST_REQUIRE(bh);
        /*
          NcbiCout << bh.GetAccessSeq_id_Handle() << ": "
          << bh.GetState() << " vs " << sx_LoadFromGB(bh).GetState()
          << NcbiEndl;
        */
        BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));
    }
}


BOOST_AUTO_TEST_CASE(Scaffold2Fasta2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    CStopWatch sw(CStopWatch::eStart);
    CScope::TIds ids = s_LoadScaffoldIds("ALWZ02");

    CScope scope(*om);
    scope.AddDefaults();

    unique_ptr<CNcbiOstream> out;
    unique_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    CScope::TBioseqHandles handles = scope.GetBioseqHandles(ids);
    ITERATE ( CScope::TBioseqHandles, it, handles ) {
        CBioseq_Handle scaffold = *it;
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }

    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(WithdrawnStateTest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();

    CBioseq_Handle bh;

    // AFFP02000011.1 is live
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP02000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    // AFFP01000011.1 is dead and suppressed permanently in ID.
    // It's marked as only dead by WGS VDB reader currently (2/1/2019)
    // because there's no way to store both 'dead' and 'suppressed' bits together.
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(),
                      CBioseq_Handle::fState_dead);
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    // AFFP01000012.1 is dead, suppressed permanently, and withdrawn in ID.
    // It's marked as only dead and withdrawn by WGS VDB reader currently (2/1/2019)
    // because there's no way to store both 'dead' and 'suppressed' bits together.
    // It's marked as only withdrawn by OSG WGS plugin currently (2/1/2019).
    // Note that Withdrawn state assumes 'no_data'.
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000012.1"));
    BOOST_CHECK(!bh);
    if ( !sx_StatesMatch(bh.GetState(),
                         CBioseq_Handle::fState_no_data |
                         CBioseq_Handle::fState_suppress_perm |
                         CBioseq_Handle::fState_dead |
                         CBioseq_Handle::fState_withdrawn) ) {
        BOOST_CHECK_EQUAL(bh.GetState(),
                          CBioseq_Handle::fState_no_data |
                          CBioseq_Handle::fState_withdrawn);
    }
}


BOOST_AUTO_TEST_CASE(QualityTest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ABYI02000001.1"));
    BOOST_REQUIRE(bh);
    CGraph_CI graph_it(bh, SAnnotSelector().AddNamedAnnots("Phrap Graph"));
    BOOST_CHECK_EQUAL(graph_it.GetSize(), 1u);
    CFeat_CI feat_it(bh);
    BOOST_CHECK_EQUAL(feat_it.GetSize(), 116u);
}


BOOST_AUTO_TEST_CASE(GITest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    //bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AABR01000100.1"));
    //BOOST_CHECK(bh);

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(25725721));
    BOOST_CHECK(bh);
    BOOST_CHECK(bh && sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(25561849));
    BOOST_CHECK(bh);
    BOOST_CHECK(bh && sx_Equal(bh, sx_LoadFromGB(bh)));

    // zero value in gi index
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(25561850));
    BOOST_CHECK(!bh);

    // absent value in gi index
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(25562845));
    BOOST_CHECK(!bh);

    // gi is out of WGS range
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(HashTest)
{
    CSeq_id_Handle id = CSeq_id_Handle::GetHandle("BBXB01000080.1");

    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    CScope wgs_scope(*om);
    wgs_scope.AddDefaults();

    int wgs_hash = wgs_scope.GetSequenceHash(id);
    BOOST_CHECK(wgs_hash != 0);

    sx_InitGBLoader(*om);
    CScope gb_scope(*om);
    gb_scope.AddDataLoader("GBLOADER");
    
    int gb_hash = gb_scope.GetSequenceHash(id);
    BOOST_CHECK(gb_hash != 0);

    BOOST_CHECK_EQUAL(wgs_hash, gb_hash);
}


BOOST_AUTO_TEST_CASE(FetchNoSeq1)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "NZ_AAAA01000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(!bh);
}


BOOST_AUTO_TEST_CASE(FetchNoSeq2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA010000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(!bh);
}


BOOST_AUTO_TEST_CASE(FetchNoSeq3)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA09000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(!bh);
}


BOOST_AUTO_TEST_CASE(FetchNoSeq4)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|XXXX01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(!bh);
}


BOOST_AUTO_TEST_CASE(FetchNoSeq5)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAPB01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(!bh);
}


BOOST_AUTO_TEST_CASE(FetchMinorVer1)
{
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }

    CRef<CObjectManager> om = sx_GetEmptyOM();
    string loader_name =
        CWGSDataLoader::RegisterInObjectManager(*om, "", vector<string>(1, "AAAB01.4")).GetLoader()->GetName();

    string id = "AAAB01000034";
    CScope scope(*om);
    scope.AddDataLoader(loader_name);

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    CSeqdesc_CI it(bh, CSeqdesc::e_Source, 1);
    BOOST_REQUIRE(it);
    ++it;
    BOOST_CHECK(!it);
}


BOOST_AUTO_TEST_CASE(FetchMinorVer2)
{
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }

    CRef<CObjectManager> om = sx_GetEmptyOM();
    string loader_name =
        CWGSDataLoader::RegisterInObjectManager(*om, "", vector<string>(1, "AAAB01.5")).GetLoader()->GetName();

    string id = "AAAB01000034";
    CScope scope(*om);
    scope.AddDataLoader(loader_name);

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Source, 1));
    CSeqdesc_CI it(bh, CSeqdesc::e_Source);
    BOOST_REQUIRE(it);
    ++it;
    BOOST_CHECK(!it);
}


BOOST_AUTO_TEST_CASE(FetchIndex2)
{
    if ( !sx_HasNewWGSRepository() ) {
        return;
    }

    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(1174167502));
    BOOST_CHECK(bh);
    BOOST_CHECK(bh && sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("SMB81856"));
    BOOST_CHECK(bh);
    BOOST_CHECK(bh && sx_Equal(bh, sx_LoadFromGB(bh)));
}


BOOST_AUTO_TEST_CASE(TestReplaced)
{
    bool config_keep_replaced = true;
    
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ABAJ01000001"));
    BOOST_CHECK_EQUAL(bool(bh), config_keep_replaced);

    sx_InitGBLoader(*om);
    scope.AddDataLoader("GBLOADER");
    
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ABAJ01000001"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestReplacedProtein1)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CAP20437"));
    BOOST_CHECK(!bh);

    sx_InitGBLoader(*om);
    scope.AddDataLoader("GBLOADER");
    
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CAP20437"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestReplacedProtein2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CAP20511"));
    BOOST_CHECK(!bh);

    sx_InitGBLoader(*om);
    scope.AddDataLoader("GBLOADER");
    
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CAP20511"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestReplacedProtein3)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CAP20512"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestReplacedProtein4)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("309367003"));
    BOOST_CHECK(!bh);

    sx_InitGBLoader(*om);
    scope.AddDataLoader("GBLOADER");
    
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("309367003"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestReplacedProtein5)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("309366966"));
    BOOST_CHECK(!bh);

    sx_InitGBLoader(*om);
    scope.AddDataLoader("GBLOADER");
    
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("309366966"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestReplacedProtein6)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("309366967"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestReplacedNoGI)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("JACYIV010000036.1"));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr)
{
    LOG_POST("Checking WGS master sequence descriptors");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("BASL01000795.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Genbank));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(total_count, 10);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr4)
{
    LOG_POST("Checking WGS master sequence descriptors 4+9");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CAAD010000001.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Embl));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 1u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr7)
{
    LOG_POST("Checking WGS master sequence descriptors: Unverified");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AVKQ01000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 3u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["Unverified"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr8)
{
    LOG_POST("Checking VDB WGS master sequence descriptors on a detached protein");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("MBA2057862.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(total_count, 12);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr9)
{
    LOG_POST("Checking VDB WGS master sequence descriptors on scaffolds");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ALWZ04S0000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Title)));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 4);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 10);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr10)
{
    LOG_POST("Checking WGS master sequence descriptors with split main seqence");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("KGB65708"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr11)
{
    LOG_POST("Checking WGS master sequence descriptors with split main seqence");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("JRAV01000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 8);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr12)
{
    LOG_POST("Checking VDB WGS master sequence descriptors with split main seqence");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AAHIBN010000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Genbank));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(total_count, 11);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr13)
{
    LOG_POST("Checking VDB WGS master sequence descriptors with partial optional desc");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ALWZ040100108.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 4);
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 11);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr14)
{
    LOG_POST("Checking VDB WGS master sequence descriptors with partial optional desc");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ALWZ050100108.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 4);
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 10);
}


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(StateTest);
    NCBITEST_DISABLE(WithdrawnStateTest);
}
