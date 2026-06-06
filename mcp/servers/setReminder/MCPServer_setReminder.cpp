// MCP Server for setReminder: MCPServer_setReminder.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServer_setReminder.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LevelTrace.h>
#include <MEMprintf.h>
#include <TimerThread.h>

#include <MCPServerCore.h>

const char *MCPServer_Name = "MCPServer_setReminder";
const char *MCPServer_Version = "1.0.0";
const char *MCPServer_Capabilities = "\"tools\":{}";
bool MCPServer_Asynchronous = true;

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

static const char* MCPInputTestRequest_setReminder =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"setReminder\","
         "   \"arguments\": { \"Reminder\": \"Check build status\","
         "                    \"TimeoutMS\": 5000 }"
         " }"
         "}"; 

const char* MCPInputTestRequests[] = 
        {
        MCPInputTestRequest_setReminder,
        NULL
        };

// ***************************************************************************
// **** Reminder Timer Thread ************************************************
// ***************************************************************************

class ReminderThread_MCPServer_Gateway : public MessageQueueIF
        {
        public:
                virtual void* Receive() {return NULL;}
                virtual void* Receive(unsigned TimeoutMS) {return NULL;}
                virtual bool Send(void *Msg)
                        {
                        return PostNotifyAction(Msg);   // Static call.
                        }
                virtual bool SendPriority(void *Msg) {return Send(Msg);}
        } MCPServer_Gateway;

class ReminderTimerThread : public TimerThread
        {
        protected:
                virtual void OnMessageDestruct(MessageQueueIF &Queue, 
                                               void *ReminderText);
        public:
                bool SetTimer(void *ReminderText, unsigned TimeoutMS);
        };

void ReminderTimerThread::OnMessageDestruct(MessageQueueIF &Queue, 
                                            void *ReminderText)
        {
        char *Text = (char *)ReminderText;
        FreeUnescapeJSONString(&Text);
        return;
        }

bool ReminderTimerThread::SetTimer(void *ReminderText, unsigned TimeoutMS)
        {
        return TimerThread::SetTimer(MCPServer_Gateway,ReminderText,TimeoutMS);
        }

// ***************************************************************************
// **** Init / Shutdown ******************************************************
// ***************************************************************************

ReminderTimerThread ReminderTimer;

bool MCPServer_OnInitialize(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "MCPServer_OnInitialize";
        TINFO(("%s: %s Initialized",ProcName,MCPServer_Name));
        // -----------------------------
        // ---- Start ReminderTimer ----
        // -----------------------------
        bool Status = ReminderTimer.Start();
        if (!Status)
                {
                TERROR(("%s: ReminderTimer NOT Started",ProcName));
                }
        return true;
        }

bool MCPServer_OnShutdown()
        {
        static const char *ProcName = "MCPServer_OnShutdown";
        TINFO(("%s: %s Shutting Down...",ProcName,MCPServer_Name));
        ReminderTimer.Stop(true);
        return true;
        }

// ***************************************************************************
// **** MCP Directoy Handling ************************************************
// ***************************************************************************

const unsigned MCPToolInfoCount = 1;

MCPToolInfo_t MCPToolInfo[] = 
        {
                // --------------------
                // ---- setReminder ----
                // --------------------
                {
                // Name
                "setReminder",
                // Description
                "Requests that a reminder (as text) be sent to the MCP "
                "client at a specified time.",
                2,
                // InputSchema {Properties}
                        {
                        "\"Reminder\":{\"type\":\"string\","
                                      "\"description\":\"Reminder text\"}",
                        "\"TimeoutMS\":{\"type\":\"number\","
                                      "\"description\":\"Time to notify\"}"
                        },
                // [Required]
                "\"Reminder\",\"TimeoutMS\""
                }
                // ---------------
                // ---- <end> ----
                // ---------------
        };

// ***************************************************************************
// **** Tool Handlers ********************************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- Handle_setReminder ----------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_setReminder(JSON_Value &idRequest, 
                                  const char *Reminder, 
                                  unsigned TimeoutMS)
        {
        const char *ProcName = "FormMCPResponse_setReminder";
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "result": {
            "content": [
              {
                "type": "text", 
                "text": "Reminder set for 5000ms: Check build status"
              }
            ],
            "isError": false
          }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        // ----------------
        // ---- result ----
        // ----------------
        Canvas.printf("\"result\":{");
        Canvas.printf("\"content\":[{");
        Canvas.printf("\"type\":\"text\","
                      "\"text\":\"Reminder set for %ums: %s\"",
                      TimeoutMS,Reminder);
        Canvas.printf("}],"); /* content */
        Canvas.printf("\"isError\":false");
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

char* MCPtool_setReminder(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_setReminder";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "setReminder",
            "arguments": { "Reminder": "Check build status",
                           "TimeoutMS": 5000 }
          }
        }
        ----------------------------------------------------------- */
        char *MCPOutput = NULL;
        // -------------------------
        // ---- Extract Reminder ----
        // -------------------------
        const char *Reminder = Arguments.Find_member_string("Reminder");
        if (!Reminder)
                {
                TERROR(("%s: Missing Argument: 'Reminder'",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Missing parameter: Reminder");
                return MCPOutput;
                }
        // --------------------------
        // ---- Extract TimeoutMS ----
        // --------------------------
        JSON_Value_Number *TimeoutValue = Arguments.Find_member_number("TimeoutMS");
        if (!TimeoutValue)
                {
                TERROR(("%s: Missing Argument: 'TimeoutMS'",ProcName));
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Missing parameter: TimeoutMS");
                return MCPOutput;
                }
        unsigned TimeoutMS = (unsigned)TimeoutValue->int_number;
        // -----------------------
        // ---- Set the Timer ----
        // -----------------------
        char *ReminderCopy = UnescapeJSONString(Reminder, strlen(Reminder));
        if (ReminderCopy)
                {
                bool Status = ReminderTimer.SetTimer(ReminderCopy,TimeoutMS);
                if (!Status)
                        {
                        TERROR(("%s: SetTimer() Failed",ProcName));
                        FreeUnescapeJSONString(&ReminderCopy);
                        MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                          MCPErrorCode_Exception,
                                                          "Timer Error",
                                                          "Failed to set reminder timer");
                        return MCPOutput;
                        }
                }
        MCPOutput = FormMCPResponse_setReminder(idRequest,Reminder,TimeoutMS);
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
        if (strcmp(ToolName,"setReminder") == 0)  
                {
                MCPOutput = MCPtool_setReminder(idRequest,Arguments);
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
// **** Handle_notify_action -------------------------------------------------
// ****************************************************************************

char* Handle_notify_action(void *UserPtr)
        {
        const char *ProcName = "Handle_notify_action";
        char *ReminderText = (char *)UserPtr;
        if (!ReminderText) return NULL;
        TINFO(("%s: Sending Reminder: %s",ProcName,ReminderText));
        /* ----- OUTPUT (MCP Notification - no id) -------------------
        {
          "jsonrpc": "2.0",
          "method": "notifications/message",
          "params": {
            "level": "info",
            "data": "REMINDER: Check build status"
          }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"method\":\"notifications/message\",");
        Canvas.printf("\"params\":{");
        char *EscapedText = EscapeJSONString(ReminderText);
        Canvas.printf("\"level\":\"info\",");
        Canvas.printf("\"data\":\"REMINDER: %s\"",EscapedText ? EscapedText : ReminderText);
        if (EscapedText) FreeEscapeJSONString(&EscapedText);
        Canvas.printf("}");
        Canvas.printf("}");
        FreeUnescapeJSONString(&ReminderText);
        return Canvas.AquireBuffer();
        }

void Discard_notify_action(void *UserPtr)
        {
        char *ReminderText = (char *)UserPtr;
        FreeUnescapeJSONString(&ReminderText);
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

// ****************************************************************************
// **** Handle_method (Dormant) ***********************************************
// ****************************************************************************

char* Handle_method(MCPInputRequest &MCPRequest)
        {
        return NULL;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
