// MCP Server Async Feature: MCPAsyncCore.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPAsyncCore.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// ENABLE: LINK MCPAsyncServer with MCPServer and set MCPServer_Asynchronous.
//
// Flow: 
//      1. MCPClient sends a MCP tool request (or resources/read).
//      2. Handle_tools_call (or Handle_resources_read) dispatches.
//      3. A handler can return an MCPOutput directly,
//         or:
//              a. Derive a work item instance of MCPAsyncWork
//              b. Send the work item to Begin_AsyncWork() and
//                 return Return_MCPOutput_Pending() from the tool
//                 request handler:
//                      {
//                      bool Status = Begin_AsyncWork(&WorkItem);
//                      if (Status) MCPOutput = Return_MCPOutput_Pending();
//                      else    {
//                              if (WorkItem) delete WorkItem;
//                              MCPOutput = FormMCPResponse_ERROR();
//                              }
//                      }
//              c. Process the tool call in BeginHandler() (in thread)
//                      i. Do long-running work.
//                     ii. Fill in output details into the work item.
//              d. Again in main, CompleteHandler() should return MCPOutput.
//
// Abbreviated Flow: 
//      3.a.     Derive a work item instance of MCPAsyncWork_MCPOutput 
//        c.ii.  Fill in MCPOutput into the work item.
//        d.     Done: CompleteHandler() automatically returns MCPOutput.
//
// Note on MCPAsyncWork for resource/read:
//
//      The derived MCPAsyncWork would pass an empty JSON_Object:
//
//              static JSON_Object _emptyJSONObject;
//
//      into the constructor and would need to save off inURI itself.
//

#ifndef __MCPServerAsync_Core

        #define __MCPServerAsync_Core

#include <MCPServerCore.h>

// ***************************************************************************
// **** Asynchronous MCPWork Objects for MCPServer ***************************
// ***************************************************************************

// -------------------------------
// ---- MCPAsyncWork Messages ----
// -------------------------------

struct MCPAsyncWork             // [Work] sent to thread by Begin_AsyncWork().
        {                       // Work performed in BeginHandler().
        char EyeCatcher[8];     // [Result] sent to client CompleteHandler().
        // -------------------- 
        // ---- Input Args ----
        // --------------------
        JSON_Object Arguments;          // Local copy.
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPAsyncWork(JSON_Value &inId, JSON_Object &inArgs); 
        virtual ~MCPAsyncWork() {}
        private:
                JSON_Value *Id;         // Points to sId or nId or zId.
                JSON_Value_String sId;  // Id local storage.
                JSON_Value_Number nId;
                JSON_Value_Null zId;
                bool idcmp(JSON_Value &IdToFind);      // Thread-safe compare.
                // ----------------
                // ---- Output ----
                // ----------------
                volatile bool Cancelled;        // via AbortWork() processing.
                bool rcBeginHandler;
                friend class RunMCPThread;
                friend class RunMCPDispatcher;
        protected:
                // -----------------------------------------------------------
                // ---- get_Id / set_Id : Get current Id / Adopt a new id ----
                // -----------------------------------------------------------
                // Note: When a Multi-Round-Trip (input_required) sequence is 
                //       begun, the the MCP client receives a response to the 
                //       work item with the current Id (get_Id()). The resume 
                //       response is returned to the MCPServer with a new Id. 
                //       Use set_Id() to adopt the new Id.
                // -----------------------------------------------------------
                JSON_Value& get_Id() {return *Id;}
                void set_Id(JSON_Value &inId);          // Thread-safe set.
                // -----------------------------------------------------------
                // Note: A derived class defines BeginHandler() and
                //       CompleteHandler(). BeginHandler() must set result
                //       information for CompleteHandler(). Allow the destructor
                //       to clean up any output details (e.g. free memory).
                //
                //       Note: If BeginHandler() fails to return 'true' then 
                //       CompleteHandler() won't be invoked before the instance
                //       of MCPAsyncWork is reaped in the main thread.
                //
                //       AbortWork() should if it can (e.g. ending a child process
                //       at the other end of pipe). AbortWork() will only be called 
                //       when the thread is presently running in BeginHandler().
                //       Whether or not AbortWork() succeeds, CompleteHandler() 
                //       will be skipped and no response sent to the client.
                // -----------------------------------------------------------
                virtual bool AbortWork();
                virtual bool BeginHandler() = 0;        // Called in Thread.
                virtual char* CompleteHandler() = 0;    // Called in Main.
                bool isCancelled() {return Cancelled;}
        };

struct MCPAsyncWork_MCPOutput : MCPAsyncWork      // Simplified Handler
        {
        // ----------------
        // ---- Output ----
        // ----------------
        char *MCPOutput;
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPAsyncWork_MCPOutput(JSON_Value &inId, JSON_Object &inArgs) 
                : 
                MCPAsyncWork(inId,inArgs)
                {
                MCPOutput = NULL;
                return;
                }
        virtual ~MCPAsyncWork_MCPOutput();
        protected:
                //
                // Note: A derived class only needs to define BeginHandler()
                //       which should set MCPOutput upon exit.
                //
                virtual char* CompleteHandler();
        };
                         
bool _Begin_AsyncWork(MCPAsyncWork **WorkItem);

#define Begin_AsyncWork(WorkItem) _Begin_AsyncWork((MCPAsyncWork**)WorkItem)

// ----------------------------------------------------------
// ---- AdoptResumeId() -------------------------------------
// ---------------------
//
//      Helper for updating for the MCPAsyncWork::Id after:
//
//              MCPSyncSamplerEx::BlockingSample() or 
//              MCPSyncRoundTrip::BlockingRequest()
//      
#define AdoptResumeId(Source) set_Id((Source).get_resumeId())

/* ***************************************************************************
 Notes:

        **********************************************  
        **** Handling Progress via Advanced Hooks ****
        **********************************************

        The standard Handle_tools_call() signature does not receive the
        full MCPInputRequest (and thus no access to _meta.progressToken).
        An advanced hook lets you intercept specific tools that need it.
        See MCPServerCore.h "Advanced Hooks" for the dispatch pattern.

        Steps:

        1. Derive a work item that carries the progress token:

           struct MCPAsyncWork_withProgress : MCPAsyncWork_MCPOutput
                   {
                   AutoString ProgressToken;
                   MCPAsyncWork_withProgress(JSON_Value &inId, 
                                             JSON_Object &inArgs,
                                             const char *inToken)
                           :
                           MCPAsyncWork_MCPOutput(inId,inArgs),
                           ProgressToken(inToken)
                           {
                           return;
                           }
                   protected:
                           virtual bool BeginHandler()
                                   {
                                   // Post progress during work.
                                   PostProgress(ProgressToken,1,3,
                                                "Connecting...");
                                   // ... do work ...
                                   PostProgress(ProgressToken,2,3,
                                                "Processing...");
                                   // ... do work ...
                                   PostProgress(ProgressToken,3,3,
                                                "Formatting...");
                                   MCPOutput = FormMCPResponse_...(get_Id(),...);
                                   return MCPOutput != NULL;
                                   }
                   };

        2. Define a hook handler for tools that need progress:

           char* Handle_tools_call_advanced(MCPInputRequest &MCPRequest,
                                            JSON_Value &idRequest,
                                            const char *ToolName, 
                                            JSON_Object &Arguments)
                   {
                   char *MCPOutput = NULL;
                   if (strcmp(ToolName,"longRunningTool") == 0)  
                           {
                           // ---------------------------------
                           // Extract progressToken from _meta.
                           // ---------------------------------
                           const char *Token = NULL;
                           JSON_Object *Meta;
                           Meta = MCPRequest.Get_param_object("_meta");
                           if (Meta)
                                   {
                                   Token = Meta->Find_member_string(
                                                   "progressToken");
                                   }   
                           // ----------------------------
                           // Create work item with token.
                           // ----------------------------
                           MCPAsyncWork_withProgress *Work;
                           Work = new MCPAsyncWork_withProgress(idRequest,
                                                                Arguments,
                                                                Token);
                           if (Work)
                                   {
                                   bool Status = Begin_AsyncWork(&Work);
                                   if (Status)
                                           {
                                           MCPOutput = Return_MCPOutput_Pending();
                                           }
                                   else    {
                                           if (Work) delete Work;
                                           MCPOutput = FormMCPResponse_ERROR(...);
                                           }
                                   }
                           }
                   return MCPOutput; // NULL = fall through to Handle_tools_call.
                   }

        3. Register the hook at startup:

           bool MCPServer_OnStartup()
                   {
                   Handle_tools_call_hook = Handle_tools_call_advanced;
                   return true;
                   }

        4. PostProgress() builds and sends a progress notification:

           bool PostProgress(const char *ProgressToken,
                             double Progress,
                             double Total,
                             const char *Message)
                   {
                   if (!ProgressToken) return false; // No-op.
                   MemoryPrintf Canvas;
                   Canvas.printf("{");
                   Canvas.printf("\"jsonrpc\":\"2.0\",");
                   Canvas.printf("\"method\":\"notifications/progress\",");
                   Canvas.printf("\"params\":{");
                   Canvas.printf("\"progressToken\":\"%s\",",ProgressToken);
                   Canvas.printf("\"progress\":%g",Progress);
                   if (Total > 0) Canvas.printf(",\"total\":%g",Total);
                   if (Message) Canvas.printf(",\"message\":\"%s\"",Message);
                   Canvas.printf("}"); // params
                   Canvas.printf("}");
                   char *MCPOutput = Canvas.AquireBuffer();
                   return PostMCPOutput(&MCPOutput);
                   }

        **********************************************************
        **** Updating MCPExternToolInfo / MCPExternPromptInfo ****
        **********************************************************

        For dynamic tool and prompt lists, update MCPExternToolInfo and
        MCPExternToolInfoCount (and the Prompt equivalents) only in the 
        main thread (i.e. via CompleteHandler()). CompleteHandler() could 
        then use FormMCPNotification_ToolsListChanged() to notify clients 
        of the change.

******************************************************************************
******************************************************************************
**** Example 1: helloWorld Async *********************************************
**********************************
   MCPAsyncWork_MCPOutput is the simplest path. A derived class only
   needs to implement BeginHandler() which sets MCPOutput. The inherited
   CompleteHandler() returns it to the client automatically.

   Note: Arguments is a local copy (not a reference), so
         Arguments.Find_member_string() is safe in BeginHandler().
******************************************************************************

#include <MCPAsyncCore.h>

// ------------------------------------------------------------------
// ---- 1. Define the work item (derives MCPAsyncWork_MCPOutput) ----
// ------------------------------------------------------------------

struct MCPAsyncWork_helloWorld : MCPAsyncWork_MCPOutput
        {
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPAsyncWork_helloWorld(JSON_Value &inId, JSON_Object &inArgs)
                :
                MCPAsyncWork_MCPOutput(inId,inArgs) 
                {
                return;
                }
        protected:
                virtual bool BeginHandler()             // Called in thread.
                        {
                        const char *Name = Arguments.Find_member_string("name");
                        MCPOutput = FormMCPResponse_helloWorld(get_Id(), Name);
                        return MCPOutput != NULL;
                        }
        };

// --------------------------------------------------
// ---- 2. Branch sync/async in the tool handler ----
// --------------------------------------------------

char* MCPtool_helloWorld(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        char *MCPOutput = NULL;
        const char *Name = Arguments.Find_member_string("name");
        if (!Name)
                {
                TERROR(("%s: Missing Argument: 'name' (id=%s)",
                        ProcName,
                        id2string(idRequest)));
                MCPOutput = FormMCPResponse_ERROR(&idRequest, ...);
                }
        else if (!MCPServer_Asynchronous)               // Sync path.
                {
                MCPOutput = FormMCPResponse_helloWorld(idRequest, Name);
                }
        else    {                                       // Async path.
                MCPAsyncWork_helloWorld *WorkItem;
                WorkItem = new MCPAsyncWork_helloWorld(idRequest, Arguments);
                if (WorkItem)
                        {
                        bool Status = Begin_AsyncWork(&WorkItem);
                        if (Status)
                                {
                                MCPOutput = Return_MCPOutput_Pending();
                                }
                        else    {
                                if (WorkItem) delete WorkItem;
                                TERROR(("%s: Begin_AsyncWork Failed (id=%s)",
                                        ProcName,
                                        id2string(idRequest)));
                                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_Exception,
                                                  "Can't Schedule Work",
                                                  "Begin_AsyncWork() Failed");
                                }
                        }
                }
        return MCPOutput;
        }

******************************************************************************
**** Example 2: Full MCPAsyncWork (BeginHandler + CompleteHandler) ***********
*******************************************************************
   When BeginHandler() produces raw results and CompleteHandler()
   builds the MCPOutput on the main thread. Better for complex
   result objects (e.g. SAPControlResultSet*).

   The tool handler (step 2) is identical -- only the work item differs.
******************************************************************************

// --------------------------------------------------------
// ---- 1. Define the work item (derives MCPAsyncWork) ----
// --------------------------------------------------------

struct MCPAsyncWork_helloWorld2 : MCPAsyncWork
        {
        // ----------------
        // ---- Output ----
        // ----------------
        char *Greeting;
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPAsyncWork_helloWorld2(JSON_Value &inId, JSON_Object &inArgs)
                : MCPAsyncWork(inId,inArgs)
                {
                Greeting = NULL;
                }
        virtual ~MCPAsyncWork_helloWorld2()
                {
                if (Greeting) free(Greeting);
                }
        protected:
                virtual bool BeginHandler()             // Called in thread.
                        {
                        const char *Name = Arguments.Find_member_string("name");
                        MemoryPrintf Canvas;
                        Canvas.printf("How are you, %s.", Name);
                        Greeting = Canvas.AquireBuffer();
                        return Greeting != NULL;
                        }
                virtual char* CompleteHandler()          // Called in main.
                        {
                        return FormMCPResponse_helloWorld(get_Id(),Greeting);
                        }
        };

*****************************************************************************/

#endif
