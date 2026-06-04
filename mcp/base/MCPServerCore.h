// MCP Server : MCPServer.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServerCore.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Define the following symbols for an MCPServer aspect:
//
//              MCPServer_Name
//              MCPServer_Version
//              MCPServer_Capabilities          - "" or "\"tools\":{...}" or etc.
//              MCPServer_Asynchronous          - to support Notify.
//
//              MCPServer_OnInitialize          - at MCP Client Initialize.
//              MCPServer_OnShutdown            - after message handling.
//
//              MCPToolInfoCount                - Tools it provides.  
//              MCPToolInfo[]
//
//              MCPPromptInfoCount              - Prompts it provides.
//              MCPPromptInfo[]
//
//              MCPResourceInfoCount            - Resources it provides.
//              MCPResourceInfo[]
//              MCPResourceTemplateInfoCount    - Resource templates.
//              MCPResourceTemplateInfo[]
//
//              Handle_tools_call               - MCP Client Pulls.
//              Handle_prompts_get              - MCP Client Pulls.
//              Handle_completion_complete      - MCP Client Pulls.
//              Handle_resources_read           - MCP Client Pulls.
//              Handle_resources_subscribe      - MCP Client Pulls.
//              Handle_resources_unsubscribe    - MCP Client Pulls.
//              Handle_notification             - Can ignore.
//              Handle_sampling_response        - Match Id from request.
//              Handle_method                   - resources, logging, etc.
//
//              Handle_notify_action            - MCP Server Pushes (w/struct).
//              Discard_notify_action           - Unhandled Pushes.
//
//      Define test cases:
//
//              MCPInputTestRequests[] = {NULL} - Null Terminated list of JSON.
//
// NOTICE: Function handlers returning an "MCPOutput" JSON buffer
//         must assume it will be freed by the caller.
//

#ifndef __MCPServer_Core

        #define __MCPServer_Core

#include <time.h>
#include <MCPServerJSON.h>

extern const char *MCPServer_Name;
extern const char *MCPServer_Version;
extern const char *MCPServer_Capabilities;      // Additional capabilities.

extern bool MCPServer_Asynchronous;             // Notify support requires.

// *************************
// **** MCP Error Codes ****
// *************************

#define MCPErrorCode_ParseError -32700
#define MCPErrorCode_InvalidRequest -32600
#define MCPErrorCode_NoMethod -32601
#define MCPErrorCode_InvalidParms -32602
#define MCPErrorCode_Exception -32603
#define MCPErrorCode_ServerDefMin -32000
#define MCPErrorCode_ServerDefMax -32099

// ***************************************************************************
// **** Share Program Arguments **********************************************
// *****************************
//
//      These are the inputs passed into main(argc, argv).
//
// ***************************************************************************

int MCPServer_getArgc();
const char** MCPServer_getArgv();

// ***************************************************************************
// **** Utilties *************************************************************
// ***************************************************************************

// --------------------------
// ---- String Utilities ----
// --------------------------

const char* MCP_strtok(const char *Start, 
                       size_t *Length, 
                       const char *Delimiters=" ,");
                       
const char* MCP_ltrim(const char *Start, size_t *Length);

size_t* MCP_rtrim(const char *Start, size_t *Length);

#define MCP_trim(Start,Length)  MCP_rtrim(Start, MCP_ltrim(Start, Length))
// Use: char *Token;
//      size_t TokenLength = 0;
//      Token = MCP_strtok(Input, &TokenLength);
//      Token = MCP_trim(Token, &TokenLength);
//      Token[TokenLength] = '\0';

// --------------------------------
// ---- Threadsafe localtime() ----
// --------------------------------

struct tm* MCP_localtime(struct tm *tmTime, time_t tTime);

// ***************************************************************************
// **** Init / Shutdown ******************************************************
// ***************************************************************************

bool MCPServer_OnInitialize(MCPInputRequest &MCPRequest);
bool MCPServer_OnShutdown();

// ***************************************************************************
// **** JSON Output Responses ************************************************
// ****************************
//
//      FormMCPResponse_Text() returns an empty result:{} if Text is NULL.
//
// ***************************************************************************

char* FormMCPResponse_Text(JSON_Value *idRequest, 
                           const char *Text, 
                           bool isError=false);
                                                                    
char* FormMCPResponse_ERROR(JSON_Value *idRequest,
                            int Code, 
                            const char *ErrorTag, 
                            const char *ErrorFormat,
                            ...);

// ***************************************************************************
// **** MCP Tool Directoy Handling *******************************************
// ***************************************************************************

struct MCPToolInfo_t  
        {
        const char *Name;
        const char *Description;        // No embeded newlines or control.
        unsigned nProperties;
        const char* Properties[16];
        const char *Required;
        };

extern const unsigned MCPToolInfoCount;

extern MCPToolInfo_t MCPToolInfo[];

// ***************************************************************************
// **** MCP Prompt Directory Handling ****************************************
// ***************************************************************************

struct MCPPromptInfo_t
        {
        const char *Name;
        const char *Description;        // Markdown, "C\n" escaped control.
        unsigned nArguments;            // Each Arg: {"name":"x",
        const char* Arguments[16];      //            "description":"y",
        };                              //            "required":true} or {0}.

extern const unsigned MCPPromptInfoCount;

extern MCPPromptInfo_t MCPPromptInfo[];

// ***************************************************************************
// **** MCP Resource Directory Handling **************************************
// ***************************************************************************

struct MCPResourceInfo_t
        {
        const char *URI;                // e.g. "file:///logs/server.log"
        const char *Name;               // Human-readable display name.
        const char *Description;        // What this resource contains.
        const char *MimeType;           // e.g. "text/plain" (or NULL).
        };

extern const unsigned MCPResourceInfoCount;

extern MCPResourceInfo_t MCPResourceInfo[];

struct MCPResourceTemplateInfo_t
        {
        const char *URITemplate;        // e.g. "file:///logs/{name}.log"
        const char *Name;               // Human-readable display name.
        const char *Description;        // What this template resolves to.
        const char *MimeType;           // e.g. "text/plain" (or NULL).
        };

extern const unsigned MCPResourceTemplateInfoCount;

extern MCPResourceTemplateInfo_t MCPResourceTemplateInfo[];

// ***************************************************************************
// **** Tool Handlers ********************************************************
// ***************************************************************************

char* Handle_tools_call(JSON_Value &idRequest,
                        const char *ToolName, 
                        JSON_Object &Arguments);        // Return MCPOutput.

// ---------------------------------------
// ---- Advanced Hook (Set to Enable) ----
// ---------------------------------------

extern char* (*Handle_tools_call_hook)(MCPInputRequest &MCPRequest,
                                       JSON_Value &idRequest,
                                       const char *ToolName, 
                                       JSON_Object &Arguments);

// ***************************************************************************
// **** Prompt Handlers ******************************************************
// ***************************************************************************

char* FormMCPResponse_prompt(JSON_Value &idRequest,
                             const char *Description,
                             const char *PromptText);

char* Handle_prompts_get(JSON_Value &idRequest,
                         const char *PromptName, 
                         JSON_Object &Arguments);       // Return MCPOutput.

// ---------------------------------------
// ---- Advanced Hook (Set to Enable) ----
// ---------------------------------------

extern char* (*Handle_prompts_get_hook)(MCPInputRequest &MCPRequest,
                                        JSON_Value &idRequest,
                                        const char *PromptName, 
                                        JSON_Object &Arguments);

// ***************************************************************************
// **** Completion Handlers **************************************************
// ***************************************************************************

char* FormMCPResponse_completion(JSON_Value &idRequest,
                                 unsigned nValues,
                                 const char **Values);

char* Handle_completion_complete(JSON_Value &idRequest,
                                 const char *RefType,
                                 const char *RefName,
                                 const char *ArgName,    // Return MCPOutput,
                                 const char *ArgValue);  // or NULL for empty.

// ---------------------------------------
// ---- Advanced Hook (Set to Enable) ----
// ---------------------------------------

extern char* (*Handle_completion_complete_hook)(MCPInputRequest &MCPRequest,
                                                JSON_Value &idRequest,
                                                const char *RefType,
                                                const char *RefName,
                                                const char *ArgName,
                                                const char *ArgValue);

// ------------------------------------------------------------
// ---- Utility: Case insenstive strstr - Completion match ----
// ------------------------------------------------------------

const char* MCP_stristr(const char *String, const char *ToFind);

// ***************************************************************************
// **** Resource Handlers ****************************************************
// ************************
//
//      The core handles resources/list and resources/templates/list
//      automatically from MCPResourceInfo[] and MCPResourceTemplateInfo[].
//
//      The aspect implements:
//              Handle_resources_read()        - return resource content.
//              Handle_resources_subscribe()   - track subscription state.
//              Handle_resources_unsubscribe() - clear subscription state.
//
//      Use FormMCPResponse_resource() to build the read response.
//      Return NULL from subscribe/unsubscribe to reject unknown URIs
//      (the core returns an error). Return an empty result MCPOutput 
//      to acknowledge (e.g. FormMCPResponse_Text(&idRequest,NULL)).
//
//      PostResourceUpdated() sends a notifications/resources/updated
//      message to the client. It calls PostMCPOutput() internally,
//      which requires MCPServer_Asynchronous = true (async message
//      loop). In synchronous mode, PostMCPOutput is unavailable and
//      the notification will not be delivered.
//
//      To process Handle_resources_read asynchronously, return
//      Return_MCPOutput_Pending() and handle the async work in a thread 
//      or derived MCPAsyncWork instance.
//
// ***************************************************************************

char* FormMCPResponse_resource(JSON_Value &idRequest,
                               const char *URI,
                               const char *MimeType,
                               const char *Text);

char* Handle_resources_read(JSON_Value &idRequest,
                            const char *URI);           // Return MCPOutput.

char* Handle_resources_subscribe(JSON_Value &idRequest,
                                 const char *URI);      // Return MCPOutput.

char* Handle_resources_unsubscribe(JSON_Value &idRequest,
                                   const char *URI);    // Return MCPOutput.

// -----------------------------------------
// ---- Resource Updated Notification   ----
// -----------------------------------------

char* FormMCPNotification_ResourceUpdated(const char *URI);

bool PostResourceUpdated(const char *URI);

// ***************************************************************************
// **** Notifications Handlers ***********************************************
// ***************************************************************************

void Handle_notification(MCPInputRequest &MCPRequest);

// ***************************************************************************
// **** Sampling Response Handlers *******************************************
// *********************************
//
//      The concept is: At a client request (e.g. tools/call), the server
//      can decide to return an MCPOutput with a sampling request INSTEAD
//      of a response. The aspect implmenting Handle_sampling_response() 
//      would store whatever is necessary such that when the sampling
//      response returned, it could key off the idRequest and return the
//      response to it initial client request.
//
//              1. Client -> Server: tools/call
//              2. Client <- Server: sampling/createMessage (w/unique id)
//              3. Client -> Server: response invokes Handle_sampling_response
//              4. Client <- Server: result
//
// ***************************************************************************

char* FormMCPRequest_sampling(JSON_Value &idRequest,
                              const char *SystemPrompt,
                              const char *UserMessage,
                              int MaxTokens);

char* Handle_sampling_response(MCPInputRequest &MCPRequest); // Ret MCPOutput.

// ***************************************************************************
// **** Notify Action ********************************************************
// ********************
//
//      Only active when MCPServer is running in Asynchronious mode.
//
//      Two ways to send a deferred response from a worker thread:
//
//                              (A) PostNotifyAction  (B) PostMCPOutput
//                              --------------------  -----------------
//      Thread builds:          Raw result struct     Complete MCPOutput
//      Main thread runs:       Handle_notify_action  Validate and send
//      Aspect implements:      Handle + Discard      (nothing extra)
//      Best for:               Complex/multi-step    Simple fire-and-forget
//
//      Note: (B) caller must include the correct JSON-RPC id.
//
//      Note: Notify servers may be started in MCPServer_OnInitialize()
//            and ended in: MCPServer_OnShutdown().
//
// ***************************************************************************

bool PostNotifyAction(void *UserPtr);           // Initate "Notify" (A)

char* Handle_notify_action(void *UserPtr);      // Return MCPOutput.  (A)

void Discard_notify_action(void *UserPtr);      // Free unscheduled.  (A)

// ***************************************************************************
// **** Other Method Handlers ************************************************
// ***************************
//
//      Tools, prompts, and resources have dedicated handlers above.
//      To support additional capabilities (e.g. logging), implement 
//      Handle_method() in your aspect and set MCPServer_Capabilities.
//      For example: MCPServer_Capabilities = "\"tools\":{},"
//                                            "\"prompts\":{"
//                                                "\"listChanged\":true"
//                                            "},"
//                                            "\"resources\":{"
//                                                "\"subscribe\":true,"
//                                                "\"listChanged\":true"
//                                            "},"
//                                            "\"logging\":{}";
//
// ***************************************************************************

char* Handle_method(MCPInputRequest &MCPRequest);       // Return MCPOutput.

// ***************************************************************************
// **** Return_MCPOutput_Pending *********************************************
// *******************************
//
//      For tools that take too long to respond synchronously:
//
//              1. Set MCPServer_Asynchronous = true in your aspect.
//              2. In Handle_tools_call(), save idRequest, start a thread,
//                 and return Return_MCPOutput_Pending().
//
//      Then EITHER:
//
//         (A) PostNotifyAction - struct-based, main-thread formatting:
//              3. Thread calls PostNotifyAction(ptr) where ptr
//                 carries the original idRequest and result.
//              4. Handle_notify_action() builds and returns MCPOutput.
//              5. Discard_notify_action() frees ptr on shutdown.
//
//         (B) PostMCPOutput - direct, thread builds full response:
//              3. Thread builds the complete MCPOutput JSON 
//                 (must include the correct JSON-RPC id).
//              4. Thread calls PostMCPOutput(MCPOutput).
//                 No Handle/Discard handlers needed.
//
//      See: MCPServer.cpp - "Return_MCPOutput_Pending" for details.
//
// ***************************************************************************

char* Return_MCPOutput_Pending();               // Special "MCPOutput" value.

bool PostMCPOutput(char **MCPOutput);           // Send MCPOutput to client.

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

extern const char* MCPInputTestRequests[];  // Array of requests, NULL term.
                                            // to run a test call at: 
                                            // C:\> MCPServer Test

/* ***************************************************************************
 Notes:

        ************************
        **** Advanced Hooks ****
        ************************
     
        Three optional function-pointer hooks let an aspect intercept the
        full MCPInputRequest before the standard handlers see it. Each hook
        defaults to NULL (disabled). Set them in MCPServer_OnInitialize().
     
        Pattern:
             1. MCPServer.cpp extracts Id, ToolName/PromptName, Arguments
                from the MCPInputRequest as usual for validation.
             2. If the hook pointer is non-NULL, it is called FIRST with
                the full MCPInputRequest plus the already-extracted fields.
             3. If the hook returns MCPOutput (non-NULL), that response is
                used and the standard handler is skipped.
             4. If the hook returns NULL, dispatch falls through to the
                standard Handle_tools_call / Handle_prompts_get /
                Handle_completion_complete handler.
     
        This lets an aspect selectively intercept specific tools (e.g.
        those needing _meta access for progressToken) while leaving all
        other tools to the simple handler path.
     
        Hooks:
             Handle_tools_call_hook
             Handle_prompts_get_hook
             Handle_completion_complete_hook
     
        Use case: MCP Progress Notifications
             A hook handler can extract _meta.progressToken from the
             MCPInputRequest, pass it into an MCPAsyncWork-derived struct,
             and have BeginHandler() call PostMCPOutput() with progress
             JSON during long-running operations.
             See MCPAsyncCore.h "Handling Progress" for the full pattern.

******************************************************************************

*****************************************************************************/

#endif
