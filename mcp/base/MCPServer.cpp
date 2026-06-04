// MCP Server : MCPServer.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServer.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Version: 02/14/2026 (reviewd and corrected by Opus on 06/23/2026)
//
//      Specification: https://modelcontextprotocol.io/specification/
//      
//      To Register in VSCode: Ctrl-Shift-P > MCP:List Servers > + Add Server
//      To Control Server: Ctrl-Shift-P > MCP:List Servers > Start / Stop
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
        #include <io.h>
        #include <process.h>
        #define isatty _isatty
        #define EOFSequence "^Z"
#else
        #include <unistd.h>
        #ifndef stricmp
                #define stricmp strcasecmp
        #endif
        #define EOFSequence "^D"
#endif

#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPServerCore.h>

#define MCPServerLOG "MCPServerLog.txt"                 // Log File.

// ***************************************************************************
// **** Utilities ************************************************************
// ***************************************************************************

const char* TrimQuote(const char *String, size_t *StringLen)
        {
        if (!String)
                {
                if (StringLen) *StringLen = 0;
                return NULL;
                }
        size_t Length = strlen(String);        
        if (String[0] == '"')
                {
                String++;
                Length--;
                if (String[Length-1] == '"') Length--;
                }
        if (StringLen) *StringLen = Length;
        return String;
        }

// ***************************************************************************
// **** LevelTrace Redirect **************************************************
// ***************************************************************************

static int LevelTraceFileTrace(void *UserPtr,
                               unsigned Level, 
                               const char *Format, 
                               va_list Args)
        {
        FILE *LevelTraceFile = (FILE *)UserPtr;
        if (Level > GetMaxTraceLevel()) return 0;
        if (!LevelTraceFile) return 0;
        // ---------------
        // ---- Trace ----
        // ---------------
        if (Level == TRACELEVEL_ERROR) 
                {
                fprintf(LevelTraceFile,"ERROR: ");
                }
        else if (Level == TRACELEVEL_INFO) 
                {
                fprintf(LevelTraceFile,"INFO:  ");
                }
        else if (Level == TRACELEVEL_DEBUG) 
                {
                fprintf(LevelTraceFile,"DEBUG: ");
                }
        vfprintf(LevelTraceFile,Format,Args);
        fprintf(LevelTraceFile,"\n");
        fflush(LevelTraceFile);
        return 1;
        }

// ***************************************************************************
// **** Load JSON Input Request **********************************************
// ***************************************************************************

#ifdef _DEBUG
        #define ReadSTDIN_MCPRequest_BufferSIZE 16
#else
        #define ReadSTDIN_MCPRequest_BufferSIZE 2048
#endif /* _DEBUG */

char* ReadSTDIN_MCPRequest(FILE *in)    // "getline()" - reads until EOL.
        {
        const char *ProcName = "ReadSTDIN_MCPRequest";
        size_t BufferSize = ReadSTDIN_MCPRequest_BufferSIZE;
        char *Buffer = (char *)malloc(BufferSize);
        if (!Buffer)
                {
                TERROR(("%s: Unable to malloc(%u)", ProcName,BufferSize));
                return NULL;
                }
        size_t Used = 0;
        do      {
                char *Pos = Buffer + Used;
                char *readBuffer = fgets(Pos,BufferSize-Used,in);
                if (!readBuffer)
                        {
                        if (feof(in)) TDEBUG(("%s: fgets() - EOF",ProcName));
                        else TERROR(("%s: fgets() Error",ProcName));
                        free(Buffer);
                        Buffer = NULL;
                        break;
                        }
                size_t readBufferLength = strlen(readBuffer);
                if (!readBufferLength)
                        {
                        TERROR(("%s: Unexpected fgets() no read",ProcName));
                        free(Buffer);
                        Buffer = NULL;
                        break;
                        }
                if (readBuffer[readBufferLength-1] == '\n')
                        {
                        readBuffer[readBufferLength-1] = '\0';
                        break;
                        }
                Used += readBufferLength;
                size_t NewBufferSize = BufferSize * 2;
                char *NewBuffer = (char *)realloc(Buffer,NewBufferSize);
                if (!NewBuffer)
                        {
                        TERROR(("%s: Unable to realloc(%u)",ProcName,
                                                            NewBufferSize));
                        free(Buffer);
                        Buffer = NULL;
                        break;
                        }
                Buffer = NewBuffer;
                BufferSize = NewBufferSize;
                } while (1);
        return Buffer;
        }      
       
void FreeMCPRequestBuffer(char **Buffer)
        {
        if (Buffer && *Buffer)
                {
                free(*Buffer);
                *Buffer = NULL;
                }
        return;
        }

// ***************************************************************************
// **** JSON Output Responses ************************************************
// ***************************************************************************

void FreeMCPResponseBuffer(char **Buffer)
        {
        if (Buffer && *Buffer)
                {
                free(*Buffer);
                *Buffer = NULL;
                }
        return;
        }

// ---------------------------------------------------------------------------
// ---- FormMCPResponse_Text -------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_Text(JSON_Value *idRequest, const char *Text)
        {
        /* ----- OUTPUT (If Text provided) ---------------------------
        {
           "jsonrpc": "2.0",
           "id": 1,
           "result": {
             "content": [{"type":"text","text":"..."}],
             "isError": false
           }
        }
        ----- OUTPUT (If Text is NULL) -------------------------------
        {
           "jsonrpc": "2.0",
           "id": 1,
           "result": {}
        }
        ----------------------------------------------------------- */
        JSON_Object root;
        root.Add_member_string("jsonrpc", "2.0");
        root.Add_member_value("id", *idRequest);
        JSON_Object *result = root.Add_member_object("result");
        if (Text && result)
                {
                JSON_Value_Array *content = result->Add_member_array("content");
                if (content)
                        {
                        JSON_Object *textItem = content->Add_value_object();
                        if (textItem)
                                {
                                textItem->Add_member_string("type", "text");
                                textItem->Add_member_string("text", Text);
                                }
                        }
                result->Add_member_bool("isError", false);
                }
        MemoryPrintf Canvas;
        PrintJSONObject(Canvas, root, true, false);
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- FormMCPResponse_Text -------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_Text(JSON_Value *idRequest, 
                           const char *Text, 
                           bool isError)
        {
        /* ----- OUTPUT (If Text provided) ---------------------------
        {
           "jsonrpc": "2.0",
           "id": 1,
           "result": {
             "content": [{"type":"text","text":"..."}],
             "isError": false
           }
        }
        ----- OUTPUT (If Text is NULL) -------------------------------
        {
           "jsonrpc": "2.0",
           "id": 1,
           "result": {}
        }
        ----------------------------------------------------------- */
        JSON_Object root;
        root.Add_member_string("jsonrpc", "2.0");
        root.Add_member_value("id", *idRequest);
        JSON_Object *result = root.Add_member_object("result");
        if (Text && result)
                {
                JSON_Value_Array *content = result->Add_member_array("content");
                if (content)
                        {
                        JSON_Object *textItem = content->Add_value_object();
                        if (textItem)
                                {
                                textItem->Add_member_string("type","text");
                                textItem->Add_member_string("text",Text);
                                }
                        }
                result->Add_member_bool("isError",isError);
                }
        MemoryPrintf Canvas;
        PrintJSONObject(Canvas, root, true, false);
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- FormMCPResponse_ERROR ------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_ERROR(JSON_Value *idRequest,
                            int Code, 
                            const char *ErrorTag, 
                            const char *ErrorFormat,
                            ...)
        {
        static const char *ProcName = "FormMCPResponse_ERROR";
        /* ----- OUTPUT ----------------------------------------------
        {
           "jsonrpc": "2.0",
           "id": 1,
           "error": {
             "code": -32602,
             "message": "Invalid params",
             "data": "Missing required parameter: name"
           }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"error\":{");
        Canvas.printf("\"code\":%d,",Code);
        // ----------------------------
        // ---- Format Error Text ----
        // ----------------------------
        va_list Args;
        MemoryPrintf ErrorCanvas;
        va_start(Args, ErrorFormat);
        ErrorCanvas.vprintf(ErrorFormat, Args);
        va_end(Args);
        const char *ErrorText = ErrorCanvas.GetBuffer();
        // ----------------------------------------------
        // ---- Emit "message" with truncated detail ----
        // ----------------------------------------------
        MemoryPrintf MessageCanvas;
        MessageCanvas.printf(!ErrorText 
                             ? "%s" 
                             : strlen(ErrorText) <= 128
                             ? "%s [%s]"
                             : "%s [%.128s...]",ErrorTag,ErrorText);
        const char *CombinedMsg = MessageCanvas.GetBuffer();
        if (CombinedMsg)
                {
                char *jsonMessage = EscapeJSONString(CombinedMsg);
                if (jsonMessage) 
                        {
                        Canvas.printf("\"message\":\"%s\",",jsonMessage);
                        }
                FreeEscapeJSONString(&jsonMessage);
                }
        TDEBUG(("%s: Error (id=%s): %s",ProcName,
                                        id2string(idRequest),
                                        CombinedMsg ? CombinedMsg : ErrorTag));
        // --------------------------------
        // ---- Emit full "data" field ----
        // --------------------------------
        if (ErrorText)
                {
                char *jsonErrorText = EscapeJSONString(ErrorText);
                if (jsonErrorText)
                        {
                        bool HintTrace = Code == MCPErrorCode_ParseError
                                      || Code == MCPErrorCode_Exception;
                        Canvas.printf("\"data\":\"%s%s\"",
                                      jsonErrorText,
                                      HintTrace
                                      ? " (readTrace tool fetches details)"
                                      : "");
                        FreeEscapeJSONString(&jsonErrorText);
                        }
                }
        // ----------------------------
        Canvas.printf("}");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- Handle_initialize ----------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_initialize(JSON_Value *idRequest,
                                 const char *protocolVersion,
                                 JSON_Object *capabilities,
                                 JSON_Object *clientInfo)
        {
        const char *ProcName = "FormMCPResponse_initialize";
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 1,
          "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {
              "tools": {} 
            },
            "serverInfo": { "name": "MCPServer", "version": "0.1.0" }
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
        Canvas.printf("\"protocolVersion\":\"2024-11-05\",");
        Canvas.printf("\"capabilities\":{\"tools\":{}},");
        Canvas.printf("\"serverInfo\":{"
                                       "\"name\":\"%s\","
                                       "\"version\":\"%s\"}",
                      MCPServer_Name,
                      MCPServer_Version);
        Canvas.printf("}");
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

char* Handle_initialize(MCPInputRequest *MCPRequest)
        {
        const char *ProcName = "Handle_initialize";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 1,
          "method": "initialize",
          "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": { "name": "vscode-copilot", "version": "1.0.0" }
          }
        }
        ----------------------------------------------------------- */
        if (!MCPRequest) return NULL;
        MemoryPrintf Canvas;
        JSON_Value *idRequest = MCPRequest->Get_id();
        const char *protocolVersion = MCPRequest->Get_param_string("protocolVersion");
        JSON_Object *capabilities = MCPRequest->Get_param_object("capabilities");;
        JSON_Object *clientInfo = MCPRequest->Get_param_object("clientInfo");;
        if (clientInfo)
                {
                const char *client_name = clientInfo->Find_member_string("name");
                const char *client_version = clientInfo->Find_member_string("version");
                TINFO(("%s: Client: name=%s, Version=%s",ProcName,
                                                         client_name
                                                         ? client_name
                                                         : "<none>",
                                                         client_version
                                                         ? client_version
                                                         : "<none>"));
                }
        char *MCPOutput = FormMCPResponse_initialize(idRequest,
                                                     protocolVersion,
                                                     capabilities,
                                                     clientInfo);
        if (!MCPOutput)
                {
                TERROR(("%s: No MCPOutput",ProcName));
                }
        return MCPOutput;
        }

void Handle_initialized(MCPInputRequest *MCPRequest)
        {
        const char *ProcName = "Handle_initialized";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "method": "notifications/initialized"
        }
        ----------------------------------------------------------- */
        TINFO(("%s: Client responded: notifications/initialized",ProcName));
        return;
        }

// ***************************************************************************
// **** MCP Directoy Handling ************************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- Handle_tools_list ----------------------------------------------------
// ---------------------------------------------------------------------------

char* Handle_tools_list(JSON_Value *idRequest)
        {
        static const char *ProcName = "Handle_tools_list";
        /* ----- INPUT -------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": "1",
          "method": "tools/list",
          "params": {}
        }
        ------------------------------------------------------------- */
        /* ----- OUTPUT ------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 2,
          "result": {
            "tools": [
              {
                "name": "get_block_info",
                "description": "Returns details about a block at specific coordinates.",
                "inputSchema": {
                  "type": "object",
                  "properties": {
                    "x": { "type": "number" },
                    "y": { "type": "number" },
                    "z": { "type": "number" }
                  },
                  "required": ["x", "y", "z"]
                }
              }
            ]
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
        Canvas.printf("\"tools\":[");
        for (unsigned i = 0; i < MCPToolInfoCount; i++)
                {
                Canvas.printf("{");
                Canvas.printf("\"name\":\"%s\",",MCPToolInfo[i].Name);
                Canvas.printf("\"description\":\"%s\",",MCPToolInfo[i].Description);
                Canvas.printf("\"inputSchema\":{");
                Canvas.printf("\"type\":\"object\",");
                Canvas.printf("\"properties\":{");
                unsigned nProperties = MCPToolInfo[i].nProperties;
                for (unsigned j=0; j < nProperties; j++)
                        {
                        Canvas.printf(j + 1 == nProperties ? "%s" : "%s,",
                                      MCPToolInfo[i].Properties[j]);
                        }
                Canvas.printf("},"); /* properties */
                Canvas.printf("\"required\":[%s]",MCPToolInfo[i].Required);
                Canvas.printf("}"); /* inputSchema */
                if (i + 1 == MCPToolInfoCount) Canvas.printf("}"); 
                else Canvas.printf("},");
                }
        Canvas.printf("]"); /* tools */
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- Handle_tools_call ----------------------------------------------------
// ---------------------------------------------------------------------------

char* Handle_tools_call(MCPInputRequest *MCPRequest)
        {
        const char *ProcName = "Handle_tools_call";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "get_block_info",
            "arguments": { "x": 10, "y": 64, "z": 20 }
          }
        }
        ----------------------------------------------------------- */
        if (!MCPRequest) return NULL;
        char *MCPOutput = NULL;
        JSON_Value *idRequest = MCPRequest->Get_id();

        const char *ToolName = MCPRequest->Get_param_string("name");
        JSON_Object *Arguments = MCPRequest->Get_param_object("arguments");
        if (!ToolName)
                {
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Missing tool name",
                                                  "Missing tool name");
                }
        else if (!Arguments)
                {
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Missing arguments",
                                                  "Missing arguments for '%s'",
                                                  ToolName);
                }
        else MCPOutput = Handle_tools_call(*idRequest,ToolName,*Arguments);
        if (!MCPOutput)
                {
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Unknown tool call",
                                                  "Unknown tool: '%s'",
                                                  ToolName);
                }
        return MCPOutput;
        }

// ***************************************************************************
// **** MCP Main *************************************************************
// ***************************************************************************

bool MCPServerInitalized = true;

int MCPMain(const char *InputRequestBuffer, FILE *out)
        {
        const char *ProcName = "MCPMain";
        int rc = 0;
        char *MCPOutput = NULL;
        MCPInputRequest *MCPRequest = NULL;
        JSON_Value *idRequest = NULL;
        const char *Method = NULL;
        // **************************************
        // **** Build MCP Request from JSON  ****
        // **************************************
        MCPRequest = ParseJSON_MSCPInput(InputRequestBuffer);
        if (!MCPRequest)
                {
                TERROR(("%s: Unable to parse JSON MCP Request",ProcName));
                MCPOutput = FormMCPResponse_ERROR(0,
                                                  MCPErrorCode_ParseError,
                                                  "Parse Error",
                                                  "Unable to parse JSON Request");
                TERROR(("%s: Input Request:",ProcName));
                TPRINT(("-----------------------------"));
                TPRINT(("%s",InputRequestBuffer));
                TPRINT(("-----------------------------"));
                }
        else do {
                // ****************************************
                // **** Run Method Produce JSON Output ****
                // ****************************************
                idRequest = MCPRequest->Get_id();
                Method = MCPRequest->Get_method();
                TINFO(("%s: Handling Request Method %s",ProcName,
                                                        Method
                                                        ? Method
                                                        : "<none>"));
                if (!Method)
                        {
                        TERROR(("%s: No Method in Request",ProcName));
                        MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                          MCPErrorCode_NoMethod,
                                                          "No Method",
                                                          "No method found");
                        break;
                        }
                // -------------------------------------------
                // ---- Handle notifications (without Id) ----
                // -------------------------------------------
                if (strcmp(Method,"notifications/initialized") == 0)
                        {
                        Handle_initialized(MCPRequest);
                        break;
                        }
                else if (strncmp(Method,"notifications/",14) == 0)
                        {
                        break;
                        }
                // ----------------------------------------
                // ---- Handle functional methods (Id) ----
                // ----------------------------------------
                if (!idRequest)
                        {
                        TERROR(("%s: No Id in Request",ProcName));
                        MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                          MCPErrorCode_InvalidRequest,
                                                          "No Id",
                                                          "No Id found");
                        break;
                        }
                if (strcmp(Method,"initialize") == 0)
                        {
                        MCPServerInitalized = MCPServer_OnInitialize(*MCPRequest);
                        MCPOutput = Handle_initialize(MCPRequest);
                        break;
                        }
                if (strcmp(Method,"tools/list") == 0)
                        {
                        MCPOutput = Handle_tools_list(idRequest);
                        break;
                        }
                if (strcmp(Method, "tools/call") == 0)
                        {
                        MCPOutput = Handle_tools_call(MCPRequest);
                        break;
                        }
                TERROR(("%s: Invalid Method '%s'",ProcName,Method));
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_NoMethod,
                                                  "Unknown Method",
                                                  "Method %s not found",
                                                   Method 
                                                   ? Method 
                                                   : "<none>");
                } while(0);
        if (MCPOutput) 
                {
                bool Valid = ValidateJSON(MCPOutput);
                if (!Valid)
                        {
                        TERROR(("%s: MCPOutput Failed to Parse",ProcName));
                        FreeMCPResponseBuffer(&MCPOutput);
                        MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                          MCPErrorCode_Exception,
                                                          "MCP Output invalid",
                                                          "Invalid output for %s",
                                                           Method 
                                                           ? Method 
                                                           : "<none>");
                        }
                TDEBUG(("%s: Returning %u bytes",ProcName,strlen(MCPOutput)));
                /* ---- LSP ------------------------------------------------
                fprintf(out,"Content-Length: %u\n\n",strlen(MCPOutput));
                -----------------------------------------------------------*/
                fprintf(out,"%s\n",MCPOutput);
                fflush(out);
                }
        if (MCPOutput) FreeMCPResponseBuffer(&MCPOutput);
        if (MCPRequest) delete MCPRequest;
        return 0;
        }

int MCPMain(FILE *in, FILE *out)
        {
        const char *ProcName = "MCPMain";
        int rc = 0;
        do      {
                char *stdinBuffer = ReadSTDIN_MCPRequest(stdin);
                // -----------------------
                // ---- Log Timestamp ----
                // -----------------------
                time_t Now = time(NULL);
                struct tm *Time = localtime(&Now);
                TINFO(("<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>"));
                TINFO(("<< Begin MCP Sequence @ "
                       "%u-%.2u-%.2u %.2u:%.2u:%.2u >>",Time->tm_year + 1900,
                                                        Time->tm_mon + 1,
                                                        Time->tm_mday,
                                                        Time->tm_hour,
                                                        Time->tm_min,
                                                        Time->tm_sec));
                TINFO(("<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>"));
                // -------------------------
                // ---- Process Request ----
                // -------------------------
                if (!stdinBuffer)
                        {
                        TINFO(("%s: stdin broken-pipe - Exit",ProcName));
                        break;
                        }
                rc = MCPMain(stdinBuffer,out);
                if (rc)
                        {
                        TERROR(("%s: MCPMain() Failed",ProcName));
                        }
                FreeMCPRequestBuffer(&stdinBuffer);
                } while(rc == 0);
        return rc;
        }

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

extern const char* MCPInputTestRequests[];    // Array of requests, NULL term.

const char* MCPInputTestRequest_Init =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"id\": 1,"
        "  \"method\": \"initialize\","
        "  \"params\": {"
        "    \"protocolVersion\": \"2024-11-05\","
        "    \"capabilities\": {},"
        "    \"clientInfo\": { \"name\": \"vscode-copilot\", \"version\": \"1.0.0\" }"
        "  }"
        "}";

const char* MCPInputTestRequest_List =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"id\": \"2id\","
        "  \"method\": \"tools/list\","
        "  \"params\": {}"
        "}"; 

int MCPTest(FILE *out)
        {
        const char *ProcName = "MCPTest";
        // --------------
        // Initialization
        // --------------
        int rc = MCPMain(MCPInputTestRequest_Init,out);
        // ---------
        // Tool List
        // ---------
        if (rc == 0) rc = MCPMain(MCPInputTestRequest_List,out);
        // ----------------
        // Aspect Test Call
        // ----------------
        for (unsigned i=0; rc == 0 && MCPInputTestRequests[i]; i++)
                {
                const char *Request = MCPInputTestRequests[i]; 
                rc = MCPMain(Request,out);
                }
        return rc;
        }

// ***************************************************************************
// **** main() ***************************************************************
// ***************************************************************************

int main(int argc, const char *argv[])
        {
        const char *ProcName = "main";
        int Status = 0;
        // **********************
        // **** Process Args ****
        // **********************
        bool TestMode = false;
        if (argc == 2 && stricmp(argv[1],"test") == 0)
                {
                TestMode = true;
                }
        // *********************
        // **** Setup Trace ****
        // *********************
        const char *TraceFilename = !TestMode ? MCPServerLOG : NULL;
        FILE *LevelTraceFile = NULL;
        if (TraceFilename)
                {
                LevelTraceFile = fopen(TraceFilename,"w");
                if (!LevelTraceFile)
                        {
                        TERROR(("Can't Open: %s",TraceFilename));
                        return 1;
                        }
                SetLevelTraceFunction(LevelTraceFileTrace,LevelTraceFile);
                }
        SetMaxTraceLevel(TRACELEVEL_DEBUG);  
        // ********************
        // **** Initialize ****
        // ********************
        InitJSONParser();
        // **********************
        // **** Run MCP Test ****
        // **********************
        if (TestMode)
                {
                Status = MCPTest(stdout);
                }
        else    {
                if (isatty(fileno(stderr)))
                        {
                        fprintf(stderr,"Interactive: Enter one JSON request "
                                       "per line. Send EOF (%s) to Exit.\n",
                                       EOFSequence);
                        }
                Status = MCPMain(stdin,stdout);
                }
        if (MCPServerInitalized) MCPServer_OnShutdown();
        DeinitJSONParser();
        // ***********************
        // **** Cleanup Trace ****
        // ***********************
        if (LevelTraceFile)
                {
                SetLevelTraceFunction(NULL,NULL);
                fclose(LevelTraceFile);
                }
        return Status;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
