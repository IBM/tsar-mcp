// ACPClient: ACPClient.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ACPClient.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// ACP Protocol <-> ACPClient mapping:
//
//      1. Initalize (ACPInitRequester)
//      2. Start Session (ACPSessionRequester)
//
//      Within a Session (ACPSession):
//
//              3. Issue Requests:
//
//                      ACPClientRequest (Asynchronous)
//                      ACPSyncRequester (Synchronous)
//                      ACPPromptRequester (Synchronous prompting)
//
//              4. Cancel Requests: CancelPrompt()
//
//      5. Close Session: ACPSessionCloser
//

#ifndef __ACP_Client_Main

        #define __ACP_Client_Main

#include <AutoString.h>
#include <MEMprintf.h>
#include <RPCClient.h>

#ifndef ACPProtocolVersion
        #define ACPProtocolVersion 1    // An integer, not a date string.
#endif

// ===========================================================================
// ==== ACP Client ===========================================================
// ===========================================================================

class ACPClientThread;

// ***************************************************************************
// **** ACPSession ***********************************************************
// *****************
//
// ACPSession mirrors a session in the server. 
//
// OPEN a session on the server with: ACPSssionRequester
//
//      Once the session is activated, all notification events
//      on behalf of the sessionId appear in OnSessionUpdate()
//
// SEQUENTIALLY perform WORK against a session:
//
//      ACPClientRequest - Asynchronous non-blocking requests.
//      ACPSyncRequester - Synchronous blocking requests.
//      ACPPromptRequester - Synchronous blocking sampling request.
//
// CLOSE the session on the server with: ACPSessionCloser.
//
// Note: A ACPSession can NOT be REUSED. The sessionId lives 
//       in the object forever. This prevents asynchronious 
//       CancelPrompt() calls from closing in unintended session. 
//
// WARNING: ACPClientThread (ACPSessionManager) must outlive all
//          ACPSessions constructed under it (otherwise assert will fire)!
//
// ---------------------------------------------------------------------------
//
// CANCEL an outstanding request with: CancelPrompt()
//
//      Threadsafe: Call from a thread that is not BlockingPrompt().
//
//      CancelPrompt() does NOT unblock BlockingPrompt(), but 
//      once the agent answers the outstanding prompt with stopReason 
//      "cancelled", BlockingPrompt() will return. 
//
//      RPCSyncRequester::CancelWait() is the opposite: it unblocks 
//      the caller and leaves the agent working.
//
// ***************************************************************************

class ACPSession : public Link
        {
        private:
                ACPClientThread &Client;        // Caller owns it.
                JSON_Value *sessionId;  // Points to sId or nId or zId.
                JSON_Value_String sId;
                JSON_Value_Number nId;
                JSON_Value_Null zId;
                void assign_SessionId(JSON_Value &inId);
                // ---------------------------
                // Declared to prevent copying
                // ---------------------------
                ACPSession(ACPSession &Source);
                ACPSession& operator =(ACPSession &Source);
                // -------------------------------------------------------
                // Receives notifications, dispatches to OnSessionUpdate()
                // -------------------------------------------------------
                bool OnNotify(RPCOutputResponse **RPCNotify);
                friend class ACPSessionManager;         // Dispatches here.
        protected:
                // ----------------------------------------
                // ---- OnSessionUpdate -------------------
                // ---------------------
                // Invoked for session/update. 
                // 
                // Chain back to the parent if not handled.
                // ----------------------------------------
                virtual bool OnSessionUpdate(const char *Kind,
                                             JSON_Object &Update);
                // --------------------------------------------------
                // Note: OnSessionUpdate() is already locked, but a 
                //       derived class should Lock() to touch its own
                //       state elsewhere if requests are outstanding.
                // --------------------------------------------------
                void Lock();
                void Unlock();
        public:
                ACPSession(ACPClientThread &inClient);
                ACPSession(ACPClientThread &inClient, JSON_Value &inId);
                ~ACPSession();
                ACPClientThread& get_Client() {return Client;}
                // ----------------------------
                // --- sessionId lifecycle ----
                // ----------------------------
                bool set_SessionId(JSON_Value &inId);
                JSON_Value& get_SessionId() {return *sessionId;}
                bool isSession() {return !sessionId->isNull();}
                // ----------------------
                // ---- CancelPrompt ----
                // ----------------------
                bool CancelPrompt();
        };

// ---------------------------
// ---- ACPSessionManager ----
// ---------------------------

DefineLinkList(ACPSessionList,ACPSession);

class ACPSessionManager                 // Session dispatcher.
        {
        private:
                Mutex SessionMutex;
                ACPSessionList Sessions;
                ACPSession* FindSession(RPCOutputResponse &Message);
        public:
                ~ACPSessionManager();
                void AddSession(ACPSession *Session);
                void RemoveSession(ACPSession *Session);
                bool HandleNotify(RPCOutputResponse **RPCNotify);
                void Take() {SessionMutex.Take();}
                void Release() {SessionMutex.Release();}
        };

// ***************************************************************************
// **** ACPClientThread ******************************************************
// *********************
//
//      Within a ACPClientThread multiple ACPSession can exist.
//      On behalf of a session, requests must run in sequence.
//
// ***************************************************************************

class ACPClientThread : public RPCClientThread
        {
        private:
                int ProtocolVersion;
                JSON_Object AgentInfo;
                ACPSessionManager SessionManager;
                friend class ACPSession;        // Registers itself.
        protected:
                // ------------------------------------------------
                // ---- OnRPCNotify -------------------------------
                // -----------------
                // Dispatches to SessionManager. A derived class
                // must chain forward if not handling an RPCNotify.
                // ------------------------------------------------
                virtual bool OnRPCNotify(RPCOutputResponse **RPCNotify);
        public:
                ACPClientThread(const char *Command)
                        :
                        RPCClientThread(Command)
                        {
                        ProtocolVersion = 0;    // Until initialize answers.
                        return;
                        }
                ~ACPClientThread() {Stop(true);}
                int get_ProtocolVersion() {return ProtocolVersion;}
                JSON_Object& get_AgentInfo() {return AgentInfo;}
                // -----------------------------------------
                // Negotiated values set by ACPInitRequester
                // -----------------------------------------
                void set_ProtocolVersion(int Version)
                        {
                        ProtocolVersion = Version;
                        }
                void set_AgentInfo(JSON_Object &Info) {AgentInfo = Info;}
        };

// ***************************************************************************
// **** ACPClientRequest *****************************************************
// ***********************
//
// Issue asynchronous requests to the ACPServer on behalf of a session.
//
//      Define: OnRequest()
//              OnResponse(RPCOutputResponse **RPCResponse)
//
// Carries the session so OnRequest() / OnResponse() can access it.
//
// Use RPCClientRequest for non-session requests.
//
// ***************************************************************************

struct ACPClientRequest : public RPCClientRequest
        {
        // -----------------
        // ---- Methods ----
        // -----------------
        ACPClientRequest(ACPSession &inSession) : Session(inSession) {}
        virtual ~ACPClientRequest() {}
        JSON_Value& get_SessionId() {return Session.get_SessionId();}
        bool isSession() {return Session.isSession();}
        private:
                ACPSession &Session;    // Caller owns it; outlives this.
        };

// ***************************************************************************
// **** Blocking ACPClient Requester *****************************************
// ***************************************************************************
//
// Issue Synchronous requests to the ACPServer on behalf of a session.
//
// Session updates appear in the ACPSession.
//
// Note: Use RPCSyncRequester for non-session requests.
//
// ***************************************************************************
          
class ACPSyncRequester : public RPCSyncRequester
        {
        private:
                ACPSession &Session;    // Caller owns it; outlives this.
        public:
                ACPSyncRequester(ACPSession &inSession)
                        :
                        RPCSyncRequester(inSession.get_Client()),
                        Session(inSession)
                        {
                        return;
                        }
                ACPSession& get_Session() {return Session;}
                JSON_Value& get_SessionId() {return Session.get_SessionId();}
                bool isSession() {return Session.isSession();}
        };

// ===========================================================================
// ==== ACP Requesters =======================================================
// ===========================================================================

// ---------------------------
// ---- ACPInitRequester -----
// ----------------------
//
// Initialize ACP Server Agent
//
// ---------------------------

class ACPInitRequester : public RPCSyncRequester
        {
        private:
                ACPClientThread &ACPClient;     // Reaches connection state.
        protected:
                virtual char* OnRequest(JSON_Value &idRequest);
        public:
                ACPInitRequester(ACPClientThread &Client)
                        :
                        RPCSyncRequester(Client),
                        ACPClient(Client)
                        {
                        return;
                        }
                // ----------------------------------------------------
                // Records protocolVersion and agentInfo on the Client,
                // and reports an agent that answered with a version we
                // did not ask for. Returns whether the agent answered.
                // ----------------------------------------------------
                bool BlockingInitialize(unsigned TimeoutMS=0);
        };

// ----------------------------------------------------------------
// ---- ACPSessionRequester ---------------------------------------
// -------------------------
//
// Activates 'inSession' such that ACPSession::isSession() is true.
//
// Parameters: cwd: Current working directory is a canonical
//                  path representing agent's workspace.
//
//             modeId: NULL -- Agent default
//                     "ask" -- Answers without taking action.
//
// Note: BlockingNewSession() will fail on an existing ACPSession.
//
// ----------------------------------------------------------------

class ACPSessionRequester : public ACPSyncRequester
        {
        private:
                AutoString Cwd;
                AutoString ModeId;
        protected:
                virtual char* OnRequest(JSON_Value &idRequest);
                virtual bool OnResponse(RPCOutputResponse **RPCResponse);
        public:
                ACPSessionRequester(ACPSession &inSession,
                                    const char *cwd,
                                    const char *modeId);
                bool BlockingNewSession(unsigned TimeoutMS=0);
        };

// ============================
// ==== ACPPromptRequester ====
// ============================

// --------------------------
// ---- ACPSessionSample ----   Session accumulates agent replies.
// --------------------------

class ACPSessionSample : public ACPSession
        {
        private:
                MemoryPrintf Reply;     // What the agent has said.
        protected:
                virtual bool OnSessionUpdate(const char *Kind,
                                             JSON_Object &Update);
        public:
                ACPSessionSample(ACPClientThread &inClient)
                        :
                        ACPSession(inClient)
                        {
                        return;
                        }
                ACPSessionSample(ACPClientThread &inClient, JSON_Value &inId)
                        :
                        ACPSession(inClient,inId)
                        {
                        return;
                        }
                void clear_Reply();             // Clears accumulator.
                const char* get_Reply();
        };

// -------------------------------------------------------------------------
// ---- ACPPromptRequester -------------------------------------------------
// ------------------------
//
// BlockingPrompt() -- Sends a text prompt request to the ACP Server.
//
//      Return: True : Answer completed with "end_turn".
//
//                     Sampling answer: ACPSessionSample::get_Reply()
//
//              False: Incomplete answer or error. Use:
//
//                      get_WaitResult() -- RCP Error or zero.
//
//                      get_ErrorReason() -- Text answer or "" (success).
//
//                         no_session:     Session never opened.
//                         not_sent:       See: get_WaitResult().
//                         timeout         TimeoutMS elapsed.
//                         wait_cancelled  CancelWait(): we stopped waiting.
//                         no_result       Answered but with no result.
//                         cancelled       CancelPrompt(): was issued.
//
// Note: To clear the sampling session use: ACPSessionSample::clear_Reply()
// 
// -------------------------------------------------------------------------

class ACPPromptRequester : public ACPSyncRequester
        {
        private:
                AutoString PromptText;
                AutoString ErrorReason;
        protected:
                virtual char* OnRequest(JSON_Value &idRequest);
        public:
                ACPPromptRequester(ACPSession &inSession)
                        :
                        ACPSyncRequester(inSession)
                        {
                        return;
                        }
                bool BlockingPrompt(const char *Text,
                                    unsigned TimeoutMS=0);
                const char* get_ErrorReason();
        };

// --------------------------------------------------------------------------
// ---- ACPSessionCloser ----------------------------------------------------
// ----------------------
//
// Closes the ACP server's session resource.
//
// Note: isSession() will still be true, but ACP server will reject requests.
// Note: BlockingClose() can safely be called twice.
// --------------------------------------------------------------------------

class ACPSessionCloser : public ACPSyncRequester
        {
        protected:
                virtual char* OnRequest(JSON_Value &idRequest);
        public:
                ACPSessionCloser(ACPSession &inSession)
                        :
                        ACPSyncRequester(inSession)
                        {
                        return;
                        }
                bool BlockingClose(unsigned TimeoutMS=0);
        };

/* **************************************************************************
 ****************************************************************************

 Notes:

 ****************************************************************************
 **** Example ***************************************************************
 *************

        Loads IBM Bob as an ACP Server, asks a 'Question'

 ****************************************************************************

#include <stdio.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>

#include <ACPClient.h> 

static const char *ACP_Workspace = "C:\\Temp";

// ****************
// **** main() ****
// ****************

static const char *ACPServerCMD =
        // -----------------------------------------------------------
        // Note: 'bob' is an npm shim: bob.cmd -> cmd.exe -> node.exe. 
        //       Spawning node directly so popenNT_getpid() is the 
        //       agent and pabortNT() can terminate it.
        // -----------------------------------------------------------
        "node "
        "\"C:\\Users\\Eric\\AppData\\Roaming\\npm\\node_modules"
                "\\@ibm\\bob\\dist\\bob.js\" "
        "acp --trust";

int main(int argc, const char *argv[])
        {
        static const char *ProcName = "main";
        const char *Question = "What is 17 multiplied by 23? "
                               "Answer with only the number.";
        // ********************
        // **** Initialize ****
        // ********************
        SetMaxTraceLevel(TRACELEVEL_DEBUG);
        InitJSONParser();
        // ***********************
        // **** ACPClient Run ****
        // ***********************
        bool Status = true;
        ACPClientThread Client(ACPServerCMD);
        Client.JSONTron(false);                  // Probe: dump every message.
        do      {
                Status = Client.Start();
                if (!Status)
                        {
                        TERROR(("%s: Can't start: %s",ProcName,ACPServerCMD));
                        break;
                        }
                // -------------------------------
                // ---- Initialize ACP Server ----
                // -------------------------------
                ACPInitRequester InitRequester(Client);
                Status = InitRequester.BlockingInitialize();
                if (!Status)
                        {
                        TERROR(("%s: BlockingInitialize() Failed",ProcName));
                        break;
                        }
                // ------------------------
                // ---- Open a Session ----
                // ------------------------
                ACPSessionSample Session(Client);
                ACPSessionRequester SessionRequester(Session,
                                                     ACP_Workspace,
                                                     "ask");
                Status = SessionRequester.BlockingNewSession();
                if (!Status)
                        {
                        TERROR(("%s: BlockingNewSession() Failed",ProcName));
                        break;
                        }
                // ----------------------------------------
                // ---- Sample Once -----------------------
                // -----------------
                // The requester tells the agent to answer;
                // the answer itself lands in the session.
                // ----------------------------------------
                ACPPromptRequester PromptRequester(Session);
                Session.clear_Reply();
                Status = PromptRequester.BlockingPrompt(Question);
                if (!Status)
                        {
                        TERROR(("%s: BlockingPrompt() Failed",ProcName));
                        break;
                        }
                TPRINT(("-----------------------------"));
                TPRINT(("%s",Session.get_Reply()));
                TPRINT(("-----------------------------"));
                // -------------------------
                // ---- Close a Session ----
                // -------------------------
                ACPSessionCloser SessionCloser(Session);
                Status = SessionCloser.BlockingClose();
                if (!Status)
                        {
                        TERROR(("%s: BlockingClose() Failed",ProcName));
                        break;
                        }
                } while (0);
        // *******************
        // **** Terminate ****
        // *******************
        Client.Stop(true);      // Join before DeinitJSONParser() resets.
        DeinitJSONParser();
        return Status ? 0 : 1;
        }

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __ACP_Client_Main */
