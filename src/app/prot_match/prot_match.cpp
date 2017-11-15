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
* Author:  Justin Foley
*
* File Description:
*   implicit protein matching driver code
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/streamiter.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <objtools/edit/protein_match/generate_match_table.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/prot_match_utils.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include "run_binary.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CProteinMatchApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    using TSeqEntryLabel = string;
    using TEntryFilenameMap = map<TSeqEntryLabel, string>;
    using TEntryOStreamMap  = map<TSeqEntryLabel, unique_ptr<CObjectOStream>>;


    enum ENextAction {
        eRunMatch,
        eOverwrite,
        eSkipAhead,
        eReportWildDependent
    };


    template<typename TRoot>
    void x_GenerateMatchTable(CObjectIStream& istr, 
        const string& out_stub, 
        bool keep_temps, 
        CBinRunner& assm_assm_blastn,
        CBinRunner& compare_annots, 
        CMatchTabulate& match_tab,
        CScope& db_scope);

    void x_WriteMatchTable(
        const string& table_file,
        const CMatchTabulate& match_tab,
        bool suppress_exception=false) const;

    void x_InitNucProtSetMatch(CRef<CSeq_entry> nuc_prot_set,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map,
        list<string>& current_nuc_accessions,
        map<string, string>& new_nuc_accessions,
        map<string, list<string>>& local_prot_ids,
        map<string, list<string>>& prot_accessions,
        CMatchIdInfo& match_info,
        ENextAction& next_action,
        CMatchTabulate& match_tab,
        CScope& db_scope);

    void x_InitNucSeqMatch(CRef<CSeq_entry> nuc_seq,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map,
        list<string>& current_nuc_accessions,
        map<string, string>& new_nuc_accessions,
        map<string, list<string>>& prot_accessions,
        CMatchIdInfo& match_info,
        ENextAction& next_action,
        CScope& db_scope);

    template<typename TRoot>
    bool x_CheckForWildDependents(CObjectIStream& istr,
        CMatchTabulate& match_tab);

    bool x_ApplyOverwrite(CRef<CSeq_entry> update_entry,
                        CMatchTabulate& match_tab,
                        set<string>& processed_nuc_accessions);



    CObjectIStream* x_InitObjectIStream(const CArgs& args);
    CObjectOStream* x_InitObjectOStream(const string& filename, 
        const bool binary) const;
    CObjectIStream* x_InitObjectIStream(const string& filename,
        const bool binary) const;

    TTypeInfo x_GetRootTypeInfo(CObjectIStream& istr) const;
    bool x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const;

    void x_WriteEntry(const CSeq_entry& seq_entry,
        const string& key,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map) const;

    void x_WriteEntry(const CSeq_entry& seq_entry,
        const string& filename,
        CObjectOStream& ostream) const;

    void x_GetBlastArgs(
        const bool binary_output,
        const string& update_file, 
        const string& genbank_file,
        const string& alignment_file,
        vector<string>& blast_args) const;

    void x_GetCompareAnnotsArgs(const string& update_file,
        const string& genbank_file,
        const string& alignment_manifest_file,
        const string& annot_file,
        vector<string>& compare_annots_args) const;

    void x_ReadAnnotFile(const string& filename, list<CRef<CSeq_annot>>& seq_annots) const;

    void x_ReadAlignmentFile(const string& filename, CRef<CSeq_align>& alignment) const;

    void x_LogTempFile(const string& string);
    void x_DeleteTempFiles(void);

    struct SSeqEntryFilenames {
        string db_nuc_prot_set;
        string db_nuc_seq;
        string local_nuc_prot_set;
        string local_nuc_seq;
    };

    void x_GetSeqEntryFileNames(const string& file_stub, 
    SSeqEntryFilenames& filenames) const;

    void x_GetSeqEntryFileNames(const string& file_stub,
        TEntryFilenameMap&  filename_map) const;

    void x_CreateSeqEntryOStreams(const TEntryFilenameMap& filename_map,
        TEntryOStreamMap& ostream_map);

    void x_WriteToSeqEntryTempFiles(
        const CBioseq_set& nuc_prot_set,
        const CSeq_entry& db_entry,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map);

    void x_GatherLocalProteinIds(const CBioseq_set& nuc_prot_set, list<string>& id_list) const;

    void x_GatherProteinAccessions(const CBioseq_set& nuc_prot_set, list<string>& id_list) const;
    void x_GatherProteinAccessions(const CBioseq_set& nuc_prot_set, set<string>& id_set) const;

    void x_GatherReplacedProteinAccessions(const CBioseq_set& nuc_prot_set, set<string>& prot_ids) const;
 
    void x_GatherReplacedProteinAccessions(const CBioseq_set& nuc_prot_set, list<string>& prot_ids) const;

    void x_GatherReplacedProteinAccessions(const CBioseq_set& nuc_prot_set, map<string, list<string>>& replaced_prots) const;

    void x_RelabelNucSeq(CRef<CSeq_entry> nuc_prot_set);

    unique_ptr<CMatchSetup> m_pMatchSetup;
    set<string> m_TempFiles;
    bool m_TempFilesCreated;
};


void CProteinMatchApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    arg_desc->AddKey("i",
        "InputFile",
        "Update input file",
        CArgDescriptions::eInputFile);

    arg_desc->AddKey("o",
        "OutputFile",
        "Match-table file",
        CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("bindir", 
        "Binary_Dir", 
        "Directory containing C++ binaries, if not CWD",
        CArgDescriptions::eString);
    arg_desc->AddFlag("keep-temps", "Retain temporary files");

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc,
        CDataLoadersUtil::fDefault);

    SetupArgDescriptions(arg_desc.release());

    return;
}




int CProteinMatchApp::Run(void)
{
    m_TempFilesCreated = false;
    const CArgs& args = GetArgs();

    const string bin_dir = args["bindir"] ? 
        args["bindir"].AsString() :
        CDir::GetCwd();

#ifdef NCBI_OS_MSWIN
    CBinRunner assm_assm_blastn(bin_dir, "assm_assm_blastn.exe");
    CBinRunner compare_annots(bin_dir, "compare_annots.exe");
#else
    CBinRunner assm_assm_blastn(bin_dir, "assm_assm_blastn");
    CBinRunner compare_annots(bin_dir, "compare_annots");
#endif

    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *obj_mgr);
    CRef<CScope> db_scope = Ref(new CScope(*obj_mgr));
    db_scope->AddDefaults();
    m_pMatchSetup.reset(new CMatchSetup(db_scope));

    bool keep_temps = args["keep-temps"];
    
    unique_ptr<CObjectIStream> pInStream(x_InitObjectIStream(args));


    const string table_file = args["o"].AsString();
    if (NStr::IsBlank(table_file)) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Table filename not specified");
    }

    CMatchTabulate match_tab(db_scope);

    const TTypeInfo root_info =  x_GetRootTypeInfo(*pInStream);

    // Check for wild dependent errors
    bool has_wild_dependent = false;
    if (root_info == CSeq_entry::GetTypeInfo()) {
        has_wild_dependent = x_CheckForWildDependents<CSeq_entry>(*pInStream, match_tab);
    }
    else {
        has_wild_dependent = x_CheckForWildDependents<CBioseq_set>(*pInStream, match_tab);
    }
    pInStream->Close();

    if (has_wild_dependent) {
        x_WriteMatchTable(table_file, match_tab);
        return 0; // What should I return here?
    }
    pInStream.reset(x_InitObjectIStream(args));


    try {
        if (root_info == CSeq_entry::GetTypeInfo()) {
            x_GenerateMatchTable<CSeq_entry>(*pInStream,
                table_file,
                keep_temps,
                assm_assm_blastn,
                compare_annots,
                match_tab,
                *db_scope);
        } else { // Must be CBioseq_set
            x_GenerateMatchTable<CBioseq_set>(*pInStream,
                table_file,
                keep_temps,
                assm_assm_blastn,
                compare_annots,
                match_tab,
                *db_scope);
        }

    } 
    catch (...) 
    {
        const bool suppress_write_exceptions = true;
        x_WriteMatchTable(table_file, match_tab, suppress_write_exceptions);
        throw;
    }

    x_WriteMatchTable(table_file, match_tab);

    return 0;
}


void CProteinMatchApp::x_WriteMatchTable(
        const string& table_file,
        const CMatchTabulate& match_tab,
        const bool suppress_exception) const
{
    if (match_tab.GetNum_rows() == 0) {
        ERR_POST(Warning << "Match table is empty");
        return;
    }

    try {
        CNcbiOfstream ostr(table_file.c_str());
        match_tab.WriteTable(ostr);
    }
    catch (...) {
        if (suppress_exception) {
            return;
        }
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "Could not write match table");
    }
}

template<typename TRoot>
bool CProteinMatchApp::x_CheckForWildDependents(CObjectIStream& istr,
    CMatchTabulate& match_tab)
{
    CObjectIStreamIterator<TRoot, CBioseq_set> bioseqset_it(istr, eNoOwnership,
        CObjectIStreamIterator<CBioseq_set>::CParams().FilterByMember("class",
            [](const CObjectIStream& istr, 
               CBioseq_set& root,
               TMemberIndex mem_index, 
               CObjectInfo* objinfo_ptr,
               void* extra)->bool {

                if (!objinfo_ptr) {
                    return false;
                }
                    
                CBioseq_set::EClass e_class = *CTypeConverter<CBioseq_set::EClass>::SafeCast(objinfo_ptr->GetObjectPtr());
                return (e_class == CBioseq_set::eClass_nuc_prot);
            }));

    bool has_wild_dependent = false;
    for (const CBioseq_set& nuc_prot_set : bioseqset_it) {

        CRef<CSeq_id> nuc_seqid;
        const bool success = m_pMatchSetup->GetAccession(
            nuc_prot_set.GetNucFromNucProtSet(),
            nuc_seqid);

        string db_nuc_acc_string = "";
        if (success) {
            string update_nuc_acc_string = nuc_seqid->GetSeqIdString();

            list<string> update_prot_accessions;
            x_GatherProteinAccessions(nuc_prot_set, update_prot_accessions);
            CConstRef<CSeq_entry> db_entry = m_pMatchSetup->GetDBEntry(*nuc_seqid);

            if (!db_entry.IsNull() &&
                db_entry->IsSet()) {
                set<string> db_prot_accessions;
                x_GatherProteinAccessions(db_entry->GetSet(), db_prot_accessions);

                list<string> wild_dependents;
                for (const string& update_prot_accession : update_prot_accessions) {
                    if (db_prot_accessions.find(update_prot_accession) == db_prot_accessions.end()) {
                        wild_dependents.push_back(update_prot_accession);
                    }
                }

                if (!wild_dependents.empty()) {
                    match_tab.ReportWildDependents(update_nuc_acc_string, wild_dependents);
                    has_wild_dependent = true;
                }
            }
        }
    }
    return has_wild_dependent;
}


template<typename TRoot>
void CProteinMatchApp::x_GenerateMatchTable(CObjectIStream& istr, 
        const string& out_stub,
        bool keep_temps, 
        CBinRunner& assm_assm_blastn,
        CBinRunner& compare_annots, 
        CMatchTabulate& match_tab,
        CScope& db_scope)

{
    try {
        TEntryFilenameMap filename_map;
        x_GetSeqEntryFileNames(out_stub, filename_map);
       
        list<string> current_nuc_accessions; 
        map<string, string> new_nuc_accessions; 
        map<string, list<string>> local_prot_ids;
        map<string, list<string>> prot_accessions;

        list<CMatchIdInfo> match_id_info;
   
        { 
            TEntryOStreamMap ostream_map; // must go out of scope before we attempt to remove temporary files - MSS-670
            CObjectIStreamIterator<TRoot, CBioseq_set> bioseqset_it(istr, eNoOwnership,
                CObjectIStreamIterator<CBioseq_set>::CParams().FilterByMember("class",
                    [](const CObjectIStream& istr, 
                    CBioseq_set& root,
                    TMemberIndex mem_index, 
                    CObjectInfo* objinfo_ptr,
                    void* extra)->bool {

                    if (!objinfo_ptr) {
                        return false;
                    }
                    
                    CBioseq_set::EClass e_class = *CTypeConverter<CBioseq_set::EClass>::SafeCast(objinfo_ptr->GetObjectPtr());
                    return (e_class == CBioseq_set::eClass_nuc_prot);
                }));

            set<string> processed_nuc_accessions;
            ENextAction next_action;
            for (const CBioseq_set& obj : bioseqset_it) 
            {
                CRef<CSeq_entry> seq_entry = Ref(new CSeq_entry());
                CRef<CBioseq_set> bio_set = Ref(new CBioseq_set());
                bio_set->Assign(obj);
                seq_entry->SetSet(*bio_set);

                list<string> set_current_nuc_accessions;
                map<string, string> set_new_nuc_accessions;
                map<string, list<string>> set_prot_accessions;
                map<string, list<string>> set_local_prot_ids;

                CMatchIdInfo match_info;

                x_InitNucProtSetMatch(seq_entry,
                    filename_map,
                    ostream_map,
                    set_current_nuc_accessions,
                    set_new_nuc_accessions,
                    set_local_prot_ids,
                    set_prot_accessions,
                    match_info,
                    next_action,
                    match_tab,
                    db_scope); 
            
                for (const string& accession : set_current_nuc_accessions) {
                    processed_nuc_accessions.insert(accession);
                }
                for (auto key_val : set_new_nuc_accessions) {
                    processed_nuc_accessions.insert(key_val.second);
                }
              
                if (next_action == eOverwrite) {
                    x_ApplyOverwrite(seq_entry, match_tab, processed_nuc_accessions);
                    continue;
                }
                else if (next_action != eRunMatch) {
                    continue;
                }

                match_id_info.push_back(match_info);

                // Clean this up by gathering accessions into struct
                current_nuc_accessions.merge(set_current_nuc_accessions);
                new_nuc_accessions.insert(set_new_nuc_accessions.begin(), set_new_nuc_accessions.end());
                prot_accessions.insert(set_prot_accessions.begin(), set_prot_accessions.end());
                local_prot_ids.insert(set_local_prot_ids.begin(), set_local_prot_ids.end());
            }
            istr.Close();

            // Second pass - to detect unprocessed nucleotide sequences
            unique_ptr<CObjectIStream> pIstr(x_InitObjectIStream(GetArgs()));
            CObjectIStreamIterator<TRoot, CBioseq> bioseq_it(*pIstr, eNoOwnership);
            for (const CBioseq& bioseq : bioseq_it) {
                if (!bioseq.IsSetInst() ||
                    !bioseq.IsNa()) {
                    continue;
                }

                CRef<CSeq_id> nuc_seqid;
                const bool success = m_pMatchSetup->GetAccession(bioseq, nuc_seqid);
                if (!success || 
                    (processed_nuc_accessions.find(nuc_seqid->GetSeqIdString()) != 
                     processed_nuc_accessions.end())) {
                    continue;
                } 

                CMatchIdInfo match_info;

                CRef<CSeq_entry> seq_entry = Ref(new CSeq_entry());
                CRef<CBioseq> bio_seq = Ref(new CBioseq());
                bio_seq->Assign(bioseq);
                seq_entry->SetSeq(*bio_seq);

                x_InitNucSeqMatch(seq_entry,
                    filename_map,
                    ostream_map,
                    current_nuc_accessions,
                    new_nuc_accessions,
                    prot_accessions,
                    match_info,
                    next_action,
                    db_scope);


                if (next_action == eOverwrite) {
                    x_ApplyOverwrite(seq_entry, match_tab, processed_nuc_accessions);
                } 
                else {
                    match_id_info.push_back(match_info);
                }
            }
        }

        if (!m_TempFilesCreated) {
            return;
        }

        const string alignment_file = out_stub + ".merged.asn";
        //const bool binary_output = true;
        const bool binary_output = false;

        vector<string> blast_args;
        x_GetBlastArgs(
            binary_output,
            filename_map["local_nuc_seq"],
            filename_map["db_nuc_seq"],
            alignment_file,
            blast_args);

        x_LogTempFile(alignment_file);
        assm_assm_blastn.Exec(blast_args);
        // Create alignment manifest tempfile 
        const string manifest_file = out_stub + ".aln.mft";
        try {
            CNcbiOfstream ostr(manifest_file.c_str());
            x_LogTempFile(manifest_file);
            ostr << alignment_file << endl;
        }
        catch(...) {
            NCBI_THROW(CProteinMatchException,
                eOutputError,
                "Could not write alignment manifest");
        }
        const string annot_file = out_stub + ".compare.asn";

        vector<string> compare_annots_args;
        x_GetCompareAnnotsArgs(
            filename_map["local_nuc_prot_set"],
            filename_map["db_nuc_prot_set"],
            manifest_file, 
            annot_file,
            compare_annots_args);

        x_LogTempFile(annot_file);
        compare_annots.Exec(compare_annots_args);
 
        unique_ptr<CObjectIStream> align_istr_ptr(x_InitObjectIStream(alignment_file, false)); // false => not binary
        unique_ptr<CObjectIStream> annot_istr_ptr(x_InitObjectIStream(annot_file, true));
    
        match_tab.GenerateMatchTable(
            local_prot_ids, 
            prot_accessions, 
            current_nuc_accessions,
            new_nuc_accessions,
            match_id_info,
            *align_istr_ptr,
            *annot_istr_ptr);

        align_istr_ptr->Close();
        annot_istr_ptr->Close();
        if (!keep_temps) {
            x_DeleteTempFiles();
        }
    } 
    catch(...)
    {
        if (!keep_temps) {
            x_DeleteTempFiles();
        }
        throw;
    }
}


void CProteinMatchApp::x_InitNucSeqMatch(CRef<CSeq_entry> nuc_seq,
    const CProteinMatchApp::TEntryFilenameMap& filename_map,
    CProteinMatchApp::TEntryOStreamMap& ostream_map,
    list<string>& current_nuc_accessions,
    map<string, string>& new_nuc_accessions,
    map<string, list<string>>& prot_accessions,
    CMatchIdInfo& match_info,
    ENextAction& next_action,
    CScope& db_scope)
{
    next_action = eRunMatch;
    CRef<CSeq_id> nuc_seqid;
    const bool success = m_pMatchSetup->GetAccession(
        nuc_seq->GetSeq(),
        nuc_seqid);

    string db_nuc_acc_string = "";
    if (success) {
        string update_nuc_acc_string = nuc_seqid->GetSeqIdString();

        match_info.SetUpdateNucId() = update_nuc_acc_string;

        list<CRef<CSeq_id>> hist_ids;
        if (m_pMatchSetup->GetReplacedIdsFromHist(
                    nuc_seq->GetSeq(),
                    hist_ids)) {

            if (db_scope.GetBioseqHandle(*nuc_seqid)) {
                next_action = eOverwrite;
                return;
            }
            nuc_seqid = hist_ids.front();
            db_nuc_acc_string = nuc_seqid->GetSeqIdString();
            new_nuc_accessions[db_nuc_acc_string] = update_nuc_acc_string;
        }
        else {
            db_nuc_acc_string = update_nuc_acc_string;
        }
    }

    match_info.SetDBNucId() = db_nuc_acc_string; 

    current_nuc_accessions.push_back(db_nuc_acc_string);

    if (nuc_seqid.IsNull() || NStr::IsBlank(db_nuc_acc_string)) {
        NCBI_THROW(CProteinMatchException, 
        eInputError,
        "Failed to find valid sequence id");
    }

    CConstRef<CSeq_entry> db_entry = m_pMatchSetup->GetDBEntry(*nuc_seqid);
    if (db_entry.IsNull()) {
        NCBI_THROW(CProteinMatchException, 
        eInternalError,
        "Failed to fetch database entry");
    }

    if (db_entry->IsSet()) {
        x_GatherProteinAccessions(db_entry->GetSet(), prot_accessions[db_nuc_acc_string]);

        x_GatherProteinAccessions(db_entry->GetSet(), match_info.SetDBProtIds());
    }

    // Convert nuc-seq id to a local id
    const CSeq_id* id_ptr = nuc_seq->GetSeq().GetFirstId();
    if (id_ptr) {
        if (!id_ptr->IsLocal() ||
            nuc_seq->GetSeq().GetId().size() > 1) {
            CRef<CSeq_id> local_id = Ref(new CSeq_id());
            local_id->SetLocal().SetStr(id_ptr->GetSeqIdString(true)); 
            nuc_seq->SetSeq().ResetId();
            nuc_seq->SetSeq().SetId().push_back(local_id);
        }
    }

    if (!m_TempFilesCreated) {
        x_CreateSeqEntryOStreams(filename_map, ostream_map);
        m_TempFilesCreated = true;
    }

    x_WriteEntry(*nuc_seq,
        "local_nuc_seq",
        filename_map,
        ostream_map);

    if (db_entry->IsSet()) {
        const CBioseq& db_nuc_seq = db_entry->GetSet().GetNucFromNucProtSet();
        CRef<CSeq_entry> db_nuc_se = Ref(new CSeq_entry());
        db_nuc_se->SetSeq().Assign(db_nuc_seq);
        x_WriteEntry(*db_nuc_se, 
            "db_nuc_seq",
            filename_map,
            ostream_map);
    } 
    else { // db_entry.IsSeq() == true
        x_WriteEntry(*db_entry,
            "db_nuc_seq",
            filename_map,
            ostream_map);
    }
}


void s_FindAccessions(const CBioseq_set& set, list<string>& nuc_ids, map<string, list<string>>& prot_ids) 
{

    const bool with_version = false;
    if (set.IsSetClass() &&
        (set.GetClass() == CBioseq_set::eClass_nuc_prot)) {
        list<string> prot_accessions;
        string nuc_id;
        for (CRef<CSeq_entry> seq_seqentry : set.GetSeq_set()) {
            const CBioseq& bioseq = seq_seqentry->GetSeq();
            if (bioseq.IsNa()) {
                for (CRef<CSeq_id> id : bioseq.GetId()) {
                    if (id->IsGenbank() || id->IsOther()) {
                        nuc_id = id->GetSeqIdString(with_version);
                        nuc_ids.push_back(nuc_id);
                        break;
                    }
                }
            }
            else 
            if (bioseq.IsAa()) {
                for (CRef<CSeq_id> id : bioseq.GetId()) {
                    if (id->IsGenbank() || id->IsOther()) {
                        prot_accessions.push_back(id->GetSeqIdString(with_version));
                        break;
                    }
                }
            }
        }
        if (!prot_accessions.empty()) {
            prot_ids[nuc_id] = prot_accessions;
        }
        return;
    }


    // else
    for (CRef<CSeq_entry> seqentry : set.GetSeq_set()) {
        if (seqentry->IsSet()) {
            s_FindAccessions(seqentry->GetSet(), nuc_ids, prot_ids);
        }
        else { // IsSeq
            const CBioseq& bioseq = seqentry->GetSeq();
            if (bioseq.IsNa()) {
                for (CRef<CSeq_id> id : bioseq.GetId()) {
                    if (id->IsGenbank() || id->IsOther()) {
                        nuc_ids.push_back(id->GetSeqIdString(with_version));
                        break;
                    }
                }
            } 
        }
    }
}


void s_FindAccessions(const CSeq_entry& entry, list<string>& nuc_ids, map<string, list<string>>& prot_ids) {

    if (entry.IsSet()) {
        s_FindAccessions(entry.GetSet(), nuc_ids, prot_ids);
        return;
    }
    
    const bool with_version = false; 
    const CBioseq& bioseq = entry.GetSeq();   
    if (bioseq.IsNa()) {
        for (CRef<CSeq_id> id : bioseq.GetId()) {
            if (id->IsGenbank() || id->IsOther()) {
                nuc_ids.push_back(id->GetSeqIdString(with_version));
                return;
            }
        }
    } 
}

bool CProteinMatchApp::x_ApplyOverwrite(CRef<CSeq_entry> update_entry, CMatchTabulate& match_tab, set<string>& processed_nuc_accessions) 
{

    SOverwriteIdInfo overwrite_info;
    CRef<CSeq_id> update_nuc_id;

    const bool is_set = update_entry->IsSet(); // This needs to be a nuc-prot set. Should I enforce it?

    const CBioseq& update_nuc_seq = is_set ? 
                                    update_entry->GetSet().GetNucFromNucProtSet() :
                                    update_entry->GetSeq();


    if (!m_pMatchSetup->GetAccession(update_nuc_seq, update_nuc_id)) {
        return false;
    }
    list<CRef<CSeq_id>> hist_nuc_ids;
    if (!m_pMatchSetup->GetReplacedIdsFromHist(update_nuc_seq, hist_nuc_ids)) {
        return false;
    }

    const bool with_version = false;
    overwrite_info.update_nuc_id = update_nuc_id->GetSeqIdString(with_version);

    for (CRef<CSeq_id> hist_nuc_id : hist_nuc_ids) {
        overwrite_info.replaced_nuc_ids.push_back(hist_nuc_id->GetSeqIdString(with_version)); 
    }

    if (is_set) { 
        const CBioseq_set& update_set = update_entry->GetSet();
        x_GatherProteinAccessions(update_set, overwrite_info.update_prot_ids);
        x_GatherReplacedProteinAccessions(update_set, overwrite_info.replaced_prot_ids);
        x_GatherReplacedProteinAccessions(update_set, overwrite_info.replaced_prot_id_map);
    }

    CSeq_entry_Handle db_entry = m_pMatchSetup->GetTopLevelEntry(*hist_nuc_ids.front());
    if (db_entry) {
        s_FindAccessions(*db_entry.GetCompleteSeq_entry(), 
                overwrite_info.db_nuc_ids, 
                overwrite_info.db_prot_ids);
    }

    match_tab.OverwriteEntry(overwrite_info);
    processed_nuc_accessions.insert(overwrite_info.update_nuc_id);

    return true;

}

void CProteinMatchApp::x_InitNucProtSetMatch(CRef<CSeq_entry> nuc_prot_set,
    const CProteinMatchApp::TEntryFilenameMap& filename_map,
    CProteinMatchApp::TEntryOStreamMap& ostream_map,
    list<string>& current_nuc_accessions,
    map<string, string>& new_nuc_accessions,
    map<string, list<string>>& local_prot_ids,
    map<string, list<string>>& prot_accessions,
    CMatchIdInfo& match_info,
    ENextAction& next_action,
    CMatchTabulate& match_tab,
    CScope& db_scope) 
{
    next_action = eRunMatch;

    CRef<CSeq_id> nuc_seqid;
    const bool success = m_pMatchSetup->GetAccession(
        nuc_prot_set->GetSet().GetNucFromNucProtSet(),
        nuc_seqid);

    string db_nuc_acc_string = "";
    if (success) {
        string update_nuc_acc_string = nuc_seqid->GetSeqIdString();
        match_info.SetUpdateNucId() = update_nuc_acc_string;

        list<CRef<CSeq_id>> hist_ids;
        if (m_pMatchSetup->GetReplacedIdsFromHist(
                    nuc_prot_set->GetSet().GetNucFromNucProtSet(),
                    hist_ids)) {

            if (db_scope.GetBioseqHandle(*nuc_seqid)) {
                next_action = eOverwrite;
                return;
            }
            nuc_seqid = hist_ids.front();
            db_nuc_acc_string = nuc_seqid->GetSeqIdString();
            new_nuc_accessions[db_nuc_acc_string] = update_nuc_acc_string;
        }
        else {
            db_nuc_acc_string = update_nuc_acc_string;
        }
    } 

    match_info.SetDBNucId() = db_nuc_acc_string;
    current_nuc_accessions.push_back(db_nuc_acc_string);

    if (nuc_seqid.IsNull() || NStr::IsBlank(db_nuc_acc_string)) {
        NCBI_THROW(CProteinMatchException, 
        eInputError,
        "Failed to find valid sequence id");
    }

    CConstRef<CSeq_entry> db_entry = m_pMatchSetup->GetDBEntry(*nuc_seqid);
    if (db_entry.IsNull()) {
        NCBI_THROW(CProteinMatchException, 
        eInternalError,
        "Failed to fetch database entry");
    }

    x_GatherLocalProteinIds(nuc_prot_set->GetSet(), local_prot_ids[db_nuc_acc_string]);

    x_GatherLocalProteinIds(nuc_prot_set->GetSet(), match_info.SetUpdateLocalProtIds());

    if (db_entry->IsSet()) {
        x_GatherProteinAccessions(db_entry->GetSet(), prot_accessions[db_nuc_acc_string]);

        x_GatherProteinAccessions(db_entry->GetSet(), match_info.SetDBProtIds());
    }

    CRef<CSeq_entry> core_nuc_prot_set = m_pMatchSetup->GetCoreNucProtSet(*nuc_prot_set);

   // CRef<CSeq_entry> core_nuc_prot_set = nuc_prot_set;

    x_RelabelNucSeq(core_nuc_prot_set); // Temporary - need to fix this

    if (!m_TempFilesCreated) {
        x_CreateSeqEntryOStreams(filename_map, ostream_map);
        m_TempFilesCreated = true;
    }

    x_WriteToSeqEntryTempFiles(
        core_nuc_prot_set->GetSet(),
        *db_entry,
        filename_map,
        ostream_map);

    return;
}


TTypeInfo CProteinMatchApp::x_GetRootTypeInfo(CObjectIStream& istr) const
{
    set<TTypeInfo> knownTypes, matchingTypes;
    knownTypes.insert(CSeq_entry::GetTypeInfo());
    knownTypes.insert(CBioseq_set::GetTypeInfo());
    matchingTypes = istr.GuessDataType(knownTypes);

    if (matchingTypes.empty()) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Unrecognized input");
    }

    if (matchingTypes.size() > 1) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Ambiguous input");
    }

    return *matchingTypes.begin();
}


CObjectIStream* CProteinMatchApp::x_InitObjectIStream(
    const CArgs& args) 
{
    ESerialDataFormat serial = eSerial_AsnText;

    const char* infile = args["i"].AsString().c_str();
    CNcbiIstream* pInputStream = new CNcbiIfstream(infile, ios::binary); 
   

    CObjectIStream* pI = CObjectIStream::Open(
            serial, *pInputStream, eTakeOwnership);

    if (!pI) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Failed to create input stream");
    }
    return pI;
}


bool CProteinMatchApp::x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const
{
    try {
        istr.Read(ObjectInfo(seq_entry));
    }
    catch (CException&) {
        return false;
    }

    return true;
}


bool CProteinMatchApp::x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const
{

    CRef<CBioseq_set> seq_set = Ref(new CBioseq_set());
    try {
        istr.Read(ObjectInfo(seq_set.GetNCObject()));
    }
    catch (CException&) {
        return false;
    }

    seq_entry.SetSet(seq_set.GetNCObject());
    return true;
}


void CProteinMatchApp::x_RelabelNucSeq(CRef<CSeq_entry> nuc_prot_set) 
{

    CRef<CSeq_id> local_id;
    // Could optimize this. No need to look for CDS features if the 
    // nuc-prot set contains only a nucleotide sequence.
    if (!m_pMatchSetup->GetNucSeqIdFromCDSs(*nuc_prot_set, local_id)) {

        // There are no CDS features, so just make the nuc-seq id local 
        const CBioseq& nuc_seq = nuc_prot_set->GetSet().GetNucFromNucProtSet();
        const CSeq_id* id_ptr = nuc_seq.GetFirstId();
        if (id_ptr) {
            if (id_ptr->IsLocal()) {
                if (nuc_seq.GetId().size() == 1) {
                    return; // Bioseq already has a single local id => nothing to do here
                } // else
                local_id = Ref(new CSeq_id());
                local_id->Assign(*id_ptr);
            }
            else {
                local_id = Ref(new CSeq_id());
                local_id->SetLocal().SetStr(id_ptr->GetSeqIdString(true)); 
            } 
        }
    }

    if (!m_pMatchSetup->UpdateNucSeqIds(local_id, nuc_prot_set.GetNCObject())) {
        NCBI_THROW(CProteinMatchException, 
                    eExecutionError, 
                    "Unable to assign local nucleotide id");
    }
    return;
}


void CProteinMatchApp::x_GatherLocalProteinIds(const CBioseq_set& nuc_prot_set,
    list<string>& id_list) const 
{
    id_list.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; // Throw an exception here
    }

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa()) {
            const CSeq_id* local_id = bioseq.GetLocalId();
            if (local_id != nullptr) {
                id_list.push_back(local_id->GetSeqIdString());
            }
        }
    }
}


void CProteinMatchApp::x_GatherProteinAccessions(const CBioseq_set& nuc_prot_set,
    list<string>& id_list) const 
{
    id_list.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; 
    }

    const bool with_version = false;

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa()) {
            for (CRef<CSeq_id> id : bioseq.GetId()) {
                if (id->IsGenbank() || id->IsOther()) {
                    id_list.push_back(id->GetSeqIdString(with_version));
                    break;
                }
            }
        }
    }
}

void CProteinMatchApp::x_GatherProteinAccessions(const CBioseq_set& nuc_prot_set,
    set<string>& id_set) const 
{
    id_set.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; 
    }

    const bool with_version = false;

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa()) {
            for (CRef<CSeq_id> id : bioseq.GetId()) {
                if (id->IsGenbank() || id->IsOther()) {
                    id_set.insert(id->GetSeqIdString(with_version));
                    break;
                }
            }
        }
    }
}

void CProteinMatchApp::x_GatherReplacedProteinAccessions(const CBioseq_set& nuc_prot_set,
        set<string>& id_set) const 
{
    id_set.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; 
    }
    const bool with_version = false;

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa() &&
            bioseq.IsSetInst() &&
            bioseq.GetInst().IsSetHist() &&
            bioseq.GetInst().GetHist().IsSetReplaces()) {

            const CSeq_hist_rec& replaces = bioseq.GetInst().GetHist().GetReplaces();
            if (replaces.IsSetIds()) {
                for (CRef<CSeq_id> id : replaces.GetIds()) {
                    id_set.insert(id->GetSeqIdString(with_version));
                }
            }
        }
    }
}

void CProteinMatchApp::x_GatherReplacedProteinAccessions(const CBioseq_set& nuc_prot_set,
        list<string>& id_list) const 
{
    id_list.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; 
    }
    const bool with_version = false;

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa() &&
            bioseq.IsSetInst() &&
            bioseq.GetInst().IsSetHist() &&
            bioseq.GetInst().GetHist().IsSetReplaces()) {

            const CSeq_hist_rec& replaces = bioseq.GetInst().GetHist().GetReplaces();
            if (replaces.IsSetIds()) {
                for (CRef<CSeq_id> id : replaces.GetIds()) {
                    id_list.push_back(id->GetSeqIdString(with_version));
                }
            }
        }
    }
}



void CProteinMatchApp::x_GatherReplacedProteinAccessions(const CBioseq_set& nuc_prot_set,
    map<string, list<string>>& replaced_proteins) const
{
    replaced_proteins.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; 
    }
    const bool with_version = false;

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa() &&
            bioseq.IsSetInst() &&
            bioseq.GetInst().IsSetHist() &&
            bioseq.GetInst().GetHist().IsSetReplaces()) {

            CRef<CSeq_id> pUpdateId;
            if (!m_pMatchSetup->GetAccession(bioseq, pUpdateId)) {
                continue;
            }
            const string update_id = pUpdateId->GetSeqIdString(with_version);

            list<string> replaced_id_list;
            const CSeq_hist_rec& replaces = bioseq.GetInst().GetHist().GetReplaces();
            if (replaces.IsSetIds()) {
                for (CRef<CSeq_id> id : replaces.GetIds()) {
                    replaced_id_list.push_back(id->GetSeqIdString(with_version));
                }
            }

            if (!replaced_id_list.empty()) {
                replaced_proteins[update_id] = replaced_id_list;
            }
        }
    }
}



void CProteinMatchApp::x_GetSeqEntryFileNames(const string& file_stub, 
    CProteinMatchApp::SSeqEntryFilenames& filenames) const
{
    if (NStr::IsBlank(file_stub)) {
        NCBI_THROW(CProteinMatchException, 
            eInternalError, 
            "Empty file stub");
    }

    filenames.db_nuc_prot_set = file_stub + ".db.asn";
    filenames.db_nuc_seq      = file_stub + ".db_nuc.asn";
    filenames.local_nuc_prot_set = file_stub + ".local.asn";
    filenames.local_nuc_seq      = file_stub + ".local_nuc.asn";   
}


void CProteinMatchApp::x_GetSeqEntryFileNames(const string& file_stub,
    CProteinMatchApp::TEntryFilenameMap&  filename_map) const
{

    filename_map["db_nuc_prot_set"]     = file_stub + ".db.asn";
    filename_map["db_nuc_seq"]          = file_stub + ".db_nuc.asn";
    filename_map["local_nuc_prot_set"]  = file_stub + ".local.asn";
    filename_map["local_nuc_seq"]       = file_stub + ".local_nuc.asn";   
}


void CProteinMatchApp::x_CreateSeqEntryOStreams(const CProteinMatchApp::TEntryFilenameMap& filename_map,
    CProteinMatchApp::TEntryOStreamMap& ostream_map) 
{
    static const bool as_text = false;
    for (const auto& key_val : filename_map) {
        ostream_map[key_val.first].reset(x_InitObjectOStream(key_val.second, as_text));
        x_LogTempFile(key_val.second);
    }
}


void CProteinMatchApp::x_WriteEntry(const CSeq_entry& seq_entry,
        const string& key,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map) const
{
    x_WriteEntry(seq_entry,
            filename_map.at(key),
            *ostream_map[key]);
}


void CProteinMatchApp::x_WriteEntry(const CSeq_entry& seq_entry,
        const string& filename,
        CObjectOStream& ostream) const
{
    try { 
        ostream << seq_entry;
    } catch (...) {
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "Failed to write to " + filename);
    }
}



void CProteinMatchApp::x_WriteToSeqEntryTempFiles(
    const CBioseq_set& nuc_prot_set,
    const CSeq_entry& db_entry,
    const CProteinMatchApp::TEntryFilenameMap& filename_map,
    CProteinMatchApp::TEntryOStreamMap& ostream_map) 

{
    // Write the genbank nuc-prot-set to a file
    if (db_entry.IsSet()) {
        x_WriteEntry(db_entry,
            "db_nuc_prot_set",
            filename_map,
            ostream_map);

        const CBioseq& db_nuc_seq = db_entry.GetSet().GetNucFromNucProtSet();
        CRef<CSeq_entry> db_nuc_se = Ref(new CSeq_entry());
        db_nuc_se->SetSeq().Assign(db_nuc_seq);
        x_WriteEntry(*db_nuc_se, 
            "db_nuc_seq",
            filename_map,
            ostream_map);
    } 
    else { // db_entry.IsSeq() == true
        x_WriteEntry(db_entry,
            "db_nuc_seq",
            filename_map,
            ostream_map);
    }

    CRef<CSeq_entry> nuc_prot_se = Ref(new CSeq_entry());
    nuc_prot_se->SetSet().Assign(nuc_prot_set);


    x_WriteEntry(*nuc_prot_se, 
        "local_nuc_prot_set",
        filename_map,
        ostream_map);

    // Write update nucleotide sequence
    const CBioseq& nuc_seq = nuc_prot_set.GetNucFromNucProtSet();
    CRef<CSeq_entry> nuc_se = Ref(new CSeq_entry());
    nuc_se->SetSeq().Assign(nuc_seq);
    x_WriteEntry(*nuc_se, 
        "local_nuc_seq",
        filename_map,
        ostream_map);
}


void CProteinMatchApp::x_ReadAnnotFile(const string& filename,
    list<CRef<CSeq_annot>>& seq_annots) const
{
    const bool binary = true;
    unique_ptr<CObjectIStream> pObjIstream(x_InitObjectIStream(filename, binary));

    if (!seq_annots.empty()) {
        seq_annots.clear();
    }

    while(!pObjIstream->EndOfData()) {
        CRef<CSeq_annot> pSeqAnnot(new CSeq_annot());
        try {
            pObjIstream->Read(ObjectInfo(*pSeqAnnot));
        }
        catch (CException&) {
            NCBI_THROW(CProteinMatchException, 
                       eInputError, 
                      "Could not read \"" + filename + "\"");
        }
        seq_annots.push_back(pSeqAnnot);
    }
}


void CProteinMatchApp::x_ReadAlignmentFile(const string& filename, 
    CRef<CSeq_align>& pSeqAlign) const
{
    const bool binary = true;
    unique_ptr<CObjectIStream> pObjIstream(x_InitObjectIStream(filename, binary));

    try {
        pObjIstream->Read(ObjectInfo(*pSeqAlign));
    } 
    catch (CException&) {
        NCBI_THROW(CProteinMatchException, 
            eInputError, 
            "Could not read \"" + filename + "\"");
    }
}


CObjectIStream* CProteinMatchApp::x_InitObjectIStream(const string& filename,
    const bool binary) const
{
    IOS_BASE::openmode op_mode = IOS_BASE::in;
    if (binary) {
        op_mode |= IOS_BASE::binary;
    }

    CNcbiIstream* pInStream = new CNcbiIfstream(filename.c_str(), op_mode);

    if (pInStream->fail()) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Could not create input stream for \"" + filename + "\"");
    }

    ESerialDataFormat serial = binary ? eSerial_AsnBinary : eSerial_AsnText;

    CObjectIStream* pObjIStream = CObjectIStream::Open(serial,
                                                       *pInStream,
                                                       eTakeOwnership);

    if (!pObjIStream) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Unable to open input file \"" + filename + "\"");
    }

    return pObjIStream;
}


CObjectOStream* CProteinMatchApp::x_InitObjectOStream(const string& filename, 
    const bool binary) const
{
    if (filename.empty()) {
        NCBI_THROW(CProteinMatchException, 
                   eOutputError, 
                   "Output file name not specified");
    }

    ESerialDataFormat serial = binary ? eSerial_AsnBinary : eSerial_AsnText;

    CObjectOStream* pOstr = CObjectOStream::Open(filename, serial);

    if (!pOstr) {
        NCBI_THROW(CProteinMatchException, 
                   eOutputError, 
                   "Unable to open output file: " + filename);
    }

    return pOstr;
}


void CProteinMatchApp::x_GetBlastArgs(
    const bool binary_output,
    const string& update_file, 
    const string& genbank_file,
    const string& alignment_file,
    vector<string>& blast_args) const
{
    if (NStr::IsBlank(update_file) ||
        NStr::IsBlank(genbank_file)) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "assm_assm_blastn input file not specified");
    }


    if (NStr::IsBlank(alignment_file)) {
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "assm_assm_blastn alignment file not specified");
    }

    vector<string> args {
        "-query", update_file,
        "-target", genbank_file,
        "-task", "megablast", 
        "-word_size", "16", 
        "-evalue", "0.01", 
        "-gapopen", "2", 
        "-gapextend", "1", 
        "-best_hit_overhang", "0.1", 
        "-best_hit_score_edge", "0.1", 
        "-align-output", alignment_file,
        "-nogenbank"
    };

    blast_args = args;
    blast_args.push_back("-ofmt");
    if (binary_output) {
        blast_args.push_back("asn-binary");
    }
    else 
    {
        blast_args.push_back("asn-text");    
    }
}


void CProteinMatchApp::x_GetCompareAnnotsArgs(
        const string& update_file,
        const string& genbank_file,
        const string& alignment_manifest_file,
        const string& annot_file,
        vector<string>& compare_annots_args)  const
{
    if (NStr::IsBlank(update_file) ||
        NStr::IsBlank(genbank_file) ||
        NStr::IsBlank(alignment_manifest_file)) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "compare_annots input file not specified");
    }

    if (NStr::IsBlank(annot_file)) {
        NCBI_THROW(CProteinMatchException,
            eOutputError, 
            "compare_annots annot file is not specified");
    }

    vector<string> args {
        "-q_scope_type", "annots" ,
        "-q_scope_args", update_file,
        "-s_scope_type", "annots",
        "-s_scope_args", genbank_file,
        "-alns", alignment_manifest_file,
        "-o_asn", annot_file,
        "-nogenbank"
    };

    compare_annots_args = args;
}


void CProteinMatchApp::x_LogTempFile(const string& filename)
{   
    m_TempFiles.insert(filename);
}


void CProteinMatchApp::x_DeleteTempFiles(void) 
{
    string file_list = " ";
    for (string filename : m_TempFiles) {
        auto tempfile = CFile(filename);
        if (tempfile.Exists()) {
            tempfile.Remove();
        }
    }
}


END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CProteinMatchApp().AppMain(argc, argv);
}

