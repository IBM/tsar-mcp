// MCP Multi Round-Trip Requests: MCPServerMRTR.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServerMRTR.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// ENABLE: LINK MCPServerMRTR.cpp with MCPServer.cpp
//

#include <assert.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPMultiTripCore.h>

// -------------------------------
// ---- Stale MRTR Management ----
// -------------------------------

#define RoundTripTTL (60*20)                    // 20 Minutes.

extern void PostMRTRReapTTL();                  // From MCPServer.cpp

// ***************************************************************************
// **** MCPRoundTripManager Methods ******************************************
// ***************************************************************************

DefineLinkList(MCPRoundTripList,MCPRoundTripRequest);

class MCPRoundTripManager
        {
        private:
                Mutex RequestMutex;
                MCPRoundTripList Requests;
        protected:
                void AddRequest(MCPRoundTripRequest *Request)
                        {
                        RequestMutex.Take();
                        Requests.AddBottom(Request);
                        RequestMutex.Release();
                        }
                void DeleteRequest(MCPRoundTripRequest **Request)
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
                MCPRoundTripRequest* FindRequest(JSON_Value &requestState);
        public:
                MCPRoundTripManager();
                ~MCPRoundTripManager() {Clear();}
                char* BeginRequest(MCPRoundTripRequest **Request);
                void Clear();
                char* HandleResponse(MCPInputRequest &Response);
                void ReapTTL(time_t secondsTL);
        };
          
// ***************************************************************************
// **** MCPRoundTripRequest Methods ******************************************
// ***************************************************************************
  
MCPRoundTripRequest::MCPRoundTripRequest(JSON_Value &inId, JSON_Object &inArgs) 
        :
        Id(inId.isString() ? (JSON_Value&)sId 
                           : inId.isNumber()
                           ? (JSON_Value&)nId
                           : (JSON_Value&)zId),
        Arguments(inArgs)
        {
        if (inId.isString()) sId = (JSON_Value_String &)inId;
        else if (inId.isNumber()) nId = (JSON_Value_Number &)inId;
        RequestTime = 0;
        GenerateRequestState();
        return;
        }

// ---------------------------------------------------------------
// ---- GenerateRequestState : Build a unique requestState -------
// --------------------------------------------------------
//
//    In a hostile environment (HTTP / MCP Gateway) requestState
//    must not be guessable: swap the UniqueCounter for a CSPRNG
//    sequence (/dev/urandom). It is only a one-time nonce for a
//    lookup table -- never deciphered -- so it needn't be signed.
//
// ---------------------------------------------------------------

bool MCPRoundTripRequest::GenerateRequestState()
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
        Canvas.printf("RoundTrip_%s_%u",id2string(Id),UniqueNumber);
        const char *idString = Canvas.GetBuffer();
        if (idString)
                {
                requestState.SetString(idString,strlen(idString));
                }
        return idString != NULL;
        }

// ***************************************************************************
// **** MCPRoundTripManager Methods ******************************************
// ***************************************************************************

#define requestState2string(reqState)                                        \
        (!(reqState).isString()                                              \
         ? ""                                                                \
         : ((JSON_Value_String &)(reqState)).string)

MCPRoundTripManager::MCPRoundTripManager()
        {
        Requests.SetAutoCleanup(true);
        return;
        }

void MCPRoundTripManager::Clear() 
        {
        static const char *ProcName = "MCPRoundTripManager::Clear";
        RequestMutex.Take();
        MCPRoundTripRequest *Request = Requests.GetFirst();
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

char* MCPRoundTripManager::BeginRequest(MCPRoundTripRequest **Request)
        {
        static const char *ProcName = "MCPRoundTripManager::BeginRequest";
        char *MCPOutput = NULL;
        if (!Request || !*Request)
                {
                TERROR(("%s: No Request",ProcName));
                MCPOutput = FormMCPResponse_ERROR(NULL,
                                                  MCPErrorCode_Exception,
                                                  "Can't request Sampling",
                                                  "No Trip Request");
                return MCPOutput;
                }
        PostMRTRReapTTL();              // Reap abandoned trips on new activity.
        MCPOutput = (*Request)->RequestInput();
        if (!MCPOutput)
                {
                TERROR(("%s: No Input Request Made",ProcName));
                }
        else    {
                (*Request)->RequestTime = time(NULL);
                JSON_Value &inputID = (*Request)->Id;
                JSON_Value &requestState = (*Request)->requestState;
                TDEBUG(("%s: Starting Trip Request '%.64s' for '%s'",
                                 ProcName,
                                 requestState2string(requestState),
                                 id2string(inputID)));
                AddRequest(*Request);
                *Request = NULL;
                }
        return MCPOutput;
        }

MCPRoundTripRequest* MCPRoundTripManager::FindRequest(JSON_Value &requestState)
        {
        RequestMutex.Take();
        MCPRoundTripRequest *Current = Requests.GetFirst();
        while (Current)
                {
                if (idcmp(Current->requestState,requestState)) break;
                Current = Requests.GetNext(Current);
                }
        RequestMutex.Release();
        return Current;
        }

void MCPRoundTripManager::ReapTTL(time_t secondsTL) // From main thread.
        {
        static const char *ProcName = "MCPRoundTripManager::ReapTTL";
        time_t Now = time(NULL);
        RequestMutex.Take();
        MCPRoundTripRequest *Current = Requests.GetFirst();
        while (Current)
                {
                if (Now - Current->RequestTime > secondsTL)
                        {
                        Requests.Remove(Current);
                        TERROR(("%s: Trip Request Expired: %.64s",
                                ProcName,
                                requestState2string(Current->requestState)));
                        // -----------------------------------------------
                        // ~MCPSyncTripRequest takes TripRequestConnector,
                        // so the delete must not hold RequestMutex. Once
                        // released the walk cursor is no longer reliable:
                        // restart from the head.
                        // -----------------------------------------------
                        RequestMutex.Release();
                        delete Current;
                        RequestMutex.Take();
                        Current = Requests.GetFirst();
                        }
                else Current = Requests.GetNext(Current);
                }
        RequestMutex.Release();
        return;
        }

char* MCPRoundTripManager::HandleResponse(MCPInputRequest &Response)
        {
        static const char *ProcName = "MCPRoundTripManager::HandleResponse";
        ReapTTL(RoundTripTTL);
        char *MCPOutput = NULL;
        JSON_Value *idResponse = Response.Get_id();
        JSON_Object *Params = Response.Get_params();
        JSON_Value *requestState = Params
                                 ? Params->FindMemberValue("requestState")
                                 : NULL;
        if (!idResponse)
                {
                TERROR(("%s: No Id in Response",ProcName));
                MCPOutput = FormMCPResponse_ERROR(idResponse,
                                                  MCPErrorCode_InvalidRequest,
                                                  "No Id",
                                                  "No Id found");
                }
        else if (!requestState)
                {
                TERROR(("%s: No requestState in Response",ProcName));
                MCPOutput = FormMCPResponse_ERROR(idResponse,
                                                  MCPErrorCode_InvalidRequest,
                                                  "No requestState",
                                                  "No requestState found in %s",
                                                  id2string(idResponse));
                }

        else if (!requestState->isString())
                {
                TERROR(("%s: No requestState in Response",ProcName));
                MCPOutput = FormMCPResponse_ERROR(idResponse,
                                                  MCPErrorCode_InvalidRequest,
                                                  "requestState is not a string",
                                                  "requestState in %s is not a string",
                                                  id2string(idResponse));
                }
        else    {
                MCPRoundTripRequest *Request = FindRequest(*requestState);
                if (!Request)
                        {
                        TDEBUG(("%s: Handler not found: %.64s",
                                 ProcName,
                                 requestState2string(*requestState)));
                        return NULL;    // No handler handled by caller.
                        }
                MCPOutput = Request->OnResume(Response);
                if (!MCPOutput)
                        {
                        TERROR(("%s: OnResume returned NULL (id=%s)",
                                ProcName,
                                id2string(idResponse)));
                        MCPOutput = FormMCPResponse_ERROR(idResponse,
                                               MCPErrorCode_Exception,
                                               "Round Trip Failed",
                                               "Requester no longer waiting");
                        }
                DeleteRequest(&Request);
                }
        return MCPOutput;
        }

// ***************************************************************************
// **** MRTR Events **********************************************************
// ***************************************************************************

static MCPRoundTripManager TripManager; 

char* _RequestRoundTrip(MCPRoundTripRequest **Request)
        {
        // ------------------------
        // Runs in caller's thread.
        // ------------------------
        return TripManager.BeginRequest(Request);
        }

static char* Handle_MCPTripResponse(MCPInputRequest &Response)
        {
        // --------------------
        // Runs in Main Thread.
        // --------------------
        return TripManager.HandleResponse(Response);
        }

static void Handle_MCPMRTRReapTTL()
        {
        // --------------------
        // Runs in Main Thread.
        // --------------------
        TripManager.ReapTTL(RoundTripTTL);
        return;
        }

// ***************************************************************************
// **** Enable MCPServerMRTR - Setup Function Pointers ***********************
// ***************************************************************************

// -----------------------------------------------------------------
// ---- MCP Multi Round-Trip Request Extension (External Hooks) ----
// -----------------------------------------------------------------

extern void (*OnShutdownMRTRHook)();
extern char* (*Handle_MRTR_hook)(MCPInputRequest &Response);
extern void (*Handle_MRTR_reapttl)();

static void OnShutdown()
        {
        TripManager.Clear();
        return;
        }

struct _MCPServerMRTRInit
        {
        _MCPServerMRTRInit()
                {
                Handle_MRTR_hook = Handle_MCPTripResponse;
                Handle_MRTR_reapttl = Handle_MCPMRTRReapTTL;
                OnShutdownMRTRHook = OnShutdown;
                }
        };

static _MCPServerMRTRInit MCPServerMRTRInit;

// ***************************************************************************
// **** Blocking Round Trip Request to be used by Threads ********************
// *******************************************************
//
// Implementation Notes:
//
//      MCPSyncRoundTrip and MCPSyncTripRequest cross-reference each
//      other and can be destroyed independently in any thread. All
//      Wireup/Unwire go through one shared mutex (TripRequestConnector).
//      A single mutex is essential: per-object mutexes would deadlock
//      AB/BA when both sides are reaped concurrently.
//
//      MCPSyncTripRequest::OnResume() returns Return_MCPOutput_Pending()
//      (not NULL): the response was handled, but the worker thread still
//      delivers the real result via CompleteHandler()/PostMCPOutput().
//
// ***************************************************************************

// --------------------------
// ---- MCPSyncRoundTrip ----
// --------------------------

struct MCPSyncTripRequest : MCPRoundTripRequest
        {
        // -------------------- 
        // ---- Input Args ----
        // --------------------
        MCPSyncRoundTrip *SyncRequester;
        char *MCPOutputInputRequest;
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPSyncTripRequest(MCPSyncRoundTrip &Requester,
                           JSON_Value &inId,
                           va_list Params);
        virtual ~MCPSyncTripRequest();
        protected:
                // -------------------------------
                // Note: Handlers return MCPOutput
                // -------------------------------
                virtual char* RequestInput();
                virtual char* OnResume(MCPInputRequest &MCPRequest);
        };

// ----------------------------------------------------------------
// ---- Global Wireup ---------------------------------------------
// ----------------------------------------------------------------
//
// One mutex covers deletion of either side in any order (usually
// both in the main thread, but that is not required).
//
// ----------------------------------------------------------------

struct TripRequestConnector
        {
        Mutex WiringMutex;
        void Wireup(MCPSyncRoundTrip &SyncRequester, MCPSyncTripRequest &Work);
        void Unwire(MCPSyncRoundTrip &SyncRequester);
        void Unwire(MCPSyncTripRequest &Work);
        void Take() {WiringMutex.Take();}
        void Release() {WiringMutex.Release();}
        } static TripRequestConnector;

void TripRequestConnector::Wireup(MCPSyncRoundTrip &SyncRequester, 
                                  MCPSyncTripRequest &Work)
        {
        Take();
        SyncRequester.Work = &Work;
        Work.SyncRequester = &SyncRequester;
        Release();
        return;
        }
         
void TripRequestConnector::Unwire(MCPSyncRoundTrip &SyncRequester)
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

void TripRequestConnector::Unwire(MCPSyncTripRequest &Work)
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

// --------------------------
// ---- MSRoundTripQueue ----
// --------------------------

static char MCPSyncRoundTripCANCEL[] = "MCPSyncRoundTripCANCEL"; 

void MSRoundTripQueue::OnMessageDestruct(void *Msg)
        {
        static const char *ProcName = "MSRoundTripQueue::OnMessageDestruct";
        if (Msg == MCPSyncRoundTripCANCEL)
                {
                TDEBUG(("%s: Unhandled CANCEL Signal",ProcName));
                }
        else    {
                MCPInputRequest *outResponse = (MCPInputRequest *)Msg;
                TINFO(("%s: Unhandled Trip Response: MCPInputRequest",
                       ProcName));
                delete outResponse;
                }
        return;
        }

// --------------------------
// ---- MCPSyncRoundTrip ----
// --------------------------
         
MCPSyncRoundTrip::MCPSyncRoundTrip(JSON_Value &inId) 
        {
        set_Id(inId);
        Work = NULL;
        Cancelled = false;
        return;
        }

MCPSyncRoundTrip::~MCPSyncRoundTrip()
        {
        TripRequestConnector.Unwire(*this);
        return;
        }

MCPInputRequest* MCPSyncRoundTrip::BlockingRequest(unsigned TimeoutMS, ...)
        {
        static const char *ProcName = "MCPSyncRoundTrip::BlockingRequest";
        if (isCancelled())
                {
                TINFO(("%s: Cancelled -- no further Sampling",ProcName));
                return NULL;
                }
        va_list Params;
        va_start(Params,TimeoutMS);
        MCPSyncTripRequest *SyncWork = new MCPSyncTripRequest(*this,*Id,Params);
        va_end(Params);
        if (!SyncWork)
                {
                TERROR(("%s: Can't allocate MCPSyncTripRequest (%s)",
                        ProcName,
                        id2string(Id)));
                return NULL;
                }
        if (!SyncWork->MCPOutputInputRequest)
                {
                TERROR(("%s: FormSamplingRequest Failed (%s)",
                        ProcName,
                        id2string(Id)));
                delete SyncWork;
                return NULL;
                }
        TripRequestConnector.Wireup(*this,*SyncWork);
        const char *requestState = SyncWork->requestState.string;
        // -----------------------------------------
        // ---- Record Pending Sampling Request ----
        // -----------------------------------------
        char *MCPOutput = _RequestRoundTrip((MCPRoundTripRequest**)&SyncWork);
        if (SyncWork)
                {
                TERROR(("%s: Unable to _RequestRoundTrip (%.64s)",
                        ProcName,
                        requestState));
                TripRequestConnector.Unwire(*this); // Disassociate Work item.
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
                TERROR(("%s: PostMCPOutput() Failed (%.64s)",ProcName,
                                                             requestState));
                TripRequestConnector.Unwire(*this); // Disassociate Work item.
                // Work lost on MCPRoundTripList (until Clear()).
                return NULL;
                }
        // ------------------------------------------------------
        // ---- Get the Sampling Response from the MCPClient ----
        // ------------------------------------------------------
        void *Msg = NULL;
        if (TimeoutMS)
                {
                TINFO(("%s: Waiting %u mSeconds for MRTR Response: %.64s",
                       ProcName,
                       TimeoutMS,
                       requestState));
                Msg = ResponseQ.Receive(TimeoutMS);
                }
        else    {
                TDEBUG(("%s: Waiting indefinitely for MRTR Response: %.64s", 
                        ProcName,
                        requestState));
                Msg = ResponseQ.Receive();
                }
        TripRequestConnector.Unwire(*this);     // Done: Disassociate Work item.
        if (!Msg)
                {
                TERROR(("%s: ResponseQ.Receive() Timeout (%.64s)",
                        ProcName,
                        requestState));
                return NULL;
                }
        if (Msg == MCPSyncRoundTripCANCEL)
                {
                TINFO(("%s: Sampling Waiting Cancelled (%.64s)",
                       ProcName,
                       requestState));
                return NULL;
                }
        MCPInputRequest *outResponse = (MCPInputRequest *)Msg;
        JSON_Value *idResponse = outResponse->Get_id();
        TINFO(("%s: Recieved MRTR Response: %s -> %s",
               ProcName,
               id2string(Id),
               id2string(idResponse)));
        if (idResponse)
                {
                TDEBUG(("%s: Adopting Resume Id: %s",ProcName,id2string(idResponse)));
                set_Id(*idResponse);
                }
        return outResponse;
        }

bool MCPSyncRoundTrip::CancelWait()
        {
        static const char *ProcName = "MCPSyncRoundTrip::CancelWait";
        TINFO(("%s: CancelWait() Request",ProcName));
        Cancelled = true;
        return ResponseQ.SendPriority(MCPSyncRoundTripCANCEL);
        }

void MCPSyncRoundTrip::set_Id(JSON_Value &inId)
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

// ----------------------------
// ---- MCPSyncTripRequest ----
// ----------------------------

static JSON_Object _emptyJSONObject;

MCPSyncTripRequest::MCPSyncTripRequest(MCPSyncRoundTrip &Requester, 
                                       JSON_Value &inId,
                                       va_list Params)
        :
        MCPRoundTripRequest(inId,_emptyJSONObject)
        {
        SyncRequester = NULL;
        MCPOutputInputRequest = Requester.FormInputRequest(inId,
                                                           requestState,
                                                           Params);
        return;
        }

MCPSyncTripRequest::~MCPSyncTripRequest() 
        {
        static const char *ProcName = "~MCPSyncTripRequest";
        if (MCPOutputInputRequest) 
                {
                TERROR(("%s: Unexpected MCPOutput Input Request",ProcName));
                free(MCPOutputInputRequest);
                MCPOutputInputRequest = NULL;
                }
        bool MRTRPending = false;
        TripRequestConnector.Take();
        if (SyncRequester)
                {
                MRTRPending = true;
                SyncRequester->CancelWait();      // Must be pending.
                }
        TripRequestConnector.Release();
        TripRequestConnector.Unwire(*this);
        if (MRTRPending)
                {
                TERROR(("%s: Trip Pending: %.64s",
                        ProcName,
                        requestState2string(requestState)));
                }
        else    {
                TDEBUG(("%s: Trip completed: %.64s",
                        ProcName,
                        requestState2string(requestState)));
                }
        return;
        }

char* MCPSyncTripRequest::OnResume(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "MCPSyncTripRequest::OnResume";
        TINFO(("%s: Handling: %s",ProcName,id2string(MCPRequest.Get_id())));
        MCPInputRequest *RequestCopy = new MCPInputRequest(MCPRequest);
        void *Msg = RequestCopy;
        if (!RequestCopy)
                {
                TERROR(("%s: Can't Duplicate Resume Response (%s)",
                        ProcName,
                        id2string(MCPRequest.Get_id())));
                Msg = MCPSyncRoundTripCANCEL;
                }
        char *MCPOutput = NULL;
        bool SentStatus = false;
        TripRequestConnector.Take();
        if (SyncRequester) 
                {
                SentStatus = SyncRequester->ResponseQ.Send(Msg);
                if (SentStatus) RequestCopy = NULL; 
                }
        TripRequestConnector.Release();
        if (SentStatus)
                {
                TripRequestConnector.Unwire(*this);
                TINFO(("%s: Unblocked BlockingRequest() Wait (%s)",
                       ProcName,
                       id2string(MCPRequest.Get_id())));
                MCPOutput = Return_MCPOutput_Pending();
                }
        if (RequestCopy)
                {
                TERROR(("%s: No requester waiting (%s)",
                        ProcName,
                        id2string(MCPRequest.Get_id())));
                delete RequestCopy;
                }
        return MCPOutput;
        }

char* MCPSyncTripRequest::RequestInput()
        {
        char *outMCPOutput = MCPOutputInputRequest; // Return stored request.
        MCPOutputInputRequest = NULL;
        return outMCPOutput;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
