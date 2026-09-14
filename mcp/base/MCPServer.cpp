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
//      Specification: https://modelcontextprotocol.io/specification/
//      
//      To Register in VSCode: Ctrl-Shift-P > MCP:List Servers > + Add Server
//      To Control Server: Ctrl-Shift-P > MCP:List Servers > Start / Stop
//

#include <ctype.h>
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

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#ifdef _WIN32
        #include <SnapStack.h>
#endif

#include <MCPServerCore.h>

#define MCPServerLOG "MCPServerLog_%u.txt"              // LevelTrace Logfile.
#ifdef _WIN32
        #define MCPServerLOGPath getenv("TEMP")
#else
        #define MCPServerLOGPath getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp"
#endif

#ifndef ReadSTDIN_MCPMessage_MAXSIZE
        #define ReadSTDIN_MCPMessage_MAXSIZE (20*1024*1024)
#endif

static bool MCPJSONTrace = false;               // Trace JSON from/to client.

// ------------------------------------------------------------
// ---- Prompt tool "emulator" for clients lacking support ----
// --------------------------------------------------------
//
//      - MCPtool_listPrompts   Like: "prompts/list"
//      - MCPtool_getPrompt     Like: "prompts/get"
//
// ------------------------------------------------------------

#ifndef IncludePromptTools
        #define IncludePromptTools 1
#endif

// ***************************************************************************
// **** Share Program Arguments **********************************************
// ***************************************************************************

static int MCPServer_argc = 0;
static const char** MCPServer_argv = NULL;

int MCPServer_getArgc()
        {
        return MCPServer_argc;
        }

const char** MCPServer_getArgv()
        {
        return MCPServer_argv;
        }

// ***************************************************************************
// **** Utilties *************************************************************
// ***************************************************************************

// ----------------------------------
// ---- Extract delimited tokens ----
// ----------------------------------

const char* MCP_strtok(const char *Start, 
                       size_t *Length, 
                       const char *Delimiters)
        {
        if (!Start)
                {
                *Length = 0;
                return NULL;
                }
        Start += *Length;
        while (*Start && strchr(Delimiters,*Start)) Start++;
        if (!*Start)
                {
                *Length = 0;
                return NULL;
                }
        const char *End = Start;
        while (*End && !strchr(Delimiters,*End)) End++;
        *Length = (unsigned)(End - Start);
        return Start;
        }

const char* MCP_ltrim(const char *Start, size_t *Length)
        {
        while (*Length && isspace(*Start))
                {
                Start++;
                *Length = *Length - 1;
                }
        return Start;
        }

size_t* MCP_rtrim(const char *Start, size_t *Length)
        {
        while (*Length && isspace(Start[*Length - 1]))
                {
                *Length = *Length - 1;
                }
        return Length;
        }        

// ---------------------------------------------------
// ---- Case insenstive strstr - Completion match ----
// ---------------------------------------------------

const char* MCP_stristr(const char *String, const char *ToFind)
        {
        if (!ToFind || !*ToFind) return String; // Found anything.
        const char *spos = String;
        const char *fpos = ToFind;
        while (*spos && *fpos)
	        {
	        String = spos;
	        while (*spos && *fpos && toupper(*spos) == toupper(*fpos))
	                {
	                spos++;
	                fpos++;
	                }
	        if (*fpos)			// No match...
	                {
	                spos = String + 1;
	                fpos = ToFind;
	                }
	        }
    return *fpos == '\0' ? String : NULL;
    }

// -------------------------------------------------
// ---- MCP_localtime - Thread safe localtime() ----
// -------------------------------------------------

struct tm* MCP_localtime(struct tm *tmTime, time_t tTime)
        {
        static CriticalSection CS;
        CS.Take();
        struct tm *Time = localtime(&tTime);
        memcpy(tmTime,Time,sizeof(struct tm));
        CS.Release();
        return tmTime;
        }

// ***************************************************************************
// **** LevelTrace Redirect **************************************************
// ***************************************************************************

static int LevelTraceFileTrace(void *UserPtr,
                               unsigned Level, 
                               const char *Format, 
                               va_list Args)
        {
        static Mutex LevelTraceMutex;
        static time_t LastTraceTime = 0;
        FILE *LevelTraceFile = (FILE *)UserPtr;
        if (Level > GetMaxTraceLevel()) return 0;
        if (!LevelTraceFile) return 0;
        MemoryPrintf Canvas;
        // -------------------
        // ---- Timestamp ----
        // -------------------
        time_t Now = time(NULL);
        if (Now - LastTraceTime > 5)
                {
                LevelTraceMutex.Take();
                if (Now - LastTraceTime > 5)
                        {
                        bool FirstLine = LastTraceTime == 0;
                        LastTraceTime = Now;
                        LevelTraceMutex.Release();
                        struct tm tmBuf;
                        MCP_localtime(&tmBuf,Now);
                        Canvas.printf("%s"
                                      "%u-%.2u-%.2u %.2u:%.2u:%.2u\n",
                                      FirstLine ? "" : "\n",
                                      tmBuf.tm_year + 1900,
                                      tmBuf.tm_mon + 1,
                                      tmBuf.tm_mday,
                                      tmBuf.tm_hour,
                                      tmBuf.tm_min,
                                      tmBuf.tm_sec);
                        }
                else LevelTraceMutex.Release();
                }
        // ----------------
        // ---- Format ----
        // ----------------
        const char *Tag = "";
        if (Level == TRACELEVEL_ERROR) Tag = "ERROR: ";
        else if (Level == TRACELEVEL_INFO) Tag = "INFO:  ";
        else if (Level == TRACELEVEL_DEBUG) Tag = "DEBUG: ";
        MemoryPrintf BodyCanvas;
        BodyCanvas.vprintf(Format,Args);
        const char *Body = BodyCanvas.GetBuffer();
        if (Body)
                {
                size_t LineLen = 0;
                bool FirstLine = true;
                const char *Line = MCP_strtok(Body,&LineLen,"\r\n");
                while (Line)
                        {
                        Canvas.printf("%s%s%.*s\n",
                                      Tag,
                                      FirstLine ? "" : "  ",
                                      LineLen,Line);
                        FirstLine = false;
                        Line = MCP_strtok(Line,&LineLen,"\r\n");
                        }
                }
        // ------------------------
        // ---- Lock and Trace ----
        // ------------------------
        const char *TraceOutput = Canvas.GetBuffer();
        if (TraceOutput)
                {
                LevelTraceMutex.Take();
                fputs(TraceOutput,LevelTraceFile);
                fflush(LevelTraceFile);
                LevelTraceMutex.Release();
                }
        return 1;
        }

static char* ReadTraceFile(bool Tail, size_t ReadLength)
        {
        static const char *ProcName = "ReadTraceFile";
        MemoryPrintf TraceNameCanvas;
        TraceNameCanvas.printf("%s%c%s_%u.log",MCPServerLOGPath,
                                               FILE_SEPERATOR,
                                               MCPServer_Name,
                                               getpid());
        const char *Filename = TraceNameCanvas.GetBuffer();
        FILE *in = fopen(Filename,"r");
        if (!in)
                {
                TERROR(("%s: Can't Open %s (errno = %u)",ProcName,
                                                         Filename,
                                                         errno));
                return NULL;
                }
        fseek(in,0,SEEK_END);
        size_t FileSize = (size_t)ftell(in);
        if (!FileSize)
                {
                TERROR(("%s: File %s is empty",ProcName,Filename));
                fclose(in);
                return NULL;
                }
        size_t BytesToRead = ReadLength < FileSize ? ReadLength : FileSize;
        char *Document = (char *)malloc(BytesToRead+1);
        if (!Document)
                {
                TERROR(("%s: malloc(%u) failed for %s",
                        ProcName,
                        (unsigned)(BytesToRead+1),
                        Filename));
                fclose(in);
                return NULL;
                }
        TINFO(("%s: Loading from: %s (%s %u of %u bytes)",
               ProcName,
               Filename,
               Tail ? "tail" : "head",
               (unsigned)BytesToRead,
               (unsigned)FileSize));
        if (Tail && BytesToRead < FileSize)
                {
                fseek(in,(long)(FileSize - BytesToRead),SEEK_SET);
                }
        else    {
                fseek(in,0,SEEK_SET);
                }
        size_t nRead = fread(Document,1,BytesToRead,in);
        if (ferror(in))
                {
                TERROR(("%s: Read Error %s (errno = %u)",ProcName,
                                                         Filename,
                                                         errno));
                fclose(in);
                free(Document);
                return NULL;
                }
        fclose(in);
        Document[nRead] = '\0';
        return Document;
        }

// ***************************************************************************
// **** Load JSON Input Request (Synchronous) ********************************
// ***************************************************************************

#ifdef _DEBUG
        #define ReadSTDIN_MCPMessage_BufferSIZE 128
#else
        #define ReadSTDIN_MCPMessage_BufferSIZE 2048
#endif /* _DEBUG */            

static char ReadSTDIN_OVERFLOW_SENTINEL[ReadSTDIN_MCPMessage_BufferSIZE];

void FreeMCPMessageBuffer(char **Buffer)
        {
        if (Buffer && *Buffer)
                {
                if (*Buffer != ReadSTDIN_OVERFLOW_SENTINEL)
                        {
                        free(*Buffer);
                        }
                *Buffer = NULL;
                }
        return;
        }

char* ReadSTDIN_MCPMessage(FILE *in)    // "getline()" - reads until EOL.
        {
        const char *ProcName = "ReadSTDIN_MCPMessage";
        size_t BufferSize = ReadSTDIN_MCPMessage_BufferSIZE;
        char *Buffer = (char *)malloc(BufferSize);
        if (!Buffer)
                {
                TERROR(("%s: Unable to malloc(%u)", ProcName,BufferSize));
                return NULL;
                }
        size_t Used = 0;
        do      {
                // ---------------------------
                // --- Read until Newline ----
                // ---------------------------
                char *Pos = Buffer + Used;
                char *readBuffer = fgets(Pos,BufferSize-Used,in);
                if (!readBuffer)
                        {
                        if (feof(in)) TDEBUG(("%s: fgets() - EOF",ProcName));
                        else TERROR(("%s: fgets() Error",ProcName));
                        FreeMCPMessageBuffer(&Buffer);
                        break;
                        }
                size_t readBufferLength = strlen(readBuffer);
                if (!readBufferLength)
                        {
                        TERROR(("%s: Unexpected fgets() no read",ProcName));
                        FreeMCPMessageBuffer(&Buffer);
                        break;
                        }
                if (readBuffer[readBufferLength-1] == '\n')
                        {
                        readBuffer[readBufferLength-1] = '\0';
                        break;
                        }
                // --------------------------
                // --- Overflow handling ----
                // --------------------------
                if (Buffer == ReadSTDIN_OVERFLOW_SENTINEL)
                        {
                        continue; // ...reading until the end of message.
                        }
                if (BufferSize > ReadSTDIN_MCPMessage_MAXSIZE)
                        {
                        TERROR(("%s: Size governor triggered: "
                                "ReadSTDIN_MCPMessage_MAXSIZE (0x%X bytes)",
                                ProcName,
                                ReadSTDIN_MCPMessage_MAXSIZE));
                        FreeMCPMessageBuffer(&Buffer);
                        Buffer = ReadSTDIN_OVERFLOW_SENTINEL;
                        BufferSize = ReadSTDIN_MCPMessage_BufferSIZE;
                        Used = 0;
                        continue;
                        }
                // --------------------------
                // --- Reallocate Buffer ----
                // --------------------------
                Used += readBufferLength;
                size_t NewBufferSize = BufferSize * 2;
                char *NewBuffer = (char *)realloc(Buffer,NewBufferSize);
                if (!NewBuffer)
                        {
                        TERROR(("%s: Unable to realloc(%u)",ProcName,
                                                            NewBufferSize));
                        FreeMCPMessageBuffer(&Buffer);
                        break;
                        }
                Buffer = NewBuffer;
                BufferSize = NewBufferSize;
                } while (1);
        return Buffer;
        }      

// ***************************************************************************
// **** MCP Extensions *******************************************************
// ***************************************************************************

// -----------------------------------------------
// ---- MCP Async Extension (Internal Hooks) ----
// -----------------------------------------------

void (*InitMCPAsyncHook)() = NULL;
void (*DeinitMCPAsyncHook)() = NULL;
char* (*Handle_async_object)(void *UserPtr) = NULL;
void (*Discard_async_object)(void *UserPtr) = NULL;
bool (*Cancel_async_object)(MCPInputRequest &MCPRequest) = NULL;

// -----------------------------------------------
// ---- MCP Sample Extension (Internal Hooks) ----
// -----------------------------------------------

void (*OnInitMCPSampleHook)(MCPInputRequest &MCPRequest) = NULL;
char* (*Handle_sampling_hook)(MCPInputRequest &MCPRequest) = NULL;
void (*DeinitMCPSampleHook)() = NULL;

// -----------------------------------------------
// ---- MCP Sample Extension (Internal Hooks) ----
// -----------------------------------------------

void (*OnStartupMCPSampleExHook)() = NULL;

// -----------------------------------------------
// ---- MCP MRTR Extension (Internal Hooks) ----
// -----------------------------------------------

char* (*Handle_MRTR_hook)(MCPInputRequest &MCPRequest) = NULL;
void (*Handle_MRTR_reapttl)() = NULL;
void (*OnShutdownMRTRHook)() = NULL;

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
           "result": {
             "isError": false
           }
        }
        ----------------------------------------------------------- */
        JSON_Object root;
        root.Add_member_string("jsonrpc", "2.0");
        root.Add_member_value("id", *idRequest);
        JSON_Object *result = root.Add_member_object("result");
        if (result)
                {
                if (Text)
                        {
                        JSON_Value_Array *content;
                        content = result->Add_member_array("content");
                        if (content)
                                {
                                JSON_Object *textItem;
                                textItem = content->Add_value_object();
                                if (textItem)
                                        {
                                        textItem->Add_member_string("type","text");
                                        textItem->Add_member_string("text",Text);
                                        }
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
        char *jsonMessage = CombinedMsg ? EscapeJSONString(CombinedMsg) : NULL;
        Canvas.printf("\"message\":\"%s\",",jsonMessage ? jsonMessage : "");
        if (jsonMessage) FreeEscapeJSONString(&jsonMessage);
        TDEBUG(("%s: Error (id=%s): %s",ProcName,
                                        id2string(idRequest),
                                        CombinedMsg ? CombinedMsg : ErrorTag));
        // --------------------------------
        // ---- Emit full "data" field ----
        // --------------------------------
        char *jsonErrorText = ErrorText ? EscapeJSONString(ErrorText) : NULL;
        bool HintTrace = Code == MCPErrorCode_ParseError
                      || Code == MCPErrorCode_Exception;
        Canvas.printf("\"data\":\"%s%s\"",
                      jsonErrorText ? jsonErrorText : "",
                      HintTrace
                      ? " (readTrace tool fetches details)"
                      : "");
        if (jsonErrorText) FreeEscapeJSONString(&jsonErrorText);
        // ----------------------------
        Canvas.printf("}");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- Return_MCPOutput_Pending ---------------------------------------------
// ------------------------------
//
//      Asynchronous tool/call handling (Deferred Response Pattern).
//
//      Requires: MCPServer_Asynchronous = true  (in the aspect .cpp)
//
//      Normally, Handle_tools_call() returns an MCPOutput JSON string
//      which MCPMain() immediately sends to the client. For tools that
//      take a long time (network calls, shell commands, etc.), the 
//      aspect can defer the response:
//
//      Flow:
//              1. Handle_tools_call() saves the request id (idRequest)
//                 in a structure, starts a worker thread, and returns
//                 Return_MCPOutput_Pending().
//
//              2. MCPMain() recognizes the sentinel pointer (identity
//                 comparison, not strcmp) and suppresses output --
//                 no response is sent to the client yet.
//
//      Then the worker thread delivers via EITHER path:
//
//         (A) PostNotifyAction - struct-based, main-thread formatting:
//              3. The worker thread packages the result into a struct
//                 (which must include the original idRequest) and calls:
//                      PostNotifyAction(pMyResultStruct)
//              4. The main loop dispatches to Handle_notify_action()
//                 on the main thread. The aspect builds the JSON-RPC
//                 response (with the original id) and returns it as 
//                 MCPOutput, which is sent to the client.
//
//         (B) PostMCPOutput - direct, thread builds full response:
//              3. The worker thread builds the complete MCPOutput JSON
//                 (must include the correct JSON-RPC id) and calls:
//                      PostMCPOutput(MCPOutput)
//                 The main loop validates and sends it to the client.
//                 No Handle/Discard handlers needed for this path.
//
//      See: MCPServerCore.h - "Return_MCPOutput_Pending" for details.
//
// ---------------------------------------------------------------------------

static const char *MCPOutput_Pending = "__MCPOutput_Response_Pending_";

char* Return_MCPOutput_Pending() 
        {
        return (char*)MCPOutput_Pending;        // Special "MCPOutput" value!
        }

// ***************************************************************************
// **** MCP Protocol Version Policy ******************************************
// **********************************
//
//      MCPServerJSON reports the raw per-request version; the ceiling
//      clamp and the initialize-negotiated fallback live here. Touched
//      only on the main message-loop thread.
//
// ***************************************************************************

static int NegotiatedVersion = MCPProtocolVersion_ServerMax;

static void MCPSetNegotiatedVersion(int Version)
        {
        NegotiatedVersion = Version;
        return;
        }

int MCPProtocolVersion(MCPInputRequest &MCPRequest)
        {
        int Client = MCPRequest.Version();
        int Effective = Client > NegotiatedVersion 
                        ? NegotiatedVersion
                        : Client;
        return Effective;
        }

// ***************************************************************************
// **** Handle Initialize ****************************************************
// ***************************************************************************

static char* FormMCPResponse_initialize(JSON_Value *idRequest,
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
        int ClientVersion = MCPProtocolVersionInt(protocolVersion);
        int EffectiveVersion = ClientVersion > MCPProtocolVersion_ServerMax
                              ? MCPProtocolVersion_ServerMax
                              : ClientVersion;
        MCPSetNegotiatedVersion(EffectiveVersion);
        Canvas.printf("\"protocolVersion\":\"%d-%.2d-%.2d\",",
                      EffectiveVersion / 10000,
                      EffectiveVersion / 100 % 100,
                      EffectiveVersion % 100);
        Canvas.printf("\"capabilities\":{%s},",MCPServer_Capabilities);
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

static char* Handle_initialize(MCPInputRequest &MCPRequest)
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
        MemoryPrintf Canvas;
        JSON_Value *idRequest = MCPRequest.Get_id();
        const char *protocolVersion = MCPRequest.Get_param_string("protocolVersion");
        JSON_Object *capabilities = MCPRequest.Get_param_object("capabilities");
        JSON_Object *clientInfo = MCPRequest.Get_param_object("clientInfo");
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
        TINFO(("%s: Client offered protocolVersion=%s",ProcName,
                                                       protocolVersion
                                                       ? protocolVersion
                                                       : "<none>"));
        if (MCPProtocolVersionInt(protocolVersion) == MCPProtocolVersion_Invalid)
                {
                TERROR(("%s: Missing/malformed protocolVersion",ProcName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing or malformed required "
                                             "parameter: protocolVersion");
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

static void Handle_initialized(MCPInputRequest &MCPRequest)
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
// **** Handle server/discover ***********************************************
// ***************************************************************************
//
//      Stateless discovery (MCP 2026-07-28). Reports supported protocol
//      versions, capabilities, and serverInfo in one request -- no state,
//      no OnInitialize. Modern clients MAY probe this before any other RPC.
//
// ***************************************************************************

static char* Handle_server_discover(JSON_Value *idRequest)
        {
        const char *ProcName = "Handle_server_discover";
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": "discover-1",
          "result": {
            "resultType": "complete",
            "supportedVersions": ["2026-07-28", "2025-11-25", "2024-11-05"],
            "capabilities": { "tools": {} },
            "_meta": {
              "io.modelcontextprotocol/serverInfo":
                            { "name": "MCPServer", "version": "0.1.0" }
            }
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
        Canvas.printf("\"resultType\":\"complete\",");
        Canvas.printf("\"supportedVersions\":["
                      "\"%d-%.2d-%.2d\",\"%d-%.2d-%.2d\",\"%d-%.2d-%.2d\"],",
                      MCPProtocolVersion_20260728 / 10000,
                      MCPProtocolVersion_20260728 / 100 % 100,
                      MCPProtocolVersion_20260728 % 100,
                      MCPProtocolVersion_20251125 / 10000,
                      MCPProtocolVersion_20251125 / 100 % 100,
                      MCPProtocolVersion_20251125 % 100,
                      MCPProtocolVersion_20241105 / 10000,
                      MCPProtocolVersion_20241105 / 100 % 100,
                      MCPProtocolVersion_20241105 % 100);
        Canvas.printf("\"capabilities\":{%s},",MCPServer_Capabilities);
        Canvas.printf("\"_meta\":{");
        Canvas.printf("\"io.modelcontextprotocol/serverInfo\":{");
        Canvas.printf("\"name\":\"%s\",",MCPServer_Name);
        Canvas.printf("\"version\":\"%s\"",MCPServer_Version);
        Canvas.printf("}"); /* serverInfo */
        Canvas.printf("}"); /* _meta */
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ***************************************************************************
// **** MCP Tool Handlers ****************************************************
// ***************************************************************************

// ======================================
// ==== Internal Tools (Declaration) ====
// ======================================

extern const unsigned MCPInternToolInfoCount;
extern MCPToolInfo_t MCPInternToolInfo[];

// =========================================
// ==== Extern Tools (Aspect-Populated) ====
// =========================================

unsigned MCPExternToolInfoCount = 0;
MCPToolInfo_t* MCPExternToolInfo = NULL;

// --------------------
// ---- Trace Tool ----
// --------------------

static char* MCPtool_readTrace(JSON_Value&, JSON_Object&);

// ----------------------
// ---- Prompt Tools ----
// ----------------------

static char* MCPtool_listPrompts(JSON_Value&, JSON_Object&);
static char* MCPtool_getPrompt(JSON_Value&, JSON_Object&);

// --------------------
// ---- About Tool ----
// --------------------

static char* MCPtool_about(MCPInputRequest&, JSON_Object&);

// ---------------------------------------------------------------------------
// ---- Handle_tools_list ----------------------------------------------------
// ---------------------------------------------------------------------------

static void FormToolsList(MemoryPrintf &Canvas, 
                          unsigned nTools, 
                          MCPToolInfo_t ToolInfo[])
        {
        for (unsigned i = 0; i < nTools; i++)
                {
                TINFO(("%s: Exporting Tool: %s",MCPServer_Name,
                                                ToolInfo[i].Name));
                Canvas.printf("{");
                Canvas.printf("\"name\":\"%s\",",ToolInfo[i].Name);
                char *EscDesc = EscapeJSONString(ToolInfo[i].Description);
                Canvas.printf("\"description\":\"%s\",",EscDesc ? EscDesc : "");
                FreeEscapeJSONString(&EscDesc);
                Canvas.printf("\"inputSchema\":{");
                Canvas.printf("\"type\":\"object\",");
                Canvas.printf("\"properties\":{");
                unsigned nProperties = ToolInfo[i].nProperties;
                for (unsigned j=0; j < nProperties; j++)
                        {
                        Canvas.printf(j + 1 == nProperties ? "%s" : "%s,",
                                      ToolInfo[i].Properties[j]);
                        }
                Canvas.printf("},"); /* properties */
                Canvas.printf("\"required\":[%s]",ToolInfo[i].Required
                                                  ? ToolInfo[i].Required
                                                  : "");
                Canvas.printf("}"); /* inputSchema */
                if (i + 1 == nTools) Canvas.printf("}"); 
                else Canvas.printf("},");
                }
        return;
        }

static char* Handle_tools_list(JSON_Value *idRequest)
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
        bool NeedComma = false;
        if (MCPInternToolInfoCount)
                {
                FormToolsList(Canvas,MCPInternToolInfoCount,MCPInternToolInfo);
                NeedComma = true;
                }
        if (MCPToolInfoCount)
                {
                if (NeedComma) Canvas.printf(",");
                FormToolsList(Canvas,MCPToolInfoCount,MCPToolInfo);
                NeedComma = true;
                }
        if (MCPExternToolInfoCount)
                {
                if (NeedComma) Canvas.printf(",");
                FormToolsList(Canvas,MCPExternToolInfoCount,MCPExternToolInfo);
                }
        Canvas.printf("]"); /* tools */
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ----------------------------------------------
// ---- FormMCPNotification_ToolsListChanged ----
// ----------------------------------------------

char* FormMCPNotification_ToolsListChanged()
        {
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"method\":\"notifications/tools/list_changed\"");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ---- Handle_tools_call_dispatch -------------------------------------------
// ---------------------------------------------------------------------------

// --------------------------------------------
// ---- Advanced Tool Hook (Set to Enable) ----
// --------------------------------------------

char* (*Handle_tools_call_hook)(MCPInputRequest &MCPRequest,
                                JSON_Value &idRequest,
                                const char *ToolName, 
                                JSON_Object &Arguments) = NULL;

// ------------------------------------
// ---- Handle_tools_call_dispatch ----
// ------------------------------------

static char* Handle_tools_call_dispatch(MCPInputRequest &MCPRequest)
        {
        const char *ProcName = "Handle_tools_call_dispatch";
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
        char *MCPOutput = NULL;
        JSON_Value *idRequest = MCPRequest.Get_id();
        // ----------------------------------------------------
        // ---- MRTR: a resume retry carries requestState -----
        // ----------------------------------------------------
        if (MCPRequest.Get_param("requestState"))
                {
                if (Handle_MRTR_hook)
                        {
                        MCPOutput = Handle_MRTR_hook(MCPRequest);
                        }
                if (!MCPOutput)
                        {
                        MCPOutput = Handle_tools_call_resume(MCPRequest);
                        }
                if (!MCPOutput)
                        {
                        TERROR(("%s: No pending MRTR for id: %s",
                                ProcName,
                                id2string(idRequest)));
                        MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                          MCPErrorCode_InvalidParms,
                                                          "Invalid Request State",
                                                          "No pending MRTR for id: %s",
                                                          id2string(idRequest));
                        }
                return MCPOutput;
                }
        const char *ToolName = MCPRequest.Get_param_string("name");
        JSON_Object *Arguments = MCPRequest.Get_param_object("arguments");
        // ---------------------------
        // ---- Validate Request  ----
        // ---------------------------
        if (!ToolName)
                {
                TERROR(("%s: Missing tool name",ProcName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing tool name");
                }
        if (!Arguments)
                {
                TERROR(("%s: Missing arguments for: %s",ProcName,ToolName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing arguments for '%s'",
                                             ToolName);
                // static JSON_Object NoArgs;
                // Arguments = &NoArgs;                 // Alternative.
                }
        // ----------------------------------
        // ---- Dispatch: Internal Tools ----
        // ----------------------------------
        if (strcmp(ToolName,"about_tsar-mcp") == 0)
                {
                return MCPtool_about(MCPRequest,*Arguments);
                }
        if (strcmp(ToolName,"readTrace") == 0)
                {
                return MCPtool_readTrace(*idRequest,*Arguments);
                }
        if (strcmp(ToolName,"listPrompts") == 0)
                {
                return MCPtool_listPrompts(*idRequest,*Arguments);
                }
        if (strcmp(ToolName,"getPrompt") == 0)
                {
                return MCPtool_getPrompt(*idRequest,*Arguments);
                }
        // --------------------------------------
        // ---- Dispatch: Advanced Tool Hook ----
        // --------------------------------------
        TINFO(("%s: Dispatching to handler (id=%s): %s",ProcName,
                                                        id2string(idRequest),
                                                        ToolName));
        if (Handle_tools_call_hook)
                {
                MCPOutput = Handle_tools_call_hook(MCPRequest,
                                                   *idRequest,
                                                   ToolName,
                                                   *Arguments);
                if (MCPOutput)
                        {
                        TDEBUG(("%s: Hook handled tool: %s",ProcName,
                                                            ToolName));
                        }
                }
        // ------------------------------------
        // ---- Dispatch: Standard Handler ----
        // ------------------------------------
        if (!MCPOutput)
                {
                MCPOutput = Handle_tools_call(*idRequest,ToolName,*Arguments);
                }
        if (!MCPOutput)
                {
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Unknown tool: '%s'",
                                                  ToolName);
                }
        return MCPOutput;
        }

// ***************************************************************************
// **** MCP Prompt Handlers **************************************************
// ***************************************************************************

// ===========================================
// ==== Extern Prompts (Aspect-Populated) ====
// ===========================================

unsigned MCPExternPromptInfoCount = 0;
MCPPromptInfo_t* MCPExternPromptInfo = NULL;

// ---------------------------------------------------------------------------
// ---- Handle_prompts_list --------------------------------------------------
// ---------------------------------------------------------------------------

static void FormPromptsList(MemoryPrintf &Canvas,
                            unsigned nPrompts,
                            MCPPromptInfo_t PromptInfo[])
        {
        for (unsigned i = 0; i < nPrompts; i++)
                {
                TINFO(("%s: Exporting Prompt: %s",MCPServer_Name,
                                                  PromptInfo[i].Name));
                Canvas.printf("{");
                Canvas.printf("\"name\":\"%s\",",PromptInfo[i].Name);
                char *EscDesc = EscapeJSONString(PromptInfo[i].Description);
                Canvas.printf("\"description\":\"%s\",",EscDesc ? EscDesc : "");
                FreeEscapeJSONString(&EscDesc);
                Canvas.printf("\"arguments\":[");
                unsigned nArguments = PromptInfo[i].nArguments;
                for (unsigned j = 0; j < nArguments; j++)
                        {
                        Canvas.printf(j + 1 == nArguments ? "%s" : "%s,",
                                      PromptInfo[i].Arguments[j]);
                        }
                Canvas.printf("]"); /* arguments */
                if (i + 1 == nPrompts) Canvas.printf("}");
                else Canvas.printf("},");
                }
        return;
        }

static char* Handle_prompts_list(JSON_Value *idRequest)
        {
        static const char *ProcName = "Handle_prompts_list";
        /* ----- OUTPUT ------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 2,
          "result": {
            "prompts": [
              {
                "name": "update_summary",
                "description": "Check and update 0Summary.md",
                "arguments": []
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
        Canvas.printf("\"prompts\":[");
        bool NeedComma = false;
        if (MCPPromptInfoCount)
                {
                FormPromptsList(Canvas,MCPPromptInfoCount,MCPPromptInfo);
                NeedComma = true;
                }
        if (MCPExternPromptInfoCount)
                {
                if (NeedComma) Canvas.printf(",");
                FormPromptsList(Canvas,MCPExternPromptInfoCount,
                                       MCPExternPromptInfo);
                }
        Canvas.printf("]"); /* prompts */
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ------------------------------------------------
// ---- FormMCPNotification_PromptsListChanged ----
// ------------------------------------------------

char* FormMCPNotification_PromptsListChanged()
        {
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"method\":\"notifications/prompts/list_changed\"");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- Handle_prompts_get_dispatch ------------------------------------------
// ---------------------------------------------------------------------------

// --------------------------------
// ---- FormMCPResponse_prompt ----
// --------------------------------

char* FormMCPResponse_prompt(JSON_Value &idRequest,
                             const char *Description,
                             const char *PromptText)
        {
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        // ----------------
        // ---- result ----
        // ----------------
        Canvas.printf("\"result\":{");
        Canvas.printf("\"description\":\"%s\",",Description);
        Canvas.printf("\"messages\":[{");
        Canvas.printf("\"role\":\"user\",");
        Canvas.printf("\"content\":{");
        Canvas.printf("\"type\":\"text\",");
        char *EscapedPrompt = EscapeJSONString(PromptText);
        Canvas.printf("\"text\":\"%s\"",EscapedPrompt ? EscapedPrompt : "");
        FreeEscapeJSONString(&EscapedPrompt);
        Canvas.printf("}"); /* content */
        Canvas.printf("}]"); /* messages */
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// -----------------------------------------------
// ---- Advanced Propmpt Hook (Set to Enable) ----
// -----------------------------------------------

char* (*Handle_prompts_get_hook)(MCPInputRequest &MCPRequest,
                                 JSON_Value &idRequest,
                                 const char *PromptName, 
                                 JSON_Object &Arguments) = NULL;

// -------------------------------------
// ---- Handle_prompts_get_dispatch ----
// -------------------------------------

static char* Handle_prompts_get_dispatch(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "Handle_prompts_get_dispatch";
        /* ----- INPUT -------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "prompts/get",
          "params": {
            "name": "update_summary",
            "arguments": {}
          }
        }
        ------------------------------------------------------------- */
        char *MCPOutput = NULL;
        JSON_Value *idRequest = MCPRequest.Get_id();
        const char *PromptName = MCPRequest.Get_param_string("name");
        JSON_Object *Arguments = MCPRequest.Get_param_object("arguments");
        if (!Arguments)
                {
                static JSON_Object NoArgs;
                Arguments = &NoArgs;
                }
        // ---------------------------
        // ---- Validate Request  ----
        // ---------------------------
        if (!PromptName)
                {
                TERROR(("%s: Missing prompt name",ProcName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing prompt name");
                }
        // ----------------------------------------
        // ---- Dispatch: Advanced Prompt Hook ----
        // ----------------------------------------
        TINFO(("%s: Dispatching to handler (id=%s): %s",ProcName,
                                                        id2string(idRequest),
                                                        PromptName));
        if (Handle_prompts_get_hook)
                {
                MCPOutput = Handle_prompts_get_hook(MCPRequest,
                                                    *idRequest,
                                                    PromptName,
                                                    *Arguments);
                if (MCPOutput)
                        {
                        TDEBUG(("%s: Hook handled prompt: %s",ProcName,
                                                              PromptName));
                        }
                }
        // ------------------------------------
        // ---- Dispatch: Standard Handler ----
        // ------------------------------------
        if (!MCPOutput)
                {
                MCPOutput = Handle_prompts_get(*idRequest,
                                               PromptName,
                                               *Arguments);
                }
        if (!MCPOutput)
                {
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Unknown prompt: '%s'",
                                                  PromptName);
                }
        return MCPOutput;
        }

// ***************************************************************************
// **** MCP Completion Handlers **********************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- Handle_completion_dispatch -------------------------------------------
// ---------------------------------------------------------------------------

// ------------------------------------
// ---- FormMCPResponse_completion ----
// ------------------------------------

char* FormMCPResponse_completion(JSON_Value &idRequest,
                                 unsigned nValues,
                                 const char **Values)
        {
        /* ----- OUTPUT ------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 5,
          "result": {
            "completion": {
              "values": ["file1.md","file2.md"],
              "total": 2,
              "hasMore": false
            }
          }
        }
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"result\":{");
        Canvas.printf("\"completion\":{");
        Canvas.printf("\"values\":[");
        for (unsigned i = 0; i < nValues; i++)
                {
                Canvas.printf(i + 1 == nValues ? "\"%s\"" : "\"%s\",",
                              Values[i]);
                }
        Canvas.printf("],");
        Canvas.printf("\"total\":%u,",nValues);
        Canvas.printf("\"hasMore\":false");
        Canvas.printf("}"); /* completion */
        Canvas.printf("}"); /* result */
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// --------------------------------------------------
// ---- Advanced Completion Hook (Set to Enable) ----
// --------------------------------------------------

char* (*Handle_completion_complete_hook)(MCPInputRequest &MCPRequest,
                                         JSON_Value &idRequest,
                                         const char *RefType,
                                         const char *RefName,
                                         const char *ArgName,
                                         const char *ArgValue) = NULL;

// ------------------------------------
// ---- Handle_completion_dispatch ----
// ------------------------------------

static char* Handle_completion_dispatch(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "Handle_completion_dispatch";
        /* ----- INPUT -------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 5,
          "method": "completion/complete",
          "params": {
            "ref": { "type": "ref/prompt", "name": "update_summary" },
            "argument": { "name": "filenames", "value": "" }
          }
        }
        ----------------------------------------------------------- */
        JSON_Value *idRequest = MCPRequest.Get_id();
        JSON_Object *Ref = MCPRequest.Get_param_object("ref");
        JSON_Object *Argument = MCPRequest.Get_param_object("argument");

        const char *RefType  = Ref ? Ref->Find_member_string("type") : NULL;
        const char *RefName  = Ref ? Ref->Find_member_string("name") : NULL;
        const char *ArgName  = Argument ? Argument->Find_member_string("name") : NULL;
        const char *ArgValue = Argument ? Argument->Find_member_string("value") : NULL;
        TDEBUG(("%s: ref=%s/%s arg=%s value=%s",ProcName,
                RefType  ? RefType  : "(null)",
                RefName  ? RefName  : "(null)",
                ArgName  ? ArgName  : "(null)",
                ArgValue ? ArgValue : "(null)"));

        char *MCPOutput = NULL;
        if (Handle_completion_complete_hook)
                {
                MCPOutput = Handle_completion_complete_hook(MCPRequest,
                                                            *idRequest,
                                                            RefType,RefName,
                                                            ArgName,ArgValue);
                }
        if (!MCPOutput)
                {
                MCPOutput = Handle_completion_complete(*idRequest,
                                                       RefType,RefName,
                                                       ArgName,ArgValue);
                }
        if (!MCPOutput) 
                {
                // ------------------------------------
                // ---- Default: empty completions ----
                // ------------------------------------
                MCPOutput = FormMCPResponse_completion(*idRequest,0,NULL);
                }
        return MCPOutput;
        }

// ***************************************************************************
// **** MCP Resource Handlers ************************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- Handle_resources_list ------------------------------------------------
// ---------------------------------------------------------------------------

static char* Handle_resources_list(JSON_Value *idRequest)
        {
        static const char *ProcName = "Handle_resources_list";
        /* ----- OUTPUT ------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 2,
          "result": {
            "resources": [
              {
                "uri": "file:///logs/server.log",
                "name": "Server Log",
                "description": "Current server log file",
                "mimeType": "text/plain"
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
        Canvas.printf("\"resources\":[");
        for (unsigned i = 0; i < MCPResourceInfoCount; i++)
                {
                TINFO(("%s: Exporting Resource: %s",MCPServer_Name,
                                                    MCPResourceInfo[i].URI));
                Canvas.printf("{");
                Canvas.printf("\"uri\":\"%s\",",MCPResourceInfo[i].URI);
                char *EscName = EscapeJSONString(MCPResourceInfo[i].Name);
                Canvas.printf("\"name\":\"%s\",",EscName ? EscName : "");
                FreeEscapeJSONString(&EscName);
                char *EscDesc = EscapeJSONString(MCPResourceInfo[i].Description);
                Canvas.printf("\"description\":\"%s\"",EscDesc ? EscDesc : "");
                FreeEscapeJSONString(&EscDesc);
                if (MCPResourceInfo[i].MimeType)
                        {
                        Canvas.printf(",\"mimeType\":\"%s\"",
                                      MCPResourceInfo[i].MimeType);
                        }
                if (i + 1 == MCPResourceInfoCount) Canvas.printf("}");
                else Canvas.printf("},");
                }
        Canvas.printf("]"); /* resources */
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- Handle_resources_templates_list --------------------------------------
// ---------------------------------------------------------------------------

static char* Handle_resources_templates_list(JSON_Value *idRequest)
        {
        static const char *ProcName = "Handle_resources_templates_list";
        /* ----- OUTPUT ------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 2,
          "result": {
            "resourceTemplates": [
              {
                "uriTemplate": "file:///logs/{name}.log",
                "name": "Log File",
                "description": "A log file by name",
                "mimeType": "text/plain"
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
        Canvas.printf("\"resourceTemplates\":[");
        for (unsigned i = 0; i < MCPResourceTemplateInfoCount; i++)
                {
                TINFO(("%s: Exporting Template: %s",MCPServer_Name,
                                MCPResourceTemplateInfo[i].URITemplate));
                Canvas.printf("{");
                Canvas.printf("\"uriTemplate\":\"%s\",",
                              MCPResourceTemplateInfo[i].URITemplate);
                char *EscName = EscapeJSONString(
                                MCPResourceTemplateInfo[i].Name);
                Canvas.printf("\"name\":\"%s\",",EscName ? EscName : "");
                FreeEscapeJSONString(&EscName);
                char *EscDesc = EscapeJSONString(
                                MCPResourceTemplateInfo[i].Description);
                Canvas.printf("\"description\":\"%s\"",EscDesc ? EscDesc : "");
                FreeEscapeJSONString(&EscDesc);
                if (MCPResourceTemplateInfo[i].MimeType)
                        {
                        Canvas.printf(",\"mimeType\":\"%s\"",
                                      MCPResourceTemplateInfo[i].MimeType);
                        }
                if (i + 1 == MCPResourceTemplateInfoCount) Canvas.printf("}");
                else Canvas.printf("},");
                }
        Canvas.printf("]"); /* resourceTemplates */
        Canvas.printf("}"); /* result */
        // ----------------
        // ----------------
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- FormMCPResponse_resource ---------------------------------------------
// ---------------------------------------------------------------------------

char* FormMCPResponse_resource(JSON_Value &idRequest,
                               const char *URI,
                               const char *MimeType,
                               const char *Text)
        {
        /* ----- OUTPUT ------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "result": {
            "contents": [
              {
                "uri": "file:///logs/server.log",
                "mimeType": "text/plain",
                "text": "..."
              }
            ]
          }
        }
        ----------------------------------------------------------- */
        if (!Text) Text = "";
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(&idRequest));
        Canvas.printf("\"result\":{");
        Canvas.printf("\"contents\":[{");
        Canvas.printf("\"uri\":\"%s\",",URI ? URI : "");
        if (MimeType)
                {
                Canvas.printf("\"mimeType\":\"%s\",",MimeType);
                }
        Canvas.printf("\"text\":\"");
        char *EscText = EscapeJSONString(Text);
        if (EscText)
                {
                Canvas.fputs(EscText);
                FreeEscapeJSONString(&EscText);
                }
        Canvas.printf("\"");
        Canvas.printf("}]"); /* contents */
        Canvas.printf("}"); /* result */
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ---------------------------------------------------------------------------
// ---- Handle_resources_read_dispatch ---------------------------------------
// ---------------------------------------------------------------------------

static char* Handle_resources_read_dispatch(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "Handle_resources_read_dispatch";
        /* ----- INPUT -------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "resources/read",
          "params": { "uri": "file:///logs/server.log" }
        }
        ------------------------------------------------------------- */
        JSON_Value *idRequest = MCPRequest.Get_id();
        const char *URI = MCPRequest.Get_param_string("uri");
        if (!URI)
                {
                TERROR(("%s: Missing URI",ProcName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: uri");
                }
        TINFO(("%s: Reading resource (id=%s): %s",ProcName,
                                                   id2string(idRequest),
                                                   URI));
        char *MCPOutput = Handle_resources_read(*idRequest,URI);
        if (!MCPOutput)
                {
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Invalid Params",
                                                  "Resource not found: '%s'",
                                                  URI);
                }
        return MCPOutput;
        }

// ---------------------------------------------------------------------------
// ---- Handle_resources_subscribe_dispatch ----------------------------------
// ---------------------------------------------------------------------------

static char* Handle_resources_subscribe_dispatch(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "Handle_resources_subscribe_dispatch";
        /* ----- INPUT -------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 4,
          "method": "resources/subscribe",
          "params": { "uri": "file:///logs/server.log" }
        }
        ------------------------------------------------------------- */
        JSON_Value *idRequest = MCPRequest.Get_id();
        const char *URI = MCPRequest.Get_param_string("uri");
        if (!URI)
                {
                TERROR(("%s: Missing URI",ProcName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: uri");
                }
        TINFO(("%s: Subscribe (id=%s): %s",ProcName,
                                            id2string(idRequest),
                                            URI));
        char *MCPOutput = Handle_resources_subscribe(*idRequest,URI);
        if (!MCPOutput)
                {
                // -----------------------------------------------
                // ---- Reject unknown URI -----------------------
                // -----------------------------------------------
                TERROR(("%s: Unknown URI: %s",ProcName,URI));
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Resource Not Found",
                                                  "Cannot subscribe: "
                                                  "unknown resource '%s'",
                                                  URI);
                }
        return MCPOutput;
        }

// ---------------------------------------------------------------------------
// ---- Handle_resources_unsubscribe_dispatch --------------------------------
// ---------------------------------------------------------------------------

static char* Handle_resources_unsubscribe_dispatch(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "Handle_resources_unsubscribe_dispatch";
        /* ----- INPUT -------------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 5,
          "method": "resources/unsubscribe",
          "params": { "uri": "file:///logs/server.log" }
        }
        ------------------------------------------------------------- */
        JSON_Value *idRequest = MCPRequest.Get_id();
        const char *URI = MCPRequest.Get_param_string("uri");
        if (!URI)
                {
                TERROR(("%s: Missing URI",ProcName));
                return FormMCPResponse_ERROR(idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: uri");
                }
        TINFO(("%s: Unsubscribe (id=%s): %s",ProcName,
                                              id2string(idRequest),
                                              URI));
        char *MCPOutput = Handle_resources_unsubscribe(*idRequest,URI);
        if (!MCPOutput)
                {
                // -----------------------------------------------
                // ---- Reject unknown URI -----------------------
                // -----------------------------------------------
                TERROR(("%s: Unknown URI: %s",ProcName,URI));
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_InvalidParms,
                                                  "Resource Not Found",
                                                  "Cannot unsubscribe: "
                                                  "unknown resource '%s'",
                                                  URI);
                }
        return MCPOutput;
        }

// ---------------------------------------------------------------------------
// ---- FormMCPNotification_ResourceUpdated ----------------------------------
// ---------------------------------------------------------------------------

char* FormMCPNotification_ResourceUpdated(const char *URI)
        {
        /* ----- OUTPUT ------------------------------------------------
        {
          "jsonrpc": "2.0",
          "method": "notifications/resources/updated",
          "params": { "uri": "file:///logs/server.log" }
        }
        ----------------------------------------------------------- */
        if (!URI) return NULL;
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"method\":\"notifications/resources/updated\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"uri\":\"%s\"",URI);
        Canvas.printf("}");
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

bool PostResourceUpdated(const char *URI)
        {
        char *Notification = FormMCPNotification_ResourceUpdated(URI);
        if (!Notification) return false;
        return PostMCPOutput(&Notification);
        }

// ***************************************************************************
// **** MCP Internal Tools  **************************************************
// ***************************************************************************

const unsigned MCPInternToolInfoCount = 2 + (IncludePromptTools ? 2 : 0);

MCPToolInfo_t MCPInternToolInfo[] =
        {
                // ------------------------
                // ---- about_tsar-mcp ----
                // ------------------------
                {
                // Name
                "about_tsar-mcp",
                // Description
                "Reports this TSAR-MCP server's identity, protocol support, "
                "linked advanced modules, attribution, and tool inventory. "
                "Call to learn what this server is and what it can do.",
                0,
                // InputSchema {Properties}
                        {
                        ""
                        },
                // [Required]
                ""
                },
                // -------------------
                // ---- readTrace ----
                // -------------------
                {
                // Name
                "readTrace",
                // Description
                "Returns the MCPServer trace log. By default returns "
                "the last 50 lines (tail). Use to diagnose errors, "
                "inspect tool call flow, or review server activity.",
                2,
                // InputSchema {Properties}
                        {
                        "\"nLines\":{\"type\":\"integer\","
                                    "\"description\":\"Number of lines "
                                    "to return (default 50)\"}",
                        "\"tail\":{\"type\":\"boolean\","
                                  "\"description\":\"true=read from end "
                                  "(default), false=read from beginning\"}"
                        },
                // [Required]
                ""
                }
                // ================================
                // ==== ==== Prompt Tools ==== ====
                // ================================
  #if IncludePromptTools
                ,
                // ---------------------
                // ---- listPrompts ----
                // ---------------------
                {
                // Name
                "listPrompts",
                // Description
                "Returns the directory of available MCP prompts and their "
                "argument schemas. Call with no arguments to list all prompts. "
                "Use before calling getPrompt to discover prompt names "
                "and required arguments.",
                1,
                // InputSchema {Properties}
                        {
                        "\"promptName\":{\"type\":\"string\","
                                        "\"description\":\"Optional. Name of a "
                                        "specific prompt to retrieve. If omitted, "
                                        "all prompts are returned.\"}"
                        },
                // [Required]
                ""
                },
                // -------------------
                // ---- getPrompt ----
                // -------------------
                {
                // Name
                "getPrompt",
                // Description
                "Executes an MCP prompt by name and returns the rendered "
                "prompt text. Use listPrompts first to discover "
                "available prompts and their argument schemas.",
                2,
                // InputSchema {Properties}
                        {
                        "\"name\":{\"type\":\"string\","
                                  "\"description\":\"Name of the prompt to execute.\"}",
                        "\"arguments\":{\"type\":\"object\","
                                       "\"description\":\"Key-value pairs matching "
                                       "the prompt's argument schema. "
                                       "All values must be strings.\"}"
                        },
                // [Required]
                "\"name\""
                }
  #endif
                // ---------------
                // ---- <end> ----
                // ---------------
        };

// -------------------
// ---- readTrace ----
// -------------------

static char* FormMCPResponse_readTrace(JSON_Value &idRequest,const char *Trace)
        {
        const char *ProcName = "FormMCPResponse_readTrace";
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "result": {
            "content": [
              {
                "type": "text", 
                "text": "about nLines of TraceFile"
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
        Canvas.printf("\"result\":{");
        Canvas.printf("\"content\":[{");
        Canvas.printf("\"type\":\"text\",\"text\":\"");
        char *jsonTrace = EscapeJSONString(Trace);
        if (jsonTrace)
                {
                Canvas.fputs(jsonTrace);
                FreeEscapeJSONString(&jsonTrace);
                }
        Canvas.printf("\"");
        Canvas.printf("}],"); /* content */
        Canvas.printf("\"isError\":false");
        Canvas.printf("}"); /* result */
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

static char* MCPtool_readTrace(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_readTrace";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "readTrace",
            "arguments": { "nLines": 100, "tail": true }
          }
        }
        ----------------------------------------------------------- */
        char *MCPOutput = NULL;
        // -------------------------
        // ---- Parse Arguments ----
        // -------------------------
        unsigned nLines = 0;
        bool Tail = true;
        JSON_Value_Number *vNLines = Arguments.Find_member_number("nLines");
        if (vNLines)
                {
                nLines = (unsigned)vNLines->int_number;
                }
        JSON_Value_Boolean *vTail = Arguments.Find_member_bool("tail");
        if (vTail)
                {
                Tail = vTail->boolean;
                }
        if (nLines == 0) nLines = 50;
        if (nLines > 5000) 
                {
                TDEBUG(("%s: nLines (%u) exceed 5000",ProcName,nLines));
                nLines = 5000;
                }
        // -------------------------
        // ---- Read Trace File ----
        // -------------------------
        size_t ReadLength = (size_t)nLines * 150;
        char *TraceFile = ReadTraceFile(Tail,ReadLength);
        if (!TraceFile)
                {
                MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                                  MCPErrorCode_Exception,
                                                  "Read Error",
                                                  "Unable to read trace file");
                return MCPOutput;
                }
        // ---------------------------------
        // ---- Trim to line boundaries ----
        // ---------------------------------
        char *Start = TraceFile;
        if (Tail)
                {
                // Skip partial first line.
                char *FirstEOL = strchr(Start,'\n');
                if (FirstEOL) Start = FirstEOL + 1;
                }
        else if (!Tail)
                {
                // Truncate after nLines.
                char *Pos = Start;
                unsigned Lines = 0;
                while (*Pos && Lines < nLines)
                        {
                        if (*Pos == '\n') Lines++;
                        Pos++;
                        }
                *Pos = '\0';
                }
        MCPOutput = FormMCPResponse_readTrace(idRequest,Start);
        free(TraceFile);
        return MCPOutput;
        }

// --------------------------------------------------------------
// ---- MCPtool_listPrompts -------------------------------------
// --------------------------------------------------------------
//
//      Serializes MCPPromptInfo[] as a JSON array inside a text 
//      tool response.
//      Optional argument "promptName" filters to a single entry.
//
//      Output text format mirrors prompts/list result:
//              [{ "name": "...", 
//                "description": "...", 
//                "arguments": [...] 
//              },... ]
//
// --------------------------------------------------------------

static void EmitPromptEntry(MemoryPrintf &Canvas, MCPPromptInfo_t &Prompt)
        {
        Canvas.printf("{");
        Canvas.printf("\"name\":\"%s\",", Prompt.Name);
        char *EscDesc = EscapeJSONString(Prompt.Description);
        Canvas.printf("\"description\":\"%s\",", EscDesc ? EscDesc : "");
        FreeEscapeJSONString(&EscDesc);
        Canvas.printf("\"arguments\":[");
        unsigned nArgs = Prompt.nArguments;
        for (unsigned j = 0; j < nArgs; j++)
                {
                Canvas.printf(j + 1 == nArgs ? "%s" : "%s,",
                              Prompt.Arguments[j]);
                }
        Canvas.printf("]"); /* arguments */
        Canvas.printf("}");
        return;
        }

static char* MCPtool_listPrompts(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_listPrompts";
        const char *FilterName = Arguments.Find_member_string("promptName");
        if (MCPPromptInfoCount == 0 && MCPExternPromptInfoCount == 0)
                {
                return FormMCPResponse_Text(&idRequest, "[]");
                }
        MemoryPrintf Canvas;
        Canvas.printf("[");
        unsigned i, nEmitted = 0;
        for (i = 0; i < MCPPromptInfoCount; i++)
                {
                if (FilterName && 
                    strcmp(FilterName,MCPPromptInfo[i].Name) != 0)
                        {
                        continue;       // Not what we're looking for.
                        }
                if (nEmitted > 0) Canvas.printf(",");
                EmitPromptEntry(Canvas,MCPPromptInfo[i]);
                nEmitted++;
                }
        for (i = 0; i < MCPExternPromptInfoCount; i++)
                {
                if (FilterName &&
                    strcmp(FilterName,MCPExternPromptInfo[i].Name) != 0)
                        {
                        continue;       // Not what we're looking for.
                        }
                if (nEmitted > 0) Canvas.printf(",");
                EmitPromptEntry(Canvas,MCPExternPromptInfo[i]);
                nEmitted++;
                }
        Canvas.printf("]");
        if (FilterName && nEmitted == 0)
                {
                MemoryPrintf ErrMsg;
                TDEBUG(("%s: Prompt not found: %s",ProcName,FilterName));
                ErrMsg.printf("Prompt not found: %s. "
                              "Call listPrompts with no arguments to see "
                              "all available prompts.",
                              FilterName);
                return FormMCPResponse_Text(&idRequest,ErrMsg.GetBuffer(),true);
                }
        return FormMCPResponse_Text(&idRequest,Canvas.GetBuffer());
        }

// ----------------------------------------------------------
// ---- MCPtool_getPrompt -----------------------------------
// ----------------------------------------------------------
//
//      Renders a prompt by name and repackages the output
//      of Handle_prompts_get() inside a MCP tool response.
//
//      The "arguments" tool parameter is an object whose 
//      members are passed directly to Handle_prompts_get 
//      as the Arguments object.
//
//      Returns the rendered as prompt text (markdown).
//
//      Note: Prompts exposed by Handle_prompts_get_hook()
//            are not offered because the MCPInputRequest
//            parameter for a prompt request isn't available;
//            meta values would need to be re-synthesized.
//            If MCPtool_getPrompt must reach such a prompt,
//            also expose it via Handle_prompts_get().
//
// ----------------------------------------------------------

static char* MCPtool_getPrompt(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_getPrompt";
        const char *PromptName = Arguments.Find_member_string("name");
        if (!PromptName)
                {
                TERROR(("%s: Missing parameter: name", ProcName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: name");
                }
        // -------------------------
        // Retrieve Prompt arguments 
        // -------------------------
        JSON_Object *PromptArgs = Arguments.Find_member_object("arguments");
        static JSON_Object NoArgs;
        if (!PromptArgs) PromptArgs = &NoArgs;
        // ---------------------
        // Invoke Prompt Handler
        // ---------------------
        TINFO(("%s: Invoking prompt: %s",ProcName,PromptName));
        char *PromptResponse = Handle_prompts_get(idRequest,PromptName,*PromptArgs);
        if (!PromptResponse)
                {
                TDEBUG(("%s: Prompt not found: %s", ProcName,PromptName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Prompt not found: %s",PromptName);
                }
        // ----------------------------------
        // Unwrap Handle_prompts_get Response 
        // ----------------------------------
        JSON_Object *ResponseObj = BuildJSONObject(PromptResponse);
        if (!ResponseObj)
                {
                TERROR(("%s: BuildJSONObject() FAILED: %s",ProcName,PromptName));
                free(PromptResponse);
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_Exception,
                                             "Internal Error",
                                             "Failed to parse prompt response: %s",
                                             PromptName);
                }
        JSON_Object *ErrorObj = ResponseObj->Find_member_object("error");
        if (ErrorObj)
                {
                // ---------------------------------------------
                // Handle_prompts_get returned an error, forward
                // ---------------------------------------------
                TERROR(("%s: Forwarding Handle_prompts_get(%s) Error",
                        ProcName,
                        PromptName));
                delete ResponseObj;
                return PromptResponse; 
                }
        free(PromptResponse);
        // -------------------------------------
        // ---- Extract Prompt Text ------------
        // -------------------------
        // Path: result.messages[0].content.text
        // -------------------------------------
        JSON_Object *Result = ResponseObj->Find_member_object("result");
        JSON_Value_Array *Messages = Result 
                                     ? Result->Find_member_array("messages")
                                     : NULL;
        JSON_Object *Msg0 = NULL;
        if (Messages && Messages->nValues > 0)
                {
                JSON_Value *Elem0 = (*Messages)[0];
                if (Elem0 && Elem0->isObject())
                        {
                        Msg0 = (JSON_Value_Object*)Elem0;
                        }
                }
        JSON_Object *Content  = Msg0 ? Msg0->Find_member_object("content") : NULL;
        const char *PromptText = Content ? Content->Find_member_string("text")
                                         : NULL;
        // --------------------------------------
        // Rewrap PromptText into a Tool response
        // --------------------------------------
        char *RawPrompt = PromptText ? UnescapeJSONString(PromptText) : NULL;
        char *MCPOutput = FormMCPResponse_Text(&idRequest,
                                               RawPrompt,
                                               !RawPrompt); // isError
        if (RawPrompt) FreeUnescapeJSONString(&RawPrompt);
        delete ResponseObj;
        return MCPOutput;
        }

// ***************************************************************************
// **** about_tsar-mcp Tool **************************************************
// **************************
//
//      Self-describing introspection tool.
//
// ***************************************************************************

#ifndef About_ProjectName
        #define About_ProjectName "TSAR-MCP (Tools Slightly Above the Runtime)"
#endif
#ifndef About_Credit
        #define About_Credit "(c) 1997, 2026 Eric Kass / " \
                             "International Business Machines Corporation " \
                             "(SPDX: MIT)"
#endif

const char* MCPServer_AspectCredit = NULL;

static char* MCPtool_about(MCPInputRequest &MCPRequest, JSON_Object &)
        {
        const char *ProcName = "MCPtool_about";
        TINFO(("%s: Reporting server identity",ProcName));
        JSON_Value *idRequest = MCPRequest.Get_id();
        MemoryPrintf Canvas;
        // ------------------
        // ---- Identity ----
        // ------------------
        Canvas.printf("%s\n",About_ProjectName);
        Canvas.printf("Server: %s v%s\n",MCPServer_Name,MCPServer_Version);
        // ------------------------------------------------
        // ---- Protocol (negotiated for this request) ----
        // ------------------------------------------------
        int ServerMax = MCPProtocolVersion_ServerMax;
        int ClientVersion = MCPRequest.Version();          // What the client is.
        int SpeakVersion = MCPProtocolVersion(MCPRequest); // Effective (clamped).
        Canvas.printf("Protocol:\n");
        Canvas.printf("| Client: %d-%.2d-%.2d\n",
                      ClientVersion / 10000,
                      ClientVersion / 100 % 100,
                      ClientVersion % 100);
        Canvas.printf("| Server: %d-%.2d-%.2d\n",
                      ServerMax / 10000,
                      ServerMax / 100 % 100,
                      ServerMax % 100);
        Canvas.printf("| Speak:  %d-%.2d-%.2d\n",
                      SpeakVersion / 10000,
                      SpeakVersion / 100 % 100,
                      SpeakVersion % 100);
        // -------------------------------------------------
        // ---- Async event loop (this binary) -------------
        // -------------------------------------------------
        Canvas.printf("Async: %s\n",MCPServer_Asynchronous ? "on" : "off");
        // -------------------------------------------------
        // ---- Linked modules (probe registered hooks) ----
        // -------------------------------------------------
        bool AnyModule = false;
        Canvas.printf("Modules:");
        const char *Sep = " ";
        if (InitMCPAsyncHook || Handle_async_object)
                {
                Canvas.printf("%sAsync",Sep); Sep = ", "; AnyModule = true;
                }
        if (OnInitMCPSampleHook || Handle_sampling_hook)
                {
                Canvas.printf("%sSample",Sep); Sep = ", "; AnyModule = true;
                }
        if (OnStartupMCPSampleExHook)
                {
                Canvas.printf("%sSampleEx(+AIcURL)",Sep);
                Sep = ", "; AnyModule = true;
                }
        if (Handle_MRTR_hook || Handle_MRTR_reapttl)
                {
                Canvas.printf("%sMRTR",Sep); Sep = ", "; AnyModule = true;
                }
        if (!AnyModule) Canvas.printf(" (core only)");
        Canvas.printf("\n");
        // -------------------------------
        // ---- Advanced hooks in use ----
        // -------------------------------
        if (Handle_tools_call_hook ||
            Handle_prompts_get_hook ||
            Handle_completion_complete_hook)
                {
                Canvas.printf("Hooks:");
                const char *HSep = " ";
                if (Handle_tools_call_hook)
                        { Canvas.printf("%stools_call",HSep); HSep = ", "; }
                if (Handle_prompts_get_hook)
                        { Canvas.printf("%sprompts_get",HSep); HSep = ", "; }
                if (Handle_completion_complete_hook)
                        { Canvas.printf("%scompletion",HSep); HSep = ", "; }
                Canvas.printf("\n");
                }
        // ----------------
        // ---- Credit ----
        // ----------------
        Canvas.printf("Credit: %s\n",About_Credit);
        if (MCPServer_AspectCredit)
                {
                Canvas.printf("        %s\n",MCPServer_AspectCredit);
                }
        // -------------------
        // ---- Inventory ----
        // -------------------
        unsigned nTools = MCPInternToolInfoCount
                        + MCPToolInfoCount
                        + MCPExternToolInfoCount;
        unsigned nPrompts = MCPPromptInfoCount + MCPExternPromptInfoCount;
        Canvas.printf("Inventory: tools=%u, prompts=%u, resources=%u",
                      nTools,nPrompts,MCPResourceInfoCount);
        return FormMCPResponse_Text(idRequest,Canvas.GetBuffer());
        }

// ***************************************************************************
// **** MCPServer MRTR Support ***********************************************
// ***************************************************************************

// -------------------------------------
// ---- FormMCPResult_inputRequired ----
// -------------------------------------
//
//      Generic MRTR (2026-07-28) input_required RESULT envelope. Answers
//      the original request id, embeds one client-side request (Method +
//      pre-formed ParamsJSON) under the "inputRequest0" slot, and carries
//      the requestState token alongside it so the resume can be correlated.
//      ParamsJSON is injected verbatim (caller escapes).
//

char* FormMCPResult_inputRequired(JSON_Value &idRequest,
                                  JSON_Value_String &requestState,
                                  const char *Method,
                                  const char *ParamsJSON)
        {
        static const char *ProcName = "FormMCPResult_inputRequired";
        /* ----- OUTPUT (MCP 2026-07-28 MRTR) ------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "result": {
            "resultType": "input_required",
            "inputRequests": {
              "inputRequest0": {
                "method": "<Method>",
                "params": <ParamsJSON>
              }
            },
            "requestState": "<requestState>"
          }
        }
        ----------------------------------------------------------- */
        if (!requestState.string || !Method || !ParamsJSON)
                {
                TERROR(("%s: Missing requestState, Method, or ParamsJSON "
                        "(id=%s)",ProcName,id2string(idRequest)));
                return NULL;
                }
        // requestState.string is already JSON-escaped (JSON tree convention).
        const char *jsonState = requestState.string;
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"result\":{");
        Canvas.printf("\"resultType\":\"input_required\",");
        Canvas.printf("\"inputRequests\":{");
        Canvas.printf("\"inputRequest0\":{");
        Canvas.printf("\"method\":\"%s\",",Method);
        Canvas.printf("\"params\":%s",ParamsJSON);
        Canvas.printf("}"); /* inputRequest0 */
        Canvas.printf("},"); /* inputRequests */
        Canvas.printf("\"requestState\":\"%s\"",jsonState);
        Canvas.printf("}"); /* result */
        Canvas.printf("}");
        return Canvas.AquireBuffer();
        }

// ***************************************************************************
// **** MCPServer Sampling Support *******************************************
// ***************************************************************************

// ---------------------------------
// ---- FormMCPRequest_sampling ----
// ---------------------------------

char* FormMCPRequest_sampling(JSON_Value &idRequest,
                              const char *SystemPrompt,
                              const char *UserMessage,
                              int MaxTokens)
        {
        static const char *ProcName = "FormMCPRequest_sampling";
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": "Smpl_1_0",
          "method": "sampling/createMessage",
          "params": {
            "messages": [
              {"role":"user","content":{"type":"text","text":"..."}}
            ],
            "systemPrompt": "...",
            "maxTokens": 1024
          }
        }
        ----------------------------------------------------------- */
        if (!UserMessage)
                {
                TERROR(("%s: No UserMessage (id=%s)",ProcName,
                                                     id2string(idRequest)));
                return NULL;
                }
        char *jsonUserMessage = EscapeJSONString(UserMessage);
        if (!jsonUserMessage)
                {
                TERROR(("%s: EscapeJSONString Failed (id=%s)",
                        ProcName,
                        id2string(idRequest)));
                return NULL;
                }
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"method\":\"sampling/createMessage\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"messages\":[");
        Canvas.printf("{\"role\":\"user\",");
        Canvas.printf("\"content\":{\"type\":\"text\",\"text\":\"%s\"}}",
                      jsonUserMessage);
        Canvas.printf("],");
        FreeEscapeJSONString(&jsonUserMessage);
        if (SystemPrompt)
                {
                char *jsonSystemPrompt = EscapeJSONString(SystemPrompt);
                if (jsonSystemPrompt)
                        {
                        Canvas.printf("\"systemPrompt\":\"%s\",",
                                      jsonSystemPrompt);
                        FreeEscapeJSONString(&jsonSystemPrompt);
                        }
                }
        Canvas.printf("\"maxTokens\":%d",MaxTokens);
        Canvas.printf("}}");
        return Canvas.AquireBuffer();
        }

// -------------------------------------------------
// ---- FormMCPRequest_sampling (MRTR overload) ----
// -------------------------------------------------
//
//      2026-07-28 sampling as an input_required round trip: builds the
//      sampling/createMessage params and wraps them via
//      FormMCPResult_inputRequired. The classic (2024-11-05) overload
//      above emits the same params inside a REQUEST instead.
//

char* FormMCPRequest_sampling(JSON_Value &idRequest,
                              JSON_Value_String &requestState,
                              const char *SystemPrompt,
                              const char *UserMessage,
                              int MaxTokens)
        {
        static const char *ProcName = "FormMCPRequest_sampling";
        if (!UserMessage)
                {
                TERROR(("%s: No UserMessage (id=%s)",ProcName,
                                                     id2string(idRequest)));
                return NULL;
                }
        char *jsonUserMessage = EscapeJSONString(UserMessage);
        if (!jsonUserMessage)
                {
                TERROR(("%s: EscapeJSONString Failed (id=%s)",
                        ProcName,id2string(idRequest)));
                return NULL;
                }
        char *jsonSystemPrompt = SystemPrompt ? EscapeJSONString(SystemPrompt)
                                              : NULL;
        // ---- Build the sampling/createMessage params object ----
        MemoryPrintf Params;
        Params.printf("{");
        Params.printf("\"messages\":[");
        Params.printf("{\"role\":\"user\",");
        Params.printf("\"content\":{\"type\":\"text\",\"text\":\"%s\"}}",
                      jsonUserMessage);
        Params.printf("],");
        if (jsonSystemPrompt)
                {
                Params.printf("\"systemPrompt\":\"%s\",",jsonSystemPrompt);
                }
        Params.printf("\"maxTokens\":%d",MaxTokens);
        Params.printf("}");
        FreeEscapeJSONString(&jsonUserMessage);
        if (jsonSystemPrompt) FreeEscapeJSONString(&jsonSystemPrompt);
        return FormMCPResult_inputRequired(idRequest,
                                           requestState,
                                           "sampling/createMessage",
                                           Params.GetBuffer());
        }

// ***************************************************************************
// **** MCP Main *************************************************************
// ***************************************************************************

bool MCPMain(const char *InputRequestBuffer, FILE *out)
        {
        const char *ProcName = "MCPMain";
        char *MCPOutput = NULL;
        MCPInputRequest *MCPRequest = NULL;
        JSON_Value *idRequest = NULL;
        const char *Method = NULL;
        int Version = MCPProtocolVersion_20251125;
        // ********************************************
        // **** Handle MCP Client Message Overflow ****
        // ********************************************
        if (InputRequestBuffer == ReadSTDIN_OVERFLOW_SENTINEL)
                {
                TERROR(("%s: MCP Client Message Overflow",ProcName));
                MCPOutput = FormMCPResponse_ERROR(0,
                                                  MCPErrorCode_Exception,
                                                  "Buffer Overflow",
                                                  "MCP Client Message > 0x%X",
                                                  ReadSTDIN_MCPMessage_MAXSIZE);
                goto OverflowResume;
                }
        // **************************************
        // **** Build MCP Request from JSON  ****
        // **************************************
        if (MCPJSONTrace) 
                {
                TPRINT(("------------------------------"));
                TINFO(("%s: Tracing Input:",ProcName));
                TraceJSON(InputRequestBuffer);
                }
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
                // -----------------------------
                idRequest = MCPRequest->Get_id();
                Method = MCPRequest->Get_method();
                Version = MCPRequest->Version();
                // ****************************************
                // **** Run Method Produce JSON Output ****
                // ****************************************
                TINFO(("%s: Handling Request Method %s",ProcName,
                                                        Method
                                                        ? Method
                                                        : "<none>"));
                if (!Method && !idRequest)
                        {
                        TERROR(("%s: No Method in Request",ProcName));
                        MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                          MCPErrorCode_NoMethod,
                                                          "No Method",
                                                          "No method found");
                        break;
                        }
                else if (!Method)
                        {
                        if (Handle_sampling_hook)
                                {
                                MCPOutput = Handle_sampling_hook(*MCPRequest);
                                }
                        if (!MCPOutput)
                                {
                                MCPOutput = Handle_sampling_response(*MCPRequest);
                                }
                        if (!MCPOutput)
                                {
                                MCPOutput = FormMCPResponse_ERROR(
                                                idRequest,
                                                MCPErrorCode_Exception,
                                                "Invalid Sampling Response",
                                                "Unrecognized Sampling (id=%s)",
                                                id2string(idRequest));
                                }
                        break;
                        }
                // -------------------------------------------
                // ---- Handle notifications (without Id) ----
                // -------------------------------------------
                if (strcmp(Method,"notifications/initialized") == 0)
                        {
                        Handle_initialized(*MCPRequest);
                        break;
                        }
                else if (strcmp(Method,"notifications/cancelled") == 0)
                        {
                        bool Handled = false;
                        if (Cancel_async_object)
                                {
                                Handled = Cancel_async_object(*MCPRequest);
                                }
                        if (!Handled) Handle_notification(*MCPRequest);
                        break;
                        }
                else if (strncmp(Method,"notifications/",14) == 0)
                        {
                        Handle_notification(*MCPRequest);
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
                if (Version == MCPProtocolVersion_Invalid)
                        {
                        TERROR(("%s: Malformed _meta protocolVersion",ProcName));
                        MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                          MCPErrorCode_InvalidParms,
                                                          "Invalid Params",
                                                          "Malformed _meta field: "
                                                          "protocolVersion");
                        break;
                        }
                if (strcmp(Method,"initialize") == 0)
                        {
                        if (OnInitMCPSampleHook) 
                                {
                                OnInitMCPSampleHook(*MCPRequest);
                                }
                        MCPServer_OnInitialize(*MCPRequest);
                        MCPOutput = Handle_initialize(*MCPRequest);
                        break;
                        }
  #if !ClampMCPProtocolVersion_20251125
                if (strcmp(Method,"server/discover") == 0)
                        {
                        MCPOutput = Handle_server_discover(idRequest);
                        break;
                        }
  #endif
                if (strcmp(Method,"tools/list") == 0)
                        {
                        MCPOutput = Handle_tools_list(idRequest);
                        break;
                        }
                if (strcmp(Method,"tools/call") == 0)
                        {
                        MCPOutput = Handle_tools_call_dispatch(*MCPRequest);
                        break;
                        }
                if (strcmp(Method,"prompts/list") == 0)
                        {
                        MCPOutput = Handle_prompts_list(idRequest);
                        break;
                        }
                if (strcmp(Method,"prompts/get") == 0)
                        {
                        MCPOutput = Handle_prompts_get_dispatch(*MCPRequest);
                        break;
                        }
                if (strcmp(Method,"completion/complete") == 0)
                        {
                        MCPOutput = Handle_completion_dispatch(*MCPRequest);
                        break;
                        }
                if (strcmp(Method,"resources/list") == 0)
                        {
                        MCPOutput = Handle_resources_list(idRequest);
                        break;
                        }
                if (strcmp(Method,"resources/templates/list") == 0)
                        {
                        MCPOutput = Handle_resources_templates_list(idRequest);
                        break;
                        }
                if (strcmp(Method,"resources/read") == 0)
                        {
                        MCPOutput = Handle_resources_read_dispatch(*MCPRequest);
                        break;
                        }
                if (strcmp(Method,"resources/subscribe") == 0)
                        {
                        MCPOutput = Handle_resources_subscribe_dispatch(*MCPRequest);
                        break;
                        }
                if (strcmp(Method,"resources/unsubscribe") == 0)
                        {
                        MCPOutput = Handle_resources_unsubscribe_dispatch(*MCPRequest);
                        break;
                        }
                // ----------------------------------
                // ---- Handle all other methods ----
                // ----------------------------------
                MCPOutput = Handle_method(*MCPRequest);
                if (MCPOutput) break;
                // ----------------------------
                // ---- Method not handled ----
                // ----------------------------
                TERROR(("%s: Invalid Method '%s'",ProcName,Method));
                MCPOutput = FormMCPResponse_ERROR(idRequest,
                                                  MCPErrorCode_NoMethod,
                                                  "Unknown Method",
                                                  "Method %s not found",
                                                   Method 
                                                   ? Method 
                                                   : "<none>");
                } while(0);
        if (MCPOutput == MCPOutput_Pending)
                {
                TINFO(("%s: MCP is Response Pending (id=%s)",
                       ProcName,
                       id2string(idRequest)));
                MCPOutput = NULL;
                }
        else if (MCPOutput) 
                {
OverflowResume: bool Valid = ValidateJSON(MCPOutput);
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
                char idBuf[64];
                FuzzyPeekJSONId(MCPOutput,idBuf,sizeof(idBuf));
                TINFO(("%s: Returning %u bytes (id=%s)",ProcName,
                                                        strlen(MCPOutput),
                                                        idBuf));
                if (MCPJSONTrace) 
                        {
                        TPRINT(("       --------------------------"));
                        TINFO(("%s: Tracing Output:",ProcName));
                        TraceJSON(MCPOutput);
                        TPRINT(("       --------------------------"));
                        }
                fprintf(out,"%s\n",MCPOutput);
                fflush(out);
                }
        if (MCPOutput) FreeMCPResponseBuffer(&MCPOutput);
        if (MCPRequest) delete MCPRequest;
        return true;
        }

// ***************************************************************************
// **** MCP Asynchronous Handler *********************************************
// *******************************
//
//      main()
//              {
//              MCPServerIOReader MCPServerReader;
//              MCPServerReader.Start(stdin);
//              MCPServerMain.Run(stdin,stdout);
//              }
//
//      This is a multi-threaded version of MCPMainLoop() that is designed 
//      to handle asynchronious notifications from MCP server to client. 
//
//      Worker threads deliver deferred responses via either:
//
//      (A) PostNotifyAction(UserPtr):
//              Handle_notify_action() is called on the main thread to
//              build and return the MCPOutput JSON string.
//              Discard_notify_action() frees outstanding UserPtr storage
//              on shutdown.
//
//      (B) PostMCPOutput(MCPOutput):
//              The worker thread builds the complete MCPOutput JSON
//              (including the correct JSON-RPC id) and posts it directly.
//              The main thread validates and sends it. No Handle/Discard
//              handlers needed.
//              
// ***************************************************************************

enum MCPServerIOMessage_Reason_t
        {
        MIM_Ping=0, 
        MIM_Shutdown, 
        MIM_MCPRequest,                 // Message from MCPServerIOReader.
        MIM_NotifyAction,               // Handle MCPNotify.
        MIM_MCPResponse,                // Return MCPOutput to client.
        MIM_MCPAsyncObject,             // Route MCPAsyncWork Objects.
        MIM_MCPMRTRReapTTL              // Route MRTR timeout. 
        };

struct MCPServerIOMessage               // Requests handled in main thread.
        {
        char EyeCatcher[8];
        MCPServerIOMessage_Reason_t Reason;
        void *UserPtr;
        MCPServerIOMessage(MCPServerIOMessage_Reason_t reason, void *userPtr) 
                {
                memcpy(EyeCatcher,"ERKIOMSG",8);
                Reason = reason;
                UserPtr = userPtr;
                }
        };

class MCPServerIOMain : public MessageQueueLoop
        {
        private:
                FILE *inFile;
                FILE *outFile;
        protected: 
                // ------------------
                // ---- ASThread ----
                // ------------------
                virtual bool OnMessage(void *Msg);
                virtual void OnMessageDestruct(void *Msg);
                // -------------
                // ---- MCP ----
                // -------------
                bool OnMCPMessage(char *MCPMessage);
                bool OnMCPNotify(void *UserPtr);
                bool OnMCPOutput(void *UserPtr);
                bool OnMCPAsyncObject(void *UserPtr);
        public:
                MCPServerIOMain();
                ~MCPServerIOMain() {ClearQueue();}
                bool PostAsyncObject(void *UserPtr);
                bool PostMCPMessage(char *MCPMessage);
                bool PostMCPNotify(void *UserPtr);
                bool PostMCPOutput(char **MCPOutput);
                bool PostMRTRReapTTL();
                bool PostShutdown();
                int Run(FILE *in, FILE *out);
        };

// ***************************
// **** MCPServerIOReader ****
// ***************************

class MCPServerIOReader : public ASThread
        {
        private:
                FILE *inFile;
        protected:
                virtual int ThreadMain();
        public:
                bool Start(FILE *inFile);
        };

// ***************************************************************************
// **** MCPServerIOMain Implementation ***************************************
// ***************************************************************************

MCPServerIOMain::MCPServerIOMain()
        {
        const char *ProcName = "MCPServerIOMain::MCPServerIOMain";
        inFile = NULL;
        outFile = NULL;
        return;
        }

bool MCPServerIOMain::OnMCPMessage(char *MCPMessage)
        {
        static const char *ProcName = "MCPServerIOMain::OnMCPMessage";
        // -----------------------
        // ---- Log Timestamp ----
        // -----------------------
        time_t Now = time(NULL);
        struct tm tmBuf;
        MCP_localtime(&tmBuf,Now);
        TINFO(("  <<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>\n"
               "<< Begin MCP Sequence @ "
               "%u-%.2u-%.2u %.2u:%.2u:%.2u >>\n"
               "<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>",
               tmBuf.tm_year + 1900,
               tmBuf.tm_mon + 1,
               tmBuf.tm_mday,
               tmBuf.tm_hour,
               tmBuf.tm_min,
               tmBuf.tm_sec));
        // -------------------------
        // ---- Process Request ----
        // -------------------------
        if (!MCPMessage)
                {
                TERROR(("%s: Missing MCPMessage",ProcName));
                }
        else    {
                bool Status = MCPMain(MCPMessage,outFile);
                if (!Status)
                        {
                        TERROR(("%s: MCPMain() Failed",ProcName));
                        }
                FreeMCPMessageBuffer(&MCPMessage);
                }
        return true;
        }

bool MCPServerIOMain::OnMessage(void *Msg)
        {
        static const char *ProcName = "MCPServerIOMain::OnMessage";
        MCPServerIOMessage *MCPMsg = (MCPServerIOMessage *)Msg;
        if (MCPMsg->Reason == MIM_MCPRequest)
                {
                char *MCPMessage = (char *)MCPMsg->UserPtr;
                bool Status = OnMCPMessage(MCPMessage);
                if (!Status)
                        {
                        TERROR(("%s: OnMCPMessage() Failed",ProcName));
                        }
                }
        else if (MCPMsg->Reason == MIM_NotifyAction)
                {
                bool Status = OnMCPNotify(MCPMsg->UserPtr);
                if (!Status)
                        {
                        TERROR(("%s: OnMCPNotify() Failed",ProcName));
                        }
                }
        else if (MCPMsg->Reason == MIM_MCPResponse)
                {
                bool Status = OnMCPOutput(MCPMsg->UserPtr);
                if (!Status)
                        {
                        TERROR(("%s: OnMCPResponse() Failed",ProcName));
                        }
                }
        else if (MCPMsg->Reason == MIM_MCPAsyncObject)
                {
                bool Status = OnMCPAsyncObject(MCPMsg->UserPtr);
                if (!Status)
                        {
                        TERROR(("%s: OnMCPAsyncObject() Failed",ProcName));
                        }
                }
        else if (MCPMsg->Reason == MIM_MCPMRTRReapTTL)
                {
                if (Handle_MRTR_reapttl) Handle_MRTR_reapttl();
                }
        else if (MCPMsg->Reason == MIM_Shutdown)
                {
                bool Status = Stop();
                if (!Status)
                        {
                        TERROR(("%s: Failed to Stop()",ProcName));
                        }
                else    {
                        TINFO(("%s: Stopping Message Loop",ProcName));
                        }
                }
        else if (MCPMsg->UserPtr)
                {
                TERROR(("%s: Can't handle UserPtr for MIM_Reason: %d",
                        ProcName,
                        MCPMsg->Reason));
                }
        delete MCPMsg;
        return true;
        }

bool MCPServerIOMain::OnMCPNotify(void *UserPtr)
        {
        static const char *ProcName = "MCPServerIOMain::OnMCPNotify";
        char *MCPOutput = Handle_notify_action(UserPtr);
        return MCPOutput ? OnMCPOutput(MCPOutput) : true;
        }

bool MCPServerIOMain::OnMCPOutput(void *UserPtr)
        {
        static const char *ProcName = "MCPServerIOMain::OnMCPOutput";
        bool Status = true;
        char *MCPOutput = (char *)UserPtr;
        if (MCPOutput) 
                {
                char idBuf[64];
                FuzzyPeekJSONId(MCPOutput,idBuf,sizeof(idBuf));
                bool Valid = ValidateJSON(MCPOutput);
                if (!Valid)
                        {
                        TERROR(("%s: MCPOutput Failed to Parse (id=%s):",
                                ProcName,
                                idBuf));
                        TPRINT(("%s",MCPOutput));
                        Status = false;
                        }
                else    {
                        TINFO(("%s: Returning %u bytes (id=%s)",ProcName,
                               strlen(MCPOutput),
                               idBuf));
                        if (MCPJSONTrace) 
                                {
                                TPRINT(("       --------------------------"));
                                TINFO(("%s: Tracing Output:",ProcName));
                                TraceJSON(MCPOutput);
                                TPRINT(("       --------------------------"));
                                }
                        fprintf(outFile,"%s\n",MCPOutput);
                        fflush(outFile);
                        }
                FreeMCPResponseBuffer(&MCPOutput);
                }
        return Status;
        }

bool MCPServerIOMain::OnMCPAsyncObject(void *UserPtr)
        {
        static const char *ProcName = "MCPServerIOMain::OnMCPAsyncObject";
        char *MCPOutput = NULL;
        if (!Handle_async_object)
                {
                TERROR(("%s: No Handle_async_object() Handler",ProcName));
                MCPOutput = FormMCPResponse_ERROR(NULL,
                                                  MCPErrorCode_Exception,
                                                  "Async Handler Error",
                                                  "No Handler Defined");
                }
        else    {
                MCPOutput = Handle_async_object(UserPtr);
                }
        return MCPOutput ? OnMCPOutput(MCPOutput) : true;
        }

void MCPServerIOMain::OnMessageDestruct(void *Msg)
        {
        static const char *ProcName = "MCPServerIOMain::OnMessageDestruct";
        MCPServerIOMessage *MCPMsg = (MCPServerIOMessage *)Msg;
        if (MCPMsg->Reason == MIM_MCPRequest)
                {
                char *MCPMessage = (char *)MCPMsg->UserPtr;
                FreeMCPMessageBuffer(&MCPMessage);
                }
        else if (MCPMsg->Reason == MIM_NotifyAction)
                {
                Discard_notify_action(MCPMsg->UserPtr);
                }
        else if (MCPMsg->Reason == MIM_MCPResponse)
                {
                char *MCPOutput = (char *)MCPMsg->UserPtr;
                if (MCPOutput) FreeMCPResponseBuffer(&MCPOutput);
                }
        else if (MCPMsg->Reason == MIM_MCPAsyncObject)
                {
                if (!Discard_async_object)
                        {
                        TERROR(("%s: No Discard_async_object() Handler",
                                ProcName));
                        }
                else Discard_async_object(MCPMsg->UserPtr);
                }
        else if (MCPMsg->Reason == MIM_MCPMRTRReapTTL)
                {
                (void)0; // Nothing to do.
                }
        else if (MCPMsg->UserPtr)
                {
                TERROR(("%s: Can't handle UserPtr for MIM_Reason: %d",
                        ProcName,
                        MCPMsg->Reason));
                }
        delete MCPMsg;
        return;
        }

bool MCPServerIOMain::PostAsyncObject(void *UserPtr)
        {
        const char *ProcName = "MCPServerIOMain::PostAsyncObject";
        bool Status = false;
        MCPServerIOMessage *MCPMsg;
        MCPMsg = new MCPServerIOMessage(MIM_MCPAsyncObject,UserPtr);
        if (!MCPMsg)
                {
                TERROR(("%s: Can't allocate MCPServerIOMessage",ProcName));
                }
        else    {
                Status = Send(MCPMsg);
                if (!Status) delete MCPMsg;
                }
        return Status;
        }

bool MCPServerIOMain::PostMRTRReapTTL()
        {
        const char *ProcName = "MCPServerIOMain::PostMRTRReapTTL";
        bool Status = false;
        MCPServerIOMessage *MCPMsg;
        MCPMsg = new MCPServerIOMessage(MIM_MCPMRTRReapTTL,NULL);
        if (!MCPMsg)
                {
                TERROR(("%s: Can't allocate MCPServerIOMessage",ProcName));
                }
        else    {
                Status = Send(MCPMsg);
                if (!Status) delete MCPMsg;
                }
        return Status;
        }

bool MCPServerIOMain::PostMCPMessage(char *MCPMessage)
        {
        const char *ProcName = "MCPServerIOMain::PostMCPMessage";
        bool Status = false;
        MCPServerIOMessage *MCPMsg;
        MCPMsg = new MCPServerIOMessage(MIM_MCPRequest,MCPMessage);
        if (!MCPMsg)
                {
                TERROR(("%s: Can't allocate MCPServerIOMessage",ProcName));
                }
        else    {
                Status = Send(MCPMsg);
                if (!Status) delete MCPMsg;
                }
        return Status;
        }

bool MCPServerIOMain::PostMCPNotify(void *UserPtr)
        {
        const char *ProcName = "MCPServerIOMain::PostMCPNotify";
        bool Status = false;
        MCPServerIOMessage *MCPMsg;
        MCPMsg = new MCPServerIOMessage(MIM_NotifyAction,UserPtr);
        if (!MCPMsg)
                {
                TERROR(("%s: Can't allocate MCPServerIOMessage",ProcName));
                }
        else    {
                Status = Send(MCPMsg);
                if (!Status) delete MCPMsg;
                }
        return Status;
        }

bool MCPServerIOMain::PostMCPOutput(char **MCPOutput)
        {
        const char *ProcName = "MCPServerIOMain::PostMCPOutput";
        if (!MCPOutput || !*MCPOutput) 
                {
                TERROR(("%s: No MCPOutput to Post",ProcName));
                return false;
                }
        bool Status = false;
        MCPServerIOMessage *MCPMsg;
        MCPMsg = new MCPServerIOMessage(MIM_MCPResponse,*MCPOutput);
        if (!MCPMsg)
                {
                TERROR(("%s: Can't allocate MCPServerIOMessage",ProcName));
                }
        else    {
                Status = Send(MCPMsg);
                if (!Status) delete MCPMsg;
                else *MCPOutput = NULL;         // MCPOutput is owned. 
                }
        return Status;
        }

bool MCPServerIOMain::PostShutdown()
        {
        const char *ProcName = "MCPServerIOMain::PostShutdown";
        bool Status = false;
        MCPServerIOMessage *MCPMsg;
        MCPMsg = new MCPServerIOMessage(MIM_Shutdown,NULL);
        if (!MCPMsg)
                {
                TERROR(("%s: Can't allocate MCPServerIOMessage",ProcName));
                }
        else    {
                Status = SendPriority(MCPMsg);
                if (!Status) delete MCPMsg;
                }
        return Status;
        }

int MCPServerIOMain::Run(FILE *in, FILE *out)
        {
        inFile = in;
        outFile = out;
        int rc = MessageQueueLoop::Run();
        return rc;
        }

// ***************************
// **** MCPServerIOReader ****
// ***************************

static MCPServerIOMain MCPServerMain;

bool MCPServerIOReader::Start(FILE *in)
        {
        inFile = in;
        return ASThread::Start();
        }

int MCPServerIOReader::ThreadMain()
        {
        const char *ProcName = "MCPServerIOReader::ThreadMain";
        bool Continue = true;
        while (Continue)
                {
                char *MCPMessage = ReadSTDIN_MCPMessage(inFile);
                if (!MCPMessage)
                        {
                        TERROR(("%s: stdin broken-pipe - Exit",ProcName));
                        MCPServerMain.PostShutdown();
                        Continue = false;
                        }
                else    {
                        Continue = MCPServerMain.PostMCPMessage(MCPMessage);
                        if (!Continue)
                                {
                                TERROR(("%s: PostMCPMessage Failed",ProcName));
                                FreeMCPMessageBuffer(&MCPMessage);
                                }
                        }
                }
        TINFO(("%s: Exiting ThreadMain()",ProcName));
        return 0;
        }

// **************************
// **** PostAsyncObject ****
// **************************

bool PostAsyncObject(void *UserPtr)     // Return an MCPAsyncWork to main. 
        {
        return MCPServerMain.PostAsyncObject(UserPtr);
        }

// **************************
// **** PostAsyncObject ****
// **************************

void PostMRTRReapTTL()                  // Invoke MRTR TripManager ReapTTL.
        {
        MCPServerMain.PostMRTRReapTTL();
        return;
        }

// ***********************
// **** PostMCPOutput ****
// ***********************

bool PostMCPOutput(char **MCPOutput)    // Send to MCPOutput to client.
        {
        return MCPServerMain.PostMCPOutput(MCPOutput);
        }

// **************************
// **** PostNotifyAction ****
// **************************

bool PostNotifyAction(void *UserPtr)    // Initate "Notify" in main thread.
        {
        return MCPServerMain.PostMCPNotify(UserPtr);
        }

// ********************************************
// **** PostShutdown (Main thread to Exit) ****
// ********************************************

bool PostShutdown()
        {
        return MCPServerMain.PostShutdown();
        }

// ***************************************************************************
// **** MCP Main Loop (Synchronous) ******************************************
// ***************************************************************************

bool MCPMainLoop(FILE *in, FILE *out)
        {
        const char *ProcName = "MCPMainLoop";
        bool Status = true;
        do      {
                char *stdinBuffer = ReadSTDIN_MCPMessage(stdin);
                // -----------------------
                // ---- Log Timestamp ----
                // -----------------------
                time_t Now = time(NULL);
                struct tm tmBuf;
                MCP_localtime(&tmBuf,Now);
                TINFO(("<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>"));
                TINFO(("<< Begin MCP Sequence @ "
                       "%u-%.2u-%.2u %.2u:%.2u:%.2u >>",tmBuf.tm_year + 1900,
                                                        tmBuf.tm_mon + 1,
                                                        tmBuf.tm_mday,
                                                        tmBuf.tm_hour,
                                                        tmBuf.tm_min,
                                                        tmBuf.tm_sec));
                TINFO(("<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>"));
                // -------------------------
                // ---- Process Request ----
                // -------------------------
                if (!stdinBuffer)
                        {
                        TINFO(("%s: stdin broken-pipe - Exit",ProcName));
                        break;
                        }
                Status = MCPMain(stdinBuffer,out);
                if (!Status)
                        {
                        TERROR(("%s: MCPMain() Failed",ProcName));
                        }
                FreeMCPMessageBuffer(&stdinBuffer);
                } while(Status);
        return Status;
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
#if ClampMCPProtocolVersion_20251125
        "    \"protocolVersion\": \"2025-11-25\","
#else
        "    \"protocolVersion\": \"2026-07-28\","
#endif
        "    \"capabilities\": {},"
        "    \"clientInfo\": { \"name\": \"vscode-copilot\", \"version\": \"1.0.0\" }"
        "  }"
        "}";

#if ClampMCPProtocolVersion_20251125
        const char* MCPInputTestRequest_About =
                "{"
                 " \"jsonrpc\": \"2.0\","
                 " \"id\": 3,"
                 " \"method\": \"tools/call\","
                 " \"params\": {"
                 "   \"name\": \"about_tsar-mcp\","
                 "   \"arguments\": {}"
                 " }"
                 "}";
#else /* MCPProtocolVersion_20260728 */
        const char* MCPInputTestRequest_About =
                "{"
                 " \"jsonrpc\": \"2.0\","
                 " \"id\": 3,"
                 " \"method\": \"tools/call\","
                 " \"params\": {"
                 "   \"name\": \"about_tsar-mcp\","
                 "   \"arguments\": {},"
                 "   \"_meta\": {"
                 "     \"io.modelcontextprotocol/protocolVersion\": \"2026-07-28\""
                 "   }"
                 " }"
                 "}";
#endif

const char* MCPInputTestRequest_List =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"id\": \"2id\","
        "  \"method\": \"tools/list\","
        "  \"params\": {}"
        "}"; 

const char* MCPInputTestRequest_Discover =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"id\": \"disc-1\","
        "  \"method\": \"server/discover\","
        "  \"params\": {"
        "    \"_meta\": {"
        "      \"io.modelcontextprotocol/protocolVersion\": \"2026-07-28\""
        "    }"
        "  }"
        "}";

bool MCPTest(FILE *out)
        {
        const char *ProcName = "MCPTest";
        bool Status;
        // --------------
        // Initialization
        // --------------
        Status = MCPMain(MCPInputTestRequest_Init,out);
        // ---------
        // Tool List
        // ---------
        Status = Status && MCPMain(MCPInputTestRequest_List,out);
        // --------------
        // About_tsar-mcp
        // --------------
        Status = Status && MCPMain(MCPInputTestRequest_About,out);
        // -----------------------
        // Stateless Discovery
        // -----------------------
        Status = Status && MCPMain(MCPInputTestRequest_Discover,out);
        // ----------------
        // Aspect Test Call
        // ----------------
        for (unsigned i=0; MCPInputTestRequests[i]; i++)
                {
                const char *Request = MCPInputTestRequests[i]; 
                Status = Status && MCPMain(Request,out);
                }
        return Status;
        }

// ***************************************************************************
// **** Exception Handler ****************************************************
// ***************************************************************************

#ifdef _WIN32

static void SafeWarmSnapshot()
        {
        InvocationCallStack Stack;
        Stack.Snapshot();
        return;
        }

static void InitializeSymbolEngine()
        {
        __try   {
                SafeWarmSnapshot();  // In a new stack-frame (see SnapStack.h)
                }
        __except (EXCEPTION_EXECUTE_HANDLER)
                {
                TERROR(("InitializeSymbolEngine: symbol warm-up faulted (ignored)"));
                }
        return;
        }

static void TraceStack(EXCEPTION_POINTERS *EXInfo)
        {
        InvocationCallStack Stack;
        if (EXInfo) Stack.Snapshot(EXInfo);
        else Stack.Snapshot();
        InvocationEntry *Current = Stack.GetFirstEntry();
        while (Current)
                {
                if (*Current->GetFunction())
                        {
                        TPRINT(("%s (%s:%u)",
                                Current->GetFunction(),
                                Current->GetModule(),
                                Current->GetLineNumber()));
                        }
                else    {
                        TPRINT(("%s (%u)",Current->GetProgram(),
                                          Current->GetLineNumber()));
                        }
                Current = Stack.GetNextEntry(Current);
                }
        return;
        }

int MCPExceptionHandler(const char *ProcName, EXCEPTION_POINTERS *EXInfo)
        {
        TERROR(("***************************************************"));
        TERROR(("**** EXCEPTION **** : %s",ProcName));
        if (EXInfo)
                {
                CONTEXT *Context = EXInfo->ContextRecord;
  #if defined(CONTEXT_AMD64)
                TERROR(("Context Rip: %p",Context->Rip));
                TERROR(("Context Rbp: %p",Context->Rbp));
                TERROR(("Context Rsp: %p",Context->Rsp));
  #elif defined(CONTEXT_i386)
                TERROR(("Context Eip: %p",Context->Eip));
                TERROR(("Context Ebp: %p",Context->Ebp));
                TERROR(("Context Esp: %p",Context->Esp));
  #endif
                }
        __try   {
                TraceStack(EXInfo);
                }
        __except (EXCEPTION_EXECUTE_HANDLER)
                {
                TERROR(("Exception in TraceStack()"));
                }
        TERROR(("***************************************************"));
        fflush(NULL);
        return EXCEPTION_EXECUTE_HANDLER;
        }

#endif // _WIN32

// ***************************************************************************
// **** main() ***************************************************************
// ***************************************************************************

static FILE *LevelTraceFile = NULL;
static bool JSONParserInitialized = false;

struct  {
        int client_fileno;
        FILE *clientout;
        void *hStdout;
        } static stdoutMask = {-1,NULL,NULL};

static bool _RunMCPServer(bool TestMode)
        {
        bool Status = true;
        // =======================
        // ==== Run MainLoops ====
        // =======================
        if (!TestMode && isatty(fileno(stdin)))
                {
                printf("Interactive: Enter one JSON request "
                       "per line. Send EOF (%s) to Exit.\n",EOFSequence);
                }
        if (TestMode)
                {
                if (MCPServer_Asynchronous)
                        {
                        TERROR(("Shouldn't run Test mode asynchronously."));
                        }
                Status = MCPTest(stdoutMask.clientout);
                }
        else if (!MCPServer_Asynchronous)
                {
                // ---------------------
                // ---- Syncronious ----
                // ---------------------
                Status = MCPMainLoop(stdin,stdoutMask.clientout);
                }
        else    {
                // ----------------------
                // ---- Asynchronous ----
                // ----------------------
                MCPServerIOReader MCPServerReader;
                Status = MCPServerReader.Start(stdin);
                if (!Status)
                        {
                        TERROR(("Can't Start MCPServerReader"));
                        }
                else    {
                        int rc = MCPServerMain.Run(stdin,stdoutMask.clientout);
                        if (MCPServerReader.IsStarted()) 
                                {
                                MCPServerReader.Kill(true);
                                }
                        Status = rc == 0;
                        }
                }
        return Status;
        }

static bool RunMCPServer(bool TestMode)
        {
        static const char *ProcName = "RunMCPServer";
        bool Status = false;
  #ifdef _WIN32
        InitializeSymbolEngine();
        __try   {
  #endif
                Status = _RunMCPServer(TestMode);
  #ifdef _WIN32
                }
        __except (MCPExceptionHandler(ProcName,GetExceptionInformation()))
                {
                TERROR(("%s: Hardware Exception - Exiting",ProcName));
                Status = false;
                }
  #endif
        return Status;
        }

static void Help()
        {
        TPRINT(("MCP Server: %s [Options]",MCPServer_Name));
        TPRINT((""));
        TPRINT(("Options:"));
        TPRINT(("     Test        : Test mode (Aspect dependent)."));
        TPRINT(("     -debug-     : Info logging."));
        TPRINT(("     -debug      : Debug logging."));
        TPRINT(("     -trace      : JSON in/out trace."));
        TPRINT(("     -help or /? : This help."));
        return;
        }

// -----------------------
// ---- main_mcp_init ----
// -----------------------

void main_mcp_deinit();

int main_mcp_init(int argc, const char *argv[])
        {
        static const char *ProcName = "main_mcp_init";
        // **********************
        // **** Process Args ****
        // **********************
        MCPServer_argc = argc;                  // Save for sharing to aspect.
        MCPServer_argv = argv;
        bool TestMode = false;
        for (int argi = 1; argi < argc; argi++)
                {
                if ((argv[argi][0] == '-' || argv[argi][0] == '/') &&
                    (argv[argi][1] == 'h' || argv[argi][1] == '?'))
                        {
                        Help();
                        return 2;
                        }
                else if (stricmp(argv[argi],"-trace") == 0)
                        {
                        MCPJSONTrace = true;
                        }
                else if (stricmp(argv[argi],"-debug-") == 0)
                        {
                        SetMaxTraceLevel(TRACELEVEL_INFO);
                        }
                else if (stricmp(argv[argi],"-debug") == 0)
                        {
                        SetMaxTraceLevel(TRACELEVEL_DEBUG);
                        }
                else if (stricmp(argv[argi],"test") == 0)
                        {
                        TestMode = true;
                        }
                }
        // *********************
        // **** Setup Trace ****
        // *********************
        if (!TestMode)
                {
                MemoryPrintf Canvas;
                Canvas.printf("%s%c%s_%u.log",MCPServerLOGPath,
                                              FILE_SEPERATOR,
                                              MCPServer_Name,
                                              getpid());
                const char *TraceFilename = Canvas.GetBuffer();
                LevelTraceFile = fopen(TraceFilename,"w");
                if (!LevelTraceFile)
                        {
                        fprintf(stderr,"Can't Open: %s\n",TraceFilename);
                        return 3;
                        }
                fprintf(stderr,"Trace File: %s\n",TraceFilename);
                fflush(stderr);
                SetLevelTraceFunction(LevelTraceFileTrace,LevelTraceFile);
                TINFO(("Trace File: %s",TraceFilename));
                }
        // ****************************************************
        // **** Isolate MCP transport stream (Hide stdout) ****
        // ****************************************************
        // For rogue libraries or mis-behaving aspects.
        // ****************************************************
        fflush(stdout);
        stdoutMask.client_fileno = dup(fileno(stdout));
        stdoutMask.clientout = fdopen(stdoutMask.client_fileno,"w");
        if (!stdoutMask.clientout)
                {
                TERROR(("Can't isolate stdout; fdopen() failed (%d).",errno));
                close(stdoutMask.client_fileno);
                stdoutMask.client_fileno = -1;
                main_mcp_deinit();
                return 4;
                }
        dup2(fileno(stderr),fileno(stdout));
  #ifdef _WIN32
        // ------------------------------------------
        // Windows specific (for ironclad isolation).
        // ------------------------------------------
        stdoutMask.hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);
        SetStdHandle(STD_OUTPUT_HANDLE,hStderr);
  #endif
        // ********************************
        // **** Initialize JSON Parser ****
        // ********************************
        InitJSONParser();
        JSONParserInitialized = true;
        return 0;
        }

// ------------------
// ---- main_mcp ----
// ------------------

int main_mcp()
        {
        static const char *ProcName = "main_mcp";
        // **********************
        // **** Process Args ****
        // **********************
        bool TestMode = false;
        for (int argi = 1; argi < MCPServer_argc; argi++)
                {
                if (stricmp(MCPServer_argv[argi],"test") == 0)
                        {
                        TestMode = true;
                        }
                }
        // ************************
        // **** Run MCP Server ****
        // ************************
        bool Status = MCPServer_OnStartup();
        if (!Status)
                {
                TERROR(("%s: MCPServer_OnStartup failed.",ProcName));
                }
        else    {
                if (OnStartupMCPSampleExHook) OnStartupMCPSampleExHook();
                if (InitMCPAsyncHook) InitMCPAsyncHook();
                Status = RunMCPServer(TestMode);
                if (DeinitMCPAsyncHook) DeinitMCPAsyncHook();
                if (DeinitMCPSampleHook) DeinitMCPSampleHook();
                if (OnShutdownMRTRHook) OnShutdownMRTRHook();
                MCPServer_OnShutdown();
                }
        MCPServerMain.ClearQueue();
        return Status ? 0 : 1;
        }

// -------------------------
// ---- main_mcp_deinit ----
// -------------------------

void main_mcp_deinit()
        {
        static const char *ProcName = "main_mcp_deinit";
        if (JSONParserInitialized)
                {
                DeinitJSONParser();
                JSONParserInitialized = false;
                }
        // ************************
        // **** Restore stdout ****
        // ************************
  #ifdef _WIN32
        if (stdoutMask.hStdout)
                {
                SetStdHandle(STD_OUTPUT_HANDLE,stdoutMask.hStdout);
                stdoutMask.hStdout = NULL;
                }
  #endif
        if (stdoutMask.clientout)
                {
                fflush(stdoutMask.clientout);
                fflush(stdout);
                dup2(stdoutMask.client_fileno,fileno(stdout));
                fclose(stdoutMask.clientout);
                stdoutMask.clientout = NULL;
                stdoutMask.client_fileno = -1;
                }
        // ***********************
        // **** Cleanup Trace ****
        // ***********************
        if (LevelTraceFile)
                {
                SetLevelTraceFunction(NULL,NULL);
                fclose(LevelTraceFile);
                LevelTraceFile = NULL;
                }
        return;
        }

// ==============
// ==== main ====
// ==============

#ifndef NO_MAIN_MCP

int main(int argc, const char *argv[])
        {
        static const char *ProcName = "main";
        int rc_main = main_mcp_init(argc,argv);
        if (rc_main)
                {
                TERROR(("%s: main_mcp_init() returned: %d",ProcName,rc_main));
                return rc_main;
                }
        rc_main = main_mcp();
        if (rc_main)
                {
                TERROR(("%s: main_mcp() returned: %d",ProcName,rc_main));
                }
        main_mcp_deinit();
        return rc_main;
        }

#endif /* NO_MAIN_MCP */

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
