#ifndef TYPEMAPIMPL__HPP
#define TYPEMAPIMPL__HPP

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
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <map>
#include <memory>


/** @addtogroup TypeLookup
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CTypeInfoMapData
{
public:
    TTypeInfo GetTypeInfo(TTypeInfo key, TTypeInfoGetter1 func);
    TTypeInfo GetTypeInfo(TTypeInfo key, TTypeInfoCreator1 func);

private:
    map<TTypeInfo, TTypeInfo> m_Map;
};

class NCBI_XSERIAL_EXPORT CTypeInfoMap2Data
{
public:
    TTypeInfo GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                          TTypeInfoGetter2 func);
    TTypeInfo GetTypeInfo(TTypeInfo arg1, TTypeInfo arg2,
                          TTypeInfoCreator2 func);

private:
    map<TTypeInfo, map<TTypeInfo, TTypeInfo> > m_Map;
};


END_NCBI_SCOPE

#endif  /* TYPEMAPIMPL__HPP */


/* @} */


/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/04/15 16:19:11  siyan
* Added doxygen support
*
* Revision 1.3  2002/12/23 18:38:52  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.2  2000/11/07 17:25:14  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.1  2000/10/13 16:28:34  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* ===========================================================================
*/
