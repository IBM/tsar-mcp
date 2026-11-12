// MCP Server Hybrid Blocking Sampler : MCPSampleEx.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPSampleEx.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef _WIN32
        #ifndef stricmp
                #define stricmp strcasecmp
        #endif
#endif

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPSampleEx.h>

static bool MCPJSONTrace = false;
static bool ExternPriorityDEFAULT = false; // true = Prefer External sampling.

// ***************************************************
// **** ACP Workspace Retrieval **********************
// *****************************
// The ACP agent requires a workspace. Set either via:
//       MCPSyncSamplerEx::SetWorkspace()
// or via:
//       MCPSample_ACP_CWD environment variable.
// ***************************************************

static const char* get_ACPWorkspace(const char *Cwd)
        {
        if (Cwd && *Cwd) return Cwd;
        Cwd = getenv("MCPSample_ACP_CWD");
        return (Cwd && *Cwd) ? Cwd : NULL;
        }

// ***********************************
// **** isExternSamplingSupported ****
// ***********************************

bool isExternSamplingSupported()
        {
        static bool ExSupportedChecked = false;
        static bool ExSupported = false;
        if (ExSupportedChecked) return ExSupported;
        ExSupportedChecked = true;              // Probe once; cache result.
        // ---------------------------------------
        // ---- Check External AI Environment ----
        // ---------------------------------------
        const char *URLEndpoint = getenv("MCPSample_AI_URL");
        const char *AIModel = getenv("MCPSample_AI_MODEL");
        const char *APIKey = getenv("MCPSample_AI_KEY");
        bool AIEnvarSet = URLEndpoint && *URLEndpoint &&
                          AIModel &&  *AIModel &&
                          APIKey && *APIKey;
        // --------------------------------
        // ---- Check Functioning cURL ----
        // --------------------------------
        bool cURLworks = false;
        if (AIEnvarSet)
                {
                cURLworks = cURL_Check();
                }
        ExSupported = AIEnvarSet && cURLworks;
        if (ExSupported)
                {
                TINFO(("SamplingEx: External Sampling Supported"));
                }
        else    {
                TINFO(("SamplingEx: External Sampling NOT Supported"));
                }
        if (!AIEnvarSet)   
                {
                if (!URLEndpoint || !*URLEndpoint)
                        {
                        TINFO(("SamplingEx: MCPSample_AI_URL not set"));
                        }
                if (!AIModel || !*AIModel)
                        {
                        TINFO(("SamplingEx: MCPSample_AI_MODEL not set"));
                        }
                if (!APIKey || !*APIKey)
                        {
                        TINFO(("SamplingEx: MCPSample_AI_KEY not set"));
                        }
                }
        else if (!cURLworks)    
                {
                TERROR(("SamplingEx: Unable to invoke cURL"));
                }
        return ExSupported;
        }

// ***************************************************
// **** External Sampling Priority *******************
// *********************************
// MCPSample_EXTERN_PRIORITY prioritizes external over 
//                           internal sampling.
// Which external engine then runs is decided by which
// environment is configured -- ACP or AI model.
// ***************************************************

static void ProbeExternPriority()
        {
        const char *PriorityENV = getenv("MCPSample_EXTERN_PRIORITY");
        if (!PriorityENV)
                {
                TINFO(("SamplingEx: MCPSample_EXTERN_PRIORITY not set"));
                return;
                }
        char cENV = PriorityENV[0];
        ExternPriorityDEFAULT = cENV == '1' || 
                                cENV == 'Y' || cENV == 'y' || 
                                cENV == 'T' || cENV == 't';
        if (ExternPriorityDEFAULT)
                {
                TINFO(("SamplingEx: External Sampling Priority"));
                }
        else    {
                TINFO(("SamplingEx: Internal Sampling Priority"));
                }
        return;
        }

// ***********************************************
// **** isSamplingExSupported(MCPRequest) ********
// ***********************************************

static const char metaClientCaps[] =
        "io.modelcontextprotocol/clientCapabilities";

bool isMRTRSamplingSupported(MCPInputRequest &MCPRequest)
        {
        int ProtocolVersion = MCPProtocolVersion(MCPRequest);
        if (ProtocolVersion < MCPProtocolVersion_20260728) return false;
        JSON_Object *Params = MCPRequest.Get_params();
        if (!Params) return false;
        JSON_Object *Meta = Params->Find_member_object("_meta");
        if (!Meta) return false;
        JSON_Object *Caps = Meta->Find_member_object(metaClientCaps);
        if (!Caps) return false;
        if (!Caps->Find_member_object("sampling")) return false;
        return true;
        }

bool isSamplingExSupported(MCPInputRequest &MCPRequest)
        {
        return isSamplingSupported() ||
               isMRTRSamplingSupported(MCPRequest) ||
               isACPSamplingSupported() ||
               isExternSamplingSupported();               
        }

// *****************************
// **** TraceJSONDocumentFn ****
// *****************************

static int TraceJSONDocumentFn(void *UserPtr, const char *Format, va_list Args)
        {
        MemoryPrintf &Canvas = *(MemoryPrintf *)UserPtr;
        return Canvas.vprintf(Format,Args);
        }

// ***************************************************************************
// **** Enable MCPServerSample - Setup Function Pointers *********************
// ***************************************************************************

// -----------------------------------------------
// ---- MCP Sample Extension (External Hooks) ----
// -----------------------------------------------

extern void (*OnStartupMCPSampleExHook)();
extern void (*OnShutdownMCPSampleExHook)();

static void OnStartup()
        {
        // ----------------------------------
        // ---- Parse specific Arguments ----
        // ----------------------------------
        int argc = MCPServer_getArgc();
        const char** argv = MCPServer_getArgv();
        for (int argi = 1; argi < argc; argi++)
                {
                if (stricmp(argv[argi],"-trace") == 0)
                        {
                        MCPJSONTrace = true;
                        setACPJSONTrace(true);
                        }
                }
        // -------------------------------------------------------
        // Probe the engines now so env diagnostics show together.
        // -------------------------------------------------------
        isExternSamplingSupported();
        isACPSamplingSupported();
        ProbeExternPriority();
        return;
        }

static void OnShutdown()
        {
        DeInitSampleACP();
        return;
        }

struct _MCPServerSampleExInit
        {
        _MCPServerSampleExInit()
                {
                OnStartupMCPSampleExHook = OnStartup;
                OnShutdownMCPSampleExHook = OnShutdown;
                }
        };

static _MCPServerSampleExInit MCPServerSampleExInit;

// ***************************************************************************
// **** MCP Sample Emulator **************************************************
// ***************************************************************************

static MCPInputRequest* MCPSampleJSONBuilder(const char *AIModel,
                                             const char *Content,
                                             size_t Content_Length,
                                             bool QuoteContent)
        {
        static const char *ProcName = "MCPSampleJSONBuilder";
        // ------------------------------------------
        // ---- Build MCP Sampling Response JSON ----
        // ------------------------------------------
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":\"sample_emu_0\",");
        Canvas.printf("\"result\":{");
        Canvas.printf("\"role\":\"assistant\",");
        Canvas.printf("\"content\":{");
        Canvas.printf("\"type\":\"text\",");
        if (QuoteContent)
                {
                Canvas.printf("\"text\":\"%.*s\"",Content_Length,Content);
                }
        else    {
                Canvas.printf("\"text\":%.*s",Content_Length,Content);
                }
        Canvas.printf("},");
        Canvas.printf("\"model\":\"%s\",",AIModel);
        Canvas.printf("\"stopReason\":\"endTurn\"");
        Canvas.printf("}");
        Canvas.printf("}");
        // ------------------------------------------------
        // ---- Build MCP Sampling Response JSONObject ----
        // ------------------------------------------------
        MCPInputRequest *MCPRequest = NULL;
        const char *JSONBuffer = Canvas.GetBuffer();
        if (!JSONBuffer)
                {
                TERROR(("%s: Memory Error - no JSONBuffer",ProcName));
                }
        else    {
                if (MCPJSONTrace)
                        {
                        TPRINT(("------------------------------"));
                        TINFO(("%s: Tracing Input:",ProcName));
                        TraceJSON(JSONBuffer);
                        }
                MCPRequest = ParseJSON_MCPInput(JSONBuffer);
                if (!MCPRequest)
                        {
                        TERROR(("%s: ParseJSON_MCPInput() Failed",ProcName));
                        }
                }
        return MCPRequest;
        }

static MCPInputRequest* MCPSampleEmulator(const char *URLEndpoint,
                                          const char *AIModel,
                                          const char *AIResponse)
        {
        static const char *ProcName = "MCPSampleEmulator";
        // ------------------
        // Extract AI Content
        // ------------------
        Node *AIResponseNode = ParseJSON_mt(AIResponse);
        if (!AIResponseNode) 
                {
                TERROR(("%s: ParseJSON_mt() Error",ProcName));
                return NULL;
                }
        size_t Content_Length = 0;
        const char *Content = ExtractAIContent(URLEndpoint,
                                               AIModel,
                                               AIResponseNode,
                                               &Content_Length);
        if (!Content || !Content_Length)
                {
                TERROR(("%s: No Content found in AIResponse",ProcName));
                MemoryPrintf Canvas;
                PrintJSONDocument(AIResponseNode,
                                  TraceJSONDocumentFn,
                                  &Canvas);
                TPRINT(("%s",Canvas.GetBuffer()));
                DeleteJSON_mt(AIResponseNode);
                return NULL;
                }
        // -----------------------
        // Build Sampling Response
        // -----------------------
        MCPInputRequest *MCPRequest = MCPSampleJSONBuilder(AIModel,
                                                           Content,
                                                           Content_Length,
                                                           false);
        DeleteJSON_mt(AIResponseNode);
        return MCPRequest;
        }

// ***************************************************************************
// **** MCPSyncSamplerEx *****************************************************
// ***************************************************************************

MCPSyncSamplerEx::MCPSyncSamplerEx(JSON_Value &inId)
        :
        SyncSampler(inId),
        SyncRoundTrip(inId)
        {
        set_Id(inId);
        ExternPriority = ExternPriorityDEFAULT;
        MRTRSupported = false;
        return;
        }

MCPSyncSamplerEx::~MCPSyncSamplerEx()
        {
        static const char *ProcName = "MCPSyncSamplerEx::~MCPSyncSamplerEx";
        if (SyncACPSampler.isOpen())
                {
                TDEBUG(("%s: EndSampling() was not called",ProcName));
                }
        EndSampling();                  // Runs here if the caller didn't.
        return;
        }

void MCPSyncSamplerEx::EndSampling()
        {
        static const char *ProcName = "MCPSyncSamplerEx::EndSampling";
        // ----------------------------------------------------------
        // ---- EndSampling -----------------------------------------
        // -----------------
        //
        // Cleanup resources needed by samplers (presently only ACP).
        //
        // Called by the MCPSyncSamplerEx instance destructor in
        // any case; but ACP, for example issues BlockingClose() to
        // close the session -- it's better if it happens in the 
        // thread doing sampling rather than in the MCP message loop.
        // 
        // ----------------------------------------------------------
        if (SyncACPSampler.isOpen())
                {
                TDEBUG(("%s: Closing ACP Session on: %s",
                        ProcName,
                        SyncACPSampler.get_AgentName()));
                SyncACPSampler.BlockingClose();
                }
        return;
        }

void MCPSyncSamplerEx::set_Id(JSON_Value &inId)
        {
        if (inId.isString())
                {
                sId = (JSON_Value_String &)inId;
                Id = &sId;
                }
        else if (inId.isNumber())
                {
                nId = (JSON_Value_Number &)inId;
                Id = &nId;
                }
        else Id = &zId;
        return;
        }
                  
MCPInputRequest* MCPSyncSamplerEx::BlockingSample(const char *SystemMessage,
                                                  const char *UserMessage,
                                                  int MaxTokens,
                                                  unsigned TimeoutMS)
        {
        static const char *ProcName = "MCPSyncSamplerEx::BlockingSample";
        MCPInputRequest *MCPRequest = NULL;
        // ---------------------------
        // ---- Determine Support ----
        // ---------------------------
        bool ExSupported = isExternSamplingSupported();
        // ---- Internal = MRTR (2026-07-28) or classic sampling. ----
        bool InSupported = MRTRSupported || isSamplingSupported();
        const char *ACPCwd = get_ACPWorkspace(ACPWorkspace.GetString());
        bool ACPSupported = isACPSamplingSupported();
        if (ACPSupported)
                {
                if (!ACPCwd)
                        {
                        TERROR(("%s: ACP Sampling requires a workspace "
                                "or MCPSample_ACP_CWD to be set",
                                ProcName));
                        ACPSupported = false;
                        }
                }
        // ---------------
        // ---- Route ----
        // ---------------
        bool ExternAvailable = ExSupported || ACPSupported;
        if (InSupported && (!ExternPriority || !ExternAvailable))
                {
                if (MRTRSupported)
                        {
                        TINFO(("%s: Using MRTR Client Sampling",ProcName));
                        MCPRequest = SyncRoundTrip.BlockingRequest(
                                                        TimeoutMS,
                                                        SystemMessage,
                                                        UserMessage,
                                                        MaxTokens);
                        if (MCPRequest)
                                {
                                JSON_Value &resumeId = 
                                        SyncRoundTrip.get_resumeId();
                                TDEBUG(("%s: Adopting Resume Id: %s",
                                        ProcName,
                                        id2string(resumeId)));
                                set_Id(resumeId);
                                }
                        }
                else    {
                        TINFO(("%s: Using MCP Client Sampling",ProcName));
                        MCPRequest = SyncSampler.BlockingSample(SystemMessage,
                                                                UserMessage,
                                                                MaxTokens,
                                                                TimeoutMS);
                        }
                }
        else if (ACPSupported) do
                {
                bool Status;
                TINFO(("%s: Using ACP Sampling",ProcName));
                // -----------------
                // Start ACP Sampler
                // -----------------
                if (!SyncACPSampler.isOpen())
                        {
                        Status = SyncACPSampler.BlockingOpen(ACPCwd);
                        if (!Status)
                                {
                                TERROR(("%s: BlockingOpen() Failed: %s",
                                        ProcName,
                                        SyncACPSampler.get_ErrorReason()));
                                break;
                                }
                        TINFO(("%s: Agent: %s/%s",
                               ProcName,
                               SyncACPSampler.get_AgentName(),
                               SyncACPSampler.get_AgentVersion()));
                        }
                // ------
                // Sample
                // ------
                const char *Reply = SyncACPSampler.BlockingSample(SystemMessage,
                                                                  UserMessage,
                                                                  TimeoutMS);
                Status = (Reply != NULL);
                if (!Status)
                        {
                        TERROR(("%s: BlockingSample() Failed: %s",
                                ProcName,
                                SyncACPSampler.get_ErrorReason()));
                        break;
                        }
                char *EscReply = EscapeJSONString(Reply);
                if (!EscReply)
                        {
                        TERROR(("%s: EscapeJSONString() Failed",ProcName));
                        break;
                        }
                MCPRequest = MCPSampleJSONBuilder(SyncACPSampler.get_AgentName(),
                                                  EscReply,
                                                  strlen(EscReply),
                                                  true);

                FreeEscapeJSONString(&EscReply);
                } while(0);
        else if (ExSupported) 
                {
                TINFO(("%s: Using External AI Sampling",ProcName));
                const char *URLEndpoint = getenv("MCPSample_AI_URL");
                const char *AIModel = getenv("MCPSample_AI_MODEL");
                const char *APIKey = getenv("MCPSample_AI_KEY");
                if (MCPJSONTrace)
                        {
                        TPRINT(("       --------------------------"));
                        TINFO(("%s: Tracing Output:",ProcName));
                        char *DummyReq = FormMCPRequest_sampling(
                                                get_resumeId(),
                                                SystemMessage,
                                                UserMessage,
                                                MaxTokens);
                        if (DummyReq)
                                {
                                TraceJSON(DummyReq);
                                free(DummyReq);
                                }
                        TPRINT(("       --------------------------"));
                        }
                char *AIResponse = cURL_AI(cURL,
                                           URLEndpoint,
                                           AIModel,
                                           APIKey,
                                           SystemMessage,
                                           UserMessage);
                if (!AIResponse)
                        {
                        TERROR(("%s: cURL_AI(\"%.64s\") Failed",
                                ProcName,
                                UserMessage));
                        }
                else    {
                        MCPRequest = MCPSampleEmulator(URLEndpoint,
                                                       AIModel,
                                                       AIResponse);
                        cURL_FreeResponseBuffer(&AIResponse);
                        }
                }
        else    {
                TERROR(("%s: Sampling Requested but NONE Supported",ProcName));
                }
        return MCPRequest;
        }

bool MCPSyncSamplerEx::CancelWait()
        {
        bool a = cURL.Cancel();
        bool b = SyncSampler.CancelWait();
        bool c = SyncRoundTrip.CancelWait();
        // ----------------------------------------
        // ---- ACP Cancel ------------------------
        // ---------------
        // d: Attempt to cancel work (asynchronous)
        // e: Abort our wait
        // ----------------------------------------
        bool d = SyncACPSampler.CancelPrompt();
        bool e = SyncACPSampler.CancelWait();
        return a || b || c || d || e;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
