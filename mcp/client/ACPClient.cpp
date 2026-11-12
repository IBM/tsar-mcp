// ACP Client: ACPClient.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ACPClient.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <AutoString.h>
#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <popenNT.h>

#include <ACPClient.h>

// ***************************************************************************
// **** ACPClientThread ******************************************************
// ***************************************************************************

bool ACPClientThread::OnRPCNotify(RPCOutputResponse **RPCNotify)
        {
        if (RPCClientThread::OnRPCNotify(RPCNotify)) return true;
        if (!RPCNotify || !*RPCNotify) return false;
        // ---------------------------------------
        // Session traffic belongs to the session.
        // ---------------------------------------
        return SessionManager.HandleNotify(RPCNotify);
        }

// ***************************************************************************
// **** ACPSession ***********************************************************
// ***************************************************************************

ACPSession::ACPSession(ACPClientThread &inClient)
        :
        Client(inClient)
        {
        assign_SessionId(zId);
        Client.SessionManager.AddSession(this);
        return;
        }

ACPSession::ACPSession(ACPClientThread &inClient, JSON_Value &inId)
        :
        Client(inClient)
        {
        assign_SessionId(inId);
        Client.SessionManager.AddSession(this);
        return;
        }

ACPSession::~ACPSession()
        {
        // -----------------------------------------------------------
        // RemoveSession() takes SessionMutex, which the Client thread
        // holds for the whole of a dispatch (HandleNotify); therefore
        // blocks until Notify messages are handled.
        // -----------------------------------------------------------
        Client.SessionManager.RemoveSession(this);
        return;
        }

// ***************************************************************************
// **** session/cancel *******************************************************
// ***************************************************************************

static char* FormACPNotification_cancel(JSON_Value &SessionId)
        {
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "method": "session/cancel",
          "params": { "sessionId": "9677..." }
        }
        No id -- a notification is never answered.
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"method\":\"session/cancel\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"sessionId\":%s",id2string(SessionId));
        Canvas.printf("}");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

bool ACPSession::CancelPrompt()         // Threadsafe.
        {
        static const char *ProcName = "ACPSession::CancelPrompt";
        // --------------------------------------------------
        // Session is unlocked, because the id cannot change.
        // --------------------------------------------------
        if (!isSession())
                {
                TERROR(("%s: No Session to cancel",ProcName));
                return false;
                }
        char *ACPOutput = FormACPNotification_cancel(get_SessionId());
        if (!ACPOutput)
                {
                TERROR(("%s: FormACPNotification_cancel() Failed",ProcName));
                return false;
                }
        TINFO(("%s: Cancelling Session %s",ProcName,
                                           id2string(get_SessionId())));
        bool Status = Client.PostXCPOutput(&ACPOutput); // Asynchronous.
        if (!Status)
                {
                TERROR(("%s: PostXCPOutput() Failed",ProcName));
                FreeXCPRequestBuffer(&ACPOutput);
                }
        return Status;
        }

bool ACPSession::OnNotify(RPCOutputResponse **RPCNotify)
        {
        static const char *ProcName = "ACPSession::OnNotify";
        JSON_Object *Params = (*RPCNotify)->Get_params();
        JSON_Object *Update = Params ? Params->Find_member_object("update")
                                     : NULL;
        const char *Kind = Update
                         ? Update->Find_member_string("sessionUpdate")
                         : NULL;
        if (!Kind)
                {
                TERROR(("%s: session/update with no sessionUpdate",ProcName));
                return false;
                }
        return OnSessionUpdate(Kind,*Update);
        }

bool ACPSession::OnSessionUpdate(const char *Kind, JSON_Object &Update)
        {
        static const char *ProcName = "ACPSession::OnSessionUpdate";
        if (strcmp(Kind,"available_commands_update") == 0)
                {
                JSON_Value_Array *List;
                List = Update.Find_member_array("availableCommands");
                TDEBUG(("%s: %u Commands for Session %s",ProcName,
                                        List ? List->nValues : 0,
                                        id2string(get_SessionId())));
                return true;
                }
        if (strcmp(Kind,"session_info_update") == 0)
                {
                TDEBUG(("%s: %s for Session %s",ProcName,
                                                Kind,
                                                id2string(get_SessionId())));
                return true;
                }
        return false;   // Not ours -- let OnXCPResponse ring.
        }

void ACPSession::assign_SessionId(JSON_Value &inId)
        {
        if (inId.isString()) 
                {
                sId = (JSON_Value_String &)inId;
                sessionId = &sId;
                }
        else if (inId.isNumber()) 
                {
                nId = (JSON_Value_Number &)inId;
                sessionId = &nId;
                }
        else sessionId = &zId;
        return;
        }

bool ACPSession::set_SessionId(JSON_Value &inId)
        {
        static const char *ProcName = "ACPSession::set_SessionId";
        if (isSession())
                {
                TERROR(("%s: Session %s is already open",ProcName,
                                                id2string(get_SessionId())));
                return false;
                }
        assign_SessionId(inId);
        if (!isSession())
                {
                TERROR(("%s: sessionId is not a String or Number",ProcName));
                return false;
                }
        return true;
        }

void ACPSession::Lock()
        {
        Client.SessionManager.Take();
        return;
        }

void ACPSession::Unlock()
        {
        Client.SessionManager.Release();
        return;
        }

// ***************************************************************************
// **** ACPSessionManager ****************************************************
// ***********************
//
// Routes by params.sessionId. Shaped like RPCRequestManager, with its
// ownership inverted at every point:
//
//      owns nothing     the caller owns every ACPSession, so
//                       AutoCleanup stays off and Remove never deletes
//      leaves on ask    a session goes in its own destructor, not
//                       because a response arrived
//      joins silently   a session sends no message to register
//
// Which inverts the locking too. FindRequest() may drop RequestMutex
// before the handler runs because requests only ever leave that list
// on the Client thread -- the same thread doing the finding. Sessions
// leave on the caller's thread, so SessionMutex must be HELD across
// the dispatch: it is what keeps the session alive while the Client
// thread is inside it. A borrowed ACPSession* is stale the moment the
// lock drops, which is why nothing hands one out.
//
// Nothing reached from a response takes SessionMutex, and nothing
// reached from a dispatch takes Connector. Keep it that way: the two
// locks never nest, so there is no order to get wrong.
//
// ***************************************************************************

// ----------------------------------------------
// ---- ACPIsSessionMatch -----------------------
// -----------------------
// Whether OnNotify() messages match the session.
// ----------------------------------------------

static bool ACPIsSessionMatch(JSON_Value &SessionId,
                              RPCOutputResponse &Message)
        {
        static const char *ProcName = "ACPIsSessionMatch";
        if (!SessionId.isString() && !SessionId.isNumber()) return false;
        JSON_Object *Params = Message.Get_params();
        JSON_Value *NotifySession = Params
                                  ? Params->FindMemberValue("sessionId")
                                  : NULL;
        if (!NotifySession) return false;
        if (!NotifySession->isString() && !NotifySession->isNumber())
                {
                TERROR(("%s: sessionId is not a String or Number",ProcName));
                return false;
                }
        return idcmp(SessionId,*NotifySession);
        }

// ---------------------------
// ---- ACPSessionManager ----
// ---------------------------

ACPSessionManager::~ACPSessionManager()
        {
        static const char *ProcName = "~ACPSessionManager";
        SessionMutex.Take();
        ACPSession *Session = Sessions.GetFirst();
        SessionMutex.Release();
        if (Session)
                {
                // ------------------------------------------
                // Sessions will deregister upon destruction;
                // therefore, the manager must exist.
                // ------------------------------------------
                TERROR(("%s: Sessions outliving the Client!",ProcName));
                assert(0);
                }
        return;
        }

void ACPSessionManager::AddSession(ACPSession *Session)
        {
        SessionMutex.Take();
        Sessions.AddBottom(Session);
        SessionMutex.Release();
        return;
        }

void ACPSessionManager::RemoveSession(ACPSession *Session)
        {
        SessionMutex.Take();
        Sessions.Remove(Session);
        SessionMutex.Release();
        return;
        }

ACPSession* ACPSessionManager::FindSession(RPCOutputResponse &Message)
        {
        // ---------------------------------
        // Take SessionMutex before calling!
        // ---------------------------------
        ACPSession *Current = Sessions.GetFirst();
        while (Current)
                {
                if (ACPIsSessionMatch(Current->get_SessionId(),Message))
                        {
                        break;
                        }
                Current = Sessions.GetNext(Current);
                }
        return Current;
        }

bool ACPSessionManager::HandleNotify(RPCOutputResponse **RPCNotify)
        {
        if (!RPCNotify || !*RPCNotify) return false;
        // ----------------------------------------------------
        // Held across the dispatch. The lock keeps the session 
        // alive while we are inside it.
        // ----------------------------------------------------
        SessionMutex.Take();
        ACPSession *Session = FindSession(**RPCNotify);
        bool Handled = Session ? Session->OnNotify(RPCNotify) : false;
        SessionMutex.Release();
        return Handled;
        }

// ***************************************************************************
// **** initialize ***********************************************************
// ***************************************************************************

static char* FormACPRequest_initialize(JSON_Value &idRequest)
        {
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 1,
          "method": "initialize",
          "params": {
            "protocolVersion": 1,
            "clientCapabilities": { "fs": {...} }
          }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"method\":\"initialize\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"protocolVersion\":%d,",ACPProtocolVersion);
        Canvas.printf("\"clientCapabilities\":{");
        Canvas.printf("\"fs\":{\"readTextFile\":false,"
                             "\"writeTextFile\":false}");
        Canvas.printf("}");
        Canvas.printf("}");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

char* ACPInitRequester::OnRequest(JSON_Value &idRequest)
        {
        return FormACPRequest_initialize(idRequest);
        }

bool ACPInitRequester::BlockingInitialize(unsigned TimeoutMS)
        {
        static const char *ProcName = "ACPInitRequester::BlockingInitialize";
        RPCOutputResponse *ACPResponse;
        ACPResponse = RPCSyncRequester::BlockingRequest(TimeoutMS);
        if (!ACPResponse)
                {
                TERROR(("%s: No initialize Response",ProcName));
                return false;
                }
        bool Status = false;
        JSON_Object *Result = ACPResponse->Get_result();
        if (!Result)
                {
                TERROR(("%s: initialize Response has no result",ProcName));
                TraceRPCError(ProcName,*ACPResponse);
                }
        else    {
                // ---------------------------------------------
                // ---- Record what the agent agreed to --------
                // -----------------------------------
                // protocolVersion is an integer here, not an
                // MCP date.
                // ---------------------------------------------
                JSON_Value *Version =
                                Result->FindMemberValue("protocolVersion");
                if (Version && Version->isNumber())
                        {
                        JSON_Value_Number *Number;
                        Number = (JSON_Value_Number *)Version;
                        ACPClient.set_ProtocolVersion(Number->int_number);
                        // ---------------------------------------------
                        // Nothing below ACPProtocolVersion exists, so a
                        // different answer is the agent disagreeing --
                        // and this is where that decision will live if
                        // a second ACP version ever ships.
                        // ---------------------------------------------
                        if (Number->int_number != ACPProtocolVersion)
                                {
                                TERROR(("%s: Agent answered "
                                        "protocolVersion %d, asked %d",
                                        ProcName,
                                        Number->int_number,
                                        ACPProtocolVersion));
                                }
                        }
                else TERROR(("%s: No integer protocolVersion",ProcName));
                JSON_Object *AgentInfo =
                                Result->Find_member_object("agentInfo");
                if (AgentInfo) ACPClient.set_AgentInfo(*AgentInfo);
                const char *Name = AgentInfo
                                 ? AgentInfo->Find_member_string("name")
                                 : NULL;
                const char *AgentVersion = AgentInfo
                                 ? AgentInfo->Find_member_string("version")
                                 : NULL;
                TINFO(("%s: Agent: name=%s, Version=%s",ProcName,
                                                Name ? Name : "?",
                                                AgentVersion ? AgentVersion
                                                             : "?"));
                TINFO(("%s: Negotiated protocolVersion=%d",ProcName,
                                        ACPClient.get_ProtocolVersion()));
                Status = true;
                }
        delete ACPResponse;
        return Status;
        }

// ***************************************************************************
// **** session/new **********************************************************
// ***************************************************************************

static char* FormACPRequest_newSession(JSON_Value &idRequest,
                                       const char *Cwd,
                                       const char *ModeId)
        {
        static const char *ProcName = "FormACPRequest_newSession";
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 2,
          "method": "session/new",
          "params": {
            "cwd": "C:\\Temp\\Bob",
            "mcpServers": [],
            "_meta": { "bob/mode": "ask" }
          }
        }
        ----------------------------------------------------------- */
        char *EscCwd = EscapeJSONString(Cwd ? Cwd : "");
        if (!EscCwd)
                {
                TERROR(("%s: EscapeJSONString() Failed",ProcName));
                return NULL;
                }
        char *EscMode = NULL;
        if (ModeId && *ModeId)
                {
                EscMode = EscapeJSONString(ModeId);
                if (!EscMode)
                        {
                        TERROR(("%s: EscapeJSONString() Failed",ProcName));
                        FreeEscapeJSONString(&EscCwd);
                        return NULL;
                        }
                }
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"method\":\"session/new\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"cwd\":\"%s\",",EscCwd);
        // ---------------------------------------------------------------
        // Empty: the agent's own stdio MCP servers are unaffected, and
        // only HTTP servers may be injected here anyway.
        // ---------------------------------------------------------------
        Canvas.printf("\"mcpServers\":[]");
        if (EscMode)
                {
                Canvas.printf(",\"_meta\":{\"bob/mode\":\"%s\"}",EscMode);
                }
        Canvas.printf("}");
        Canvas.printf("}");
        char *ACPOutput = Canvas.AquireBuffer();
        FreeEscapeJSONString(&EscCwd);
        FreeEscapeJSONString(&EscMode);
        return ACPOutput;
        }

ACPSessionRequester::ACPSessionRequester(ACPSession &inSession,
                                         const char *cwd,
                                         const char *modeId)
        :
        ACPSyncRequester(inSession)
        {
        Cwd = cwd;
        ModeId = modeId;
        return;
        }

char* ACPSessionRequester::OnRequest(JSON_Value &idRequest)
        {
        return FormACPRequest_newSession(idRequest,
                                         Cwd.GetString(),
                                         ModeId.GetString());
        }

bool ACPSessionRequester::OnResponse(RPCOutputResponse **RPCResponse)
        {
        static const char *ProcName = "ACPSessionRequester::OnResponse";
        // --------------------------------------------------------
        // Activates the session by setting the session's sessionId 
        // so it's descoverable by FindSession()
        // --------------------------------------------------------
        JSON_Object *Result = (RPCResponse && *RPCResponse)
                            ? (*RPCResponse)->Get_result()
                            : NULL;
        JSON_Value *NewSession = Result
                               ? Result->FindMemberValue("sessionId")
                               : NULL;
        if (NewSession && get_Session().set_SessionId(*NewSession))
                {
                TDEBUG(("%s: Session %s is open",ProcName,
                                                 id2string(get_SessionId())));
                }
        // ---------------------------------------------------------------
        // Chained LAST: the base hands the response to the waiter, and
        // the waiter may be running before it returns.
        // ---------------------------------------------------------------
        return ACPSyncRequester::OnResponse(RPCResponse);
        }

bool ACPSessionRequester::BlockingNewSession(unsigned TimeoutMS)
        {
        static const char *ProcName = "ACPSessionRequester::BlockingNewSession";
        if (isSession())
                {
                TERROR(("%s: Session %s is already open",ProcName,
                                                id2string(get_SessionId())));
                return false;
                }
        RPCOutputResponse *ACPResponse;
        ACPResponse = ACPSyncRequester::BlockingRequest(TimeoutMS);
        if (!ACPResponse)
                {
                TERROR(("%s: No session/new Response",ProcName));
                return false;
                }
        // -----------------------------------
        // ---- Verify Session Activation ----
        // -----------------------------------
        bool Status = isSession();      // OnResponse() activated the session.
        if (!Status)
                {
                TERROR(("%s: session/new Response has no sessionId",ProcName));
                TraceRPCError(ProcName,*ACPResponse);
                }
        else    {
                TINFO(("%s: sessionId=%s",ProcName,
                                          id2string(get_SessionId())));
                }
        delete ACPResponse;
        return Status;
        }

// ***************************************************************************
// **** session/prompt *******************************************************
// ***************************************************************************

static char* FormACPRequest_prompt(JSON_Value &idRequest,
                                   JSON_Value &SessionId,
                                   const char *Text)
        {
        static const char *ProcName = "FormACPRequest_prompt";
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "session/prompt",
          "params": {
            "sessionId": "9677...",
            "prompt": [ { "type": "text", "text": "..." } ]
          }
        }
        ----------------------------------------------------------- */
        char *EscText = EscapeJSONString(Text ? Text : "");
        if (!EscText)
                {
                TERROR(("%s: EscapeJSONString() Failed",ProcName));
                return NULL;
                }
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"method\":\"session/prompt\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"sessionId\":%s,",id2string(SessionId));
        Canvas.printf("\"prompt\":[{\"type\":\"text\",\"text\":\"%s\"}]",
                      EscText);
        Canvas.printf("}");
        Canvas.printf("}");
        char *ACPOutput = Canvas.AquireBuffer();
        FreeEscapeJSONString(&EscText);
        return ACPOutput;
        }

char* ACPPromptRequester::OnRequest(JSON_Value &idRequest)
        {
        return FormACPRequest_prompt(idRequest,
                                     get_SessionId(),
                                     PromptText.GetString());
        }

bool ACPPromptRequester::BlockingPrompt(const char *Text,
                                        unsigned TimeoutMS)
        {
        static const char *ProcName = "ACPPromptRequester::BlockingPrompt";
        ErrorReason = "";
        if (!isSession())
                {
                ErrorReason = "no_session";
                TERROR(("%s: No Session -- session/new Failed",ProcName));
                return false;
                }
        PromptText = Text;
        RPCOutputResponse *ACPResponse;
        ACPResponse = ACPSyncRequester::BlockingRequest(TimeoutMS);
        if (!ACPResponse)
                {
                // -------------------------
                // Decipher no answer error:
                // -------------------------
                switch (get_WaitResult())       // Ask RPC layer.
                        {
                        case RPCWait_Timeout:
                                ErrorReason = "timeout";
                                break;
                        case RPCWait_Cancelled:
                                ErrorReason = "wait_cancelled";
                                break;
                        default:
                                ErrorReason = "not_sent";
                                break;
                        }
                TERROR(("%s: No session/prompt Response: %s",ProcName,
                                                      get_ErrorReason()));
                return false;
                }
        bool Status = false;
        JSON_Object *Result = ACPResponse->Get_result();
        if (!Result)
                {
                ErrorReason = "no_result";
                TERROR(("%s: session/prompt Response has no result",ProcName));
                TraceRPCError(ProcName,*ACPResponse);
                }
        else    {
                const char *StopReason =
                                Result->Find_member_string("stopReason");
                if (!StopReason) StopReason = "no_stopReason";
                // -------------------------------------------------------
                // Only end_turn is a complete answer. Everything else is
                // the agent's word for why not, passed through as it
                // came -- the set is the agent's, not ours.
                // -------------------------------------------------------
                Status = (strcmp(StopReason,"end_turn") == 0);
                if (!Status) ErrorReason = StopReason;
                TINFO(("%s: stopReason=%s",ProcName,StopReason));
                }
        delete ACPResponse;
        return Status;
        }

const char* ACPPromptRequester::get_ErrorReason()
        {
        const char *Reason = ErrorReason.GetString();
        return Reason ? Reason : "";
        }

// ***************************************************************************
// **** session/close ********************************************************
// ***************************************************************************

static char* FormACPRequest_closeSession(JSON_Value &idRequest,
                                         JSON_Value &SessionId)
        {
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 4,
          "method": "session/close",
          "params": { "sessionId": "9677..." }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"method\":\"session/close\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"sessionId\":%s",id2string(SessionId));
        Canvas.printf("}");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

char* ACPSessionCloser::OnRequest(JSON_Value &idRequest)
        {
        return FormACPRequest_closeSession(idRequest,get_SessionId());
        }

bool ACPSessionCloser::BlockingClose(unsigned TimeoutMS)
        {
        static const char *ProcName = "ACPSessionCloser::BlockingClose";
        if (!isSession())
                {
                TERROR(("%s: No Session to close",ProcName));
                return false;
                }
        RPCOutputResponse *ACPResponse;
        ACPResponse = ACPSyncRequester::BlockingRequest(TimeoutMS);
        if (!ACPResponse)
                {
                TERROR(("%s: No session/close Response",ProcName));
                return false;
                }
        // ---------------------------------------------------------------
        // Read here, on the caller's thread: the session keeps its id
        // either way, so there is nothing for the Client thread to
        // latch before the next inbound line is parsed. result is {}.
        // ---------------------------------------------------------------
        bool Status = (ACPResponse->Get_error() == NULL);
        if (!Status)
                {
                TraceRPCError(ProcName,*ACPResponse);
                }
        else    {
                TINFO(("%s: Session %s is closed",ProcName,
                                                id2string(get_SessionId())));
                }
        delete ACPResponse;
        return Status;
        }

// ***************************************************************************
// **** ACPSessionSample *****************************************************
// ***************************************************************************

bool ACPSessionSample::OnSessionUpdate(const char *Kind, JSON_Object &Update)
        {
        static const char *ProcName = "ACPSessionSample::OnSessionUpdate";
        if (strcmp(Kind,"agent_message_chunk") == 0)
                {
                JSON_Object *Content = Update.Find_member_object("content");
                const char *Text = Content
                                 ? Content->Find_member_string("text")
                                 : NULL;
                if (Text)
                        {
                        char *Raw = UnescapeJSONString(Text);
                        if (!Raw)
                                {
                                TERROR(("%s: UnescapeJSONString() Failed",
                                        ProcName));
                                }
                        else    {
                                Reply.printf("%s",Raw);
                                FreeUnescapeJSONString(&Raw);
                                }
                        }
                return true;
                }
        return ACPSession::OnSessionUpdate(Kind,Update);
        }

void ACPSessionSample::clear_Reply()
        {
        // ---------------------------------------------------------------
        // The Client thread holds this lock for the whole of a dispatch,
        // so this waits for any notification already inside this session
        // rather than emptying the buffer underneath an append.
        // ---------------------------------------------------------------
        Lock();
        Reply.Clear();
        Unlock();
        return;
        }

const char* ACPSessionSample::get_Reply()
        {
        const char *Text = Reply.GetBuffer();
        return Text ? Text : "";
        }


// ***************************************************************************
// ******************************* End of File *******************************
// ***************************************************************************
