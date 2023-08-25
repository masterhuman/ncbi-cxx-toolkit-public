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
* Author:  Aaron Ucko, Mati Shomrat, Colleen Bollin, NCBI
*
* File Description:
*   runs ExtendedCleanup on ASN.1 files
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <serial/objectio.hpp>


#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/autodef_with_tax.hpp>
#include <objtools/validator/tax_validation_and_cleanup.hpp>
#include <objtools/validator/dup_feats.hpp>

#include <objtools/edit/huge_file.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/edit/huge_asn_loader.hpp>
#include <objtools/edit/huge_file_process.hpp>

#include <objtools/writers/async_writers.hpp>
#include <objtools/edit/remote_updater.hpp>
#include <objtools/cleanup/huge_file_cleanup.hpp>

#include "read_hooks.hpp"
#include "bigfile_processing.hpp"

#include <common/ncbi_revision.h>

#ifndef NCBI_SC_VERSION
#   define THIS_IS_TRUNK_BUILD
#elif (NCBI_SC_VERSION == 0)
#   define THIS_IS_TRUNK_BUILD
#endif

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static const CDataLoadersUtil::TLoaders default_loaders =
  CDataLoadersUtil::fGenbank | CDataLoadersUtil::fGenbankOffByDefault | CDataLoadersUtil::fVDB;

enum EProcessingMode
{
    eModeRegular,
    eModeBatch,
    eModeBigfile,
    eModeHugefile,
};

struct TThreadState
{
    CRef<CScope>                m_Scope;
    bool                        m_IsMultiSeq = false;
    CCleanupChangeCore          m_changes;
};

class CCleanupApp :
    public CNcbiApplication,
    public CGBReleaseFile::ISeqEntryHandler,
    public ISubmitBlockHandler,
    public IProcessorCallback
{
public:
    CCleanupApp();
    void Init() override;
    int Run() override;

    bool HandleSubmitBlock(CSubmit_block& block) override;
    bool HandleSeqEntry(CRef<CSeq_entry>& se) override;
    bool HandleSeqEntry(CSeq_entry_Handle entry);
    bool HandleSeqID( const string& seqID );

    // IProcessorCallback interface functionality
    void Process(CRef<CSerialObject>& obj) override;

#if 0
    bool ObtainSeqEntryFromSeqEntry(
        unique_ptr<CObjectIStream>& is,
        CRef<CSeq_entry>& se);
    bool ObtainSeqEntryFromBioseq(
        unique_ptr<CObjectIStream>& is,
        CRef<CSeq_entry>& se);
    bool ObtainSeqEntryFromBioseqSet(
        unique_ptr<CObjectIStream>& is,
        CRef<CSeq_entry>& se);
#endif

private:
    // types

    void x_OpenOStream(const string& filename, const string& dir = kEmptyStr, bool remove_orig_dir = true);
    void x_CloseOStream();
    //bool x_ProcessSeqSubmit(unique_ptr<CObjectIStream>& is);
    bool x_ProcessBigFile(unique_ptr<CObjectIStream>& is, TTypeInfo asn_type);
    void x_ProcessOneFile(unique_ptr<CObjectIStream>& is, EProcessingMode mode, TTypeInfo asn_type, bool first_only);
    void x_ProcessOneFile(const string& filename);
    void x_ProcessOneDirectory(const string& dirname, const string& suffix);

    bool x_ProcessHugeFile(edit::CHugeFileProcess& process, bool first_only);
    bool x_ProcessHugeFileBlob(edit::CHugeFileProcess& process);
    CConstRef<CSerialObject> x_ProcessTraditionally(edit::CHugeAsnReader& reader);
    void x_ProcessTraditionally(edit::CHugeFileProcess& process, bool first_only);

    void x_FeatureOptionsValid(const string& opt);
    void x_KOptionsValid(const string& opt);
    void x_XOptionsValid(const string& opt);
    bool x_ProcessFeatureOptions(const string& opt, CSeq_entry_Handle seh);
    bool x_RemoveDuplicateFeatures(CSeq_entry_Handle seh);
    bool x_ProcessXOptions(const string& opt, CSeq_entry_Handle seh, Uint4 options);
    bool x_GFF3Batch(CSeq_entry_Handle seh);
    enum EFixCDSOptions {
        eFixCDS_FrameFromLoc = 0x1,
        eFixCDS_Retranslate = 0x2,
        eFixCDS_ExtendToStop = 0x4
    };
    const Uint4 kGFF3CDSFixOptions = eFixCDS_FrameFromLoc | eFixCDS_Retranslate | eFixCDS_ExtendToStop;

    bool x_FixCDS(CSeq_entry_Handle seh, Uint4 options, const string& missing_prot_name);
    bool x_BatchExtendCDS(CSeq_feat&, CBioseq_Handle);
    bool x_BasicAndExtended(CSeq_entry_Handle entry, const string& label, Uint4 options = 0);

    bool x_ReportChanges(const string_view prefix, CCleanupChangeCore changes);

    //template<typename T> void x_WriteToFile(const T& s);

    // data
    unique_ptr<edit::CRemoteUpdater> m_remote_updater;
    unique_ptr<CObjectOStream>  m_Out;          // output
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    bool                        m_do_basic = false;
    bool                        m_do_extended = false;
    TThreadState                m_state;
    bool                        m_IsHugeSet = false;
};


CCleanupApp::CCleanupApp()
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}

void CCleanupApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Perform ExtendedCleanup on an ASN.1 Seq-entry into a flat report");

    // input
    {{
        // name
        arg_desc->AddOptionalKey("i", "InputFile",
            "Input file name", CArgDescriptions::eInputFile);

        // input file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddOptionalKey("serial", "SerialFormat", "Obsolete; Input file format is now autodetected",
            CArgDescriptions::eString, CArgDescriptions::fHidden);

        // output file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddOptionalKey("outformat", "OutputSerialFormat", "Output file format",
            CArgDescriptions::eString);
        arg_desc->SetConstraint("outformat", &(*new CArgAllow_Strings,
            "text", "binary", "XML", "JSON"));

        // id
        arg_desc->AddOptionalKey("id", "ID",
            "Specific ID to display", CArgDescriptions::eString);

        // input type:
        arg_desc->AddOptionalKey("type", "AsnType", "Obsolete; ASN.1 object type is now autodetected",
            CArgDescriptions::eString, CArgDescriptions::fHidden);

        // path
        arg_desc->AddOptionalKey("indir", "path", "Path to files", CArgDescriptions::eDirectory);

        // suffix
        arg_desc->AddDefaultKey("x", "suffix", "File Selection Suffix", CArgDescriptions::eString, ".ent");

        // results
        arg_desc->AddOptionalKey("outdir", "results", "Path for Results", CArgDescriptions::eDirectory);
    }}

    // batch processing
    {{
        arg_desc->AddFlag("batch", "Process NCBI release file");
        // compression
        arg_desc->AddFlag("c", "Obsolete - do not use",
                          CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);

        // imitate limitation of C Toolkit version
        arg_desc->AddFlag("firstonly", "Process only first element");
    }}

    // big file processing
    {{
        arg_desc->AddFlag("bigfile", "Process big files containing many bioseqs");
    }}

    // output
    {{
        // name
        arg_desc->AddOptionalKey("o", "OutputFile",
            "Output file name", CArgDescriptions::eOutputFile);
    }}

    // normal cleanup options (will replace -nocleanup and -basic)
    {{
        arg_desc->AddOptionalKey("K", "Cleanup", "Systemic Cleaning Options\n"
                                 "\tb Basic\n"
                                 "\ts Extended\n"
                                 "\tn Normalize Descriptor Order\n"
                                 "\tu Remove Cleanup User-object\n",
                                 CArgDescriptions::eString);
    }}

    // extra cleanup options
    {{
        arg_desc->AddOptionalKey("F", "Feature", "Feature Cleaning Options\n"
                                 "\tr Remove Redundant Gene xref\n"
                                 "\ta Adjust for Missing Stop Codon\n"
                                 "\tp Clear internal partials\n"
                                 "\tz Delete or Update EC Numbers\n"
                                 "\td Remove duplicate features\n",
                                  CArgDescriptions::eString);

        arg_desc->AddOptionalKey("X", "Miscellaneous", "Other Cleaning Options\n"
            "\td Automatic Definition Line\n"
            "\tw GFF/WGS Genome Cleanup\n"
            "\tr Regenerate Definition Lines\n"
            "\tb Batch Cleanup of Multireader Output\n"
            "\ta Remove Assembly Gaps\n"
            "\ti Make Influenza Small Genome Sets\n"
            "\tf Make IRD misc_feats\n",
            CArgDescriptions::eString);

        arg_desc->AddFlag("T", "TaxonomyLookup");
    }}

    // misc
    {{
        // no-cleanup
        arg_desc->AddFlag("nocleanup",
            "Do not perform extended data cleanup prior to formatting");
        arg_desc->AddFlag("basic",
            "Perform basic data cleanup prior to formatting");
        arg_desc->AddFlag("noobj",
            "Do not create Ncbi_cleanup object");

        // show progress
        arg_desc->AddFlag("showprogress",
            "List ID for which cleanup is occuring");
        arg_desc->AddFlag("debug", "Save before.sqn");

        arg_desc->AddFlag("huge",
            "Process file in huge files mode");

    }}

    // remote
    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, default_loaders);

    SetupArgDescriptions(arg_desc.release());
}


void CCleanupApp::x_FeatureOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized;
    for (char c : opt) {
        if (!isspace(c)) {
            if (c != 'r' && c != 'a' && c != 'p' && c != 'z' && c != 'd') {
                unrecognized += c;
            }
        }
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CArgException, eInvalidArg, "Invalid -F arguments:" + unrecognized);
    }
}


void CCleanupApp::x_KOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized;
    for (char c : opt) {
        if (!isspace(c)) {
            if (c != 'b' && c != 's' && c != 'u' && c != 'n') {
                unrecognized += c;
            }
        }
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CArgException, eInvalidArg, "Invalid -K arguments:" + unrecognized);
    }
}


void CCleanupApp::x_XOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized;
    for (char c : opt) {
        if (!isspace(c)) {
            if (c != 'w' && c != 'r' && c != 'b' && c != 'a' &&
                c != 'i' && c != 'f' && c != 'd') {
                unrecognized += c;
            }
        }
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CArgException, eInvalidArg, "Invalid -X arguments:" + unrecognized);
    }
}


#if 0

static unique_ptr<CObjectTypeInfo> GetEntryTypeInfo()
{
    // 'data' member of CSeq_submit ...
    CObjectTypeInfo submitTypeInfo = CType<CSeq_submit>();
    CObjectTypeInfoMI data = submitTypeInfo.FindMember("data");

    // points to a container (pointee may has different types) ...
    const CPointerTypeInfo* dataPointerType = CTypeConverter<CPointerTypeInfo>::SafeCast(data.GetMemberType().GetTypeInfo());
    const CChoiceTypeInfo* dataChoiceType = CTypeConverter<CChoiceTypeInfo>::SafeCast(dataPointerType->GetPointedType());

    // that is a list of pointers to 'CSeq_entry' (we process only that case)
    const CItemInfo* entries = dataChoiceType->GetItemInfo("entrys");

    return unique_ptr<CObjectTypeInfo>(new CObjectTypeInfo(entries->GetTypeInfo()));
}


static CObjectTypeInfoMI GetSubmitBlockTypeInfo()
{
    CObjectTypeInfo submitTypeInfo = CType<CSeq_submit>();
    return submitTypeInfo.FindMember("sub");
}


static void CompleteOutputFile(CObjectOStream& out)
{
    out.EndContainer(); // ends the list of entries

    out.EndChoiceVariant(); // ends 'entrys'
    out.PopFrame();
    out.EndChoice(); // ends 'data'
    out.PopFrame();

    out.EndClass(); // ends 'CSeq_submit'

    Separator(out);

    fflush(NULL);
}

// returns false if fails to read object of expected type, throws for all other errors
bool CCleanupApp::x_ProcessSeqSubmit(unique_ptr<CObjectIStream>& is)
{
    CRef<CSeq_submit> sub(new CSeq_submit);
    if (sub.Empty()) {
        NCBI_THROW(CFlatException, eInternal,
            "Could not allocate Seq-submit object");
    }
    try {

        CObjectTypeInfoMI submitBlockObj = GetSubmitBlockTypeInfo();
        submitBlockObj.SetLocalReadHook(*is, new CReadSubmitBlockHook(*this, *m_Out));

        unique_ptr<CObjectTypeInfo> entryObj = GetEntryTypeInfo();
        entryObj->SetLocalReadHook(*is, new CReadEntryHook(*this, *m_Out));

        *is >> *sub;

        entryObj->ResetLocalReadHook(*is);
        submitBlockObj.ResetLocalReadHook(*is);

        CompleteOutputFile(*m_Out);
    }
    catch (const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch (...) {
        return false;
    }

    if (!sub->IsSetSub() || !sub->IsSetData()) {
        NCBI_THROW(CFlatException, eInternal, "No data in Seq-submit");
    }
    else if (!sub->GetData().IsEntrys()) {
        NCBI_THROW(CFlatException, eInternal, "Wrong data in Seq-submit");
    }

    return true;
}
#endif

bool CCleanupApp::x_ProcessBigFile(unique_ptr<CObjectIStream>& is, TTypeInfo asn_type)
{
    EBigFileContentType content_type = eContentUndefined;
    if (asn_type == CSeq_entry::GetTypeInfo()) {
        content_type = eContentSeqEntry;
    }
    else if (asn_type == CBioseq_set::GetTypeInfo()) {
        content_type = eContentBioseqSet;
    }
    else if (asn_type == CSeq_submit::GetTypeInfo()) {
        content_type = eContentSeqSubmit;
    } else {
        _ASSERT(0);
    }

    return ProcessBigFile(*is, *m_Out, *this, content_type);
}

void CCleanupApp::x_ProcessOneFile(unique_ptr<CObjectIStream>& is, EProcessingMode mode, TTypeInfo asn_type, bool first_only)
{
    if (mode == eModeBatch) {
        CGBReleaseFile in(*is.release());
        in.RegisterHandler([this](CRef<CSeq_entry>& entry) -> bool
        {
            return HandleSeqEntry(entry);
        });
        in.Read();  // HandleSeqEntry will be called from this function
    }
    else if (mode == eModeBigfile) {
        x_ProcessBigFile(is, asn_type);
    }
#if 0
    else {
        bool proceed = true;
        size_t num_cleaned = 0;

        while (proceed) {

            // clean exit
            if (is->EndOfData()) {
                is->SetFailFlags(CObjectIStream::eEOF);
                break;
            }

            CRef<CSeq_entry> se(new CSeq_entry);

            if (asn_type == CSeq_entry::GetTypeInfo() ) {
                //
                //  Straight through processing: Read a seq_entry, then process
                //  a seq_entry:
                //
                proceed = ObtainSeqEntryFromSeqEntry(is, se);
                if (proceed) {
                    HandleSeqEntry(se);
                    x_WriteToFile(*se);
                }
            } else if (asn_type == CBioseq::GetTypeInfo()) {
                //
                //  Read object as a bioseq, wrap it into a seq_entry, then process
                //  the wrapped bioseq as a seq_entry:
                //
                proceed = ObtainSeqEntryFromBioseq(is, se);
                if (proceed) {
                    HandleSeqEntry(se);
                    if (se->IsSeq()) {
                        x_WriteToFile(se->GetSeq());
                    }
                    else {
                        x_WriteToFile(se->GetSet());
                    }
                }
            } else if (asn_type == CBioseq_set::GetTypeInfo()) {
                //
                //  Read object as a bioseq_set, wrap it into a seq_entry, then
                //  process the wrapped bioseq_set as a seq_entry:
                //
                proceed = ObtainSeqEntryFromBioseqSet(is, se);
                if (proceed) {
                    HandleSeqEntry(se);
                    if (se->IsSet()) {
                        x_WriteToFile(se->GetSet());
                    }
                    else {
                        x_WriteToFile(se->GetSeq());
                    }
                }
            } else if (asn_type == CSeq_submit::GetTypeInfo()) {
                proceed = x_ProcessSeqSubmit(is);
            } else {
                proceed = false;
            }

            if (proceed) {
                ++num_cleaned;
            }

            if (first_only) {
                break;
            }
        }

        if (num_cleaned == 0 || (!first_only && (is->GetFailFlags() & CObjectIStream::fEOF) != CObjectIStream::fEOF)) {
            NCBI_THROW(CFlatException, eInternal, "Unable to construct Seq-entry object");
        }
    }
#endif
}

void CCleanupApp::x_ProcessOneFile(const string& filename)
{
    const CArgs& args = GetArgs();

    m_state = TThreadState();
    m_state.m_Scope.Reset(new CScope(*m_Objmgr));
    m_state.m_Scope->AddDefaults();

    _ASSERT(!NStr::IsBlank(filename));

    if (args["type"]) {
        cerr << "Warning: -type argument should not be used; ASN.1 object type is now autodetected." << endl;
    }
    if (args["serial"]) {
        cerr << "Warning: -serial argument should not be used; Input file format is now autodetected." << endl;
    }


    CCleanupHugeAsnReader::TOptions options{0};
    if (m_do_extended) {
        options |= CCleanupHugeAsnReader::eExtendedCleanup;
    }
    if (args["noobj"]) {
        options |= CCleanupHugeAsnReader::eNoNcbiUserObjects;
    }

    if (args["X"] && (NStr::Find(args["X"].AsString(), "i") != NPOS)) {
        options |= CCleanupHugeAsnReader::eEnableSmallGenomeSets;
    }

    edit::CHugeFileProcess huge_process(new CCleanupHugeAsnReader(options));
    huge_process.OpenFile(filename);

    TTypeInfo asn_type = huge_process.GetFile().m_content;

    if (asn_type == nullptr) {
        string msg = "Unable to open input file " + filename + ". Content not recognized.";
        NCBI_THROW(CArgException, eInvalidArg, msg);
    }

    // need to set output if -o not specified
    bool opened_output = false;

    if (!args["o"] && args["outdir"]) {
        x_OpenOStream(filename, args["outdir"].AsString());
        opened_output = true;
    }

    EProcessingMode mode = eModeRegular;
    m_state.m_IsMultiSeq = false;
    bool is_Xi = args["X"] && (NStr::Find(args["X"].AsString(), "i") != string::npos);
    // "-X i" is incompatible with -huge mode, until fixed, always use Regular
    if (asn_type == CBioseq::GetTypeInfo()) {
        // always regular mode
        mode = eModeRegular;
    } else if (args["huge"]) {
      //  if (!is_Xi)
            mode = eModeHugefile;
    } else if (args["batch"]) {
        mode = eModeBatch;
    } else if (args["bigfile"]) {
        mode = eModeBigfile;
    }

    if (mode == eModeHugefile) {
        huge_process.OpenReader();
        x_ProcessHugeFile(huge_process, args["firstonly"]);
    } else
    if (mode == eModeRegular) {
        huge_process.OpenReader();
        x_ProcessTraditionally(huge_process, args["firstonly"]);
    } else {
        unique_ptr<CObjectIStream> is = huge_process.GetFile().MakeObjStream(0);
        x_ProcessOneFile(is, mode, asn_type, args["firstonly"]);
    }

    m_state.m_changes += dynamic_cast<CCleanupHugeAsnReader&>(huge_process.GetReader()).GetChanges();

    if (opened_output) {
        // close output file if we opened one
        x_CloseOStream();
    }
}

void CCleanupApp::x_ProcessTraditionally(edit::CHugeFileProcess& process, bool first_only)
{
    bool proceed = true;
    size_t num_cleaned = 0;
    auto reader = process.GetReader();

    while (proceed) {

        auto anytop = x_ProcessTraditionally(reader);
        proceed = anytop;

        if (anytop) {
            *m_Out << *anytop;
        }

        if (proceed) {
            ++num_cleaned;
        }

        if (first_only) {
            break;
        }
    }

    //if (num_cleaned == 0 || (!first_only && (is->GetFailFlags() & CObjectIStream::fEOF) != CObjectIStream::fEOF)) {
    //    NCBI_THROW(CArgException, eInvalidArg, "Unable to construct Seq-entry object");
    //}
}

CConstRef<CSerialObject> CCleanupApp::x_ProcessTraditionally(edit::CHugeAsnReader& reader)
{
    auto anytop = reader.ReadAny();
    if (anytop.Empty())
        return {};

    CConstRef<CSerialObject> topobject;
    CRef<CSeq_entry>  topentry;

    if (anytop->GetThisTypeInfo() == CSeq_entry::GetTypeInfo()) {
        topentry.Reset(CTypeConverter<CSeq_entry>::SafeCast(anytop));
        HandleSeqEntry(topentry);
        topobject = topentry;
    } else
    if (anytop->GetThisTypeInfo() == CSeq_submit::GetTypeInfo()) {
        auto submit = Ref(CTypeConverter<CSeq_submit>::SafeCast(anytop));
        if (submit->IsEntrys()) {
            topentry = submit->SetData().SetEntrys().front();
            if (submit->IsSetSub()) {
                HandleSubmitBlock(submit->SetSub());
            }
            submit->SetData().SetEntrys().clear();
            HandleSeqEntry(topentry);
            submit->SetData().SetEntrys().push_back(topentry);
        }
        topobject = submit;
    } else
    if (anytop->GetThisTypeInfo() == CBioseq_set::GetTypeInfo()) {
        auto bioset = Ref(CTypeConverter<CBioseq_set>::SafeCast(anytop));
        topentry = Ref(new CSeq_entry);
        topentry->SetSet(*bioset);
        bioset.Reset();
        HandleSeqEntry(topentry);
        if (topentry->IsSet())
            topobject.Reset(&topentry->GetSet());
        else
            topobject.Reset(&topentry->GetSeq());
    } else
    if (anytop->GetThisTypeInfo() == CBioseq::GetTypeInfo()) {
        auto bioseq = Ref(CTypeConverter<CBioseq>::SafeCast(anytop));
        topentry = Ref(new CSeq_entry);
        topentry->SetSeq(*bioseq);
        bioseq.Reset();
        HandleSeqEntry(topentry);
        if (topentry->IsSet())
            topobject.Reset(&topentry->GetSet());
        else
            topobject.Reset(&topentry->GetSeq());
    } else {
        //_ASSERT(0);
    }
    return topobject;
}

bool CCleanupApp::x_ProcessHugeFileBlob(edit::CHugeFileProcess& process)
{
    CGenBankAsyncWriterEx<CConstRef<CSeq_entry>> writer(m_Out.get());

    CConstRef<CSerialObject> topobject; // top object is used to write output, can be submit, entry, bioseq, bioseq_set
    CRef<CSeq_submit> submit;
    CRef<CSeq_entry>  topentry;

    _ASSERT(m_state.m_IsMultiSeq);

    topentry = Ref(new CSeq_entry);
    auto& reader = dynamic_cast<CCleanupHugeAsnReader&>(process.GetReader());

    if (reader.GetTopEntry()) {
        topentry->Assign(*reader.GetTopEntry());
    } else {
        topentry->SetSet().SetClass() = CBioseq_set::eClass_genbank;
        topentry->SetSet().SetSeq_set().clear();
    }
    m_IsHugeSet = (topentry->GetSet().GetClass() == CBioseq_set::eClass_genbank);

    if (reader.GetSubmitBlock())
    {
        submit.Reset(new CSeq_submit);
        submit->SetSub().Assign(*reader.GetSubmitBlock());

        submit->SetData().SetEntrys().clear();
        submit->SetData().SetEntrys().push_back(topentry);
        HandleSubmitBlock(submit->SetSub());
    }


    if (submit)
        topobject = submit;
    else
        topobject = topentry;

    writer.StartWriter(topobject);
    try
    {

        bool proceed = process.ForEachEntry (m_state.m_Scope,
            [this, &writer] (CSeq_entry_Handle seh) -> bool
            {
                HandleSeqEntry(seh.GetEditHandle());
                writer.PushNextEntry(seh.GetCompleteSeq_entry());
                return true;
            });
        writer.FinishWriter();

        return proceed;
    }
    catch(...)
    {
        writer.CancelWriter();
        throw;
    }


}

bool CCleanupApp::x_ProcessHugeFile(edit::CHugeFileProcess& process, bool first_only)
{
    return process.ForEachBlob([this, first_only](edit::CHugeFileProcess& p_process) -> bool
    {
        m_state.m_IsMultiSeq = p_process.GetReader().IsMultiSequence();
        if (m_state.m_IsMultiSeq) {

            bool proceed = x_ProcessHugeFileBlob(p_process);
            if (!proceed)
                return false;
        } else {
            auto topobject = x_ProcessTraditionally(p_process.GetReader());
            m_Out->ResetLocalHooks();
            *m_Out << *topobject;
        }

        return !first_only;
    });
}


void CCleanupApp::x_ProcessOneDirectory(const string& dirname, const string& suffix)
{
    CDir dir(dirname);

    string mask = "*" + suffix;
    size_t num_files = 0;

    CDir::TEntries files(dir.GetEntries(mask, CDir::eFile));
    for (CDir::TEntry ii : files) {
        if (ii->IsFile()) {
            string fname = CDirEntry::MakePath(dirname, ii->GetName());
            x_ProcessOneFile(fname);
            num_files++;
        }
    }
    if (num_files == 0) {
        NCBI_THROW(CArgException, eInvalidArg, "No files found!");
    }
}


int CCleanupApp::Run()
{
    // initialize conn library
    CONNECT_Init(&GetConfig());

    const CArgs& args = GetArgs();

    // flag validation
    if (args["F"]) {
        x_FeatureOptionsValid(args["F"].AsString());
    }
    if (args["K"]) {
        x_KOptionsValid(args["K"].AsString());
    }
    if (args["X"]) {
        x_XOptionsValid(args["X"].AsString());
    }
    if (args["batch"] && args["bigfile"]) {
        NCBI_THROW(CArgException, eInvalidArg, "\"batch\" and \"bigfile\" arguments are incompatible. Only one of them may be used.");
    }
    if (args["X"] && args["bigfile"]) {
        NCBI_THROW(CArgException, eInvalidArg, "\"X\" and \"bigfile\" arguments are incompatible. Only one of them may be used.");
    }

    if (args["K"]) {
        if (NStr::Find(args["K"].AsString(), "b") != string::npos) {
            m_do_basic = true;
        }
        if (NStr::Find(args["K"].AsString(), "s") != string::npos) {
            m_do_basic = true;
            m_do_extended = true;
        }
    }
    else if (args["X"]) {
        m_do_basic = true;
        if (NStr::Find(args["X"].AsString(), "w") != string::npos) {
            //Extended Cleanup is part of -X w
            m_do_extended = false;
        }
    } else if (args["F"]) {
        m_do_basic = true;
    } else {
        if (args["basic"]) {
            m_do_basic = true;
        }
        if (!args["nocleanup"]) {
            m_do_extended = true;
        }
    }

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CArgException, eInvalidArg, "Could not create object manager");
    }

    CDataLoadersUtil::SetupObjectManager(args, *m_Objmgr, default_loaders);

    m_remote_updater.reset(new edit::CRemoteUpdater(nullptr));

    // need to set output (-o) if specified, if not -o and not -outdir need to use standard output
    bool opened_output = false;
    if (args["o"]) {
        string abs_output_path = CDirEntry::CreateAbsolutePath(args["o"].AsString());
        if (args["i"]) {
            string fname = args["i"].AsString();
            if (args["indir"]) {
                fname = CDirEntry::MakePath(args["indir"].AsString(), fname);
            }
            if (abs_output_path == CDirEntry::CreateAbsolutePath(fname)) {
                ERR_POST("Input and output files should be different");
                return 1;
            }
        }
        x_OpenOStream(args["o"].AsString(),
                      args["outdir"] ? args["outdir"].AsString() : kEmptyStr,
                      false);
        opened_output = true;
    } else if (!args["outdir"] || args["id"]) {
        x_OpenOStream(kEmptyStr);
        opened_output = true;
    }

    if (args["id"]) {
        string seqID = args["id"].AsString();
        HandleSeqID(seqID);
    } else if (args["i"]) {
        string fname = args["i"].AsString();
        if (args["indir"]) {
            fname = CDirEntry::MakePath(args["indir"].AsString(), fname);
        }
        x_ProcessOneFile(fname);
    } else if (args["outdir"]) {
        x_ProcessOneDirectory(args["indir"].AsString(), args["x"].AsString());
    } else {
        cerr << "Error: stdin is no longer supported; please use -i" << endl;
    }

    if (opened_output) {
        // close output file if we opened one
        x_CloseOStream();
    }

    if (m_do_basic && !m_do_extended)
        x_ReportChanges("BasicCleanup", m_state.m_changes);
    if (m_do_extended)
        x_ReportChanges("ExtendedCleanup",  m_state.m_changes);

    #ifdef THIS_IS_TRUNK_BUILD
        m_remote_updater->ReportStats(std::cerr);
    #endif

    return 0;
}

#if 0
bool CCleanupApp::ObtainSeqEntryFromSeqEntry(
    unique_ptr<CObjectIStream>& is,
    CRef<CSeq_entry>& se )
{
    try {
        *is >> *se;
        if (se->Which() == CSeq_entry::e_not_set) {
            return false;
        }
        return true;
    }
    catch(const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch( ... ) {
        return false;
    }
}

bool CCleanupApp::ObtainSeqEntryFromBioseq(
    unique_ptr<CObjectIStream>& is,
    CRef<CSeq_entry>& se )
{
    try {
        CRef<CBioseq> bs( new CBioseq );
        if ( ! bs ) {
            NCBI_THROW(CFlatException, eInternal,
            "Could not allocate Bioseq object");
        }
        *is >> *bs;

        se->SetSeq( bs.GetObject() );
        return true;
    }
    catch (const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch( ... ) {
        return false;
    }
}

bool CCleanupApp::ObtainSeqEntryFromBioseqSet(
    unique_ptr<CObjectIStream>& is,
    CRef<CSeq_entry>& se )
{
    try {
        CRef<CBioseq_set> bss( new CBioseq_set );
        if ( ! bss ) {
            NCBI_THROW(CFlatException, eInternal,
            "Could not allocate Bioseq object");
        }
        *is >> *bss;

        se->SetSet( bss.GetObject() );
        return true;
    }
    catch (const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch( ... ) {
        return false;
    }
}
#endif

bool CCleanupApp::HandleSeqID( const string& seq_id )
{
    CRef<CScope> scope(new CScope(*m_Objmgr));
    scope->AddDefaults();

    CBioseq_Handle bsh;
    try {
        CSeq_id SeqId(seq_id);
        bsh = scope->GetBioseqHandle(SeqId);
    }
    catch ( CException& ) {
        ERR_FATAL("The ID " << seq_id << " is not a valid seq ID." );
    }

    if (!bsh) {
        ERR_FATAL("Sequence for " << seq_id << " cannot be retrieved.");
        return false;
    }

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->Assign(*(bsh.GetSeq_entry_Handle().GetCompleteSeq_entry()));
    HandleSeqEntry(entry);
    *m_Out << *entry;

    return true;
}

bool CCleanupApp::x_ProcessFeatureOptions(const string& opt, CSeq_entry_Handle seh)
{
    if (NStr::IsBlank(opt)) {
        return false;
    }
    bool any_changes = false;
    if (NStr::Find(opt, "r") != string::npos) {
        any_changes |= CCleanup::RemoveUnnecessaryGeneXrefs(seh);
    }
    if (NStr::Find(opt, "a") != string::npos) {
        any_changes |= x_FixCDS(seh, eFixCDS_ExtendToStop, kEmptyStr);
    }
    if (NStr::Find(opt, "p") != string::npos) {
        any_changes |= CCleanup::ClearInternalPartials(seh);
    }
    if (NStr::Find(opt, "z") != string::npos) {
        any_changes |= CCleanup::FixECNumbers(seh);
    }
    if (NStr::Find(opt, "d") != string::npos) {
        any_changes |= x_RemoveDuplicateFeatures(seh);
    }
    return any_changes;
}

bool CCleanupApp::x_RemoveDuplicateFeatures(CSeq_entry_Handle seh)
{
    bool any_change = false;
    set< CSeq_feat_Handle > deleted_feats = validator::GetDuplicateFeaturesForRemoval(seh);
    if (deleted_feats.empty()) {
        return false;
    }
    set< CBioseq_Handle > orphans = validator::ListOrphanProteins(seh);
    for (auto df : deleted_feats) {
        CSeq_feat_EditHandle eh(df);
        eh.Remove();
        any_change = true;
    }
    for (auto orph : orphans) {
        CBioseq_EditHandle eh(orph);
        eh.Remove();
        any_change = true;
    }
    any_change |= CCleanup::RenormalizeNucProtSets(seh);
    return any_change;

}

bool CCleanupApp::x_ProcessXOptions(const string& opt, CSeq_entry_Handle seh, Uint4 options)
{
    bool any_changes = false;
    if (NStr::Find(opt, "w") != string::npos) {
        any_changes = CCleanup::WGSCleanup(seh, true, options);
    }
    if (NStr::Find(opt, "r") != string::npos) {
        bool change_defline = CAutoDefWithTaxonomy::RegenerateDefLines(seh);
        if (change_defline) {
            any_changes = true;
            CCleanup::NormalizeDescriptorOrder(seh);
        }
    }
    if (NStr::Find(opt, "b") != string::npos) {
        any_changes |= x_GFF3Batch(seh);
    }
    if (NStr::Find(opt, "a") != string::npos) {
        any_changes |= CCleanup::ConvertDeltaSeqToRaw(seh);
    }
    if (!m_IsHugeSet && (NStr::Find(opt, "i") != string::npos)) {
        if (CCleanup::MakeSmallGenomeSet(seh) > 0) {
            any_changes = true;
        }
    }
    if (NStr::Find(opt, "f") != string::npos) {
        if (CCleanup::MakeIRDFeatsFromSourceXrefs(seh)) {
            any_changes = true;
        }
    }
    if (NStr::Find(opt, "d") != string::npos) {
        CCleanup::AutodefId(seh);
        any_changes = true;
    }
    return any_changes;
}


bool CCleanupApp::x_BatchExtendCDS(CSeq_feat& sf, CBioseq_Handle b)
{
    if (!sf.GetData().IsCdregion()) {
        // not coding region
        return false;
    }
    if (sequence::IsPseudo(sf, b.GetScope())) {
        return false;
    }

    // check for existing stop codon
    string translation;
    try {
        CSeqTranslator::Translate(sf, b.GetScope(), translation, true);
    } catch (CSeqMapException& e) {
        cout << e.what() << endl;
        return false;
    } catch (CSeqVectorException& e) {
        cout << e.what() << endl;
        return false;
    }
    if (NStr::EndsWith(translation, "*")) {
        //already has stop codon
        return false;
    }

    if (CCleanup::ExtendToStopCodon(sf, b, 50)) {
        feature::RetranslateCDS(sf, b.GetScope());
        return true;
    } else {
        return false;
    }
}


bool CCleanupApp::x_FixCDS(CSeq_entry_Handle seh, Uint4 options, const string& missing_prot_name)
{
    bool any_changes = false;
    for (CBioseq_CI bi(seh, CSeq_inst::eMol_na); bi; ++bi) {
        any_changes |= CCleanup::SetGeneticCodes(*bi);
        for (CFeat_CI fi(*bi, CSeqFeatData::eSubtype_cdregion); fi; ++fi) {
            CConstRef<CSeq_feat> orig = fi->GetSeq_feat();
            CRef<CSeq_feat> sf(new CSeq_feat());
            sf->Assign(*orig);
            bool feat_change = false;
            if ((options & eFixCDS_FrameFromLoc) &&
                CCleanup::SetFrameFromLoc(sf->SetData().SetCdregion(), sf->GetLocation(), bi.GetScope())) {
                feat_change = true;
            }
            if ((options & eFixCDS_Retranslate)) {
                feat_change |= feature::RetranslateCDS(*sf, bi.GetScope());
            }
            if ((options & eFixCDS_ExtendToStop) &&
                x_BatchExtendCDS(*sf, *bi)) {
                CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(*orig, seh.GetScope());
                if (mrna && CCleanup::LocationMayBeExtendedToMatch(mrna->GetLocation(), sf->GetLocation())) {
                    CRef<CSeq_feat> new_mrna(new CSeq_feat());
                    new_mrna->Assign(*mrna);
                    if (CCleanup::ExtendStopPosition(*new_mrna, sf)) {
                        CSeq_feat_EditHandle efh(seh.GetScope().GetSeq_featHandle(*mrna));
                        efh.Replace(*new_mrna);
                    }
                }
                CConstRef<CSeq_feat> gene = sequence::GetGeneForFeature(*orig, seh.GetScope());
                if (gene && CCleanup::LocationMayBeExtendedToMatch(gene->GetLocation(), sf->GetLocation())) {
                    CRef<CSeq_feat> new_gene(new CSeq_feat());
                    new_gene->Assign(*gene);
                    if (CCleanup::ExtendStopPosition(*new_gene, sf)) {
                        CSeq_feat_EditHandle efh(seh.GetScope().GetSeq_featHandle(*gene));
                        efh.Replace(*new_gene);
                    }
                }

                feat_change = true;
            }
            if (feat_change) {
                CSeq_feat_EditHandle ofh = CSeq_feat_EditHandle(seh.GetScope().GetSeq_featHandle(*orig));
                ofh.Replace(*sf);
                any_changes = true;
            }
            //also set protein name if missing, change takes place on protein bioseq
            if (!NStr::IsBlank(missing_prot_name)) {
                string current_name = CCleanup::GetProteinName(*sf, seh);
                if (NStr::IsBlank(current_name)) {
                    CCleanup::SetProteinName(*sf, missing_prot_name, false, seh.GetScope());
                    any_changes = true;
                }
            }
        }
    }
    return any_changes;
}


bool CCleanupApp::x_GFF3Batch(CSeq_entry_Handle seh)
{
    bool any_changes = x_FixCDS(seh, kGFF3CDSFixOptions, kEmptyStr);
    CCleanup cleanup;
    cleanup.SetScope(&(seh.GetScope()));
    Uint4 options = CCleanup::eClean_NoNcbiUserObjects;
    auto changes = cleanup.BasicCleanup(seh, options);
    any_changes |= (!changes.Empty());
    changes = cleanup.ExtendedCleanup(seh, options);
    any_changes |= (!changes.Empty());
    any_changes |= x_FixCDS(seh, 0, "unnamed protein product");

    return any_changes;
}


bool CCleanupApp::x_BasicAndExtended(CSeq_entry_Handle entry, const string& label, Uint4 options)
{
    if (!m_do_basic && !m_do_extended) {
        return false;
    }

    bool any_changes = false;
    CCleanup cleanup;
    cleanup.SetScope(&(entry.GetScope()));

    if (m_state.m_IsMultiSeq) {
        options |= ( CCleanup::eClean_KeepTopSet | CCleanup::eClean_KeepSingleSeqSet);
        //if (submit)
        //options |= CCleanup::eScope_UseInPlace; // RW-1070 - CCleanup::eScope_UseInPlace is essential
    }

    if (m_do_basic && !m_do_extended) {
        // perform BasicCleanup
        try {
            auto changes = *cleanup.BasicCleanup(entry, options);
            m_state.m_changes += changes;
            any_changes = !changes.Empty();
        }
        catch (CException& e) {
            LOG_POST(Error << "error in basic cleanup: " << e.GetMsg() << label);
        }
    }

    if (m_do_extended) {
        // perform ExtendedCleanup
        try {
            auto changes = *cleanup.ExtendedCleanup(entry, options);
            m_state.m_changes += changes;
            any_changes = !changes.Empty();
        }
        catch (CException& e) {
            LOG_POST(Error << "error in extended cleanup: " << e.GetMsg() << label);
        }
    }
    return any_changes;
}



bool CCleanupApp::HandleSubmitBlock(CSubmit_block& block)
{
    CCleanup cleanup;
    bool any_changes = false;
    try {
        auto changes = *cleanup.BasicCleanup(block);
        any_changes = x_ReportChanges("BasicCleanup of SubmitBlock", changes);
    } catch (CException& e) {
        LOG_POST(Error << "error in cleanup of SubmitBlock: " << e.GetMsg());
    }
    return any_changes;
}


bool CCleanupApp::HandleSeqEntry(CSeq_entry_Handle entry)
{
    string label;
    entry.GetCompleteSeq_entry()->GetLabel(&label, CSeq_entry::eBoth);

    const CArgs& args = GetArgs();

    if (args["showprogress"]) {
        LOG_POST(Error << label + "\n");
    }

    if (args["debug"]) {
        ESerialDataFormat outFormat = eSerial_AsnText;

        unique_ptr<CObjectOStream> debug_out(CObjectOStream::Open(outFormat, "before.sqn",
            eSerial_StdWhenAny));

        *debug_out << *(entry.GetCompleteSeq_entry());
    }

    bool any_changes = false;

    if (args["T"]) {
        validator::CTaxValidationAndCleanup tval(m_remote_updater->GetUpdateFunc());
        any_changes |= tval.DoTaxonomyUpdate(entry, true);
    }

    if (args["K"] && NStr::Find(args["K"].AsString(), "u") != string::npos) {
        CRef<CSeq_entry> se(const_cast<CSeq_entry *>(entry.GetCompleteSeq_entry().GetPointer()));
        any_changes |= CCleanup::RemoveNcbiCleanupObject(*se);
    }

    Uint4 options = 0;
    if (args["noobj"]) {
        options = CCleanup::eClean_NoNcbiUserObjects;
    }
    if (m_IsHugeSet) {
        options |= CCleanup::eClean_InHugeSeqSet;
    }

    any_changes |= x_BasicAndExtended(entry, label, options);

    if (args["F"]) {
        any_changes |= x_ProcessFeatureOptions(args["F"].AsString(), entry);
    }
    if (args["X"]) {
        any_changes |= x_ProcessXOptions(args["X"].AsString(), entry, options);
    }
    if (args["K"] && NStr::Find(args["K"].AsString(), "n") != string::npos && !m_do_extended) {
        any_changes |= CCleanup::NormalizeDescriptorOrder(entry);
    }

    return true;
}

bool CCleanupApp::HandleSeqEntry(CRef<CSeq_entry>& se)
{
    if (!se) {
        return false;
    }

    auto entryHandle = m_state.m_Scope->AddTopLevelSeqEntry(*se);
    if (!entryHandle) {
        NCBI_THROW(CArgException, eInvalidArg, "Failed to insert entry to scope.");
    }

    if (HandleSeqEntry(entryHandle)) {
        if (entryHandle.GetCompleteSeq_entry().GetPointer() != se.GetPointer()) {
            se->Assign(*entryHandle.GetCompleteSeq_entry());
        }
        m_state.m_Scope->RemoveTopLevelSeqEntry(entryHandle);
        return true;
    }
    m_state.m_Scope->RemoveTopLevelSeqEntry(entryHandle);
    return false;
}

void CCleanupApp::x_OpenOStream(const string& filename, const string& dir, bool remove_orig_dir)
{
    ESerialDataFormat outFormat = eSerial_AsnText;

    const CArgs& args = GetArgs();
    if (args["outformat"]) {
        if (args["outformat"].AsString() == "binary") {
            outFormat = eSerial_AsnBinary;
        }
        else if (args["outformat"].AsString() == "XML") {
            outFormat = eSerial_Xml;
        }
        else if (args["outformat"].AsString() == "JSON") {
            outFormat = eSerial_Json;
        }
    }

    if (NStr::IsBlank(filename)) {
        m_Out.reset(CObjectOStream::Open(outFormat, cout));
    } else if (!NStr::IsBlank(dir)) {
        string base = filename;
        if (remove_orig_dir) {
            const char buf[2] = { CDirEntry::GetPathSeparator(), 0 };
            size_t pos = NStr::Find(base, buf, NStr::eCase, NStr::eReverseSearch);
            if (pos != string::npos) {
                base = base.substr(pos + 1);
            }
        }
        string fname = CDirEntry::MakePath(dir, base);
        m_Out.reset(CObjectOStream::Open(outFormat, fname, eSerial_StdWhenAny));
    } else {
        m_Out.reset(CObjectOStream::Open(outFormat, filename, eSerial_StdWhenAny));
    }
}


void CCleanupApp::x_CloseOStream()
{
    m_Out->Close();
    m_Out.reset();
}

// IProcessorCallback interface functionality
void CCleanupApp::Process(CRef<CSerialObject>& obj)
{
    //static long long cnt;
    //cerr << ++cnt << ' ' << obj->GetThisTypeInfo()->GetName() << '\n';
    if (obj->GetThisTypeInfo() == CSeq_entry::GetTypeInfo()) {
        CRef<CSeq_entry> entry(dynamic_cast<CSeq_entry*>(obj.GetPointer()));
        HandleSeqEntry(entry);
    }
}

bool CCleanupApp::x_ReportChanges(const string_view prefix, CCleanupChangeCore changes)
{
    bool any_changes = false;
    auto changes_str = changes.GetDescriptions();
    if (changes_str.empty()) {
        LOG_POST(Error << "No changes from " << prefix << "\n");
    }
    else {
        LOG_POST(Error << "Changes from " << prefix << ":\n");
        for (auto it : changes_str) {
            LOG_POST(Error << it);
        }
        any_changes = true;
    }
    return any_changes;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    // scan and replace deprecated arguments; RW-1324
    for (int i = 1; i < argc; ++i)
    {
        string a = argv[i];
        if (a == "-r")
        {
            if ((i+1) < argc)
            {
                string param = argv[i+1];
                if (!param.empty() && param[0] != '-')
                {
                    argv[i] = "-outdir";
                    ++i; // skip parameter
                    cerr << "Warning: deprecated use of -r argument. Please use -outdir instead." << endl;
                }
            }
        }
        else if (a == "-p")
        {
            argv[i] = "-indir";
            cerr << "Warning: argument -p is deprecated. Please use -indir instead." << endl;
        }
        else if (a == "-R")
        {
            argv[i] = "-r";
            cerr << "Warning: argument -R is deprecated. Please use -r instead." << endl;
        }
        else if (a == "-gbload")
        {
            argv[i] = "-genbank";
            cerr << "Warning: argument -gbload is deprecated. Please use -genbank instead." << endl;
        }
    }

    #ifdef _DEBUG
    // this code converts single argument into multiple, just to simplify testing
    list<string> split_args;
    vector<const char*> new_argv;

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' '))
    {
        NStr::Split(argv[1], " ", split_args);

        auto it = split_args.begin();
        while (it != split_args.end())
        {
            auto next = it; ++next;
            if (next != split_args.end() &&
                ((it->front() == '"' && it->back() != '"') ||
                 (it->front() == '\'' && it->back() != '\'')))
            {
                it->append(" "); it->append(*next);
                next = split_args.erase(next);
            } else it = next;
        }
        for (auto& rec: split_args)
        {
            if (rec.front()=='\'' && rec.back()=='\'')
                rec=rec.substr(1, rec.length()-2);
        }
        argc = 1 + split_args.size();
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (const string& s : split_args)
        {
            new_argv.push_back(s.c_str());
            std::cerr << s.c_str() << " ";
        }
        std::cerr << "\n";


        argv = new_argv.data();
    }
    #endif

    return CCleanupApp().AppMain(argc, argv);
}
