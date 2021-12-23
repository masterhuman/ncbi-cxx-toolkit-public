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
 *   'medline.asn'.
 */

#ifndef OBJECTS_MEDLINE_MEDLINE_ENTRY_HPP
#define OBJECTS_MEDLINE_MEDLINE_ENTRY_HPP


// generated includes
#include <objects/medline/Medline_entry_.hpp>

#include <objects/biblio/citation_base.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_MEDLINE_EXPORT CMedline_entry
    : public CMedline_entry_Base, public ICitationBase
{
    typedef CMedline_entry_Base Tparent;
public:
    // constructor
    CMedline_entry(void);
    // destructor
    ~CMedline_entry(void);
    
protected:    
    // Appends a label to "label" based on content
    bool GetLabelV1(string* label, TLabelFlags flags) const override;
    bool GetLabelV2(string* label, TLabelFlags flags) const override;

private:
    // Prohibit copy constructor and assignment operator
    CMedline_entry(const CMedline_entry& value);
    CMedline_entry& operator=(const CMedline_entry& value);

};



/////////////////// CMedline_entry inline methods

// constructor
inline
CMedline_entry::CMedline_entry(void)
{
}

/////////////////// end of CMedline_entry inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_MEDLINE_MEDLINE_ENTRY_HPP
/* Original file checksum: lines: 90, chars: 2473, CRC32: 9f1bc06c */
