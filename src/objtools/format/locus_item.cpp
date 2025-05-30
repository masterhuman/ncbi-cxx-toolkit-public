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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   flat-file generator -- locus item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/general_macros.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seq_macros.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqblock/PDB_replace.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/indexer.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/context.hpp>
#include <objmgr/util/objutil.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


CLocusItem::CLocusItem(CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_Length(0), m_Biomol(CMolInfo::eBiomol_unknown), m_Date("01-JAN-1900")
{
   x_GatherInfo(ctx);
}

IFlatItem::EItem CLocusItem::GetItemType(void) const
{
    return eItem_Locus;
}

void CLocusItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatLocus(*this, text_os);
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


static bool s_IsGenomeView(const CBioseqContext& ctx)
{
    return ((ctx.IsSegmented()  &&  !ctx.HasParts())  ||
        (ctx.IsDelta()  &&  !ctx.IsDeltaLitOnly()));
}

void CLocusItem::x_GatherInfo(CBioseqContext& ctx)
{
    CSeqdesc_CI mi_desc(ctx.GetHandle(), CSeqdesc::e_Molinfo);
    if ( mi_desc ) {
        x_SetObject(mi_desc->GetMolinfo());
    }

    // NB: order of execution is important, as some values depend on others
    x_SetName(ctx);
    x_SetLength(ctx);
    x_SetBiomol(ctx);   // must come before x_SetStrand
    x_SetStrand(ctx);
    x_SetTopology(ctx);
    x_SetDivision(ctx);
    x_SetDate(ctx);
}


// Name


static bool s_IsSeperatorNeeded(const string& basename, size_t suffix_len)
{
    // This first check put here to emulate what may be a bug in the original
    // code which adds an 'S' segment seperator only if it DOES make the
    // string longer than the max.

    if (basename.length() + suffix_len < 16) {
        return false;
    }

    char last = basename[basename.length() - 1];
    char next_to_last = basename[basename.length() - 2];

    // If the last character is not a digit then don't use a seperator.
    if (!isdigit((unsigned char) last)) {
        return false;
    }

    // If the last two characters are a non-digit followed by a '0',
    // then don't use seperator.
    if ((last == '0')  &&  (!isdigit((unsigned char) next_to_last))) {
        return false;
    }

    // If we made it to here, use a seperator
    return true;
}


static void s_AddLocusSuffix(string &basename, CBioseqContext& ctx)
{
    size_t numsegs = ctx.GetMaster().GetNumParts();
    // If there's one or less segments, no suffix is needed.
    if (numsegs <= 1) {
        return;
    }
    // If the basestring has one or less characters, no suffix is needed.
    if (basename.length() <= 1) {
        return;
    }

    // Add the suffix
    ostringstream locus;
    locus << basename;

    size_t suffix_len = NStr::NumericToString(numsegs).length();

    if (s_IsSeperatorNeeded(basename, suffix_len)) {
        locus << 'S';
    }
    locus << setfill('0') << setw(suffix_len) << ctx.GetPartNumber();
    basename = locus.str();
}

static string s_FastaGetOriginalID (CBioseqContext& ctx)

{
    const CBioseq_Handle& bsh = ctx.GetHandle();
    const CBioseq& seq = *bsh.GetCompleteBioseq();

    FOR_EACH_SEQDESC_ON_BIOSEQ (it, seq) {
        const CSeqdesc& desc = **it;
        if (! desc.IsUser()) continue;
        if (! desc.GetUser().IsSetType()) continue;
        const CUser_object& usr = desc.GetUser();
        const CObject_id& oi = usr.GetType();
        if (! oi.IsStr()) continue;
        const string& type = oi.GetStr();
        if (! NStr::EqualNocase(type, "OrginalID") && ! NStr::EqualNocase(type, "OriginalID")) continue;
        FOR_EACH_USERFIELD_ON_USEROBJECT (uitr, usr) {
            const CUser_field& fld = **uitr;
            if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
                const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                if (! NStr::EqualNocase(label_str, "LocalId")) continue;
                if (fld.IsSetData() && fld.GetData().IsStr()) {
                    return fld.GetData().GetStr();
                }
            }
        }
    }

    return "";
}

static bool s_ShouldUseOriginalID (CBioseqContext& ctx)
{
    if (ctx.Config().IsFormatLite()) return false;

    const CBioseq_Handle& bsh = ctx.GetHandle();
    const CBioseq& seq = *bsh.GetCompleteBioseq();

    FOR_EACH_SEQID_ON_BIOSEQ (id_itr, seq) {
        const CSeq_id& sid = **id_itr;
        switch (sid.Which()) {
            case CSeq_id::e_Local:
                break;
            case CSeq_id::e_General:
                {
                    const CDbtag& dbtag = sid.GetGeneral();
                    if (dbtag.IsSetDb()) {
                        const string& db = dbtag.GetDb();
                        if (! NStr::EqualNocase(db, "TMSMART") &&
                            ! NStr::EqualNocase(db, "BankIt") &&
                            ! NStr::EqualNocase(db, "NCBIFILE")) {
                            return false;
                        }
                    }
                }
                break;
            default:
                return false;
        }
    }

    return true;
}


void CLocusItem::x_SetName(CBioseqContext& ctx)
{
    try {
        if (ctx.IsPart()) {
            string basename = ctx.GetMaster().GetBaseName();
            if (!NStr::IsBlank(basename)) {
                m_Name = basename;
                m_FullName = m_Name;
                s_AddLocusSuffix(m_Name, ctx);
                return;
            }
        }

        CSeq_id_Handle id = GetId(ctx.GetHandle(), eGetId_Best);

        /*CSeq_id::E_Choice choice = id->Which();
        switch (choice) {
        case CSeq_id::e_Other:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
        case CSeq_id::e_Pdb:
        case CSeq_id::e_Tpd:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpg:
        break;
        default:
        id.Reset();
        break;
        }
        if (!id) {
        ITERATE (CBioseq::TId, it, seq->GetId()) {
        if ((*it)->IsGenbank()) {
        id = *it;
        break;
        }
        }
        }
        if (!id) {
        return;
        }*/

        const static size_t MAX_LOCUS_ACCN_LEN = 38;

        const CTextseq_id* tsip = id.GetSeqId()->GetTextseq_Id();
        if (tsip) {
            if (s_IsGenomeView(ctx)) {
                if (tsip->IsSetAccession()) {
                    m_Name = tsip->GetAccession();
                }
            } else {
                if (tsip->IsSetName()) {
                    m_Name = tsip->GetName();
                    if (x_NameHasBadChars(m_Name)  &&  tsip->IsSetAccession()) {
                        m_Name = tsip->GetAccession();
                    }
                }
            }
        }

        if (x_NameHasBadChars(m_Name)  ||  NStr::IsBlank(m_Name)) {
            m_Name = id.GetSeqId()->GetSeqIdString();
            if (s_ShouldUseOriginalID(ctx)) {
                string origID = s_FastaGetOriginalID(ctx);
                if (! NStr::IsBlank(origID)) {
                    m_Name = origID;
                }
            }
        }

        m_FullName = m_Name;
        if( m_Name.length() > MAX_LOCUS_ACCN_LEN ) {
            m_Name.resize(MAX_LOCUS_ACCN_LEN);
            *m_Name.rbegin() = '>';
        }
    } catch (CException&) {
    }
}


bool CLocusItem::x_NameHasBadChars(const string& name) const
{
    ITERATE(string, iter, name) {
        unsigned char current_char = (unsigned char)(*iter);
        if ( !isalnum(current_char) && current_char != '_' ) {
            return true;
        }
    }
    return false;
}


// Length

void CLocusItem::x_SetLength(CBioseqContext& ctx)
{
    m_Length = sequence::GetLength(ctx.GetLocation(), &ctx.GetScope());
}


// Strand

void CLocusItem::x_SetStrand(CBioseqContext& ctx)
{
    const CBioseq_Handle& bsh = ctx.GetHandle();

    CSeq_inst::TMol bmol = bsh.IsSetInst_Mol() ?
        bsh.GetInst_Mol() : CSeq_inst::eMol_not_set;

    m_Strand = bsh.IsSetInst_Strand() ?
        bsh.GetInst_Strand() : CSeq_inst::eStrand_not_set;
    if ( m_Strand == CSeq_inst::eStrand_other ) {
        m_Strand = CSeq_inst::eStrand_not_set;
    }

    // cleanup for formats other than GBSeq
    if ( !ctx.Config().IsFormatGBSeq() && !ctx.Config().IsFormatINSDSeq() ) {
        // if ds-DNA don't show ds
        if ( bmol == CSeq_inst::eMol_dna  &&  m_Strand == CSeq_inst::eStrand_ds ) {
            m_Strand = CSeq_inst::eStrand_not_set;
        }

        // ss-any RNA don't show ss
        const bool is_nucleic_acid = bmol <= CSeq_inst::eMol_rna;
        const bool is_in_set_1 = (m_Biomol >= CMolInfo::eBiomol_mRNA &&
            m_Biomol <= CMolInfo::eBiomol_peptide);
        const bool is_in_set_2 = (m_Biomol >= CMolInfo::eBiomol_cRNA &&
            m_Biomol <= CMolInfo::eBiomol_tmRNA);
        if ( (! is_nucleic_acid  || is_in_set_1 || is_in_set_2)  &&
            m_Strand == CSeq_inst::eStrand_ss ) {
            m_Strand = CSeq_inst::eStrand_not_set;
        }
    }
}


// Biomol

void CLocusItem::x_SetBiomol(CBioseqContext& ctx)
{
    if ( ctx.IsProt() ) {
        return;
    }

    CSeq_inst::TMol bmol = ctx.GetHandle().GetBioseqMolType();

    if ( bmol > CSeq_inst::eMol_aa ) {
        bmol = CSeq_inst::eMol_not_set;
    }

    const CMolInfo* molinfo = dynamic_cast<const CMolInfo*>(GetObject());
    if ( molinfo  &&  molinfo->GetBiomol() <= CMolInfo::eBiomol_tmRNA ) {
        m_Biomol = molinfo->GetBiomol();
    }

    if ( m_Biomol <= CMolInfo::eBiomol_genomic ) {
        if ( bmol == CSeq_inst::eMol_aa ) {
            m_Biomol = CMolInfo::eBiomol_peptide;
        } else if ( bmol == CSeq_inst::eMol_na ) {
            m_Biomol = CMolInfo::eBiomol_unknown;
        } else if ( bmol == CSeq_inst::eMol_rna ) {
            m_Biomol = CMolInfo::eBiomol_pre_RNA;
        } else {
            m_Biomol = CMolInfo::eBiomol_genomic;
        }
    } else if ( m_Biomol == CMolInfo::eBiomol_other_genetic ) {
        if ( bmol == CSeq_inst::eMol_rna ) {
            m_Biomol = CMolInfo::eBiomol_pre_RNA;
        }
    }
}


// Topology

void CLocusItem::x_SetTopology(CBioseqContext& ctx)
{
    const CBioseq_Handle& bsh = ctx.GetHandle();

    m_Topology = bsh.GetInst_Topology();
    const CSeq_loc& loc = ctx.GetLocation();
    if ( loc.IsWhole() ) {
        return;
    }
    if ( loc.IsInt()) {
        if ( m_Topology ==CSeq_inst::eTopology_circular ) {
            const CSeq_interval& ival = loc.GetInt();
            if (ival.GetFrom() == 0 && bsh.IsSetInst_Length() && ival.GetTo() == bsh.GetBioseqLength() - 1 ) {
                if (ival.CanGetStrand() && ival.GetStrand() == eNa_strand_minus) {
                    return;
                }
            }
        }
    }
    // otherwise an interval is always linear
    m_Topology = CSeq_inst::eTopology_linear;
}


// Division

static CTempString x_GetDivisionProcIdx(const CBioseqContext& ctx, const CBioseq_Handle& bsh,
                                     bool is_prot, CMolInfo::TTech tech)

{
    CTempString division;

    const CBioSource* bsrc = nullptr;

    CRef<CSeqEntryIndex> idx = ctx.GetSeqEntryIndex();
    if (! idx) return division;
    CRef<CBioseqIndex> bsx = idx->GetBioseqIndex (bsh);
    if (! bsx) return division;

    // Find the BioSource object for this sequnece.
    CConstRef<CBioSource> srcr = bsx->GetBioSource();
    if (srcr) {
        bsrc = &(*srcr);
    } else if ( is_prot ) {
        CRef<CFeatureIndex> sfxp = bsx->GetFeatureForProduct();
        if (sfxp) {
            CRef<CFeatureIndex> fsx = sfxp->GetOverlappingSource();
            if (fsx) {
                bsrc = &(fsx->GetMappedFeat().GetData().GetBiosrc());
            }
        }
    }

    bool is_transgenic = false;
    bool is_env_sample = false;

    CBioSource::TOrigin origin = CBioSource::eOrigin_unknown;
    if ( bsrc ) {
        origin = bsrc->GetOrigin();
        if ( bsrc->CanGetOrg() ) {
            const COrg_ref& org = bsrc->GetOrg();
            if ( org.CanGetOrgname()  &&  org.GetOrgname().CanGetDiv() ) {
                division = org.GetOrgname().GetDiv();
            }
        }

        ITERATE(CBioSource::TSubtype, it, bsrc->GetSubtype() ) {
            CSubSource::TSubtype subtype = (*it)->IsSetSubtype() ?
                (*it)->GetSubtype() : CSubSource::eSubtype_other;
            if (subtype == CSubSource::eSubtype_transgenic) {
                is_transgenic = true;
            } else if (subtype == CSubSource::eSubtype_environmental_sample) {
                is_env_sample = true;
            }
        }
    }

    switch (tech) {
    case CMolInfo::eTech_est:
        division = "EST";
        break;
    case CMolInfo::eTech_sts:
        division = "STS";
        break;
    case CMolInfo::eTech_survey:
        division = "GSS";
        break;
    case CMolInfo::eTech_htgs_0:
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
        division = "HTG";
        break;
    case CMolInfo::eTech_htc:
        division = "HTC";
        break;
    case CMolInfo::eTech_tsa:
        division = "TSA";
        break;
    default:
        break;
    }

    if (origin == CBioSource::eOrigin_synthetic   ||
        origin == CBioSource::eOrigin_mut         ||
        origin == CBioSource::eOrigin_artificial  ||
        is_transgenic ) {
        division = "SYN";
    } else if (is_env_sample) {
        if (tech == CMolInfo::eTech_unknown          ||
            tech == CMolInfo::eTech_standard         ||
            tech == CMolInfo::eTech_htgs_3           ||
            tech == CMolInfo::eTech_wgs              ||
            tech == CMolInfo::eTech_concept_trans    ||
            tech == CMolInfo::eTech_seq_pept         ||
            tech == CMolInfo::eTech_both             ||
            tech == CMolInfo::eTech_seq_pept_overlap ||
            tech == CMolInfo::eTech_seq_pept_homol   ||
            tech == CMolInfo::eTech_concept_trans_a  ||
            tech == CMolInfo::eTech_other) {
            division = "ENV";
        }
    }

    if (is_transgenic && tech == CMolInfo::eTech_survey) {
        division = "GSS";
    }

    ITERATE (CBioseq_Handle::TId, iter, bsh.GetId()) {
        if (*iter  &&  iter->GetSeqId()->IsPatent()) {
            division = "PAT";
            break;
        }
    }
    if ( is_prot ) {
        const CSeq_feat* cds = GetCDSForProduct(bsh);
        if ( cds ) {
            const CSeq_loc & cds_loc = cds->GetLocation();
            const CSeq_id *pSeqId = cds_loc.GetId();
            CBioseq_Handle nuc;
            if( pSeqId ) {
                nuc = bsh.GetScope().GetBioseqHandle(*pSeqId);
            } else {
                // fall back on first Seq-id in location that is local to this Seq-entry
                // (example where this code is needed to prevent a crash: AL772325.4 proteins)
                ITERATE( CSeq_loc, cds_loc_iter, cds_loc ) {
                    try {
                        CSeq_id_Handle cds_loc_piece_id = cds_loc_iter.GetSeq_id_Handle();
                        if( cds_loc_piece_id ) {
                            nuc = bsh.GetScope().GetBioseqHandleFromTSE(
                                cds_loc_piece_id, bsh );
                            if( nuc ) {
                                break;
                            }
                        }
                    } catch(...) {
                        // ignore exceptions because it's common for us to fail
                        // to find something since we're only looking locally
                    }
                }
            }
            if( nuc ) {
                ITERATE (CBioseq_Handle::TId, iter, nuc.GetId()) {
                    if (*iter  &&  iter->GetSeqId()->IsPatent()) {
                        division = "PAT";
                        break;
                    }
                }
            }
        }
    }

    // more complicated code for division, if necessary, goes here

    for (CSeqdesc_CI gb_desc(bsh, CSeqdesc::e_Genbank); gb_desc; ++gb_desc) {
        const CGB_block& gb = gb_desc->GetGenbank();
        if (gb.CanGetDiv()) {
            if (division.empty()    ||
                gb.GetDiv() == "SYN"  ||
                gb.GetDiv() == "PAT") {
                    division = gb.GetDiv();
            }
        }
    }

    // Set a default value (3 spaces)
    if ( division.empty() ) {
        division = "   ";
    }

    return division;
}

static CTempString x_GetDivisionProc(const CBioseq_Handle& bsh, bool is_prot,
                                     CMolInfo::TTech tech)

{
    CTempString division;

    const CBioSource* bsrc = nullptr;

    // Find the BioSource object for this sequnece.
    CSeqdesc_CI src_desc(bsh, CSeqdesc::e_Source);
    if ( src_desc ) {
        bsrc = &(src_desc->GetSource());
    } else {
        CFeat_CI feat(bsh, CSeqFeatData::e_Biosrc);
        if ( feat ) {
            bsrc = &(feat->GetData().GetBiosrc());
        } else if ( is_prot ) {
            // if protein with no sources, get sources applicable to
            // DNA location of CDS
            const CSeq_feat* cds = GetCDSForProduct(bsh);
            if ( cds ) {
                CConstRef<CSeq_feat> nuc_fsrc = GetBestOverlappingFeat(
                    cds->GetLocation(),
                    CSeqFeatData::e_Biosrc,
                    eOverlap_Contains,
                    bsh.GetScope());
                if ( nuc_fsrc ) {
                    bsrc = &(nuc_fsrc->GetData().GetBiosrc());
                } else {
                    CBioseq_Handle nuc =
                        bsh.GetScope().GetBioseqHandle(cds->GetLocation());
                    CSeqdesc_CI nuc_dsrc(nuc, CSeqdesc::e_Source);
                    if ( nuc_dsrc ) {
                        bsrc = &(nuc_dsrc->GetSource());
                    }
                }
            }
        }
    }

    bool is_transgenic = false;
    bool is_env_sample = false;

    CBioSource::TOrigin origin = CBioSource::eOrigin_unknown;
    if ( bsrc ) {
        origin = bsrc->GetOrigin();
        if ( bsrc->CanGetOrg() ) {
            const COrg_ref& org = bsrc->GetOrg();
            if ( org.CanGetOrgname()  &&  org.GetOrgname().CanGetDiv() ) {
                division = org.GetOrgname().GetDiv();
            }
        }

        ITERATE(CBioSource::TSubtype, it, bsrc->GetSubtype() ) {
            CSubSource::TSubtype subtype = (*it)->IsSetSubtype() ?
                (*it)->GetSubtype() : CSubSource::eSubtype_other;
            if (subtype == CSubSource::eSubtype_transgenic) {
                is_transgenic = true;
            } else if (subtype == CSubSource::eSubtype_environmental_sample) {
                is_env_sample = true;
            }
        }
    }

    switch (tech) {
    case CMolInfo::eTech_est:
        division = "EST";
        break;
    case CMolInfo::eTech_sts:
        division = "STS";
        break;
    case CMolInfo::eTech_survey:
        division = "GSS";
        break;
    case CMolInfo::eTech_htgs_0:
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
        division = "HTG";
        break;
    case CMolInfo::eTech_htc:
        division = "HTC";
        break;
    case CMolInfo::eTech_tsa:
        division = "TSA";
        break;
    default:
        break;
    }

    if (origin == CBioSource::eOrigin_synthetic   ||
        origin == CBioSource::eOrigin_mut         ||
        origin == CBioSource::eOrigin_artificial  ||
        is_transgenic ) {
        division = "SYN";
    } else if (is_env_sample) {
        if (tech == CMolInfo::eTech_unknown          ||
            tech == CMolInfo::eTech_standard         ||
            tech == CMolInfo::eTech_htgs_3           ||
            tech == CMolInfo::eTech_wgs              ||
            tech == CMolInfo::eTech_concept_trans    ||
            tech == CMolInfo::eTech_seq_pept         ||
            tech == CMolInfo::eTech_both             ||
            tech == CMolInfo::eTech_seq_pept_overlap ||
            tech == CMolInfo::eTech_seq_pept_homol   ||
            tech == CMolInfo::eTech_concept_trans_a  ||
            tech == CMolInfo::eTech_other) {
            division = "ENV";
        }
    }

    if (is_transgenic && tech == CMolInfo::eTech_survey) {
        division = "GSS";
    }

    ITERATE (CBioseq_Handle::TId, iter, bsh.GetId()) {
        if (*iter  &&  iter->GetSeqId()->IsPatent()) {
            division = "PAT";
            break;
        }
    }
    if ( is_prot ) {
        const CSeq_feat* cds = GetCDSForProduct(bsh);
        if ( cds ) {
            const CSeq_loc & cds_loc = cds->GetLocation();
            const CSeq_id *pSeqId = cds_loc.GetId();
            CBioseq_Handle nuc;
            if( pSeqId ) {
                nuc = bsh.GetScope().GetBioseqHandle(*pSeqId);
            } else {
                // fall back on first Seq-id in location that is local to this Seq-entry
                // (example where this code is needed to prevent a crash: AL772325.4 proteins)
                ITERATE( CSeq_loc, cds_loc_iter, cds_loc ) {
                    try {
                        CSeq_id_Handle cds_loc_piece_id = cds_loc_iter.GetSeq_id_Handle();
                        if( cds_loc_piece_id ) {
                            nuc = bsh.GetScope().GetBioseqHandleFromTSE(
                                cds_loc_piece_id, bsh );
                            if( nuc ) {
                                break;
                            }
                        }
                    } catch(...) {
                        // ignore exceptions because it's common for us to fail
                        // to find something since we're only looking locally
                    }
                }
            }
            if( nuc ) {
                ITERATE (CBioseq_Handle::TId, iter, nuc.GetId()) {
                    if (*iter  &&  iter->GetSeqId()->IsPatent()) {
                        division = "PAT";
                        break;
                    }
                }
            }
        }
    }

    // more complicated code for division, if necessary, goes here

    for (CSeqdesc_CI gb_desc(bsh, CSeqdesc::e_Genbank); gb_desc; ++gb_desc) {
        const CGB_block& gb = gb_desc->GetGenbank();
        if (gb.CanGetDiv()) {
            if (division.empty()    ||
                gb.GetDiv() == "SYN"  ||
                gb.GetDiv() == "PAT") {
                    division = gb.GetDiv();
            }
        }
    }

    // Set a default value (3 spaces)
    if ( division.empty() ) {
        division = "   ";
    }

    return division;
}

string CLocusItem::GetDivision(const CBioseq_Handle& bsh, const CBioseqContext* ctx)
{
    if ( bsh.IsSetInst_Repr() && bsh.GetInst_Repr() == CSeq_inst::eRepr_delta) {
        if (bsh.IsSetInst_Ext()) {
            const CBioseq_Handle::TInst_Ext& ext = bsh.GetInst_Ext();
            if ( ext.IsDelta() ) {
                ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
                    if ( (*it)->IsLoc() ) {
                        return "CON";
                    }
                }
            }
        }
    }

    CMolInfo::TTech tech = CMolInfo::eTech_unknown;

    CSeqdesc_CI::TDescChoices desc_choices;
    desc_choices.reserve(1);
    desc_choices.push_back(CSeqdesc::e_Molinfo);

    for (CSeqdesc_CI desc_it(bsh, desc_choices); desc_it;  ++desc_it) {
        if (desc_it->Which() == CSeqdesc::e_Molinfo) {
            tech = desc_it->GetMolinfo().GetTech();
            break;
        }
    }

    if (ctx && ctx->UsingSeqEntryIndex()) {
        return x_GetDivisionProcIdx (*ctx, bsh, bsh.IsAa(), tech);
    }

    return x_GetDivisionProc (bsh, bsh.IsAa(), tech);
}

void CLocusItem::x_SetDivision(CBioseqContext& ctx)
{
    // contig style (old genome_view flag) forces CON division
    if ( ctx.DoContigStyle() ) {
        m_Division = "CON";
        return;
    }
    // "genome view" forces CON division
    if (s_IsGenomeView(ctx)){
        m_Division = "CON";
        return;
    }

    const CBioseq_Handle& bsh = ctx.GetHandle();

    if ( ctx.UsingSeqEntryIndex() ) {
        m_Division = x_GetDivisionProcIdx (ctx, bsh, ctx.IsProt(), ctx.GetTech());
    } else {
        m_Division = x_GetDivisionProc (bsh, ctx.IsProt(), ctx.GetTech());
    }

    const CMolInfo* molinfo = dynamic_cast<const CMolInfo*>(GetObject());
    if ( ctx.Config().IsFormatEMBL() ) {
        for ( CSeqdesc_CI embl_desc(bsh, CSeqdesc::e_Embl);
            embl_desc;
            ++embl_desc ) {
                const CEMBL_block& embl = embl_desc->GetEmbl();
                if ( embl.CanGetDiv() ) {
                    if (embl.GetDiv() == CEMBL_block::eDiv_other && ! molinfo) {
                            m_Division = "HUM";
                        } else {
                            m_Division = embl.GetDiv();
                        }
                }
            }
    }

    // Set a default value (3 spaces)
    if ( m_Division.empty() ) {
        m_Division = "   ";
    }
}


// Date

void CLocusItem::x_SetDate(CBioseqContext& ctx)
{
    const CBioseq_Handle& bsh = ctx.GetHandle();

    const CDate* date = x_GetDateForBioseq(bsh);
    if (! date) {
        // if bioseq is product of CDS or mRNA feature, get
        // date from parent bioseq.
        CBioseq_Handle parent = GetNucleotideParent(bsh);
        if ( parent ) {
            date = x_GetDateForBioseq(parent);
        }
    }

    if ( date ) {
        m_Date.erase();
        DateToString(*date, m_Date);
    }
}


const CDate* CLocusItem::x_GetDateForBioseq(const CBioseq_Handle& bsh) const
{
    const CDate* result = nullptr;

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Update_date);
        if ( desc ) {
            result = &(desc->GetUpdate_date());
        }
    }}

    {{
        // temporarily (???) also look at genbank block entry date
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Genbank);
        if ( desc ) {
            const CGB_block& gb = desc->GetGenbank();
            if ( gb.CanGetEntry_date() ) {
                result = x_GetLaterDate(result, &gb.GetEntry_date());
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Embl);
        if ( desc ) {
            const CEMBL_block& embl = desc->GetEmbl();
            if ( embl.CanGetCreation_date() ) {
                result = x_GetLaterDate(result, &embl.GetCreation_date());
            }
            if ( embl.CanGetUpdate_date() ) {
                result = x_GetLaterDate(result, &embl.GetUpdate_date());
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Sp);
        if ( desc ) {
            const CSP_block& sp = desc->GetSp();
            if ( sp.CanGetCreated()  &&  sp.GetCreated().IsStd() ) {
                result = x_GetLaterDate(result, &sp.GetCreated());
            }
            if ( sp.CanGetSequpd()  &&  sp.GetSequpd().IsStd() ) {
                result = x_GetLaterDate(result, &sp.GetSequpd());
            }
            if ( sp.CanGetAnnotupd()  &&  sp.GetAnnotupd().IsStd() ) {
                result = x_GetLaterDate(result, &sp.GetAnnotupd());
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Pdb);
        if ( desc ) {
            const CPDB_block& pdb = desc->GetPdb();
            if ( pdb.CanGetDeposition()  &&  pdb.GetDeposition().IsStd() ) {
                result = x_GetLaterDate(result, &pdb.GetDeposition());
            }
            if ( pdb.CanGetReplace() ) {
                const CPDB_replace& replace = pdb.GetReplace();
                if ( replace.CanGetDate()  &&   replace.GetDate().IsStd() ) {
                    result = x_GetLaterDate(result, &replace.GetDate());
                }
            }
        }
    }}

    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Create_date);
        if ( desc ) {
            result = x_GetLaterDate(result, &(desc->GetCreate_date()));
        }
    }}

    return result;
}


const CDate* CLocusItem::x_GetLaterDate(const CDate* d1, const CDate* d2) const
{
    if (! d1 || d1->Which() == CDate::e_Str) {
        return d2;
    }

    if (! d2 || d2->Which() == CDate::e_Str) {
        return d1;
    }

    return (d1->Compare(*d2) == CDate::eCompare_after) ? d1 : d2;
}


END_SCOPE(objects)
END_NCBI_SCOPE
