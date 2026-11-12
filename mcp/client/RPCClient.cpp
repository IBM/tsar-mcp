// RPC Client: RPCClient.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: RPCClient.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//

#include <assert.h>
#include <stdio.h>

#include <AutoString.h>
#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>

#include <RPCClient.h>

// ***************************************************************************
// **** RPCClientRequest *****************************************************
// ***************************************************************************

void RPCClientRequest::set_Id(JSON_Value &inId)
        {
        if (inId.isString()) 
                {
                sId = (JSON_Value_String &)inId;
                Id = &sId;
                }
        else if (inId.isNumber()) 
                {
                nId = (JSON_Value_Number &)inId;
                Id = &nId;
                }
        else Id = &zId;
        return;
        }

void RPCClientRequest::set_Id(const char *inId)
        {
        if (inId)
                {
                sId.SetString(inId,strlen(inId));
                Id = &sId;
                }
        else Id = &zId;
        return;        
        }

void RPCClientRequest::set_Id(unsigned long inId)
        {
        nId.long_number = (longlong_t)inId;
        nId.double_number = (double)inId;
        nId.int_number = (int)inId;
        // ---------------------------------------------------------------
        // dec2_number is value*100 and only meaningful while it fits: a
        // wrapped product would be read back as a bogus "cents" value.
        // ---------------------------------------------------------------
        nId.dec2_number = (inId <= (unsigned long)(0x7FFFFFFF/100))
                        ? (int)(inId * 100)
                        : 0;
        Id = &nId;
        return;
        }

// ***************************************************************************
// **** TraceRPCError ********************************************************
// ***************************************************************************

bool TraceRPCError(const char *ProcName, RPCOutputResponse &Response)
        {
        JSON_Object *Error = Response.Get_error();
        if (!Error) return false;
        JSON_Value_Number *Code = Error->Find_member_number("code");
        const char *Message = Error->Find_member_string("message");
        TERROR(("%s: Error %d: %s",ProcName,
                                   Code ? Code->int_number : 0,
                                   Message ? Message : "?"));
        return true;
        }

// ***************************************************************************
// **** RPCRequestManager ****************************************************
// ***************************************************************************

RPCRequestManager::RPCRequestManager()
        {
        Requests.SetAutoCleanup(true);
        return;
        }

void RPCRequestManager::AddRequest(RPCClientRequest *Request)
        {
        RequestMutex.Take();
        Requests.AddBottom(Request);
        RequestMutex.Release();
        return;
        }
               
void RPCRequestManager::DeleteRequest(RPCClientRequest **Request)
        {
        if (Request && *Request)
                {
                RequestMutex.Take();
                Requests.Remove(*Request);
                RequestMutex.Release();
                delete *Request;
                *Request = NULL;
                }
        return;
        }

RPCClientRequest* RPCRequestManager::FindRequest(JSON_Value &idResponse)
        {
        RequestMutex.Take();
        RPCClientRequest *Current = Requests.GetFirst();
        while (Current)
                {
                if (idcmp(Current->get_Id(),idResponse)) break;
                Current = Requests.GetNext(Current);
                }
        RequestMutex.Release();
        return Current;
        }

void RPCRequestManager::Clear() 
        {
        static const char *ProcName = "RPCRequestManager::Clear";
        RequestMutex.Take();
        RPCClientRequest *Request = Requests.GetFirst();
        if (Request)
                {
                TERROR(("%s: Outstanding Requests!",ProcName));
                }
        while (Request)
                {
                Requests.Remove(Request);
                RequestMutex.Release();
                delete Request;
                RequestMutex.Take();
                Request = Requests.GetFirst();
                } 
        RequestMutex.Release();
        return;
        }

char* RPCRequestManager::BeginRequest(RPCClientRequest **Request)
        {
        static const char *ProcName = "RPCRequestManager::BeginRequest";
        char *RPCOutput = NULL;
        if (!Request || !*Request)
                {
                TERROR(("%s: No Request",ProcName));
                return NULL;
                }
        RPCOutput = (*Request)->OnRequest();
        if (!RPCOutput)
                {
                TERROR(("%s: No Client Request Made",ProcName));
                }
        else    {
                JSON_Value &requestId = (*Request)->get_Id();
                TDEBUG(("%s: Starting Client Request: %s",
                        ProcName,
                        id2string(requestId)));
                AddRequest(*Request);
                *Request = NULL;
                }
        return RPCOutput;
        }

bool RPCRequestManager::HandleResponse(RPCOutputResponse **RPCResponse)
        {
        static const char *ProcName = "RPCRequestManager::HandleResponse";
        if (!RPCResponse || !*RPCResponse)
                {
                TERROR(("%s: No RPCResponse",ProcName));
                return false;
                }
        bool Status = false;
        JSON_Value *idResponse = (*RPCResponse)->Get_id();
        if (!idResponse)
                {
                TERROR(("%s: No Id in Request",ProcName));
                }
        else    {
                RPCClientRequest *Request = FindRequest(*idResponse);
                if (!Request)
                        {
                        TDEBUG(("%s: Handler not found: %s",
                                ProcName,
                                id2string(idResponse)));
                        return false;    // No handler handled by caller.
                        }
                Status = Request->OnResponse(RPCResponse);
                if (!Status)
                        {
                        TERROR(("%s: OnResponse returned failed (id=%s)",
                                ProcName,
                                id2string(idResponse)));
                        }
                DeleteRequest(&Request);
                }
        return Status;
        }

// ***************************************************************************
// **** Form JSON-RPC Error Response *****************************************
// ***************************************************************************

static char* FormRPCResponse_NoMethod(JSON_Value &id, const char *Method)
        {
        static const char *ProcName = "FormRPCResponse_NoMethod";
        char *EscMethod = EscapeJSONString(Method ? Method : "");
        if (!EscMethod)
                {
                TERROR(("%s: EscapeJSONString() Failed",ProcName));
                return NULL;
                }
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(id));
        Canvas.printf("\"error\":{");
        Canvas.printf("\"code\":%d,",RPCErrorCode_NoMethod);
        Canvas.printf("\"message\":\"Method not found\",");
        Canvas.printf("\"data\":\"%s\"",EscMethod);
        Canvas.printf("}");
        Canvas.printf("}");
        char *RPCOutput = Canvas.AquireBuffer();
        FreeEscapeJSONString(&EscMethod);
        return RPCOutput;
        }

// ***************************************************************************
// **** RPCClientThread ******************************************************
// ***************************************************************************

RPCClientThread::RPCClientThread(const char *Command) 
        :
        XCPClientThread(Command)
        {
        JSONTrace = false;
        nextId = 0;
        return;
        }

bool RPCClientThread::BeginClientRequest(RPCClientRequest **Request)
        {
        static const char *ProcName = "RPCClientThread::BeginClientRequest";
        bool Status = PostReference((void **)Request);
        if (!Status)
                {
                TERROR(("%s: PostReference() Failed",ProcName));
                }
        return Status;
        }

bool RPCClientThread::JSONTron(bool On)
        {
        bool Before = JSONTrace;
        JSONTrace = On;
        return Before;
        }

unsigned RPCClientThread::get_nextId()
        {
        nextIdCS.Take();
        unsigned Id = ++nextId;
        nextIdCS.Release();
        return Id;
        }

bool RPCClientThread::OnReference(void *UserPtr)
        {
        static const char *ProcName = "RPCClientThread::OnReference";
        RPCClientRequest *ClientRequest = (RPCClientRequest *)UserPtr;
        if (ClientRequest) 
                {
                ClientRequest->set_Id(get_nextId());
                char *RPCOutput = RequestManager.BeginRequest(&ClientRequest);
                if (!RPCOutput)
                        {
                        TERROR(("%s: RequestManager.BeginRequest() Failed",
                                ProcName));
                        }
                else    {
                        bool Status = OnXCPRequest(RPCOutput);
                        if (!Status)
                                {
                                TERROR(("%s: OnXCPRequest Failed",ProcName));
                                // DeleteRequest() probably not necessary. 
                                }
                        FreeXCPRequestBuffer(&RPCOutput);
                        }
                if (ClientRequest) delete ClientRequest;
                }
        return true;
        }

void RPCClientThread::OnReferenceDestruct(void *UserPtr)
        {
        static const char *ProcName = "RPCClientThread::OnReferenceDestruct";
        RPCClientRequest *ClientRequest = (RPCClientRequest *)UserPtr;
        if (ClientRequest) 
                {
                TDEBUG(("%s: Reaping unsent Client Request",ProcName));
                delete ClientRequest;
                }
        return;
        }

bool RPCClientThread::OnXCPRequest(const char *XCPRequest)
        {
        static const char *ProcName = "RPCClientThread::OnXCPRequest";
        // --------------------------------------------------------
        // Every outbound message lands here -- queued through
        // PostXCPOutput() or handed over directly by OnReference()
        // and OnRPCRequest().
        // --------------------------------------------------------
        if (JSONTrace) 
                {
                RPCInputRequest *RPCRequest = ParseJSON_RPCInput(XCPRequest);
                if (!RPCRequest)
                        {
                        TERROR(("%s: Parse JSON RPC Request Failed",ProcName));
                        TERROR(("%s: Request:",ProcName));
                        TPRINT(("-----------------------------"));
                        TPRINT(("%s",XCPRequest));
                        TPRINT(("-----------------------------"));
                        return false;
                        }
                TPRINT(("------------------------------"));
                TINFO(("%s: Tracing Request:",ProcName));
                TraceJSON(*RPCRequest,true);
                delete RPCRequest;
                }
        return XCPClientThread::OnXCPRequest(XCPRequest);
        }

bool RPCClientThread::OnXCPResponse(const char *XCPResponse)
        {
        static const char *ProcName = "RPCClientThread::OnXCPResponse";
        RPCOutputResponse *RPCResponse = ParseJSON_RPCOutput(XCPResponse);
        if (!RPCResponse)
                {
                TERROR(("%s: Unable to parse JSON RPC Response",ProcName));
                TERROR(("%s: Response:",ProcName));
                TPRINT(("-----------------------------"));
                TPRINT(("%s",XCPResponse));
                TPRINT(("-----------------------------"));
                return false;
                }
        if (JSONTrace)
                {
                TPRINT(("------------------------------"));
                TINFO(("%s: Tracing Response:",ProcName));
                TraceJSON(*RPCResponse,true);
                }
        // ---------------------------------------------------------------
        // Three shapes arrive on the wire. RPCOutputResponse derives from
        // RPCInputRequest, so all three are read from this one parse:
        //
        //       method &&  id  -- server->client request: MUST be answered
        //       method && !id  -- notification:           MUST NOT be answered
        //      !method &&  id  -- response:               correlate by id
        // ---------------------------------------------------------------
        const char *Method = RPCResponse->Get_method();
        JSON_Value *idResponse = RPCResponse->Get_id();
        if (Method && idResponse) OnRPCRequest(*RPCResponse);
        else if (Method)
                {
                bool Handled = OnRPCNotify(&RPCResponse);
                // -------------------------------------------------------
                // Logged here, not in OnRPCNotify(), so the whole
                // override chain has had its chance first.
                //
                // Method points INTO RPCResponse. A handler may take the
                // object (set it NULL) and still report not-handled, and
                // its new owner may already have deleted it on another
                // thread -- the name is readable only while it is ours.
                // -------------------------------------------------------
                if (!Handled && RPCResponse)
                        {
                        TINFO(("%s: Unhandled Notification: %s",
                               ProcName,
                               Method));
                        }
                }
        else if (idResponse) OnRPCResponse(&RPCResponse);
        else    {
                TERROR(("%s: Neither method nor id -- discarded",ProcName));
                TPRINT(("-----------------------------"));
                TPRINT(("%s",XCPResponse));
                TPRINT(("-----------------------------"));
                }
        if (RPCResponse) delete RPCResponse;
        return true;
        }

bool RPCClientThread::OnRPCNotify(RPCOutputResponse** /*RPCNotify*/)
        {
        return false;
        }

bool RPCClientThread::OnRPCRequest(RPCInputRequest &RPCRequest)
        {
        static const char *ProcName = "RPCClientThread::OnRPCRequest";
        const char *Method = RPCRequest.Get_method();
        JSON_Value *idRequest = RPCRequest.Get_id();
        if (!idRequest)
                {
                TERROR(("%s: No Id in Server Request",ProcName));
                return false;
                }
        // --------------------------------------------------------
        // Server is waiting, must return an error if nothing else.
        // --------------------------------------------------------
        TINFO(("%s: Declining Server Request: %s (id=%s)",
               ProcName,
               Method ? Method : "",
               id2string(idRequest)));
        char *RPCOutput = FormRPCResponse_NoMethod(*idRequest,Method);
        if (!RPCOutput)
                {
                TERROR(("%s: FormRPCResponse_NoMethod() Failed",ProcName));
                return false;
                }
        bool Status = OnXCPRequest(RPCOutput);
        if (!Status)
                {
                TERROR(("%s: OnXCPRequest Failed",ProcName));
                }
        FreeXCPRequestBuffer(&RPCOutput);
        return Status;
        }

bool RPCClientThread::OnRPCResponse(RPCOutputResponse **RPCResponse)
        {
        static const char *ProcName = "RPCClientThread::OnRPCResponse";
        if (!RPCResponse || !*RPCResponse)
                {
                TERROR(("%s: No RPCResponse",ProcName));
                return false;
                }
        bool Status = RequestManager.HandleResponse(RPCResponse);
        if (!Status)
                {
                TDEBUG(("%s: HandleResponse() -- Not handled",ProcName));
                }
        return Status;
        }

bool RPCClientThread::OnShutdown()
        {
        bool rc = XCPClientThread::OnShutdown();
        RequestManager.Clear();
        return rc;
        }

// ***************************************************************************
// **** Blocking RPCClient Requester *****************************************
// ***************************************************************************
//
// Implementation Notes:
//
//      RPCSyncRequester and RPCSyncRequest hold cross-references
//      to each other (Requester->Work, Work->SyncRequester). Both sides
//      can be destroyed independently, in any thread. RPCConnector
//      uses a single shared mutex (WiringMutex) to serialize all
//      Wireup/Unwire operations.
//
// Note: Connector is never taken under RequestMutex. FindRequest()
//       compares ids and releases; every handler that reaches a
//       requester runs after it.
//
// ***************************************************************************

// ------------------------
// ---- RPCSyncRequest ----
// ------------------------

struct RPCSyncRequest : RPCClientRequest
        {
        RPCClientThread &RPCClient;
        RPCSyncRequester *SyncRequester;
        // -----------------
        // ---- Methods ----
        // -----------------
        RPCSyncRequest(RPCClientThread &Client);
        virtual ~RPCSyncRequest();
        protected:
                // -------------------------------
                // Note: Handlers return RPCOutput
                // -------------------------------
                virtual char* OnRequest();
                virtual bool OnResponse(RPCOutputResponse **RPCResponse);
        };

// --------------------------------------------------------------------
// ---- Wireup --------------------------------------------------------
// ------------
// Note: One mutex manages delete of RPCSyncRequester or
//       RPCSyncRequest in any order. It is anticipated that both
//       are deleted in the Client thread, but it is not a requirement.
// --------------------------------------------------------------------

void RPCConnector::Wireup(RPCSyncRequester &SyncRequester, 
                          RPCSyncRequest &Work)
        {
        Take();
        SyncRequester.Work = &Work;
        Work.SyncRequester = &SyncRequester;
        Release();
        return;
        }
         
void RPCConnector::Unwire(RPCSyncRequester &SyncRequester)
        {
        Take();
        if (SyncRequester.Work)
                {
                assert(SyncRequester.Work->SyncRequester == &SyncRequester);
                SyncRequester.Work->SyncRequester = NULL;
                SyncRequester.Work = NULL;
                }
        Release();
        return;
        }

void RPCConnector::Unwire(RPCSyncRequest &Work)
        {
        Take();
        if (Work.SyncRequester)
                {
                assert(Work.SyncRequester->Work == &Work);
                Work.SyncRequester->Work = NULL;
                Work.SyncRequester = NULL;
                }
        Release();
        return;
        }

// -------------------------
// ---- RPCMessageQueue ----
// -------------------------

static char RPCSyncRequestCANCEL[] = "RPCSyncRequestCANCEL"; 

void RPCMessageQueue::OnMessageDestruct(void *Msg)
        {
        static const char *ProcName = "RPCMessageQueue::OnMessageDestruct";
        if (Msg == RPCSyncRequestCANCEL)
                {
                TDEBUG(("%s: Unhandled CANCEL Signal",ProcName));
                }
        else    {
                RPCOutputResponse *outResponse = (RPCOutputResponse *)Msg;
                TINFO(("%s: Unhandled Client Response: RPCOutputResponse",
                       ProcName));
                delete outResponse;
                }
        return;
        }

// --------------------------
// ---- RPCSyncRequester ----
// --------------------------
         
RPCSyncRequester::RPCSyncRequester(RPCClientThread &Client) 
        :
        RPCClient(Client)
        {
        Work = NULL;
        WaitResult = RPCWait_NotSent;
        return;
        }

RPCSyncRequester::~RPCSyncRequester()
        {
        RPCClient.Connector.Unwire(*this);
        return;
        }

RPCOutputResponse* RPCSyncRequester::BlockingRequest(unsigned TimeoutMS)
        {
        static const char *ProcName = "RPCSyncRequester::BlockingRequest";
        WaitResult = RPCWait_NotSent;   // Until something reaches the wire.
        if (!RPCClient.IsStarted())     // Once started, a BlockingRequest()
                {                       // won't block even if server exits.
                TERROR(("%s: Server is not running",ProcName));
                return NULL;
                }
        long ServerPID = RPCClient.getID();     // Valid: Start() waited.
        RPCSyncRequest *SyncWork = new RPCSyncRequest(RPCClient);
        if (!SyncWork)
                {
                TERROR(("%s: Can't allocate RPCSyncRequest",ProcName));
                return NULL;
                }
        RPCClient.Connector.Wireup(*this,*SyncWork);
        // ---------------------------------
        // ---- Initiate Client Request ----
        // ---------------------------------
        bool Status = BeginRPCClientRequest(RPCClient,&SyncWork);
        if (!Status || SyncWork)
                {
                TERROR(("%s: Server [%ld] BeginClientRequest Failed",
                        ProcName,
                        ServerPID));
                RPCClient.Connector.Unwire(*this); // Disassociate Work item.
                if (SyncWork) 
                        {
                        delete SyncWork;    
                        SyncWork = NULL;
                        }
                return NULL;
                }
        // ---------------------------------------------
        // ---- Get the Response from the RPCClient ----
        // ---------------------------------------------
        void *Msg = NULL;
        if (TimeoutMS)
                {
                TINFO(("%s: Waiting %u mSeconds for Server [%ld] Response",
                       ProcName,
                       TimeoutMS,
                       ServerPID));
                Msg = ResponseQ.Receive(TimeoutMS);
                }
        else    {
                TDEBUG(("%s: Waiting indefinitely for Server [%ld] Response", 
                        ProcName,
                        ServerPID));
                Msg = ResponseQ.Receive();
                }
        RPCClient.Connector.Unwire(*this);      // Done: Disassociate Work item.
        if (!Msg)
                {
                WaitResult = RPCWait_Timeout;
                TERROR(("%s: Server [%ld] ResponseQ.Receive() Timeout",
                        ProcName,
                        ServerPID));
                return NULL;
                }
        if (Msg == RPCSyncRequestCANCEL)
                {
                WaitResult = RPCWait_Cancelled;
                TINFO(("%s: Server [%ld] Request Wait Cancelled",
                       ProcName,
                       ServerPID));
                return NULL;
                }
        WaitResult = RPCWait_Answered;
        RPCOutputResponse *outResponse = (RPCOutputResponse *)Msg;
        JSON_Value *ResponseId = outResponse->Get_id();
        TINFO(("%s: Received Server [%ld] Response: %s",
               ProcName,
               ServerPID,
               id2string(ResponseId)));
        return outResponse;
        }

bool RPCSyncRequester::CancelWait()
        {
        static const char *ProcName = "RPCSyncRequester::CancelWait";
        TINFO(("%s: CancelWait() Server [%ld] Request",ProcName,
                                                       RPCClient.getID()));
        return ResponseQ.SendPriority(RPCSyncRequestCANCEL);
        }

bool RPCSyncRequester::OnResponse(RPCOutputResponse **RPCResponse)
        {
        static const char *ProcName = "RPCSyncRequester::OnResponse";
        if (!RPCResponse || !*RPCResponse)
                {
                // An override chained after taking the response. The
                // waiter has nothing to receive and must not be left
                // waiting on it.
                TERROR(("%s: No Response to deliver",ProcName));
                return false;
                }
        void *Msg = *RPCResponse;
        bool Sent = ResponseQ.Send(Msg);
        if (Sent) *RPCResponse = NULL;  // The waiter owns it now.
        return Sent;
        }

// --------------------------
// Parent RPCClient Interface
// --------------------------

long RPCSyncRequester::getID()
        {
        return RPCClient.getID();
        }

bool RPCSyncRequester::PostXCPOutput(char **XCPOutput)
        {
        return RPCClient.PostXCPOutput(XCPOutput);
        }

// ------------------------
// ---- RPCSyncRequest ----
// ------------------------

RPCSyncRequest::RPCSyncRequest(RPCClientThread &Client)
        :
        RPCClient(Client)
        {
        SyncRequester = NULL;
        return;
        }

RPCSyncRequest::~RPCSyncRequest() 
        {
        static const char *ProcName = "RPCSyncRequest::~RPCSyncRequest";
        bool RequestPending = false;
        RPCClient.Connector.Take();
        if (SyncRequester)
                {
                RequestPending = true;
                SyncRequester->CancelWait();            // Must be pending.
                }
        RPCClient.Connector.Release();
        RPCClient.Connector.Unwire(*this);
        if (RequestPending)
                {
                TERROR(("%s: Request Pending: %s",ProcName,
                                                  id2string(get_Id())));
                }
        else    {
                TDEBUG(("%s: Request completed: %s",ProcName,
                                                    id2string(get_Id())));
                }
        return;
        }

bool RPCSyncRequest::OnResponse(RPCOutputResponse **RPCResponse)
        {
        static const char *ProcName = "RPCSyncRequest::OnResponse";
        if (!RPCResponse || !*RPCResponse)
                {
                TERROR(("%s: No Response (%s)",ProcName,id2string(get_Id())));
                return false;
                }
        TINFO(("%s: Handling: %s",ProcName,id2string(get_Id())));
        bool SentStatus = false;
        RPCClient.Connector.Take();
        if (SyncRequester) 
                {
                // The queue is the Requester's own: this asks, the
                // same way ~RPCSyncRequest asks through CancelWait().
                SentStatus = SyncRequester->OnResponse(RPCResponse);
                }
        RPCClient.Connector.Release();
        if (SentStatus)
                {
                RPCClient.Connector.Unwire(*this);
                TINFO(("%s: Unblocked Request Wait (%s)",
                       ProcName,
                       id2string(get_Id())));
                }
        return true;
        }

char* RPCSyncRequest::OnRequest()
        {
        static const char *ProcName = "RPCSyncRequest::OnRequest";
        char *RPCOutput = NULL;
        RPCClient.Connector.Take();
        if (SyncRequester) 
                {
                RPCOutput = SyncRequester->OnRequest(get_Id());
                }
        RPCClient.Connector.Release();
        if (!RPCOutput)
                {
                TERROR(("%s: SyncRequester->OnRequest(%s) Failed",
                        ProcName,
                        id2string(get_Id())));
                }
        return RPCOutput;
        }

// ***************************************************************************
// ******************************* End of File *******************************
// ***************************************************************************
