/* flat2asn.cpp
 *
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
 * File Name:  flat2asn.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Main routines for parsing flat files to ASN.1 file format.
 * Available flat file format are GENBANK (LANL), EMBL, SWISS-PROT.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/objostrasn.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/fta_parse_buf.h>
#include <objtools/logging/listener.hpp>

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "flat2asn.cpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using TConfig = Parser;

/**********************************************************/
static bool InitConfig(const CArgs& args, Parser& config)
{
    bool retval = true;

    /* Ignore multiple tokens in DDBJ's VERSION line
     */
    config.ign_toks = args["c"].AsBoolean();

    /* Do not reject records if protein Seq-ids are from sources
     * that differ from that of the "-s" argument
     */
    config.ign_prot_src = args["j"].AsBoolean();

    if (args["m"].AsInteger() == 1) {
        config.mode = Parser::EMode::HTGS;
    }
    else if (args["m"].AsInteger() == 2) {
        config.mode = Parser::EMode::HTGSCON;
    }
    else if (args["m"].AsInteger() == 3) {
        config.mode = Parser::EMode::Relaxed;
    }
    else {
        config.mode = Parser::EMode::Release;
    }

    /* replace update by currdate
     */
    config.date = args["L"].AsBoolean();

    config.allow_uwsec = args["w"].AsBoolean();
    config.convert = true;
    config.seg_acc = args["I"].AsBoolean();
    config.no_date = args["N"].AsBoolean();
    config.diff_lt = args["Y"].AsBoolean();
    config.xml_comp = args["X"].AsBoolean();
    config.sp_dt_seq_ver = args["e"].AsBoolean();
    config.simple_genes = args["G"].AsBoolean();

    /* do not sort
     */
    config.sort = !args["S"].AsBoolean();

    config.debug = args["D"].AsBoolean();
    config.segment = args["E"].AsBoolean();
    config.accver = args["V"].AsBoolean();
    config.histacc = !args["F"].AsBoolean();

    config.transl = args["t"].AsBoolean();
    config.entrez_fetch = args["z"].AsBoolean() ? 1 : 0;
    config.taxserver = args["O"].AsBoolean() ? 0 : 1;
    config.medserver = args["u"].AsBoolean() ? 0 : 1;
    config.normalize = args["normalize"].AsBoolean();
    config.ign_bad_qs = false;
    config.cleanup = args["C"].AsInteger();
    config.allow_crossdb_featloc = args["d"].AsBoolean();
    config.genenull = args["U"].AsBoolean();
    config.qsfile = args["q"].AsString().c_str();
    if (!*config.qsfile)
        config.qsfile = nullptr;

    config.qamode = args["Q"].AsBoolean();

    if (args["y"].AsString() == "Bioseq-set")
        config.output_format = Parser::EOutput::BioseqSet;
    else if (args["y"].AsString() == "Seq-submit")
        config.output_format = Parser::EOutput::Seqsubmit;

    config.fpo.always_look = config.fpo.replace_cit = !args["h"].AsBoolean();

    std::string format = args["f"].AsString(),
                source = args["s"].AsString();

    config.all = args["a"].AsBoolean();

   if (!fta_set_format_source(config, format, source)) {
        retval = false;
    }

    return(retval);
}

class CFlat2AsnApp : public ncbi::CNcbiApplication
{
public:
    void Init() override;
    int Run() override;

private:
    unique_ptr<TConfig> x_ParseArgs(const CArgs& args, IObjtoolsListener& listener);
    bool x_OpenFiles(const CArgs& args, TConfig& config, IObjtoolsListener& listener);
    unique_ptr<CMemoryFileMap> m_pFileMap;
    bool m_DelInput=false;
    string m_InputFile;
};

/**********************************************************/
bool CFlat2AsnApp::x_OpenFiles(const CArgs& args, TConfig& config, IObjtoolsListener& listener)
{
    Char      str[1000];

    m_InputFile = args["i"].AsString();
    if (m_InputFile == "stdin")
    {
        m_InputFile = CDirEntry::GetTmpName(CFile::eTmpFileCreate);
        m_DelInput = true;
        auto fd = fopen(m_InputFile.c_str(), "w");

        while(fgets(str, 999, stdin))
            fprintf(fd, "%s", str);
        fclose(fd);
    }

    m_pFileMap.reset(new CMemoryFileMap(m_InputFile));
    auto fileSize = m_pFileMap->GetFileSize();
    config.ffbuf.set((const char*)m_pFileMap->Map(0, fileSize));

    if (!config.ffbuf.start) {
        listener.PutMessage(
                CObjtoolsMessage("Failed to open input flatfile " + m_InputFile, eDiag_Fatal));
        return false;
    }


    if(config.qsfile)
    {
        config.qsfd = fopen(config.qsfile, "rb");
        if(!config.qsfd)
        {
            listener.PutMessage(
                   CObjtoolsMessage("Failed to open Quality Scores file " + string(config.qsfile), eDiag_Fatal));
            config.ffbuf.start = nullptr;
            return false;
        }
    }
    return true;
}

/**********************************************************/
unique_ptr<TConfig> CFlat2AsnApp::x_ParseArgs(const CArgs& args, IObjtoolsListener& listener)
{
    unique_ptr<Parser> pConfig(new TConfig());

    /* As of June, 2004 sequence length limitation removed
     */
    pConfig->limit = 0;

    if (!InitConfig(args, *pConfig))
    {
        return nullptr;
    }

    if(!x_OpenFiles(args, *pConfig, listener))
    {
        return nullptr;
    }

    return pConfig;
}

void CFlat2AsnApp::Init()
{
    std::unique_ptr<ncbi::CArgDescriptions> arg_descrs(new ncbi::CArgDescriptions);
    arg_descrs->Delete("h");

    arg_descrs->AddDefaultKey("i", "InputFlatfile", "Input flatfile to parse", ncbi::CArgDescriptions::eString, "stdin");

    arg_descrs->AddOptionalKey("o", "OutputAsnFile", "Output ASN.1 file", ncbi::CArgDescriptions::eOutputFile);
    //arg_descrs->AddOptionalKey("l", "LogFile", "Log file", ncbi::CArgDescriptions::eOutputFile);
    arg_descrs->AddAlias("l", "logfile");

    arg_descrs->AddDefaultKey("a", "ParseRegardlessAccessionPrefix", "Parse all flatfile entries, regardless of accession prefix letter", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("D", "DebugMode", "Debug mode, output everything possible", ncbi::CArgDescriptions::eBoolean, "F");

    arg_descrs->AddKey("f", "FlatfileFormat", "Flatfile format", CArgDescriptions::eString);
    arg_descrs->AddAlias("format", "f");
    arg_descrs->SetConstraint("f",
            &(*new CArgAllow_Strings, "embl", "genbank", "sprot", "xml"));
    arg_descrs->AddKey("s", "SourceData", "Source of the data file", CArgDescriptions::eString);
    arg_descrs->AddAlias("source", "s");
    arg_descrs->SetConstraint("s",
            &(*new CArgAllow_Strings, "embl", "ddbj", "lanl", "ncbi", "sprot", "flybase", "refseq", "uspto"));

    arg_descrs->AddDefaultKey("u", "AvoidMuidLookup", "Avoid MUID lookup", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddOptionalKey("pubmed", "source", "Always eutils", CArgDescriptions::eString, CArgDescriptions::fHidden);
    arg_descrs->AddFlag("normalize", "Normalize output deterministically for tests", CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_descrs->AddDefaultKey("h", "AvoidReferencesLookup", "Avoid lookup of references which already have muids", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("r", "AvoidReferencesReplacement", "Avoid replacement of references with MedArch server versions", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("t", "TranslationReplacement", "Replace original translation with parser generated version", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("O", "AvoidOrganismLookup", "Avoid ORGANISM lookup", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("z", "UsePubseq", "Connect to PUBSEQ OS to fetch missing bioseqs", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("b", "BinaryOutput", "ASN.1 output in binary format", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("v", "Verbose", "Display verbose error message on console", ncbi::CArgDescriptions::eBoolean, "F");

    arg_descrs->AddDefaultKey("E", "EmblSegmentedSet", "Treat input EMBL flatfile as a segmented set", ncbi::CArgDescriptions::eBoolean, "F");

    arg_descrs->AddDefaultKey("N", "NoDates", "No update date or current date", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("S", "NoSort", "Don't sort the entries in flatfile", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("L", "ReplaceUpdateDate", "Replace update date from LOCUS line with current date", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("I", "UseAccession", "Use accession for segmented set id", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("V", "ParseAccessionVersion", "Parse with ACCESSION.VERSION identifiers", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("F", "NoPopulate", "Do not populate Seq-inst.hist.replaces with secondary accessions", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("c", "IgnoreMultipleTokens", "Ignore multiple tokens in DDBJ's VERSION line", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("j", "NoReject", "Do not reject records if protein Seq-ids are from sources that differ from that of the \"-s\" argument",
                              ncbi::CArgDescriptions::eBoolean, "F");

    // constraint: 0, 1
    arg_descrs->AddDefaultKey("m", "ParsingMode", "Parsing mode. Values: 0 - RELEASE, 1 - HTGS, 2 - HTGSCON, 3 - Relaxed (only applies to NCBI source)", ncbi::CArgDescriptions::eInteger, "0");
    arg_descrs->AddAlias("mode", "m");

    arg_descrs->AddDefaultKey("Y", "AllowsInconsistentPair", "Allows inconsistent pairs of /gene+/locus_tag quals, when same genes go along with different locus_tags", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("w", "AllowsUnusualWgsAccessions", "Allows unusual secondary WGS accessions with prefixes not matching the primary one", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("X", "CompatibleMode", "INSDSeq/GenBank/EMBL compatible mode. Please don't use it", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("e", "SwissprotVersion", "For SwissProt \"Reviewed\" records only: parse the sequence version number from \"sequence version\" DT line into Seq-id.version slot", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("g", "AllowsSingleBaseGap", "For ALL sources: allows single-base \"gap\" features (obsolete)", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("G", "SimpleGeneLocations", "Always generate simple gene locations, no joined ones", ncbi::CArgDescriptions::eBoolean, "F");

    // constraint: 0, 1, 2
    arg_descrs->AddDefaultKey("C", "CleanupMode", "Cleanup function to use:\n       0,1 - Toolkit's SeriousSeqEntryCleanup;\n        2 - none.\n   ", ncbi::CArgDescriptions::eInteger, "1");

    arg_descrs->AddDefaultKey("d", "AllowsXDb", "Allow cross-database join locations", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("U", "UseNull", "Use NULLs for Gene Features that are not single-interval", ncbi::CArgDescriptions::eBoolean, "T");

    arg_descrs->AddDefaultKey("q", "QualityScoresFilename", "Quality Scores filename", ncbi::CArgDescriptions::eString, "");
    arg_descrs->AddDefaultKey("Q", "QAMode", "QA mode: does not populate top level create-date and\n      NcbiCleanup user-object", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("y", "OutputFormat", "Output format. Possible values: \"Bioseq-set\" and \"Seq-submit\"", ncbi::CArgDescriptions::eString, "Bioseq-set");
    arg_descrs->SetConstraint("y",
            &(*new  CArgAllow_Strings,
                "Bioseq-set", "Seq-submit"));

    SetupArgDescriptions(arg_descrs.release());
}


class CFlat2AsnListener : public CObjtoolsListener
{
public:
    CFlat2AsnListener(const string& prefix, EDiagSev minSev=eDiagSevMin) 
        : m_Prefix(prefix), m_MinSev(minSev) {}
    ~CFlat2AsnListener() override {}

    bool PutMessage(const IObjtoolsMessage& msg) override {
        if (msg.GetSeverity() >= m_MinSev) {
            return CObjtoolsListener::PutMessage(msg);
        }
        return false;
    }

    void Dump(CNcbiOstream& ostr) const override {
        if (m_Prefix.empty()) {
            CObjtoolsListener::Dump(ostr);
        }
        else {
            for (const auto& pMessage : m_Messages) {
                ostr << m_Prefix << " ";
                pMessage->Dump(ostr);
            }
        }
    }

    void Dump(CNcbiOstream& ostr, const string& prefix, EDiagSev minSev) const 
    {
        for (const auto& pMessage : m_Messages) {
            if (pMessage->GetSeverity() >= minSev) {
                ostr << prefix << " ";
                pMessage->Dump(ostr);
            }
        }
    }

private:
    string m_Prefix;
    EDiagSev m_MinSev;
};


int CFlat2AsnApp::Run()
{
    const auto& args = GetArgs();
    const bool haveLogFile = args["logfile"];

    CNcbiOstream* pLogStream = haveLogFile ?
        &args["logfile"].AsOutputFile() :
        &NcbiCerr;

    CFlat2AsnListener messageListener(haveLogFile ? "" : "[" + CNcbiApplication::GetAppName() + "]",
                                      haveLogFile ? eDiagSevMin : eDiag_Warning);

    auto pConfig = x_ParseArgs(args, messageListener);
    if (!pConfig)
    {
        return 1;
    }

    CFlatFileParser ffparser(&messageListener);
    auto pSerialObject = ffparser.Parse(*pConfig);
    m_pFileMap.reset();
    if (m_DelInput) {
        CDirEntry(m_InputFile).Remove();
    }

    if (messageListener.Count() > 0) {
        messageListener.Dump(*pLogStream);
        if (haveLogFile) {
            messageListener.Dump(NcbiCerr, 
                    "[" + CNcbiApplication::GetAppName() + "]", 
                    eDiag_Warning);
        }
    }

    if (pSerialObject) {
        CNcbiOstream* pOstream = args["o"] ?
            &args["o"].AsOutputFile() :
            &NcbiCout;
        if (args["b"].AsBoolean()) {
            *pOstream << MSerial_AsnBinary << *pSerialObject;
        }
        else {
            *pOstream << MSerial_AsnText << *pSerialObject;
        }
        return 0;
    }

    return 1;
}

int main(int argc, const char* argv[])
{
    return CFlat2AsnApp().AppMain(argc, argv);
}
