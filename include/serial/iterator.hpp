#ifndef ITERATOR__HPP
#define ITERATOR__HPP

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
* Revision 1.7  2000/05/05 16:26:51  vasilche
* Simplified iterator templates.
*
* Revision 1.6  2000/05/05 13:08:16  vasilche
* Simplified CTypesIterator interface.
*
* Revision 1.5  2000/05/04 16:23:09  vasilche
* Updated CTypesIterator and CTypesConstInterator interface.
*
* Revision 1.4  2000/04/10 21:01:38  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.3  2000/03/29 18:02:39  vasilche
* Workaroung of bug in MSVC: abstract member in template.
*
* Revision 1.2  2000/03/29 17:22:34  vasilche
* Fixed ambiguity in Begin() template function.
*
* Revision 1.1  2000/03/29 15:55:19  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/iteratorbase.hpp>
#include <serial/typeinfo.hpp>
#include <serial/stdtypes.hpp>
#include <set>

BEGIN_NCBI_SCOPE

class CBeginInfo
{
public:
    typedef CChildrenIterator TChildrenIterator;
    typedef CObjectInfo TObjectInfo;

    CBeginInfo(TObjectPtr objectPtr, TTypeInfo typeInfo,
               bool detectLoops = false)
        : m_ObjectPtr(objectPtr), m_TypeInfo(typeInfo),
          m_DetectLoops(detectLoops)
        {
        }

    TObjectPtr GetObjectPtr(void) const
        {
            return m_ObjectPtr;
        }
    TTypeInfo GetTypeInfo(void) const
        {
            return m_TypeInfo;
        }
    bool DetectLoops(void) const
        {
            return m_DetectLoops;
        }

private:
    TObjectPtr m_ObjectPtr;
    TTypeInfo m_TypeInfo;
    bool m_DetectLoops;
};

class CConstBeginInfo
{
public:
    typedef CConstChildrenIterator TChildrenIterator;
    typedef CConstObjectInfo TObjectInfo;

    CConstBeginInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo,
               bool detectLoops = false)
        : m_ObjectPtr(objectPtr), m_TypeInfo(typeInfo),
          m_DetectLoops(detectLoops)
        {
        }
    CConstBeginInfo(const CBeginInfo& beginInfo)
        : m_ObjectPtr(beginInfo.GetObjectPtr()),
          m_TypeInfo(beginInfo.GetTypeInfo()),
          m_DetectLoops(beginInfo.DetectLoops())
        {
        }

    TConstObjectPtr GetObjectPtr(void) const
        {
            return m_ObjectPtr;
        }
    TTypeInfo GetTypeInfo(void) const
        {
            return m_TypeInfo;
        }
    bool DetectLoops(void) const
        {
            return m_DetectLoops;
        }

private:
    TConstObjectPtr m_ObjectPtr;
    TTypeInfo m_TypeInfo;
    bool m_DetectLoops;
};

template<class BeginInfo>
class CTreeIteratorBase
{
    typedef CTreeIteratorBase<BeginInfo> TThis;
public:
    typedef BeginInfo TBeginInfo;
    typedef typename TBeginInfo::TChildrenIterator TChildrenIterator;
    typedef typename TChildrenIterator::TObjectInfo TObjectInfo;
    typedef set<TConstObjectPtr> TVisitedObjects;

    CTreeIteratorBase(void)
        : m_Valid(false), m_SkipSubTree(false), m_Stack(0), m_StackDepth(0)
        {
        }

    virtual ~CTreeIteratorBase(void);


    TObjectInfo& Get(void)
        {
            _ASSERT(Valid());
            return m_CurrentObject;
        }
    const TObjectInfo& Get(void) const
        {
            _ASSERT(Valid());
            return m_CurrentObject;
        }
    TTypeInfo GetCurrentTypeInfo(void) const
        {
            return Get().GetTypeInfo();
        }

    // reset iterator to initial state (!Valid() && End())
    void Reset(void);

    // points to a valid object
    bool Valid(void) const
        {
            return m_Valid;
        }
    // end of tree (also Valid() == false)
    bool End(void) const
        {
            return !Valid() && m_Stack == 0;
        }
    // post condition: Valid() || End()
    void Next(void);

    // precondition: Valid()
    // postcondition: Valid(); will skip on the next Next()
    void SkipSubTree(void)
        {
            m_SkipSubTree = true;
        }

    operator bool(void)
        {
            return !End();
        }
    CTreeIteratorBase<BeginInfo>& operator++(void)
        {
            Next();
            return *this;
        }

    class CTreeLevel : public TChildrenIterator {
    public:
        CTreeLevel(const TObjectInfo& owner, CTreeLevel* previousLevel)
            : TChildrenIterator(owner), m_PreviousLevel(previousLevel)
            {
            }

        CTreeLevel* Pop(void)
            {
                CTreeLevel* previousLevel = m_PreviousLevel;
                delete this;
                return previousLevel;
            }

    private:
        CTreeLevel* m_PreviousLevel;
    };

    void Begin(const TBeginInfo& beginInfo);
    void Erase(void)
        {
            PrepareErase();
            m_Stack->Erase();
            m_Valid = false;
            m_SkipSubTree = true;
        }

protected:
    // post condition: Valid() || End()
    virtual bool CanSelectCurrentObject(void);
    virtual bool CanEnterCurrentObject(void);

protected:
    // have to make these methods protected instead of private due to
    // bug in GCC
    CTreeIteratorBase(const CTreeIteratorBase<BeginInfo>&);
    CTreeIteratorBase<BeginInfo>& operator=(const CTreeIteratorBase<BeginInfo>&);
private:
    void PrepareErase(void);

    bool m_Valid;
    bool m_SkipSubTree;
    // stack of tree level iterators
    CTreeLevel* m_Stack;
    size_t m_StackDepth;
    // currently selected object
    TObjectInfo m_CurrentObject;
    auto_ptr<TVisitedObjects> m_VisitedObjects;
};

template<class BeginInfo>
class CTypeIteratorBase : public CTreeIteratorBase<BeginInfo>
{
    typedef CTreeIteratorBase<BeginInfo> CParent;
protected:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CTypeIteratorBase(TTypeInfo needType)
        : m_NeedType(needType)
        {
        }
    CTypeIteratorBase(TTypeInfo needType, const TBeginInfo& beginInfo)
        : m_NeedType(needType)
        {
            Begin(beginInfo);
        }

    virtual bool CanSelectCurrentObject(void);
    virtual bool CanEnterCurrentObject(void);

private:
    TTypeInfo m_NeedType;
};

template<class C, class TypeGetter = C>
class CTypeIterator : public CTypeIteratorBase<CBeginInfo>
{
    typedef CTypeIteratorBase<CBeginInfo> CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CTypeIterator(void)
        : CParent(TypeGetter::GetTypeInfo())
        {
        }
    CTypeIterator(const TBeginInfo& beginInfo)
        : CParent(TypeGetter::GetTypeInfo(), beginInfo)
        {
        }

    CTypeIterator<C, TypeGetter>& operator=(const TBeginInfo& beginInfo)
        {
            Begin(beginInfo);
            return *this;
        }

    C& operator*(void)
        {
            return *static_cast<C*>(Get().GetObjectPtr());
        }
    const C& operator*(void) const
        {
            return *static_cast<const C*>(Get().GetObjectPtr());
        }
};

template<class C, class TypeGetter = C>
class CTypeConstIterator : public CTypeIteratorBase<CConstBeginInfo>
{
    typedef CTypeIteratorBase<CConstBeginInfo> CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CTypeConstIterator(void)
        : CParent(TypeGetter::GetTypeInfo())
        {
        }
    CTypeConstIterator(const TBeginInfo& beginInfo)
        : CParent(TypeGetter::GetTypeInfo(), beginInfo)
        {
        }

    CTypeConstIterator<C, TypeGetter>& operator=(const TBeginInfo& beginInfo)
        {
            Begin(beginInfo);
            return *this;
        }

    const C& operator*(void) const
        {
            return *static_cast<const C*>(Get().GetObjectPtr());
        }
};

template<typename T>
class CStdTypeIterator : public CTypeIterator<T, CStdTypeInfo<T> >
{
    typedef CTypeIterator<T, CStdTypeInfo<T> > CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CStdTypeIterator(void)
        {
        }
    CStdTypeIterator(const TBeginInfo& beginInfo)
        : CParent(beginInfo)
        {
        }

    CStdTypeIterator<T>& operator=(const TBeginInfo& beginInfo)
        {
            Begin(beginInfo);
            return *this;
        }
};

template<typename T>
class CStdTypeConstIterator : public CTypeConstIterator<T, CStdTypeInfo<T> >
{
    typedef CTypeConstIterator<T, CStdTypeInfo<T> > CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CStdTypeConstIterator(void)
        {
        }
    CStdTypeConstIterator(const TBeginInfo& beginInfo)
        : CParent(beginInfo)
        {
        }

    CStdTypeConstIterator<T>& operator=(const TBeginInfo& beginInfo)
        {
            Begin(beginInfo);
            return *this;
        }
};

typedef CTypeIterator<CObject> CObjectsIterator;
typedef CTypeConstIterator<CObject> CObjectsConstIterator;

template<class BeginInfo>
class CTypesIteratorBase : public CTreeIteratorBase<BeginInfo>
{
    typedef CTreeIteratorBase<BeginInfo> CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;
    typedef list<TTypeInfo> TTypeList;

    CTypesIteratorBase(void)
        {
        }
    CTypesIteratorBase(TTypeInfo type)
        {
            m_TypeList.push_back(type);
        }
    CTypesIteratorBase(TTypeInfo type1, TTypeInfo type2)
        {
            m_TypeList.push_back(type1);
            m_TypeList.push_back(type2);
        }
    CTypesIteratorBase(const TTypeList& typeList)
        : m_TypeList(typeList)
        {
        }
    CTypesIteratorBase(const TTypeList& typeList, const TBeginInfo& beginInfo)
        : m_TypeList(typeList)
        {
            Begin(beginInfo);
        }

    const TTypeList& GetTypeList(void) const
        {
            return m_TypeList;
        }

    CTypesIteratorBase<BeginInfo>& AddType(TTypeInfo type)
        {
            m_TypeList.push_back(type);
            return *this;
        }

    CTypesIteratorBase<BeginInfo>& operator=(const TBeginInfo& beginInfo)
        {
            Begin(beginInfo);
            return *this;
        }

    typename CParent::TObjectInfo::TObjectPtr GetFoundPtr(void) const
        {
            return Get().GetObjectPtr();
        }
    TTypeInfo GetFoundType(void) const
        {
            return Get().GetTypeInfo();
        }
    TTypeInfo GetMatchType(void) const
        {
            return m_MatchType;
        }

protected:
    virtual bool CanSelectCurrentObject(void);
    virtual bool CanEnterCurrentObject(void);

private:
    TTypeList m_TypeList;
    TTypeInfo m_MatchType;
};

typedef CTypesIteratorBase<CBeginInfo> CTypesIterator;
typedef CTypesIteratorBase<CConstBeginInfo> CTypesConstIterator;

enum EDetectLoops {
    eDetectLoops
};

template<class C>
inline
CBeginInfo Begin(C& obj)
{
    return CBeginInfo(&obj, C::GetTypeInfo(), false);
}

template<class C>
inline
CConstBeginInfo ConstBegin(const C& obj)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), false);
}

template<class C>
inline
CConstBeginInfo Begin(const C& obj)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), false);
}

template<class C>
inline
CBeginInfo Begin(C& obj, EDetectLoops)
{
    return CBeginInfo(&obj, C::GetTypeInfo(), true);
}

template<class C>
inline
CConstBeginInfo ConstBegin(const C& obj, EDetectLoops)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), true);
}

template<class C>
inline
CConstBeginInfo Begin(const C& obj, EDetectLoops)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), true);
}

template<class C>
class Type
{
public:
    static TTypeInfo GetTypeInfo(void)
        {
            return C::GetTypeInfo();
        }
    static void AddTo(CTypesIterator& i)
        {
            i.AddType(GetTypeInfo());
        }
    static void AddTo(CTypesConstIterator& i)
        {
            i.AddType(GetTypeInfo());
        }
    static bool Match(const CTypesIterator& i)
        {
            return i.GetMatchType() == GetTypeInfo();
        }
    static bool Match(const CTypesConstIterator& i)
        {
            return i.GetMatchType() == GetTypeInfo();
        }
    static C* Get(const CTypesIterator& i)
        {
            if ( !Match(i) )
                return 0;
            return static_cast<C*>(i.GetFoundPtr());
        }
    static const C* Get(const CTypesConstIterator& i)
        {
            if ( !Match(i) )
                return 0;
            return static_cast<const C*>(i.GetFoundPtr());
        }
};

//#include <serial/iterator.inl>

END_NCBI_SCOPE

#endif  /* ITERATOR__HPP */
