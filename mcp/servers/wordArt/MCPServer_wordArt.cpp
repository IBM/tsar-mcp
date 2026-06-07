// MCP Server for wordArt: MCPServer_wordArt.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServer_wordArt.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Basic Sample: Demonstrates LLM sampling using only the core
//      MCPServerCore.h primitives.
//
//      Sampling Flow:
//              1. Client -> Server: tools/call (wordArt) [request_id]
//              2. Server -> Client: sampling/createMessage [sample_id]
//              3. Client -> Server: sampling response (LLM output) [sample_id]
//              4. Server -> Client: tool result (ASCII art) [request_id]
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPServerCore.h>

const char *MCPServer_Name = "MCPServer_wordArt";
const char *MCPServer_Version = "0.1.0";
const char *MCPServer_Capabilities = "\"tools\":{}";

bool MCPServer_Asynchronous = false;

#define MAXACTIVESAMPLINGS 4                    // Number active at one time.

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

static const char* MCPInputTestRequest_WordArt =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"wordArt\","
         "   \"arguments\": { \"text\": \"Hello\", \"style\": \"big\" }"
         " }"
         "}";

const char* MCPInputTestRequests[] =
        {
        MCPInputTestRequest_WordArt,
        NULL
        };

// ***************************************************************************
// **** Init / Shutdown ******************************************************
// ***************************************************************************

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
                // -----------------
                // ---- wordArt ----
                // -----------------
                {
                // Name
                "wordArt",
                // Description
                "Renders the provided text as an artistic ASCII graphic using "
                "the specified artistic style. Returns the ASCII art block.",
                2,
                // InputSchema {Properties}
                        {
                        "\"text\":{\"type\":\"string\","
                                  "\"description\":"
                                  "\"The word, phrase, or concept to render\"}",
                        "\"style\":{\"type\":\"string\","
                                   "\"description\":"
                                   "\"The artistic style (e.g. Cyberpunk, Cubism, "
                                   "Egyptian, Vaporwave, etc.)\"}"
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
// ---- MCPtool_wordArt ------------------------------------------------------
// ---------------------------------------------------------------------------

// ==============================================
// ==== Manage Outstanding Sampling Requests ====
// ==============================================

struct _ActiveSampling
        {
        bool isSampling;
        JSON_Value *idRequest;
        JSON_Value_String idSampling;
     // JSON_Object Arguments;                  // Save request arguments.
        // -----------------
        // ---- Methods ----
        // -----------------
        _ActiveSampling() {isSampling = false; idRequest=NULL;}
        bool BeginSampling(JSON_Value &_idRequest, JSON_Object &_Arguments);
        bool GenerateSamplingId(JSON_Value &idRequest);
        void EndSampling() 
                {
                idRequest = NULL;
                _idRequestStore.Clear();
                isSampling = false;
                }
        private:
                JSON_Object _idRequestStore;    // Could be a string or number.
        } static ActiveSamplings[MAXACTIVESAMPLINGS];

bool _ActiveSampling::BeginSampling(JSON_Value &_idRequest, 
                                    JSON_Object &_Arguments)
        {
        static const char *ProcName = "_ActiveSampling::BeginSampling";
        // ----------------------------------
        // ---- Generate the Sampling Id ----
        // ----------------------------------
        bool Status = GenerateSamplingId(_idRequest);
        if (Status)
                {
                // ---------------------------------------------------
                // ---- Store data required when Sampling returns ----
                // +-------------------------------------------------+
                // | _idRequest points to the original id sent from  |
                // | the MCP client. A reference (pointer) to it     |
                // | will be invalid once sampling returns; so make  |
                // | a deep copy because we don't know if it's a     |
                // | string or number -- therefore, store an object. |
                // +-------------------------------------------------+
                _idRequestStore.Add_member_value("id",_idRequest);
                idRequest = _idRequestStore.FindMemberValue("id");
                if (idRequest)
                        {
                        // +----------------------------------------------+
                        // | Save whatever original request arguments are |
                        // | needed by the response handler; or just save | 
                        // | everything. SAVE NO POINTER TO THE ORIGINAL! |
                        // +----------------------------------------------+
                     // Arguments = _Arguments;
                        }
                else    {
                        TERROR(("%s: Failed to Store Id (%s)",
                                ProcName,
                                id2string(_idRequest)));
                        }
                isSampling = idRequest != NULL;
                }
        else    {
                TERROR(("%s: Can't generate sampling ID (%s)",
                        ProcName,
                        id2string(_idRequest)));
                }
        return isSampling;
        }

bool _ActiveSampling::GenerateSamplingId(JSON_Value &idRequest)
        {
        // --------------------------
        // ---- Unique Id Number ----
        // --------------------------
        static unsigned UniqueCounter = 0;
        unsigned UniqueNumber = ++UniqueCounter;
        // --------------------------------------------
        // ---- Build: Smpl_RequestId_UniqueNumber ----
        // --------------------------------------------
        MemoryPrintf Canvas;
        Canvas.printf("Smpl_%s_%u",id2string(idRequest),UniqueNumber);
        const char *idString = Canvas.GetBuffer();
        if (idString)
                {
                idSampling.SetString(idString,strlen(idString));
                }
        return idString != NULL;
        }

// ---------------------------------------
// ---- Sampling Management Functions ----
// ---------------------------------------
 
static _ActiveSampling* BeginSampling(JSON_Value &idRequest, 
                                      JSON_Object &Arguments)
        {
        static const char *ProcName = "BeginSampling";
        unsigned i = 0;        
        _ActiveSampling *Sampling = NULL;
        for (; i < MAXACTIVESAMPLINGS; i++)
                {
                // ---------------------------------------------
                // ---- If there's space, save off stuff... ----
                // ---------------------------------------------
                if (!ActiveSamplings[i].isSampling)
                        {
                        bool Status = ActiveSamplings[i].BeginSampling(idRequest,
                                                                       Arguments);
                        if (Status)
                                {
                                Sampling = &ActiveSamplings[i];
                                TINFO(("%s: BeginSampling [%s] in slot: %d",
                                       ProcName,
                                       id2string(Sampling->idSampling),
                                       i));
                                }
                        else    {
                                TERROR(("%s: BeginSampling() failed",ProcName));
                                }
                        break;
                        }
                }
        if (i == MAXACTIVESAMPLINGS)
                {
                TERROR(("%s: Too many outstanding Samplings (%d)",ProcName,i));
                }
        return Sampling;
        }

static _ActiveSampling* FindSampling(JSON_Value &idRequest)
        {
        static const char *ProcName = "FindSampling";
        unsigned i=0;
        for (;i < MAXACTIVESAMPLINGS; i++)
                {
                if (idcmp(ActiveSamplings[i].idSampling,idRequest))
                        {
                        TINFO(("%s: Complete Sampling [%s] in slot: %d",
                                ProcName,
                                id2string(idRequest),
                                i));
                        return &ActiveSamplings[i];
                        }
                }
        TERROR(("%s: State Error: No matching sampling for: %s",
                ProcName,
                id2string(idRequest)));
        return NULL;
        }

static void EndSampling(_ActiveSampling *Sampling)
        {
        if (Sampling) Sampling->EndSampling();
        }

// =========================
// ==== MCPtool_wordArt ====
// ========================= 

char* MCPtool_wordArt(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_wordArt";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "wordArt",
            "arguments": { "text": "Hello", "style": "big" }
          }
        }
        ----------------------------------------------------------- */
        // ---------------------------------------------------------------
        // ---- Claim a sampling slot and stash idRequest for later.  ----
        // ---- If the client has no LLM, FormMCPRequest_sampling will  ----
        // ---- still be sent; the client must reject it.              ----
        // ---------------------------------------------------------------
        _ActiveSampling *Sampling = BeginSampling(idRequest,Arguments);
        if (!Sampling)
                {
                TERROR(("%s: Couldn't Record Sampling (%s)",ProcName,
                        id2string(idRequest)));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_Exception,
                                             "Invalid State",
                                             "Couldn't Record Sampling");
                }
        // ----------------------------
        // ---- Validate Arguments ----
        // ----------------------------
        const char *Text  = Arguments.Find_member_string("text");
        const char *Style = Arguments.Find_member_string("style");
        if (!Style || !*Style) Style = "big";
        if (!Text)
                {
                EndSampling(Sampling);                
                TERROR(("%s: Missing Argument: 'text'",ProcName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: text");
                }
        // -----------------------------------------------
        // ---- Build system prompt with chosen style ----
        // -----------------------------------------------
        MemoryPrintf SysPrompt;
        SysPrompt.printf(
                "You are an avant-garde ASCII artist specialized in the \"%s\" style. "
                "Render the provided text or concept as a highly detailed ASCII art "
                "graphic. "
                "RULES: "
                "1. Return ONLY the ASCII art block. "
                "2. Do NOT use markdown backticks (```). "
                "3. Ensure the graphic is self-contained and visually cohesive. "
                "4. Incorporate motifs and patterns iconic to the chosen style.",
                Style);
        TINFO(("%s: Requesting wordArt (style=%s): %s",ProcName,Style,Text));
        // ---------------------------------------------------------
        // ---- Return the sampling request as MCPOutput.        ----
        // ---- The framework sends it to the client's LLM.      ----
        // ---- Handle_sampling_response() delivers the result.  ----
        // ---------------------------------------------------------
        char *MCPOutput = FormMCPRequest_sampling(Sampling->idSampling,
                                                  SysPrompt.GetBuffer(),
                                                  Text,
                                                  1024);
        if (!MCPOutput)
                {
                EndSampling(Sampling);                
                TERROR(("%s: FormMCPRequest_sampling Failed",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_Exception,
                                                  "Sampling Failed",
                                                  "Could not form sampling request");
                }
        return MCPOutput;
        }

// ---------------------------------------------------------------------------
// ---- Handle_tools_call ----------------------------------------------------
// ---------------------------------------------------------------------------

char* Handle_tools_call(JSON_Value &idRequest,
                        const char *ToolName, 
                        JSON_Object &Arguments)
        {
        const char *ProcName = "Handle_tools_call";
        char *MCPOutput = NULL;
        if (strcmp(ToolName,"wordArt") == 0)
                {
                MCPOutput = MCPtool_wordArt(idRequest,Arguments);
                }
        return MCPOutput;
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
// **** Handle_sampling_response *********************************************
// ****************************************************************************

char* Handle_sampling_response(MCPInputRequest &MCPRequest)
        {
        const char *ProcName = "Handle_sampling_response";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "result": {
            "role": "assistant",
            "content": { "type": "text", "text": "..." }
          }
        }
        ----------------------------------------------------------- */
        JSON_Value *idRequest = MCPRequest.Get_id();
        if (!idRequest)
                {
                TERROR(("%s: No Id in Request",ProcName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidRequest,
                                             "No Id",
                                             "No Id found");
                }
        // ----------------------------
        // ---- Find Sampling Slot ----
        // ----------------------------
        _ActiveSampling *Sampling = FindSampling(*idRequest);
        if (!Sampling)
                {
                TERROR(("%s: Couldn't find sampling (%s)",
                        ProcName,
                        id2string(*idRequest)));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_Exception,
                                             "Invalid State",
                                             "No matching sampling request");
                }
        // -----------------------------------
        // ---- Extract art from result   ----
        // -----------------------------------
        const char *ArtText = NULL;
        JSON_Object *Result = MCPRequest.Get_result();
        if (Result)
                {
                JSON_Object *Content = Result->Find_member_object("content");
                if (Content)
                        {
                        ArtText = Content->Find_member_string("text");
                        }
                }
        if (!ArtText || !*ArtText)
                {
                TERROR(("%s: No text in sampling result",ProcName));
                char *MCPOutput = FormMCPResponse_Text(Sampling->idRequest,
                                                       "No output returned by LLM",
                                                       true);
                EndSampling(Sampling);
                return MCPOutput;
                }
        TINFO(("%s: wordArt complete",ProcName));
        char *MCPOutput = FormMCPResponse_Text(Sampling->idRequest,ArtText);
        EndSampling(Sampling);
        return MCPOutput;
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
