// MCP Server Async Feature: MCPServerAsync.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServerAsync.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Request Flow:   MCPClient [JSON] -> {stdout}
//         {stdin} ->   @ MCPServerIOReader [JSON] #thread
//                          PostMCPMessage(JSON:MCPMessage) =>
//                 =>   @ MCPServerIOMain [JSON:MCPMessage] #main
//                          Handle_tools_call(MCPInputRequest) #main
//                            Begin_AsyncWork(MCPAsyncWork)      >--+
//                            Return_MCPOutput_Pending => NULL      |
//                      @ RunMCPDispatcher [MCPAsyncWork] #main     |
//                              Send(MCPAsyncWork) =>            <--+
//                 =>   @ RunMCPThread [MCPAsyncWork] #thread
//                          BeginHandler(MCPAsyncWork) #thread
//                            PostAsyncObject(MCPAsyncWork) =>
//                 =>   @ MCPServerIOMain [MCPAsyncWork] #main
//                          Handle_async_work(MCPAsyncWork)
//                      @ RunMCPDispatcher [MCPAsyncWork] #main
//                            Complete_SyncWork(MCPAsyncWork)
//                              CompleteHandler(MCPAsyncWork) #main
//                      @ MCPServerIOMain [JSON:MCPOutput] #main
//                          fprintf(JSON:MCPOutput) -> {stdout}
//           {stdin} -> MCPClient [JSON:MCPOutput]
//
// ENABLE: LINK MCPAsyncServer with MCPServer and set MCPServer_Asynchronous.
//

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPAsyncCore.h>

#ifndef NumMCPThreads                   // Number of simultaneous 
        #define NumMCPThreads 6         // requests; others will queue.
#endif

// ----------------------------
// ---- From MCPServer.cpp ----
// ----------------------------

bool PostAsyncObject(void *UserPtr);
bool PostShutdown();

#ifdef _WIN32
     int MCPExceptionHandler(const char *ProcName, EXCEPTION_POINTERS *EXInfo);
#endif

// ***************************************************************************
// **** Asynchronous MCPWork Objects for MCPServer ***************************
// ***************************************************************************

// -------------------------------------------
// ---- Dispatcher and Thread for MCPWork ----
// -------------------------------------------

class RunMCPThread : public MessageQueueThread
        {      
        private:
                Mutex WorkItemMutex;
                MCPAsyncWork *ActiveWorkItem;
        protected:
                virtual bool OnMessage(void *Msg);
                virtual void OnMessageDestruct(void *Msg);
                void SetWorkItem(MCPAsyncWork *Item);
        public:
                virtual ~RunMCPThread() {Stop(false);}
                bool AbortWork();
        };

class RunMCPDispatcher : public MessageQueue    // Runs in Main Thread. 
        {
        private:
                ASThreadID_t MainThread;
                RunMCPThread Threads[NumMCPThreads];
                MCPAsyncWork* CurrentWorkItems[NumMCPThreads];
                bool DispatchWork();
        protected:
                int FindWorkItem(JSON_Value &Id);
                int FindWorkItem(MCPAsyncWork *ToFind);
                virtual void OnMessageDestruct(void *Msg);
                // -------------------------
                // ---- Work Management ----
                // -------------------------
                char* Complete_SyncWork(MCPAsyncWork *WorkItem); // MCPOutput.
                bool Cancel_AsyncWork(JSON_Value &Id);
        public:
                RunMCPDispatcher() 
                        {
                        memset(CurrentWorkItems,0,sizeof(CurrentWorkItems));
                        MainThread = GetThreadID();
                        return;
                        }
                virtual ~RunMCPDispatcher() {Clear();}
                //
                // MCPServer Callbacks
                //
                static bool Cancel_async_work(MCPInputRequest &MCPRequest);
                static void Discard_async_work(void *UserPtr);
                static char* Handle_async_work(void *UserPtr);
                //
                // Note: Only call methods from the main thread.
                //
                bool _Begin_AsyncWork(MCPAsyncWork **WorkItem);
                bool StartThreads();
                void StopThreads(bool WaitToEnd);
        };

// ----------------------
// ---- RunMCPThread ----
// ----------------------

bool RunMCPThread::AbortWork()
        {
        bool Status = true;
        WorkItemMutex.Take();
        if (ActiveWorkItem) 
                {
                // -----------------------------------------------
                // Skip CompleteHandler() par protocol requirment.
                // -----------------------------------------------
                ActiveWorkItem->Cancelled = true;
                // --------------------------------------------
                // Let Worker attempt to stop work-in-progress.
                // --------------------------------------------
                Status = ActiveWorkItem->AbortWork();
                }
        WorkItemMutex.Release();
        return Status;
        }

bool RunMCPThread::OnMessage(void *Msg)
        {
        static const char *ProcName = "RunMCPThread::OnMessage";
        MCPAsyncWork *WorkItem = (MCPAsyncWork *)Msg;
        SetWorkItem(WorkItem);                          // Gate Enterence.
        JSON_Value &idRequest = WorkItem->get_Id();
        TDEBUG(("%s: Processing Work Item: %s",ProcName,id2string(idRequest)));
  #ifdef _WIN32
        __try   {
  #endif
                WorkItem->rcBeginHandler = WorkItem->BeginHandler();
  #ifdef _WIN32
                }
        __except (MCPExceptionHandler(ProcName,GetExceptionInformation()))
                {
                WorkItem->rcBeginHandler = false;
                TERROR(("%s: Hardware Exception - Shutdown",ProcName));
                PostShutdown();
                }
  #endif
        SetWorkItem(NULL);                              // Gate Exit.
        if (WorkItem->rcBeginHandler)
                {
                TDEBUG(("%s: Thread finished work: %s",ProcName,
                                                       id2string(idRequest)));
                }
        else    {
                TERROR(("%s: BeginHandler(%s) Failed",ProcName,
                                                      id2string(idRequest)));
                }
        bool Status = PostAsyncObject(WorkItem);        // "Results" to main.
        if (!Status)
                {
                TERROR(("%s: PostAsyncObject(%s) Failed - ZOMBIE",
                        ProcName,
                        id2string(idRequest)));
                }
        return true;
        }

void RunMCPThread::OnMessageDestruct(void *Msg)
        {
        static const char *ProcName = "RunMCPThread::OnMessageDestruct";
        // --------------------------------------
        // ---- Cleanup work on Thread Queue ----
        // --------------------------------------
        MCPAsyncWork *WorkItem = (MCPAsyncWork *)Msg;
        JSON_Value &idRequest = WorkItem->get_Id();
        TINFO(("%s: Clearing Work Item: %s",ProcName,id2string(idRequest)));
        delete WorkItem;
        return;
        }

void RunMCPThread::SetWorkItem(MCPAsyncWork *Item)
        {
        WorkItemMutex.Take();
        ActiveWorkItem = Item;
        WorkItemMutex.Release();
        return;
        }

// --------------------------
// ---- RunMCPDispatcher ----
// --------------------------

bool RunMCPDispatcher::_Begin_AsyncWork(MCPAsyncWork **WorkItem)
        {
        static const char *ProcName = "RunMCPDispatcher::_Begin_AsyncWork";
        if (GetThreadID() != MainThread)
                {
                TERROR(("%s: Must be called from the main thread",ProcName));
                return false;
                }
        if (!WorkItem || !*WorkItem) return false;
        JSON_Value &idRequest = (*WorkItem)->get_Id();
        TDEBUG(("%s: Enqueueing Work: %s",ProcName,id2string(idRequest)));
        bool Status = Send(*WorkItem);
        if (!Status)
                {
                TERROR(("%s: Failed to Queue Work Item: %s",ProcName,
                                                            id2string(idRequest)));
                }
        else    {
                *WorkItem = NULL;               // Work item is enqueued.
                bool dStatus = DispatchWork();
                if (!dStatus)
                        {
                        TERROR(("%s: Dispatching Call Failed",ProcName));
                        // Don't return false because WorkItem is on Queue.
                        }
                }
        return Status;
        }

bool RunMCPDispatcher::Cancel_AsyncWork(JSON_Value &Id) // True if found
        {
        static const char *ProcName = "RunMCPDispatcher::Cancel_AsyncWork";
        bool WorkFound = false;
        // ---------------------------------
        // ---- Locate Work in Progress ----
        // ---------------------------------
        int iWork = FindWorkItem(Id);
        if (iWork >= 0)
                {
                WorkFound = true;
                TINFO(("%s: Aborting Work (%s) on Thread %u",ProcName,
                                                             id2string(Id),
                                                             iWork));
                bool Cancelled = Threads[iWork].AbortWork();
                TINFO(("%s: Work %s Cancelled (%s)",ProcName,
                                                    Cancelled ? "was" : "NOT",
                                                    id2string(Id)));
                }
        // ------------------------------
        // ---- Locate Work Enqueued ----
        // ------------------------------
        if (!WorkFound)
                {      
                unsigned nEnqueued = GetMessageCount();
                for (unsigned i=0; i < nEnqueued; i++)
                        {
                        // ----------------------------------------------------
                        // Itterate through entire queue to keep work in order.
                        // ----------------------------------------------------
                        MCPAsyncWork *WorkItem = (MCPAsyncWork *)Receive(0);
                        if (!WorkItem)
                                {
                                TERROR(("%s: State Error - Queue Empty: %s",
                                        ProcName,
                                        id2string(Id)));
                                }
                        else if (WorkItem->idcmp(Id))
                                {
                                WorkFound = true;
                                WorkItem->Cancelled = true;
                                TINFO(("%s: Dequeuing Work - Cancelled (%s)",
                                       ProcName,
                                       id2string(Id)));
                                OnMessageDestruct(WorkItem);
                                }
                        else Send(WorkItem);
                        }
                }
        return WorkFound;
        }

char* RunMCPDispatcher::Complete_SyncWork(MCPAsyncWork *WorkItem)    
        {
        static const char *ProcName = "RunMCPDispatcher::Complete_SyncWork";
        if (!WorkItem) 
                {
                TERROR(("%s: Work Item EXPECTED",ProcName));
                return NULL;
                }
        JSON_Value &idRequest = WorkItem->get_Id();
        // --------------------------------------------
        // ---- Work Management : Reset / Dispatch ----
        // --------------------------------------------
        int rcFind = FindWorkItem(WorkItem);
        if (rcFind < 0)
                {
                TERROR(("%s: CRITICAL State No Work Item: %s",
                        ProcName,
                        id2string(idRequest)));
                }
        else CurrentWorkItems[rcFind] = NULL;           // Thread is available.
        DispatchWork();                                 // Restart it working.
        // ----------------------------
        // ---- Complete Work Item ----
        // ----------------------------
        char *MCPOutput = NULL;
        if (WorkItem->Cancelled)
                {
                TINFO(("%s: Work Cancelled, skipping CompleteHandler() : %s",
                        ProcName,
                        id2string(idRequest)));
                }
        else if (!WorkItem->rcBeginHandler)
                {
                TERROR(("%s: Work failed, not calling CompleteHandler() : %s",
                        ProcName,
                        id2string(idRequest)));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                              MCPErrorCode_Exception,
                                              "Not calling CompleteHandler()",
                                              "BeginHandler() Failed");
                }
        else MCPOutput = WorkItem->CompleteHandler();
        delete WorkItem;
        return MCPOutput;
        }

bool RunMCPDispatcher::DispatchWork()           // Runs in main thread.
        {
        static const char *ProcName = "RunMCPDispatcher::DispatchWork";
        bool Status = true;
        bool WorkPending = GetMessageCount() > 0; 
        for (unsigned i=0; WorkPending && i < NumMCPThreads; i++)
                {
                if (CurrentWorkItems[i]) continue;      // Thread available? 
                MCPAsyncWork *WorkItem = (MCPAsyncWork *)Receive(0);
                if (!WorkItem)                          // Strange, no work!
                        {
                        TERROR(("%s: State Error, no WorkItem",ProcName));
                        Status = false;
                        break;                   
                        }
                JSON_Value &idRequest = WorkItem->get_Id();
                TDEBUG(("%s: Dispatching to Thread %d Work Item: %s",
                        ProcName,
                        i,
                        id2string(idRequest)));
                Status = Threads[i].Send(WorkItem);
                if (!Status)
                        {
                        TERROR(("%s: Failed to Dispatch: %s",
                                ProcName,
                                id2string(idRequest)));
                        SendPriority(WorkItem);         // Return to queue.
                        break;
                        }
                CurrentWorkItems[i] = WorkItem;
                WorkPending = GetMessageCount() > 0;
                }
        return Status;
        }

int RunMCPDispatcher::FindWorkItem(JSON_Value &IdToFind)
        {
        for (unsigned i=0; i < NumMCPThreads; i++)
                {
                if (CurrentWorkItems[i] &&
                    CurrentWorkItems[i]->idcmp(IdToFind))
                        {
                        return i;
                        }
                }
        return -1;
        }

int RunMCPDispatcher::FindWorkItem(MCPAsyncWork *ToFind)
        {
        for (unsigned i=0; i < NumMCPThreads; i++)
                {
                if (CurrentWorkItems[i] && 
                    CurrentWorkItems[i] == ToFind)
                        {
                        return i;
                        }
                }
        return -1;
        }

void RunMCPDispatcher::OnMessageDestruct(void *Msg)
        {
        static const char *ProcName = "RunMCPDispatcher::OnMessageDestruct";
        // ------------------------------------------
        // ---- Cleanup work on Dispatcher Queue ----
        // ------------------------------------------
        MCPAsyncWork *WorkItem = (MCPAsyncWork *)Msg;
        JSON_Value &idRequest = WorkItem->get_Id();
        TINFO(("%s: Clearing%sWork Item: %s",ProcName,
                                               WorkItem->Cancelled
                                               ? " (Cancelled) "
                                               : " ",
                                               id2string(idRequest)));
        delete WorkItem;
        return;
        }

bool RunMCPDispatcher::StartThreads()
        {
        static const char *ProcName = "RunMCPDispatcher::StartThreads";
        bool Status = true;
        TINFO(("%s: %u MCPAsyncWorkers",ProcName,NumMCPThreads));
        for (unsigned i=0; Status && i < NumMCPThreads; i++)
                {
                Status = Threads[i].Start();
                if (!Status)
                        {
                        TERROR(("%s: Thread Start(%d) Failed",ProcName,i));
                        }
                }
        return Status;
        }

void RunMCPDispatcher::StopThreads(bool WaitToEnd)
        {
        static const char *ProcName = "RunMCPDispatcher::StopThreads";
        unsigned i;
        bool WorkPending = false;
        // ------------------------
        // ---- Signal Threads ----
        // ------------------------
        for (i=0; i < NumMCPThreads; i++)
                {
                Threads[i].Stop(false);
                }
        // ------------------
        // ---- End Work ----
        // ------------------
        for (i=0; i < NumMCPThreads; i++)
                {
                MCPAsyncWork *WorkItem = CurrentWorkItems[i];
                if (WorkItem) 
                        {
                        Threads[i].AbortWork();
                        WorkPending = true;
                        }
                }
        // --------------------------
        // ---- Wait for Threads ----
        // --------------------------
        if (WaitToEnd)
                {
                for (i=0; i < NumMCPThreads; i++)
                        {
                        Threads[i].Stop(WaitToEnd);
                        }
                }
        // ------------------------------------------
        // ---- Notify Residual "Work half done" ----
        //      ---------------------------------
        // Work might still enter Discard_async_work.
        // ------------------------------------------
        if (WorkPending) 
                {
                for (i=0; i < NumMCPThreads; i++)
                        {
                        MCPAsyncWork *WorkItem = CurrentWorkItems[i];
                        if (WorkItem)
                                {
                                JSON_Value &idRequest = WorkItem->get_Id();
                                TERROR(("%s: ZOMBIE Work Item (%s)",
                                        ProcName,
                                        id2string(idRequest)));
                                }
                        }
                }
        // --------------------------------------------
        // ---- Clear Queue -> OnMessageDestruct() ----
        // --------------------------------------------
        Clear();
        return;
        }

// ===========================================================================
// ==== Specific MCPAsyncWork Messages =======================================
// ===========================================================================

// -------------------------------------------------------------
// ---- IdMutex serializes set_Id (Thread) vs idcmp (Main). ----
// -------------------------------------------------------------
static CriticalSection IdMutex;

MCPAsyncWork::MCPAsyncWork(JSON_Value &inId, JSON_Object &inArgs) 
        : 
        Arguments(inArgs)
        {
        set_Id(inId);
        memcpy(EyeCatcher,"ERKASYNC",8);
        rcBeginHandler = false;
        Cancelled = false;
        return;
        }

void MCPAsyncWork::set_Id(JSON_Value &inId)
        {
        IdMutex.Take();
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
        IdMutex.Release();
        return;
        }

bool MCPAsyncWork::idcmp(JSON_Value &IdToFind)
        {
        IdMutex.Take();
        bool rc = ::idcmp(*Id,IdToFind);
        IdMutex.Release();
        return rc;
        }

bool MCPAsyncWork::AbortWork()
        {
        static const char *ProcName = "MCPServerAsync::AbortWork";
        TINFO(("%s: Don't know how to abort: %s",ProcName,
                                                 id2string(get_Id())));
        return false;
        }

// --------------------------------
// ---- MCPAsyncWork_MCPOutput ----
// --------------------------------

MCPAsyncWork_MCPOutput::~MCPAsyncWork_MCPOutput()
        {
        if (MCPOutput) 
                {
                free(MCPOutput);
                MCPOutput = NULL;
                }
        return;
        }

char* MCPAsyncWork_MCPOutput::CompleteHandler() 
        {
        char *rcMCPOutput = MCPOutput;
        MCPOutput = NULL;
        return rcMCPOutput;                     // Return MCPOutput to client.
        }

// ***************************************************************************
// ***************************************************************************
// **** System-wide Dispatcher ***********************************************
// ***************************************************************************
// ***************************************************************************

static RunMCPDispatcher Dispatcher;

static bool MCPServerAsyncInited = false;

static void Initialize()
        {
        static const char *ProcName = "MCPServerAsync::Initialize";
        if (MCPServer_Asynchronous)
                {
                TINFO(("%s: Starting Threads",ProcName));
                Dispatcher.StartThreads();
                MCPServerAsyncInited = true;
                }
        else TDEBUG(("%s: MCPServer is not Asynchronous",ProcName));
        return;
        }

static void Deinitialize()
        {
        static const char *ProcName = "MCPServerAsync::Deinitialize";
        if (MCPServerAsyncInited)
                {
                TINFO(("%s: Stopping Threads",ProcName));
                Dispatcher.StopThreads(true);
                TDEBUG(("%s: Threads Complete",ProcName));
                MCPServerAsyncInited = false;
                }
        return;
        }

// ***************************************************************************
// **** Handle Asynchronous Work *********************************************
// ******************************
//
//      Async work responses via MCPServer via RunMCPDispatcher.
//
// ***************************************************************************

bool _Begin_AsyncWork(MCPAsyncWork **WorkItem)
        {
        return Dispatcher.Begin_AsyncWork(WorkItem);
        }

char* RunMCPDispatcher::Handle_async_work(void *UserPtr)
        {
        static const char *ProcName = "Handle_async_work";
        char *MCPOutput = NULL;
        if (UserPtr)
                {
                MCPAsyncWork *WorkItem = (MCPAsyncWork *)UserPtr;
                JSON_Value &idRequest = WorkItem->get_Id();
                TDEBUG(("%s: Completing Work Item: %s",
                        ProcName,
                        id2string(idRequest)));
                MCPOutput = Dispatcher.Complete_SyncWork(WorkItem);
                }
        return MCPOutput;
        }

bool RunMCPDispatcher::Cancel_async_work(MCPInputRequest &MCPRequest)
        {
        // --------------------------------------------------
        // ---- Extract Id from notifications/cancelled ----
        // --------------------------------------------------
        JSON_Value *Id = MCPRequest.Get_param("requestId");
        // -------------------------------
        // ---- Attemp to Cancel Work ----
        // -------------------------------
        bool WorkFound = Id ? Dispatcher.Cancel_AsyncWork(*Id) : false;
        return WorkFound;
        }

void RunMCPDispatcher::Discard_async_work(void *UserPtr)
        {
        static const char *ProcName = "Discard_async_work";
        if (!UserPtr) return;
        // ---------------------------------------------------------
        // ---- Cleanup work responses on MCPServerIOMain Queue ----
        // ---------------------------------------------------------
        MCPAsyncWork *WorkItem = (MCPAsyncWork *)UserPtr;
        JSON_Value &idRequest = WorkItem->get_Id();
        TINFO(("%s: Clearing Work Response: %s",ProcName,
                                                id2string(idRequest)));
        delete WorkItem;
        return;
        }

// ***************************************************************************
// **** Enable MCPServerAsync - Setup Function Pointers **********************
// ***************************************************************************

// ----------------------------------------------
// ---- MCP Async Extension (External Hooks) ----
// ----------------------------------------------

extern void (*InitMCPAsyncHook)();
extern void (*DeinitMCPAsyncHook)();
extern char* (*Handle_async_object)(void *UserPtr);
extern void (*Discard_async_object)(void *UserPtr);
extern bool (*Cancel_async_object)(MCPInputRequest &MCPRequest);

struct _MCPServerAsyncInit
        {
        _MCPServerAsyncInit()
                {
                InitMCPAsyncHook = Initialize;
                DeinitMCPAsyncHook = Deinitialize;
                Handle_async_object = RunMCPDispatcher::Handle_async_work;
                Discard_async_object = RunMCPDispatcher::Discard_async_work;
                Cancel_async_object = RunMCPDispatcher::Cancel_async_work;
                }
        };

static _MCPServerAsyncInit MCPServerAsyncInit;

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
