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
 *   'seqfeat.asn'.
 */

// standard includes

// generated includes
#include <objects/seqfeat/Genetic_code.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CGenetic_code::~CGenetic_code(void)
{
}


const string& CGenetic_code::GetName(void) const
{
    if ( !m_Name ) {
        iterate( CGenetic_code::Tdata, gce, Get() ) {
            if ( (*gce)->IsName() ) {
                m_Name = &((*gce)->GetName());
                break;
            }
        }
    }
    return m_Name ? *m_Name : CNcbiEmptyString::Get();
}


// Retrieve the genetic-code's id.
int CGenetic_code::GetId(void) const
{
    if ( m_Id == 255 ) {
        iterate( CGenetic_code::Tdata, gce, Get() ) {
            if ( (*gce)->IsId() ) {
                m_Id = (*gce)->GetId();
                break;
            }
        }
    }
    return m_Id;
}


const string& CGenetic_code::GetNcbieaa(void) const
{
    if ( !m_Ncbieaa ) {
        iterate( CGenetic_code::Tdata, gce, Get() ) {
            if ( (*gce)->IsNcbieaa() ) {
                m_Ncbieaa =  &((*gce)->GetNcbieaa());
                break;
            }
        }
    }
    return m_Ncbieaa ? *m_Ncbieaa : CNcbiEmptyString::Get();
}


const string& CGenetic_code::GetSncbieaa(void) const
{
    if ( !m_Sncbieaa ) {
        iterate( CGenetic_code::Tdata, gce, Get() ) {
            if ( (*gce)->IsSncbieaa() ) {
                m_Sncbieaa = &((*gce)->GetSncbieaa());
            }
        }
    }
    return m_Sncbieaa ? *m_Sncbieaa : CNcbiEmptyString::Get();
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 6.1  2002/11/26 18:52:11  shomrat
* Add convenience methods for the retrieval of internal data
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 64, chars: 1892, CRC32: dc2193d9 */
