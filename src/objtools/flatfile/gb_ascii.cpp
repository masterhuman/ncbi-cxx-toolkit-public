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
 * File Name: gb_ascii.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Parse gb from blocks to asn.
 * Build GenBank format entry block.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seq/Seq_inst.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objmgr/scope.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>

#include "index.h"
#include "genbank.h"

#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>
#include "ftanet.h"

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "indx_blk.h"
#include "utilref.h"
#include "utilfeat.h"
#include "loadfeat.h"
#include "gb_ascii.h"
#include "add.h"
#include "nucprot.h"
#include "fta_qscore.h"
#include "citation.h"
#include "fcleanup.h"
#include "utilfun.h"
#include "entry.h"
#include "ref.h"
#include "xgbparint.h"
#include "xutils.h"


#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "gb_ascii.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/**********************************************************/
static char* GBDivOffset(const DataBlk& entry, Int4 div_shift)
{
    return (entry.mOffset + div_shift);
}

/**********************************************************/
static void CheckContigEverywhere(IndexblkPtr ibp, Parser::ESource source)
{
    bool condiv = (NStr::CompareNocase(ibp->division, "CON") == 0);

    if (condiv && ibp->segnum != 0) {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_ConDivInSegset, "Use of the CON division is not allowed for members of segmented set : %s|%s. Entry skipped.", ibp->locusname, ibp->acnum);
        ibp->drop = true;
        return;
    }

    if (! condiv && ibp->is_contig == false && ibp->origin == false &&
        ibp->is_mga == false) {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingSequenceData, "Required sequence data is absent. Entry dropped.");
        ibp->drop = true;
    } else if (! condiv && ibp->is_contig && ibp->origin == false) {
        ErrPostEx(SEV_WARNING, ERR_DIVISION_MappedtoCON, "Division [%s] mapped to CON based on the existence of CONTIG line.", ibp->division);
    } else if (ibp->is_contig && ibp->origin) {
        if (source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ) {
            ErrPostEx(SEV_INFO, ERR_FORMAT_ContigWithSequenceData, "The CONTIG/CO linetype and sequence data are both present. Ignoring sequence data.");
        } else {
            ErrPostEx(SEV_REJECT, ERR_FORMAT_ContigWithSequenceData, "The CONTIG/CO linetype and sequence data may not both be present in a sequence record.");
            ibp->drop = true;
        }
    } else if (condiv && ibp->is_contig == false && ibp->origin == false) {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingContigFeature, "No CONTIG data in GenBank format file, entry dropped.");
        ibp->drop = true;
    } else if (condiv && ibp->is_contig == false && ibp->origin) {
        ErrPostEx(SEV_WARNING, ERR_DIVISION_ConDivLacksContig, "Division is CON, but CONTIG data have not been found.");
    }
}

/**********************************************************/
bool GetGenBankInstContig(const DataBlk& entry, CBioseq& bsp, ParserPtr pp)
{
    DataBlkPtr dbp;

    char* p;
    char* q;
    char* r;

    bool locmap;

    bool allow_crossdb_featloc;
    Int4 i;
    int  numerr;

    dbp = TrackNodeType(entry, ParFlat_CONTIG);
    if (! dbp || ! dbp->mOffset)
        return true;

    i = static_cast<Int4>(dbp->len) - ParFlat_COL_DATA;
    if (i <= 0)
        return false;

    p = StringNew(i);
    StringNCpy(p, &dbp->mOffset[ParFlat_COL_DATA], i);
    p[i - 1] = '\0';
    for (q = p, r = p; *q != '\0'; q++)
        if (*q != '\n' && *q != '\t' && *q != ' ')
            *r++ = *q;
    *r = '\0';

    for (q = p; *q != '\0'; q++)
        if ((q[0] == ',' && q[1] == ',') || (q[0] == '(' && q[1] == ',') ||
            (q[0] == ',' && q[1] == ')'))
            break;
    if (*q != '\0') {
        ErrPostEx(SEV_REJECT, ERR_LOCATION_ContigHasNull, "The join() statement for this record's contig line contains one or more comma-delimited components which are null.");
        MemFree(p);
        return false;
    }

    if (pp->buf)
        MemFree(pp->buf);
    pp->buf = nullptr;

    CRef<CSeq_loc> loc = xgbparseint_ver(p, locmap, numerr, bsp.GetId(), pp->accver);
    if (loc.Empty()) {
        MemFree(p);
        return true;
    }

    allow_crossdb_featloc     = pp->allow_crossdb_featloc;
    pp->allow_crossdb_featloc = true;

    TSeqLocList locs;
    locs.push_back(loc);
    i = fta_fix_seq_loc_id(locs, pp, p, nullptr, true);

    if (i > 999)
        fta_create_far_fetch_policy_user_object(bsp, i);

    pp->allow_crossdb_featloc = allow_crossdb_featloc;

    if (loc->IsMix()) {
        XGappedSeqLocsToDeltaSeqs(loc->GetMix(), bsp.SetInst().SetExt().SetDelta().Set());
        bsp.SetInst().SetRepr(CSeq_inst::eRepr_delta);
    } else
        bsp.SetInst().ResetExt();

    MemFree(p);
    return true;
}

/**********************************************************
 *
 *   bool GetGenBankInst(pp, entry, dnaconv):
 *
 *      Fills in Seq-inst for an entry. Assumes Bioseq
 *   already allocated.
 *
 *                                              3-30-93
 *
 **********************************************************/
static bool GetGenBankInst(ParserPtr pp, const DataBlk& entry, unsigned char* dnaconv)
{
    EntryBlkPtr ebp;
    Int2        topology;
    Int2        strand;
    char*       topstr;

    char*        bptr = entry.mOffset;
    IndexblkPtr  ibp  = pp->entrylist[pp->curindx];
    LocusContPtr lcp  = &ibp->lc;

    topstr = bptr + lcp->topology;

    ebp             = static_cast<EntryBlk*>(entry.mpData);
    CBioseq& bioseq = ebp->seq_entry->SetSeq();

    CSeq_inst& inst = bioseq.SetInst();
    inst.SetRepr(ibp->is_mga ? CSeq_inst::eRepr_virtual : CSeq_inst::eRepr_raw);

    /* get linear, circular, tandem topology, blank is linear which = 1
     */
    topology = CheckTPG(topstr);
    if (topology > 1)
        inst.SetTopology(static_cast<CSeq_inst::ETopology>(topology));

    strand = CheckSTRAND((lcp->strand >= 0) ? bptr + lcp->strand : "   ");
    if (strand > 0)
        inst.SetStrand(static_cast<CSeq_inst::EStrand>(strand));

    if (GetSeqData(pp, entry, bioseq, ParFlat_ORIGIN, dnaconv, (ibp->is_prot ? eSeq_code_type_iupacaa : eSeq_code_type_iupacna)) == false)
        return false;

    if (ibp->is_contig && ! GetGenBankInstContig(entry, bioseq, pp))
        return false;

    return true;
}

/**********************************************************/
static char* GetGenBankLineage(string_view sv)
{
    char* p;
    char* str;

    if (sv.empty())
        return nullptr;

    str = StringSave(GetBlkDataReplaceNewLine(sv, ParFlat_COL_DATA));
    if (! str)
        return nullptr;

    for (p = str; *p != '\0';)
        p++;
    if (p == str) {
        MemFree(str);
        return nullptr;
    }
    for (p--;; p--) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '.' && *p != ';') {
            p++;
            break;
        }
        if (p == str)
            break;
    }
    if (p == str) {
        MemFree(str);
        return nullptr;
    }
    *p = '\0';
    return (str);
}

/**********************************************************
 *
 *   static GBBlockPtr GetGBBlock(pp, entry, mfp, biosp):
 *
 *                                              4-7-93
 *
 **********************************************************/
static CRef<CGB_block> GetGBBlock(ParserPtr pp, const DataBlk& entry, CMolInfo& mol_info, CBioSource* bio_src)
{
    LocusContPtr lcp;

    CRef<CGB_block> gbb(new CGB_block),
        ret;

    IndexblkPtr ibp;
    char*       bptr;
    char*       eptr;
    char*       ptr;
    char*       str;
    Char        msg[4];
    size_t      len;
    Int2        div;

    bool if_cds;
    bool pat_ref = false;
    bool est_kwd = false;
    bool sts_kwd = false;
    bool gss_kwd = false;
    bool htc_kwd = false;
    bool fli_kwd = false;
    bool wgs_kwd = false;
    bool tpa_kwd = false;
    bool tsa_kwd = false;
    bool tls_kwd = false;
    bool env_kwd = false;
    bool mga_kwd = false;

    bool cancelled;
    bool drop;

    char* tempdiv;
    char* p;
    Int4  i;

    ibp            = pp->entrylist[pp->curindx];
    ibp->wgssec[0] = '\0';

    bptr = xSrchNodeType(entry, ParFlat_SOURCE, &len);
    str  = StringSave(GetBlkDataReplaceNewLine(string_view(bptr, len), ParFlat_COL_DATA));
    if (str) {
        p = StringRChr(str, '.');
        if (p && p > str && p[1] == '\0' && *(p - 1) == '.')
            *p = '\0';

        gbb->SetSource(str);
        MemFree(str);
    }

    if (! ibp->keywords.empty()) {
        gbb->SetKeywords().swap(ibp->keywords);
        ibp->keywords.clear();
    } else
        GetSequenceOfKeywords(entry, ParFlat_KEYWORDS, ParFlat_COL_DATA, gbb->SetKeywords());

    if (ibp->is_mga && ! fta_check_mga_keywords(mol_info, gbb->GetKeywords())) {
        return ret;
    }

    if (ibp->is_tpa && ! fta_tpa_keywords_check(gbb->GetKeywords())) {
        return ret;
    }

    if (ibp->is_tsa && ! fta_tsa_keywords_check(gbb->GetKeywords(), pp->source)) {
        return ret;
    }

    if (ibp->is_tls && ! fta_tls_keywords_check(gbb->GetKeywords(), pp->source)) {
        return ret;
    }

    for (const string& key : gbb->GetKeywords()) {
        fta_keywords_check(key.c_str(), &est_kwd, &sts_kwd, &gss_kwd, &htc_kwd, &fli_kwd, &wgs_kwd, &tpa_kwd, &env_kwd, &mga_kwd, &tsa_kwd, &tls_kwd);
    }

    if (ibp->env_sample_qual == false && env_kwd) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_ENV_NoMatchingQualifier, "This record utilizes the ENV keyword, but there are no /environmental_sample qualifiers among its source features.");
        return ret;
    }

    bptr = xSrchNodeType(entry, ParFlat_ORIGIN, &len);
    eptr = bptr + len;
    ptr  = SrchTheChar(bptr, eptr, '\n');
    if (ptr) {
        eptr = ptr;
        bptr += 6;

        if (eptr != bptr) {
            while (isspace(*bptr) != 0)
                bptr++;
            len = eptr - bptr;
            if (len > 0) {
                gbb->SetOrigin(string(bptr, eptr));
            }
        }
    }

    lcp = &ibp->lc;

    bptr = GBDivOffset(entry, lcp->div);

    if (*bptr != ' ') {
        if_cds = check_cds(entry, pp->format);
        div    = CheckDIV(bptr);
        if (div != -1) {
            string div_str(bptr, bptr + 3);
            gbb->SetDiv(div_str);

            if (div == 16) /* "ORG" replaced by "UNA" */
                gbb->SetDiv("UNA");

            /* preserve the division code for later use
             */
            const char* p_div = gbb->GetDiv().c_str();
            StringCpy(ibp->division, p_div);

            if (ibp->psip.NotEmpty())
                pat_ref = true;

            if (ibp->is_tpa &&
                (StringEqu(p_div, "EST") || StringEqu(p_div, "GSS") ||
                 StringEqu(p_div, "PAT") || StringEqu(p_div, "HTG"))) {
                ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTPADivcode, "Division code \"%s\" is not legal for TPA records. Entry dropped.", p_div);
                return ret;
            }

            if (ibp->is_tsa && ! StringEqu(p_div, "TSA")) {
                ErrPostEx(SEV_REJECT, ERR_DIVISION_BadTSADivcode, "Division code \"%s\" is not legal for TSA records. Entry dropped.", p_div);
                return ret;
            }

            cancelled = IsCancelled(gbb->GetKeywords());

            if (StringEqu(p_div, "HTG")) {
                if (! HasHtg(gbb->GetKeywords())) {
                    ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTGKeywords, "Division is HTG, but entry lacks HTG-related keywords. Entry dropped.");
                    return ret;
                }
            }

            tempdiv = StringSave(gbb->GetDiv());

            if (fta_check_htg_kwds(gbb->SetKeywords(), pp->entrylist[pp->curindx], mol_info))
                gbb->ResetDiv();

            DefVsHTGKeywords(mol_info.GetTech(), entry, ParFlat_DEFINITION, ParFlat_ORIGIN, cancelled);

            CheckHTGDivision(tempdiv, mol_info.GetTech());
            if (tempdiv)
                MemFree(tempdiv);

            i = 0;
            if (est_kwd)
                i++;
            if (sts_kwd)
                i++;
            if (gss_kwd)
                i++;
            if (ibp->htg > 0)
                i++;
            if (htc_kwd)
                i++;
            if (fli_kwd)
                i++;
            if (wgs_kwd)
                i++;
            if (env_kwd)
                i++;
            if (mga_kwd) {
                if (ibp->is_mga == false) {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeCAGE, "This is apparently _not_ a CAGE record, but the special keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_mga) {
                ErrPostEx(SEV_ERROR, ERR_KEYWORD_NoGeneExpressionKeywords, "This is apparently a CAGE or 5'-SAGE record, but it lacks the required keywords. Entry dropped.");
            }
            if (tpa_kwd) {
                if (ibp->is_tpa == false && pp->source != Parser::ESource::EMBL) {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTPA, "This is apparently _not_ a TPA record, but the special \"TPA\" and/or \"Third Party Annotation\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_tpa) {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPA, "This is apparently a TPA record, but it lacks the required \"TPA\" and/or \"Third Party Annotation\" keywords. Entry dropped.");
                return ret;
            }
            if (tsa_kwd) {
                if (ibp->is_tsa == false) {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTSA, "This is apparently _not_ a TSA record, but the special \"TSA\" and/or \"Transcriptome Shotgun Assembly\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_tsa) {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTSA, "This is apparently a TSA record, but it lacks the required \"TSA\" and/or \"Transcriptome Shotgun Assembly\" keywords. Entry dropped.");
                return ret;
            }
            if (tls_kwd) {
                if (ibp->is_tls == false) {
                    ErrPostEx(SEV_REJECT, ERR_KEYWORD_ShouldNotBeTLS, "This is apparently _not_ a TLS record, but the special \"TLS\" and/or \"Targeted Locus Study\" keywords are present. Entry dropped.");
                    return ret;
                }
                i++;
            } else if (ibp->is_tls) {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTLS, "This is apparently a TLS record, but it lacks the required \"TLS\" and/or \"Targeted Locus Study\" keywords. Entry dropped.");
                return ret;
            }
            if (i > 1) {
                if (i == 2 && ibp->htg > 0 && env_kwd)
                    ErrPostEx(SEV_WARNING, ERR_KEYWORD_HTGPlusENV, "This HTG record also has the ENV keyword, which is an unusual combination. Confirmation that isolation and cloning steps actually occured might be appropriate.");
                else if ((i == 2 && wgs_kwd && tpa_kwd) ||
                         (i == 2 && tsa_kwd && tpa_kwd) ||
                         (i == 2 && pp->source == Parser::ESource::DDBJ &&
                          env_kwd && tpa_kwd)) {
                } else if (i != 2 || env_kwd == false ||
                           (est_kwd == false && gss_kwd == false && wgs_kwd == false)) {
                    if (i != 2 || pp->source != Parser::ESource::DDBJ ||
                        ibp->is_tsa == false || env_kwd == false) {
                        if (pp->source != Parser::ESource::DDBJ || ibp->is_wgs == false ||
                            (env_kwd == false && tpa_kwd == false)) {
                            ErrPostEx(SEV_REJECT, ERR_KEYWORD_ConflictingKeywords, "This record contains more than one of the special keywords used to indicate that a sequence is an HTG, EST, GSS, STS, HTC, WGS, ENV, FLI_CDNA, TPA, CAGE, TSA or TLS sequence.");
                            return ret;
                        }
                    }
                }
            }

            if (wgs_kwd)
                i--;

            if (ibp->is_contig && i > 0 &&
                wgs_kwd == false && tpa_kwd == false && env_kwd == false) {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_IllegalForCON, "This CON record should not have HTG, EST, GSS, STS, HTC, FLI_CDNA, CAGE, TSA or TLS special keywords. Entry dropped.");
                return ret;
            }

            CMolInfo::TTech thtg = mol_info.GetTech();
            if (thtg == CMolInfo::eTech_htgs_0 || thtg == CMolInfo::eTech_htgs_1 ||
                thtg == CMolInfo::eTech_htgs_2 || thtg == CMolInfo::eTech_htgs_3) {
                RemoveHtgPhase(gbb->SetKeywords());
            }

            bptr = xSrchNodeType(entry, ParFlat_KEYWORDS, &len);
            if (bptr) {
                string kw = GetBlkDataReplaceNewLine(string_view(bptr, len), ParFlat_COL_DATA);

                if (! est_kwd && kw.find("EST") != string::npos) {
                    ErrPostEx(SEV_WARNING, ERR_KEYWORD_ESTSubstring, "Keyword %s has substring EST, but no official EST keywords found", kw.c_str());
                }
                if (! sts_kwd && kw.find("STS") != string::npos) {
                    ErrPostEx(SEV_WARNING, ERR_KEYWORD_STSSubstring, "Keyword %s has substring STS, but no official STS keywords found", kw.c_str());
                }
            }

            if (! ibp->is_contig) {
                drop                 = false;
                CMolInfo::TTech tech = mol_info.GetTech();
                string          p_div;
                if (gbb->IsSetDiv())
                    p_div = gbb->GetDiv();

                check_div(ibp->is_pat, pat_ref, est_kwd, sts_kwd, gss_kwd, if_cds, p_div, &tech, ibp->bases, pp->source, drop);

                if (tech != CMolInfo::eTech_unknown)
                    mol_info.SetTech(tech);
                else
                    mol_info.ResetTech();

                if (! p_div.empty())
                    gbb->SetDiv(p_div);
                else
                    gbb->ResetDiv();

                if (drop) {
                    return ret;
                }
            } else if (gbb->GetDiv() == "CON") {
                gbb->ResetDiv();
            }
        } else if (pp->mode != Parser::EMode::Relaxed) {
            MemCpy(msg, bptr, 3);
            msg[3] = '\0';
            ErrPostEx(SEV_REJECT, ERR_DIVISION_UnknownDivCode, "Unknown division code \"%s\" found in GenBank flatfile. Record rejected.", msg);
            return ret;
        }

        if (IsNewAccessFormat(ibp->acnum) == 0 && *ibp->acnum == 'T' &&
            gbb->IsSetDiv() && gbb->GetDiv() != "EST") {
            ErrPostStr(SEV_INFO, ERR_DIVISION_MappedtoEST, "Leading T in accession number.");

            mol_info.SetTech(CMolInfo::eTech_est);
            gbb->ResetDiv();
        }
    }

    bool is_htc_div = gbb->IsSetDiv() && gbb->GetDiv() == "HTC",
         has_htc    = HasHtc(gbb->GetKeywords());

    if (is_htc_div && ! has_htc) {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_MissingHTCKeyword, "This record is in the HTC division, but lacks the required HTC keyword.");
        return ret;
    }

    if (! is_htc_div && has_htc) {
        ErrPostEx(SEV_ERROR, ERR_DIVISION_InvalidHTCKeyword, "This record has the special HTC keyword, but is not in HTC division. If this record has graduated out of HTC, then the keyword should be removed.");
        return ret;
    }

    if (is_htc_div) {
        bptr = entry.mOffset;
        p    = bptr + lcp->molecule;
        if (*p == 'm' || *p == 'r')
            p++;
        else if (StringEquN(p, "pre-", 4))
            p += 4;
        else if (StringEquN(p, "transcribed ", 12))
            p += 12;

        if (! StringEquN(p, "RNA", 3)) {
            ErrPostEx(SEV_ERROR, ERR_DIVISION_HTCWrongMolType, "All HTC division records should have a moltype of pre-RNA, mRNA or RNA.");
            return ret;
        }
    }

    if (fli_kwd)
        mol_info.SetTech(CMolInfo::eTech_fli_cdna);

    /* will be used in flat file database
     */
    if (gbb->IsSetDiv()) {
        if (gbb->GetDiv() == "EST") {
            ibp->EST = true;
            mol_info.SetTech(CMolInfo::eTech_est);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "STS") {
            ibp->STS = true;
            mol_info.SetTech(CMolInfo::eTech_sts);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "GSS") {
            ibp->GSS = true;
            mol_info.SetTech(CMolInfo::eTech_survey);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "HTC") {
            ibp->HTC = true;
            mol_info.SetTech(CMolInfo::eTech_htc);

            gbb->ResetDiv();
        } else if (gbb->GetDiv() == "SYN" && bio_src && bio_src->IsSetOrigin() &&
                   bio_src->GetOrigin() == CBioSource::eOrigin_synthetic) {
            gbb->ResetDiv();
        }
    } else if (mol_info.IsSetTech()) {
        if (mol_info.GetTech() == CMolInfo::eTech_est)
            ibp->EST = true;
        if (mol_info.GetTech() == CMolInfo::eTech_sts)
            ibp->STS = true;
        if (mol_info.GetTech() == CMolInfo::eTech_survey)
            ibp->GSS = true;
        if (mol_info.GetTech() == CMolInfo::eTech_htc)
            ibp->HTC = true;
    }

    if (mol_info.IsSetTech())
        fta_remove_keywords(mol_info.GetTech(), gbb->SetKeywords());

    if (ibp->is_tpa)
        fta_remove_tpa_keywords(gbb->SetKeywords());

    if (ibp->is_tsa)
        fta_remove_tsa_keywords(gbb->SetKeywords(), pp->source);

    if (ibp->is_tls)
        fta_remove_tls_keywords(gbb->SetKeywords(), pp->source);

    if (bio_src) {
        if (bio_src->IsSetSubtype()) {
            for (const auto& subtype : bio_src->GetSubtype()) {
                if (subtype->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                    fta_remove_env_keywords(gbb->SetKeywords());
                    break;
                }
            }
        }
        if (bio_src->IsSetOrg()) {
            const COrg_ref& org_ref = bio_src->GetOrg();
            if (org_ref.IsSetOrgname() && org_ref.GetOrgname().IsSetMod()) {
                for (const auto& mod : org_ref.GetOrgname().GetMod()) {
                    if (! mod->IsSetSubtype())
                        continue;

                    COrgMod::TSubtype stype = mod->GetSubtype();
                    if (stype == COrgMod::eSubtype_metagenome_source) {
                        fta_remove_mag_keywords(gbb->SetKeywords());
                        break;
                    }
                }
            }
        }
    }

    if (pp->source == Parser::ESource::DDBJ && gbb->IsSetDiv() && bio_src &&
        bio_src->IsSetOrg() && bio_src->GetOrg().IsSetOrgname() &&
        bio_src->GetOrg().GetOrgname().IsSetDiv()) {
        gbb->ResetDiv();
    } else if (gbb->IsSetDiv() &&
               bio_src &&
               bio_src->IsSetOrg() &&
               bio_src->GetOrg().IsSetOrgname() &&
               bio_src->GetOrg().GetOrgname().IsSetDiv() &&
               bio_src->GetOrg().GetOrgname().GetDiv() == gbb->GetDiv()) {
        gbb->ResetDiv();
    }

    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, gbb->SetExtra_accessions());
    ret.Reset(gbb.Release());

    return ret;
}

/**********************************************************
 *
 *   static MolInfoPtr GetGenBankMolInfo(pp, entry, orp):
 *
 *      Data from :
 *   LOCUS ... column 37, or column 53 if "EST"
 *
 **********************************************************/
static CRef<CMolInfo> GetGenBankMolInfo(ParserPtr pp, const DataBlk& entry, const COrg_ref* org_ref)
{
    IndexblkPtr ibp;
    char*       bptr;
    char*       molstr = nullptr;

    CRef<CMolInfo> mol_info(new CMolInfo);

    bptr = entry.mOffset;
    ibp  = pp->entrylist[pp->curindx];

    molstr = bptr + ibp->lc.molecule;

    bptr = GBDivOffset(entry, ibp->lc.div);

    if (StringEquN(bptr, "EST", 3))
        mol_info->SetTech(CMolInfo::eTech_est);
    else if (StringEquN(bptr, "STS", 3))
        mol_info->SetTech(CMolInfo::eTech_sts);
    else if (StringEquN(bptr, "GSS", 3))
        mol_info->SetTech(CMolInfo::eTech_survey);
    else if (StringEquN(bptr, "HTG", 3))
        mol_info->SetTech(CMolInfo::eTech_htgs_1);
    else if (ibp->is_wgs) {
        if (ibp->is_tsa)
            mol_info->SetTech(CMolInfo::eTech_tsa);
        else if (ibp->is_tls)
            mol_info->SetTech(CMolInfo::eTech_targeted);
        else
            mol_info->SetTech(CMolInfo::eTech_wgs);
    } else if (ibp->is_tsa)
        mol_info->SetTech(CMolInfo::eTech_tsa);
    else if (ibp->is_tls)
        mol_info->SetTech(CMolInfo::eTech_targeted);
    else if (ibp->is_mga) {
        mol_info->SetTech(CMolInfo::eTech_other);
        mol_info->SetTechexp("cage");
    }

    GetFlatBiomol(mol_info->SetBiomol(), mol_info->GetTech(), molstr, pp, entry, org_ref);
    if (mol_info->GetBiomol() == CMolInfo::eBiomol_unknown) // not set
        mol_info->ResetBiomol();

    return mol_info;
}

/**********************************************************/
static void FakeGenBankBioSources(const DataBlk& entry, CBioseq& bioseq)
{
    char* bptr;
    char* end;
    char* ptr;

    Char ch;

    size_t len = 0;
    bptr       = SrchNodeSubType(entry, ParFlat_SOURCE, ParFlat_ORGANISM, &len);

    if (! bptr) {
        ErrPostStr(SEV_WARNING, ERR_ORGANISM_NoOrganism, "No Organism data in genbank format file");
        return;
    }

    end  = bptr + len;
    ch   = *end;
    *end = '\0';

    CRef<CBioSource> bio_src(new CBioSource);
    bptr += ParFlat_COL_DATA;

    if (GetGenomeInfo(*bio_src, bptr) && bio_src->GetGenome() != 9) /* ! Plasmid */
    {
        while (*bptr != ' ' && *bptr != '\0')
            bptr++;
        while (*bptr == ' ')
            bptr++;
    }

    ptr = StringChr(bptr, '\n');
    if (! ptr) {
        *end = ch;
        return;
    }

    COrg_ref& org_ref = bio_src->SetOrg();

    *ptr = '\0';
    org_ref.SetTaxname(bptr);
    *ptr = '\n';

    for (;;) {
        bptr = ptr + 1;
        if (! StringEquN(bptr, "               ", ParFlat_COL_DATA))
            break;

        ptr = StringChr(bptr, '\n');
        if (! ptr)
            break;

        *ptr = '\0';
        if (StringChr(bptr, ';') || ! StringChr(ptr + 1, '\n')) {
            *ptr = '\n';
            break;
        }

        bptr += ParFlat_COL_DATA;
        string& taxname = org_ref.SetTaxname();
        taxname += ' ';
        taxname += bptr;

        *ptr = '\n';
    }

    *end = ch;

    if (org_ref.GetTaxname() == "Unknown.") {
        string& taxname = org_ref.SetTaxname();
        taxname         = taxname.substr(0, taxname.size() - 1);
    }

    ptr = GetGenBankLineage(string_view(bptr, end - bptr));
    if (ptr) {
        org_ref.SetOrgname().SetLineage(ptr);
        MemFree(ptr);
    }

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetSource(*bio_src);
    bioseq.SetDescr().Set().push_back(descr);
}

/**********************************************************/
static void fta_get_user_field(char* line, const Char* tag, CUser_object& user_obj)
{
    char* p;
    char* q;
    char* res;
    Char  ch;

    p = StringStr(line, "USER        ");
    if (! p)
        ch = '\0';
    else {
        ch = 'U';
        *p = '\0';
    }

    res = StringSave(line);
    if (ch == 'U')
        *p = 'U';

    for (q = res, p = res; *p != '\0'; p++)
        if (*p != ' ')
            *q++ = *p;
    *q = '\0';

    CRef<CUser_field> root_field(new CUser_field);
    root_field->SetLabel().SetStr(tag);

    for (q = res;;) {
        q = StringStr(q, "\nACCESSION=");
        if (! q)
            break;

        q += 11;
        for (p = q; *p != '\0' && *p != '\n' && *p != ';';)
            p++;
        ch = *p;
        *p = '\0';

        CRef<CUser_field> cur_field(new CUser_field);
        cur_field->SetLabel().SetStr("accession");
        cur_field->SetString(q);

        *p = ch;

        CRef<CUser_field> field_set(new CUser_field);
        field_set->SetData().SetFields().push_back(cur_field);

        if (StringEquN(p, ";gi=", 4)) {
            p += 4;
            for (q = p; *p >= '0' && *p <= '9';)
                p++;
            ch = *p;
            *p = '\0';

            cur_field.Reset(new CUser_field);
            cur_field->SetLabel().SetStr("gi");
            cur_field->SetNum(atoi(q));
            field_set->SetData().SetFields().push_back(cur_field);

            *p = ch;
        }

        root_field->SetData().SetFields().push_back(cur_field);
    }

    MemFree(res);

    if (! root_field->IsSetData())
        return;

    user_obj.SetData().push_back(root_field);
}

/**********************************************************/
static void fta_get_str_user_field(char* line, const Char* tag, CUser_object& user_obj)
{
    char* p;
    char* q;
    char* r;
    char* res;
    Char  ch;

    p = StringStr(line, "USER        ");
    if (! p)
        ch = '\0';
    else {
        ch = 'U';
        *p = '\0';
    }

    res = StringNew(StringLen(line));
    for (q = line; *q == ' ' || *q == '\n';)
        q++;
    for (r = res; *q != '\0';) {
        if (*q != '\n') {
            *r++ = *q++;
            continue;
        }
        while (*q == ' ' || *q == '\n')
            q++;
        if (*q != '\0')
            *r++ = ' ';
    }
    *r = '\0';
    if (ch == 'U')
        *p = 'U';

    if (*res == '\0') {
        MemFree(res);
        return;
    }

    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(tag);
    field->SetString(res);

    MemFree(res);

    user_obj.SetData().push_back(field);
}

/**********************************************************/
static void fta_get_user_object(CSeq_entry& seq_entry, const DataBlk& entry)
{
    char*  p;
    char*  q;
    char*  r;
    size_t l;

    p = xSrchNodeType(entry, ParFlat_USER, &l);
    if (l < ParFlat_COL_DATA)
        return;

    q = StringSave(string_view(p, l - 1));

    CRef<CUser_object> user_obj(new CUser_object);
    user_obj->SetType().SetStr("RefGeneTracking");

    for (p = q;;) {
        p = StringStr(p, "USER        ");
        if (! p)
            break;
        for (p += 12; *p == ' ';)
            p++;
        for (r = p; *p != '\0' && *p != '\n' && *p != ' ';)
            p++;
        if (*p == '\0' || p == r)
            break;
        if (StringEquN(r, "Related", 7))
            fta_get_user_field(p, "Related", *user_obj);
        else if (StringEquN(r, "Assembly", 8))
            fta_get_user_field(p, "Assembly", *user_obj);
        else if (StringEquN(r, "Comment", 7))
            fta_get_str_user_field(p, "Comment", *user_obj);
        else
            continue;
    }

    MemFree(q);

    if (! user_obj->IsSetData())
        return;

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser(*user_obj);

    if (seq_entry.IsSeq())
        seq_entry.SetSeq().SetDescr().Set().push_back(descr);
    else
        seq_entry.SetSet().SetDescr().Set().push_back(descr);
}

/**********************************************************/
static void fta_get_mga_user_object(TSeqdescList& descrs, char* offset, size_t len)
{
    char* str;
    char* p;

    if (! offset)
        return;

    str = StringSave(offset + ParFlat_COL_DATA);
    p   = StringChr(str, '\n');
    if (p)
        *p = '\0';
    p = StringChr(str, '-');
    if (p)
        *p++ = '\0';

    CRef<CUser_object> user_obj(new CUser_object);

    CObject_id& id = user_obj->SetType();
    id.SetStr("CAGE-Tag-List");

    CRef<CUser_field> field(new CUser_field);

    field->SetLabel().SetStr("CAGE_tag_total");
    field->SetData().SetInt(static_cast<CUser_field::C_Data::TInt>(len));
    user_obj->SetData().push_back(field);

    field.Reset(new CUser_field);

    field->SetLabel().SetStr("CAGE_accession_first");
    field->SetData().SetStr(str);
    user_obj->SetData().push_back(field);

    field.Reset(new CUser_field);

    field->SetLabel().SetStr("CAGE_accession_last");
    field->SetData().SetStr(p);
    user_obj->SetData().push_back(field);

    MemFree(str);

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser(*user_obj);

    descrs.push_back(descr);
}

/**********************************************************/
static void GetGenBankDescr(ParserPtr pp, const DataBlk& entry, CBioseq& bioseq)
{
    IndexblkPtr ibp;

    DataBlkPtr dbp;

    char* offset;
    char* str;
    char* p;
    char* q;

    bool is_htg;

    ibp = pp->entrylist[pp->curindx];

    CBioSource* bio_src = nullptr;
    COrg_ref*   org_ref = nullptr;

    /* ORGANISM
     */

    for (auto& descr : bioseq.SetDescr().Set()) {
        if (descr->IsSource()) {
            bio_src = &(descr->SetSource());
            if (bio_src->IsSetOrg())
                org_ref = &bio_src->SetOrg();
            break;
        }
    }

    /* MolInfo from LOCUS line
     */
    CRef<CMolInfo> mol_info = GetGenBankMolInfo(pp, entry, org_ref);

    /* DEFINITION data ==> descr_title
     */
    str        = nullptr;
    size_t len = 0;
    offset     = xSrchNodeType(entry, ParFlat_DEFINITION, &len);

    string title;
    if (offset) {
        str = StringSave(GetBlkDataReplaceNewLine(string_view(offset, len), ParFlat_COL_DATA));

        for (p = str; *p == ' ';)
            p++;
        if (p > str)
            fta_StringCpy(str, p);

        title = str;
        MemFree(str);
        str = nullptr;

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetTitle(title);
        bioseq.SetDescr().Set().push_back(descr);

        if (ibp->is_tpa == false && pp->source != Parser::ESource::EMBL &&
            StringEquN(title.c_str(), "TPA:", 4)) {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTPA, "This is apparently _not_ a TPA record, but the special \"TPA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }
        if (ibp->is_tsa == false && StringEquN(title.c_str(), "TSA:", 4)) {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTSA, "This is apparently _not_ a TSA record, but the special \"TSA:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }
        if (ibp->is_tls == false && StringEquN(title.c_str(), "TLS:", 4)) {
            ErrPostEx(SEV_REJECT, ERR_DEFINITION_ShouldNotBeTLS, "This is apparently _not_ a TLS record, but the special \"TLS:\" prefix is present on its definition line. Entry dropped.");
            ibp->drop = true;
            return;
        }
    }

    CRef<CUser_object> dbuop;
    offset = xSrchNodeType(entry, ParFlat_DBLINK, &len);
    if (offset)
        fta_get_dblink_user_object(bioseq.SetDescr().Set(), offset, len, pp->source, &ibp->drop, dbuop);
    else {
        offset = xSrchNodeType(entry, ParFlat_PROJECT, &len);
        if (offset)
            fta_get_project_user_object(bioseq.SetDescr().Set(), offset, Parser::EFormat::GenBank, &ibp->drop, pp->source);
    }

    if (ibp->is_mga) {
        offset = xSrchNodeType(entry, ParFlat_MGA, &len);
        fta_get_mga_user_object(bioseq.SetDescr().Set(), offset, ibp->bases);
    }
    if (ibp->is_tpa &&
        (title.empty() || (! StringEquN(title.c_str(), "TPA:", 4) &&
                           ! StringEquN(title.c_str(), "TPA_exp:", 8) &&
                           ! StringEquN(title.c_str(), "TPA_inf:", 8) &&
                           ! StringEquN(title.c_str(), "TPA_asm:", 8) &&
                           ! StringEquN(title.c_str(), "TPA_reasm:", 10)))) {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTPA, "This is apparently a TPA record, but it lacks the required \"TPA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }
    if (ibp->is_tsa && ! ibp->is_tpa &&
        (title.empty() || ! StringEquN(title.c_str(), "TSA:", 4))) {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTSA, "This is apparently a TSA record, but it lacks the required \"TSA:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }
    if (ibp->is_tls && (title.empty() || ! StringEquN(title.c_str(), "TLS:", 4))) {
        ErrPostEx(SEV_REJECT, ERR_DEFINITION_MissingTLS, "This is apparently a TLS record, but it lacks the required \"TLS:\" prefix on its definition line. Entry dropped.");
        ibp->drop = true;
        return;
    }

    /* REFERENCE
     */
    /* pub should be before GBblock because we need patent ref
     */
    dbp = TrackNodeType(entry, ParFlat_REF_END);
    for (; dbp; dbp = dbp->mpNext) {
        if (dbp->mType != ParFlat_REF_END)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, dbp, ParFlat_COL_DATA);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    dbp = TrackNodeType(entry, ParFlat_REF_NO_TARGET);
    for (; dbp; dbp = dbp->mpNext) {
        if (dbp->mType != ParFlat_REF_NO_TARGET)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, dbp, ParFlat_COL_DATA);
        if (pubdesc.NotEmpty()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetPub(*pubdesc);
            bioseq.SetDescr().Set().push_back(descr);
        }
    }

    /* GB-block
     */
    CRef<CGB_block> gbbp = GetGBBlock(pp, entry, *mol_info, bio_src);

    if ((pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL) &&
        ibp->is_contig && (! mol_info->IsSetTech() || mol_info->GetTech() == CMolInfo::eTech_unknown)) {
        CMolInfo::TTech tech = fta_check_con_for_wgs(bioseq);
        if (tech == CMolInfo::eTech_unknown)
            mol_info->ResetTech();
        else
            mol_info->SetTech(tech);
    }

    if (mol_info->IsSetBiomol() || mol_info->IsSetTech()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetMolinfo(*mol_info);
        bioseq.SetDescr().Set().push_back(descr);
    }

    if (gbbp.Empty()) {
        ibp->drop = true;
        return;
    }

    if (pp->taxserver == 1 && gbbp->IsSetDiv())
        fta_fix_orgref_div(bioseq.GetAnnot(), org_ref, *gbbp);

    if (StringEquNI(ibp->division, "CON", 3))
        fta_add_hist(pp, bioseq, gbbp->SetExtra_accessions(), Parser::ESource::DDBJ, CSeq_id::e_Ddbj, true, ibp->acnum);
    else
        fta_add_hist(pp, bioseq, gbbp->SetExtra_accessions(), Parser::ESource::DDBJ, CSeq_id::e_Ddbj, false, ibp->acnum);

    {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetGenbank(*gbbp);
        bioseq.SetDescr().Set().push_back(descr);
    }

    offset = xSrchNodeType(entry, ParFlat_PRIMARY, &len);
    if (! offset && ibp->is_tpa && ibp->is_wgs == false) {
        if (ibp->inferential || ibp->experimental) {
            if (! fta_dblink_has_sra(dbuop)) {
                ErrPostEx(SEV_REJECT, ERR_TPA_TpaSpansMissing, "TPA:%s record lacks both AH/PRIMARY linetype and Sequence Read Archive links. Entry dropped.", (ibp->inferential == false) ? "experimental" : "inferential");
                ibp->drop = true;
                return;
            }
        } else if (ibp->specialist_db == false) {
            ErrPostEx(SEV_REJECT, ERR_TPA_TpaSpansMissing, "TPA record lacks required AH/PRIMARY linetype. Entry dropped.");
            ibp->drop = true;
            return;
        }
    }

    if (offset && len > 0 &&
        fta_parse_tpa_tsa_block(bioseq, offset, ibp->acnum, ibp->vernum, len, ParFlat_COL_DATA, ibp->is_tpa) == false) {
        ibp->drop = true;
        return;
    }

    if (mol_info.NotEmpty() && mol_info->IsSetTech() &&
        (mol_info->GetTech() == CMolInfo::eTech_htgs_0 ||
         mol_info->GetTech() == CMolInfo::eTech_htgs_1 ||
         mol_info->GetTech() == CMolInfo::eTech_htgs_2))
        is_htg = true;
    else
        is_htg = false;

    /* COMMENT data
     */
    offset = xSrchNodeType(entry, ParFlat_COMMENT, &len);
    if (offset && len > 0) {
        str = GetDescrComment(offset, len, ParFlat_COL_DATA, (pp->xml_comp ? false : is_htg), ibp->is_pat);
        if (str) {
            bool           bad = false;
            TUserObjVector user_objs;

            fta_parse_structured_comment(str, bad, user_objs);
            if (bad) {
                ibp->drop = true;
                MemFree(str);
                return;
            }

            for (auto& user_obj : user_objs) {
                CRef<CSeqdesc> descr(new CSeqdesc);
                descr->SetUser(*user_obj);
                bioseq.SetDescr().Set().push_back(descr);
            }

            if (pp->xml_comp) {
                for (q = str, p = q; *p != '\0';) {
                    if (*p == ';' && (p[1] == ' ' || p[1] == '~'))
                        *p = ' ';
                    if (*p == '~' || *p == ' ') {
                        *q++ = ' ';
                        for (p++; *p == ' ' || *p == '~';)
                            p++;
                    } else
                        *q++ = *p++;
                }
                *q = '\0';
            }

            if (str[0] != 0) {
                CRef<CSeqdesc> descr(new CSeqdesc);
                descr->SetComment(str);
                bioseq.SetDescr().Set().push_back(descr);
            }
            MemFree(str);
        }
    }

    /* DATE
     */
    if (pp->no_date) /* -N in command line means no date */
        return;

    CRef<CDate> date;
    if (pp->date) /* -L in command line means replace date */
    {
        CTime time(CTime::eCurrent);
        date.Reset(new CDate);
        date->SetToTime(time);
    } else if (ibp->lc.date > 0) {
        CRef<CDate_std> std_date = GetUpdateDate(entry.mOffset + ibp->lc.date, pp->source);
        if (std_date.NotEmpty()) {
            date.Reset(new CDate);
            date->SetStd(*std_date);
        }
    }

    if (date.NotEmpty()) {
        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUpdate_date(*date);
        bioseq.SetDescr().Set().push_back(descr);
    }
}

/**********************************************************/
static void GenBankGetDivision(char* division, Int4 div, const DataBlk& entry)
{
    StringNCpy(division, GBDivOffset(entry, div), 3);
    division[3] = '\0';
}

static void xGenBankGetDivision(char* division, Int4 div, const string& locusText)
{
    StringCpy(division, locusText.substr(64, 3).c_str());
}

/**********************************************************
 *
 *   bool GenBankAscii(pp):
 *
 *      Return FALSE if allocate entry block failed.
 *
 *                                              3-17-93
 *
 **********************************************************/
bool GenBankAsciiOrig(ParserPtr pp)
{
    Int2                                       curkw;
    int                                        imax;
    int                                        segindx;
    int                                        total         = 0;
    int                                        total_long    = 0;
    int                                        total_dropped = 0;
    char*                                      ptr;
    char*                                      eptr;
    char*                                      div;
    unique_ptr<DataBlk, decltype(&xFreeEntry)> pEntry(nullptr, &xFreeEntry);
    EntryBlkPtr                                ebp;

    //    unsigned char*    dnaconv;
    //    unsigned char*    protconv;
    unsigned char* conv;

    TEntryList seq_entries;

    CSeq_loc locs;

    bool seq_long = false;

    IndexblkPtr ibp;
    IndexblkPtr tibp;

    auto dnaconv  = GetDNAConv();     /* set up sequence alphabets */
    auto protconv = GetProteinConv(); /* set up sequence alphabets */

    segindx = -1;

    imax = pp->indx;
    for (int i = 0; i < imax; i++) {
        pp->curindx = i;
        ibp         = pp->entrylist[i];

        err_install(ibp, pp->accver);

        if (ibp->segnum == 1)
            segindx = i;

        if (ibp->drop && ibp->segnum == 0) {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        pEntry.reset(LoadEntry(pp, ibp->offset, ibp->len));
        if (! pEntry) {
            FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
            // dnaconv.reset();
            // protconv.reset();
            return false;
        }

        ebp   = static_cast<EntryBlk*>(pEntry->mpData);
        ptr   = pEntry->mOffset;
        eptr  = ptr + pEntry->len;
        curkw = ParFlat_LOCUS;
        while (curkw != ParFlat_END && ptr < eptr) {
            ptr = GetGenBankBlock(&ebp->chain, ptr, &curkw, eptr);
        }

        auto ppCurrentEntry = pp->entrylist[pp->curindx];
        if (ppCurrentEntry->lc.div > -1) {
            GenBankGetDivision(ppCurrentEntry->division, ppCurrentEntry->lc.div, *pEntry);
            if (StringEqu(ibp->division, "TSA")) {
                if (ibp->tsa_allowed == false)
                    ErrPostEx(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession, "The record with accession \"%s\" is not expected to have a TSA division code.", ibp->acnum);
                ibp->is_tsa = true;
            }
        }

        CheckContigEverywhere(ibp, pp->source);
        if (ibp->drop && ibp->segnum == 0) {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        if (ptr >= eptr) {
            ibp->drop = true;
            ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry. Entry dropped.");
            if (ibp->segnum == 0) {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                total_dropped++;
                continue;
            }
        }
        GetGenBankSubBlock(*pEntry, ibp->bases);

        CRef<CBioseq> bioseq = CreateEntryBioseq(pp);
        ebp->seq_entry.Reset(new CSeq_entry);
        ebp->seq_entry->SetSeq(*bioseq);
        GetScope().AddBioseq(*bioseq);

        AddNIDSeqId(*bioseq, *pEntry, ParFlat_NCBI_GI, ParFlat_COL_DATA, pp->source);

        if (StringEquN(pEntry->mOffset + ibp->lc.bp, "aa", 2)) {
            ibp->is_prot = true;
            conv         = protconv.get();
        } else {
            ibp->is_prot = false;
            conv         = dnaconv.get();
        }


        if (! GetGenBankInst(pp, *pEntry, conv)) {
            ibp->drop = true;
            ErrPostStr(SEV_REJECT, ERR_SEQUENCE_BadData, "Bad sequence data. Entry dropped.");
            if (ibp->segnum == 0) {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                total_dropped++;
                continue;
            }
        }

        FakeGenBankBioSources(*pEntry, *bioseq);
        LoadFeat(pp, *pEntry, *bioseq);

        if (! bioseq->IsSetAnnot() && ibp->drop && ibp->segnum == 0) {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        GetGenBankDescr(pp, *pEntry, *bioseq);
        if (ibp->drop && ibp->segnum == 0) {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        fta_set_molinfo_completeness(*bioseq, ibp);

        if (ibp->is_tsa)
            fta_tsa_tls_comment_dblink_check(*bioseq, true);

        if (ibp->is_tls)
            fta_tsa_tls_comment_dblink_check(*bioseq, false);

        if (bioseq->GetInst().IsNa()) {
            if (bioseq->GetInst().GetRepr() == CSeq_inst::eRepr_raw) {
                if (ibp->gaps)
                    GapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
                else if (ibp->htg == 4 || ibp->htg == 1 || ibp->htg == 2 ||
                         (ibp->is_pat && pp->source == Parser::ESource::DDBJ))
                    SeqToDelta(*bioseq, ibp->htg);
            } else if (ibp->gaps)
                AssemblyGapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
        }

        if (no_date(pp->format, bioseq->GetDescr().Get()) && pp->debug == false &&
            pp->no_date == false &&
            pp->mode != Parser::EMode::Relaxed) {
            ibp->drop = true;
            ErrPostStr(SEV_ERROR, ERR_DATE_IllegalDate, "Illegal create date. Entry dropped.");
            if (ibp->segnum == 0) {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                total_dropped++;
                continue;
            }
        }

        if (pEntry->mpQscore.empty() && pp->accver) {
            if (pp->ff_get_qscore)
                pEntry->mpQscore = (*pp->ff_get_qscore)(ibp->acnum, ibp->vernum);
            else if (pp->ff_get_qscore_pp)
                pEntry->mpQscore = (*pp->ff_get_qscore_pp)(ibp->acnum, ibp->vernum, pp);
            if (pp->qsfd && ibp->qslength > 0)
                pEntry->mpQscore = GetQSFromFile(pp->qsfd, ibp);
        }

        if (! QscoreToSeqAnnot(pEntry->mpQscore, *bioseq, ibp->acnum, ibp->vernum, false, true)) {
            if (pp->ign_bad_qs == false) {
                ibp->drop = true;
                ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore. Entry dropped.");
                if (ibp->segnum == 0) {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                    total_dropped++;
                    continue;
                }
            } else {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse, "Error while parsing QScore.");
            }
        }

        pEntry->mpQscore.clear();

        if (ibp->psip.NotEmpty()) {
            CRef<CSeq_id> id(new CSeq_id);
            id->SetPatent(*ibp->psip);
            bioseq->SetId().push_back(id);
            ibp->psip.Reset();
        }

        /* add PatentSeqId if patent is found in reference
         */
        if (pp->mode != Parser::EMode::Relaxed &&
            pp->debug == false &&
            ibp->wgs_and_gi != 3 &&
            no_reference(*bioseq)) {
            if (pp->source == Parser::ESource::Flybase) {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references, "No references for entry from FlyBase. Continue anyway.");
            } else if (pp->source == Parser::ESource::Refseq &&
                       StringEquN(ibp->acnum, "NW_", 3)) {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references, "No references for RefSeq's NW_ entry. Continue anyway.");
            } else if (ibp->is_wgs) {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references, "No references for WGS entry. Continue anyway.");
            } else {
                ibp->drop = true;
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references, "No references. Entry dropped.");
                if (ibp->segnum == 0) {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                    total_dropped++;
                    continue;
                }
            }
        }

        if (ibp->segnum == ibp->segtotal) {
            seq_entries.push_back(ebp->seq_entry);
            ebp->seq_entry.Reset();

            if (ibp->segnum < 2) {
                if (ibp->segnum != 0) {
                    ErrPostEx(SEV_WARNING, ERR_SEGMENT_OnlyOneMember, "Segmented set contains only one member.");
                }
                segindx = i;
            } else {
                GetSeqExt(pp, locs);
                // LCOV_EXCL_START
                // Excluded per Mark's request on 12/14/2016
                BuildBioSegHeader(pp, seq_entries, locs);
                // LCOV_EXCL_STOP
            }

            /* reject the whole set if any one entry was rejected
             */
            if (ibp->segnum != 0) {
                div   = pp->entrylist[segindx]->division;
                int j = segindx;
                for (; j <= i; j++) {
                    tibp = pp->entrylist[j];
                    err_install(tibp, pp->accver);
                    if (! StringEqu(div, tibp->division)) {
                        ErrPostEx(SEV_WARNING, ERR_DIVISION_Mismatch, "Division different in segmented set: %s: %s", div, tibp->division);
                    }
                    if (tibp->drop) {
                        ErrPostEx(SEV_WARNING, ERR_SEGMENT_Rejected, "Reject the whole segmented set");
                        break;
                    }
                }
                if (j <= i) {
                    for (j = segindx; j <= i; j++) {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", tibp->locusname, tibp->acnum);
                        total_dropped++;
                    }

                    seq_entries.clear();
                    continue;
                }
            }

            DealWithGenes(seq_entries, pp);

            if (seq_entries.empty()) {
                if (ibp->segnum != 0) {
                    ErrPostEx(SEV_WARNING, ERR_SEGMENT_Rejected, "Reject the whole segmented set.");
                    int j = segindx;
                    for (; j <= i; j++) {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", tibp->locusname, tibp->acnum);
                        total_dropped++;
                    }
                } else {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                    total_dropped++;
                }
                continue;
            }

            if (pp->source == Parser::ESource::Flybase && ! seq_entries.empty())
                fta_get_user_object(*(*seq_entries.begin()), *pEntry);

            /* remove out all the features if their seqloc has
             * "join" or "order" among other segments, to the annot
             * which in class = parts
             */
            if (ibp->segnum != 0)
                // LCOV_EXCL_START
                // Excluded per Mark's request on 12/14/2016
                CheckFeatSeqLoc(seq_entries);
            // LCOV_EXCL_STOP

            fta_find_pub_explore(pp, seq_entries);

            /* change qual "citation" on features to SeqFeat.cit
             * find citation in the list by serial_number.
             * If serial number not found remove /citation
             */
            ProcessCitations(seq_entries);

            /* check for long sequences in each segment */
            if (pp->limit != 0) {
                if (ibp->segnum != 0) {
                    int j = segindx;
                    for (; j <= i; j++) {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        if (tibp->bases <= (size_t)pp->limit)
                            continue;

                        if (tibp->htg == 1 || tibp->htg == 2 || tibp->htg == 4) {
                            ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence, "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem", tibp->locusname, tibp->acnum, pp->limit);
                        } else {
                            seq_long = true;
                            ErrPostEx(SEV_REJECT, ERR_ENTRY_LongSequence, "Sequence %s|%s is longer than limit %ld", tibp->locusname, tibp->acnum, pp->limit);
                        }
                    }
                } else if (ibp->bases > (size_t)pp->limit) {
                    if (ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 4) {
                        ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence, "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem", ibp->locusname, ibp->acnum, pp->limit);
                    } else {
                        seq_long = true;
                        ErrPostEx(SEV_REJECT, ERR_ENTRY_LongSequence, "Sequence %s|%s is longer than limit %ld", ibp->locusname, ibp->acnum, pp->limit);
                    }
                }
            }
            if (pp->mode == Parser::EMode::Relaxed) {
                for (auto pEntry : seq_entries) {
                    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
                    g_InstantiateMissingProteins(pScope->AddTopLevelSeqEntry(*pEntry));
                }
            }
            if (pp->convert) {
                if (pp->cleanup <= 1) {
                    FinalCleanup(seq_entries);

                    if (pp->qamode && ! seq_entries.empty())
                        fta_remove_cleanup_user_object(*seq_entries.front());
                }

                MaybeCutGbblockSource(seq_entries);
            }

            EntryCheckDivCode(seq_entries, pp);

            if (pp->xml_comp)
                fta_set_strandedness(seq_entries);

            if (fta_EntryCheckGBBlock(seq_entries)) {
                ErrPostStr(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty, "Attention: GBBlock is not empty");
            }

            /* check for identical features
             */
            if (pp->qamode) {
                fta_sort_descr(seq_entries);
                fta_sort_seqfeat_cit(seq_entries);
            }

            if (pp->citat) {
                StripSerialNumbers(seq_entries);
            }

            PackEntries(seq_entries);
            CheckDupDates(seq_entries);

            if (ibp->segnum != 0) {
                int j = segindx;
                for (; j <= i; j++)
                    err_install(pp->entrylist[j], pp->accver);
            }
            if (seq_long) {
                seq_long = false;
                if (ibp->segnum != 0)
                    total_long += (i - segindx + 1);
                else
                    total_long++;
            } else {
                pp->entries.splice(pp->entries.end(), seq_entries);

                if (ibp->segnum != 0)
                    total += (i - segindx + 1);
                else
                    total++;
            }

            if (ibp->segnum != 0) {
                for (int j = segindx; j <= i; j++) {
                    tibp = pp->entrylist[j];
                    err_install(tibp, pp->accver);
                    ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed, "OK - entry parsed successfully: \"%s|%s\".", tibp->locusname, tibp->acnum);
                }
            } else {
                ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed, "OK - entry parsed successfully: \"%s|%s\".", ibp->locusname, ibp->acnum);
            }

            seq_entries.clear();
        } else {
            GetSeqExt(pp, locs);

            seq_entries.push_back(ebp->seq_entry);
            ebp->seq_entry.Reset();
        }

    } /* for, ascii block entries */

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingComplete, "COMPLETED : SUCCEEDED = %d (including: LONG ones = %d); SKIPPED = %d.", total, total_long, total_dropped);
    // MemFree(dnaconv);
    // MemFree(protconv);

    return true;
}
bool GenBankAscii(ParserPtr pp)
{
    int               imax;
    int               total         = 0;
    int               total_long    = 0;
    int               total_dropped = 0;
    unique_ptr<Entry> pEntry;
    unsigned char*    conv;

    TEntryList seq_entries;

    CSeq_loc locs;

    IndexblkPtr ibp;

    auto dnaconv  = GetDNAConv();     /* set up sequence alphabets */
    auto protconv = GetProteinConv(); /* set up sequence alphabets */

    imax = pp->indx;
    for (int i = 0; i < imax; i++) {
        pp->curindx = i;
        ibp         = pp->entrylist[i];

        err_install(ibp, pp->accver);

        if (ibp->drop && ibp->segnum == 0) {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        pEntry.reset(LoadEntryGenbank(pp, ibp->offset, ibp->len));
        if (! pEntry) {
            FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
            return false;
        }

        xGetGenBankBlocks(*pEntry);

        if (pp->entrylist[pp->curindx]->lc.div > -1) {
            xGenBankGetDivision(pp->entrylist[pp->curindx]->division, pp->entrylist[pp->curindx]->lc.div, pEntry->mBaseData);
            if (StringEqu(ibp->division, "TSA")) {
                if (ibp->tsa_allowed == false)
                    ErrPostEx(SEV_WARNING, ERR_TSA_UnexpectedPrimaryAccession, "The record with accession \"%s\" is not expected to have a TSA division code.", ibp->acnum);
                ibp->is_tsa = true;
            }
        }

        CheckContigEverywhere(ibp, pp->source);
        if (ibp->drop && ibp->segnum == 0) {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        auto lastType = pEntry->mSections.back()->mType;
        if (lastType != ParFlat_END) {
            ibp->drop = true;
            ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry. Entry dropped.");
            if (ibp->segnum == 0) {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                total_dropped++;
                continue;
            }
        }
        xGetGenBankSubBlocks(*pEntry, ibp->bases);

        CRef<CBioseq> pBioseq = CreateEntryBioseq(pp);
        pEntry->mSeqEntry.Reset(new CSeq_entry);
        pEntry->mSeqEntry->SetSeq(*pBioseq);
        GetScope().AddBioseq(*pBioseq);
        pEntry->xInitNidSeqId(*pBioseq, ParFlat_NCBI_GI, ParFlat_COL_DATA, pp->source);

        if (pEntry->IsAA()) {
            ibp->is_prot = true;
            conv         = protconv.get();
        } else {
            ibp->is_prot = false;
            conv         = dnaconv.get();
        }

        if (! pEntry->xInitSeqInst(conv)) {
            ibp->drop = true;
            ErrPostStr(SEV_REJECT, ERR_SEQUENCE_BadData, "Bad sequence data. Entry dropped.");
            if (ibp->segnum == 0) {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
                total_dropped++;
                continue;
            }
        }
        return false;

        /*FakeGenBankBioSources(*pEntry, *bioseq);
        LoadFeat(pp, *pEntry, *bioseq);

        if (! bioseq->IsSetAnnot() && ibp->drop && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        GetGenBankDescr(pp, *pEntry, *bioseq);
        if (ibp->drop && ibp->segnum == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".", ibp->locusname, ibp->acnum);
            total_dropped++;
            continue;
        }

        fta_set_molinfo_completeness(*bioseq, ibp);

        if (ibp->is_tsa)
            fta_tsa_tls_comment_dblink_check(*bioseq, true);

        if (ibp->is_tls)
            fta_tsa_tls_comment_dblink_check(*bioseq, false);

        if (bioseq->GetInst().IsNa())
        {
            if (bioseq->GetInst().GetRepr() == CSeq_inst::eRepr_raw)
            {
                if (ibp->gaps)
                    GapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
                else if(ibp->htg == 4 || ibp->htg == 1 || ibp->htg == 2 ||
                        (ibp->is_pat && pp->source == Parser::ESource::DDBJ))
                        SeqToDelta(*bioseq, ibp->htg);
            }
            else if (ibp->gaps)
                AssemblyGapsToDelta(*bioseq, ibp->gaps, &ibp->drop);
        }

        if (no_date(pp->format, bioseq->GetDescr().Get()) && pp->debug == false &&
           pp->no_date == false &&
           pp->mode != Parser::EMode::Relaxed)
        {
            ibp->drop = true;
            ErrPostStr(SEV_ERROR, ERR_DATE_IllegalDate,
                       "Illegal create date. Entry dropped.");
            if(ibp->segnum == 0)
            {
                ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                          "Entry skipped: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
                total_dropped++;
                continue;
            }
        }

        if (! pEntry->mpQscore && pp->accver)
        {
            if (pp->ff_get_qscore)
                pEntry->mpQscore = (*pp->ff_get_qscore)(ibp->acnum, ibp->vernum);
            else if (pp->ff_get_qscore_pp)
                pEntry->mpQscore = (*pp->ff_get_qscore_pp)(ibp->acnum, ibp->vernum, pp);
            if (pp->qsfd && ibp->qslength > 0)
                pEntry->mpQscore = GetQSFromFile(pp->qsfd, ibp);
        }

        if (!QscoreToSeqAnnot(pEntry->mpQscore, *bioseq, ibp->acnum, ibp->vernum, false, true))
        {
            if(pp->ign_bad_qs == false)
            {
                ibp->drop = true;
                ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse,
                          "Error while parsing QScore. Entry dropped.");
                if(ibp->segnum == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                    total_dropped++;
                    continue;
                }
            }
            else
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_FailedToParse,
                          "Error while parsing QScore.");
            }
        }

        if (pEntry->mpQscore)
        {
            MemFree(pEntry->mpQscore);
            pEntry->mpQscore = nullptr;
        }

        if (ibp->psip.NotEmpty())
        {
            CRef<CSeq_id> id(new CSeq_id);
            id->SetPatent(*ibp->psip);
            bioseq->SetId().push_back(id);
            ibp->psip.Reset();
        }

        // add PatentSeqId if patent is found in reference
        //
        if(pp->mode != Parser::EMode::Relaxed &&
           pp->debug == false &&
           ibp->wgs_and_gi != 3 &&
           no_reference(*bioseq))
        {
            if(pp->source == Parser::ESource::Flybase)
            {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references for entry from FlyBase. Continue anyway.");
            }
            else if(pp->source == Parser::ESource::Refseq &&
                    StringEquN(ibp->acnum, "NW_", 3))
            {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references for RefSeq's NW_ entry. Continue anyway.");
            }
            else if(ibp->is_wgs)
            {
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references for WGS entry. Continue anyway.");
            }
            else
            {
                ibp->drop = true;
                ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                           "No references. Entry dropped.");
                if(ibp->segnum == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                    total_dropped++;
                    continue;
                }
            }
        }

        if (ibp->segnum == ibp->segtotal)
        {
            seq_entries.push_back(ebp->seq_entry);
            ebp->seq_entry.Reset();

            if (ibp->segnum < 2)
            {
                if(ibp->segnum != 0)
                {
                    ErrPostEx(SEV_WARNING, ERR_SEGMENT_OnlyOneMember,
                              "Segmented set contains only one member.");
                }
                segindx = i;
            }
            else
            {
                GetSeqExt(pp, locs);
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
                BuildBioSegHeader(pp, seq_entries, locs);
// LCOV_EXCL_STOP
            }

            // reject the whole set if any one entry was rejected
            //
            if(ibp->segnum != 0)
            {
                div = pp->entrylist[segindx]->division;
                int j = segindx;
                for(; j <= i; j++)
                {
                    tibp = pp->entrylist[j];
                    err_install(tibp, pp->accver);
                    if (! StringEqu(div, tibp->division))
                    {
                        ErrPostEx(SEV_WARNING, ERR_DIVISION_Mismatch,
                                  "Division different in segmented set: %s: %s",
                                  div, tibp->division);
                    }
                    if (tibp->drop)
                    {
                        ErrPostEx(SEV_WARNING, ERR_SEGMENT_Rejected,
                                  "Reject the whole segmented set");
                        break;
                    }
                }
                if(j <= i)
                {
                    for(j = segindx; j <= i; j++)
                    {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                                  "Entry skipped: \"%s|%s\".",
                                  tibp->locusname, tibp->acnum);
                        total_dropped++;
                    }

                    seq_entries.clear();
                    continue;
                }
            }

            DealWithGenes(seq_entries, pp);

            if (seq_entries.empty())
            {
                if(ibp->segnum != 0)
                {
                    ErrPostEx(SEV_WARNING, ERR_SEGMENT_Rejected,
                              "Reject the whole segmented set.");
                    int j = segindx;
                    for(; j <= i; j++)
                    {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                                  "Entry skipped: \"%s|%s\".",
                                  tibp->locusname, tibp->acnum);
                        total_dropped++;
                    }
                }
                else
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                              "Entry skipped: \"%s|%s\".",
                              ibp->locusname, ibp->acnum);
                    total_dropped++;
                }
                continue;
            }

            if (pp->source == Parser::ESource::Flybase && !seq_entries.empty())
                fta_get_user_object(*(*seq_entries.begin()), *pEntry);

            // remove out all the features if their seqloc has
            // "join" or "order" among other segments, to the annot
            // which in class = parts
            //
            if(ibp->segnum != 0)
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
                CheckFeatSeqLoc(seq_entries);
// LCOV_EXCL_STOP

            fta_find_pub_explore(pp, seq_entries);

            // change qual "citation" on features to SeqFeat.cit
            // find citation in the list by serial_number.
            // If serial number not found remove /citation
            //
            ProcessCitations(seq_entries);

            // check for long sequences in each segment
            //
            if(pp->limit != 0)
            {
                if(ibp->segnum != 0)
                {
                    int j = segindx;
                    for(; j <= i; j++)
                    {
                        tibp = pp->entrylist[j];
                        err_install(tibp, pp->accver);
                        if(tibp->bases <= (size_t) pp->limit)
                            continue;

                        if(tibp->htg == 1 || tibp->htg == 2 || tibp->htg == 4)
                        {
                            ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence,
                                      "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem",
                                      tibp->locusname, tibp->acnum, pp->limit);
                        }
                        else
                        {
                            seq_long = true;
                            ErrPostEx(SEV_REJECT, ERR_ENTRY_LongSequence,
                                      "Sequence %s|%s is longer than limit %ld",
                                      tibp->locusname, tibp->acnum, pp->limit);
                        }
                    }
                }
                else if(ibp->bases > (size_t) pp->limit)
                {
                    if(ibp->htg == 1 || ibp->htg == 2 || ibp->htg == 4)
                    {
                        ErrPostEx(SEV_WARNING, ERR_ENTRY_LongHTGSSequence,
                                  "HTGS Phase 0/1/2 sequence %s|%s exceeds length limit %ld: entry has been processed regardless of this problem",
                                  ibp->locusname, ibp->acnum, pp->limit);
                    }
                    else
                    {
                        seq_long = true;
                        ErrPostEx(SEV_REJECT, ERR_ENTRY_LongSequence,
                                  "Sequence %s|%s is longer than limit %ld",
                                  ibp->locusname, ibp->acnum, pp->limit);
                    }
                }
            }
            if (pp->mode == Parser::EMode::Relaxed) {
                for(auto pEntry : seq_entries) {
                    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
                    g_InstantiateMissingProteins(pScope->AddTopLevelSeqEntry(*pEntry));
                }
            }
            if (pp->convert)
            {
                if(pp->cleanup <= 1)
                {
                    FinalCleanup(seq_entries);

                    if (pp->qamode && !seq_entries.empty())
                        fta_remove_cleanup_user_object(*seq_entries.front());
                }

                MaybeCutGbblockSource(seq_entries);
            }

            EntryCheckDivCode(seq_entries, pp);

            if(pp->xml_comp)
                fta_set_strandedness(seq_entries);

            if (fta_EntryCheckGBBlock(seq_entries))
            {
                ErrPostStr(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty,
                           "Attention: GBBlock is not empty");
            }

            // check for identical features
            //
            if(pp->qamode)
            {
                fta_sort_descr(seq_entries);
                fta_sort_seqfeat_cit(seq_entries);
            }

            if (pp->citat)
            {
                StripSerialNumbers(seq_entries);
            }

            PackEntries(seq_entries);
            CheckDupDates(seq_entries);

            if(ibp->segnum != 0) {
                int j = segindx;
                for(; j <= i; j++)
                    err_install(pp->entrylist[j], pp->accver);
            }
            if (seq_long)
            {
                seq_long = false;
                if(ibp->segnum != 0)
                    total_long += (i - segindx + 1);
                else
                    total_long++;
            }
            else
            {
                pp->entries.splice(pp->entries.end(), seq_entries);

                if(ibp->segnum != 0)
                    total += (i - segindx + 1);
                else
                    total++;
            }

            if(ibp->segnum != 0)
            {
                for(int j = segindx; j <= i; j++)
                {
                    tibp = pp->entrylist[j];
                    err_install(tibp, pp->accver);
                    ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                              "OK - entry parsed successfully: \"%s|%s\".",
                              tibp->locusname, tibp->acnum);
                }
            }
            else
            {
                ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                          "OK - entry parsed successfully: \"%s|%s\".",
                          ibp->locusname, ibp->acnum);
            }

            seq_entries.clear();
        }
        else
        {
            GetSeqExt(pp, locs);

            seq_entries.push_back(ebp->seq_entry);
            ebp->seq_entry.Reset();
        }

    */} // for, ascii block entries

        FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

        ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingComplete, "COMPLETED : SUCCEEDED = %d (including: LONG ones = %d); SKIPPED = %d.", total, total_long, total_dropped);
        // MemFree(dnaconv);
        // MemFree(protconv);

        return false;
}

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
/**********************************************************
 *
 *   static void SrchFeatSeqLoc(sslbp, sfp):
 *
 *                                              9-14-93
 *
 **********************************************************/
static void SrchFeatSeqLoc(TSeqFeatList& feats, CSeq_annot::C_Data::TFtable& feat_table)
{
    for (CSeq_annot::C_Data::TFtable::iterator feat = feat_table.begin(); feat != feat_table.end();) {
        if ((*feat)->IsSetLocation() && (*feat)->GetLocation().GetId()) {
            ++feat;
            continue;
        }

        /* SeqLocId will return NULL if any one of seqid in the SeqLocPtr
         * is diffenent, so move out cursfp to sslbp
         */

        feats.push_back(*feat);
        feat = feat_table.erase(feat);
    }
}

/**********************************************************
 *
 *   static void FindFeatSeqLoc(sep, data, index, indent):
 *
 *                                              9-14-93
 *
 **********************************************************/
static void FindFeatSeqLoc(TEntryList& seq_entries, TSeqFeatList& feats)
{
    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            const CSeq_id& first_id = *(*bioseq->GetId().begin());
            if (IsSegBioseq(first_id) || ! bioseq->IsSetAnnot())
                continue;

            /* process this bioseq entry */
            CBioseq::TAnnot annots = bioseq->SetAnnot();
            for (CBioseq::TAnnot::iterator annot = annots.begin(); annot != annots.end();) {
                if (! (*annot)->IsSetData() || ! (*annot)->GetData().IsFtable()) {
                    ++annot;
                    continue;
                }

                CSeq_annot::C_Data::TFtable& feat_table = (*annot)->SetData().SetFtable();
                SrchFeatSeqLoc(feats, feat_table);

                if (! feat_table.empty()) {
                    ++annot;
                    continue;
                }

                annot = annots.erase(annot);
            }
        }
    }
}

/**********************************************************/
static CBioseq_set* GetParts(TEntryList& seq_entries)
{
    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq_set> bio_set(Begin(*entry)); bio_set; ++bio_set) {
            if (bio_set->IsSetClass() && bio_set->GetClass() == CBioseq_set::eClass_parts)
                return bio_set.operator->();
        }
    }

    return nullptr;
}

/**********************************************************
 *
 *   void CheckFeatSeqLoc(sep):
 *
 *      Remove out all the features which its seqloc has
 *   "join" or "order" among other segments, then insert
 *   into the annot which in the level of the class = parts.
 *
 *                                              9-14-93
 *
 **********************************************************/
void CheckFeatSeqLoc(TEntryList& seq_entries)
{
    TSeqFeatList feats_no_id;
    FindFeatSeqLoc(seq_entries, feats_no_id);

    CBioseq_set* parts = GetParts(seq_entries);

    if (! feats_no_id.empty() && parts) /* may need to delete duplicate
                                                           one   9-14-93 */
    {
        for (auto& annot : parts->SetAnnot()) {
            if (! annot->IsFtable())
                continue;

            annot->SetData().SetFtable().splice(annot->SetData().SetFtable().end(), feats_no_id);
            break;
        }

        if (parts->GetAnnot().empty()) {
            CRef<CSeq_annot> new_annot(new CSeq_annot);
            new_annot->SetData().SetFtable().swap(feats_no_id);
            parts->SetAnnot().push_back(new_annot);
        }
    }
}

END_NCBI_SCOPE
// LCOV_EXCL_STOP
