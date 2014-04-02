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

/// @file ProjectItem.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'gbproj.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: ProjectItem_.hpp


#ifndef OBJECTS_GBPROJ_PROJECTITEM_HPP
#define OBJECTS_GBPROJ_PROJECTITEM_HPP


// generated includes
#include <corelib/ncbitime.hpp>
#include <objects/gbproj/ProjectItem_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CDate;
class CProjectFolder;

/////////////////////////////////////////////////////////////////////////////
class NCBI_GBPROJ_EXPORT CProjectItem : public CProjectItem_Base
{
    typedef CProjectItem_Base Tparent;
public:
    // constructor
    CProjectItem(void);
    // destructor
    ~CProjectItem(void);


    /// SetCreateDate() will add a descriptor for creation date, and insure
    /// that only one such descriptor exists
    void SetCreateDate(const CTime& time);
    void SetCreateDate(const CDate& date);

    /// SetModifiedDate() will add a descriptor for the update date, and insure
    /// that only one such descriptor exists
    void SetModifiedDate(const CTime& time);
    void SetModifiedDate(const CDate& date);

    /// wrapper for setting the object pointed to by this item
    void SetObject(CSerialObject& object);

    /// retrieve the object pointed to as a CObject*
    const CSerialObject* GetObject() const;

    /// enabled flag
    bool IsEnabled(void) const;

    void    SetParentFolder(CProjectFolder*) {}
    CProjectFolder* GetParentFolder() { return 0; }
    const CProjectFolder* GetParentFolder() const { return 0; }

    void     SetUserObject(CObject* object);
    CObject* GetUserObject();

private:
    // Prohibit copy constructor and assignment operator
    CProjectItem(const CProjectItem& value);
    CProjectItem& operator=(const CProjectItem& value);

protected:
	CRef<CObject>   m_UserObject;
};

/////////////////// CProjectItem inline methods

/////////////////// end of CProjectItem inline methods

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_GBPROJ_PROJECTITEM_HPP
/* Original file checksum: lines: 86, chars: 2448, CRC32: f0ac93d */
