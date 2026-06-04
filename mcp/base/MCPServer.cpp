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
//      Version: 02/13/2026 (reviewd and corrected by Opus on 06/23/2026)
//
//      Specification: https://modelcontextprotocol.io/specification/
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
#include <MainArguments.h>
#include <MEMprintf.h>

#include <MCPServerJSON.h>

#define MCPServer_Name "MCPServer_HelloWorld"
#define MCPServer_Version "0.1.0"

#define MCPServerLOG "MCPServerLog.txt"                 // Log File.

// ******************************
// **** Tools in this module ****
// ******************************

char* MCPtool_helloWorld(JSON_Value *idRequest, JSON_Object *Arguments);
              
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
// ---- FormMCPResponse_ERROR ------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_ERROR(JSON_Value *idRequest,
                            int Code, 
                            const char *Message, 
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
        Canvas.printf("\"code\":\"%d\",",Code);
        Canvas.printf("\"message\":\"%s\",",Message);
        // ----------------------------
        // ---- Foremat Error Text ----
        // ----------------------------
        va_list Args;
        MemoryPrintf ErrorCanvas;
        va_start(Args, ErrorFormat);
        ErrorCanvas.vprintf(ErrorFormat, Args);
        va_end(Args);
        const char *ErrorText = ErrorCanvas.GetBuffer();
        TDEBUG(("%s: Error (id=%s): %s: %s",ProcName,
                                             id2string(idRequest),
                                             Message,
                                             ErrorText ? ErrorText 
                                                       : "(no detail)"));
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

struct MCPToolInfo_t  
        {
        const char *Name;
        const char *Description;
        int nProperties;
        const char* Properties[10];
        const char *Required;
        };

#define MCPToolInfoCount 1

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
                        "\"name\":{\"type\":\"string\"}"
                        },
                // [Required]
                "\"name\""
                },
                // -------------------------------
                // ---- get_block_info (Test) ----
                // -------------------------------
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
        for (int i = 0; i < MCPToolInfoCount; i++)
                {
                Canvas.printf("{");
                Canvas.printf("\"name\":\"%s\",",MCPToolInfo[i].Name);
                Canvas.printf("\"description\":\"%s\",",MCPToolInfo[i].Description);
                Canvas.printf("\"inputSchema\":{");
                Canvas.printf("\"type\":\"object\",");
                Canvas.printf("\"properties\":{");
                int nProperties = MCPToolInfo[i].nProperties;
                for (int j=0; j < nProperties; j++)
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
        else if (strcmp(ToolName,"helloWorld") == 0)  
                {
                MCPOutput = MCPtool_helloWorld(idRequest,Arguments);
                }
        else    {
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Unknown tool call",
                                                  "Unknown tool: '%s'",
                                                  ToolName);
                }
        return MCPOutput;
        }
// ---------------------------------------------------------------------------
// ---- Handle_helloWorld ----------------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_helloWorld(JSON_Value *idRequest, const char *Name)
        {
        const char *ProcName = "FormMCPResponse_helloWorld";
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

        Note: The "type" field is one of:
                - "text": Plain text content.
                - "markdown": Markdown-formatted text.
                - "html": HTML-formatted content.
                - "image": An image, which would include a URL and alt text.
                - "list": A list, which would include an array of items.
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
        char *jsonName = EscapeJSONString(Name);
        Canvas.printf("\"type\":\"text\","
                      "\"text\":\"How are you, %s.\"",
                      jsonName ? jsonName : Name);
        FreeEscapeJSONString(&jsonName);
        Canvas.printf("}],"); /* content */
        Canvas.printf("\"isError\":false");
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

char* MCPtool_helloWorld(JSON_Value *idRequest, JSON_Object *Arguments)
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
        if (!Arguments) return NULL;
        char *MCPOutput = NULL;
        const char *Name = Arguments->Find_member_string("name");
        if (!Name)
                {
                TERROR(("%s: Missing Argument: 'name'",ProcName));
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Missing required parameter: name");
                }
        else    {
                MCPOutput = FormMCPResponse_helloWorld(idRequest,Name);
                }
        return MCPOutput;
        }

// ***************************************************************************
// **** MCP Main *************************************************************
// ***************************************************************************

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
                        MCPOutput = Handle_initialize(MCPRequest);
                        break;
                        }
                if (strcmp(Method,"tools/list") == 0)
                        {
                        MCPOutput = Handle_tools_list(idRequest);
                        break;
                        }
                if (strcmp(Method,"tools/call") == 0)
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

const char* MCPInputTestRequest_Call =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"helloWorld\","
         "   \"arguments\": { \"name\": \"eric\" }"
         " }"
         "}"; 

const char *MCPInputTestRequest = MCPInputTestRequest_Call;

int MCPTest(FILE *out)
        {
        const char *ProcName = "MCPTest";
        TraceJSON(MCPInputTestRequest);
        return MCPMain(MCPInputTestRequest,out);
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
