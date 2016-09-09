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
 */

/// @file ValidError.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'valerr.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: ValidError_.hpp


#ifndef OBJECTS_VALERR_VALIDERROR_HPP
#define OBJECTS_VALERR_VALIDERROR_HPP


// generated includes
#include <objects/valerr/ValidError_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CSeqdesc;
class CSeq_entry;

/////////////////////////////////////////////////////////////////////////////
class NCBI_VALERR_EXPORT CValidError : public CValidError_Base
{
    typedef CValidError_Base Tparent;
public:
    // constructor
    CValidError(const CSerialObject* obj = NULL);
    // destructor
    ~CValidError(void);

    void AddValidErrItem(EDiagSev             sev,     // severity
                         unsigned int         ec,      // error code
                         const string&        msg,     // specific error message
                         const string&        desc,    // offending object's description
                         const CSerialObject& obj,     // offending object
                         const string&        acc,     // accession of object.
                         const int            ver,     // version of object.
                         const int            seq_offset = 0);

    void AddValidErrItem(EDiagSev             sev,     // severity
                         unsigned int         ec,      // error code
                         const string&        msg,     // specific error message
                         const string&        desc,    // offending object's description
                         const CSerialObject& obj,     // offending object
                         const string&        acc,     // accession of object.
                         const int            ver,     // version of object.
                         const string&        feature_id,  // feature ID for object
                         const int            seq_offset = 0);
 
     void AddValidErrItem(EDiagSev            sev,     // severity
                         unsigned int         ec,      // error code
                         const string&        msg,     // specific error message
                         const string&        desc,    // offending object's description
                         const CSeqdesc&      seqdesc, // offending object
                         const CSeq_entry&    ctx,     // place of packaging
                         const string&        acc,     // accession of object or context.
                         const int            ver,     // version of object.
                         const int            seq_offset = 0);

     void AddValidErrItem(EDiagSev            sev,     // severity
                          unsigned int         ec,      // error code
                          const string&        msg);     // specific error message

    // Statistics
    SIZE_TYPE TotalSize(void)    const;
    SIZE_TYPE Size(EDiagSev sev) const;

    SIZE_TYPE InfoSize    (void) const;
    SIZE_TYPE WarningSize (void) const;
    SIZE_TYPE ErrorSize   (void) const;
    SIZE_TYPE CriticalSize(void) const;
    SIZE_TYPE FatalSize   (void) const;

    // Get the validated object (Seq-entry, Seq-submit or Seq-align)
    const CSerialObject* GetValidated(void) const;

    // for suppressing errors by type. Suppression list
    // will be checked at the time that a AddValidErrItem
    // method is called
    void SuppressError(unsigned int ec);
    bool ShouldSuppress(unsigned int ec);
    void ClearSuppressions();

protected:
    friend class CValidError_CI;

    typedef map<EDiagSev, SIZE_TYPE>               TSevStats;
    typedef CConstRef<CSerialObject>               TValidated;
    typedef CConstRef<CSeq_entry>                  TContext;

    // data
    TSevStats   m_Stats;     // severity statistics
    TValidated  m_Validated; // the validated object
    vector<unsigned int> m_SuppressionList;

private:
    // Prohibit copy constructor & assignment operator
    CValidError(const CValidError&);
    CValidError& operator= (const CValidError&);

};


class NCBI_VALERR_EXPORT CValidError_CI
{
public:
    CValidError_CI(void);
    CValidError_CI(const CValidError& ve,
                   const string& errcode = kEmptyStr,
                   EDiagSev minsev  = eDiagSevMin,
                   EDiagSev maxsev  = eDiagSevMax);
    CValidError_CI(const CValidError_CI& iter);
    virtual ~CValidError_CI(void);

    CValidError_CI& operator=(const CValidError_CI& iter);
    CValidError_CI& operator++(void);

    bool IsValid(void) const;
    DECLARE_OPERATOR_BOOL(IsValid());

    const CValidErrItem& operator* (void) const;
    const CValidErrItem* operator->(void) const;

private:
    bool Filter(const CValidErrItem& item) const;
    bool AtEnd(void) const;
    void Next(void);

    CConstRef<CValidError>               m_Validator;
    CValidError::TErrs::const_iterator   m_Current;

    // filters:
    string                               m_ErrCodeFilter;
    EDiagSev                             m_MinSeverity;
    EDiagSev                             m_MaxSeverity;
};


/////////////////// CValidError inline methods

// constructor

inline
SIZE_TYPE CValidError::TotalSize(void) const 
{
    return GetErrs().size();
}


inline
SIZE_TYPE CValidError::Size(EDiagSev sev) const 
{
    return const_cast<CValidError*>(this)->m_Stats[sev]; 
}


inline
SIZE_TYPE CValidError::InfoSize(void) const
{
    return Size(eDiag_Info);
}


inline
SIZE_TYPE CValidError::WarningSize(void) const
{
    return Size(eDiag_Warning);
}


inline
SIZE_TYPE CValidError::ErrorSize(void) const
{
    return Size(eDiag_Error);
}


inline
SIZE_TYPE CValidError::CriticalSize(void) const
{
    return Size(eDiag_Critical);
}


inline
SIZE_TYPE CValidError::FatalSize(void) const
{
    return Size(eDiag_Fatal);
}

inline
const CSerialObject* CValidError::GetValidated(void) const
{
    return m_Validated.GetPointerOrNull();
}


/////////////////// end of CValidError inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_VALERR_VALIDERROR_HPP
/* Original file checksum: lines: 94, chars: 2596, CRC32: 9c930424 */
