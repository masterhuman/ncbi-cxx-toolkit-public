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
* Author: Eugene Vasilchenko
*
* File Description:
*   Standard serialization exceptions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/07/03 20:47:22  vasilche
* Removed unused variables/functions.
*
* Revision 1.2  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.1  2000/02/17 20:02:43  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
*
*/

#include <serial/exception.hpp>

BEGIN_NCBI_SCOPE

CSerialException::CSerialException(const string& msg) THROWS_NONE
    : runtime_error(msg)
{
}

CSerialException::~CSerialException(void) THROWS_NONE
{
}

CSerialIOException::CSerialIOException(const string& msg) THROWS_NONE
    : CSerialException(msg)
{
}

CSerialIOException::~CSerialIOException(void) THROWS_NONE
{
}

CSerialEofException::CSerialEofException(void) THROWS_NONE
    : CSerialIOException("end of file")
{
}

CSerialEofException::~CSerialEofException(void) THROWS_NONE
{
}

CInvalidChoiceSelection::CInvalidChoiceSelection(const string& current,
                                                 const string& mustBe) THROWS_NONE
    : runtime_error("Invalid choice selection: "+current+". "
                    "Expected: "+mustBe)
{
}

CInvalidChoiceSelection::CInvalidChoiceSelection(unsigned currentIndex,
                                                 unsigned mustBeIndex,
                                                 const char* const names[],
                                                 unsigned namesCount) THROWS_NONE
    : runtime_error(string("Invalid choice selection: ")+
                    GetName(currentIndex, names, namesCount)+". "
                    "Expected: "+
                    GetName(mustBeIndex, names, namesCount))
{
}

const char* CInvalidChoiceSelection::GetName(unsigned index,
                                             const char* const names[],
                                             unsigned namesCount)
{
    if ( index > namesCount )
        return "?unknown?";
    return names[index];
    
}

CInvalidChoiceSelection::~CInvalidChoiceSelection(void) THROWS_NONE
{
}

END_NCBI_SCOPE
