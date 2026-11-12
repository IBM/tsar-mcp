// MCP Server for spellCheck: MCPServer_spellCheck.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServer_spellCheck.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Simple aspect to demonstrate Sampling.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPServerCore.h>
#include <MCPAsyncCore.h>
#include <MCPSampleEx.h>

const char *MCPServer_Name = "MCPServer_spellCheck";
const char *MCPServer_Version = "0.3.0";
const char *MCPServer_Capabilities = "\"tools\":{}";

bool MCPServer_Asynchronous = true;

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

static const char* MCPInputTestRequest_SpellCheck =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"spellCheck\","
         "   \"arguments\": { \"text\": \"This is a tset message with erors.\" }"
         " }"
         "}";

const char* MCPInputTestRequests[] =
        {
        MCPInputTestRequest_SpellCheck,
        NULL
        };

// ***************************************************************************
// **** Init / Shutdown ******************************************************
// ***************************************************************************

static char* spellCheck_tools_call_hook(MCPInputRequest &MCPRequest,
                                        JSON_Value &idRequest,
                                        const char *ToolName,
                                        JSON_Object &Arguments);

bool MCPServer_OnStartup()
        {
        static const char *ProcName = "MCPServer_OnStartup";
        TINFO(("%s: %s Startup",ProcName,MCPServer_Name));
        // --------------------------------------
        // ---- Set MCPServer Advanced Hooks ----
        // --------------------------------------
        Handle_tools_call_hook = spellCheck_tools_call_hook;
        return true;
        }

bool MCPServer_OnInitialize(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "MCPServer_OnInitialize";
        TINFO(("%s: %s Initialized",ProcName,MCPServer_Name));
        return true;
        }

bool MCPServer_OnShutdown()
        {
        static const char *ProcName = "MCPServer_OnShutdown";
        TINFO(("%s: %s Shutting Down...",ProcName,MCPServer_Name));
        return true;
        }

// ***************************************************************************
// **** MCP Directoy Handling ************************************************
// ***************************************************************************

const unsigned MCPToolInfoCount = 1;

MCPToolInfo_t MCPToolInfo[] =
        {
                // --------------------
                // ---- spellCheck ----
                // --------------------
                {
                // Name
                "spellCheck",
                // Description
                "Checks spelling in the provided text or markdown content. "
                "Returns the corrected version of the input text.",
                1,
                // InputSchema {Properties}
                        {
                        "\"text\":{\"type\":\"string\","
                                  "\"description\":"
                                  "\"The text or markdown content to spell-check\"}"
                        },
                // [Required]
                "\"text\""
                }
                // ---------------
                // ---- <end> ----
                // ---------------
        };

// ***************************************************************************
// **** Tool Handlers ********************************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- Handle_spellCheck ----------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_spellCheck(JSON_Value &idRequest, const char *Text)
        {
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "result": {
            "content": [
              {
                "type": "text",
                "text": "This is a test message with errors."
              }
            ],
            "isError": false
          }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Summary;
        // Return the corrected text (or original if sampling unavailable)
        Summary.printf("%s",Text);
        return FormMCPResponse_Text(&idRequest, Summary.GetBuffer());
        }

// ---------------------------------------------------------------
// ---- Define the work item (derives MCPAsyncWork_MCPOutput) ----
// ---------------------------------------------------------------

struct MCPAsyncWork_spellCheck : MCPAsyncWork_MCPOutput
        {
        MCPSyncSamplerEx Sampler;
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPAsyncWork_spellCheck(JSON_Value &inId, JSON_Object &inArgs)
                :
                MCPAsyncWork_MCPOutput(inId,inArgs),
                Sampler(inId)
                {
                return;
                }
        protected:
                virtual bool AbortWork()
                        {
                        Sampler.CancelWait();
                        return true;
                        }
                virtual bool BeginHandler();
        };

bool MCPAsyncWork_spellCheck::BeginHandler()             // Called in thread.
        {
        const char *ProcName = "MCPAsyncWork_spellCheck::BeginHandler";
        const char *Text = Arguments.Find_member_string("text");
        SamplerExEnder EndSamplingExit(Sampler);
        // -----------------------------------------
        // ---- Send Request and Wait for Reply ----
        // -----------------------------------------
        MCPInputRequest *Response = Sampler.BlockingSample(
                "You are a spell checker. Your task is to correct spelling "
                "errors in the provided text. Return ONLY the corrected text, "
                "with no explanations, in the same form as the original, "
                "and no additional commentary. Preserve the original "
                "capitalization and punctuation as much as possible.",
                Text,
                1024,
                30000);  // 30 second timeout
        AdoptResumeId(Sampler);
        if (!Response)
                {
                if (isCancelled())
                        {
                        TINFO(("%s: Sampling cancelled",ProcName));
                        return false;
                        }
                TERROR(("%s: Sampling failed",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&get_Id(),
                                  MCPErrorCode_Exception,
                                  "Sampling Failed",
                                  "Could not get LLM response");
                return MCPOutput != NULL;
                }
        // --------------------------------------------
        // ---- Extract Corrected Text from Result ----
        // --------------------------------------------
        const char *CorrectedText = ExtractSampleText(*Response);
        if (!CorrectedText)
                {
                TERROR(("%s: No text in sampling result",ProcName));
                delete Response;
                MCPOutput = FormMCPResponse_ERROR(&get_Id(),
                                  MCPErrorCode_Exception,
                                  "Sampling Failed",
                                  "No text in result content");
                return MCPOutput != NULL;
                }
        
        TINFO(("%s: Spell-check complete",ProcName));
        MCPOutput = FormMCPResponse_spellCheck(get_Id(),CorrectedText);
        delete Response;
        return MCPOutput != NULL;
        }

// -----------------------------------------------
// ---- Branch sync/async in the tool handler ----
// -----------------------------------------------

char* MCPtool_spellCheck(JSON_Value &idRequest,
                         JSON_Object &Arguments,
                         MCPInputRequest &MCPRequest)
        {
        const char *ProcName = "MCPtool_spellCheck";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "spellCheck",
            "arguments": { "text": "This is a tset message with erors." }
          }
        }
        ----------------------------------------------------------- */
        char *MCPOutput = NULL;
        const char *Text = Arguments.Find_member_string("text");
        if (!Text)
                {
                TERROR(("%s: Missing Argument: 'text'",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                        MCPErrorCode_InvalidParms,
                                        "Invalid Params",
                                        "Missing parameter: text");
                }
        else if (!MCPServer_Asynchronous)               // Sync path.
                {
                TERROR(("%s: Asynchronous mode required for sampling",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                MCPErrorCode_Exception,
                                "Async Required",
                                "Spell-checking requires asynchronous mode");
                }
        else    {                                       // Async path.
                MCPAsyncWork_spellCheck *WorkItem;
                WorkItem = new MCPAsyncWork_spellCheck(idRequest, Arguments);
                if (WorkItem)
                        {
                        bool isMRTRSample = isMRTRSamplingSupported(MCPRequest);
                        WorkItem->Sampler.SetMRTRSupported(isMRTRSample);
                        bool Status = Begin_AsyncWork(&WorkItem);
                        if (Status)
                                {
                                MCPOutput = Return_MCPOutput_Pending();
                                }
                        else    {
                                if (WorkItem) delete WorkItem;
                                TERROR(("%s: Begin_AsyncWork Failed",ProcName));
                                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_Exception,
                                                  "Can't Schedule Work",
                                                  "Begin_AsyncWork() Failed");
                                }
                        }
                }
        return MCPOutput;
        }

// ---------------------------------------------------------------------------
// ---- Handle_tools_call (advanced hook) ------------------------------------
// ---------------------------------------------------------------------------

static char* spellCheck_tools_call_hook(MCPInputRequest &MCPRequest,
                                        JSON_Value &idRequest,
                                        const char *ToolName,
                                        JSON_Object &Arguments)
        {
        char *MCPOutput = NULL;
        if (strcmp(ToolName,"spellCheck") == 0)
                {
                MCPOutput = MCPtool_spellCheck(idRequest,Arguments,MCPRequest);
                }
        return MCPOutput; // Others via Handle_tools_call.
        }

char* Handle_tools_call(JSON_Value &idRequest,
                        const char *ToolName, 
                        JSON_Object &Arguments)
        {
        return NULL;                            // spellCheck dispatches via hook.
        }

// ****************************************************************************
// **** Handle_notification (Dormant) *****************************************
// ****************************************************************************

void Handle_notification(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "Handle_notification";
        const char *Method = MCPRequest.Get_method();
        TDEBUG(("%s: Ignoring: %s",ProcName,Method));
        return;
        }

// ****************************************************************************
// **** Handle_sampling_response (Dormant) ***********************************
// ****************************************************************************

char* Handle_sampling_response(MCPInputRequest &MCPRequest)
        {
        return NULL;
        }

// ****************************************************************************
// **** Handle_tools_call_resume (Dormant) ************************************
// ****************************************************************************

char* Handle_tools_call_resume(MCPInputRequest &MCPRequest)
        {
        return NULL;
        }

// ****************************************************************************
// **** Handle_notify_action (Dormant) ****************************************
// ****************************************************************************

char* Handle_notify_action(void *UserPtr)
        {
        return NULL;
        }

void Discard_notify_action(void *UserPtr)
        {
        return;
        }

// ****************************************************************************
// **** Handle_completion_complete (Dormant) *********************************
// ****************************************************************************

char* Handle_completion_complete(JSON_Value &idRequest,
                                 const char *RefType,
                                 const char *RefName,
                                 const char *ArgName,
                                 const char *ArgValue)
        {
        return NULL;
        }

// ****************************************************************************
// **** Handle_prompts_get (Dormant) ******************************************
// ****************************************************************************

const unsigned MCPPromptInfoCount = 0;

MCPPromptInfo_t MCPPromptInfo[] = { {0} };

char* Handle_prompts_get(JSON_Value &idRequest,
                         const char *PromptName, 
                         JSON_Object &Arguments)
        {
        return NULL;
        }

// ****************************************************************************
// **** Resources (Dormant) ***************************************************
// ****************************************************************************

const unsigned MCPResourceInfoCount = 0;

MCPResourceInfo_t MCPResourceInfo[] = { {0} };

const unsigned MCPResourceTemplateInfoCount = 0;

MCPResourceTemplateInfo_t MCPResourceTemplateInfo[] = { {0} };

char* Handle_resources_read(JSON_Value &idRequest,
                            const char *URI)
        {
        return NULL;
        }

char* Handle_resources_subscribe(JSON_Value &idRequest,
                                 const char *URI)
        {
        return NULL;
        }

char* Handle_resources_unsubscribe(JSON_Value &idRequest,
                                   const char *URI)
        {
        return NULL;
        }

// ***************************************************************************
// **** Handle_method (Dormant) *********************************************
// ***************************************************************************

char* Handle_method(MCPInputRequest &MCPRequest)
        {
        return NULL;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
