#ifndef MISC_NETSTORAGE___OBJECT__HPP
#define MISC_NETSTORAGE___OBJECT__HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <connect/services/impl/netstorage_impl.hpp>
#include "state.hpp"


BEGIN_NCBI_SCOPE


namespace NDirectNetStorageImpl
{

class CObj : public INetStorageObjectState, private IState, private ILocation
{
public:
    CObj(SNetStorageObjectImpl&, ISelector* selector, TNetStorageFlags flags) :
        m_Selector(selector->Clone(flags)),
        m_State(this),
        m_Location(this),
        m_IsOpened(false)
    {
        _ASSERT(m_Selector.get());
    }

    CObj(SNetStorageObjectImpl&, SContext* context, TNetStorageFlags flags) :
        m_Selector(context->Create(flags)),
        m_State(this),
        m_Location(this),
        m_IsOpened(false)
    {
    }

    CObj(SNetStorageObjectImpl&, SContext* context, TNetStorageFlags flags, const string& service) :
        m_Selector(context->Create(flags, service)),
        m_State(this),
        m_Location(this),
        m_IsOpened(false)
    {
        // Server reports locator to the client before writing anything
        // So, object must choose location for writing here to make locator valid
        m_Selector->SetLocator();
    }

    CObj(SNetStorageObjectImpl&, SContext* context, const string& object_loc) :
        m_Selector(context->Create(object_loc)),
        m_State(this),
        m_Location(this),
        m_IsOpened(true)
    {
    }

    CObj(SNetStorageObjectImpl&, SContext* context, const string& key, TNetStorageFlags flags, const string& service = kEmptyStr) :
        m_Selector(context->Create(key, flags, service)),
        m_State(this),
        m_Location(this),
        m_IsOpened(true)
    {
    }

    ERW_Result Read(void*, size_t, size_t*);
    ERW_Result PendingCount(size_t* count) { *count = 0; return eRW_Success; }

    ERW_Result Write(const void*, size_t, size_t*);
    ERW_Result Flush() { return eRW_Success; }

    void Close();
    void Abort();

    string GetLoc() const;
    bool Eof();
    Uint8 GetSize();
    list<string> GetAttributeList() const;
    string GetAttribute(const string&) const;
    void SetAttribute(const string&, const string&);
    CNetStorageObjectInfo GetInfo();
    void SetExpiration(const CTimeout&);

    string FileTrack_Path();
    TUserInfo GetUserInfo();

    CNetStorageObjectLoc& Locator();
    string Relocate(TNetStorageFlags, TNetStorageProgressCb cb);
    void CancelRelocate();
    bool Exists();
    ENetStorageRemoveResult Remove();

private:
    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
    ERW_Result WriteImpl(const void*, size_t, size_t*);
    void CloseImpl() {}
    void AbortImpl() {}

    template <class TCaller>
    auto Meta(TCaller caller)                   -> decltype(caller(nullptr));

    template <class TCaller>
    auto Meta(TCaller caller, bool restartable) -> decltype(caller(nullptr));

    template <class TCaller>
    auto MetaImpl(TCaller caller)               -> decltype(caller(nullptr));

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    ENetStorageRemoveResult RemoveImpl();
    void SetExpirationImpl(const CTimeout&);
    string FileTrack_PathImpl();
    TUserInfo GetUserInfoImpl();

    bool IsSame(const ILocation* other) const { return To<CObj>(other); }

    void RemoveOldCopyIfExists() const;

    SNetStorageObjectImpl* Clone(TNetStorageFlags flags, ISelector** selector) const
    {
        _ASSERT(selector);
        auto l = [&](CObj& state) { *selector = state.m_Selector.get(); };
        return SNetStorageObjectImpl::CreateAndStart<CObj>(l, m_Selector.get(), flags);
    }

    ISelector::Ptr m_Selector;
    IState* m_State;
    ILocation* m_Location;
    bool m_IsOpened;
    bool m_CancelRelocate = false;
};

}

END_NCBI_SCOPE

#endif
