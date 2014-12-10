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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write source qualifiers
 *
 */

#include <ncbi_pch.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>

#include <objmgr/seqdesc_ci.hpp>

#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_single_data.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/writers/src_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CSrcWriter::HANDLERMAP CSrcWriter::sHandlerMap;
CSrcWriter::NAMEMAP CSrcWriter::sFieldnameToColname;
//  ----------------------------------------------------------------------------
//  Default Fields:
//  ----------------------------------------------------------------------------
static const string arrDefaultFields[] = {
    "gi",
    "organism",
    "taxid",
    "specimen-voucher",
    "culture-collection",
    "bio-material",
    "strain",
    "sub-strain",
    "isolate",
    "sub-species",
    "variety",
    "forma",
    "cultivar",
    "ecotype",
    "serotype",
    "serovar",
    "type-material",
    "old-name"
};


const size_t countDefaultFields = sizeof(arrDefaultFields)/sizeof(string);

const CSrcWriter::FIELDS CSrcWriter::sDefaultFields(
    arrDefaultFields, arrDefaultFields + countDefaultFields);

//  ----------------------------------------------------------------------------
bool CSrcWriter::WriteBioseqHandle( 
    CBioseq_Handle bsh,
    const FIELDS& desiredFields,
    CNcbiOstream& out)
//  ----------------------------------------------------------------------------
{
    FIELDS colNames = xProcessFieldNames(desiredFields);

    if (!xGather(bsh, colNames)) {
        return false;
    }
    if (!xFormatTabDelimited(colNames, out)) {
        return false;
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool CSrcWriter::WriteBioseqHandles( 
    const vector<CBioseq_Handle>& vecBsh,
    const FIELDS& desiredFields,
    CNcbiOstream& out,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    typedef vector<CBioseq_Handle> HANDLES;
    FIELDS colNames = xProcessFieldNames(desiredFields);

    for (HANDLES::const_iterator it = vecBsh.begin(); it != vecBsh.end(); ++it) {
        if (!xGather(*it, colNames)) {
            return false;
        }
    }
    if (!xFormatTabDelimited(colNames,out)) {
        return false;
    }
    return true;
};

//  ----------------------------------------------------------------------------
CSrcWriter::FIELDS CSrcWriter::xProcessFieldNames(
    const FIELDS& desiredFields)
//  ----------------------------------------------------------------------------
{
    FIELDS colNames;
    for (FIELDS::const_iterator cit = desiredFields.begin();
                                cit != desiredFields.end();
                                ++cit)  {

        NAMEMAP::const_iterator mapIterator = sFieldnameToColname.find(xCompressFieldName(*cit));

        if (mapIterator != sFieldnameToColname.end()) {
          colNames.push_back(mapIterator->second);
        } else {
          colNames.push_back(*cit);
        }
    }
    return colNames;
}

//  ---------------------------------------------------------------------------
void CSrcWriter::xPrepareTableColumn(
    const string& colName,
    const string& colTitle,
    const string& colDefault)
//  ---------------------------------------------------------------------------
{
    COLUMNMAP::iterator it = mColnameToIndex.find(colName);
    if (it == mColnameToIndex.end()) {
        CRef<CSeqTable_column> pColumn(new CSeqTable_column());
        pColumn->SetHeader().SetField_name(colName);
        pColumn->SetHeader().SetTitle(colTitle);
        pColumn->SetDefault().SetString(colDefault);
        mColnameToIndex[colName] = mSrcTable->GetColumns().size();
        mSrcTable->SetColumns().push_back(pColumn);
    }
    unsigned int index = mColnameToIndex[colName];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString();
    while (column.GetData().GetString().size() < mSrcTable->GetNum_rows()) {
        column.SetData().SetString().push_back(colDefault);
    }
}

//  ----------------------------------------------------------------------------
void CSrcWriter::xInit()
//  ----------------------------------------------------------------------------
{
    if (sHandlerMap.empty()) {
        sHandlerMap["db"] = &CSrcWriter::xGatherDb;
        sHandlerMap["taxid"] = &CSrcWriter::xGatherTaxonId;
        sHandlerMap["div"] = &CSrcWriter::xGatherDivision;
        sHandlerMap["division"] = &CSrcWriter::xGatherDivision;
        sHandlerMap["genome"] = &CSrcWriter::xGatherGenome;
        sHandlerMap["lineage"] = &CSrcWriter::xGatherOrgnameLineage;
        sHandlerMap["common"] = &CSrcWriter::xGatherOrgCommon;
        sHandlerMap["org.common"] = &CSrcWriter::xGatherOrgCommon;
        sHandlerMap["origin"] = &CSrcWriter::xGatherOrigin;
        sHandlerMap["pcr-primers"] = &CSrcWriter::xGatherPcrPrimers;
        sHandlerMap["organism"] = &CSrcWriter::xGatherTaxname;
        sHandlerMap["org.common"] = &CSrcWriter::xGatherOrgCommon;
    }


    if (sFieldnameToColname.empty()) {

        sFieldnameToColname["organism"] = "organism";
        sFieldnameToColname["taxname"] = "organism";
        sFieldnameToColname["org.taxname"] = "organism";
        sFieldnameToColname["org"] = "organism";
        sFieldnameToColname["taxid"] = "taxid";
        sFieldnameToColname["div"] = "division";
        sFieldnameToColname["division"] = "division";
        sFieldnameToColname["genome"] = "genome";
        sFieldnameToColname["pcrprimers"] = "pcr-primers";
        sFieldnameToColname["subtype"] = "subtype";
        sFieldnameToColname["org.db"] = "db";
        sFieldnameToColname["org.common"] = "common";

        // Subsource
        sFieldnameToColname["insertionseq"] = "insertion-seq";
        sFieldnameToColname["plasmid"] = "plasmid";
        sFieldnameToColname["transposon"] = "transposon";
        sFieldnameToColname["subclone"] = "sub-clone";
        sFieldnameToColname["subsourcenote"] = "subsource-note";
        sFieldnameToColname["subsrcnote"] = "subsource-note";

        NAMELIST nameList = xGetOrgModSubtypeNames();
        for(NAMELIST::const_iterator cit=nameList.begin();
                                     cit != nameList.end();
                                     ++cit) {
            if (*cit != "other") {
              sFieldnameToColname[xCompressFieldName(*cit)] = *cit;
            }
        }

        nameList = xGetSubSourceSubtypeNames();
        for(NAMELIST::const_iterator cit=nameList.begin();
                                     cit != nameList.end();
                                     ++cit) {
            if(*cit != "other") {
                sFieldnameToColname[xCompressFieldName(*cit)] = *cit;
            }
        }
    } 


    mSrcTable.Reset(new CSeq_table());
    mSrcTable->SetNum_rows(0);

    const string colName = "id";
    const string displayName = "accession";
    const string defaultValue = "";
    xPrepareTableColumn(colName, displayName, defaultValue);
}

//  ----------------------------------------------------------------------------
CSrcWriter::FIELDS CSrcWriter::GetOrderedFieldNames()
//  ----------------------------------------------------------------------------
{
    FIELDS orderedFields;
    set<string> defaultSet;

    for (int i=0; i < countDefaultFields; ++i) {
        string colName = arrDefaultFields[i];
        orderedFields.push_back(colName);
        defaultSet.insert(colName);
    }

   
    FIELDS lexicalFields;
    lexicalFields.push_back("organism");
    lexicalFields.push_back("genome");
    lexicalFields.push_back("pcr-primers");
    lexicalFields.push_back("db");
    lexicalFields.push_back("common");
    lexicalFields.push_back("lineage");
    lexicalFields.push_back("origin");
    lexicalFields.push_back("note");
    lexicalFields.push_back("subsource-note");
    lexicalFields.push_back("division");
    
    NAMELIST nameList = xGetOrgModSubtypeNames();
    for(NAMELIST::const_iterator cit=nameList.begin();
                                 cit != nameList.end();
                                 ++cit) {
        if (*cit != "other" && *cit != "common") {
           lexicalFields.push_back(*cit);
        }
    }

    nameList = xGetSubSourceSubtypeNames();
    for(NAMELIST::const_iterator cit=nameList.begin();
                                 cit != nameList.end();
                                 ++cit) {
        if(*cit != "other") {
            lexicalFields.push_back(*cit);
        }
    }
    
    sort(lexicalFields.begin(), lexicalFields.end());
  
    for (FIELDS::const_iterator cit = lexicalFields.begin();
         cit != lexicalFields.end();
         ++cit)
    {
        if (defaultSet.find(*cit) == defaultSet.end()) {
            orderedFields.push_back(*cit);
        }
    }
    return orderedFields;
}

//  ----------------------------------------------------------------------------
CSrcWriter::NAMELIST CSrcWriter::xGetOrgModSubtypeNames() 
//  ----------------------------------------------------------------------------
{
    NAMELIST subtypeNames;

    typedef const CEnumeratedTypeValues::TValues TVALUES;
    TVALUES nameValPairs = COrgMod::ENUM_METHOD_NAME(ESubtype)()->GetValues();

    for (TVALUES::const_iterator cit = nameValPairs.begin();
                                 cit != nameValPairs.end();
                                 ++cit) {

      subtypeNames.push_back(cit->first);    
    }
    return subtypeNames;
}


//  ----------------------------------------------------------------------------
CSrcWriter::NAMELIST CSrcWriter::xGetSubSourceSubtypeNames() 
//  ----------------------------------------------------------------------------
{
    NAMELIST subtypeNames;

    typedef const CEnumeratedTypeValues::TValues TVALUES;
    TVALUES nameValPairs = CSubSource::ENUM_METHOD_NAME(ESubtype)()->GetValues();

    for (TVALUES::const_iterator cit = nameValPairs.begin();
                                 cit != nameValPairs.end();
                                 ++cit) {

      subtypeNames.push_back(cit->first);    
    }
    return subtypeNames;
}

//  ----------------------------------------------------------------------------
string CSrcWriter::xCompressFieldName(
    const string& fieldName) 
//  ----------------------------------------------------------------------------
{
    string name = NStr::TruncateSpaces(fieldName);
    NStr::ToLower(name);
    NStr::ReplaceInPlace(name,"\"","");
    NStr::ReplaceInPlace(name,"-","");
    NStr::ReplaceInPlace(name, "_", "");
    NStr::ReplaceInPlace(name, " ", "");

    return name;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xIsSubsourceTypeSuppressed(
    CSubSource::TSubtype subtype)
//  ----------------------------------------------------------------------------
{
    if (CSubSource::IsDiscouraged(subtype)) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xIsOrgmodTypeSuppressed(
    COrgMod::TSubtype subtype)
//  ----------------------------------------------------------------------------
{
    if (COrgMod::eSubtype_old_name == subtype) {
        return false;
    }
    if (COrgMod::IsDiscouraged(subtype)) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
CSrcWriter::HANDLER CSrcWriter::xGetHandler(
    const string& fieldName)
//  ----------------------------------------------------------------------------
{
    HANDLERMAP::iterator it = sHandlerMap.find(fieldName);
    if (it != sHandlerMap.end()) {
        return it->second;
    }
    if (COrgMod::IsValidSubtypeName(fieldName, COrgMod::eVocabulary_insdc)) {
        return &CSrcWriter::xGatherOrgModFeat;
    }
    if (CSubSource::IsValidSubtypeName(fieldName, CSubSource::eVocabulary_insdc)) {
        return &CSrcWriter::xGatherSubtypeFeat;
    }
    return 0;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xHandleSourceField(
    const CBioSource& src, 
    const string& fieldName,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    HANDLER pHandler = xGetHandler(fieldName);
    if (!pHandler) {
        CSrcError* pE = CSrcError::Create(
            ncbi::eDiag_Error,
            "Unable to find handler for field \"" + fieldName + "\".");
        pEC->PutError(*pE);
        delete pE;
        return false;
    }


    return (this->*pHandler)(src, fieldName, pEC);
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGather(
    CBioseq_Handle bsh,
    const FIELDS& desiredFields,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    // for each of biosources we may create individual record
    // with the same ID
    bool hasGi = ( find(desiredFields.begin(),desiredFields.end(),"gi") != desiredFields.end() ); 

    for (CSeqdesc_CI sdit(bsh, CSeqdesc::e_Source); sdit; ++sdit) {
        if (!xGatherId(bsh)) {
            return false;
        }
        if (hasGi) {
            if (!xGatherGi(bsh)) {
                return false;
            }
        }

        const CBioSource& src = sdit->GetSource();
        for (FIELDS::const_iterator cit = desiredFields.begin();
                cit != desiredFields.end(); ++cit) {
            if (*cit == "gi") {
                continue;
            }
            if (!xHandleSourceField(src, *cit)) {
                return false;
            }
        }
        mSrcTable->SetNum_rows(mSrcTable->GetNum_rows()+1);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherId(
    CBioseq_Handle bsh,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    static const string colName = "id";

    string label;
    bsh.GetSeqId()->GetLabel(&label, CSeq_id::eContent);
    xAppendColumnValue(colName, label);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherGi(
    CBioseq_Handle bsh,
    IMessageListener*)
//  ----------------------------------------------------------------------------

{
    static const string colName = "gi";

    
    ITERATE( CBioseq_Handle::TId, it, bsh.GetId() ){
        if( it->IsGi() ){
            string label;
            it->GetSeqId()->GetLabel(&label, CSeq_id::eContent);
            const string displayName = "gi";
            const string defaultValue = "";
            xPrepareTableColumn(colName, displayName, defaultValue);
            xAppendColumnValue(colName, label);
            return true;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherTaxname(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "organism";
    const string defaultValue = "";

    if (!src.IsSetTaxname()) {
        return true;
    }
    string value = src.GetTaxname();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgCommon(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "common";
    const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetCommon()) {
        return true;
    }
    string value = src.GetOrg().GetCommon();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgnameLineage(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "lineage";
    const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetOrgname()  
            ||  !src.GetOrg().GetOrgname().IsSetLineage()) {
        return true;
    }
    string value = src.GetOrg().GetOrgname().GetLineage();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherDivision(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "division";
    const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDivision()) {
        return true;
    }
    string value = src.GetOrg().GetDivision();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherGenome(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "genome";
    const string defaultValue = "";

    if (!src.IsSetGenome()) {
        return true;
    }
    string value = CBioSource::GetOrganelleByGenome(src.GetGenome());
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrigin(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "origin";
    const string defaultValue = "";

    if (!src.IsSetOrigin()) {
        return true;
    }
    string value = CBioSource::GetStringFromOrigin(src.GetOrigin());
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherSubtypeFeat(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{


    if ( !src.IsSetSubtype() ) {
        return true;
    }

    CSubSource::TSubtype subtype = CSubSource::GetSubtypeValue(colName,CSubSource::eVocabulary_insdc);

    if ( xIsSubsourceTypeSuppressed(subtype) ) {
        return true;
    }

    typedef list<CRef<CSubSource> > SUBSOURCES;
    const SUBSOURCES& subsources = src.GetSubtype();

    typedef map<string, int> INDEXCOUNTER;
    INDEXCOUNTER indexCounter;
    
    string key = colName;
    for (SUBSOURCES::const_iterator cit = subsources.begin();
        cit != subsources.end(); ++cit) {
    
        const CSubSource& subsrc = **cit;
        if (subsrc.GetSubtype() != subtype) {
            continue;
        }

        INDEXCOUNTER::iterator it = indexCounter.find(key);
        if (it != indexCounter.end()) {
            int index = it->second;
            indexCounter[key] = index+1;
            key += "#" + NStr::IntToString(index);
        }
        else {
            indexCounter[key] = 1;
        }

        string value = "";
        if (subsrc.IsSetName()) {
            value = subsrc.GetName();
        }
        if (value.empty()  &&  CSubSource::NeedsNoText(subsrc.GetSubtype())) {
            value = "true";
        }        
        xPrepareTableColumn(key, key, "");
        xAppendColumnValue(key, value);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgModFeat(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    if ( !src.IsSetOrgMod() ) {
        return true;
    }

    COrgMod::TSubtype subtype = COrgMod::GetSubtypeValue(colName,COrgMod::eVocabulary_insdc);

    if ( xIsOrgmodTypeSuppressed(subtype) ) {
        return true;
    }

    typedef list<CRef<COrgMod> > ORGMODS;
    const ORGMODS& orgmods = src.GetOrgname().GetMod();

    typedef map<string, int> INDEXCOUNTER;
    INDEXCOUNTER indexCounter;

    string key = colName;
    for (ORGMODS::const_iterator cit = orgmods.begin();
         cit != orgmods.end(); ++cit) {
         const COrgMod& orgmod = **cit;

        if (orgmod.GetSubtype() != subtype) {
            continue;
        }

        INDEXCOUNTER::iterator it = indexCounter.find(key);
        if (it != indexCounter.end()) {
            int index = it->second;
            indexCounter[key] = index+1;
            key += "#" + NStr::IntToString(index);
        }
        else {
            indexCounter[key] = 1;
        }

        string value = orgmod.GetSubname();
        xPrepareTableColumn(key, key, "");
        xAppendColumnValue(key, value);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherDb(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
  
    static const string displayName = "db";
    static const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDb()) {
        return true;
    }

    typedef vector< CRef< CDbtag > > DBTAGS;
    const DBTAGS& tags = src.GetOrg().GetDb();
    for (DBTAGS::const_iterator cit = tags.begin(); cit != tags.end(); ++cit) {
        const CDbtag& tag = **cit;
        if (!tag.IsSetDb()  ||  tag.GetDb().empty()  ||  !tag.IsSetTag()) {
            continue;
        }
        const CObject_id& objid = tag.GetTag();
        string dbtagStr;
        switch (objid.Which()) {
        default:
            break;
        case CObject_id::e_Str:
            if (objid.GetStr().empty()) {
                continue;
            }
            dbtagStr = objid.GetStr();
            break;
        case CObject_id::e_Id:
            dbtagStr = NStr::IntToString(objid.GetId());
            break;
        }
        string curColName = colName;
        string curDisplayName = displayName;
        curColName += tag.GetDb();
        curDisplayName += tag.GetDb();
        xPrepareTableColumn(curColName, curDisplayName, "");
        xAppendColumnValue(curColName, dbtagStr);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherTaxonId(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    static const string displayName = "taxid";
    static const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDb()) {
        return true;
    }

    typedef vector< CRef< CDbtag > > DBTAGS;
    const DBTAGS& tags = src.GetOrg().GetDb();
    string taxonIdStr;
    for (DBTAGS::const_iterator cit = tags.begin(); cit != tags.end(); ++cit) {
        const CDbtag& tag = **cit;
        if (!tag.IsSetDb()  ||  tag.GetDb() != "taxon") {
            continue;
        }
        const CObject_id& objid = tag.GetTag();
        switch (objid.Which()) {
        default:
            return false;
        case CObject_id::e_Str:
            if (objid.GetStr().empty()) {
                continue;
            }
            taxonIdStr = objid.GetStr();
            break;
        case CObject_id::e_Id:
            taxonIdStr = NStr::IntToString(objid.GetId());
            break;
        }
        break;
    }
    string curDisplayName = displayName;
    xPrepareTableColumn(colName, displayName, "");
    xAppendColumnValue(colName, taxonIdStr);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherPcrPrimers(
    const CBioSource& src,
    const string& colName,
    IMessageListener*)
//  ----------------------------------------------------------------------------
{
    const string pcrPrimersFwdNames = "pcr-primers.names.fwd";
    const string pcrPrimersFwdSequences = "pcr-primers.sequences.fwd";
    const string pcrPrimersRevNames = "pcr-primers.names.reverse";
    const string pcrPrimersRevSequences = "pcr-primers.sequences.reverse";

    unsigned int columnSetCounter = 0;

    if (!src.IsSetPcr_primers()) {
        return true;
    }
    string fwdName, fwdSequence, revName, revSequence;
    const CPCRReactionSet& pcrset = src.GetPcr_primers();

    typedef list<CRef<CPCRReaction> > REACTIONS;
    const REACTIONS& reactions = pcrset.Get();
    for (REACTIONS::const_iterator cit = reactions.begin();
            cit != reactions.end(); ++cit) {
        const CPCRReaction& reaction = **cit;
        if (reaction.IsSetForward()) {
            if (fwdName.empty()) {
                fwdName += ";";
                fwdSequence += ";";
            }
            fwdName += CSrcWriter::xPrimerSetNames(reaction.GetForward());
            fwdSequence += CSrcWriter::xPrimerSetSequences(reaction.GetForward());
        }
        if (reaction.IsSetReverse()) {
            if (revName.empty()) {
                revName += ";";
                revSequence += ";";
            }
            revName += CSrcWriter::xPrimerSetNames(reaction.GetReverse());
            revSequence += CSrcWriter::xPrimerSetSequences(reaction.GetReverse());
       }
    }
    string keyPcrPrimersFwdNames = pcrPrimersFwdNames;
    string keyPcrPrimersFwdSequences = pcrPrimersFwdSequences;
    string keyPcrPrimersRevNames = pcrPrimersRevNames;
    string keyPcrPrimersRevSequences = pcrPrimersRevSequences;
    if (columnSetCounter > 0) {
        keyPcrPrimersFwdNames += "#" + NStr::IntToString(columnSetCounter);
        keyPcrPrimersFwdSequences += "#" + NStr::IntToString(columnSetCounter);
        keyPcrPrimersRevNames += "#" + NStr::IntToString(columnSetCounter);
        keyPcrPrimersRevSequences += "#" + NStr::IntToString(columnSetCounter);
    }
    xPrepareTableColumn(
        keyPcrPrimersFwdNames, keyPcrPrimersFwdNames, "");
    xAppendColumnValue(keyPcrPrimersFwdNames, fwdName);
    xPrepareTableColumn(
        keyPcrPrimersFwdSequences, keyPcrPrimersFwdSequences, "");
    xAppendColumnValue(keyPcrPrimersFwdSequences, fwdSequence);

    xPrepareTableColumn(
        keyPcrPrimersRevNames, keyPcrPrimersRevNames, "");
    xAppendColumnValue(keyPcrPrimersRevNames, revName);
    xPrepareTableColumn(
        keyPcrPrimersRevSequences, keyPcrPrimersRevSequences, "");
    xAppendColumnValue(keyPcrPrimersRevSequences, revSequence);
    return true;
}

//  ----------------------------------------------------------------------------
string CSrcWriter::xPrimerSetNames(const CPCRPrimerSet& pset)
//  ----------------------------------------------------------------------------
{
    string names = "";
    typedef list<CRef<CPCRPrimer> > PRIMERS;
    const PRIMERS& primers = pset.Get();
    for (PRIMERS::const_iterator cit = primers.begin(); 
            cit != primers.end(); ++cit) {
        const CPCRPrimer& primer = **cit;
        names += ",";
        if (primer.IsSetName()) {
            names += primer.GetName();
        }
    }
    return names.substr(1);
}

//  ----------------------------------------------------------------------------
string CSrcWriter::xPrimerSetSequences(const CPCRPrimerSet& pset)
//  ----------------------------------------------------------------------------
{
    string sequences = "";
    typedef list<CRef<CPCRPrimer> > PRIMERS;
    const PRIMERS& primers = pset.Get();
    for (PRIMERS::const_iterator cit = primers.begin(); 
            cit != primers.end(); ++cit) {
        const CPCRPrimer& primer = **cit;
        sequences += ",";
        if (primer.IsSetSeq()) {
            sequences += primer.GetSeq();
        }
    }
    return sequences.substr(1);
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xFormatTabDelimited(
    const FIELDS& colStubs,
    CNcbiOstream& out)
//  ----------------------------------------------------------------------------
{
    // Print columns in the order given in colStubs
    map<string,NAMELIST >  ColstubToColnames;
    typedef map<string,NAMELIST > COLSTUBNAMESMAP;

    // First loop over all column names and insert them into a 
    // map whose keys are the column-name stubs
    for (COLUMNMAP::const_iterator cit=mColnameToIndex.begin();
                                   cit != mColnameToIndex.end();
                                   ++cit) {
        string colName = cit->first;
        string colStub = xGetColStub(colName);
        if (ColstubToColnames.find(colStub) == ColstubToColnames.end()) {
            ColstubToColnames[colStub] = NAMELIST(1,colName);
        } else {
            ColstubToColnames[colStub].push_back(colName);
        }
    }

    // Iterate over the colStub container and check to see if the 
    // stubs are keys in the map. If so, insert the corresponding values 
    // (the column names) into a list.
    NAMELIST colNames;
    colNames.push_back("id");
    for (FIELDS::const_iterator cit = colStubs.begin();
                                cit != colStubs.end();
                                ++cit) {
        COLSTUBNAMESMAP::iterator mapIter = ColstubToColnames.find(*cit);
        if (mapIter != ColstubToColnames.end()) {
            colNames.insert(colNames.end(), mapIter->second.begin(),
                                            mapIter->second.end());
        }
    }
   
    // Write the output table
    for (NAMELIST::const_iterator cit = colNames.begin();
                                  cit != colNames.end();
                                  ++cit) {
        const CSeqTable_column& column = mSrcTable->GetColumn(*cit);
        string displayName = column.GetHeader().GetTitle();
        out << displayName << CSrcWriter::mDelimiter;
    }
    out << '\n';


    unsigned int numRows = mSrcTable->GetNum_rows();
    for (unsigned int u=0; u < numRows; ++u) {     
        for (NAMELIST::const_iterator cit = colNames.begin();
                                      cit != colNames.end(); 
                                      ++cit) {

            const CSeqTable_column& column = mSrcTable->GetColumn(*cit);
            const string* pValue = column.GetStringPtr(u);
            bool needsQuotes = xValueNeedsQuoting(*pValue);
            if (needsQuotes) {
                out << "\"";
            }
            out << xDequotedValue(*pValue) << CSrcWriter::mDelimiter;
            if (needsQuotes) {
                out << "\"";
            }
        }
        out << '\n';
    }
    return true;  
}


//  ----------------------------------------------------------------------------
string CSrcWriter::xGetColStub(
    const string& colName)
//  ----------------------------------------------------------------------------
{
  // pcr-primers special case
  if  (NStr::Find(colName,"pcr-primers") != NPOS) {
      return "pcr-primers";
  }

  // case where column name takes the form colStub#Number
  size_t position;
  if ((position = NStr::Find(colName,"#")) != NPOS) {
      return colName.substr(0,position);
  }

  return colName;
}

//  ----------------------------------------------------------------------------
void CSrcWriter::xAppendColumnValue(
    const string& colName,
    const string& colValue)
//  ----------------------------------------------------------------------------
{
    unsigned int index = mColnameToIndex[colName];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(colValue);
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::ValidateFields(
    const FIELDS& fields,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    for (FIELDS::const_iterator cit = fields.begin(); cit != fields.end(); ++cit) {
      
        string field = *cit;
        if (field == "id" || field == "gi") {
            continue;
        }

        NAMEMAP::const_iterator mapIter = sFieldnameToColname.find(xCompressFieldName(field));
        if (mapIter != sFieldnameToColname.end()) {
            field = mapIter->second;
        }

        if (COrgMod::IsValidSubtypeName(field, COrgMod::eVocabulary_insdc)) {
            continue;
        }

        if (CSubSource::IsValidSubtypeName(field, CSubSource::eVocabulary_insdc)) {
            continue;
        } 

        // loop over the list of handlers
        HANDLERMAP::const_iterator fit = sHandlerMap.find(field);
        if (fit == sHandlerMap.end()) {
            CSrcError* pE = CSrcError::Create(
                ncbi::eDiag_Error,
                "Field name \"" + field + "\" not recognized.");
            pEC->PutError(*pE);
            delete pE;
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xValueNeedsQuoting(
    const string& value)
//  ----------------------------------------------------------------------------
{
    return (value.find(mDelimiter) != string::npos);
}

//  ----------------------------------------------------------------------------
string CSrcWriter::xDequotedValue(
    const string& value)
//  For lack of better idea, replace all occurences of "\"" with "\'\'"
//  -----------------------------------------------------------------------------
{
    return NStr::Replace(value, "\"", "\'\'");
}

//  -----------------------------------------------------------------------------
CSrcError::CSrcError(
    EDiagSev severity,
    const string& message):
//  -----------------------------------------------------------------------------
    CLineError(ILineError::eProblem_Unset, severity, "", 0,
        "", "", "", message, CLineError::TVecOfLines())
{
}

//  -----------------------------------------------------------------------------
CSrcError* CSrcError::Create(
    EDiagSev severity,
    const string& message)
//  -----------------------------------------------------------------------------
{
    return new CSrcError(severity, message);
}

END_NCBI_SCOPE
