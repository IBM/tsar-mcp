// RPCClient: RPCClient.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: RPCClient.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// JSON-RPC 2.0 over the XCP transport; sensitive to JSON-RPC 2.0 'id'.
//
// Provides blocking requesting calls for synchronous interfacing.
//

#ifndef __RPC_Client_Main

        #define __RPC_Client_Main

#include <XCPClient.h>
#include <RPCClientJSON.h>

#ifndef RPCErrorCode_NoMethod
        #define RPCErrorCode_NoMethod -32601    // JSON-RPC 2.0 standard.
#endif

// ***************************************************************************
// **** TraceRPCError ********************************************************
// *******************
//
//      Returns 'false' if 'Response' contains no error.
//
// ***************************************************************************

bool TraceRPCError(const char *ProcName, RPCOutputResponse &Response);

// ***************************************************************************
// **** RPCClientRequest *****************************************************
// ***************************************************************************

struct RPCClientRequest : public Link
        {
        // -----------------
        // ---- Methods ----
        // -----------------
        RPCClientRequest() {Id = &zId;}
        virtual ~RPCClientRequest() {}
        JSON_Value& get_Id() {return *Id;}
        private:
                // -----------------
                // Id local storage.
                // -----------------
                JSON_Value *Id;         // Points to sId or nId or zId.
                JSON_Value_String sId;
                JSON_Value_Number nId;
                JSON_Value_Null zId;
                friend class RPCClientThread;
                friend class RPCRequestManager;
        protected:
                // ---------------------------------------
                // ---- Request Identity -----------------
                // ----------------------
                // Must be set before OnRequest() returns.
                // ---------------------------------------
                void set_Id(JSON_Value &inId);
                void set_Id(const char *inId);
                void set_Id(unsigned long inId);
                // -------------------------------
                // Note: Handlers return RPCOutput
                // -------------------------------
                virtual char* OnRequest() = 0;      
                virtual bool OnResponse(RPCOutputResponse **RPCResponse) = 0;
        };

// *******************************************************
// **** RPCRequestManager ********************************
// ***********************
//
// An index of outstanding requests, keyed by JSON-RPC id.
//
// *******************************************************

DefineLinkList(RPCRequestList,RPCClientRequest);

class RPCRequestManager
        {
        private:
                Mutex RequestMutex;
                RPCRequestList Requests;
        protected:
                void AddRequest(RPCClientRequest *Request);
                void DeleteRequest(RPCClientRequest **Request);
                RPCClientRequest* FindRequest(JSON_Value &idResponse);
        public:
                RPCRequestManager();
                ~RPCRequestManager() {Clear();}
                char* BeginRequest(RPCClientRequest **Request);
                void Clear();
                bool HandleResponse(RPCOutputResponse **RPCResponse);
        };

// ***************************************************************************
// **** RPCClientThread ******************************************************
// ***************************************************************************

// --------------------------------
// ---- (SyncRequester) Wireup ----
// --------------------------------

class RPCSyncRequester;
struct RPCSyncRequest;

struct RPCConnector
        {
        Mutex WiringMutex;
        void Wireup(RPCSyncRequester &SyncRequester, RPCSyncRequest &Work);
        void Unwire(RPCSyncRequester &SyncRequester);
        void Unwire(RPCSyncRequest &Work);
        void Take() {WiringMutex.Take();}
        void Release() {WiringMutex.Release();}
        };

// *************************
// **** RPCClientThread ****
// *************************

class RPCClientThread : public XCPClientThread
        {
        private:
                CriticalSection nextIdCS;
                unsigned nextId;
                bool JSONTrace;
                friend class RPCSyncRequester;
                friend struct RPCSyncRequest;
        protected:
                RPCConnector Connector;
                RPCRequestManager RequestManager;
                virtual bool OnShutdown();
                // ------------------------------
                // ---- XCP Message Handlers ----
                // ------------------------------
                virtual bool OnReference(void *UserPtr);
                virtual void OnReferenceDestruct(void *UserPtr);
                virtual bool OnXCPRequest(const char *XCPRequest);
                virtual bool OnXCPResponse(const char *XCPResponse);
                // -----------------------------------------------------
                // ---- JSON-RPC Message Handlers ----------------------
                // -------------------------------
                // Return whether handled (true).
                //
                //   OnRPCNotify  The base claims nothing and says so:
                //                a notification carries no id, and an
                //                id is the only key this layer knows.
                //                A protocol routes them on its own --
                //                ACP on params.sessionId. Chain anyway,
                //                so an override stays correct if that
                //                ever changes.
                //   OnRPCRequest chain LAST, so the base declines only
                //                what nothing handled.
                // -----------------------------------------------------
                virtual bool OnRPCNotify(RPCOutputResponse **RPCNotify);
                virtual bool OnRPCRequest(RPCInputRequest &RPCRequest);
                virtual bool OnRPCResponse(RPCOutputResponse **RPCResponse);
        public:
                RPCClientThread(const char *Command);
                ~RPCClientThread() {Stop(true);}
                bool BeginClientRequest(RPCClientRequest **Request);
                unsigned get_nextId();
                bool JSONTron(bool On);
        };

#define BeginRPCClientRequest(Client,Request)                           \
        (Client).BeginClientRequest((RPCClientRequest**)(Request))

// ***************************************************************************
// **** Blocking RPCClient Requester *****************************************
// ***************************************************************************

enum RPCWaitResult              // BlockingRequest() error reasons.
        {
        RPCWait_Answered = 0,
        RPCWait_NotSent,        // Never reached the server.
        RPCWait_Timeout,        // TimeoutMS elapsed with nothing.
        RPCWait_Cancelled       // CancelWait(): we stopped waiting.
        };
          
class RPCMessageQueue : public MessageQueue
        {
        protected:
                virtual void OnMessageDestruct(void *Msg);
        };

class RPCSyncRequester
        {
        private:
                RPCClientThread &RPCClient;
                RPCMessageQueue ResponseQ;
                struct RPCSyncRequest *Work;
                RPCWaitResult WaitResult;
                friend struct RPCConnector;
                friend struct RPCSyncRequest;
        protected:
                // ------------------------------------
                // ---- Parent RPCClient Interface ----
                // ------------------------------------
                long getID();                           // Server PID.
                bool PostXCPOutput(char **XCPOutput);   // Uncorrelated send.
                // ------------------------------------------------
                // ---- Overridable Factory for Client Request ----
                // ----------------------------------------------
                // Do not block: runs in the Client message loop.
                // ------------------------------------------------
                virtual char* OnRequest(JSON_Value &idRequest) = 0;
                // ---------------------------------------------
                // ---- OnResponse -----------------------------
                // ----------------
                // Chain if not handled. Aquire 'RPCResponse' by
                // setting 'RPCResponse' to NULL upon return.
                // ---------------------------------------------
                virtual bool OnResponse(RPCOutputResponse **RPCResponse);
        public:
                RPCSyncRequester(RPCClientThread &Client);
                ~RPCSyncRequester();
                // -------------------------
                // ---- BlockingRequest ----
                // --------------------------------------------
                // Output: RPCClient response (caller deletes).
                // --------------------------------------------
                RPCOutputResponse* BlockingRequest(unsigned TimeoutMS=0);
                // -------------------------------------------------------
                // Why the last BlockingRequest() gave back nothing.
                // Only meaningful when it returned NULL.
                // -------------------------------------------------------
                RPCWaitResult get_WaitResult() {return WaitResult;}
                virtual bool CancelWait();
        };

/* **************************************************************************
 ****************************************************************************

 Notes:

        Layering:

                XCPClientThread   lines to and from a spawned process
                RPCClientThread   JSON-RPC 2.0: ids, correlation, requesters
                MCP / ACP         protocol meaning -- peers, not derived
                                  from one another

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __RPC_Client_Main */
