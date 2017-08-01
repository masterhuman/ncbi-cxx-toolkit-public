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

#include <ncbi_pch.hpp>

#include <sstream>

#include "nc_automation.hpp"
#include "ns_automation.hpp"
#include "wn_automation.hpp"
#include "nst_automation.hpp"

#include <connect/ncbi_pipe.hpp>
#include <connect/services/grid_app_version_info.hpp>

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define PROTOCOL_VERSION 4

#define AUTOMATION_IO_BUFFER_SIZE 4096

#define PROTOCOL_ERROR_BASE_RETCODE 20

USING_NCBI_SCOPE;

using namespace NAutomation;

struct NAutomation::SCommandImpl
{
    virtual ~SCommandImpl() {}
    virtual CJsonNode Help(const string& name, CJsonIterator& input) = 0;
    virtual bool Exec(const string& name, SInputOutput& io, void* data) = 0;
};

struct SSimpleCommandImpl : SCommandImpl
{
    SSimpleCommandImpl(TArgsInit args, TCommandExecutor exec = TCommandExecutor());
    CJsonNode Help(const string& name, CJsonIterator& input) override;
    bool Exec(const string& name, SInputOutput& io, void* data) override;

private:
    vector<CArgument> m_Args;
    TCommandExecutor m_Exec;
};

struct SVariadicCommandImpl : SCommandImpl
{
    SVariadicCommandImpl(string args, TCommandExecutor exec);
    CJsonNode Help(const string& name, CJsonIterator& input) override;
    bool Exec(const string& name, SInputOutput& io, void* data) override;

private:
    const string m_Args;
    TCommandExecutor m_Exec;
};

struct SCommandGroupImpl : SCommandImpl
{
    SCommandGroupImpl(TCommandGroup group);
    CJsonNode Help(const string& name, CJsonIterator& input) override;
    bool Exec(const string& name, SInputOutput& io, void* data) override;

private:
    TCommands m_Commands;
    TCommandExecutor m_Exec;
};

CJsonNode CArgument::Help()
{
    CJsonNode help(CJsonNode::eObject);
    help.SetString("name", m_Name);
    help.SetString("type", m_TypeOrDefaultValue.GetTypeName());
    if (m_Optional) help.SetByKey("default", m_TypeOrDefaultValue);
    return help;
}

void CArgument::Exec(CJsonIterator& input)
{
    if (input.IsValid()) {
        auto current = input.GetNode();

        if (current.GetNodeType() != m_TypeOrDefaultValue.GetNodeType()) {
            // TODO: throw "invalid type"
            cerr << "invalid type" << endl;
        }

        m_Value = current;

    } else {
        if (!m_Optional) {
            // TODO: throw "missing required arg"
            cerr << "missing required arg" << endl;
        }

        m_Value = m_TypeOrDefaultValue;
    }
}

SSimpleCommandImpl::SSimpleCommandImpl(TArgsInit args, TCommandExecutor exec) :
    m_Args(args),
    m_Exec(exec)
{
}

CJsonNode SSimpleCommandImpl::Help(const string& name, CJsonIterator&)
{
    // Full mode
    CJsonNode help(CJsonNode::eObject);
    help.SetString("command", name);

    if (m_Args.size()) {
        CJsonNode elements(CJsonNode::eArray);

        for (auto& element : m_Args) elements.Append(element.Help());

        help.SetByKey("arguments", elements);
    }

    return help;
}

bool SSimpleCommandImpl::Exec(const string& name, SInputOutput& io, void* data)
{
    auto& input = io.input;

    for (auto& element : m_Args) {
        element.Exec(input);
        if (input) ++input;
    }

    if (input) {
        // TODO: throw "too many parameters"
        cerr << "too many parameters" << endl;
    }

    if (m_Exec) m_Exec(m_Args, io, data);
    return true;
}

SVariadicCommandImpl::SVariadicCommandImpl(string args, TCommandExecutor exec) :
    m_Args(args),
    m_Exec(exec)
{
}

CJsonNode SVariadicCommandImpl::Help(const string& name, CJsonIterator&)
{
    // Full mode
    CJsonNode help(CJsonNode::eObject);
    help.SetString("command", name);
    help.SetString("arguments", m_Args);
    return help;
}

bool SVariadicCommandImpl::Exec(const string& name, SInputOutput& io, void* data)
{
    auto& input = io.input;
    TArguments args;

    for (; input; ++input) {
        args.emplace_back(input);
    }

    if (m_Exec) m_Exec(args, io, data);
    return true;
}

SCommandGroupImpl::SCommandGroupImpl(TCommandGroup group) :
    m_Commands(group.first()),
    m_Exec(group.second)
{
}

CJsonNode SCommandGroupImpl::Help(const string& name, CJsonIterator& input)
{
    // Full mode
    CJsonNode help(CJsonNode::eObject);
    help.SetString("command", name);

    if (m_Commands.size()) {
        CJsonNode elements(CJsonNode::eArray);

        for (auto& element : m_Commands) {
            if (CJsonNode sub_help = element.Help(input)) {

                // If help is called for a particular sub-command only
                if (sub_help.IsObject()) return sub_help;

                elements.Append(sub_help);
            }
        }

        help.SetByKey("sub-topics", elements);
    }

    return help;
}

bool SCommandGroupImpl::Exec(const string& name, SInputOutput& io, void* data)
{
    if (m_Commands.empty()) return false;

    if (m_Exec) m_Exec(TArguments(), io, data);

    for (auto& element : m_Commands) {
        if (element.Exec(io, data)) return true;
    }

    return false;
}

CCommand::CCommand(string name, TArgsInit args) :
    m_Name(name),
    m_Impl(new SSimpleCommandImpl(args))
{
}

CCommand::CCommand(string name, TCommandExecutor exec, TArgsInit args) :
    m_Name(name),
    m_Impl(new SSimpleCommandImpl(args, exec))
{
}

CCommand::CCommand(string name, TCommandExecutor exec, const char * const args) :
    m_Name(name),
    m_Impl(new SVariadicCommandImpl(args, exec))
{
}

CCommand::CCommand(string name, TCommandGroup group) :
    m_Name(name),
    m_Impl(new SCommandGroupImpl(group))
{
}

CJsonNode CCommand::Help(CJsonIterator& input)
{
    // Short mode (enumeration)
    if (!input.IsValid()) {
        return CJsonNode(m_Name);
    }

    // Help for a different element
    if (input.GetNode().AsString() != m_Name) {
        return CJsonNode();
    }

    return m_Impl->Help(m_Name, ++input);
}

bool CCommand::Exec(SInputOutput& io, void* data)
{
    auto& input = io.input;

    if (!input.IsValid()) {
        // TODO: throw "missing command name"
    }

    // Call to a different element
    if (input.GetNode().AsString() != m_Name) {
        return false;
    }

    ++input;
    m_Impl->Exec(m_Name, io, data);
    return true;
}

void CArgArray::Exception(const char* what)
{
    if (m_Location.empty()) {
        NCBI_THROW(CAutomationException, eInvalidInput, what);
    } else {
        NCBI_THROW_FMT(CAutomationException, eInvalidInput, m_Location <<
                ": " << what);
    }
}

TAutomationObjectRef CAutomationProc::FindObjectByPtr(const void* impl_ptr) const
{
    TPtrToObjectRefMap::const_iterator it = m_ObjectByPointer.find(impl_ptr);
    return it != m_ObjectByPointer.end() ? it->second : TAutomationObjectRef();
}

CJsonNode SServerAddressToJson::ExecOn(CNetServer server)
{
    switch (m_WhichPart) {
    case 1:
        return CJsonNode::NewStringNode(g_NetService_gethostnamebyaddr(
                server.GetHost()));
    case 2:
        return CJsonNode::NewIntegerNode(server.GetPort());
    }
    return CJsonNode::NewStringNode(server.GetServerAddress());
}

TCommands SNetServiceBase::CallCommands()
{
    TCommands cmds =
    {
        { "get_name", },
        { "get_address", {
                { "which_part", 0, }
            }},
// TODO: Remove this after GRID Python module stops using it
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        { "allow_xsite_connections", },
#endif
    };

    return cmds;
}

bool SNetServiceBase::Call(const string& method, SInputOutput& io)
{
    auto& arg_array = io.arg_array;
    auto& reply = io.reply;

    if (method == "get_name")
        reply.AppendString(m_Service.GetServiceName());
    else if (method == "get_address") {
        SServerAddressToJson server_address_proc(
                (int) arg_array.NextInteger(0));

        reply.Append(g_ExecToJson(server_address_proc,
                m_Service, m_ActualServiceType));
    } else
// TODO: Remove this after GRID Python module stops using it
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (method == "allow_xsite_connections")
            CNetService::AllowXSiteConnections();
        else
#endif
        return false;

    return true;
}

TCommands SNetService::CallCommands()
{
    TCommands cmds =
    {
        { "server_info", },
        { "exec", {
                { "command", CJsonNode::eString, },
                { "multiline", false, },
            }},
    };

    TCommands base_cmds = SNetServiceBase::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

bool SNetService::Call(const string& method, SInputOutput& io)
{
    auto& arg_array = io.arg_array;
    auto& reply = io.reply;
    if (method == "server_info")
        reply.Append(g_ServerInfoToJson(m_Service, m_ActualServiceType, true));
    else if (method == "exec") {
        string command(arg_array.NextString());
        reply.Append(g_ExecAnyCmdToJson(m_Service, m_ActualServiceType,
                command, arg_array.NextBoolean(false)));
    } else
        return SNetServiceBase::Call(method, io);

    return true;
}

CAutomationProc::CAutomationProc(IMessageSender* message_sender) :
    m_MessageSender(message_sender),
    m_OKNode(CJsonNode::NewBooleanNode(true)),
    m_ErrNode(CJsonNode::NewBooleanNode(false)),
    m_WarnNode(CJsonNode::NewStringNode("WARNING"))
{
}

TCommands CAutomationProc::CallCommands()
{
    return TCommands
    {
        SNetCacheBlob::CallCommand(),
        SNetCacheService::CallCommand(),
        SNetCacheServer::CallCommand(),
        SNetScheduleService::CallCommand(),
        SNetScheduleServer::CallCommand(),
        SNetStorageService::CallCommand(),
        SNetStorageServer::CallCommand(),
        SNetStorageObject::CallCommand(),
        SWorkerNode::CallCommand(),
    };
}

TCommands CAutomationProc::NewCommands()
{
    return TCommands
    {
        SNetCacheService::NewCommand(),
        SNetCacheServer::NewCommand(),
        SNetScheduleService::NewCommand(),
        SNetScheduleServer::NewCommand(),
        SNetStorageService::NewCommand(),
        SNetStorageServer::NewCommand(),
        SWorkerNode::NewCommand(),
    };
}

TCommands CAutomationProc::Commands()
{
    TCommands cmds =
    {
        { "exit", ExecExit },
        { "call", CallCommands },
        { "new", NewCommands },
        { "del", ExecDel, {
                { "object_id", CJsonNode::eInteger, },
            }},
        { "whatis", ExecWhatIs, {
                { "some_id", CJsonNode::eString, },
            }},
        { "echo", ExecEcho, "any" },
        { "version", ExecVersion },
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        { "allow_xsite_connections", ExecAllowXSite },
#endif
    };

    return cmds;
}

CCommand CAutomationProc::HelpCommand()
{
    return CCommand("help", CAutomationProc::Commands);
}

void CAutomationProc::ExecExit(const TArguments&, SInputOutput& io, void*)
{
    auto& reply = io.reply;
    reply = CJsonNode();
}

void CAutomationProc::ExecDel(const TArguments& args, SInputOutput& io, void* data)
{
    _ASSERT(args.size() == 1);
    _ASSERT(data);

    auto that = static_cast<CAutomationProc*>(data);
    TAutomationObjectRef& object(that->ObjectIdToRef(args[0].Value().AsInteger()));
    that->m_ObjectByPointer.erase(object->GetImplPtr());
    object = NULL;
}

void CAutomationProc::ExecVersion(const TArguments&, SInputOutput& io, void*)
{
    auto& reply = io.reply;
    reply.AppendString(GRID_APP_VERSION);
    reply.AppendString(__DATE__);
}

void CAutomationProc::ExecWhatIs(const TArguments& args, SInputOutput& io, void*)
{
    auto& reply = io.reply;
    const auto id = args[0].Value().AsString();
    auto result = g_WhatIs(id);

    if (!result) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError, "Unable to recognize the specified token.");
    }

    reply.Append(result);
}

void CAutomationProc::ExecEcho(const TArguments& args, SInputOutput& io, void*)
{
    auto& reply = io.reply;
    for (auto& arg: args) reply.Append(arg.Value());
}

void CAutomationProc::ExecAllowXSite(const TArguments&, SInputOutput&, void*)
{
    CNetService::AllowXSiteConnections();
}

CJsonNode CAutomationProc::ProcessMessage(const CJsonNode& message)
{
    SInputOutput io(message);
    auto& input = io.input;
    auto& arg_array = io.arg_array;
    auto& reply = io.reply;

    // Empty input (help)
    if (!input.IsValid()) {
        reply.AppendString("Type `help` (in single quotes) for usage. "
                "For help on a sub-topic, add it to the command, "
                "e.g. `help`, `call`, `nstobj`");
        return reply;
    }

    // Help
    if (CJsonNode help = HelpCommand().Help(input)) {
        return help;
    }

    reply.Append(m_OKNode);

    SCommandGroupImpl all_cmds(TCommandGroup(CAutomationProc::Commands, nullptr));

    string command(arg_array.NextString());
    arg_array.UpdateLocation(command);

    if (command == "call") {
        TAutomationObjectRef object_ref(ObjectIdToRef(
                (TObjectID) arg_array.NextInteger()));
        string method(arg_array.NextString());
        arg_array.UpdateLocation(method);
        if (!object_ref->Call(method, io)) {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "Unknown " << object_ref->GetType() <<
                            " method '" << method << "'");
        }
    } else if (!all_cmds.Exec("", io, this)) {
        arg_array.Exception("unknown command");
    }

    return reply;
}

void CAutomationProc::SendWarning(const string& warn_msg,
        TAutomationObjectRef source)
{
    CJsonNode warning(CJsonNode::NewArrayNode());
    warning.Append(m_WarnNode);
    warning.AppendString(warn_msg);
    warning.AppendString(source->GetType());
    warning.AppendInteger(source->GetID());
    m_MessageSender->SendMessage(warning);
}

void CAutomationProc::SendError(const CTempString& error_message)
{
    CJsonNode error(CJsonNode::NewArrayNode());
    error.Append(m_ErrNode);
    error.AppendString(error_message);
    m_MessageSender->SendMessage(error);
}

TAutomationObjectRef& CAutomationProc::ObjectIdToRef(TObjectID object_id)
{
    size_t index = (size_t) (object_id);

    if (index >= m_ObjectByIndex.size() || !m_ObjectByIndex[index]) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Object with id '" << object_id << "' does not exist");
    }
    return m_ObjectByIndex[index];
}

class CMessageSender : public IMessageSender
{
public:
    CMessageSender(CJsonOverUTTPWriter& json_writer, CPipe& pipe) :
        m_JSONWriter(json_writer),
        m_Pipe(pipe)
    {
    }

    virtual void SendMessage(const CJsonNode& message);

private:
    void SendOutputBuffer();

    CJsonOverUTTPWriter& m_JSONWriter;
    CPipe& m_Pipe;
};

void CMessageSender::SendMessage(const CJsonNode& message)
{
    if (!m_JSONWriter.WriteMessage(message))
        do
            SendOutputBuffer();
        while (!m_JSONWriter.CompleteMessage());

    SendOutputBuffer();
}

void CMessageSender::SendOutputBuffer()
{
    const char* output_buffer;
    size_t output_buffer_size;
    size_t bytes_written;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        while (output_buffer_size > 0) {
            if (m_Pipe.Write(output_buffer, output_buffer_size,
                    &bytes_written) != eIO_Success) {
                NCBI_THROW(CIOException, eWrite,
                    "Error while writing to the pipe");
            }
            output_buffer += bytes_written;
            output_buffer_size -= bytes_written;
        }
    } while (m_JSONWriter.NextOutputBuffer());
}

class CMessageDumperSender : public IMessageSender
{
public:
    CMessageDumperSender(IMessageSender* actual_sender,
            FILE* protocol_dump_file);

    void DumpInputMessage(const CJsonNode& message);

    virtual void SendMessage(const CJsonNode& message);

private:
    IMessageSender* m_ActualSender;

    FILE* m_ProtocolDumpFile;
    string m_ProtocolDumpTimeFormat;
    string m_DumpInputHeaderFormat;
    string m_DumpOutputHeaderFormat;
};

CMessageDumperSender::CMessageDumperSender(
        IMessageSender* actual_sender,
        FILE* protocol_dump_file) :
    m_ActualSender(actual_sender),
    m_ProtocolDumpFile(protocol_dump_file),
    m_ProtocolDumpTimeFormat("Y/M/D h:m:s.l")
{
    string pid_str(NStr::NumericToString((Int8) CProcess::GetCurrentPid()));

    m_DumpInputHeaderFormat.assign(1, '\n');
    m_DumpInputHeaderFormat.append(71 + pid_str.length(), '=');
    m_DumpInputHeaderFormat.append("\n----- IN ------ %s (pid: ");
    m_DumpInputHeaderFormat.append(pid_str);
    m_DumpInputHeaderFormat.append(") -----------------------\n");

    m_DumpOutputHeaderFormat.assign("----- OUT ----- %s (pid: ");
    m_DumpOutputHeaderFormat.append(pid_str);
    m_DumpOutputHeaderFormat.append(") -----------------------\n");
}

void CMessageDumperSender::DumpInputMessage(const CJsonNode& message)
{
    fprintf(m_ProtocolDumpFile, m_DumpInputHeaderFormat.c_str(),
            GetFastLocalTime().AsString(m_ProtocolDumpTimeFormat).c_str());
    g_PrintJSON(m_ProtocolDumpFile, message);
}

void CMessageDumperSender::SendMessage(const CJsonNode& message)
{
    fprintf(m_ProtocolDumpFile, m_DumpOutputHeaderFormat.c_str(),
            GetFastLocalTime().AsString(m_ProtocolDumpTimeFormat).c_str());
    g_PrintJSON(m_ProtocolDumpFile, message);

    if (m_ActualSender != NULL)
        m_ActualSender->SendMessage(message);
}

int CGridCommandLineInterfaceApp::Automation_PipeServer()
{
    CPipe pipe;

    pipe.OpenSelf();

#ifdef WIN32
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    char read_buf[AUTOMATION_IO_BUFFER_SIZE];
    char write_buf[AUTOMATION_IO_BUFFER_SIZE];

    CUTTPReader reader;
    CUTTPWriter writer;

    writer.Reset(write_buf, sizeof(write_buf));

    CJsonOverUTTPReader json_reader;
    CJsonOverUTTPWriter json_writer(writer);

    CMessageSender actual_message_sender(json_writer, pipe);

    IMessageSender* message_sender;

    auto_ptr<CMessageDumperSender> dumper_and_sender;

    if (!IsOptionSet(eProtocolDump))
        message_sender = &actual_message_sender;
    else {
        dumper_and_sender.reset(new CMessageDumperSender(&actual_message_sender,
                m_Opts.protocol_dump));
        message_sender = dumper_and_sender.get();
    }

    {
        CJsonNode greeting(CJsonNode::NewArrayNode());
        greeting.AppendString(GRID_APP_NAME);
        greeting.AppendInteger(PROTOCOL_VERSION);

        message_sender->SendMessage(greeting);
    }

    CAutomationProc proc(message_sender);

    size_t bytes_read;

    try {
        while (pipe.Read(read_buf,
                sizeof(read_buf), &bytes_read) == eIO_Success) {
            reader.SetNewBuffer(read_buf, bytes_read);

            while (json_reader.ReadMessage(reader)) {
                try {
                    if (dumper_and_sender.get() != NULL)
                        dumper_and_sender->DumpInputMessage(
                                json_reader.GetMessage());

                    CJsonNode reply(proc.ProcessMessage(
                            json_reader.GetMessage()));

                    if (!reply)
                        return 0;

                    message_sender->SendMessage(reply);
                }
                catch (CAutomationException& e) {
                    switch (e.GetErrCode()) {
                    case CAutomationException::eInvalidInput:
                        proc.SendError(e.GetMsg());
                        return 2;
                    default:
                        proc.SendError(e.GetMsg());
                    }
                }
                catch (CException& e) {
                    ostringstream os;
                    os << e.GetMsg() << ": ";
                    e.ReportExtra(os);
                    proc.SendError(os.str());
                }

                json_reader.Reset();
            }
        }
    }
    catch (CJsonOverUTTPException& e) {
        return PROTOCOL_ERROR_BASE_RETCODE + e.GetErrCode();
    }

    return 0;
}

#define DEBUG_CONSOLE_PROMPT ">>> "

int CGridCommandLineInterfaceApp::Automation_DebugConsole()
{
    printf("[" GRID_APP_NAME " automation debug console; "
            "protocol version " NCBI_AS_STRING(PROTOCOL_VERSION) "]\n"
            "Press ^D or type \"exit\" (in double qoutes) to quit.\n"
            "\n" DEBUG_CONSOLE_PROMPT);

    char read_buf[AUTOMATION_IO_BUFFER_SIZE];

    CMessageDumperSender dumper_and_sender(NULL, stdout);

    CAutomationProc proc(&dumper_and_sender);

    while (fgets(read_buf, sizeof(read_buf), stdin) != NULL) {
        try {
            CJsonNode input_message(CJsonNode::ParseArray(read_buf));

            try {
                dumper_and_sender.DumpInputMessage(input_message);

                CJsonNode reply(proc.ProcessMessage(input_message));

                if (!reply)
                    return 0;

                dumper_and_sender.SendMessage(reply);
            }
            catch (CException& e) {
                ostringstream os;
                os << e.GetMsg() << ": ";
                e.ReportExtra(os);
                proc.SendError(os.str());
            }
        }
        catch (CStringException& e) {
            printf("%*c syntax error\n", int(e.GetPos() +
                    sizeof(DEBUG_CONSOLE_PROMPT) - 1), int('^'));
        }
        printf(DEBUG_CONSOLE_PROMPT);
    }

    puts("");

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Automate()
{
    return IsOptionSet(eDebugConsole) ?
            Automation_DebugConsole() : Automation_PipeServer();
}
