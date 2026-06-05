// MCP Server for portScan: MCPServer_portScan.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServer_portScan.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Purpose: Demonstrates standard JSON-RPC error handling and native 
//      network socket integration."
//
//      Test prompt: "Probe ports 8000-8010 on invalid.hostname.test"
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LevelTrace.h>
#include <MEMprintf.h>
#include <SocketCB.h>

#include <MCPServerCore.h>

const char *MCPServer_Name = "MCPServer_portScan";
const char *MCPServer_Version = "0.1.0";
const char *MCPServer_Capabilities = "\"tools\":{}";
bool MCPServer_Asynchronous = false;

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

// Test Case 1: Invalid hostname - triggers MCPErrorCode_InvalidParms.
static const char* MCPInputTestRequest_ProbePort_Invalid =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"probePort\","
         "   \"arguments\": {"
         "     \"host\": \"invalid.hostname.test\","
         "     \"startPort\": 2222,"
         "     \"endPort\": 2222"
         "   }"
         " }"
         "}";

// Test Case 2: Valid hostname - control test
static const char* MCPInputTestRequest_ProbePort_Valid =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 4,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"probePort\","
         "   \"arguments\": {"
         "     \"host\": \"localhost\","
         "     \"startPort\": 2222,"
         "     \"endPort\": 2222"
         "   }"
         " }"
         "}";

const char* MCPInputTestRequests[] =
        {
        MCPInputTestRequest_ProbePort_Invalid,
        MCPInputTestRequest_ProbePort_Valid,
        NULL
        };

// ***************************************************************************
// **** Init / Shutdown ******************************************************
// ***************************************************************************

static bool isSocketInit = false;

bool MCPServer_OnInitialize(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "MCPServer_OnInitialize";
  #ifdef _WIN32
        isSocketInit = InitializeSockets();
  #endif
        TINFO(("%s: %s Initialized",ProcName,MCPServer_Name));
        return true;
        }

bool MCPServer_OnShutdown()
        {
        static const char *ProcName = "MCPServer_OnShutdown";
        TINFO(("%s: %s Shutting Down...",ProcName,MCPServer_Name));
  #ifdef _WIN32
        if (isSocketInit) 
                {
                DeinitializeSockets();
                isSocketInit = false;
                }
  #endif
        return true;
        }

// ***************************************************************************
// **** MCP Directoy Handling ************************************************
// ***************************************************************************

const unsigned MCPToolInfoCount = 1;

MCPToolInfo_t MCPToolInfo[] =
        {
                // -------------------
                // ---- probePort ----
                // -------------------
                {
                // Name
                "probePort",
                // Description
                "Probes a range of ports on a host to identify which ports have "
                "services running. Returns list of ports IN USE. Returns error "
                "response if hostname cannot be resolved. Returns success with "
                "isError=true if host is unreachable.",
                3,
                // InputSchema {Properties}
                        {
                        "\"host\":{\"type\":\"string\","
                                 "\"description\":\"Hostname or IP address to probe\"}",
                        "\"startPort\":{\"type\":\"number\","
                                      "\"description\":\"First port in range to probe\"}",
                        "\"endPort\":{\"type\":\"number\","
                                    "\"description\":\"Last port in range to probe\"}"
                        },
                // [Required]
                "\"host\",\"startPort\",\"endPort\""
                }
                // ---------------
                // ---- <end> ----
                // ---------------
        };

// ***************************************************************************
// **** ProbePort ************************************************************
// ***************************************************************************

bool ProbePort(const char *Host, int StartPort, int EndPort, MemoryPrintf &Results)
        {
        static const char *ProcName = "ProbePort";
        TINFO(("%s: Probing ports %d-%d on host '%s'",
               ProcName,
               StartPort,
               EndPort,
               Host));
        bool AnySuccess = false;
        int InUseCount = 0;
        Results.printf("Port scan results for %s:\n", Host);
        for (int Port = StartPort; Port <= EndPort; Port++)
                {
                SocketCtrlBlock Socket;
                if (!Socket.OpenTCPsocket(NULL, 0))
                        {
                        TERROR(("%s: Failed to create socket for port %d",ProcName,Port));
                        continue;
                        }
                if (Socket.Connect(Host, Port))         // Try to connect to the port
                        {
                        // Connection succeeded - port is IN USE
                        TINFO(("%s: Port %d in use (connection succeeded)",ProcName,Port));
                        Results.printf("  Port %d: IN USE\n", Port);
                        InUseCount++;
                        AnySuccess = true;
                        }
                else    {
                        int err = Socket.GetLastError();
                        TDEBUG(("%s: Port %d - connection failed (error %d)",
                                ProcName,
                                Port,
                                err));
                        // Connection refused or other error - port not in use or unreachable
                        // We got a response from the host, so mark as success
                        if (err == WSAECONNREFUSED || err == WSAETIMEDOUT)
                                {
                                AnySuccess = true;
                                }
                        }
                Socket.Close();
                }
        if (InUseCount == 0)
                {
                Results.printf("  No ports in use in range %d-%d\n", StartPort, EndPort);
                }
        if (AnySuccess)
                {
                TINFO(("%s: Found %d port(s) in use",ProcName,InUseCount));
                }
        else    {
                TERROR(("%s: No ports responded - host may be down or unreachable",
                        ProcName));
                }
        return AnySuccess;
        }

// ***************************************************************************
// **** Tool Handlers ********************************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- Handle_probePort -----------------------------------------------------
// ---------------------------------------------------------------------------

char* MCPtool_probePort(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_probePort";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "probePort",
            "arguments": {
              "host": "localhost",
              "startPort": 8000,
              "endPort": 8010
            }
          }
        }
        ----------------------------------------------------------- */
        // -------------------------------
        // Extract and validate parameters
        // -------------------------------
        const char *Host = Arguments.Find_member_string("host");
        if (!Host)
                {
                TERROR(("%s: Missing Argument: 'host'",ProcName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: host");
                }
        JSON_Value_Number *StartPortValue = Arguments.Find_member_number("startPort");
        if (!StartPortValue)
                {
                TERROR(("%s: Missing or invalid Argument: 'startPort'",ProcName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing or invalid parameter: startPort");
                }
        int StartPort = StartPortValue->int_number;
        JSON_Value_Number *EndPortValue = Arguments.Find_member_number("endPort");
        if (!EndPortValue)
                {
                TERROR(("%s: Missing or invalid Argument: 'endPort'",ProcName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing or invalid parameter: endPort");
                }
        int EndPort = EndPortValue->int_number;
        // -----------------------------------------------------
        // Validate hostname resolution BEFORE calling ProbePort
        // If hostname is invalid, return ERROR response.
        // -----------------------------------------------------
        socketCBaddr AddrDest;
        if (!ResolveAddress(&AddrDest, Host, StartPort))
                {
                TERROR(("%s: Hostname resolution failed: '%s'",ProcName,Host));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid host",
                                             "Hostname invalid or unreachable");
                }
        TINFO(("%s: Hostname '%s' resolved successfully",ProcName,Host));
        // ---------------
        // Probe ports ...
        // ---------------
        MemoryPrintf Results;
        bool Success = ProbePort(Host, StartPort, EndPort, Results);
        char *MCPOutput = NULL;        
        if (Success)            // At least one port responded.
                {
                /* ----- OUTPUT (Success) --------------------------------
                {
                  "jsonrpc": "2.0",
                  "id": 3,
                  "result": {
                    "content": [
                      {
                        "type": "text",
                        "text": "Port scan results for localhost:\n"
                                "Port 8000: AVAILABLE\n..."
                      }
                    ],
                    "isError": false
                  }
                }
                ----------------------------------------------------------- */
                MCPOutput = FormMCPResponse_Text(&idRequest, Results.GetBuffer());
                }
        else    {
                // No ports responded - return success response with isError=true
                /* ----- OUTPUT (Probe Failure) --------------------------
                {
                  "jsonrpc": "2.0",
                  "id": 3,
                  "result": {
                    "content": [
                      {
                        "type": "text",
                        "text": "Host is not responding"
                      }
                    ],
                    "isError": true
                  }
                }
                ----------------------------------------------------------- */
                MCPOutput = FormMCPResponse_Text(&idRequest,
                                                 "Host is not responding",
                                                 true);  // isError flag
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
        if (strcmp(ToolName,"probePort") == 0)
                {
                MCPOutput = MCPtool_probePort(idRequest,Arguments);
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
