// MCP Server Sample Feature: MCPServerSample.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServerSample.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// ENABLE: LINK MCPServerSample with MCPServer
//

#include <assert.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPSampleCore.h>

DefineLinkList(MCPSampleRequestList,MCPSampleRequest);

class MCPSampleManager
        {
        private:
                Mutex RequestMutex;
                MCPSampleRequestList Requests;
        protected:
                void AddRequest(MCPSampleRequest *Request)
                        {
                        RequestMutex.Take();
                        Requests.AddBottom(Request);
                        RequestMutex.Release();
                        }
                void DeleteRequest(MCPSampleRequest **Request)
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
                MCPSampleRequest* FindRequest(JSON_Value &Id)
                        {
                        RequestMutex.Take();
                        MCPSampleRequest *Current = Requests.GetFirst();
                        while (Current)
                                {
                                if (idcmp(Current->myId,Id)) break;
                                Current = Requests.GetNext(Current);
                                }
                        RequestMutex.Release();
                        return Current;
                        }
        public:
                MCPSampleManager();
                ~MCPSampleManager() {Clear();}
                char* BeginRequest(MCPSampleRequest **Request);
                void Clear();
                char* HandleResponse(MCPInputRequest &Response);
        };
          
// ***************************************************************************
// **** MCPSampleRequest Methods *********************************************
// ***************************************************************************
  
MCPSampleRequest::MCPSampleRequest(JSON_Value &inId, JSON_Object &inArgs) 
        : 
        Id(inId.isString() ? (JSON_Value&)sId 
                           : inId.isNumber()
                           ? (JSON_Value&)nId
                           : (JSON_Value&)zId),
        Arguments(inArgs)
        {
        if (inId.isString()) sId = (JSON_Value_String &)inId;
        else if (inId.isNumber()) nId = (JSON_Value_Number &)inId;
        GenerateMyId();
        return;
        }

// -----------------------------------------------------------------
// ---- GenerateMyId : Build a unique Sampling myId based on Id ----
// -----------------------------------------------------------------

bool MCPSampleRequest::GenerateMyId()
        {
        // --------------------------
        // ---- Unique Id Number ----
        // --------------------------
        static CriticalSection csUniqueCounter;
        static unsigned UniqueCounter = 0;
        csUniqueCounter.Take();
        unsigned UniqueNumber = ++UniqueCounter;
        csUniqueCounter.Release();
        // --------------------------------------------
        // ---- Build: Smpl_RequestId_UniqueNumber ----
        // --------------------------------------------
        MemoryPrintf Canvas;
        Canvas.printf("Smpl_%s_%u",id2string(Id),UniqueNumber);
        const char *idString = Canvas.GetBuffer();
        if (idString)
                {
                myId.SetString(idString,strlen(idString));
                }
        return idString != NULL;
        }

// -----------------------------
// ---- FormSamplingRequest ----
// -----------------------------

char* MCPSampleRequest::FormSamplingRequest(const char *SystemPrompt,
                                            const char *UserMessage,
                                            int MaxTokens)
        {
        return FormMCPRequest_sampling(myId,SystemPrompt,UserMessage,MaxTokens);
        }

// ***************************************************************************
// **** MCPSampleManager Methods *********************************************
// ***************************************************************************

MCPSampleManager::MCPSampleManager()
        {
        Requests.SetAutoCleanup(true);
        return;
        }

void MCPSampleManager::Clear() 
        {
        static const char *ProcName = "MCPSampleManager::Clear";
        RequestMutex.Take();
        MCPSampleRequest *Request = Requests.GetFirst();
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

char* MCPSampleManager::BeginRequest(MCPSampleRequest **Request)
        {
        static const char *ProcName = "MCPSampleManager::BeginRequest";
        char *MCPOutput = NULL;
        if (!Request || !*Request)
                {
                TERROR(("%s: No Request",ProcName));
                MCPOutput = FormMCPResponse_ERROR(NULL,
                                                  MCPErrorCode_Exception,
                                                  "Can't request Sampling",
                                                  "No Sample Request");
                return MCPOutput;
                }
        MCPOutput = (*Request)->RequestSampling();
        if (!MCPOutput)
                {
                TERROR(("%s: No Sampling Request Made",ProcName));
                }
        else    {
                JSON_Value &inputID = (*Request)->Id;
                JSON_Value &sampleID = (*Request)->myId;
                TDEBUG(("%s: Starting Sampling Request '%s' for '%s'",
                                 ProcName,
                                 id2string(sampleID),
                                 id2string(inputID)));
                AddRequest(*Request);
                *Request = NULL;
                }
        return MCPOutput;
        }

char* MCPSampleManager::HandleResponse(MCPInputRequest &Response)
        {
        static const char *ProcName = "MCPSampleManager::HandleResponse";
        char *MCPOutput = NULL;
        JSON_Value *idRequest = Response.Get_id();
        if (!idRequest)
                {
                TERROR(("%s: No Id in Request",ProcName));
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidRequest,
                                                  "No Id",
                                                  "No Id found");
                }
        else    {
                MCPSampleRequest *Request;
                Request = FindRequest(*idRequest);
                if (!Request)
                        {
                        TDEBUG(("%s: Handler not found: %s",
                                 ProcName,
                                 id2string(idRequest)));
                        return NULL;    // No handler handled by caller.
                        }
                MCPOutput = Request->OnResponse(Response);
                if (!MCPOutput)
                        {
                        TERROR(("%s: OnResponse returned NULL (id=%s)",
                                ProcName,
                                id2string(Request->Id)));
                        MCPOutput = FormMCPResponse_ERROR(&Request->Id,
                                                  MCPErrorCode_Exception,
                                                  "Sampling Failed",
                                                  "OnResponse failed");
                        }
                DeleteRequest(&Request);
                }
        return MCPOutput;
        }

// ***************************************************************************
// **** Sample Events ********************************************************
// ***************************************************************************

static MCPSampleManager SampleManager; 

char* _RequestSample(MCPSampleRequest **Request)
        {
        // ------------------------
        // Runs in caller's thread.
        // ------------------------
        return SampleManager.BeginRequest(Request);
        }

static char* Handle_MCPSampleResponse(MCPInputRequest &Response)
        {
        // --------------------
        // Runs in Main Thread.
        // --------------------
        return SampleManager.HandleResponse(Response);
        }

// ***************************************************************************
// **** isSamplingSupported **************************************************
// ***************************************************************************

static bool SamplingSupported = false;

static bool isSamplingSupported(MCPInputRequest &MCPRequest)
        {
        JSON_Object *Capabilities = MCPRequest.Get_param_object("capabilities");
        if (!Capabilities) return false;
        if (!Capabilities->Find_member_object("sampling")) return false;
        return true;
        }

bool isSamplingSupported()
        {
        return SamplingSupported;
        }

// ***************************************************************************
// **** Enable MCPServerSample - Setup Function Pointers *********************
// ***************************************************************************

// -----------------------------------------------
// ---- MCP Sample Extension (External Hooks) ----
// -----------------------------------------------

extern void (*OnInitMCPSampleHook)(MCPInputRequest &MCPRequest);
extern void (*DeinitMCPSampleHook)();
extern char* (*Handle_sampling_hook)(MCPInputRequest &MCPRequest);

static void OnInitialize(MCPInputRequest &MCPRequest)
        {
        SamplingSupported = isSamplingSupported(MCPRequest);
        return;
        }

static void Deinitialize()
        {
        SampleManager.Clear();
        return;
        }

struct _MCPServerSampleInit
        {
        _MCPServerSampleInit()
                {
                OnInitMCPSampleHook = OnInitialize;
                Handle_sampling_hook = Handle_MCPSampleResponse;
                DeinitMCPSampleHook = Deinitialize;
                }
        };

static _MCPServerSampleInit MCPServerSampleInit;

// ***************************************************************************
// **** Blocking Sampler to be used by Threads *******************************
// ***************************************************************************
//
// Implementation Notes:
//
//      MCPSyncSampler and MCPSyncSampleRequest hold cross-references
//      to each other (Sampler->Work, Work->SyncSampler). Both sides
//      can be destroyed independently, in any thread. MSSConnector
//      uses a single shared mutex (WiringMutex) to serialize all
//      Wireup/Unwire operations.
//
//      A single mutex is essential: an earlier design used independent
//      mutexes in each class to protect its own reference pointer.
//      This would deadlock when MCPSyncSampler was reaped in one thread
//      (taking its mutex, then reaching for the other) while
//      MCPSyncSampleRequest was reaped simultaneously in another
//      thread (taking its mutex, then reaching for the first).
//      Classic AB/BA deadlock. One mutex eliminates it.
//
//      MCPSyncSampleRequest::OnResponse() returns Return_MCPOutput_Pending()
//      rather than NULL. The sentinel signals to MCPSampleManager and
//      MCPMain that the response was handled but the original tool call
//      is still in-flight -- the worker thread will deliver the real
//      result through CompleteHandler() / PostMCPOutput().
//

// ------------------------
// ---- MCPSyncSampler ----
// ------------------------

struct MCPSyncSampleRequest : MCPSampleRequest
        {
        // -------------------- 
        // ---- Input Args ----
        // --------------------
        MCPSyncSampler *SyncSampler;
        char *MCPOutputSample;
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPSyncSampleRequest(MCPSyncSampler &Sampler,
                             JSON_Value &inId,
                             const char *SystemPrompt,
                             const char *UserMessage,
                             int MaxTokens);
        virtual ~MCPSyncSampleRequest();
        protected:
                // -------------------------------
                // Note: Handlers return MCPOutput
                // -------------------------------
                virtual char* RequestSampling();
                virtual char* OnResponse(MCPInputRequest &MCPRequest);
        };

// ----------------------------------------------------------------
// ---- Global Wireup ---------------------------------------------
// -------------------
//
// Note: One mutex handles manages delete of MCPSyncSampler or
//       MCPSyncSampleRequest in any order. It is anticipated that
//       both are deleted in the main thread, but it is not a 
//       requirement
//
// ----------------------------------------------------------------

struct MSSConnector
        {
        Mutex WiringMutex;
        void Wireup(MCPSyncSampler &SyncSampler, MCPSyncSampleRequest &Work);
        void Unwire(MCPSyncSampler &SyncSampler);
        void Unwire(MCPSyncSampleRequest &Work);
        void Take() {WiringMutex.Take();}
        void Release() {WiringMutex.Release();}
        } static mSSConnector;

void MSSConnector::Wireup(MCPSyncSampler &SyncSampler, 
                          MCPSyncSampleRequest &Work)
        {
        Take();
        SyncSampler.Work = &Work;
        Work.SyncSampler = &SyncSampler;
        Release();
        return;
        }
         
void MSSConnector::Unwire(MCPSyncSampler &SyncSampler)
        {
        Take();
        if (SyncSampler.Work)
                {
                assert(SyncSampler.Work->SyncSampler == &SyncSampler);
                SyncSampler.Work->SyncSampler = NULL;
                SyncSampler.Work = NULL;
                }
        Release();
        return;
        }

void MSSConnector::Unwire(MCPSyncSampleRequest &Work)
        {
        Take();
        if (Work.SyncSampler)
                {
                assert(Work.SyncSampler->Work == &Work);
                Work.SyncSampler->Work = NULL;
                Work.SyncSampler = NULL;
                }
        Release();
        return;
        }

// -------------------------
// ---- MSSMessageQueue ----
// -------------------------

static char MCPSyncSamplerCANCEL[] = "MCPSyncSamplerCANCEL"; 

void MSSMessageQueue::OnMessageDestruct(void *Msg)
        {
        static const char *ProcName = "MSSMessageQueue::OnMessageDestruct";
        if (Msg == MCPSyncSamplerCANCEL)
                {
                TDEBUG(("%s: Unhandled CANCEL Signal",ProcName));
                }
        else    {
                MCPInputRequest *outResponse = (MCPInputRequest *)Msg;
                TINFO(("%s: Unhandled Sampling Output: MCPInputRequest",
                       ProcName));
                delete outResponse;
                }
        return;
        }

// ------------------------
// ---- MCPSyncSampler ----
// ------------------------
         
MCPSyncSampler::MCPSyncSampler(JSON_Value &inId) 
        :
        Id(inId.isString() ? (JSON_Value&)sId 
                           : inId.isNumber()
                           ? (JSON_Value&)nId
                           : zId)
        {
        if (inId.isString()) sId = (JSON_Value_String &)inId;
        else if (inId.isNumber()) nId = (JSON_Value_Number &)inId;
        Work = NULL;
        return;
        }

MCPSyncSampler::~MCPSyncSampler()
        {
        mSSConnector.Unwire(*this);
        return;
        }

MCPInputRequest* MCPSyncSampler::BlockingSample(const char *SystemPrompt,
                                                const char *UserMessage,
                                                int MaxTokens,
                                                unsigned TimeoutMS)
        {
        static const char *ProcName = "MCPSyncSampler::BlockingSample";
        MCPSyncSampleRequest *SyncWork = new MCPSyncSampleRequest(*this,
                                                                  Id,
                                                                  SystemPrompt,
                                                                  UserMessage,
                                                                  MaxTokens);
        if (!SyncWork)
                {
                TERROR(("%s: Can't allocate MCPSyncSampleRequest (%s)",
                        ProcName,
                        id2string(Id)));
                return NULL;
                }
        if (!SyncWork->MCPOutputSample)
                {
                TERROR(("%s: FormSamplingRequest Failed (%s)",
                        ProcName,
                        id2string(Id)));
                delete SyncWork;
                return NULL;
                }
        mSSConnector.Wireup(*this,*SyncWork);
        _MCPIdString _sampleId(SyncWork->myId);
        const char *sampleId = _sampleId;
        // -----------------------------------------
        // ---- Record Pending Sampling Request ----
        // -----------------------------------------
        char *MCPOutput = _RequestSample((MCPSampleRequest**)&SyncWork);
        if (SyncWork)
                {
                TERROR(("%s: Unable to _RequestSample (%s)",ProcName,sampleId));
                mSSConnector.Unwire(*this); // Disassociate Work item.
                delete SyncWork;    
                SyncWork = NULL;
                return NULL;
                }
        // ---------------------------------------------------
        // ---- Get the Sampling Request to the MCPClient ----
        // ---------------------------------------------------
        bool Status = PostMCPOutput(&MCPOutput);
        if (!Status)
                {
                TERROR(("%s: PostMCPOutput() Failed (%s)",ProcName,sampleId));
                mSSConnector.Unwire(*this); // Disassociate Work item.
                // Work lost on MCPSampleRequestList (until Clear()).
                return NULL;
                }
        // ------------------------------------------------------
        // ---- Get the Sampling Response from the MCPClient ----
        // ------------------------------------------------------
        void *Msg = NULL;
        if (TimeoutMS)
                {
                TINFO(("%s: Waiting %u mSeconds for Sampling Response: %s",
                       ProcName,
                       TimeoutMS,
                       sampleId));
                Msg = ResponseQ.Receive(TimeoutMS);
                }
        else    {
                TDEBUG(("%s: Waiting indefinitely for Sampling Response: %s", 
                        ProcName,
                        sampleId));
                Msg = ResponseQ.Receive();
                }
        mSSConnector.Unwire(*this);     // Done: Disassociate Work item.
        if (!Msg)
                {
                TERROR(("%s: ResponseQ.Receive() Timeout (%s)",ProcName,
                                                               sampleId));
                return NULL;
                }
        if (Msg == MCPSyncSamplerCANCEL)
                {
                TINFO(("%s: Sampling Waiting Cancelled (%s)",
                       ProcName,
                       sampleId));
                return NULL;
                }
        MCPInputRequest *outResponse = (MCPInputRequest *)Msg;
        JSON_Value *ResponseId = outResponse->Get_id();
        TINFO(("%s: Recieved Sampling Response: %s -> %s",
               ProcName,
               id2string(Id),
               id2string(ResponseId)));
        return outResponse;
        }

bool MCPSyncSampler::CancelWait()
        {
        static const char *ProcName = "MCPSyncSampler::CancelWait";
        TINFO(("%s: CancelWait() Request (%s)",ProcName,id2string(Id)));
        return ResponseQ.SendPriority(MCPSyncSamplerCANCEL);
        }

char* MCPSyncSampler::FormSamplingRequest(JSON_Value &idRequest,
                                          const char *SystemPrompt,
                                          const char *UserMessage,
                                          int MaxTokens)
        {
        return FormMCPRequest_sampling(idRequest,
                                       SystemPrompt,
                                       UserMessage,
                                       MaxTokens);
        }

// ------------------------------
// ---- MCPSyncSampleRequest ----
// ------------------------------

static JSON_Object _emptyJSONObject;

MCPSyncSampleRequest::MCPSyncSampleRequest(MCPSyncSampler &Sampler,
                                           JSON_Value &inId,
                                           const char *SystemPrompt,
                                           const char *UserMessage,
                                           int MaxTokens)
        :
        MCPSampleRequest(inId,_emptyJSONObject)
        {
        SyncSampler = NULL;
        MCPOutputSample = Sampler.FormSamplingRequest(myId,
                                                      SystemPrompt,
                                                      UserMessage,
                                                      MaxTokens);
        return;
        }

MCPSyncSampleRequest::~MCPSyncSampleRequest() 
        {
        static const char *ProcName = "~MCPSyncSampleRequest";
        if (MCPOutputSample) 
                {
                TERROR(("%s: Unexpected MCPOutput Sample",ProcName));
                free(MCPOutputSample);
                MCPOutputSample = NULL;
                }
        bool SamplePending = false;
        mSSConnector.Take();
        if (SyncSampler)
                {
                SamplePending = true;
                SyncSampler->CancelWait();      // Must be pending.
                }
        mSSConnector.Release();
        mSSConnector.Unwire(*this);
        if (SamplePending)
                {
                TERROR(("%s: Sample Pending: %s",ProcName,id2string(myId)));
                }
        else    {
                TDEBUG(("%s: Sample completed: %s",ProcName,id2string(myId)));
                }
        return;
        }

char* MCPSyncSampleRequest::OnResponse(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "MCPSyncSampleRequest::OnResponse";
        TINFO(("%s: Handling: %s",ProcName,id2string(MCPRequest.Get_id())));
        MCPInputRequest *RequestCopy = new MCPInputRequest(MCPRequest);
        void *Msg = RequestCopy;
        if (!RequestCopy)
                {
                TERROR(("%s: Can't Duplicate Sample Response (%s)",
                        ProcName,
                        id2string(MCPRequest.Get_id())));
                Msg = MCPSyncSamplerCANCEL;
                }
        bool SentStatus = false;
        mSSConnector.Take();
        if (SyncSampler) 
                {
                SentStatus = SyncSampler->ResponseQ.Send(Msg);
                if (SentStatus) RequestCopy = NULL; 
                }
        mSSConnector.Release();
        if (SentStatus)
                {
                mSSConnector.Unwire(*this);
                TINFO(("%s: Unblocked Sample Wait (%s)",
                        ProcName,
                        id2string(MCPRequest.Get_id())));
                }
        if (RequestCopy)
                {
                TERROR(("%s: Sample Response unExpected (%s)",
                        ProcName,
                        id2string(MCPRequest.Get_id())));
                delete RequestCopy;
                }
        return Return_MCPOutput_Pending(); // Thread will deliver the response.
        }

char* MCPSyncSampleRequest::RequestSampling()
        {
        char *outMCPOutput = MCPOutputSample;   // Return stored request.
        MCPOutputSample = NULL;
        return outMCPOutput;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
