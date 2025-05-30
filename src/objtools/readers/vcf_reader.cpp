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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   VCF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <util/line_reader.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objects/seqfeat/Delta_item.hpp>

#include <objtools/readers/vcf_reader.hpp>

#include <algorithm>

#include "reader_message_handler.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
const double CVcfReader::mMaxSupportedVersion = 4.1;
//  ============================================================================

//  ============================================================================
class CVcfData
//  ============================================================================
{
public:
    typedef map<string,vector<string> > INFOS;
    typedef map<string, vector<string> > GTDATA;

    CVcfData() { m_pdQual = 0; };
    ~CVcfData() { delete m_pdQual; };

    string m_strLine;
    string m_strChrom;
    int m_iPos;
    vector<string> m_Ids;
    string m_strRef;
    vector<string> m_Alt;
    double* m_pdQual;
    string m_strFilter;
    INFOS m_Info;
    vector<string> m_FormatKeys;
//    vector< vector<string> > m_GenotypeData;
    GTDATA m_GenotypeData;
    enum SetType_t {
        ST_ALL_SNV,
        ST_ALL_DEL,
        ST_ALL_INS,
        ST_ALL_MNV,
        ST_MIXED
    } m_SetType;
};

//  ----------------------------------------------------------------------------
ESpecType SpecType(
    const string& spectype )
//  ----------------------------------------------------------------------------
{
    static map<string, ESpecType> typemap;
    if ( typemap.empty() ) {
        typemap["Integer"] = eType_Integer;
        typemap["Float"] = eType_Float;
        typemap["Flag"] = eType_Flag;
        typemap["Character"] = eType_Character;
        typemap["String"] = eType_String;
    }
    try {
        return typemap[spectype];
    }
    catch( ... ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Warning,
            0,
            "CVcfReader::xProcessMetaLineInfo: Unrecognized line or record type.",
            ILineError::eProblem_GeneralParsingError) );
        pErr->Throw();
        return eType_String;
    }
};

//  ----------------------------------------------------------------------------
ESpecNumber SpecNumber(
    const string& specnumber )
//  ----------------------------------------------------------------------------
{
    if ( specnumber == "R" ) {
        return eNumber_CountAllAlleles;
    }
    if ( specnumber == "A" ) {
        return eNumber_CountAlleles;
    }
    if ( specnumber == "G" ) {
        return eNumber_CountGenotypes;
    }
    if ( specnumber == "." ) {
        return eNumber_CountUnknown;
    }
    try {
        return ESpecNumber( NStr::StringToInt( specnumber ) );
    }
    catch( ... ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
                eDiag_Warning,
                0,
                "CVcfReader::xProcessMetaLineInfo: Unrecognized SpecNumber type in FORAMT directive. "
                "Recognized settings are \'A\', \'G\', \'R\', \'.\', or numeric.",
                ILineError::eProblem_GeneralParsingError) );
        pErr->Throw();
    }
    return ESpecNumber( 0 );
};

//  ----------------------------------------------------------------------------
CVcfReader::CVcfReader(
    int flags,
    CReaderListener* pRL):
    CReaderBase(flags, "", "", CReadUtil::AsSeqId, pRL),
    mActualVersion(0.0),
    m_MetaHandled(false)
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
CVcfReader::~CVcfReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CVcfReader::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    if (!m_Meta) {
        m_Meta.Reset( new CAnnotdesc );
        m_Meta->SetUser().SetType().SetStr( "vcf-meta-info" );
    }
    CRef<CSeq_annot> pAnnot = CReaderBase::ReadSeqAnnot(lr, pEC);
    if (pAnnot) {
        xAssignTrackData(*pAnnot);
        xAssignVcfMeta(*pAnnot);
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot>
CVcfReader::xCreateSeqAnnot()
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> pAnnot = CReaderBase::xCreateSeqAnnot();
    pAnnot->SetData().SetFtable();
    return pAnnot;
}

//  ----------------------------------------------------------------------------
void
CVcfReader::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    readerData.clear();
    string line;
    if (!xGetLine(lr, line)) {
        return;
    }
    if (xIsTrackLine(line)  &&  m_uDataCount) {
        xUngetLine(lr);
        return;
    }
    readerData.push_back(TReaderLine{m_uLineNumber, line});
}

//  ----------------------------------------------------------------------------
void
CVcfReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    for (auto lineInfo: readerData) {
        const auto& line = lineInfo.mData;
        if (mActualVersion == 0.0) {
            bool lineContainsVersion(false);
            xSetFileFormat(line, annot, lineContainsVersion);
            // Note:
            // Currently, the line format specifier is also processed as a
            // meta line even though it really isn't, or at least it's a very
            // different meta than all other VCF metas.
            // Uncomment the following lines to no longer process the file format
            // specifier as a meta.
            //if (lineContainsVersion) {
                //return;
            //}
        }

        if (xParseBrowserLine(line, annot)) {
            return;
        }
        if (xProcessTrackLine(line, annot)) {
            return;
        }
        if (xProcessMetaLine(line, annot)) {
            return;
        }
        if (xProcessHeaderLine(line, annot)) {
            return;
        }
        if (xProcessDataLine(line, annot)) {
            ++m_uDataCount;
            return;
        }
        CReaderMessage warning(
            eDiag_Warning,
            m_uLineNumber,
            "CVcfReader::ReadSeqAnnot: Unrecognized line or record type.");
        m_pMessageHandler->Report(warning);
    }
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessMetaLine(
    const string& line,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( line, "##" ) ) {
        if ( !m_MetaDirectives.empty() && !m_MetaHandled ) {
            m_Meta->SetUser().AddField("meta-information", m_MetaDirectives);
        }
        m_MetaHandled = true;
        return false;
    }
    m_MetaDirectives.push_back(line.substr(2));

    if (xProcessMetaLineInfo(line, annot)) {
        return true;
    }
    if (xProcessMetaLineFilter(line, annot)) {
        return true;
    }
    if (xProcessMetaLineFormat(line, annot)) {
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
void
CVcfReader::xSetFileFormat(
    const string& line,
    CSeq_annot& /*annot*/,
    bool& lineContainsVersion)
//  ----------------------------------------------------------------------------
{
    const string prefix = "##fileformat=VCFv";

    if (!NStr::StartsWith(line, prefix)) {
        CReaderMessage warning(
            eDiag_Warning,
            m_uLineNumber,
            string("CVcfReader::xProcessMetaLineFileFormat: ") +
                "Missing VCF version string. Assuming VCFv" +
                NStr::DoubleToString(mMaxSupportedVersion) +
                ". Proceed with care!");
        m_pMessageHandler->Report(warning);
        mActualVersion = mMaxSupportedVersion;
        lineContainsVersion = false;
        return;
    }

    lineContainsVersion = true;

    string versionStr = line.substr(prefix.length(), string::npos);
    try {
        mActualVersion = NStr::StringToDouble(versionStr);
    }
    catch (const std::exception&) {
        CReaderMessage warning(
            eDiag_Warning,
            m_uLineNumber,
            string("CVcfReader::xProcessMetaLineFileFormat: ") +
            "Data file contains an unrecognized version string \"" +
                versionStr +
                "\". Assuming VCFv" +
                NStr::DoubleToString(mMaxSupportedVersion) +
                ". Proceed with care!");
        m_pMessageHandler->Report(warning);
        mActualVersion = mMaxSupportedVersion;
        return;
    }

    if (mActualVersion > mMaxSupportedVersion) {
        CReaderMessage warning(
            eDiag_Warning,
            m_uLineNumber,
            string("CVcfReader::xProcessMetaLineFileFormat: Data file format \"") +
                versionStr +
                "\" exceeds reader supported format \"" +
                NStr::DoubleToString(mMaxSupportedVersion) +
                "\". Proceed with care!");
        m_pMessageHandler->Report(warning);
        mActualVersion = mMaxSupportedVersion;
        return;
    }
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessMetaLineInfo(
    const string& line,
    CSeq_annot& /*annot*/)
//  ----------------------------------------------------------------------------
{
    const string prefix = "##INFO=<";
    const string postfix = ">";

    if ( ! NStr::StartsWith( line, prefix ) || ! NStr::EndsWith( line, postfix ) ) {
        return false;
    }

    try {
        vector<string> fields;
        string key, id, numcount, type, description;
        string info = line.substr(
            prefix.length(), line.length() - prefix.length() - postfix.length() );
        NStr::Split( info, ",", fields );
        NStr::SplitInTwo( fields[0], "=", key, id );
        if ( key != "ID" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: ##INFO with bad or missing \"ID\".",
                ILineError::eProblem_BadInfoLine) );
            pErr->Throw();
        }
        NStr::SplitInTwo( fields[1], "=", key, numcount );
        if ( key != "Number" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: ##INFO with bad or missing \"Number\".",
                ILineError::eProblem_BadInfoLine) );
            pErr->Throw();
        }
        NStr::SplitInTwo( fields[2], "=", key, type );
        if ( key != "Type" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: ##INFO with bad or missing \"Type\".",
                ILineError::eProblem_BadInfoLine) );
            pErr->Throw();
        }
        NStr::SplitInTwo( fields[3], "=", key, description );
        if ( key != "Description" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: ##INFO with bad or missing \"Description\".",
                ILineError::eProblem_BadInfoLine) );
            pErr->Throw();
        }
        m_InfoSpecs[id] = CVcfInfoSpec( id, numcount, type, description );
    }
    catch (CObjReaderLineException& err) {
        ProcessError(err, nullptr);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessMetaLineFilter(
    const string& line,
    CSeq_annot& /*annot*/)
//  ----------------------------------------------------------------------------
{
    const string prefix = "##FILTER=<";
    const string postfix = ">";

    if ( ! NStr::StartsWith( line, prefix ) || ! NStr::EndsWith( line, postfix ) ) {
        return false;
    }

    try {
        vector<string> fields;
        string key, id, description;
        string info = line.substr(
            prefix.length(), line.length() - prefix.length() - postfix.length() );
        NStr::Split( info, ",", fields );
        NStr::SplitInTwo( fields[0], "=", key, id );
        if ( key != "ID" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: ##FILTER with bad or missing \"ID\".",
                ILineError::eProblem_BadFilterLine) );
            pErr->Throw();
        }
        NStr::SplitInTwo( fields[1], "=", key, description );
        if ( key != "Description" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: ##FILTER with bad or missing \"Description\".",
                ILineError::eProblem_BadFilterLine) );
            pErr->Throw();
        }
        m_FilterSpecs[id] = CVcfFilterSpec( id, description );
    }
    catch (CObjReaderLineException& err) {
        ProcessError(err, nullptr);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessMetaLineFormat(
    const string& line,
    CSeq_annot& /*annot*/)
//  ----------------------------------------------------------------------------
{
    const string prefix = "##FORMAT=<";
    const string postfix = ">";

    if ( ! NStr::StartsWith( line, prefix ) || ! NStr::EndsWith( line, postfix ) ) {
        return false;
    }

    try {
        vector<string> fields;
        string key, id, numcount, type, description;
        string info = line.substr(
            prefix.length(), line.length() - prefix.length() - postfix.length() );
        NStr::Split( info, ",", fields );
        NStr::SplitInTwo( fields[0], "=", key, id );
        if ( key != "ID" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: ##FORMAT with bad or missing \"ID\".",
                ILineError::eProblem_BadFormatLine) );
            pErr->Throw();
        }
        NStr::SplitInTwo( fields[1], "=", key, numcount );
        if ( key != "Number" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: "
                    "##FORMAT with bad or missing \"Number\".",
                ILineError::eProblem_BadFormatLine) );
            pErr->Throw();
        }
        NStr::SplitInTwo( fields[2], "=", key, type );
        if ( key != "Type" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: "
                    "##FORMAT with bad or missing \"Type\".",
                ILineError::eProblem_BadFormatLine) );
            pErr->Throw();
        }
        NStr::SplitInTwo( fields[3], "=", key, description );
        if ( key != "Description" ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xProcessMetaLineInfo: "
                    "##FORMAT with bad or missing \"Description\".",
                ILineError::eProblem_BadFormatLine) );
            pErr->Throw();
        }
        m_FormatSpecs[id] = CVcfFormatSpec( id, numcount, type, description );
    }
    catch (CObjReaderLineException& err) {
        ProcessError(err, nullptr);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessHeaderLine(
    const string& line,
    CSeq_annot& /*annot*/ )
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( line, "#CHROM" ) ) {
        return false;
    }

    //
    //  Per spec:
    //  The header line provides the column headers for the data records that follow.
    //  the first few are fixed and mandatory: CHROM .. FILTER.
    //  If genotype data is present this is followed by the FORMAT header.
    //  After that come the various headers for the genotype information, and these
    //  need to be preserved:
    //
    NStr::Split(line, " \t", m_GenotypeHeaders, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    vector<string>::iterator pos_format = find(
        m_GenotypeHeaders.begin(), m_GenotypeHeaders.end(), "FORMAT");
    if ( pos_format == m_GenotypeHeaders.end() ) {
        m_GenotypeHeaders.clear();
    }
    else {
        m_GenotypeHeaders.erase( m_GenotypeHeaders.begin(), pos_format+1 );
        m_Meta->SetUser().AddField("genotype-headers", m_GenotypeHeaders);
    }

    //
    //  The header line signals the end of meta information, so migrate the
    //  accumulated meta information into the seq descriptor:
    //
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVcfMeta(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if (m_Meta  &&  m_Meta->IsUser() &&  m_Meta->GetUser().IsSetData()) {
        if (!annot.IsSetDesc()) {
            CRef< CAnnot_descr > desc( new CAnnot_descr );
            annot.SetDesc(*desc);
        }
        annot.SetDesc().Set().push_back( m_Meta );
    }
    //else { // VCF input ought to include a header
    //    CReaderMessage warning(
    //        eDiag_Warning,
    //        m_uLineNumber,
    //        "CVcfReader::xAssignVcfMeta: Missing VCF header data.");
    //    m_pMessageHandler->Report(warning);
    //}
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessDataLine(
    const string& line,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if ( NStr::StartsWith( line, "#" ) ) {
        return false;
    }

    CVcfData data;
    if (!xParseData(line, data, nullptr)) {
        return false;
    }
    CRef<CSeq_feat> pFeat( new CSeq_feat );
    pFeat->SetData().SetVariation().SetData().SetSet().SetType(
        CVariation_ref::C_Data::C_Set::eData_set_type_package );
    pFeat->SetData().SetVariation().SetVariant_prop().SetVersion( 5 );
    CSeq_feat::TExt& ext = pFeat->SetExt();
    ext.SetType().SetStr( "VcfAttributes" );

    if (!xAssignFeatureLocationSet(data, pFeat)) {
        return false;
    }
    if (!xAssignVariationIds(data, pFeat)) {
        return false;
    }
    if (!xAssignVariationAlleleSet(data, pFeat)) {
        return false;
    }
    if (!xProcessScore(data, pFeat)) {
        return false;
    }
    if (!xProcessFilter(data, pFeat)) {
        return false;
    }
    if (!xProcessInfo( data, pFeat)) {
        return false;
    }
    if (!xProcessFormat(data, pFeat)) {
        return false;
    }

    if ( pFeat->GetExt().GetData().empty() ) {
        pFeat->ResetExt();
    }
    annot.SetData().SetFtable().push_back(pFeat);
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariationAlleleSet(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CVariation_ref::TData::TSet::TVariations& variants =
        pFeature->SetData().SetVariation().SetData().SetSet().SetVariations();

    //make one variation for the reference
    CRef<CVariation_ref> pIdentity(new CVariation_ref);
    vector<string> variant;

    switch(data.m_SetType) {
    case CVcfData::ST_ALL_INS:
        pIdentity->SetDeletion();
        break;
    default:
        variant.push_back(data.m_strRef);
        pIdentity->SetSNV(variant, CVariation_ref::eSeqType_na);
        break;
    }
    CVariation_inst& instance = pIdentity->SetData().SetInstance();
    instance.SetType(CVariation_inst::eType_identity);
    instance.SetObservation(CVariation_inst::eObservation_reference);
    if (data.m_SetType != CVcfData::ST_ALL_INS) {
        variants.push_back(pIdentity);
    }

    bool no_alt = true;
    for (unsigned int i=0; i < data.m_Alt.size(); ++i) {
        if (!NStr::Equal(data.m_Alt[i],".")) {
            no_alt = false;
        }
    }
    if (no_alt) {
        instance.SetObservation() |= CVariation_inst::eObservation_variant;
        return true;
    }


    //add additional variations, one for each alternative
    for (unsigned int i=0; i < data.m_Alt.size(); ++i) {
        if (NStr::Equal(data.m_Alt[i],".")) {
            continue;
        }
        switch(data.m_SetType) {
        default:
            if (!xAssignVariantDelins(data, i, pFeature)) {
                return false;
            }
            break;
        case CVcfData::ST_ALL_SNV:
            if (!xAssignVariantSnv(data, i, pFeature)) {
                return false;
            }
            break;
        case CVcfData::ST_ALL_MNV:
            if (!xAssignVariantMnv(data, i, pFeature)) {
                return false;
            }
            break;
        case CVcfData::ST_ALL_INS:
            if (!xAssignVariantIns(data, i, pFeature)) {
                return false;
            }
            break;
        case CVcfData::ST_ALL_DEL:
            if (!xAssignVariantDel(data, i, pFeature)) {
                return false;
            }
            break;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariantSnv(
    const CVcfData& data,
    unsigned int index,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CVariation_ref::TData::TSet::TVariations& variants =
        pFeature->SetData().SetVariation().SetData().SetSet().SetVariations();

    CRef<CVariation_ref> pVariant(new CVariation_ref);
    {{
        vector<string> variant;
        variant.push_back(data.m_Alt[index]);
        pVariant->SetSNV(variant, CVariation_ref::eSeqType_na);
    }}
    variants.push_back(pVariant);
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariantMnv(
    const CVcfData& data,
    unsigned int index,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CVariation_ref::TData::TSet::TVariations& variants =
        pFeature->SetData().SetVariation().SetData().SetSet().SetVariations();

    CRef<CVariation_ref> pVariant(new CVariation_ref);
    {{
        vector<string> variant;
        variant.push_back(data.m_Alt[index]);
        pVariant->SetMNP(variant, CVariation_ref::eSeqType_na);
    }}
    variants.push_back(pVariant);
    return true;
}

//  ----------------------------------------------------------------------------
static void
s_AddDeleteDeltaItem(
    CVariation_inst& instance )
{
    CRef<CDelta_item> pItem(new CDelta_item);

    pItem->SetSeq().SetThis();
    instance.SetType(CVariation_inst::eType_del);
    pItem->SetAction(CDelta_item::eAction_del_at);
    instance.SetDelta().push_back(pItem);
}


//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariantDel(
    const CVcfData& /*data*/,
    unsigned int /*index*/,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CVariation_ref::TData::TSet::TVariations& variants =
        pFeature->SetData().SetVariation().SetData().SetSet().SetVariations();

    CRef<CVariation_ref> pVariant(new CVariation_ref);
    {{
        //pVariant->SetData().SetNote("DEL");
        pVariant->SetDeletion();
        CVariation_inst& instance =  pVariant->SetData().SetInstance();
        s_AddDeleteDeltaItem(instance);

    }}
    variants.push_back(pVariant);
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariantIns(
    const CVcfData& data,
    unsigned int index,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CVariation_ref::TData::TSet::TVariations& variants =
        pFeature->SetData().SetVariation().SetData().SetSet().SetVariations();

    CRef<CVariation_ref> pVariant(new CVariation_ref);
    {{
        string insertion(data.m_Alt[index]);
        CRef<CSeq_literal> pLiteral(new CSeq_literal);
        pLiteral->SetSeq_data().SetIupacna().Set(insertion);
        pLiteral->SetLength(
            static_cast<TSeqPos>(insertion.size()));
        CRef<CDelta_item> pItem(new CDelta_item);
        pItem->SetAction(CDelta_item::eAction_ins_before);
        pItem->SetSeq().SetLiteral(*pLiteral);
        CVariation_inst& instance =  pVariant->SetData().SetInstance();
        instance.SetType(CVariation_inst::eType_ins);
        instance.SetDelta().push_back(pItem);
    }}
    variants.push_back(pVariant);
    return true;
}



//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariantDelins(
    const CVcfData& data,
    unsigned int index,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    string insertion(data.m_Alt[index]);

    CVariation_ref::TData::TSet::TVariations& variants =
        pFeature->SetData().SetVariation().SetData().SetSet().SetVariations();

    CRef<CVariation_ref> pVariant(new CVariation_ref);
    CVariation_inst& instance = pVariant->SetData().SetInstance();

    // If it is a deletion, add Deleteion Delta-item and be done.
    if (insertion.size() == 0) {

        s_AddDeleteDeltaItem(instance);
        variants.push_back(pVariant);
        return true;
    }

    // Must be a SNV or delins
    CRef<CSeq_literal> pLiteral(new CSeq_literal);
    pLiteral->SetSeq_data().SetIupacna().Set(insertion);
    pLiteral->SetLength(static_cast<TSeqPos>(insertion.size()));

    CRef<CDelta_item> pItem(new CDelta_item);
    pItem->SetSeq().SetLiteral(*pLiteral);
    instance.SetDelta().push_back(pItem);

    //Let's try to smartly set the Type.
    if (insertion.size() == 1 && data.m_strRef.size() == 1) {
        instance.SetType(CVariation_inst::eType_snv);
    } else {
        instance.SetType(CVariation_inst::eType_delins);
    }

    variants.push_back(pVariant);
    return true;

}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xParseData(
    const string& line,
    CVcfData& data,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    vector<string> columns;
    NStr::Split(line, "\t", columns, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    if ( columns.size() < 8 ) {
        return false;
    }
    try {
        data.m_strLine = line;

        data.m_strChrom = columns[0];
        data.m_iPos = NStr::StringToInt( columns[1] );
        NStr::Split( columns[2], ";", data.m_Ids );
        if ( (data.m_Ids.size() == 1)  &&  (data.m_Ids[0] == ".") ) {
            data.m_Ids.clear();
        }
        data.m_strRef = columns[3];
        NStr::Split( columns[4], ",", data.m_Alt );
        if ( columns[5] != "." ) {
            data.m_pdQual = new double( NStr::StringToDouble( columns[5] ) );
        }
        data.m_strFilter = columns[6];

        vector<string> infos;
        if ( columns[7] != "." ) {
            NStr::Split( columns[7], ";", infos, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate );
            for ( vector<string>::iterator it = infos.begin();
                it != infos.end(); ++it )
            {
                string key, value;
                NStr::SplitInTwo( *it, "=", key, value );
                data.m_Info[key] = vector<string>();
                NStr::Split( value, ",", data.m_Info[key] );
            }
        }
        if ( columns.size() > 8 ) {
            NStr::Split( columns[8], ":", data.m_FormatKeys, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate );

            for ( size_t u=9; u < columns.size(); ++u ) {
                vector<string> values;
                NStr::Split( columns[u], ":", values, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate );
                data.m_GenotypeData[ m_GenotypeHeaders[u-9] ] = values;
            }
        }
    }
    catch ( ... ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Unable to parse given VCF data (syntax error).",
            ILineError::eProblem_GeneralParsingError));
        ProcessError(*pErr, pEC);
        return false;
    }

    if (!xNormalizeData(data, pEC)) {
        return false;
    }

    //assign set type:

    //test for all SNVs
    bool maybeAllSnv = (data.m_strRef.size() == 1);
    if (maybeAllSnv) {
        for (size_t u=0; u < data.m_Alt.size(); ++u) {
            if (data.m_Alt[u].size() != 1) {
                maybeAllSnv = false;
                break;
            }
        }
        if (maybeAllSnv) {
            data.m_SetType = CVcfData::ST_ALL_SNV;
            return true;
        }
    }

    //test for all mnvs:
    bool maybeAllMnv = true;
    size_t refSize = data.m_strRef.size();
    for (size_t u=0; u < data.m_Alt.size(); ++u) {
        if (data.m_Alt[u].size() != refSize) {
            maybeAllMnv = false;
            break;
        }
    }
    if (maybeAllMnv) {
        data.m_SetType = CVcfData::ST_ALL_MNV;
        return true;
    }

    //test for all insertions:
    bool maybeAllIns = true;
    for (size_t u=0; u < data.m_Alt.size(); ++u) {
        if (! NStr::StartsWith(data.m_Alt[u], data.m_strRef)) {
            maybeAllIns = false;
            break;
        }
    }
    if (maybeAllIns) {
        data.m_SetType = CVcfData::ST_ALL_INS;
        return true;
    }

    //test for all deletions:
    // note: even it is all deletions we are not able to process them
    // as such because those deletions would be at different ASN1
    // locations. Hence we punt to "indel" if there is more than one
    // alternative.
    bool maybeAllDel = false;
    for (size_t u=0; u < data.m_Alt.size(); ++u) {
        if (data.m_Alt.size() == 1  && data.m_Alt[0].empty()) {
            maybeAllDel = true;
        }
    }
    if (maybeAllDel) {
        data.m_SetType = CVcfData::ST_ALL_DEL;
        return true;
    }

    data.m_SetType = CVcfData::ST_MIXED;
    return true;
}


//  ---------------------------------------------------------------------------
bool
CVcfReader::xNormalizeData(
    CVcfData& data,
    ILineErrorListener* pEC)
//  ---------------------------------------------------------------------------
{
    // make sure none of the alternatives is equal to the reference:
    for (size_t u=0; u < data.m_Alt.size(); ++u) {
        if (data.m_Alt[u] == data.m_strRef) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                0,
                "CVcfReader::xNormalizeData: Invalid alternative.",
                ILineError::eProblem_GeneralParsingError));
            ProcessError(*pErr, pEC);
            return false;
        }
    }

    // normalize ref/alt by trimming common prefices and adjusting location
    bool trimComplete = false;
    while (!data.m_strRef.empty()) {
        char leadBase = data.m_strRef[0];
        for (size_t u=0; u < data.m_Alt.size(); ++u) {
            if (!NStr::StartsWith(data.m_Alt[u], leadBase)) {
                trimComplete = true;
                break;
            }
        }
        if (trimComplete) {
            break;
        }
        data.m_strRef = data.m_strRef.substr(1);
        for (size_t u=0; u < data.m_Alt.size(); ++u) {
            data.m_Alt[u] = data.m_Alt[u].substr(1);
        }
        data.m_iPos++;
    }

    //  normalize ref/alt by trimming common postfixes and adjusting location
    trimComplete = false;
    size_t refSize = data.m_strRef.size();
    size_t trimSize = 0;
    while (refSize > trimSize) {
        string postfix = data.m_strRef.substr(refSize-1-trimSize, trimSize+1);
        for (size_t u=0; u < data.m_Alt.size(); ++u) {
            size_t altSize = data.m_Alt[u].size();
            if (altSize < trimSize+1) {
                trimComplete = true;
                break;
            }
            string postfixA = data.m_Alt[u].substr(altSize-1-trimSize, trimSize+1);
            if (postfix != postfixA) {
                trimComplete = true;
                break;
            }
        }
        if (trimComplete) {
            break;
        }
        trimSize++;
    }
    if (trimSize > 0) {
        data.m_strRef =
            data.m_strRef.substr(0, data.m_strRef.size()-trimSize);
        for (size_t u=0; u < data.m_Alt.size(); ++u) {
            data.m_Alt[u] =
                data.m_Alt[u].substr(0, data.m_Alt[u].size()-trimSize);
        }
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool
CVcfReader::xAssignFeatureLocationSet(
    const CVcfData& data,
    CRef<CSeq_feat> pFeat )
//  ---------------------------------------------------------------------------
{
    CRef<CSeq_id> pId(CReadUtil::AsSeqId(data.m_strChrom, m_iFlags));

    //context:
    // we are trying to package all the allele of this feature into a single
    // variation_ref, hence, they all need a common location.
    // Referenced location differ between the different types of variations,
    // so we need to find the most specific variation type that describes them
    // all. Once the actual variation type has been found we can set the location
    // accordingly.

    // in practice, we will choose the common variation type if it is indeed
    // common for all the alleles. Otherwise, we just make it a MNV.

    if (data.m_SetType == CVcfData::ST_ALL_SNV) {
        //set location for SNVs
        pFeat->SetLocation().SetPnt().SetPoint(data.m_iPos-1);
        pFeat->SetLocation().SetPnt().SetId(*pId);
        return true;
    }
    if (data.m_SetType == CVcfData::ST_ALL_MNV) {
        //set location for MNV. This will be the location of the reference
        pFeat->SetLocation().SetInt().SetFrom(data.m_iPos-1);
        pFeat->SetLocation().SetInt().SetTo(
            static_cast<TSeqPos>(data.m_iPos + data.m_strRef.size() - 2));
        pFeat->SetLocation().SetInt().SetId(*pId);
        return true;
    }
    if (data.m_SetType == CVcfData::ST_ALL_INS) {
        //set location for INSs. Will always be a point!
        //m_iPos points to the 1-based position of the first
        //nt that is unique between alt and ref
        pFeat->SetLocation().SetPnt().SetPoint(data.m_iPos-1);
        pFeat->SetLocation().SetPnt().SetId(*pId);
        return true;
    }
    if (data.m_SetType == CVcfData::ST_ALL_DEL) {
        if (data.m_strRef.size() == 1) {
            //deletion of a single base
            pFeat->SetLocation().SetPnt().SetPoint(data.m_iPos-1);
            pFeat->SetLocation().SetPnt().SetId(*pId);
        }
        else {
            pFeat->SetLocation().SetInt().SetFrom(data.m_iPos-1);
            //-1 for 0-based,
            //another -1 for inclusive end-point ( i.e. [], not [) )
            pFeat->SetLocation().SetInt().SetTo(
                 static_cast<TSeqPos>(data.m_iPos -1 + data.m_strRef.length() - 1));
            pFeat->SetLocation().SetInt().SetId(*pId);
        }
        return true;
    }

    //default: For MNV's we will use the single starting point
    //NB: For references of size >=2, this location will not
    //match the reference allele.  Future Variation-ref
    //normalization code will address these issues,
    //and obviate the need for this code altogether.
    if (data.m_strRef.size() == 1) {
        //deletion of a single base
        pFeat->SetLocation().SetPnt().SetPoint(data.m_iPos-1);
        pFeat->SetLocation().SetPnt().SetId(*pId);
    }
    else {
        pFeat->SetLocation().SetInt().SetFrom(data.m_iPos-1);
        pFeat->SetLocation().SetInt().SetTo(
            static_cast<TSeqPos>(data.m_iPos -1 + data.m_strRef.length() - 1));
        pFeat->SetLocation().SetInt().SetId(*pId);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessScore(
    CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CSeq_feat::TExt& ext = pFeature->SetExt();
    if ( data.m_pdQual ) {
        ext.AddField( "score", *data.m_pdQual );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessFilter(
    CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if(!NStr::Equal(data.m_strFilter,".")) {
      CSeq_feat::TExt& ext = pFeature->SetExt();
      ext.AddField( "filter", data.m_strFilter );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessInfo(
    CVcfData& data,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    if (!xAssignVariantProps(data, pFeature)) {
        return false;
    }
    CSeq_feat::TExt& ext = pFeature->SetExt();
    if (data.m_Info.empty()) {
        return true;
    }
    vector<string> infos;
    for ( map<string,vector<string> >::const_iterator cit = data.m_Info.begin();
        cit != data.m_Info.end(); cit++ )
    {
        const string& key = cit->first;
        vector<string> value = cit->second;
        if ( value.empty() ) {
            infos.push_back( key );
        }
        else {
            string joined = NStr::Join( list<string>( value.begin(), value.end() ), "," );
            infos.push_back( key + "=" + joined );
        }
    }
    ext.AddField( "info", NStr::Join( infos, ";" ) );
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessTrackLine(
    const string& strLine,
    CSeq_annot& /*current*/)
//  ----------------------------------------------------------------------------
{
    if (!xIsTrackLine(strLine)) {
        return false;
    }
    vector<string> parts;
    CReadUtil::Tokenize( strLine, " \t", parts );
    if (parts.size() >= 3) {
        const string digits("0123456789");
        bool col2_is_numeric =
            (string::npos == parts[1].find_first_not_of(digits));
        bool col3_is_numeric =
            (string::npos == parts[2].find_first_not_of(digits));
        if (col2_is_numeric  &&  col3_is_numeric) {
            return false;
        }
    }
    if (!CReaderBase::xParseTrackLine(strLine)) {
        CReaderMessage warning(
            eDiag_Warning,
            m_uLineNumber,
            "Bad track line: Expected \"track key1=value1 key2=value2 ...\". Ignored.");
        m_pMessageHandler->Report(warning);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::xProcessFormat(
    CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if (data.m_FormatKeys.empty()) {
        return true;
    }

    CSeq_feat::TExt& ext = pFeature->SetExt();
    ext.AddField("format", data.m_FormatKeys);

    CRef<CUser_field> pGenotypeData( new CUser_field );
    pGenotypeData->SetLabel().SetStr("genotype-data");

    for ( CVcfData::GTDATA::const_iterator cit = data.m_GenotypeData.begin();
            cit != data.m_GenotypeData.end(); ++cit) {
        pGenotypeData->AddField(cit->first,cit->second);
    }
    ext.SetData().push_back(pGenotypeData);
    return true;
}


//  ----------------------------------------------------------------------------
bool CVcfReader::xAssigndbSNPTag(
    const vector<string>& ids,
    CRef<CDbtag> pDbtag) const
//  ----------------------------------------------------------------------------
{
    for (const string& id : ids) {
        if (NStr::StartsWith(id, "rs") ||
            NStr::StartsWith(id, "ss") )
        {
            try {
                const int idval = NStr::StringToInt(id.substr(2));
                pDbtag->SetDb("dbSNP");
                pDbtag->SetTag().SetId(idval);
            }
            catch (...) {
                continue;
            }
            return true;
        }
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariationIds(
    CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if ( data.m_Ids.empty() ) {
        return true;
    }
    CVariation_ref& variation = pFeature->SetData().SetVariation();
//    CVariation_ref::TVariant_prop& var_prop = variation.SetVariant_prop();
//    var_prop.SetVersion( 5 );

    auto it = data.m_Info.find("SOURCE");

    if (data.m_Info.end() != it) {
        vector<string> sources = it->second;
        if (sources.size() > 0 &&
            NStr::Equal(sources.front(), "dbsnp"))
        {
            CRef<CDbtag> pDbtag = Ref(new CDbtag());
            if (xAssigndbSNPTag(data.m_Ids, pDbtag)) {
                variation.SetId(pDbtag.GetNCObject());
                return true;
            }
        }
    }


    if ( data.m_Info.find( "DB" ) != data.m_Info.end() ) {
        string id = data.m_Ids[0];
        NStr::ToLower(id);
        if (NStr::StartsWith(id, "rs")  ||  NStr::StartsWith(id, "ss")) {
            variation.SetId().SetDb("dbSNP");
        }
        else {
            variation.SetId().SetDb( "dbVar" );
        }
    }
    else if ( data.m_Info.find( "H2" ) != data.m_Info.end() ) {
        variation.SetId().SetDb( "HapMap2" );
    }
    else {
        variation.SetId().SetDb( "local" );
    }
    variation.SetId().SetTag().SetStr( data.m_Ids[0] );

    for ( size_t i=1; i < data.m_Ids.size(); ++i ) {
        if ( data.m_Info.find( "DB" ) != data.m_Info.end()
            &&  data.m_Info.find( "H2" ) != data.m_Info.end() )
        {
            variation.SetId().SetDb( "HapMap2" );
        }
        else {
            variation.SetId().SetDb( "local" );
        }
        variation.SetId().SetTag().SetStr( data.m_Ids[i] );
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool
CVcfReader::xAssignVariantProps(
    CVcfData& data,
    CRef<CSeq_feat> pFeat)
//  ----------------------------------------------------------------------------
{
    typedef CVariantProperties VP;

    CVcfData::INFOS& infos = data.m_Info;
    VP& props = pFeat->SetData().SetVariation().SetVariant_prop();
    CVcfData::INFOS::iterator it;

    props.SetResource_link() = 0;
    props.SetGene_location() = 0;
    props.SetEffect() = 0;
    props.SetMapping() = 0;
    props.SetFrequency_based_validation() = 0;
    props.SetGenotype() = 0;
    props.SetQuality_check() = 0;

    //byte F0
    props.SetVersion() = 5;

    //superbyte F1
    it = infos.find("SLO");
    if (infos.end() != it) {
        props.SetResource_link() |= VP::eResource_link_submitterLinkout;
        infos.erase(it);
    }
    it = infos.find("S3D");
    if (infos.end() != it) {
        props.SetResource_link() |= VP::eResource_link_has3D;
        infos.erase(it);
    }
    it = infos.find("TPA");
    if (infos.end() != it) {
        props.SetResource_link() |= VP::eResource_link_provisional;
        infos.erase(it);
    }
    it = infos.find("PM");
    if (infos.end() != it) {
        props.SetResource_link() |= VP::eResource_link_preserved;
        infos.erase(it);
    }
    it = infos.find("CLN");
    if (infos.end() != it) {
        props.SetResource_link() |= VP::eResource_link_clinical;
        infos.erase(it);
    }
    //todo: INFO ID=PMC
    it = infos.find("PMC");
    if (infos.end() != it) {
        infos.erase(it);
    }
    it = infos.find("PMID");
    if (infos.end() != it) {
        vector<string> pmids = it->second;
        for (vector<string>::const_iterator cit = pmids.begin();
            cit != pmids.end(); ++cit)
        {
            try {
                string db, tag;
                NStr::SplitInTwo(*cit, ":", db, tag);
                if (db != "PM") {
                    CReaderMessage warning(
                        eDiag_Warning,
                        m_uLineNumber,
                        "CVcfReader::xAssignVariantProps: Invalid PMID database ID.");
                    m_pMessageHandler->Report(warning);
                    continue;
                }
                CRef<CDbtag> pDbtag(new CDbtag);
                pDbtag->SetDb(db);
                pDbtag->SetTag().SetId(
                    NStr::StringToInt(tag));
                pFeat->SetDbxref().push_back(pDbtag);
            }
            catch(...) {}
        }
        infos.erase(it);
    }

    xAssignVariantSource(data, pFeat);

    //superbyte F2
    it = infos.find("R5");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eGene_location_near_gene_5;
        infos.erase(it);
    }
    it = infos.find("R3");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eGene_location_near_gene_3;
        infos.erase(it);
    }
    it = infos.find("INT");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eGene_location_intron;
        infos.erase(it);
    }
    it = infos.find("DSS");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eGene_location_donor;
        infos.erase(it);
    }
    it = infos.find("ASS");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eGene_location_acceptor;
        infos.erase(it);
    }
    it = infos.find("U5");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eGene_location_utr_5;
        infos.erase(it);
    }
    it = infos.find("U3");
    if (infos.end() != it) {
        props.SetGene_location() |= CVariantProperties::eGene_location_utr_3;
        infos.erase(it);
    }

    it = infos.find("SYN");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eEffect_synonymous;
        infos.erase(it);
    }
    it = infos.find("NSN");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eEffect_stop_gain;
        infos.erase(it);
    }
    it = infos.find("NSM");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eEffect_missense;
        infos.erase(it);
    }
    it = infos.find("NSF");
    if (infos.end() != it) {
        props.SetGene_location() |= VP::eEffect_frameshift;
        infos.erase(it);
    }

    //byte F3
    it = infos.find("WGT");
    if (infos.end() != it) {
        int weight = NStr::StringToInt( infos["WGT"][0] );
        switch(weight) {
        default:
            break;
        case 1:
            props.SetMap_weight() = VP::eMap_weight_is_uniquely_placed;
            infos.erase(it);
            break;
        case 2:
            props.SetMap_weight() = VP::eMap_weight_placed_twice_on_same_chrom;
            infos.erase(it);
            break;
        case 3:
            props.SetMap_weight() = VP::eMap_weight_placed_twice_on_diff_chrom;
            infos.erase(it);
            break;
        case 10:
            props.SetMap_weight() = VP::eMap_weight_many_placements;
            break;
        }
    }

    it = infos.find("ASP");
    if (infos.end() != it) {
        props.SetMapping() |= VP::eMapping_is_assembly_specific;
        infos.erase(it);
    }
    it = infos.find("CFL");
    if (infos.end() != it) {
        props.SetMapping() |= VP::eMapping_has_assembly_conflict;
        infos.erase(it);
    }
    it = infos.find("OTH");
    if (infos.end() != it) {
        props.SetMapping() |= VP::eMapping_has_other_snp;
        infos.erase(it);
    }

    //byte F4
    it = infos.find("OTH");
    if (infos.end() != it) {
        props.SetFrequency_based_validation() |= VP::eFrequency_based_validation_above_5pct_all;
        infos.erase(it);
    }
    it = infos.find("G5A");
    if (infos.end() != it) {
        props.SetFrequency_based_validation() |= VP::eFrequency_based_validation_above_5pct_1plus;
        infos.erase(it);
    }
    it = infos.find("VLD");
    if (infos.end() != it) {
        props.SetFrequency_based_validation() |= VP::eFrequency_based_validation_validated;
        infos.erase(it);
    }
    it = infos.find("MUT");
    if (infos.end() != it) {
        props.SetFrequency_based_validation() |= VP::eFrequency_based_validation_is_mutation;
        infos.erase(it);
    }
    it = infos.find("GMAF");
    if (infos.end() != it) {
        props.SetAllele_frequency() = NStr::StringToDouble(infos["GMAF"][0]);
        infos.erase(it);
    }

    //byte F5
    it = infos.find("GNO");
    if (infos.end() != it) {
        props.SetGenotype() |= VP::eGenotype_has_genotypes;
        infos.erase(it);
    }
    it = infos.find("HD");
    if (infos.end() != it) {
        props.SetResource_link() |= VP::eResource_link_genotypeKit;
        infos.erase(it);
    }

    //byte F6
    if (infos.end() != infos.find("PH3")) {
        CRef<CDbtag> pDbtag(new CDbtag);
        pDbtag->SetDb("BioProject");
        pDbtag->SetTag().SetId(60835);
        pFeat->SetData().SetVariation().SetOther_ids().push_back(pDbtag);
    }
    if (infos.end() != infos.find("KGPhase1")) {
        CRef<CDbtag> pDbtag(new CDbtag);
        pDbtag->SetDb("BioProject");
        pDbtag->SetTag().SetId(28889);
        pFeat->SetData().SetVariation().SetOther_ids().push_back(pDbtag);
    }

    //byte F7

    //byte F8
    //no relevant information found in VCF

    //byte F9
    it = infos.find("GCF");
    if (infos.end() != it) {
        props.SetQuality_check() |= VP::eQuality_check_genotype_conflict;
        infos.erase(it);
    }
    it = infos.find("NOV");
    if (infos.end() != it) {
        props.SetQuality_check() |= VP::eQuality_check_non_overlapping_alleles;
        infos.erase(it);
    }
    it = infos.find("WTD");
    if (infos.end() != it) {
        props.SetQuality_check() |= VP::eQuality_check_withdrawn_by_submitter;
        infos.erase(it);
    }
    it = infos.find("NOC");
    if (infos.end() != it) {
        props.SetQuality_check() |= VP::eQuality_check_contig_allele_missing;
        infos.erase(it);
    }
    return true;
}


//  ----------------------------------------------------------------------------
void CVcfReader::xAssignVariantSource(CVcfData& data,
    CRef<CSeq_feat> pFeat)
//  ----------------------------------------------------------------------------
{
    CVcfData::INFOS& infos = data.m_Info;
    auto it = infos.find("SOURCE");
    if (infos.end() != it) {
        vector<string> sources = it->second;
        if (sources.size() > 0 &&
            NStr::Equal(sources.front(),"dbsnp"))
        {
            bool valid_id=false;
            CRef<CDbtag> pDbtag(new CDbtag());
            if (xAssigndbSNPTag(data.m_Ids, pDbtag)) {
                pFeat->SetDbxref().push_back(pDbtag);
                valid_id = true;
            }

            if (!valid_id) {
                CReaderMessage warning(
                    eDiag_Warning,
                    m_uLineNumber,
                    "CVcfReader::xAssignVariantProps: No valid dbSNP identifier");
                m_pMessageHandler->Report(warning);
            }
            infos.erase(it);
        }
    }
}


//  ----------------------------------------------------------------------------
bool CVcfReader::xIsCommentLine(
    const CTempString& strLine)
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(strLine, "#CHROM")) {
        return false;
    }
    return CReaderBase::xIsCommentLine(strLine);
}

END_objects_SCOPE
END_NCBI_SCOPE
