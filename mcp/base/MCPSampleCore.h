// MCP Server Sample Feature: MCPSampleCore.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPSampleCore.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// ENABLE: LINK MCPServerSample.cpp with MCPServer
//
// Flow:
//
//      1. MCPClient sends a MCP tool request.
//      2. Handle_tools_call dispatches the tool request.
//      3. A tool request handler can return an MCPOutput directly,
//         or request sampling from the client's LLM:
//              a. Derive a request instance of MCPSampleRequest
//              b. Send the request to RequestSample() and return
//                 the resulting MCPOutput from the tool handler:
//                      {
//                      MySample *Request = new MySample(idRequest,Arguments);
//                      MCPOutput = RequestSample(&Request);
//                      if (Request) delete Request;  // NULL on success.
//                      }
//              c. RequestSampling() builds the sampling/createMessage
//                 JSON-RPC request (use FormSamplingRequest() helper).
//                 MCPServer sends this to the client instead of a response.
//              d. Client sends LLM result back (has id, no method).
//                 MCPServer routes it to Handle_sampling_hook which
//                 finds the pending request by myId.
//              e. OnResponse() receives the LLM result, combines it
//                 with the original Arguments, and returns the final
//                 MCPOutput response to the original tools/call.
//
// Capability Check:
//
//      Call isSamplingSupported(MCPRequest) in MCPServer_OnInitialize()
//      to verify the client advertised "sampling":{} in its capabilities.
//
// Threading Notes:
//
//      1. MCPSampleRequest can be allocatd in any thread.
//      2. RequestSample() can be called in any thread.
//      3. OnResponse() will be handled in the main thread.
//      4. Once requested, MCPSampleRequest will be reaped in the main thread.
//
// Blocking Sampling Requests for Threads (MCPSyncSampler):
//
//      MCPSyncSampler provides a blocking sampling interface for use
//      by any worker thread -- it is not coupled to MCPAsyncWork. Any
//      threading model (MCPAsyncWork, std::thread, POSIX, etc.) can
//      embed a MCPSyncSampler and call BlockingSample().
//
//      Usage:
//              1. Scope an MCPSyncSampler instance in your working thread. 
//              2. Call BlockingSample(SystemPrompt, UserMessage, MaxTokens)
//                 -- this builds the sampling request, sends it to the
//                 client via PostMCPOutput(), and blocks until the
//                 client's LLM response arrives.
//              3. The returned MCPInputRequest* is yours to inspect
//                 and delete.
//
//      Cancel:
//              CancelWait() unblocks a pending BlockingSample() call.
//              The blocked thread receives NULL. MCPAsyncWork::AbortWork()
//              can call Sampler.CancelWait() to integrate with the
//              async cancellation path.
//

#ifndef __MCPServerSample_Core

        #define __MCPServerSample_Core

#include <ASThread.h>
#include <LinkList.h>

#include <MCPServerCore.h>

// *******************************
// **** isSamplingSupported() ****
// *******************************
                                  
bool isSamplingSupported();

// ***************************************************************************
// **** MCPServer Sample Flow Control ****************************************
// ***************************************************************************

struct MCPSampleRequest : public Link
        {
        JSON_Value_String myId;
        // -------------------- 
        // ---- Input Args ----
        // --------------------
        JSON_Value &Id;                 // Points to sId or nId.
        JSON_Object Arguments;          // Local copy.
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPSampleRequest(JSON_Value &inId, JSON_Object &inArgs); 
        virtual ~MCPSampleRequest() {}
        private:
                JSON_Value_String sId;  // Id local storage.
                JSON_Value_Number nId;
                JSON_Value_Null zId;
                bool GenerateMyId();
                friend class MCPSampleManager;
        protected:
                char* FormSamplingRequest(const char *SystemPrompt,
                                          const char *UserMessage,
                                          int MaxTokens);
                // -------------------------------
                // Note: Handlers return MCPOutput
                // -------------------------------
                virtual char* RequestSampling() = 0;      
                virtual char* OnResponse(MCPInputRequest &MCPRequest) = 0;
        };
                         
char* _RequestSample(MCPSampleRequest **Request);       // Return MCPOutput.

#define RequestSample(Request) _RequestSample((MCPSampleRequest**)Request)

// ***************************************************************************
// **** Blocking Sampler to be used by Threads *******************************
// ***************************************************************************

class MSSMessageQueue : public MessageQueue
        {
        protected:
                virtual void OnMessageDestruct(void *Msg);
        };

class MCPSyncSampler
        {
        private:
                MSSMessageQueue ResponseQ;
                struct MCPSyncSampleRequest *Work;
                friend struct MSSConnector;
                friend struct MCPSyncSampleRequest;
                // -----------------
                // Id local storage.
                // -----------------
                JSON_Value_String sId;  
                JSON_Value_Number nId;
                JSON_Value zId;         // Null Id.
        protected:
                JSON_Value &Id;         // Points to sId or nId.
                // --------------------------------------------------
                // ---- Overridable Factory for Sampling Request ----
                // --------------------------------------------------
                virtual char* FormSamplingRequest(JSON_Value &idRequest,
                                                  const char *SystemPrompt,
                                                  const char *UserMessage,
                                                  int MaxTokens);
        public:
                MCPSyncSampler(JSON_Value &inId);
                ~MCPSyncSampler(); 
                // ------------------------
                // ---- BlockingSample ----
                // ----------------------------------------------------
                // Output: Copy of MCPClient response (caller deletes).
                // ----------------------------------------------------
                MCPInputRequest* BlockingSample(const char *SystemPrompt,
                                                const char *UserMessage,
                                                int MaxTokens = 1024,
                                                unsigned TimeoutMS=0);
                virtual bool CancelWait();
        };

/* ***************************************************************************
 Notes:

        1. Check for Sampling Support:

           MCPServer_OnInitialize can check if the client supports sampling 
           and set a flag to enable sampling features. For example, an 
           MCP aspect can check for "sampling" in the client's "capabilities" 
           object:
           
                bool MCPServer_OnInitialize(MCPInputRequest &MCPRequest)
                        {
                        if (isSamplingSupported(MCPRequest))
                                {
                                // Note: Sampling requests can be made.
                                }
                        return true;
                        }

******************************************************************************
**** Example: helloWorld Sample **********************************************
*********************************

struct HelloWorldSample : public MCPSampleRequest
        {
        HelloWorldSample(JSON_Value &inId, JSON_Object &inArgs)
                :
                MCPSampleRequest(inId,inArgs)
                {
                return;
                }
        virtual char* RequestSampling();      
        virtual char* OnResponse(MCPInputRequest &MCPRequest);
        };

char* HelloWorldSample::RequestSampling()
        {
        const char *Name = Arguments.Find_member_string("name");
        MemoryPrintf Canvas;
        Canvas.printf("How do you say 'Hello %s' in French?",Name);
        return FormSamplingRequest(
                "You are a translator. Reply with only the translation.",
                Canvas.GetBuffer(),
                100);
        }

char* HelloWorldSample::OnResponse(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "HelloWorldSample::OnResponse";
        //
        // Sampling result per MCP spec:
        //   {"result":{"role":"assistant",
        //              "content":{"type":"text","text":"Bonjour Eric"},
        //              "model":"claude-3-sonnet",
        //              "stopReason":"endTurn"}}
        //
        JSON_Object *Result = MCPRequest.Get_result();
        if (!Result)
                {
                TERROR(("%s: No result in sampling response (id=%s)",
                        ProcName,id2string(Id)));
                return FormMCPResponse_ERROR(&Id,
                                             MCPErrorCode_Exception,
                                             "Sampling Failed",
                                             "No result in response");
                }
        const char *Translation = NULL;
        JSON_Object *Content = Result->Find_member_object("content");
        if (Content)
                {
                Translation = Content->Find_member_string("text");
                }
        if (!Translation)
                {
                TERROR(("%s: No text in sampling result (id=%s)",
                        ProcName,id2string(Id)));
                return FormMCPResponse_ERROR(&Id,
                                             MCPErrorCode_Exception,
                                             "Sampling Failed",
                                             "No text in result content");
                }
        const char *Name = Arguments.Find_member_string("name");
        MemoryPrintf Canvas;
        Canvas.printf("Hello %s! In French: %s",Name,Translation);
        char *Greeting = Canvas.AquireBuffer();
        char *MCPOutput = FormMCPResponse_helloWorld(Id,Greeting);
        if (Greeting) free(Greeting);
        return MCPOutput;
        }

char* MCPtool_helloWorld(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_helloWorld";
        char *MCPOutput = NULL;
        const char *Name = Arguments.Find_member_string("name");
        if (!Name)
                {
                TERROR(("%s: Missing Argument: 'name'",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Missing parameter: name");
                }
        else    {
                HelloWorldSample *Request = new HelloWorldSample(idRequest,
                                                                 Arguments);
                MCPOutput = RequestSample(&Request);
                if (Request) delete Request;
                }
        return MCPOutput;
        }

******************************************************************************
**** Example: Blocking Sample from Thread ************************************
**********************************************

struct MCPAsyncWork_withSampling : MCPAsyncWork
        {
        // ----------------
        // ---- Output ----
        // ----------------
        char *Greeting;
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPAsyncWork_withSampling(JSON_Value &inId, JSON_Object &inArgs)
                : MCPAsyncWork(inId,inArgs),
                  Sampler(inId)
                {
                Greeting = NULL;
                }
        virtual ~MCPAsyncWork_withSampling()
                {
                if (Greeting) free(Greeting);
                }
        protected:
                MCPSyncSampler Sampler;
                virtual bool AbortWork()                // Called to cancel.
                        {
                        Sampler.CancelWait();
                        return true;
                        }
                virtual bool BeginHandler()             // Called in thread.
                        {
                        const char *Name = Arguments.Find_member_string("name");
                        // -----------------------------------------
                        // ---- Send Request and Wait for Reply ----
                        // -----------------------------------------
                        MemoryPrintf Question;
                        Question.printf(
                                "How do you say 'Hello %s' in French?",Name);
                        MCPInputRequest *Response;
                        Response = Sampler.BlockingSample(
                                "You are a translator. "
                                "Reply with only the translation.",
                                Question.GetBuffer(),
                                100);
                        if (!Response) return false;
                        // ----------------------------------------
                        // ---- Extract Translation from Result ---
                        // ----------------------------------------
                        const char *Translation = NULL;
                        JSON_Object *Result = Response->Get_result();
                        if (Result)
                                {
                                JSON_Object *Content;
                                Content = Result->Find_member_object("content");
                                if (Content)
                                        {
                                        Translation = Content->
                                                Find_member_string("text");
                                        }
                                }
                        if (Translation)
                                {
                                MemoryPrintf Canvas;
                                Canvas.printf("Hello %s! In French: %s",
                                              Name,Translation);
                                Greeting = Canvas.AquireBuffer();
                                }
                        delete Response;
                        return Greeting != NULL;
                        }
                virtual char* CompleteHandler()          // Called in main.
                        {
                        return FormMCPResponse_helloWorld(get_Id(),Greeting);
                        }
        };

char* MCPtool_helloWorld(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_helloWorld";
        char *MCPOutput = NULL;
        const char *Name = Arguments.Find_member_string("name");
        if (!Name)
                {
                TERROR(("%s: Missing Argument: 'name'",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Missing parameter: name");
                }
        else    {
                MCPAsyncWork_withSampling *Work;
                Work = new MCPAsyncWork_withSampling(idRequest,Arguments);
                bool Status = Begin_AsyncWork(&Work);
                if (Status) MCPOutput = Return_MCPOutput_Pending();
                else    {
                        if (Work) delete Work;
                        MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                        MCPErrorCode_Exception,
                                        "helloWorld2 Failed",
                                        "Unable to start async work");
                        }
                }
        return MCPOutput;
        }

*****************************************************************************/


#endif
