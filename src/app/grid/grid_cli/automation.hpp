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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Declaration of the automation processor.
 *
 */

#ifndef AUTOMATION__HPP
#define AUTOMATION__HPP

#include "ns_cmd_impl.hpp"
#include "util.hpp"

BEGIN_NCBI_SCOPE

namespace NAutomation
{

class CAutomationException : public CException
{
public:
    enum EErrCode {
        eInvalidInput,
        eCommandProcessingError,
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eInvalidInput:
            return "eInvalidInput";
        case eCommandProcessingError:
            return "eCommandProcessingError";
        default:
            return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CAutomationException, CException);
};

class CArgArray
{
public:
    CArgArray(const CJsonNode& args);

    CJsonNode NextNodeOrNull();
    CJsonNode NextNode();

    string GetString(const CJsonNode& node);
    string NextString() {return GetString(NextNode());}
    string NextString(const string& default_value);

    Int8 GetInteger(const CJsonNode& node);
    Int8 NextInteger() {return GetInteger(NextNode());}
    Int8 NextInteger(Int8 default_value);

    bool GetBoolean(const CJsonNode& node);
    bool NextBoolean() {return GetBoolean(NextNode());}
    bool NextBoolean(bool default_value);

    CJsonNode GetArray(const CJsonNode& node);
    CJsonNode NextArray() {return GetArray(NextNode());}

    void UpdateLocation(const string& location);
    void Exception(const char* what);

private:
    CJsonNode m_Args;
    CJsonIterator m_Position;
    string m_Location;
};

inline CArgArray::CArgArray(const CJsonNode& args) :
    m_Args(args),
    m_Position(args.Iterate())
{
}

inline CJsonNode CArgArray::NextNodeOrNull()
{
    if (m_Position) {
        if (!(*m_Position).IsNull()) {
            CJsonNode result(*m_Position);
            m_Position.Next();
            return result;
        }
        m_Position.Next();
    }
    return CJsonNode();
}

inline CJsonNode CArgArray::NextNode()
{
    CJsonNode next_node(NextNodeOrNull());
    if (!next_node)
        Exception("insufficient number of arguments");
    return next_node;
}

inline string CArgArray::GetString(const CJsonNode& node)
{
    if (!node.IsString())
        Exception("invalid argument type (expected a string)");
    return node.AsString();
}

inline string CArgArray::NextString(const string& default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetString(next_node) : default_value;
}

inline Int8 CArgArray::GetInteger(const CJsonNode& node)
{
    if (!node.IsInteger())
        Exception("invalid argument type (expected an integer)");
    return node.AsInteger();
}

inline Int8 CArgArray::NextInteger(Int8 default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetInteger(next_node) : default_value;
}

inline bool CArgArray::GetBoolean(const CJsonNode& node)
{
    if (!node.IsBoolean())
        Exception("invalid argument type (expected a boolean)");
    return node.AsBoolean();
}

inline bool CArgArray::NextBoolean(bool default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetBoolean(next_node) : default_value;
}

inline CJsonNode CArgArray::GetArray(const CJsonNode& node)
{
    if (!node.IsArray())
        Exception("invalid argument type (expected an array)");
    return node;
}

inline void CArgArray::UpdateLocation(const string& location)
{
    if (m_Location.empty())
        m_Location = location;
    else {
        m_Location.push_back(' ');
        m_Location.append(location);
    }
}

struct SInputOutput
{
    CJsonIterator input;
    CArgArray arg_array; // TODO: Remove after all commands are migrated to new parse system
    CJsonNode reply;

    SInputOutput(const CJsonNode& message) :
        input(message.Iterate()),
        arg_array(message),
        reply(CJsonNode::NewArrayNode())
    {}
};

class CArgument
{
public:
    CArgument(CJsonIterator& input) :
        m_Value(input.GetNode())
    {}

    CArgument(string name, CJsonNode::ENodeType type) :
        m_Name(name), 
        m_TypeOrDefaultValue(type),
        m_Optional(false)
    {}

    template <typename TValue>
    CArgument(string name, const TValue& value) :
        m_Name(name), 
        m_TypeOrDefaultValue(value),
        m_Optional(true)
    {}

    CJsonNode Help();
    void Exec(CJsonIterator& input);
    CJsonNode Value() const { return m_Value; }

    // Shortcuts
    template <typename TInt = Int8>
    TInt   AsInteger() const { return static_cast<TInt>(m_Value.AsInteger()); }
    string AsString()  const { return                   m_Value.AsString();   }
    double AsDouble()  const { return                   m_Value.AsDouble();   }
    bool   AsBoolean() const { return                   m_Value.AsBoolean();  }

private:
    string m_Name;
    CJsonNode m_TypeOrDefaultValue;
    CJsonNode m_Value;
    bool m_Optional;
};

struct SCommandImpl;
class CCommand;

typedef initializer_list<CArgument> TArgsInit;
typedef vector<CArgument> TArguments;
typedef vector<CCommand> TCommands;
typedef function<TCommands()> TCommandsGetter;
typedef function<void(const TArguments&, SInputOutput&, void*)> TCommandExecutor;
typedef function<bool(const string&, SInputOutput&, void*&)> TCommandChecker;
typedef pair<TCommandsGetter, TCommandChecker> TCommandGroup;

class CCommand
{
public:
    CCommand(string name, TCommandExecutor exec, TArgsInit args = TArgsInit());
    CCommand(string name, TCommandExecutor exec, const char * const args);
    CCommand(string name, TCommandsGetter getter);
    CCommand(string name, TCommandGroup group);

    CJsonNode Help(CJsonIterator& input);
    bool Exec(SInputOutput& io, void* data);

private:
    string m_Name;
    shared_ptr<SCommandImpl> m_Impl;
};

struct SServerAddressToJson : public IExecToJson
{
    SServerAddressToJson(int which_part) : m_WhichPart(which_part) {}

    virtual CJsonNode ExecOn(CNetServer server);

    int m_WhichPart;
};

typedef Int8 TObjectID;

class CAutomationProc;

class CAutomationObject : public CObject
{
public:
    CAutomationObject(CAutomationProc* automation_proc) :
        m_AutomationProc(automation_proc)
    {
    }

    void SetID(TObjectID object_id) {m_Id = object_id;}

    TObjectID GetID() const {return m_Id;}

    virtual const string& GetType() const = 0;

    virtual const void* GetImplPtr() const = 0;

    virtual bool Call(const string& method, const TArguments& args, SInputOutput& io) = 0;

    CAutomationProc* m_AutomationProc;

    template <class TDerived>
    static bool CheckCall(const string& name, SInputOutput& io, void*& data);

    template <class TDerived>
    static void ExecNew(const TArguments& args, SInputOutput& io, void* data);

    template <class TClass, void(TClass::*Method)(const TArguments&, SInputOutput& io)>
    static void ExecMethod(const TArguments& args, SInputOutput& io, void* data);

protected:
    TObjectID m_Id;
};

struct SNetServiceBase : public CAutomationObject
{
    using TSelf = SNetServiceBase;

    SNetServiceBase(CAutomationProc* automation_proc,
            CNetService::EServiceType actual_service_type) :
        CAutomationObject(automation_proc),
        m_ActualServiceType(actual_service_type)
    {
    }

    bool Call(const string& method, const TArguments& args, SInputOutput& io) override;
    void ExecGetName(const TArguments& args, SInputOutput& io);
    void ExecGetAddress(const TArguments& args, SInputOutput& io);

    CNetService m_Service;
    CNetService::EServiceType m_ActualServiceType;

    static TCommands CallCommands();
};

struct SNetService : public SNetServiceBase
{
    using TSelf = SNetService;

    SNetService(CAutomationProc* automation_proc,
            CNetService::EServiceType actual_service_type) :
        SNetServiceBase(automation_proc, actual_service_type)
    {
    }

    bool Call(const string& method, const TArguments& args, SInputOutput& io) override;
    void ExecServerInfo(const TArguments& args, SInputOutput& io);
    void ExecExec(const TArguments& args, SInputOutput& io);

    static TCommands CallCommands();
};

typedef CRef<CAutomationObject> TAutomationObjectRef;

class IMessageSender
{
public:
    virtual void SendMessage(const CJsonNode& message) = 0;

    virtual ~IMessageSender() {}
};

class CAutomationProc
{
public:
    CAutomationProc(IMessageSender* message_sender);

    CJsonNode ProcessMessage(const CJsonNode& message);

    void SendWarning(const string& warn_msg, TAutomationObjectRef source);

    void SendError(const CTempString& error_message);

    TObjectID AddObject(TAutomationObjectRef new_object);
    TObjectID AddObject(TAutomationObjectRef new_object, const void* impl_ptr);

    TAutomationObjectRef FindObjectByPtr(const void* impl_ptr) const;

    TAutomationObjectRef ReturnNetCacheServerObject(
            CNetICacheClient::TInstance ic_api,
            CNetServer::TInstance server);

    TAutomationObjectRef ReturnNetScheduleServerObject(
            CNetScheduleAPI::TInstance ns_api,
            CNetServer::TInstance server);

    TAutomationObjectRef ReturnNetStorageServerObject(
            CNetStorageAdmin::TInstance nst_api,
            CNetServer::TInstance server);

    static void ExecAllowXSite(const TArguments& args, SInputOutput& io, void* data);

    TAutomationObjectRef& ObjectIdToRef(TObjectID object_id);

private:
    IMessageSender* m_MessageSender;

    vector<TAutomationObjectRef> m_ObjectByIndex;
    typedef map<const void*, TAutomationObjectRef> TPtrToObjectRefMap;
    TPtrToObjectRefMap m_ObjectByPointer;

    CJsonNode m_OKNode;
    CJsonNode m_ErrNode;
    CJsonNode m_WarnNode;

    static CCommand HelpCommand();

    static void ExecExit(const TArguments& args, SInputOutput& io, void* data);
    static void ExecDel(const TArguments& args, SInputOutput& io, void* data);
    static void ExecVersion(const TArguments& args, SInputOutput& io, void* data);
    static void ExecWhatIs(const TArguments& args, SInputOutput& io, void* data);
    static void ExecEcho(const TArguments& args, SInputOutput& io, void* data);

    static TCommands Commands();
    static TCommands CallCommands();
    static TCommands NewCommands();
};

inline TObjectID CAutomationProc::AddObject(TAutomationObjectRef new_object)
{
    TObjectID new_object_id = m_ObjectByIndex.size();
    new_object->SetID(new_object_id);
    m_ObjectByIndex.push_back(new_object);
    return new_object_id;
}

inline TObjectID CAutomationProc::AddObject(TAutomationObjectRef new_object,
        const void* impl_ptr)
{
    m_ObjectByPointer[impl_ptr] = new_object;
    return AddObject(new_object);
}

template <class TDerived>
bool CAutomationObject::CheckCall(const string& name, SInputOutput& io, void*& data)
{
    _ASSERT(data);

    auto that = static_cast<CAutomationProc*>(data);
    auto& input = io.input;

    if (!input) {
        // TODO: throw "missing required id"
        cerr << "missing required id" << endl;
    }

    auto object_id = input.GetNode().AsInteger();
    auto& object_ref = that->ObjectIdToRef(object_id);

    if (name != object_ref->GetType()) return false;

    data = object_ref.GetPointer();
    ++input;
    return true;
}

template <class TDerived>
void CAutomationObject::ExecNew(const TArguments& args, SInputOutput& io, void* data)
{
    _ASSERT(data);

    auto that = static_cast<CAutomationProc*>(data);
    auto& reply = io.reply;

    CRef<CAutomationObject> new_object;

    try {
        new_object.Reset(TDerived::Create(args, that));
    }
    catch (CException& e) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Error in '" << TDerived::kName << "' constructor: " << e.GetMsg());
    }

    auto id = that->AddObject(new_object, new_object->GetImplPtr());
    reply.AppendInteger(id);
}

template <class TClass, void(TClass::*Method)(const TArguments&, SInputOutput& io)>
void CAutomationObject::ExecMethod(const TArguments& args, SInputOutput& io, void* data)
{
    _ASSERT(data);

    auto that = static_cast<TClass*>(data);
    (that->*Method)(args, io);
}

}

END_NCBI_SCOPE

#endif // AUTOMATION__HPP
