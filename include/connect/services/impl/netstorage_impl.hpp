#ifndef CONNECT_SERVICES_IMPL__NETSTORAGE_IMPL__HPP
#define CONNECT_SERVICES_IMPL__NETSTORAGE_IMPL__HPP

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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   NetStorage implementation declarations.
 *
 */

#include <corelib/ncbi_url.hpp>
#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/netstorage.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

struct SNetStorageObjectImpl;

/// @internal
struct NCBI_XCONNECT_EXPORT INetStorageObjectState : public IEmbeddedStreamReaderWriter
{
    INetStorageObjectState(SNetStorageObjectImpl& fsm) : m_Fsm(fsm) {}

    virtual string GetLoc() const = 0;
    virtual bool Eof() = 0;
    virtual Uint8 GetSize() = 0;
    virtual list<string> GetAttributeList() const = 0;
    virtual string GetAttribute(const string& name) const = 0;
    virtual void SetAttribute(const string& name, const string& value) = 0;
    virtual CNetStorageObjectInfo GetInfo() = 0;
    virtual void SetExpiration(const CTimeout& ttl) = 0;
    virtual string FileTrack_Path() = 0;
    virtual pair<string, string> GetUserInfo() = 0;
    virtual CNetStorageObjectLoc& Locator() = 0;
    virtual string Relocate(TNetStorageFlags flags, TNetStorageProgressCb cb) = 0;
    virtual void CancelRelocate() = 0;
    virtual bool Exists() = 0;
    virtual ENetStorageRemoveResult Remove() = 0;

protected:
    void EnterState(INetStorageObjectState* state);
    void ExitState();

private:
    SNetStorageObjectImpl& m_Fsm;
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageObjectIoState : public INetStorageObjectState
{
    Uint8 GetSize() final;
    list<string> GetAttributeList() const final;
    string GetAttribute(const string& name) const final;
    void SetAttribute(const string& name, const string& value) final;
    CNetStorageObjectInfo GetInfo() final;
    void SetExpiration(const CTimeout& ttl) final;
    string FileTrack_Path() final;
    string Relocate(TNetStorageFlags flags, TNetStorageProgressCb cb) final;
    bool Exists() final;
    ENetStorageRemoveResult Remove() final;

private:
    SNetStorageObjectIoState(SNetStorageObjectImpl& fsm) : INetStorageObjectState(fsm) {}

    friend struct SNetStorageObjectIState;
    friend struct SNetStorageObjectOState;
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageObjectIState : public SNetStorageObjectIoState
{
    SNetStorageObjectIState(SNetStorageObjectImpl& fsm) : SNetStorageObjectIoState(fsm) {}

    ERW_Result Write(const void* buf, size_t count, size_t* written) final;
    ERW_Result Flush() final;
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageObjectOState : public SNetStorageObjectIoState
{
    SNetStorageObjectOState(SNetStorageObjectImpl& fsm) : SNetStorageObjectIoState(fsm) {}

    ERW_Result Read(void* buf, size_t count, size_t* read) final;
    ERW_Result PendingCount(size_t* count) final;
    bool Eof() final;
};

/// @internal
template <class TBase, class TContext>
struct SNetStorageObjectChildState : TBase
{
    SNetStorageObjectChildState(SNetStorageObjectImpl& fsm, TContext& context) : TBase(fsm), m_Context(context) {}

    string GetLoc() const final                                       { return m_Context.locator; }
    pair<string, string> GetUserInfo() final                          { return m_Context.GetUserInfo(); }
    CNetStorageObjectLoc& Locator() final                             { return m_Context.Locator(); }
    void CancelRelocate() final                                       { return m_Context.CancelRelocate(); }

protected:
    TContext& m_Context;
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageObjectIoMode
{
    enum EApi { eAnyApi, eBuffer, eIoStream, eIReaderIWriter, eString };
    enum EMth { eAnyMth, eRead, eWrite, eEof };

    bool Set(EApi api, EMth mth)
    {
        if (m_Api != eAnyApi && m_Api != api) return false;

        m_Api = api;
        m_Mth = mth;
        return true;
    }

    void Reset() { m_Api = eAnyApi; }
    bool IoStream() const { return m_Api == eIoStream; }
    void Throw(EApi api, EMth mth, string object_loc);

private:
    static string ToString(EApi api, EMth mth);

    EApi m_Api = eAnyApi;
    EMth m_Mth = eAnyMth;
};

/// @internal
// TODO:
// Remove IReader and IEmbeddedStreamWriter bases,
// Make all methods non virtual
struct NCBI_XCONNECT_EXPORT SNetStorageObjectImpl : public CObject, public IEmbeddedStreamReaderWriter
{
    ~SNetStorageObjectImpl();

    void SetStartState(INetStorageObjectState* state);

    void SetIoMode(SNetStorageObjectIoMode::EApi api, SNetStorageObjectIoMode::EMth mth)
    {
        if (!m_IoMode.Set(api, mth)) m_IoMode.Throw(api, mth, GetLoc());
    }

    IEmbeddedStreamReaderWriter& GetReaderWriter();
    CNcbiIostream* GetRWStream();

    virtual ERW_Result Read(void* buf, size_t count, size_t* read);
    virtual ERW_Result PendingCount(size_t* count);

    virtual ERW_Result Write(const void* buf, size_t count, size_t* written);
    virtual ERW_Result Flush();

    virtual void Close();
    virtual void Abort();

    virtual string GetLoc() const                                      { A(); return m_Current->GetLoc(); }
    virtual bool Eof()                                                 { A(); return m_Current->Eof(); }
    virtual Uint8 GetSize()                                            { A(); return m_Current->GetSize(); }
    virtual list<string> GetAttributeList() const                      { A(); return m_Current->GetAttributeList(); }
    virtual string GetAttribute(const string& name) const              { A(); return m_Current->GetAttribute(name); }
    virtual void SetAttribute(const string& name, const string& value) { A(); return m_Current->SetAttribute(name, value); }
    virtual CNetStorageObjectInfo GetInfo()                            { A(); return m_Current->GetInfo(); }
    virtual void SetExpiration(const CTimeout& ttl)                    { A(); return m_Current->SetExpiration(ttl); }
    virtual string FileTrack_Path()                                    { A(); return m_Current->FileTrack_Path(); }
    virtual pair<string, string> GetUserInfo()                         { A(); return m_Current->GetUserInfo(); }
    virtual CNetStorageObjectLoc& Locator()                            { A(); return m_Current->Locator(); }
    virtual string Relocate(TNetStorageFlags flags, TNetStorageProgressCb cb) { A(); return m_Current->Relocate(flags, cb); }
    virtual void CancelRelocate()                                      { A(); return m_Current->CancelRelocate(); }
    virtual bool Exists()                                              { A(); return m_Current->Exists(); }
    virtual ENetStorageRemoveResult Remove()                           { A(); return m_Current->Remove(); }

private:
    void A() const { _ASSERT(m_Current); }
    void EnterState(INetStorageObjectState* state);
    void ExitState();

    unique_ptr<IEmbeddedStreamReaderWriter> m_ReaderWriter;
    unique_ptr<IEmbeddedStreamReaderWriter> m_IoStreamReaderWriter;
    unique_ptr<INetStorageObjectState> m_Start;
    stack<INetStorageObjectState*> m_Previous;
    INetStorageObjectState* m_Current = nullptr;
    SNetStorageObjectIoMode m_IoMode;

    friend struct INetStorageObjectState;
};

inline void SNetStorageObjectImpl::SetStartState(INetStorageObjectState* state)
{
    _ASSERT(state);
    _ASSERT(!m_Start);
    m_Start.reset(state);
    m_Current = state;
}

inline void SNetStorageObjectImpl::EnterState(INetStorageObjectState* state)
{
    _ASSERT(state);
    m_Previous.push(m_Current);
    m_Current = state;
}

inline void SNetStorageObjectImpl::ExitState()
{
    _ASSERT(!m_Previous.empty());
    m_Current = m_Previous.top();
    m_Previous.pop();
}

inline void INetStorageObjectState::EnterState(INetStorageObjectState* state)
{
    m_Fsm.EnterState(state);
}

inline void INetStorageObjectState::ExitState()
{
    m_Fsm.ExitState();
}

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorage
{
    struct SConfig;
    struct SLimits;

    static SNetStorageImpl* CreateImpl(const SConfig&, TNetStorageFlags);
    static SNetStorageByKeyImpl* CreateByKeyImpl(const SConfig&, TNetStorageFlags);
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorage::SConfig
{
    enum EDefaultStorage {
        eUndefined,
        eNetStorage,
        eNetCache,
        eNoCreate,
    };

    enum EErrMode {
        eThrow,
        eLog,
        eIgnore,
    };

    string service;
    string nc_service;
    string app_domain;
    string client_name;
    string metadata;
    EDefaultStorage default_storage;
    EErrMode err_mode;
    string ticket;
    string hello_service;

    SConfig() : default_storage(eUndefined), err_mode(eLog) {}
    void ParseArg(const string&, const string&);
    void Validate(const string&);

    static SConfig Build(const string& init_string)
    {
        return BuildImpl<SConfig>(init_string);
    }

protected:
    template <class TConfig>
    static TConfig BuildImpl(const string& init_string)
    {
        CUrlArgs url_parser(init_string);
        TConfig cfg;

        ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
            if (!field->name.empty() && !field->value.empty()) {
                cfg.ParseArg(field->name, field->value);
            }
        }

        cfg.Validate(init_string);
        return cfg;
    }

private:
    static EDefaultStorage GetDefaultStorage(const string&);
    static EErrMode GetErrMode(const string&);
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorage::SLimits
{
    struct SNamespace
    {
        static string   Name()          { return "Namespace"; }
        static size_t   MaxLength()     { return 32; }
        static bool     IsValid(char c) { return isalnum(c) || c == '_'; };
    };

    struct SUserKey
    {
        static string   Name()          { return "User key"; }
        static size_t   MaxLength()     { return 256; }
        static bool     IsValid(char c) { return ::isprint(c); };
    };

    struct SAttrName
    {
        static string   Name()          { return "Attribute name"; }
        static size_t   MaxLength()     { return 64; }
        static bool     IsValid(char c) { return isalnum(c) || c == '_'; };
    };

    struct SAttrValue
    {
        static string   Name()          { return "Attribute value"; }
        static size_t   MaxLength()     { return 900; }
        static bool     IsValid(char)   { return true; };
    };

    struct SClientName
    {
        static string   Name()          { return "Client name"; }
        static size_t   MaxLength()     { return 256; }
        static bool     IsValid(char c) { return ::isprint(c); };
    };

    struct SUserNamespace
    {
        static string   Name()          { return "User namespace"; }
        static size_t   MaxLength()     { return 64; }
        static bool     IsValid(char c) { return isalnum(c) || c == '_'; };
    };

    struct SUserName
    {
        static string   Name()          { return "User name"; }
        static size_t   MaxLength()     { return 64; }
        static bool     IsValid(char c) { return isalnum(c) || c == '_'; };
    };

    template <class TValue>
    static void Check(const string& value)
    {
        if (value.length() > TValue::MaxLength()) {
            ThrowTooLong(TValue::Name(), TValue::MaxLength());
        }

        if (!all_of(value.begin(), value.end(), TValue::IsValid)) {
            ThrowIllegalChars(TValue::Name(), value);
        }
    }

private:
    static void ThrowTooLong(const string&, size_t);
    static void ThrowIllegalChars(const string&, const string&);
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageImpl : public CObject
{
    typedef SNetStorage::SConfig TConfig;

    virtual CNetStorageObject Create(TNetStorageFlags flags) = 0;
    virtual CNetStorageObject Open(const string& object_loc) = 0;
    virtual ENetStorageRemoveResult Remove(const string& object_loc) = 0;
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageByKeyImpl : public CObject
{
    typedef SNetStorage::SConfig TConfig;

    virtual CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags) = 0;
    virtual ENetStorageRemoveResult Remove(const string& key,
            TNetStorageFlags flags) = 0;
};

#define NETSTORAGE_CONVERT_NETCACHEEXCEPTION(message) \
    catch (CNetCacheException& e) { \
        g_ThrowNetStorageException(DIAG_COMPILE_INFO, e, FORMAT(message)); \
    }
NCBI_XCONNECT_EXPORT
void g_ThrowNetStorageException(const CDiagCompileInfo& compile_info,
        const CNetCacheException& prev_exception, const string& message);

NCBI_XCONNECT_EXPORT
CNetStorageObjectInfo g_CreateNetStorageObjectInfo(const string& object_loc,
        ENetStorageObjectLocation location,
        const CNetStorageObjectLoc* object_loc_struct,
        Uint8 file_size, CJsonNode::TInstance storage_specific_info);

NCBI_XCONNECT_EXPORT
CNetStorageObjectInfo g_CreateNetStorageObjectInfo(
        const CJsonNode& object_info_node);

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES_IMPL__NETSTORAGE_IMPL__HPP */
