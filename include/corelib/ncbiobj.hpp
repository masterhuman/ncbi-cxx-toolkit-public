#ifndef NCBIOBJ__HPP
#define NCBIOBJ__HPP

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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Standard CObject and CRef classes for reference counter based GC
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/ddumpable.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

enum ENull {
    null = 0
};

class CNullPointerError : public exception
{
public:
    CNullPointerError(void) THROWS_NONE;
    ~CNullPointerError(void) THROWS_NONE;

    const char* what() const THROWS_NONE;
};



class CObject : virtual public CDebugDumpable
{
public:
    // constructors
    CObject(void);
    CObject(const CObject& src);
    // virtual destructor
    virtual ~CObject(void);

    // copy
    CObject& operator=(const CObject& src) THROWS_NONE;

    // checks state of reference counter
    bool CanBeDeleted(void) const THROWS_NONE;
    bool Referenced(void) const THROWS_NONE;
    bool ReferencedOnlyOnce(void) const THROWS_NONE;


    // change state of object
    void AddReference(void) const;
    void RemoveReference(void) const;

    // remove reference without deleting object
    void ReleaseReference(void) const;

    // mark this object as not allocated in heap
    virtual void DoNotDeleteThisObject(void);
    // mark this object as allocated in heap
    virtual void DoDeleteThisObject(void);

    // operators new/delete for additional checking in debug mode
    void* operator new(size_t size);
    void* operator new[](size_t size);
    void operator delete(void* ptr);
    void operator delete[](void* ptr);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

private:
    typedef CAtomicCounter   TCounter;
    typedef TCounter::TValue TCount;

    // special flag in counter meaning that object is not allocated in heap
    // [0]0x...xxx - non valid object -> cannot be referenced
    // [0]1c...cc0 - object non in heap -> can (not?) be referenced
    // [0]1c...cc1 - object in heap -> can be referenced
    // When TCounter is signed, all valid values start with 01;
    // when it is unsigned, they start with 1.
    enum EObjectState {
        eStateBitsInHeap  = 1 << 0,
#ifdef NCBI_COUNTER_UNSIGNED
        eStateBitsValid   = int(1 << (sizeof(TCount) * 8 - 1)),
#else
        eStateBitsValid   = int(1 << (sizeof(TCount) * 8 - 2)),
#endif
        eStateMask        = eStateBitsValid | eStateBitsInHeap,
        eCounterStep      = 1 << 1, // over InHeap bit

        eCounterNotInHeap = eStateBitsValid,
        eCounterInHeap    = eStateBitsValid | eStateBitsInHeap,

        eCounterValid     = eStateBitsValid,

        eSpecialValueMask = eStateBitsValid - eCounterStep,
        eCounterDeleted   = int((0x5b4d9f34 | ((0xada87e65 << 16) << 16))
                                & eSpecialValueMask),
        eCounterNew       = int((0x3423cb13 | ((0xfe234228 << 16) << 16))
                                & eSpecialValueMask)
    };

    // special methods for parsing object state number
    static bool ObjectStateValid(TCount count);
    static bool ObjectStateCanBeDeleted(TCount count);
    static bool ObjectStateReferenced(TCount count);
    static bool ObjectStateDoubleReferenced(TCount count);
    static bool ObjectStateReferencedOnlyOnce(TCount count);

    // initialize
    void InitCounter(void);

    // check special states
    void RemoveLastReference(void) const;

    // report different kinds of error
    void InvalidObject(void) const; // using of deleted object
    void AddReferenceOverflow(TCount count) const; // counter overflow/bad

    mutable TCounter  m_Counter;  // reference counter
    static CFastMutex sm_ObjectMutex;   // reference counter's protective mutex
};


template<class C>
class CRefBase
{
public:
    static
    void AddReference(const C* object)
        {
            object->AddReference();
        }

    static
    void RemoveReference(const C* object)
        {
            object->RemoveReference();
        }
    
    static
    void ReleaseReference(const C* object)
        {
            object->ReleaseReference();
        }
};


template<class C>
class CRef {
public:
    typedef C element_type;
    typedef element_type TObjectType;

    inline
    CRef(void) THROWS_NONE
        : m_Ptr(0)
        {
        }
    inline
    CRef(ENull /*null*/) THROWS_NONE
        : m_Ptr(0)
        {
        }
    CRef(TObjectType* ptr)
        {
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    CRef(const CRef<C>& ref)
        {
            TObjectType* ptr = ref.m_Ptr;
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    ~CRef(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr )
                CRefBase<C>::RemoveReference(ptr);
        }
    
    // test
    bool Empty(void) const THROWS_NONE
        {
            return m_Ptr == 0;
        }
    bool NotEmpty(void) const THROWS_NONE
        {
            return m_Ptr != 0;
        }
    bool IsNull(void) const THROWS_NONE
        {
            return m_Ptr == 0;
        }

    // test
    operator bool(void) THROWS_NONE
        {
            return NotEmpty();
        }
    operator bool(void) const THROWS_NONE
        {
            return NotEmpty();
        }
    bool operator!(void) const THROWS_NONE
        {
            return Empty();
        }

    // reset
    inline
    void Reset(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr ) {
                m_Ptr = 0;
                CRefBase<C>::RemoveReference(ptr);
            }
        }
    inline
    void Reset(TObjectType* newPtr)
        {
            TObjectType* oldPtr = m_Ptr;
            if ( newPtr != oldPtr ) {
                if ( newPtr )
                    CRefBase<C>::AddReference(newPtr);
                m_Ptr = newPtr;
                if ( oldPtr )
                    CRefBase<C>::RemoveReference(oldPtr);
            }
        }

    // release
    inline
    TObjectType* ReleaseOrNull(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                return 0;
            m_Ptr = 0;
            CRefBase<C>::ReleaseReference(ptr);
            return ptr;
        }
    inline
    TObjectType* Release(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            m_Ptr = 0;
            CRefBase<C>::ReleaseReference(ptr);
            return ptr;
        }

    // assign
    CRef<C>& operator=(const CRef<C>& ref)
        {
            Reset(ref.m_Ptr);
            return *this;
        }
    CRef<C>& operator=(TObjectType* ptr)
        {
            Reset(ptr);
            return *this;
        }
    CRef<C>& operator=(ENull /*null*/)
        {
            Reset(0);
            return *this;
        }

    // getters
    inline
    TObjectType* GetNonNullPointer(void) const
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return m_Ptr;
        }
    inline
    TObjectType* GetPointerOrNull(void) const THROWS_NONE
        {
            return m_Ptr;
        }
    inline
    TObjectType* GetPointer(void) const THROWS_NONE
        {
            return GetPointerOrNull();
        }
    inline
    TObjectType& GetObject(void) const
        {
            return *GetNonNullPointer();
        }

    inline
    TObjectType& operator*(void) const
        {
            return GetObject();
        }
    inline
    TObjectType* operator->(void) const
        {
            return GetPointer();
        }
    inline
    operator TObjectType*(void) const
        {
            return GetPointer();
        }

private:
    TObjectType* m_Ptr;
};


template<class C>
class CConstRef {
public:
    typedef C element_type;
    typedef const element_type TObjectType;

    inline
    CConstRef(void) THROWS_NONE
        : m_Ptr(0)
        {
        }
    inline
    CConstRef(ENull /*null*/) THROWS_NONE
        : m_Ptr(0)
        {
        }
    CConstRef(TObjectType* ptr)
        {
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    CConstRef(const CConstRef<C>& ref)
        {
            TObjectType* ptr = ref.m_Ptr;
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    CConstRef(const CRef<C>& ref)
        {
            TObjectType* ptr = ref.GetPointerOrNull();
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    ~CConstRef(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr )
                CRefBase<C>::RemoveReference(ptr);
        }
    
    // test
    bool Empty(void) const THROWS_NONE
        {
            return m_Ptr == 0;
        }
    bool NotEmpty(void) const THROWS_NONE
        {
            return m_Ptr != 0;
        }
    bool IsNull(void) const THROWS_NONE
        {
            return m_Ptr == 0;
        }

    // test
    operator bool(void) THROWS_NONE
        {
            return NotEmpty();
        }
    operator bool(void) const THROWS_NONE
        {
            return NotEmpty();
        }
    bool operator!(void) const THROWS_NONE
        {
            return Empty();
        }

    // reset
    inline
    void Reset(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr ) {
                m_Ptr = 0;
                CRefBase<C>::RemoveReference(ptr);
            }
        }
    inline
    void Reset(TObjectType* newPtr)
        {
            TObjectType* oldPtr = m_Ptr;
            if ( newPtr != oldPtr ) {
                if ( newPtr )
                    CRefBase<C>::AddReference(newPtr);
                m_Ptr = newPtr;
                if ( oldPtr )
                    CRefBase<C>::RemoveReference(oldPtr);
            }
        }

    // release
    inline
    TObjectType* ReleaseOrNull(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                return 0;
            m_Ptr = 0;
            CRefBase<C>::ReleaseReference(ptr);
            return ptr;
        }
    inline
    TObjectType* Release(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            m_Ptr = 0;
            CRefBase<C>::ReleaseReference(ptr);
            return ptr;
        }

    // assign
    CConstRef<C>& operator=(const CConstRef<C>& ref)
        {
            Reset(ref.m_Ptr);
            return *this;
        }
    CConstRef<C>& operator=(const CRef<C>& ref)
        {
            Reset(ref.GetPointerOrNull());
            return *this;
        }
    CConstRef<C>& operator=(TObjectType* ptr)
        {
            Reset(ptr);
            return *this;
        }
    CConstRef<C>& operator=(ENull /*null*/)
        {
            Reset(0);
            return *this;
        }

    // getters
    inline
    TObjectType* GetNonNullPointer(void) const
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return m_Ptr;
        }
    inline
    TObjectType* GetPointerOrNull(void) const THROWS_NONE
        {
            return m_Ptr;
        }
    inline
    TObjectType* GetPointer(void) const THROWS_NONE
        {
            return GetPointerOrNull();
        }
    inline
    TObjectType& GetObject(void) const
        {
            return *GetNonNullPointer();
        }

    inline
    TObjectType& operator*(void) const
        {
            return GetObject();
        }
    inline
    TObjectType* operator->(void) const
        {
            return GetPointer();
        }
    inline
    operator TObjectType*(void) const
        {
            return GetPointer();
        }

private:
    TObjectType* m_Ptr;
};



template<class T>
bool operator< (const CRef<T>& r1, const CRef<T>& r2)
{
    return r1.GetPointer() < r2.GetPointer();
}

template<class T>
bool operator> (const CRef<T>& r1, const CRef<T>& r2)
{
    return r1.GetPointer() > r2.GetPointer();
}

template<class T>
bool operator== (const CRef<T>& r1, ENull /*null*/)
{
    return r1.IsNull();
}

template<class T>
bool operator== (ENull /*null*/, const CRef<T>& r1)
{
    return r1.IsNull();
}

template<class T>
bool operator!= (const CRef<T>& r1, ENull /*null*/)
{
    return !r1.IsNull();
}

template<class T>
bool operator!= (ENull /*null*/, const CRef<T>& r1)
{
    return !r1.IsNull();
}

template<class T>
bool operator< (const CConstRef<T>& r1, const CConstRef<T>& r2)
{
    return r1.GetPointer() < r2.GetPointer();
}

template<class T>
bool operator> (const CConstRef<T>& r1, const CConstRef<T>& r2)
{
    return r1.GetPointer() > r2.GetPointer();
}

template<class T>
bool operator== (const CConstRef<T>& r1, ENull /*null*/)
{
    return r1.IsNull();
}

template<class T>
bool operator== (ENull /*null*/, const CConstRef<T>& r1)
{
    return r1.IsNull();
}

template<class T>
bool operator!= (const CConstRef<T>& r1, ENull /*null*/)
{
    return !r1.IsNull();
}

template<class T>
bool operator!= (ENull /*null*/, const CConstRef<T>& r1)
{
    return !r1.IsNull();
}

template<class T>
bool operator== (const CRef<T>& r1, const CRef<T>& r2)
{
    return r1.GetPointer() == r2.GetPointer();
}

template<class T>
bool operator== (const CConstRef<T>& r1, const CConstRef<T>& r2)
{
    return r1.GetPointer() == r2.GetPointer();
}

template<class T>
bool operator== (const CConstRef<T>& r1, const CRef<T>& r2)
{
    return r1.GetPointer() == r2.GetPointer();
}

template<class T>
bool operator== (const CRef<T>& r1, const CConstRef<T>& r2)
{
    return r1.GetPointer() == r2.GetPointer();
}

template<class T>
bool operator!= (const CRef<T>& r1, const CRef<T>& r2)
{
    return r1.GetPointer() != r2.GetPointer();
}

template<class T>
bool operator!= (const CConstRef<T>& r1, const CConstRef<T>& r2)
{
    return r1.GetPointer() != r2.GetPointer();
}

template<class T>
bool operator!= (const CConstRef<T>& r1, const CRef<T>& r2)
{
    return r1.GetPointer() != r2.GetPointer();
}

template<class T>
bool operator!= (const CRef<T>& r1, const CConstRef<T>& r2)
{
    return r1.GetPointer() != r2.GetPointer();
}




template<typename T>
class CObjectFor : public CObject
{
public:
    typedef T TObjectType;

    T& GetData(void)
        {
            return m_Data;
        }
    const T& GetData(void) const
        {
            return m_Data;
        }
    operator T& (void)
        {
            return GetData();
        }
    operator const T& (void) const
        {
            return GetData();
        }

    T& operator=(const T& data)
        {
            m_Data = data;
            return *this;
        }

private:
    T m_Data;
};



#include <corelib/ncbiobj.inl>

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.31  2002/05/23 22:24:21  ucko
 * Use low-level atomic operations for reference counts
 *
 * Revision 1.30  2002/05/17 14:25:40  gouriano
 * added DebugDump base class and function to CObject
 *
 * Revision 1.29  2002/05/14 21:12:59  gouriano
 * DebugDump moved into a separate class
 *
 * Revision 1.28  2002/05/14 14:42:13  gouriano
 * added DebugDump function to CObject
 *
 * Revision 1.27  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.26  2001/10/10 04:03:22  vakatov
 * Added operator> for C(Const)Ref -- for ICC and MIPSpro
 *
 * Revision 1.25  2001/06/21 15:17:42  kholodov
 * Added: null special value, support for null in CRef classes, equality
 * operators.
 *
 * Revision 1.24  2001/06/13 14:19:54  grichenk
 * Added operators == and != for C(Const)Ref
 *
 * Revision 1.23  2001/05/17 14:53:56  lavr
 * Typos corrected
 *
 * Revision 1.22  2001/03/26 21:22:51  vakatov
 * Minor cosmetics
 *
 * Revision 1.21  2001/03/13 22:43:48  vakatov
 * Made "CObject" MT-safe
 * + CObject::DoDeleteThisObject()
 *
 * Revision 1.20  2001/03/05 22:14:18  vakatov
 * Added "operator<" for CRef:: and CConstRef:: to make them usable
 * as keys in the stnadard C++ associative containers (set, map, ...)
 *
 * Revision 1.19  2001/02/21 21:16:08  grichenk
 * CRef:: Release, Reset -- reset m_Ptr BEFORE removing the reference
 *
 * Revision 1.18  2000/12/26 17:25:38  vasilche
 * CRef<> returns non const object.
 *
 * Revision 1.17  2000/12/15 19:18:36  vakatov
 * Added assignment operator for CRef<> and CConstRef<>
 *
 * Revision 1.16  2000/12/15 15:36:29  vasilche
 * Added header corelib/ncbistr.hpp for all string utility functions.
 * Optimized string utility functions.
 * Added assignment operator to CRef<> and CConstRef<>.
 * Add Upcase() and Locase() methods for automatic conversion.
 *
 * Revision 1.15  2000/12/12 14:20:14  vasilche
 * Added operator bool to CArgValue.
 * Added standard typedef element_type to CRef<> and CConstRef<>.
 * Macro iterate() now calls method end() only once and uses temporary variabl.
 * Various NStr::Compare() methods made faster.
 * Added class Upcase for printing strings to ostream with automatic conversion
 *
 * Revision 1.14  2000/11/01 20:35:01  vasilche
 * Fixed detection of heap objects.
 * Removed ECanDelete enum and related constructors.
 *
 * Revision 1.13  2000/10/13 16:25:43  vasilche
 * Added heuristic for detection of CObject allocation in heap.
 *
 * Revision 1.12  2000/09/01 13:14:25  vasilche
 * Fixed throw() declaration in CRef/CConstRef
 *
 * Revision 1.11  2000/08/15 19:42:06  vasilche
 * Changed reference counter to allow detection of more errors.
 *
 * Revision 1.10  2000/06/16 16:29:42  vasilche
 * Added SetCanDelete() method to allow to change CObject 'in heap' status 
 * immediately after creation.
 *
 * Revision 1.9  2000/06/07 19:44:16  vasilche
 * Removed unneeded THROWS declaration - they lead to encreased code size.
 *
 * Revision 1.8  2000/05/09 16:36:54  vasilche
 * CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce
 * possible errors.
 *
 * Revision 1.7  2000/04/28 16:56:13  vasilche
 * Fixed implementation of CRef<> and CConstRef<>
 *
 * Revision 1.6  2000/03/31 17:08:07  kans
 * moved ECanDelete to public area of CObject
 *
 * Revision 1.5  2000/03/29 15:50:27  vasilche
 * Added const version of CRef - CConstRef.
 * CRef and CConstRef now accept classes inherited from CObject.
 *
 * Revision 1.4  2000/03/10 14:18:37  vasilche
 * Added CRef<>::GetPointerOrNull() method similar to std::auto_ptr<>.get()
 *
 * Revision 1.3  2000/03/08 14:18:19  vasilche
 * Fixed throws instructions.
 *
 * Revision 1.2  2000/03/07 15:25:42  vasilche
 * Fixed implementation of CRef::->
 *
 * Revision 1.1  2000/03/07 14:03:11  vasilche
 * Added CObject class as base for reference counted objects.
 * Added CRef templace for reference to CObject descendant.
 *
 * ==========================================================================
 */

#endif /* NCBIOBJ__HPP */
