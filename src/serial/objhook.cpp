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
* Revision 1.6  2002/09/19 14:00:38  grichenk
* Implemented CObjectHookGuard for write and copy hooks
* Added DefaultRead/Write/Copy methods to base hook classes
*
* Revision 1.5  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.4  2000/10/20 15:51:40  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.3  2000/09/26 17:38:21  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.2  2000/09/18 20:00:23  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/08/15 19:44:48  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objhook.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/member.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

CReadObjectHook::~CReadObjectHook(void)
{
}

CReadClassMemberHook::~CReadClassMemberHook(void)
{
}

void CReadClassMemberHook::ReadMissingClassMember(CObjectIStream& in,
                                                  const CObjectInfoMI& member)
{
    member.GetMemberInfo()->
        DefaultReadMissingMember(in, member.GetClassObject().GetObjectPtr());
}

CReadChoiceVariantHook::~CReadChoiceVariantHook(void)
{
}

CReadContainerElementHook::~CReadContainerElementHook(void)
{
}

CWriteObjectHook::~CWriteObjectHook(void)
{
}

CWriteClassMemberHook::~CWriteClassMemberHook(void)
{
}

CWriteChoiceVariantHook::~CWriteChoiceVariantHook(void)
{
}

CCopyObjectHook::~CCopyObjectHook(void)
{
}

CCopyClassMemberHook::~CCopyClassMemberHook(void)
{
}

void CCopyClassMemberHook::CopyMissingClassMember(CObjectStreamCopier& copier,
                                                  const CObjectTypeInfoMI& member)
{
    member.GetMemberInfo()->DefaultCopyMissingMember(copier);
}

CCopyChoiceVariantHook::~CCopyChoiceVariantHook(void)
{
}


void CReadObjectHook::DefaultRead(CObjectIStream& in,
                                  const CObjectInfo& object)
{
    object.GetTypeInfo()->DefaultReadData(in, object.GetObjectPtr());
}

void CReadObjectHook::DefaultSkip(CObjectIStream& in,
                                  const CObjectInfo& object)
{
    object.GetTypeInfo()->DefaultSkipData(in);
}

void CReadClassMemberHook::DefaultRead(CObjectIStream& in,
                                       const CObjectInfoMI& object)
{
    in.ReadClassMember(object);
}

void CReadClassMemberHook::DefaultSkip(CObjectIStream& in,
                                       const CObjectInfoMI& object)
{
    in.SkipObject(object.GetMember());
}

void CReadChoiceVariantHook::DefaultRead(CObjectIStream& in,
                                         const CObjectInfoCV& object)
{
    in.ReadChoiceVariant(object);
}

void CWriteObjectHook::DefaultWrite(CObjectOStream& out,
                                    const CConstObjectInfo& object)
{
    object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
}

void CWriteClassMemberHook::DefaultWrite(CObjectOStream& out,
                                         const CConstObjectInfoMI& member)
{
    out.WriteClassMember(member);
}

void CWriteChoiceVariantHook::DefaultWrite(CObjectOStream& out,
                                           const CConstObjectInfoCV& variant)
{
    out.WriteChoiceVariant(variant);
}

void CCopyObjectHook::DefaultCopy(CObjectStreamCopier& copier,
                                  const CObjectTypeInfo& type)
{
    type.GetTypeInfo()->DefaultCopyData(copier);
}

void CCopyClassMemberHook::DefaultCopy(CObjectStreamCopier& copier,
                                       const CObjectTypeInfoMI& member)
{
    member.GetMemberInfo()->DefaultCopyMember(copier);
}

void CCopyChoiceVariantHook::DefaultCopy(CObjectStreamCopier& copier,
                                         const CObjectTypeInfoCV& variant)
{
    variant.GetVariantInfo()->DefaultCopyVariant(copier);
}


END_NCBI_SCOPE
