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
* Revision 1.6  2000/05/05 16:26:56  vasilche
* Simplified iterator templates.
*
* Revision 1.5  2000/05/05 13:08:21  vasilche
* Simplified CTypesIterator interface.
*
* Revision 1.4  2000/05/04 16:23:12  vasilche
* Updated CTypesIterator and CTypesConstInterator interface.
*
* Revision 1.3  2000/04/10 21:01:48  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.2  2000/03/29 18:02:40  vasilche
* Workaroung of bug in MSVC: abstract member in template.
*
* Revision 1.1  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE

template<class BeginInfo>
CTreeIteratorBase<BeginInfo>::~CTreeIteratorBase(void)
{
    Reset();
}

template<class BeginInfo>
void CTreeIteratorBase<BeginInfo>::Reset(void)
{
    m_CurrentObject.Reset();
    m_VisitedObjects.reset(0);
    m_Valid = false;
    m_SkipSubTree = false;
    while ( m_Stack ) {
        m_Stack = m_Stack->Pop();
        --m_StackDepth;
    }
    _ASSERT(m_StackDepth == 0);
    _ASSERT(End());
}

template<class BeginInfo>
void CTreeIteratorBase<BeginInfo>::Begin(const TBeginInfo& beginInfo)
{
    Reset();
    if ( !beginInfo.GetObjectPtr() )
        return;
    m_CurrentObject.Set(beginInfo.GetObjectPtr(), beginInfo.GetTypeInfo());
    if ( beginInfo.DetectLoops() )
        m_VisitedObjects.reset(new TVisitedObjects);
    if ( CanSelectCurrentObject() ) {
        m_Valid = true;
    }
    else {
        m_Valid = true;
        Next();
    }
}

template<class BeginInfo>
void CTreeIteratorBase<BeginInfo>::Next(void)
{
    // cache object/type
    if ( End() )
        return;
    m_Valid = false;
    // traverse through tree
    for (;;) {
        if ( CanEnterCurrentObject() ) {
            // go to child
            _TRACE("Children of "<<m_CurrentObject.GetTypeInfo()->GetName());
            m_Stack = new CTreeLevel(m_CurrentObject, m_Stack);
            ++m_StackDepth;
            // go to fetching object
        }
        else {
            m_SkipSubTree = false;
            // skip all finished iterators
            while ( m_Stack ) {
                m_Stack->Next();
                if ( *m_Stack ) {
                    // next child on this level
                    break;
                }
                _TRACE("End of children of "<<m_Stack->GetParent().GetTypeInfo()->GetName());
                // end of children on this level
                m_Stack = m_Stack->Pop();
                --m_StackDepth;
            }

            if ( !m_Stack ) {
                // end
                _TRACE("End of tree");
                _ASSERT(m_StackDepth == 0);
                m_CurrentObject.Reset();
                return;
            }
        }
        m_Stack->GetChild(m_CurrentObject);
        _TRACE("Next child: "<<m_CurrentObject.GetTypeInfo()->GetName());
        while ( m_CurrentObject &&
                m_CurrentObject.GetTypeInfo()->IsPointer() ) {
            m_CurrentObject.GetTypeInfo()->GetPointedObject(m_CurrentObject);
            _TRACE("Ptr: "<<m_CurrentObject.GetTypeInfo()->GetName());
        }
        if ( m_CurrentObject && CanSelectCurrentObject() ) {
            m_Valid = true;
            _TRACE("Good!");
            return;
        }
    }
}

template<class BeginInfo>
bool CTreeIteratorBase<BeginInfo>::CanSelectCurrentObject(void)
{
    TVisitedObjects* visitedObjects = m_VisitedObjects.get();
    if ( visitedObjects ) {
        TConstObjectPtr object = m_CurrentObject.GetObjectPtr();
        if ( !visitedObjects->insert(object).second ) {
            // already visited
            return false;
        }
    }
	return true;
}

template<class BeginInfo>
bool CTreeIteratorBase<BeginInfo>::CanEnterCurrentObject(void)
{
    return !m_SkipSubTree && m_CurrentObject &&
        GetCurrentTypeInfo()->HaveChildren(m_CurrentObject.GetObjectPtr());
}

template
CTreeIteratorBase<CBeginInfo>::~CTreeIteratorBase(void);
template
void CTreeIteratorBase<CBeginInfo>::Reset(void);
template
void CTreeIteratorBase<CBeginInfo>::Begin(const TBeginInfo&);
template
void CTreeIteratorBase<CBeginInfo>::Next(void);
template
bool CTreeIteratorBase<CBeginInfo>::CanSelectCurrentObject(void);
template
bool CTreeIteratorBase<CBeginInfo>::CanEnterCurrentObject(void);

template
CTreeIteratorBase<CConstBeginInfo>::~CTreeIteratorBase(void);
template
void CTreeIteratorBase<CConstBeginInfo>::Reset(void);
template
void CTreeIteratorBase<CConstBeginInfo>::Begin(const TBeginInfo&);
template
void CTreeIteratorBase<CConstBeginInfo>::Next(void);
template
bool CTreeIteratorBase<CConstBeginInfo>::CanSelectCurrentObject(void);
template
bool CTreeIteratorBase<CConstBeginInfo>::CanEnterCurrentObject(void);

template<>
void CTreeIteratorBase<CBeginInfo>::PrepareErase(void)
{
    if ( !Valid() )
        THROW1_TRACE(runtime_error, "cannot erase nonvalid iterator");
    if ( !m_Stack )
        THROW1_TRACE(runtime_error, "cannot erase root object of iterator");
    m_CurrentObject.Reset();
}


template<class BeginInfo>
bool CTypeIteratorBase<BeginInfo>::CanSelectCurrentObject(void)
{
    return CParent::CanSelectCurrentObject() &&
        GetCurrentTypeInfo()->IsType(m_NeedType);
}

template<class BeginInfo>
bool CTypeIteratorBase<BeginInfo>::CanEnterCurrentObject(void)
{
    return CParent::CanEnterCurrentObject() &&
        GetCurrentTypeInfo()->MayContainType(m_NeedType);
}

template<class BeginInfo>
bool CTypesIteratorBase<BeginInfo>::CanSelectCurrentObject(void)
{
    if ( !CParent::CanSelectCurrentObject() )
        return false;
    m_MatchType = 0;
    TTypeInfo type = GetCurrentTypeInfo();
    iterate ( TTypeList, i, GetTypeList() ) {
        if ( type->IsType(*i) ) {
            m_MatchType = *i;
            return true;
        }
    }
    return false;
}

template<class BeginInfo>
bool CTypesIteratorBase<BeginInfo>::CanEnterCurrentObject(void)
{
    if ( !CParent::CanEnterCurrentObject() )
        return false;
    TTypeInfo type = GetCurrentTypeInfo();
    iterate ( TTypeList, i, GetTypeList() ) {
        if ( type->MayContainType(*i) )
            return true;
    }
    return false;
}

template
bool CTypeIteratorBase<CBeginInfo>::CanSelectCurrentObject(void);
template
bool CTypeIteratorBase<CBeginInfo>::CanEnterCurrentObject(void);
template
bool CTypeIteratorBase<CConstBeginInfo>::CanSelectCurrentObject(void);
template
bool CTypeIteratorBase<CConstBeginInfo>::CanEnterCurrentObject(void);

template
bool CTypesIteratorBase<CBeginInfo>::CanSelectCurrentObject(void);
template
bool CTypesIteratorBase<CBeginInfo>::CanEnterCurrentObject(void);
template
bool CTypesIteratorBase<CConstBeginInfo>::CanSelectCurrentObject(void);
template
bool CTypesIteratorBase<CConstBeginInfo>::CanEnterCurrentObject(void);

END_NCBI_SCOPE
