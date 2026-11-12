// JSON Parser for MCPServer: MCPServerJSON.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServerJSON.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2016 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
  
#include <ctype.h>
#include <stdio.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <JSONParser.h>

#include <MCPServerJSON.h>

// ***************************************************************************
// **** Utilities ************************************************************
// ***************************************************************************

#if defined(__BORLANDC__)

        static int snprintf(char *Buffer, size_t Size, const char *Format,...)
                {
                va_list args;
                FILE *io = tmpfile();
                if (!io) return 0;
                va_start(args,Format);
                int rc = vfprintf(io,Format,args);
                va_end(args);
                if (rc > 0)
                        {
                        rewind(io);
                        int rc = fread(Buffer,1,Size,io);
                        if (rc >= 0 && (size_t)rc < Size) Buffer[rc] = '\0';
                        }
                fclose(io);
                if (rc < 0) rc = 0;
                return rc >= 0 && (size_t)rc < Size ? rc : -rc;
                }

#elif defined(_MSC_VER)

        #define snprintf _snprintf

#endif

// ***************************************************************************
// **** Independent File, Time, String Utilties ******************************
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
// **** BuildFQFN (Construct a Fully Qualified Path) *************************
// ***************************************************************************

char* BuildFQFN(const char *Path, const char *Filename)
        {
        static const char *ProcName = "BuildFQFN";
        if (!Filename && !Path) return NULL;
        size_t PathLength = Path ? strlen(Path) : 0;
        size_t FileLength = Filename ? strlen(Filename) : 0;
        size_t FullLength = PathLength + FileLength + 1;
        char *Fullname = (char *)malloc(FullLength + 1);
        if (!Fullname)
                {
                TERROR(("%s: Alloc Error Filename: %s",ProcName,
                                                        Filename 
                                                        ? Filename 
                                                        : Path));
                return NULL;
                }
        bool HasSeperator = false;
        char *Dest = Fullname;
        const char *Src = Path;
        // --------------
        // ---- Path ----
        // --------------
        if (PathLength)
                {
                for (unsigned i=0; i < PathLength; i++)
                        {
                        if (*Src == '"' || *Src == '\'')
                                {
                                Src++;  // Strip quotes.
                                }
                        else if (*Src == FILE_SEPERATOR_WIN || *Src == FILE_SEPERATOR_UNIX)
                                {
                                if (!HasSeperator) *Dest++ = FILE_SEPERATOR;
                                HasSeperator = true;
                                Src++;
                                }
                        else    {
                                HasSeperator = false;
                                *Dest++ = *Src++;
                                }
                        }
                if (!HasSeperator && FileLength) *Dest++ = FILE_SEPERATOR;
                }
        // --------------
        // ---- File ----
        // --------------
        HasSeperator = true;
        Src = Filename;
        for (unsigned i=0; i < FileLength; i++)
                {
                if (*Src == '"' || *Src == '\'')
                        {
                        Src++;  // Strip quotes.
                        }
                else if (*Src == FILE_SEPERATOR_WIN || *Src == FILE_SEPERATOR_UNIX)
                        {
                        if (!HasSeperator) *Dest++ = FILE_SEPERATOR;
                        HasSeperator = true;
                        Src++;
                        }
                else    {
                        HasSeperator = false;
                        *Dest++ = *Src++;
                        }
                }
        *Dest = '\0';
        // ----------------------------------------------
        // ---- Trim Trailing FILE_SEPERATOR ------------
        // ---- (keep root "/" and drive root "X:\") ----
        // ----------------------------------------------
        char *Dest_1 = Dest > Fullname ? Dest - 1 : NULL;
        if (Dest_1)
                {
                char *Dest_2 = Dest_1 > Fullname ? Dest_1 - 1 : NULL;
                if (*Dest_1 == FILE_SEPERATOR_WIN || 
                    *Dest_1 == FILE_SEPERATOR_UNIX)
                        {
                        if (Dest_2 && *Dest_2 != ':') *Dest_1 = '\0';
                        }
                }
        return Fullname;
        }

void FreeFQFN(char **Fullname)
        {
        if (Fullname && *Fullname)
                {
                free(*Fullname);
                *Fullname = NULL;
                }
        return;
        }

// ***************************************************************************
// **** Error Diagnostics ****************************************************
// ***************************************************************************

#define SurroundCHARS 30

static void ShowJSONError(const char *Text, size_t Length, size_t ErrorPos)
        {
        char Buffer[2*SurroundCHARS + 1];
        size_t Start = ErrorPos < SurroundCHARS ? ErrorPos : SurroundCHARS;
        size_t Rest = Length - ErrorPos;
        size_t Count = Start + (Rest < SurroundCHARS ? Rest : SurroundCHARS);
        const char *Pos = Text + ErrorPos - Start;
        char *BufferPos = Buffer;
        while (Count--)
                {
                *BufferPos++ = isspace(*Pos) ? ' ' : *Pos;
                Pos++;
                }
        *BufferPos = '\0';
        TPRINT(("Context:     %s",Buffer));
        TPRINT(("            %*c^",Start+1,' '));
        return;
        }

static const char* FormatJSONError(char *Buffer,
                                   const size_t BufferLen,
                                   const char *JSONBuffer,
                                   size_t JSONBufferLength,
                                   Error_ParseJSON &PE)
        {
        int rc;
        if (!BufferLen) return "";
        char *Pos = Buffer;
        size_t PosLen = BufferLen;
        // **********************
        // **** Message Text ****
        // **********************
        rc = snprintf(Pos,PosLen,"%s : ",PE.GetErrorText());
        Pos += ((unsigned)rc >= PosLen) ? PosLen : rc;
        PosLen -= ((unsigned)rc >= PosLen) ? PosLen : rc;
        JSONLexicalItem *Item = PE.GetItem();
        if (Item)
                {
                // *********************
                // **** Error Token ****
                // *********************
                rc = snprintf(Pos,PosLen,"%.*s : ",(int)Item->Length,
                                                      Item->String);
                Pos += ((unsigned)rc >= PosLen) ? PosLen : rc;
                PosLen -= ((unsigned)rc >= PosLen) ? PosLen : rc;
                if (JSONBuffer)
                        {
                        // *************************
                        // **** Show JSON Context ****
                        // *************************
                        size_t ErrorPos = Item->String - JSONBuffer;
                        size_t Start = ErrorPos < SurroundCHARS ?
                                       ErrorPos : 
                                       SurroundCHARS;
                        size_t Rest = JSONBufferLength - ErrorPos;
                        size_t Count = Start + (Rest < SurroundCHARS ? 
                                                Rest : 
                                                SurroundCHARS);
                        rc = snprintf(Pos,PosLen,"%.*s^%.*s^%.*s",
                                      (int)Start,
                                      JSONBuffer + ErrorPos - Start,
                                      (int)Item->Length,
                                      JSONBuffer + ErrorPos,
                                      (int)(Count - Item->Length - Start),
                                      JSONBuffer + ErrorPos + Item->Length);
                        }
                }
        Buffer[BufferLen-1] = '\0';
        return Buffer;
        }

void PrintFormatedJSONError(const char *JSONBuffer,
                          size_t JSONBufferLength,
                          Error_ParseJSON &PE)
        {
        char Buffer[256];
        FormatJSONError(Buffer,sizeof(Buffer),JSONBuffer,JSONBufferLength,PE);
        TPRINT(("%s",Buffer));
        return;
        }

// ***************************************************************************
// **** JSON Parser State Machines (File-Static) *****************************
// ***************************************************************************

static JSONSymbolStateMachine LexMachine;
static StateMachine LRMachine(0);
static Mutex csJSONParser;                      // Serializes all parsing.

void InitJSONParser()
        {
        SetMultithreadSafeAlloc(false);         // csJSONParser serializes.
        SetExpAlloc(JSONExpAlloc,JSONExpDealloc);
        SetupJSONTransitions(LRMachine);
        BuildJSONDetectTables();
        return;
        }

void DeinitJSONParser()
        {
        static const char *ProcName = "DeinitJSONParser";
        LRMachine.Reset(0);
        TDEBUG(("%s: JSON Heap Size: %u (items)",ProcName,JSONExpGetDepth()));
        return;
        }

// ***************************************************************************
// **** ParseJSON_mt (Low-Level Parser) **************************************
// ***********************************
//
//      Acquires csJSONParser for the duration of the parse. The
//      parser's node allocator (JSONParserExp) is a shared pool
//      that is not thread-safe. The mutex is released before 
//      returning, so the caller owns the Node* tree outright and
//      may walk it freely in any thread. Call DeleteJSON_mt() to
//      release the tree back to the pool (re-acquires the mutex).
//
// ***************************************************************************

Node* ParseJSON_mt(const char *JSONBuffer)
        {
        csJSONParser.Take();
        Node *StartNode = NULL;
        const char *NonTerminalGoal = NULL;
        Node* JSONExpression = NULL;
        try     {
                JSONExpression = ParseJSONBuffer(LRMachine,
                                                 LexMachine,
                                                 JSONBuffer,
                                                 true,
                                                 StartNode,
                                                 NonTerminalGoal);
                }
        catch (Error_ParseJSON &PE)
                {
                TPRINT(("****************************************"));
                TPRINT(("Parse Error: %s",PE.GetErrorText()));
                if (PE.GetParserState() != Error_NOSTATE)
                        {
                        TPRINT(("Parse State: %u",PE.GetParserState()));
                        }
                JSONLexicalItem *Item = PE.GetItem();
                if (Item)
                        {
                        size_t ErrorPos = Item->String - JSONBuffer;
                        TPRINT(("Position:    %u",ErrorPos));
                        TPRINT(("SymbolClass: %u",Item->SymbolClass));
                        TPRINT(("SymbolID:    %u",Item->SymbolID));
                        TPRINT(("NodeID(Ew_): %u",Item->ParserNodeID));
                        TPRINT(("Token:       %.*s",Item->Length,
                                                    Item->String));
                        ShowJSONError(JSONBuffer,strlen(JSONBuffer),ErrorPos);
                        }
                TPRINT(("****************************************"));
                PrintFormatedJSONError(JSONBuffer,strlen(JSONBuffer),PE);
                TPRINT(("****************************************"));
                }
        catch (ErrorBase &E)
                {
                TPRINT(("****************************************"));
                TPRINT(("Cought Error: %s",E.GetErrorText()));
                TPRINT(("****************************************"));
                }
        csJSONParser.Release();
        return JSONExpression;
        }

// ***************************************************************************
// **** DeleteJSON_mt (Thread-Safe Expression Deallocation) *****************
// ***************************************************************************
//
//      Acquires csJSONParser to safely return parse-tree nodes to
//      the shared pool allocator, then releases the mutex.
//
// ***************************************************************************

void DeleteJSON_mt(Node *JSONExpression)
        {
        if (!JSONExpression) return;
        csJSONParser.Take();
        delete JSONExpression;
        csJSONParser.Release();
        return;
        }

// ***************************************************************************
// **** BuildJSONObject (Thread-Safe JSON Parse + Build) *********************
// ***************************************************************************

JSON_Object* BuildJSONObject(const char *JSONBuffer)
        {
        static const char *ProcName = "BuildJSONObject";
        Node *JSONExpression = ParseJSON_mt(JSONBuffer);
        JSON_Object *JSONObj = NULL;
        if (JSONExpression)
                {
                JSONObj = BuildJSONObject(JSONExpression);
                DeleteJSON_mt(JSONExpression);
                }
        return JSONObj;
        }

// ***************************************************************************
// **** MCPInputRequest ******************************************************
// ***************************************************************************

MCPInputRequest::MCPInputRequest(JSON_Object **Request) 
        {
        JSONRequest = *Request;
        *Request = NULL;
        return;                        
        }

MCPInputRequest::MCPInputRequest(MCPInputRequest &Source)
        {
        if (this != &Source)
                {
                JSONRequest = NULL;
                *this = Source;
                }
        return;
        }

MCPInputRequest& MCPInputRequest::operator =(MCPInputRequest &Source)
        {
        if (this != &Source)
                {
                Clear();
                if (Source.JSONRequest)
                        {
                        JSONRequest = new JSON_Object(*Source.JSONRequest);
                        }
                else JSONRequest = NULL;
                }
        return *this;
        }

void MCPInputRequest::Clear()
        {
        if (JSONRequest) 
                {
                delete JSONRequest;
                JSONRequest = NULL;
                }
        return;
        }

JSON_Member* MCPInputRequest::FindMember(const char *Key)
        {
        if (!JSONRequest || !Key) return NULL;
        return JSONRequest->FindMember(Key);
        }

JSON_Value* MCPInputRequest::FindMemberValue(const char *Key)
        {
        if (!JSONRequest || !Key) return NULL;
        return JSONRequest->FindMemberValue(Key);
        }

JSON_Object* MCPInputRequest::Get_error()
        {
        JSON_Object *error = NULL;
        JSON_Value *Value = FindMemberValue("error");
        if (Value && Value->isObject())
                {
                error = (JSON_Value_Object *)Value;
                }
        return error;
        }

JSON_Value* MCPInputRequest::Get_id() 
        {
        static const char *ProcName = "MCPInputRequest::Get_id";
        JSON_Value *Value = FindMemberValue("id");
        if (Value && !Value->isString() 
                  && !Value->isNumber() 
                  && !Value->isNull())
                {
                TERROR(("%s: id is not a String, Number or Null",ProcName));
                return NULL;
                }
        return Value;
        }

const char* MCPInputRequest::Get_jsonrpc() 
        {
        const char *jsonrpc = NULL;
        JSON_Value *Value = FindMemberValue("jsonrpc");
        if (Value && Value->isString())
                {
                JSON_Value_String *String = (JSON_Value_String *)Value;
                jsonrpc = String->string;
                }
        return jsonrpc;
        }

const char* MCPInputRequest::Get_method() 
        {
        const char *method = NULL;
        JSON_Value *Value = FindMemberValue("method");
        if (Value && Value->isString())
                {
                JSON_Value_String *String = (JSON_Value_String *)Value;
                method = String->string;
                }
        return method;
        }

JSON_Object* MCPInputRequest::Get_params()
        {
        JSON_Object *params = NULL;
        JSON_Value *Value = FindMemberValue("params");
        if (Value && Value->isObject())
                {
                params = (JSON_Value_Object *)Value;
                }
        return params;
        }

JSON_Value* MCPInputRequest::Get_param(const char *Key)
        {
        static const char *ProcName = "MCPInputRequest::Get_param";
        if (!Key) return NULL;
        JSON_Object *Params = Get_params();
        if (!Params)
                {
                TERROR(("%s: No Params",ProcName));
                return NULL;
                }
        JSON_Value *Param = Params->FindMemberValue(Key);
        return Param;
        }

JSON_Object* MCPInputRequest::Get_result()
        {
        JSON_Object *result = NULL;
        JSON_Value *Value = FindMemberValue("result");
        if (Value && Value->isObject())
                {
                result = (JSON_Value_Object *)Value;
                }
        return result;
        }

// ----------------------------------
// ---- Get type specific params ----
// ----------------------------------

bool MCPInputRequest::Get_param_int(int *iParam, const char *Key)
        {
        static const char *ProcName = "MCPInputRequest::Get_param_int";
        if (!Key) return NULL; 
        JSON_Value *Param = Get_param(Key);
        if (!Param)
                {
                TERROR(("%s: No Param '%s'",ProcName,Key));
                return false;
                }
        JSON_Value_Number *vNumber = Param->isNumber() 
                                    ? (JSON_Value_Number *)Param
                                    : NULL;
        if (!vNumber || !vNumber->isInt())
                {
                TERROR(("%s: Param '%s' isn't an Integer",ProcName,Key));                
                return false;
                }
        if (iParam) *iParam = vNumber->int_number;
        return true;
        }

JSON_Object* MCPInputRequest::Get_param_object(const char *Key)
        {
        static const char *ProcName = "MCPInputRequest::Get_param_object";
        if (!Key) return NULL;
        JSON_Value *Param = Get_param(Key);
        if (!Param)
                {
                TERROR(("%s: No Param '%s'",ProcName,Key));
                return NULL;
                }
        JSON_Value_Object *vObject = Param->isObject()
                                     ? (JSON_Value_Object *)Param
                                     : NULL;
        if (!vObject)
                {
                TERROR(("%s: Param '%s' isn't an Object",ProcName,Key));
                return NULL;
                }
        return vObject;
        }

const char* MCPInputRequest::Get_param_string(const char *Key)
        {
        static const char *ProcName = "MCPInputRequest::Get_param_string";
        if (!Key) return NULL;
        JSON_Object *Params = Get_params();
        if (!Params)
                {
                TERROR(("%s: No Params",ProcName));
                return NULL;
                }
        return Params->Find_member_string(Key);
        }

// ***************************************************************************
// **** MCP Protocol Version (parsed YYYYMMDD from _meta) ********************
// ***************************************************************************
//
//      MCPInputRequest::Version() reports the per-request protocol version
//      from params._meta["io.modelcontextprotocol/protocolVersion"]
//      ("YYYY-MM-DD") as an integer YYYYMMDD -- exactly what it finds, no
//      policy. 
//
//      _meta absent (stateful handshake)       -> MCPProtocolVersion_20251125.
//      _meta present, protocolVersion absent   -> MCPProtocolVersion_20251125.
//      protocolVersion present but malformed   -> MCPProtocolVersion_Invalid.
//
// ***************************************************************************

static const char metaVersion[] = "io.modelcontextprotocol/protocolVersion";

int MCPProtocolVersionInt(const char *Version)
        {
        //
        // Parse an MCP "YYYY-MM-DD" protocol string to YYYYMMDD.
        // Returns 0 if malformed.
        //
        if (!Version) return MCPProtocolVersion_Invalid;
        int V = 0;
        int nDigits = 0;
        for (const char *p = Version; *p && nDigits < 8; p++)
                {
                if (*p >= '0' && *p <= '9')
                        {
                        V = V * 10 + (*p - '0');
                        nDigits++;
                        }
                }
        return nDigits == 8 ? V : MCPProtocolVersion_Invalid;
        }

int MCPInputRequest::Version()
        {
        JSON_Object *Params = Get_params();
        if (!Params) return MCPProtocolVersion_20251125;
        JSON_Object *Meta = Params->Find_member_object("_meta");
        if (!Meta) return MCPProtocolVersion_20251125;
        // ----------------------------------------------------------------
        // ---- Only protocolVersion (not _meta itself) marks 20260728 ----
        // ---- _meta also carries progressToken, traceparent, etc.    ----
        // ----------------------------------------------------------------
        const char *Version = Meta->Find_member_string(metaVersion);
        if (!Version) return MCPProtocolVersion_20251125;
        return MCPProtocolVersionInt(Version);
        }

// ***************************************************************************
// **** Parse JSON Input Request *********************************************
// ***************************************************************************

MCPInputRequest* ParseJSON_MCPInput(const char *JSONBuffer)
        {
        static const char *ProcName = "ParseJSON_MCPInput";
        JSON_Object *JSONRequest = BuildJSONObject(JSONBuffer);
        if (!JSONRequest)
                {
                TERROR(("%s: BuildJSONObject() FAILED",ProcName));
                return NULL;
                }
        MCPInputRequest *MCPRequest = new MCPInputRequest(&JSONRequest);
        if (!MCPRequest)
                {
                TERROR(("%s: Unable to allocate MCPRequest",ProcName));
                if (JSONRequest) delete JSONRequest;
                return NULL;
                }
        return MCPRequest;
        }

// ***************************************************************************
// **** ValidateJSON *********************************************************
// ***************************************************************************

bool ValidateJSON(const char *JSONBuffer)
        {
        static const char *ProcName = "ValidateJSON";
        Node *JSONExpression = ParseJSON_mt(JSONBuffer);
        if (JSONExpression) DeleteJSON_mt(JSONExpression);
        else    {
                TERROR(("%s: Parse FAILED",ProcName));
                return false;
                }
        return true;
        }

// ***************************************************************************
// **** TraceJSON ************************************************************
// ***************************************************************************

static int TraceJSONDocumentFn(void *UserPtr, const char *Format, va_list Args)
        {
        MemoryPrintf &Canvas = *(MemoryPrintf *)UserPtr;
        return Canvas.vprintf(Format,Args);
        }

bool TraceJSON(const char *JSONBuffer, bool Pretty)
        {
        static const char *ProcName = "TraceJSON";
        MemoryPrintf Canvas;
        Node *JSONExpression = ParseJSON_mt(JSONBuffer);
        if (!JSONExpression)
                {
                TERROR(("%s: Parse FAILED",ProcName));
                return false;
                }
        PrintJSONDocument(JSONExpression,TraceJSONDocumentFn,&Canvas,Pretty);
        DeleteJSON_mt(JSONExpression);
        TPRINT(("%s",Canvas.GetBuffer()));
        return true;
        }

void TraceJSON(MCPInputRequest &MCPRequest, bool Pretty)
        {
        static const char *ProcName = "TraceJSON";
        JSON_Object *JSONRequest = MCPRequest._getJSONRequest();
        if (!JSONRequest) return;
        MemoryPrintf Canvas;
        PrintJSONObject(Canvas,*JSONRequest,true,Pretty);
        TPRINT(("%s",Canvas.GetBuffer()));
        return;
        }
           
// ***************************************************************************
// **** ExtractSampleText ****************************************************
// ***************************************************************************
//
//      Pull the LLM text out of a sampling response, spanning both
//      protocol shapes so callers stay version-agnostic:
//        Classic (2024-11-05 / external): result.content.text
//        MRTR    (2026-07-28 resume):     inputResponses[0]
//                                         .content.content.text
//      A flattened .content.text is tolerated for the MRTR shape.
//
// ***************************************************************************

const char* ExtractSampleText(MCPInputRequest &MCPRequest)
        {
        // ---- Classic sampling result (native MCP + external). ----
        JSON_Object *Result = MCPRequest.Get_result();
        if (Result)
                {
                JSON_Object *Content = Result->Find_member_object("content");
                return Content ? Content->Find_member_string("text") : NULL;
                }
        // ---- MRTR resume: sole inputResponses entry, positional. ----
        JSON_Object *Params = MCPRequest.Get_params();
        JSON_Object *Responses = Params
                                 ? Params->Find_member_object("inputResponses")
                                 : NULL;
        if (!Responses || Responses->nMembers == 0) return NULL;
        JSON_Value *First = Responses->Members[0]->Value;
        if (!First || !First->isObject()) return NULL;
        JSON_Object *Response = (JSON_Object *)First;
        JSON_Object *Content = Response->Find_member_object("content");
        if (!Content) return NULL;
        JSON_Object *Message = Content->Find_member_object("content");
        return Message ? Message->Find_member_string("text")
                       : Content->Find_member_string("text");
        }

// ***************************************************************************
// **** PrintJSONObject to MemoryPrintf Canvas *******************************
// ***************************************************************************

static int CanvasJSONDocumentFn(void *UserPtr, const char *Format, va_list Args)
        {
        MemoryPrintf &Canvas = *(MemoryPrintf *)UserPtr;
        return Canvas.vprintf(Format,Args);
        }

int PrintJSONObject(MemoryPrintf &Canvas,       // Returns bytes output.
                    JSON_Object &Object,        
                    bool asJSON,
                    bool Pretty)           
        {
        PrintJSONDocumentFn_t PrintFn = CanvasJSONDocumentFn;
        void *PrintUserPtr = &Canvas;
        return PrintJSONObject(Object,PrintFn,PrintUserPtr,asJSON,Pretty);
        }

int PrintJSONValue(MemoryPrintf &Canvas,        // Returns bytes output.
                   JSON_Value &Value,           
                   bool asJSON,
                   bool Pretty)
        {
        PrintJSONDocumentFn_t PrintFn = CanvasJSONDocumentFn;
        void *PrintUserPtr = &Canvas;
        return PrintJSONValue(Value,PrintFn,PrintUserPtr,asJSON,Pretty);
        }

// ***************************************************************************
// **** MCP id Utilites ******************************************************
// ***************************************************************************

// -----------------------------------
// ---- id2string (Print Utility) ----
// -----------------------------------

void _MCPIdString::PrintToBuffer(JSON_Value *id)
        {
        static const char *ProcName = "_MCPIdString::PrintToBuffer";
        // ---------------------------------------------------------------
        // id2string() is used to EMIT the id ("\"id\":%s"), not merely to
        // trace it, so the result must always be a valid JSON token and
        // must round-trip the peer's id exactly -- a quietly altered id
        // correlates to the wrong request at the other end.
        // ---------------------------------------------------------------
        if (!id) strcpy(idBuffer,"null");
        else if (id->isString())
                {
                JSON_Value_String *id_String = (JSON_Value_String *)id;
                const char *String = id_String->string ? id_String->string
                                                       : "";
                // -------------------------------------------------------
                // Two quotes and a NUL must fit. snprintf() would drop the
                // closing quote on overflow -- malformed JSON -- and a
                // shortened id is worse than an admitted unknown, so an
                // id that cannot be represented is reported as null.
                // -------------------------------------------------------
                size_t Length = strlen(String);
                if (Length + 3 > sizeof(idBuffer))
                        {
                        TERROR(("%s: id too long (%u) for idBuffer (%u)",
                                ProcName,
                                (unsigned)Length,
                                (unsigned)sizeof(idBuffer)));
                        strcpy(idBuffer,"null");
                        }
                else    {
                        snprintf(idBuffer,sizeof(idBuffer),"\"%s\"",String);
                        idBuffer[sizeof(idBuffer)-1] = '\0';
                        }
                }
        else if (id->isNumber())
                {
                JSON_Value_Number *id_Number = (JSON_Value_Number *)id;
                // -------------------------------------------------------
                // int_number is a 32-bit truncation and double_number a
                // lossy view of a big integer, so pick the field that
                // represents this value exactly. isInt() reports when the
                // short form is faithful; isDouble() when it is fractional.
                // -------------------------------------------------------
                if (id_Number->isInt())
                        {
                        snprintf(idBuffer,sizeof(idBuffer),"%d",
                                 id_Number->int_number);
                        }
                else if (id_Number->isDouble())
                        {
                        snprintf(idBuffer,sizeof(idBuffer),"%g",
                                 id_Number->double_number);
                        }
  #ifdef _WIN32
                else snprintf(idBuffer,sizeof(idBuffer),"%I64d",
                              id_Number->long_number);
  #else
                else snprintf(idBuffer,sizeof(idBuffer),"%lld",
                              id_Number->long_number);
  #endif
                idBuffer[sizeof(idBuffer)-1] = '\0';
                }  
        else strcpy(idBuffer,"null");
        }

// -----------------------------------
// ---- id Compare -------------------
// -----------------------------------

bool idcmp(JSON_Value &cmpA, JSON_Value &cmpB)
        {
        bool Match = false;
        if (cmpA.isString() && cmpB.isString())
                {
                JSON_Value_String &cmpAs = (JSON_Value_String &)cmpA;
                JSON_Value_String &cmpBs = (JSON_Value_String &)cmpB;
                Match = strcmp(cmpAs.string,cmpBs.string) == 0;
                }
        else if (cmpA.isNumber() && cmpB.isNumber())
                {
                JSON_Value_Number &cmpAn = (JSON_Value_Number &)cmpA;
                JSON_Value_Number &cmpBn = (JSON_Value_Number &)cmpB;
                Match = cmpAn.double_number == cmpBn.double_number;
                }
        return Match;
        }

// ---------------------------------------------------------
// ---- FuzzyPeekJSONId (Debug Utility)  -------------------
// --------------------------------------
//
//      Quick text scan for "id" in a JSON-RPC string.
//      Not a real parser -- just enough for debug tracing.
//
//                                   by Claude Opus 4.6.2024
// ---------------------------------------------------------

const char* FuzzyPeekJSONId(const char *JSON, char *Buf, size_t BufLen)
        {
        if (!JSON || !Buf || !BufLen) return "?";
        const char *p = strstr(JSON, "\"id\"");
        if (!p) return "?";
        p += 4;
        while (*p == ' ' || *p == ':') p++;
        size_t i = 0;
        while (*p && *p != ',' && *p != '}' && i < BufLen - 1)
                Buf[i++] = *p++;
        Buf[i] = '\0';
        // ---------------------------------------------------------------
        // Mark a truncated id. Without this the tail of a long string id
        // reads as an unterminated quote -- in a log that is indistinguish-
        // able from genuinely malformed output, and sends the reader
        // hunting a bug that is not there.
        // ---------------------------------------------------------------
        if (*p && *p != ',' && *p != '}' && BufLen > 4)
                {
                strcpy(Buf + BufLen - 4,"...");
                }
        return Buf;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
