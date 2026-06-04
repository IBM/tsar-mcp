// MCP Server for helloWorld: MCPServer_helloWorld.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServer_helloWorld.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Test prompt: "Test the MCP server connection and say hello to Eric"
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPServerCore.h>

const char *MCPServer_Name = "MCPServer_helloWorld";
const char *MCPServer_Version = "0.1.2";
const char *MCPServer_Capabilities = "\"tools\":{}";
bool MCPServer_Asynchronous = false;

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

static const char* MCPInputTestRequest_HelloCall =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"helloWorld\","
         "   \"arguments\": { \"name\": \"Eric\" }"
         " }"
         "}"; 

const char* MCPInputTestRequests[] = 
        {
        MCPInputTestRequest_HelloCall,
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
                // --------------------
                // ---- helloWorld ----
                // --------------------
                {
                // Name
                "helloWorld",
                // Description
                "Checks the connection to the MCPServer.",
                1,
                // InputSchema {Properties}
                        {
                        "\"name\":{\"type\":\"string\","
                                  "\"description\":\"Whom to greet\"}"
                        },
                // [Required]
                "\"name\""
                },
                // ---------------------------------------
                // ---- get_block_info (Example Only) ----
                // ---------------------------------------
                {
                // Name
                "get_block_info",
                // Description
                "Returns details about a block at specific coordinates.",
                3,
                // InputSchema {Properties}
                        {
                        "\"x\":{\"type\":\"number\"}",
                        "\"y\":{\"type\":\"number\"}",
                        "\"z\":{\"type\":\"number\"}"
                        },
                // [Required]
                "\"x\",\"y\",\"z\""
                }
                // ---------------
                // ---- <end> ----
                // ---------------
        };

// ***************************************************************************
// **** Tool Handlers ********************************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- Handle_helloWorld ----------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_helloWorld(JSON_Value &idRequest, const char *Name)
        {
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "result": {
            "content": [
              {
                "type": "text", 
                "text": "How are you, Eric."
              }
            ],
            "isError": false
          }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Summary;
        Summary.printf("How are you, %s.",Name);
        return FormMCPResponse_Text(&idRequest, Summary.GetBuffer());
        }

char* MCPtool_helloWorld(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_helloWorld";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "helloWorld",
            "arguments": { "name": "Eric" }
          }
        }
        ----------------------------------------------------------- */
        const char *Name = Arguments.Find_member_string("name");
        if (!Name)
                {
                TERROR(("%s: Missing Argument: 'name'",ProcName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: name");
                }
        return FormMCPResponse_helloWorld(idRequest,Name);
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
        if (strcmp(ToolName,"helloWorld") == 0)  
                {
                MCPOutput = MCPtool_helloWorld(idRequest,Arguments);
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
// **** Handle_sampling_response (Dormant) ***********************************
// ****************************************************************************

char* Handle_sampling_response(MCPInputRequest &MCPRequest)
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
