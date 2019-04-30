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
 * Authors:  Josh Cherry
 *
 * File Description:  C++ wrappers for alignment file reading
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/aln_reader.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/error_codes.hpp>
#include <util/format_guess.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_errors.hpp"
#include "seqid_validate.hpp"

#include <cassert>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_Align

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

string sAlnErrorToString(const CAlnError & error)
{
    auto lineNumber = error.GetLineNum();
    if (lineNumber == -1) {
        return FORMAT(
            "At ID '" << error.GetID() << "' "
            "in category '" << static_cast<int>(error.GetCategory()) << "': "
            << error.GetMsg() << "'");
    }
    return FORMAT(
        "At ID '" << error.GetID() << "' "
        "in category '" << static_cast<int>(error.GetCategory()) << "' "
        "at line " << error.GetLineNum() << ": "
        << error.GetMsg() << "'");
}

CAlnError::CAlnError(int category, int line_num, string id, string message)
{
    switch (category) 
    {
    case -1:
        m_Category = eAlnErr_Unknown;
        break;
    case 0:
        m_Category = eAlnErr_NoError;
        break;
    case 1:
        m_Category = eAlnErr_Fatal;
        break;
    case 2:
        m_Category = eAlnErr_BadData;
        break;
    case 3:
        m_Category = eAlnErr_BadFormat;
        break;
    case 4:
        m_Category = eAlnErr_BadChar;
        break;
    default:
        m_Category = eAlnErr_Unknown;
        break;
    }

    m_LineNum = line_num;
    m_ID = id;
    m_Message = message;
}


CAlnError::CAlnError(const CAlnError& e)
{
    m_Category = e.GetCategory();
    m_LineNum = e.GetLineNum();
    m_ID = e.GetID();
    m_Message = e.GetMsg();
}


CAlnReader::CAlnReader(CNcbiIstream& is, FIdValidate fIdValidate) : 
    m_fIdValidate(fIdValidate),
    m_IS(is), m_ReadDone(false), m_ReadSucceeded(false), 
    m_UseNexusInfo(true)
{
    m_Errors.clear();
    SetAlphabet(eAlpha_Protein);
    SetAllGap(".-");
    if (!m_fIdValidate) {
        m_fIdValidate = CSeqIdValidate();
    }
}


string CAlnReader::GetAlphabetLetters(
    EAlphabet alphaId)
{
    static map<EAlphabet, string> alphaMap{

        {EAlphabet::eAlpha_Default,         // use file type default
            ""},

        {EAlphabet::eAlpha_Nucleotide,      // non negotiable due to existing code
            "ABCDGHKMNRSTUVWXYabcdghkmnrstuvwxy"},

        {EAlphabet::eAlpha_Protein,         // non negotiable due to existing code 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"},

        {EAlphabet::eAlpha_Dna,             // all ambiguity characters but not U
            "ABCDGHKMNRSTVWXYabcdghkmnrstvwxy"},

        {EAlphabet::eAlpha_Rna,             // all ambiguity characters but not T
            "ABCDGHKMNRSTVWXYabcdghkmnrstvwxy"},

        {EAlphabet::eAlpha_Dna_no_ambiguity, 
            "ACGTNacgtn"},                  // DNA + N for unknown

        {EAlphabet::eAlpha_Rna_no_ambiguity, 
            "ACGUNacgun"},                  // RNA + N for unknown
    };
    return alphaMap[alphaId];
};

    
void CAlnReader::SetFastaGap(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}


void CAlnReader::SetClustal(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}


void CAlnReader::SetPaup(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}


void CAlnReader::SetPhylip(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}



static void
sReportError(
    ILineErrorListener* pEC,
    EDiagSev severity,
    int code,
    int subcode,
    int lineNumber,
    const string& message,
    ILineError::EProblem problemType=ILineError::eProblem_GeneralParsingError)
{
    if (!pEC) {
        NCBI_THROW2(CObjReaderParseException, eFormat, message, 0);
    }
    AutoPtr<CLineErrorEx> pErr(
        CLineErrorEx::Create(
        problemType,
        severity,
        code,
        subcode,
        "",
        lineNumber,
        message));
    pEC->PutError(*pErr);
}


static void 
sReportError(
    ILineErrorListener* pEC,
    EDiagSev severity,
    int lineNumber,
    const string& message,
    ILineError::EProblem problemType=ILineError::eProblem_GeneralParsingError)
{
    sReportError(pEC, severity, eReader_Alignment, 0, lineNumber, message, problemType);
}

void CAlnReader::Read(
    TReadFlags readFlags,
    ncbi::objects::ILineErrorListener* pErrorListener)
{

    if (pErrorListener) {
        theErrorReporter.reset(new CAlnErrorReporter(pErrorListener));
    }
    if (m_ReadDone) {
        return;
    }
    // make a SSequenceInfo corresponding to our CSequenceInfo argument
    CSequenceInfo sequenceInfo(
        m_Alphabet, m_Match, m_Missing, m_BeginningGap, m_MiddleGap, m_EndGap);

    // read the alignment stream
    SAlignmentFile alignmentInfo;
    try {
        ReadAlignmentFile(m_IS, sequenceInfo, alignmentInfo);
        x_VerifyAlignmentInfo(alignmentInfo, readFlags);
    }
    catch (const SShowStopper& showStopper) {
        theErrorReporter->Fatal(showStopper);
        return;
    }

    m_Dim = m_IdStrings.size();
    m_ReadDone = true;
    m_ReadSucceeded = true;
}

void CAlnReader::Read(
    bool guess, 
    bool generate_local_ids,
    ncbi::objects::ILineErrorListener* pErrorListener)
{

    // make a SSequenceInfo corresponding to our CSequenceInfo argument
    CSequenceInfo sequenceInfo(
        m_Alphabet, m_Match, m_Missing, m_BeginningGap, m_MiddleGap, m_EndGap);

    // read the alignment stream
    SAlignmentFile alignmentInfo;
    try {
        ReadAlignmentFile(
            m_IS, generate_local_ids, m_UseNexusInfo, sequenceInfo, alignmentInfo);
        TReadFlags flags = 0;
        x_VerifyAlignmentInfo(alignmentInfo, flags);
    }
    catch (const SShowStopper& showStopper) {
        theErrorReporter->Fatal(showStopper);
        return;
    }
    m_Dim = m_IdStrings.size();
    m_ReadDone = true;
    m_ReadSucceeded = true;
}


void CAlnReader::x_ParseAndValidateSeqIds(const SLineInfo& seqIdInfo,
        TReadFlags flags,
        TIdList& ids)
{
    ids.clear();
    const auto& idString = seqIdInfo.mData;

    CSeq_id::TParseFlags parseFlags = CSeq_id::fParse_AnyLocal;
    if (flags^fGenerateLocalIDs) {
        parseFlags |= CSeq_id::fParse_RawText;
    }

    
    try {
        CSeq_id::ParseIDs(ids, idString, parseFlags);
    }
    catch (...) {  // report an error and interpret the id string as a local ID
        theErrorReporter->Error(
                seqIdInfo.mNumLine,
                EAlnSubcode::eAlnSubcode_IllegalSequenceId,
                "Unable to parse sequence ID string.");
        ids.push_back(Ref(new CSeq_id(CSeq_id::e_Local, idString)));
    }

    if (m_fIdValidate) {
        for (auto pSeqId : ids) {
            string description;
            EDiagSev severity;
            if (!m_fIdValidate(*pSeqId, description, severity)) {
                theErrorReporter->Report(
                    seqIdInfo.mNumLine,
                    severity,
                    EReaderCode::eReader_Alignment,
                    EAlnSubcode::eAlnSubcode_IllegalSequenceId,
                    description);
            }
        }
    }
    return;
}

void CAlnReader::x_VerifyAlignmentInfo(
    const SAlignmentFile& alignmentInfo,
    TReadFlags flags)
{

    const auto num_sequences = alignmentInfo.NumSequences();

    if (num_sequences == 0) {
        throw SShowStopper(
            -1,
            eAlnSubcode_BadSequenceCount,
            "No sequence data was detected in alignment file.");
    }


    if (num_sequences == 1) {
        throw SShowStopper(
        -1,
        eAlnSubcode_BadSequenceCount,
        "Only one sequence was detected in the alignment file. An alignment file must contain more than one sequence.");
    }

    const auto numSequences = alignmentInfo.NumSequences();

    m_Seqs.assign(alignmentInfo.mSequences.begin(), alignmentInfo.mSequences.end());


    for (auto seqIdInfo : alignmentInfo.mIds) {
        m_IdStrings.push_back(seqIdInfo.mData); // m_IdStrings is redundant and should be removed
        TIdList ids;
        x_ParseAndValidateSeqIds(seqIdInfo, flags, ids);
        m_Ids.push_back(ids);
    }

    auto numDeflines = alignmentInfo.NumDeflines();
    if (numDeflines) {
        if (numDeflines == m_Ids.size()) {
            m_DeflineInfo.resize(numDeflines);
            for (int i=0;  i< numDeflines;  ++i) {
                m_DeflineInfo[i] = {
                    NStr::TruncateSpaces(
                    alignmentInfo.mDeflines[i].mData),
                    alignmentInfo.mDeflines[i].mNumLine};
            }
        }
        else {
            string description = ErrorPrintf(
                    "Expected %d deflines but finding %d. If deflines are used, each sequence must have a corresponding defline. Note that deflines are optional.",
                     m_Ids.size(),
                     numDeflines);
            theErrorReporter->Error(
                    -1,
                    EAlnSubcode::eAlnSubcode_InsufficientDeflineInfo,
                    description);
        }
    }
}


void CAlnReader::x_CalculateMiddleSections()
{
    m_MiddleSections.clear();

    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        TSeqPos begin_len = m_Seqs[row_i].find_first_not_of(m_BeginningGap);
        TSeqPos end_len = 0;
        if (begin_len < m_Seqs[row_i].length()) {
            string::iterator s = m_Seqs[row_i].end();
            while (s != m_Seqs[row_i].begin()) {
                --s;
                if (m_EndGap.find(*s) != string::npos) {
                    end_len++;
                } else {
                    break;
                }
            }
        }
        m_MiddleSections.push_back(TAlignMiddleInterval(begin_len, m_Seqs[row_i].length() - end_len - 1));
    }
}


bool CAlnReader::x_IsGap(TNumrow row, TSeqPos pos, const string& residue)
{
    if (m_MiddleSections.size() == 0) {
        x_CalculateMiddleSections();
    }
    if (row > m_MiddleSections.size()) {
        return false;
    }
    if (pos < m_MiddleSections[row].first) {
        if (NStr::Find(m_BeginningGap, residue) == string::npos) {
            return false;
        } else {
            return true;
        }
    } else if (pos > m_MiddleSections[row].second) {
        if (NStr::Find(m_EndGap, residue) == string::npos) {
            return false;
        } else {
            return true;
        }
    } else {
        if (NStr::Find(m_MiddleGap, residue) == string::npos) {
            return false;
        } else {
            return true;
        }
    }
}

CRef<CSeq_id> CAlnReader::GenerateID(const string& fasta_defline,
    const TSeqPos& index,
    TFastaFlags fasta_flags)
{
    _ASSERT(index < m_Dim);
    _ASSERT(!m_Ids[index].empty());    

    return m_Ids[index].front();
}    


void CAlnReader::x_AssignDensegIds(const TFastaFlags fasta_flags, 
        CDense_seg& denseg) 
{
    CDense_seg::TIds& ids = denseg.SetIds();
    ids.resize(m_Dim);
    m_Ids.resize(m_Dim);

    for (auto i=0; i<m_Dim; ++i) {
        // Reconstruct original defline string from results 
        // returned by C code.
        string fasta_defline = m_IdStrings[i];
        if (i < m_DeflineInfo.size() && !m_DeflineInfo[i].mData.empty()) {
            fasta_defline += " " + m_DeflineInfo[i].mData;
        }
        ids[i] = GenerateID(fasta_defline, i, fasta_flags);
    }
    return;
}


CRef<CSeq_align> CAlnReader::GetSeqAlign(const TFastaFlags fasta_flags,
        ILineErrorListener* pErrorListener)
{
    if (m_Aln) {
        return m_Aln;
    } else if ( !m_ReadDone ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "CAlnReader::GetSeqAlign(): "
                   "Seq_align is not available until after Read()", 0);
    }

    if (!m_ReadSucceeded) {
        return CRef<CSeq_align>();
    }

    typedef CDense_seg::TNumseg TNumseg;

    m_Aln = new CSeq_align();
    m_Aln->SetType(CSeq_align::eType_not_set);
    m_Aln->SetDim(m_Dim);

    CDense_seg& ds = m_Aln->SetSegs().SetDenseg();
    ds.SetDim(m_Dim);
    
    CDense_seg::TStarts&  starts  = ds.SetStarts();
    //CDense_seg::TStrands& strands = ds.SetStrands();
    CDense_seg::TLens&    lens    = ds.SetLens();

    x_AssignDensegIds(fasta_flags, ds);

    // get the length of the alignment
    TSeqPos aln_stop = m_Seqs[0].size();
    for (TNumrow row_i = 1; row_i < m_Dim; row_i++) {
        if (m_Seqs[row_i].size() > aln_stop) {
            aln_stop = m_Seqs[row_i].size();
        }
    }


    m_SeqVec.resize(m_Dim);
    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        m_SeqVec[row_i].resize(m_Seqs[row_i].length(), 0);
    }
    m_SeqLen.resize(m_Dim, 0);
    vector<bool> is_gap;  is_gap.resize(m_Dim, true);
    vector<bool> prev_is_gap;  prev_is_gap.resize(m_Dim, true);
    vector<TSignedSeqPos> next_start; next_start.resize(m_Dim, 0);
    int starts_i = 0;
    TSeqPos prev_aln_pos = 0, prev_len = 0;
    bool new_seg = true;
    TNumseg numseg = 0;
    
    for (TSeqPos aln_pos = 0; aln_pos < aln_stop; aln_pos++) {
        for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
            if (aln_pos >= m_Seqs[row_i].length()) {
                if (!is_gap[row_i]) {
                    is_gap[row_i] = true;
                    new_seg = true;
                }
            } else {
                string residue = m_Seqs[row_i].substr(aln_pos, 1);
                NStr::ToUpper(residue);
                if (!x_IsGap(row_i, aln_pos, residue)) {

                    if (is_gap[row_i]) {
                        is_gap[row_i] = false;
                        new_seg = true;
                    }

                    // add to the sequence vector
                    m_SeqVec[row_i][m_SeqLen[row_i]++] = residue[0];

                } else {
  
                    if ( !is_gap[row_i] ) {
                        is_gap[row_i] = true;
                        new_seg = true;
                    }
                }

            }
        }

        if (new_seg) {
            if (numseg) { // if not the first seg
                lens.push_back(prev_len = aln_pos - prev_aln_pos);
                for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
                    if ( !prev_is_gap[row_i] ) {
                        next_start[row_i] += prev_len;
                    }
                }
            }

            starts.resize(starts_i + m_Dim);
            for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
                if (is_gap[row_i]) {
                    starts[starts_i++] = -1;
                } else {
                    starts[starts_i++] = next_start[row_i];;
                }
                prev_is_gap[row_i] = is_gap[row_i];
            }

            prev_aln_pos = aln_pos;

            numseg++;
            new_seg = false;
        }
    }

    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        m_SeqVec[row_i].resize(m_SeqLen[row_i]); // resize down to actual size
    }

    lens.push_back(aln_stop - prev_aln_pos);
    //strands.resize(numseg * m_Dim, eNa_strand_plus);
    _ASSERT(lens.size() == numseg);
    ds.SetNumseg(numseg);

#if _DEBUG
    m_Aln->Validate(true);
#endif    
    return m_Aln;
}


CSeq_inst::EMol
CAlnReader::GetSequenceMolType(
    const string& alphabet,
    const string& seqData,
    ILineErrorListener* pErrorListener
    )
{
    //if alphabet contains full complement (26) of protein chars then
    // it's definitely protein:
    if (alphabet.size() == 2*26) {
        return CSeq_inst::eMol_aa;
    }
    
    auto posFirstT = seqData.find_first_of("Tt");
    auto posFirstU = seqData.find_first_of("Uu");
    if (posFirstT != string::npos  &&  posFirstU != string::npos) {
        string msg = "Invalid Mol Type: "
                     "U and T cannot appear in the same nucleotide sequence. "
                     "Reinterpreting as protein.";
        sReportError(pErrorListener, 
                     eDiag_Error,
                     eReader_Alignment,
                     eAlnSubcode_InconsistentMolType,
                     0, msg);


        //imposible NA- can't contain both
        return CSeq_inst::eMol_aa;
    }
    return (posFirstU == string::npos  ?  CSeq_inst::eMol_dna : CSeq_inst::eMol_rna);
}


CRef<CSeq_inst> CAlnReader::x_GetSeqInst(
        CSeq_inst::EMol mol,
        const string& seqData) const
{
    auto pSeqInst = Ref(new CSeq_inst());
    pSeqInst->SetRepr(CSeq_inst::eRepr_raw);
    pSeqInst->SetMol(mol);
    pSeqInst->SetLength(seqData.size());
    CSeq_data& data = pSeqInst->SetSeq_data();
    if (mol == CSeq_inst::eMol_aa) {
        data.SetIupacaa().Set(seqData);
    } else {
        data.SetIupacna().Set(seqData);
        CSeqportUtil::Pack(&data);
    }
    return pSeqInst;
}


CRef<CSeq_entry> CAlnReader::GetSeqEntry(const TFastaFlags fasta_flags,
        ILineErrorListener* pErrorListener)
{
    if (m_Entry) {
        return m_Entry;
    } else if ( !m_ReadDone ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "CAlnReader::GetSeqEntry(): "
                   "Seq_entry is not available until after Read()", 0);
    }

    if (!m_ReadSucceeded) {
        return CRef<CSeq_entry>();
    }

    m_Entry = new CSeq_entry();
    CRef<CSeq_align> seq_align = GetSeqAlign(fasta_flags, pErrorListener);

    const CDense_seg& denseg = seq_align->GetSegs().GetDenseg();
    _ASSERT(denseg.GetIds().size() == m_Dim);

    CRef<CSeq_annot> seq_annot (new CSeq_annot);
    seq_annot->SetData().SetAlign().push_back(seq_align);

    m_Entry->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    m_Entry->SetSet().SetAnnot().push_back(seq_annot);

    auto& seq_set = m_Entry->SetSet().SetSeq_set();

    typedef CDense_seg::TDim TNumrow;
    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        const string& seq_str     = m_SeqVec[row_i];
        auto pSubEntry = Ref(new CSeq_entry());

        // seq-id(s)
        auto& ids = pSubEntry->SetSeq().SetId();
        //ids.push_back(denseg.GetIds()[row_i]);
        ids = m_Ids[row_i];

        // mol
        CSeq_inst::EMol mol   = CSeq_inst::eMol_not_set;
        CSeq_id::EAccessionInfo ai = ids.front()->IdentifyAccession();
        if (ai & CSeq_id::fAcc_nuc) {
            mol = CSeq_inst::eMol_na;
        } else if (ai & CSeq_id::fAcc_prot) {
            mol = CSeq_inst::eMol_aa;
        } else {
            mol = GetSequenceMolType(m_Alphabet, seq_str, pErrorListener);
        }
        // seq-inst
        auto pSeqInst = x_GetSeqInst(mol, seq_str);
        pSubEntry->SetSeq().SetInst(*pSeqInst);
        seq_set.push_back(pSubEntry);
    }

    if (!m_DeflineInfo.empty()) {
        int i=0;
        if (fasta_flags & CFastaReader::fAddMods) {
            for (auto& pSeqEntry : seq_set) {
                x_AddMods(m_DeflineInfo[i++], pSeqEntry->SetSeq(), pErrorListener);
            }
        }
        else {
            for (auto& pSeqEntry : seq_set) {
                x_AddTitle(m_DeflineInfo[i++].mData, 
                        pSeqEntry->SetSeq());
            }
        }
    }

    return m_Entry;
}


static void s_AppendMods(
    const CModHandler::TModList& mods,
    string& title
    )
{
    for (const auto& mod : mods) {
        title.append(" [" 
            + mod.GetName()
            + "=" 
            + mod.GetValue()
            + "]");
    }
}

void CAlnReader::x_AddMods(const SLineInfo& defline_info, 
        CBioseq& bioseq,
        ILineErrorListener* pErrorListener)
{
    auto defline = defline_info.mData;
    if (NStr::IsBlank(defline)) {
        return;
    }


    auto fReportError = 
        [&](const string& msg, EDiagSev sev, EModSubcode subcode) {
            return sReportError(pErrorListener, 
                    sev, 
                    EReaderCode::eReader_Mods,
                    subcode,
                    defline_info.mNumLine,
                    msg);
        };
        
    CModHandler::TModList mod_list;
    string remainder;

    // Parse the defline string for modifiers
    CTitleParser::Apply(defline, mod_list, remainder);
    if (mod_list.empty() && NStr::IsBlank(remainder)) {
        return;
    } 

    CModHandler mod_handler(fReportError);
    CModHandler::TModList rejected_mods;
    mod_handler.AddMods(mod_list, CModHandler::eAppendReplace, rejected_mods);

    // Apply modifiers to the bioseq
    CModHandler::TModList skipped_mods;
    CModAdder::Apply(mod_handler, bioseq, skipped_mods, fReportError);

    s_AppendMods(rejected_mods, remainder);
    s_AppendMods(skipped_mods, remainder);
    // Add title string
    NStr::TruncateSpacesInPlace(remainder);
    x_AddTitle(remainder, bioseq);
}


void CAlnReader::x_AddTitle(const string& title, CBioseq& bioseq) 
{
    if (NStr::IsBlank(title)) {
        return;
    }
    auto pDesc = Ref(new CSeqdesc());
    pDesc->SetTitle() = title;
    bioseq.SetDescr().Set().push_back(move(pDesc));
}


void CAlnReader::ParseDefline(const string& defline, 
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignoredErrors, 
    list<CRef<CSeq_id>>& ids, 
    bool& hasRange, 
    TSeqPos& rangeStart,
    TSeqPos& rangeEnd,
    TSeqTitles& seqTitles,
    ILineErrorListener* pMessageListener)
{
    CFastaDeflineReader::ParseDefline(
        defline, 
        info,
        ignoredErrors, 
        ids, 
        hasRange, 
        rangeStart, 
        rangeEnd,
        seqTitles, 
        pMessageListener);
}


CAlnReader::~CAlnReader() {}

END_NCBI_SCOPE
