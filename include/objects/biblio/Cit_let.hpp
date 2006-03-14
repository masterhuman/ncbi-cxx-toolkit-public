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
 *   using specifications from the data definition file
 *   'biblio.asn'.
 */

#ifndef OBJECTS_BIBLIO_CIT_LET_HPP
#define OBJECTS_BIBLIO_CIT_LET_HPP


// generated includes
#include <objects/biblio/Cit_let_.hpp>
#include <objects/biblio/Cit_book.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_BIBLIO_EXPORT CCit_let : public CCit_let_Base
{
    typedef CCit_let_Base Tparent;
public:
    // constructor
    CCit_let(void);
    // destructor
    ~CCit_let(void);

    // Appends a label to "label" based on content
    void GetLabel(string* label) const;

private:
    // Prohibit copy constructor and assignment operator
    CCit_let(const CCit_let& value);
    CCit_let& operator=(const CCit_let& value);

};



/////////////////// CCit_let inline methods

// constructor
inline
CCit_let::CCit_let(void)
{
}

inline
void CCit_let::GetLabel(string* label) const
{
    GetCit().GetLabel(label);
}

/////////////////// end of CCit_let inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2006/03/14 20:21:50  rsmith
* Move BasicCleanup functionality from objects to objtools/cleanup
*
* Revision 1.3  2005/05/20 13:32:18  shomrat
* Added BasicCleanup()
*
* Revision 1.2  2004/08/19 13:03:23  dicuccio
* Added missing include for Cit_book.hpp
*
* Revision 1.1  2004/02/24 15:52:22  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif // OBJECTS_BIBLIO_CIT_LET_HPP
/* Original file checksum: lines: 93, chars: 2380, CRC32: c09c1f3d */
