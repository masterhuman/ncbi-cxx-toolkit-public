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
 *   'twebenv.asn'.
 */

#ifndef QUERY_RELATED_HPP
#define QUERY_RELATED_HPP


// generated includes
#include "Query_Related_.hpp"

// generated classes

class CQuery_Related : public CQuery_Related_Base
{
    typedef CQuery_Related_Base Tparent;
public:
    // constructor
    CQuery_Related(void);
    // destructor
    ~CQuery_Related(void);

private:
    // Prohibit copy constructor and assignment operator
    CQuery_Related(const CQuery_Related& value);
    CQuery_Related& operator=(const CQuery_Related& value);

};



/////////////////// CQuery_Related inline methods

// constructor
inline
CQuery_Related::CQuery_Related(void)
{
}


/////////////////// end of CQuery_Related inline methods



/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/01/21 15:18:23  grichenk
* Re-arranged webenv files for serial test
*
*
* ===========================================================================
*/

#endif // QUERY_RELATED_HPP
/* Original file checksum: lines: 85, chars: 2280, CRC32: aafd1ae0 */
