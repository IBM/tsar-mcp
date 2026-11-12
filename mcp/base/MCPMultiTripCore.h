// MCP Multi Round-Trip Requests: MCPMultiTripCore.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPMultiTripCore.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// ENABLE: LINK MCPServerMRTR.cpp with MCPServer
//
// Flow:
//
//      1. Client calls a tool; Handle_tools_call() dispatches it.
//      2. The handler either returns an MCPOutput directly, or asks the
//         client's LLM for additional input via a round trip:
//              a. Allocate a new MCPRoundTripRequest-derived instance.
//              b. Initiate a round-trip-request "inputRequired" by passing
//                 in the new instance of the MCPRoundTripRequest into 
//                 RequestRoundTrip().
//              c. The RequestInput() method builds the input_required result
//                 with FormMCPResult_inputRequired() (embedding requestState).
//                 MCPServer sends the input_required "result" instead of the
//                 tool's response.
//                 Note: FormMCPRequest_sampling() is a specialized kind of
//                       FormMCPResult_inputRequired().
//              d. The client's resume (new id) echoes requestState and
//                 carries the result; The OnResume() method gains control.
//              e. OnResume() answers the original tool request using the
//                 RESUME id (the original id was already used by the
//                 FormMCPResult_inputRequired() "result").
//
// Threading:
//
//      MCPRoundTripRequest allocation and RequestRoundTrip() run in any 
//      thread; OnResume() and reaping run in the main thread.
//
// Blocking variant (MCPSyncRoundTrip):
//
//      A worker thread (any model) derives MCPSyncRoundTrip, implements
//      FormInputRequest() (varargs -> input_required result), and calls
//      BlockingRequest(TimeoutMS, ...). It blocks until the resume arrives 
//      (or the timeout expires). The worker thread owns the MCPInputRequest 
//      (containing the client's response to inputRequired) returned by
//      BlockingRequest(). The server is to return an MCPOutput to answer 
//      the original tool request using the get_resumeId() to address the 
//      "answer". 
//
//      CancelWait() unblocks the call (BlockingRequest() returns NULL). 
//      If BlockingRequest() is invoked within an AsyncWork, Call 
//      CancelWait() from within AbortWork().
//

#ifndef __MCPMultiTrip_Core

        #define __MCPMultiTrip_Core

#include <ASThread.h>
#include <LinkList.h>

#include <MCPServerCore.h>

// ***************************************************************************
// **** MCPServer Input Required (MRTR) Flow Control *************************
// ***************************************************************************

struct MCPRoundTripRequest : public Link
        {
        time_t RequestTime;
        JSON_Value_String requestState;
        // -------------------- 
        // ---- Input Args ----
        // --------------------
        JSON_Value &Id;                 // Points to sId or nId or zId.
        JSON_Object Arguments;          // Local copy.
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPRoundTripRequest(JSON_Value &inId, JSON_Object &inArgs);
        virtual ~MCPRoundTripRequest() {}
        private:
                // -----------------
                // Id local storage.
                // -----------------
                JSON_Value_String sId;  // Id local storage.
                JSON_Value_Number nId;
                JSON_Value_Null zId;
                friend class MCPRoundTripManager;
        protected:
                virtual bool GenerateRequestState();
                // -------------------------------
                // Note: Handlers return MCPOutput
                // -------------------------------
                virtual char* RequestInput() = 0;
                virtual char* OnResume(MCPInputRequest &MCPRequest) = 0;
        };
                    
char* _RequestRoundTrip(MCPRoundTripRequest **Request); // Return MCPOutput.

#define RequestRoundTrip(Req) _RequestRoundTrip((MCPRoundTripRequest**)Req)

// ***************************************************************************
// **** Blocking Round Trip Request to be used by Threads ********************
// ***************************************************************************

class MSRoundTripQueue : public MessageQueue
        {
        protected:
                virtual void OnMessageDestruct(void *Msg);
        };

class MCPSyncRoundTrip
        {
        private:
                volatile bool Cancelled;        // Aborts further trips.
                MSRoundTripQueue ResponseQ;
                struct MCPSyncTripRequest *Work;
                friend struct TripRequestConnector;
                friend struct MCPSyncTripRequest;
                // -----------------
                // Id local storage.
                // -----------------
                JSON_Value *Id;         // Points to sId or nId or zId.
                JSON_Value_String sId;  // Id local storage.
                JSON_Value_Number nId;
                JSON_Value_Null zId;
                void set_Id(JSON_Value &inId);
        protected:
                // -----------------------------------------------
                // ---- Overridable Factory for Input Request ----
                // -----------------------------------------------
                virtual char* FormInputRequest(JSON_Value &inId,
                                               JSON_Value_String &requestState,
                                               va_list Params) = 0;
        public:
                MCPSyncRoundTrip(JSON_Value &inId);
                ~MCPSyncRoundTrip();
                virtual bool CancelWait();
                JSON_Value& get_resumeId() {return *Id;}
                bool isCancelled() {return Cancelled;}
                // -------------------------
                // ---- BlockingRequest ----
                // ----------------------------------------------------
                // Output: Copy of MCPClient response (caller deletes).
                //         Answer the client with get_resumeId().
                // ----------------------------------------------------
                virtual MCPInputRequest* BlockingRequest(unsigned TimeoutMS,
                                                         ...);
        };

/* ***************************************************************************
 Notes:

******************************************************************************
**** Example: helloWorld Sample **********************************************
*********************************

struct HelloWorldSample : public MCPRoundTripRequest
        {
        HelloWorldSample(JSON_Value &inId, JSON_Object &inArgs)
                :
                MCPRoundTripRequest(inId,inArgs)
                {
                return;
                }
        virtual char* RequestInput();      
        virtual char* OnResume(MCPInputRequest &MCPRequest);
        };

char* HelloWorldSample::RequestInput()
        {
        const char *Name = Arguments.Find_member_string("name");
        MemoryPrintf Canvas;
        Canvas.printf("How do you say 'Hello %s' in French?",Name);
        // ---- The sampling overload embeds requestState so the ----
        // ---- client's resume can be correlated in OnResume(). ----
        return FormMCPRequest_sampling(
                Id,
                requestState,
                "You are a translator. Reply with only the translation.",
                Canvas.GetBuffer(),
                100);
        }

char* HelloWorldSample::OnResume(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "HelloWorldSample::OnResume";
        // ---- The original id was already answered by the       ----
        // ---- input_required result; answer the resume id here. ----
        JSON_Value *idResume = MCPRequest.Get_id();
        const char *Translation = ExtractSampleText(MCPRequest);
        if (!Translation)
                {
                TERROR(("%s: No text in resume (id=%s)",
                        ProcName,id2string(idResume)));
                return FormMCPResponse_ERROR(idResume,
                                             MCPErrorCode_Exception,
                                             "Sampling Failed",
                                             "No text in resume response");
                }
        const char *Name = Arguments.Find_member_string("name");
        MemoryPrintf Canvas;
        Canvas.printf("Hello %s! In French: %s",Name,Translation);
        char *Greeting = Canvas.AquireBuffer();
        char *MCPOutput = FormMCPResponse_helloWorld(*idResume,Greeting);
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
                MCPOutput = RequestRoundTrip(&Request);
                if (Request) delete Request;
                }
        return MCPOutput;
        }

******************************************************************************
**** Example: Blocking Sample from Thread ************************************
**********************************************

struct HelloWorldSyncSampler : public MCPSyncRoundTrip
        {
        HelloWorldSyncSampler(JSON_Value &inId)
                :
                MCPSyncRoundTrip(inId)
                {
                return;
                }
        protected:
                // ---- Factory: unpack the BlockingRequest() varargs   ----
                // ---- (SystemPrompt, UserMessage, MaxTokens) and wrap ----
                // ---- them as a sampling input_required round trip.   ----
                virtual char* FormInputRequest(JSON_Value &inId,
                                               JSON_Value_String &requestState,
                                               va_list Params)
                        {
                        const char *SystemPrompt = va_arg(Params,const char *);
                        const char *UserMessage = va_arg(Params,const char *);
                        int MaxTokens = va_arg(Params,int);
                        return FormMCPRequest_sampling(
                                inId,
                                requestState,
                                SystemPrompt,
                                UserMessage,
                                MaxTokens);
                        }
        };

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
                HelloWorldSyncSampler Sampler;
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
                        Response = Sampler.BlockingRequest(
                                0,      // TimeoutMS: wait indefinitely.
                                "You are a translator. "
                                "Reply with only the translation.",
                                Question.GetBuffer(),
                                100);
                        AdoptResumeId(Sampler);
                        if (!Response) return false;
                        // ----------------------------------------
                        // ---- Extract Translation from Resume ---
                        // ----------------------------------------
                        const char *Translation = ExtractSampleText(*Response);
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

#endif /* __MCPMultiTrip_Core */
