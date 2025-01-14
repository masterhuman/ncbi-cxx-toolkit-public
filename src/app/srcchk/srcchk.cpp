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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   source qualifier generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/writers/src_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
class CSrcChkApp : public CNcbiApplication
//  ============================================================================
{
public:
    CSrcChkApp();
    void Init();
    int Run();

private:
    CNcbiOstream* xInitOutputStream(
        const CArgs&);
    bool xTryProcessIdFile(
        const CArgs&);
    bool xTryProcessSeqEntry(
        const CArgs&);
    bool xGetDesiredFields(
        const CArgs&,
        vector<string>&);
    CSrcWriter* xInitWriter(
        const CArgs&);
    void xDumpError(
        const ILineError&,
        std::ostream&);

private:
    CRef<CObjectManager> m_pObjMngr;
    CRef<CScope> m_pScope;
    CRef<CSrcWriter> m_pWriter;
    CRef<CMessageListenerBase> m_pErrors;
};

CSrcChkApp::CSrcChkApp()
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}

//  ----------------------------------------------------------------------------
void CSrcChkApp::Init()
//  ----------------------------------------------------------------------------
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Extract Genbank source qualifiers",
        false);

    // input
    {{
        arg_desc->AddOptionalKey("i", "IDsFile",
            "IDs file name. Defaults to stdin",
            CArgDescriptions::eInputFile );

        arg_desc->AddOptionalKey("seq-entry", "SeqEntryFile",
            "File containing Seq-entry in ASN.1 format",
            CArgDescriptions::eInputFile );

        arg_desc->SetDependency("seq-entry",
                CArgDescriptions::eExcludes,
                "i");
    }}

    // parameters
    {{
        arg_desc->AddOptionalKey("f", "FieldsList",
            "List of fields", CArgDescriptions::eString );

        arg_desc->SetDependency("f",
                CArgDescriptions::eExcludes,
                "F");

        arg_desc->SetDependency("f",
                CArgDescriptions::eExcludes,
                "all-fields");

        arg_desc->AddOptionalKey("F", "FieldsFile",
            "File of fields", CArgDescriptions::eInputFile );

        arg_desc->SetDependency("F",
                CArgDescriptions::eExcludes,
                "all-fields");

        arg_desc->AddFlag("all-fields", "List all fields");
    }}

    {{
        arg_desc->AddFlag("list-supported-fields",
                "List in alphabetical order the fields that srcchk can display; ignore other arguments");
    }}

    // output
    {{
        arg_desc->AddOptionalKey("o", "OutputFile",
            "Output file name. Defaults to stdout",
            CArgDescriptions::eOutputFile );
    }}
    {{
    //  misc
        arg_desc->AddDefaultKey("delim", "Delimiter",
            "Column value delimiter", CArgDescriptions::eString, "\t");
    }}


    SetupArgDescriptions(arg_desc.release());
}

//  ----------------------------------------------------------------------------
int CSrcChkApp::Run()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

    if (args["list-supported-fields"]) {
        vector<string> sortedFields = CSrcWriter::sAllSrcCheckFields;
        sort(begin(sortedFields), end(sortedFields));
        for (const auto& field : sortedFields) {
            cout << field << "\n";
        }
        return 0;
    }

	CONNECT_Init(&GetConfig());
    m_pObjMngr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *m_pObjMngr);
    m_pScope.Reset(new CScope(*m_pObjMngr));
    m_pScope->AddDefaults();

    m_pErrors = new CMessageListenerStrict;
    m_pWriter.Reset(xInitWriter(args));
    bool processed = xTryProcessSeqEntry(args);
    if (!processed) {
        processed = xTryProcessIdFile(args);
    }
    size_t errorCount = m_pErrors->Count();
    for (size_t pos=0; pos < errorCount; ++pos) {
        xDumpError(m_pErrors->GetError(pos), cerr);
    }
    return (errorCount ? 1 : 0);
}

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xTryProcessIdFile(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    CNcbiOstream* pOs = xInitOutputStream(args);
    if (0 == pOs) {
        string error_msg = args["o"] ?
                           "Unable to open output file \"" + args["o"].AsString() + "\"." :
                           "Unable to write to stdout.";
        CSrcError* pE = CSrcError::Create(ncbi::eDiag_Error, error_msg);
        m_pErrors->PutError(*pE);
        delete pE;
        return false;
    }

    const streamsize maxLineSize(100);
    char line[maxLineSize];

    CSrcWriter::FIELDS desiredFields;
    if (!xGetDesiredFields(args, desiredFields)) {
        return false;
    }

    CNcbiIstream* pIfstr = 0;
    try {
        pIfstr = args["i"] ? &args["i"].AsInputFile() : &cin;
    }
    catch(const std::exception&) {
        string error_msg = args["i"] ?
                     "Unable to open ID file \"" + args["i"].AsString() + "\"." :
                     "Unable to read IDs from stdin.";
        CSrcError* pE = CSrcError::Create(ncbi::eDiag_Error, error_msg);
        m_pErrors->PutError(*pE);
        delete pE;
        return false;
    }
    vector<pair<string, CBioseq_Handle> > vecIdBsh;
    while (!pIfstr->eof()) {
        pIfstr->getline(line, maxLineSize);
        if (line[0] == 0  ||  line[0] == '#') {
            continue;
        }
        string id(line);
        NStr::TruncateSpacesInPlace(id);
        try {
            CSeq_id_Handle seqh = CSeq_id_Handle::GetHandle(id);
            CBioseq_Handle bsh = m_pScope->GetBioseqHandle(seqh);
            vecIdBsh.push_back(make_pair(id,bsh));
        } catch (const CSeqIdException& e) {
            if (e.GetErrCode() != CSeqIdException::eFormat) {
                throw;
            }
            id = NStr::PrintableString(id, NStr::fNonAscii_Quote);
            string err_msg =  "Malformatted ID \"" + id + "\"";
            ERR_POST(err_msg);
            vecIdBsh.push_back(make_pair(id,CBioseq_Handle()));
        }
    }


    if (vecIdBsh.empty() || !m_pWriter->WriteBioseqHandles(vecIdBsh, desiredFields, *pOs, m_pErrors)) {
        return false;
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xTryProcessSeqEntry(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if (!args["seq-entry"]) {
        return false;
    }
    CNcbiOstream* pOs = xInitOutputStream(args);
    if (0 == pOs) {
        string error_msg = args["o"] ?
            "Unable to open output file \"" + args["o"].AsString() + "\"." :
            "Unable to write to stdout.";
        CSrcError* pE = CSrcError::Create(ncbi::eDiag_Error, error_msg);
        m_pErrors->PutError(*pE);
        delete pE;
        return false;
    }

    CSrcWriter::FIELDS desiredFields;
    if (!xGetDesiredFields(args, desiredFields)) {
        return false;
    }

    const char* infile = args["seq-entry"].AsString().c_str();
    CNcbiIstream* pInputStream = new CNcbiIfstream(infile, ios::binary);
    unique_ptr<CObjectIStream> pI(CObjectIStream::Open(eSerial_AsnText, *pInputStream, eTakeOwnership));
    if (!pI) {
        string msg("Unable to open Seq-entry file \"" + args["seq-entry"].AsString() + "\".");
        CSrcError* pE = CSrcError::Create(ncbi::eDiag_Error, msg);
        m_pErrors->PutError(*pE);
        delete pE;
        return false;
    }

    CRef<CSeq_entry> pSe(new CSeq_entry);
    try {
        pI->Read(ObjectInfo(*pSe));
    }
    catch (const CException&) {
        string msg("Unable to process Seq-entry file \"" + args["seq-entry"].AsString() + "\".");
        CSrcError* pE = CSrcError::Create(ncbi::eDiag_Error, msg);
        m_pErrors->PutError(*pE);
        delete pE;
        return true; //!!!
    }

    m_pWriter->WriteSeqEntry(*pSe, *m_pScope, *pOs);
    return true; //!!!
}

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xGetDesiredFields(
    const CArgs& args,
    CSrcWriter::FIELDS& fields)
//  -----------------------------------------------------------------------------
{
    if (args["all-fields"]) {
        fields = CSrcWriter::sAllSrcCheckFields;
        return true;
    }

    if (args["f"]) {
        string fieldString = args["f"].AsString();
        NStr::Split(fieldString, ",", fields);
        return CSrcWriter::ValidateFields(fields, m_pErrors);
    }
    if (args["F"]) {
        const streamsize maxLineSize(100);
        char line[maxLineSize];
        CNcbiIstream* pIfstr = 0;
        try {
            pIfstr = &args["F"].AsInputFile();
        }
        catch (const std::exception&) {
            CSrcError* pE = CSrcError::Create(
                ncbi::eDiag_Error,
                "Unable to open fields file \"" + args["F"].AsString() + "\".");
            m_pErrors->PutError(*pE);
            delete pE;
            return false;
        }
        while (!pIfstr->eof()) {
            pIfstr->getline(line, maxLineSize);
            if (line[0] == 0  ||  line[0] == '#') {
                continue;
            }
            string field(line);
            NStr::TruncateSpacesInPlace(field);
            if (field.empty()) {
                continue;
            }
            if (field == "id"  ||  field == "accession") {
                //handled implicitly
                continue;
            }
            fields.push_back(field);
        }
        return CSrcWriter::ValidateFields(fields, m_pErrors);
    }


    fields.assign(
        CSrcWriter::sDefaultSrcCheckFields.begin(), CSrcWriter::sDefaultSrcCheckFields.end());
    return true;
}

//  -----------------------------------------------------------------------------
CNcbiOstream* CSrcChkApp::xInitOutputStream(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if (!args["o"]) {
        return &cout;
    }
    try {
        return &args["o"].AsOutputFile();
    }
    catch(const std::exception&) {
        return 0;
    }
}

//  ----------------------------------------------------------------------------
CSrcWriter* CSrcChkApp::xInitWriter(
    const CArgs& args)
//  ----------------------------------------------------------------------------
{
    CSrcWriter* pWriter = new CSrcWriter(0);
    pWriter->SetDelimiter(args["delim"].AsString());
    return pWriter;
}

//  ---------------------------------------------------------------------------
void CSrcChkApp::xDumpError(
    const ILineError& error,
    std::ostream& out)
//  ---------------------------------------------------------------------------
{
    out << "srcchk "
        << error.SeverityStr().c_str()
        << ":  "
        << error.ErrorMessage().c_str()
        << endl;
}

END_NCBI_SCOPE
USING_NCBI_SCOPE;

//  ===========================================================================
int main(int argc, const char** argv)
//  ===========================================================================
{
    return CSrcChkApp().AppMain(argc, argv);
}
