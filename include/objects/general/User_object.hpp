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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'general.asn'.
 */

#ifndef OBJECTS_GENERAL_USER_OBJECT_HPP
#define OBJECTS_GENERAL_USER_OBJECT_HPP


// generated includes
#include <objects/general/User_object_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_GENERAL_EXPORT CUser_object : public CUser_object_Base
{
    typedef CUser_object_Base Tparent;
public:
    /// constructor
    CUser_object(void);
    /// destructor
    ~CUser_object(void);

    /// how to interpret the value in the AddField() conversion functions below.
    enum EParseField {
        eParse_String,    ///< Add string even if all numbers
        eParse_Number     ///< Parse a real or integer number, otherwise string
    };

    /// add a data field to the user object that holds a given value
    CUser_object& AddField(const string& label, const string& value,
                           EParseField parse = eParse_String);
    CUser_object& AddField(const string& label, const char* value,
                           EParseField parse = eParse_String);
    CUser_object& AddField(const string& label, int           value);
    CUser_object& AddField(const string& label, Int8          value);
    CUser_object& AddField(const string& label, double        value);
    CUser_object& AddField(const string& label, bool          value);
#ifdef NCBI_STRICT_GI
    CUser_object& AddField(const string& label, TGi           value);
#endif

    CUser_object& AddField(const string& label, const vector<string>& value);
    CUser_object& AddField(const string& label, const vector<int>&    value);
    CUser_object& AddField(const string& label, const vector<double>& value);

    CUser_object& AddField(const string& label, CUser_object& value);
    CUser_object& AddField(const string& label,
                           const vector< CRef<CUser_object> >& value);
    CUser_object& AddField(const string& label,
                           const vector< CRef<CUser_field> >& value);

    /// Access a named field in this user object.  This is a little
    /// sneaky in that it interprets a delimiter for recursion.
    /// This version will throw an exception if the field
    /// doesn't exist.
    const CUser_field&     GetField(const string& str,
                                    const string& delim = ".",
                                    NStr::ECase use_case = NStr::eCase) const;
    CConstRef<CUser_field> GetFieldRef(const string& str,
                                       const string& delim = ".",
                                       NStr::ECase use_case = NStr::eCase) const;

    /// Access a named field in this user object.  This is a little
    /// sneaky in that it interprets a delimiter for recursion.  The
    /// 'obj_subtype' parameter is used to set the subtype of a 
    /// sub-object if a new sub-object needs to be created
    CUser_field&      SetField(const string& str,
                               const string& delim = ".",
                               const string& obj_subtype = kEmptyStr,
                               NStr::ECase use_case = NStr::eCase);
    CRef<CUser_field> SetFieldRef(const string& str,
                                  const string& delim = ".",
                                  const string& obj_subtype = kEmptyStr,
                                  NStr::ECase use_case = NStr::eCase);

    /// Verify that a named field exists
    bool HasField(const string& str,
                  const string& delim = ".",
                  NStr::ECase use_case = NStr::eCase) const;

    /// enum controlling what to return for a label
    /// this mirrors a request inside of feature::GetLabel()
    enum ELabelContent {
        eType,
        eContent,
        eBoth
    };

    /// Append a label to label.  The type defaults to content for
    /// backward compatibility
    void GetLabel(string* label, ELabelContent mode = eContent) const;

    ///
    /// enums for implicit typing of user objects
    ///

    /// general category
    enum ECategory {
        eCategory_Unknown = -1,
        eCategory_Experiment
    };

    /// sub-category experiment
    enum EExperiment {
        eExperiment_Unknown = -1,
        eExperiment_Sage
    };

    /// accessors: classify a given user object
    ECategory GetCategory(void) const;

    /// sub-category accessors:
    EExperiment GetExperimentType(void) const;
    const CUser_object& GetExperiment(void) const;

    /// format a user object as a given type.  This returns a user-object
    /// that is suitable for containing whatever specifics might be needed
    CUser_object& SetCategory(ECategory category);

    /// format a user object as a given type.  This returns a user-object
    /// that is suitable for containing whatever specifics might be needed
    CUser_object& SetExperiment(EExperiment category);

    /// Object Type
    enum EObjectType {
        eObjectType_Unknown = -1,
        eObjectType_DBLink,
        eObjectType_StructuredComment,
        eObjectType_OriginalId,
        eObjectType_Unverified,
        eObjectType_ValidationSuppression,
        eObjectType_Cleanup,
        eObjectType_AutodefOptions,
        eObjectType_FileTrack,
        eObjectType_RefGeneTracking
    };

    EObjectType GetObjectType() const;
    void SetObjectType(EObjectType obj_type);
    bool IsDBLink() const { return GetObjectType() == eObjectType_DBLink; }
    bool IsStructuredComment() const { return GetObjectType() == eObjectType_StructuredComment; }
    bool IsOriginalId() const { return GetObjectType() == eObjectType_OriginalId; }
    bool IsUnverified() const { return GetObjectType() == eObjectType_Unverified; }
    bool IsValidationSuppression() const { return GetObjectType() == eObjectType_ValidationSuppression; }
    bool IsCleanup() const { return GetObjectType() == eObjectType_Cleanup; }
    bool IsAutodefOptions() const { return GetObjectType() == eObjectType_AutodefOptions; }
    bool IsFileTrack() const { return GetObjectType() == eObjectType_FileTrack; }
    bool IsRefGeneTracking() const { return GetObjectType() == eObjectType_RefGeneTracking; }

    // for Unverified User-objects: Can have Organism and/or Feature and/or Misassembled
    bool IsUnverifiedOrganism() const;
    void AddUnverifiedOrganism();
    void RemoveUnverifiedOrganism();
    bool IsUnverifiedFeature() const;
    void AddUnverifiedFeature();
    void RemoveUnverifiedFeature();
    bool IsUnverifiedMisassembled() const;
    void AddUnverifiedMisassembled();
    void RemoveUnverifiedMisassembled();
    bool IsUnverifiedContaminant() const;
    void AddUnverifiedContaminant();
    void RemoveUnverifiedContaminant();

    void UpdateNcbiCleanup(int version);

    // returns true if one or more matching fields were removed
    bool RemoveNamedField(const string& field_name, NStr::ECase ecase = NStr::eCase);

    // Set FileTrack URL
    void SetFileTrackURL(const string& url);
    void SetFileTrackUploadId(const string& upload_id);

    // For RefGeneTracking
    // See https://confluence.ncbi.nlm.nih.gov/display/REF/RefGeneTracking+user+object?focusedCommentId=94310717#comment-94310717 for spec
    // JIRA:MSS-677
    enum ERefGeneTrackingStatus {
        eRefGeneTrackingStatus_Error = -1,
        eRefGeneTrackingStatus_NotSet,
        eRefGeneTrackingStatus_PREDICTED,
        eRefGeneTrackingStatus_PROVISIONAL,
        eRefGeneTrackingStatus_INFERRED, 
        eRefGeneTrackingStatus_VALIDATED, 
        eRefGeneTrackingStatus_REVIEWED, 
        eRefGeneTrackingStatus_PIPELINE,
        eRefGeneTrackingStatus_WGS
    };
    void SetRefGeneTrackingStatus(ERefGeneTrackingStatus status);
    ERefGeneTrackingStatus GetRefGeneTrackingStatus() const;
    void ResetRefGeneTrackingStatus();
    bool IsSetRefGeneTrackingStatus() const 
    {
        ERefGeneTrackingStatus val = GetRefGeneTrackingStatus();
        return (val != eRefGeneTrackingStatus_NotSet && val != eRefGeneTrackingStatus_Error);
    }

    void SetRefGeneTrackingGenomicSource(const string& genomic_source);
    const string& GetRefGeneTrackingGenomicSource() const;
    void ResetRefGeneTrackingGenomicSource();
    bool IsSetRefGeneTrackingGenomicSource() const { return !GetRefGeneTrackingGenomicSource().empty(); }

    void SetRefGeneTrackingGenerated(bool val = true);
    bool GetRefGeneTrackingGenerated() const;
    void ResetRefGeneTrackingGenerated();

    // Collaborator field in RefGeneTracking User-object
    void SetRefGeneTrackingCollaborator(const string& collaborator);
    const string& GetRefGeneTrackingCollaborator() const;
    void ResetRefGeneTrackingCollaborator();
    bool IsSetRefGeneTrackingCollaborator() const { return !GetRefGeneTrackingCollaborator().empty(); }


    // CollaboratorURL field in RefGeneTracking User-object
    void SetRefGeneTrackingCollaboratorURL(const string& collaborator_url);
    const string& GetRefGeneTrackingCollaboratorURL() const;
    void ResetRefGeneTrackingCollaboratorURL();
    bool IsSetRefGeneTrackingCollaboratorURL() const { return !GetRefGeneTrackingCollaboratorURL().empty(); }

    class CRefGeneTrackingException : public CException
    {
    public:
        enum EErrCode {            
            eUserFieldWithoutLabel,
            eBadUserFieldName,
            eBadUserFieldData,
            eBadStatus
        };

        virtual const char* GetErrCodeString() const override
        {
            switch (GetErrCode()) {
            case eUserFieldWithoutLabel: return "User field without label";
            case eBadUserFieldData: return "Unexpected data type";
            default:     return CException::GetErrCodeString();
            }
        }

        NCBI_EXCEPTION_DEFAULT(CRefGeneTrackingException, CException);
    };

    class CRefGeneTrackingAccession : public CObject {
    public:
        CRefGeneTrackingAccession() {}
        CRefGeneTrackingAccession(const string& accession,
                                  TGi gi = ZERO_GI,
                                  TSeqPos from = kInvalidSeqPos,
                                  TSeqPos to = kInvalidSeqPos,
                                  const string& comment = kEmptyStr,
                                  const string& acc_name = kEmptyStr) :
            m_Accession(accession),
            m_GI(gi),
            m_From(from),
            m_To(to),
            m_Comment(comment),
            m_Name(acc_name) {}

        ~CRefGeneTrackingAccession() {}

        const string& GetAccession() const { return m_Accession; }
        bool IsSetAccession() const { return !m_Accession.empty(); }
        const string& GetComment() const { return m_Comment; }
        bool IsSetComment() const { return !m_Comment.empty(); }
        const string& GetName() const { return m_Name; }
        bool IsSetName() const { return !m_Name.empty(); }
        TGi GetGI() const { return m_GI; }
        bool IsSetGI() const { return m_GI > ZERO_GI; }
        TSeqPos GetFrom() const { return m_From; }
        bool IsSetFrom() const { return m_From != kInvalidSeqPos; }
        TSeqPos GetTo() const { return m_To; }
        bool IsSetTo() const { return m_To != kInvalidSeqPos; }

        bool IsEmpty() const { return !(IsSetAccession() || IsSetComment() ||
                                       IsSetName() || IsSetGI() ||
                                       IsSetFrom() || IsSetTo()); }

        CRef<CUser_field> MakeAccessionField() const;
        static CRef<CRefGeneTrackingAccession> MakeAccessionFromUserField(const CUser_field& field);

    private:
        string m_Accession;
        TGi m_GI;
        TSeqPos m_From;
        TSeqPos m_To;
        string m_Comment;
        string m_Name;

        /// Prohibit copy constructor and assignment operator
        CRefGeneTrackingAccession(const CRefGeneTrackingAccession& cpy) = delete;
        CRefGeneTrackingAccession& operator=(const CRefGeneTrackingAccession& value) = delete;
    };

    typedef vector<CConstRef<CRefGeneTrackingAccession> > TRefGeneTrackingAccessions;

    // "IdenticalTo" field in RefGeneTracking User-object: can have only one accession
    void SetRefGeneTrackingIdenticalTo(const CRefGeneTrackingAccession& accession);
    CConstRef<CRefGeneTrackingAccession> GetRefGeneTrackingIdenticalTo() const;
    void ResetRefGeneTrackingIdenticalTo();
    bool IsSetRefGeneTrackingIdenticalTo() const { return GetRefGeneTrackingIdenticalTo() != CConstRef<CRefGeneTrackingAccession>(NULL); }

    // "Assembly" field in RefGeneTracking User-object: can have one or more accessions
    void SetRefGeneTrackingAssembly(const TRefGeneTrackingAccessions& acc_list);
    TRefGeneTrackingAccessions GetRefGeneTrackingAssembly() const;
    void ResetRefGeneTrackingAssembly();
    bool IsSetRefGeneTrackingAssembly() const { return GetRefGeneTrackingAssembly().size() > 0; }

private:
    /// Prohibit copy constructor and assignment operator
    CUser_object(const CUser_object& value);
    CUser_object& operator=(const CUser_object& value);

    bool x_IsUnverifiedType(const string& val) const;
    bool x_IsUnverifiedType(const string& val, const CUser_field& field) const;
    void x_AddUnverifiedType(const string& val);
    void x_RemoveUnverifiedType(const string& val);

    // for RefGeneTracking
    void x_SetRefGeneTrackingField(const string& field_name, const string& value);
    const string& x_GetRefGeneTrackingField(const string& field_name) const;
};



/////////////////// CUser_object inline methods

// constructor
inline
CUser_object::CUser_object(void)
{
}


/////////////////// end of CUser_object inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_GENERAL_USER_OBJECT_HPP
