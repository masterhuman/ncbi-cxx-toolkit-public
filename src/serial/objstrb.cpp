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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/05/19 19:56:56  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objstrb.hpp>
#include <serial/stdtypes.hpp>

BEGIN_NCBI_SCOPE

const CTypeInfo::TTypeInfo CObjectStreamBinaryDefs::sm_StdTypes[] =
{
    CStdTypeInfo<char>::sm_TypeInfo,
    CStdTypeInfo<unsigned char>::sm_TypeInfo,
    CStdTypeInfo<signed char>::sm_TypeInfo,
    CStdTypeInfo<short>::sm_TypeInfo,
    CStdTypeInfo<unsigned short>::sm_TypeInfo,
    CStdTypeInfo<int>::sm_TypeInfo,
    CStdTypeInfo<unsigned int>::sm_TypeInfo,
    CStdTypeInfo<long>::sm_TypeInfo,
    CStdTypeInfo<unsigned long>::sm_TypeInfo,
    CStdTypeInfo<float>::sm_TypeInfo,
    CStdTypeInfo<double>::sm_TypeInfo,
    CStdTypeInfo<string>::sm_TypeInfo
};

END_NCBI_SCOPE
