// ACP Blocking Sampler: ACPSyncSampler.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ACPSyncSampler.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//
// Note: Lock order: ACPAgentMutex -> CancelMutex.
//

#include <stdlib.h>
#include <string.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <DirList.h>

#include <ACPSyncSampler.h>

static bool ACPJSONTraceDEFAULT = false;        // Trace at Initialization. 

// ***********************************
// **** isExternSamplingSupported ****
// ***********************************

bool isACPSamplingSupported()
        {
        static bool ACPSupportReported = false;
        const char *Command = getenv("MCPSample_ACP_CMD");
        bool Supported = Command && *Command;
        if (!ACPSupportReported)
                {
                ACPSupportReported = true;
                if (!Supported)
                        {
                        TINFO(("ACPSampling: ACP Sampling NOT Supported"));
                        TINFO(("ACPSampling: MCPSample_ACP_CMD not set"));
                        }
                else    {
                        TINFO(("ACPSampling: ACP Sampling Supported"));
                        if (!getenv("MCPSample_ACP_CWD"))
                                {
                                TINFO(("ACPSampling: "
                                       "Optional MCPSample_ACP_CWD not set"));
                                }
                        }
                }
        return Supported;
        }

// ***************************************************************************
// **** Global Shared Agent **************************************************
// **************************
//
// One ACP Agent per process.
// An ACPSession per ACPSyncSampler.
//
// ACPAgentMutex guards BUILDING the Agent and nothing else. 
// Once built, ACPAgent must remain in the process until the end.
//
//      Rule: No ACPSyncSampler may outlive the Agent.
//
// Lifecycle:
//
//      InitSampleACP() -- On-the-fly at first request.
//      DeInitSampleACP() -- Process end, or when no sampler exists.
//
// ***************************************************************************

// *******************
// ***** ACPAgent ****
// *******************

static Mutex ACPAgentMutex;
static ACPClientThread *ACPAgent = NULL;

static AutoString ACPAgentName;         // Cached: safely written under lock.
static AutoString ACPAgentVersion;

static const char* get_AgentName()
        {
        const char *AgentName = ACPAgentName.GetString();
        if (*AgentName) return AgentName;
        ACPAgentMutex.Take();
        if (ACPAgent)
                {
                JSON_Object &Info = ACPAgent->get_AgentInfo();
                const char *InfoName = Info.Find_member_string("name");
                if (InfoName) ACPAgentName = InfoName;
                AgentName = ACPAgentName.GetString();
                }
        ACPAgentMutex.Release();
        return AgentName; 
        }

static const char* get_AgentVersion()
        {
        const char *AgentVersion = ACPAgentVersion.GetString();
        if (*AgentVersion) return AgentVersion;
        ACPAgentMutex.Take();
        if (ACPAgent)
                {
                JSON_Object &Info = ACPAgent->get_AgentInfo();
                const char *InfoVersion = Info.Find_member_string("version");
                if (InfoVersion) ACPAgentVersion = InfoVersion;
                AgentVersion = ACPAgentVersion.GetString();
                }
        ACPAgentMutex.Release();
        return AgentVersion; 
        }

static const char* getACPCMD(AutoString &asCommand)
        {
        static const char *ProcName = "getACPCMD";
        const char *MCPSample_ACP_CMD = getenv("MCPSample_ACP_CMD");
        if (!MCPSample_ACP_CMD) return NULL;
        DirectoryList Resolver;
        Resolver.AddPath(MCPSample_ACP_CMD,DL_Before);
        DirectoryItem *Resolved = Resolver.GetFirst();
        if (!Resolved)
                {
                TERROR(("Can't resolve Path: %s",MCPSample_ACP_CMD));
                asCommand = MCPSample_ACP_CMD;
                }
        else asCommand = Resolved->GetDirectory();
        return asCommand.GetString();
        }

static const char* getACPCWD(AutoString &asWorkspace, const char *Cwd)
        {
        static const char *ProcName = "getACPCWD";
        if (!Cwd) return NULL;
        DirectoryList Resolver;
        Resolver.AddPath(Cwd,DL_Before);
        DirectoryItem *Resolved = Resolver.GetFirst();
        if (!Resolved)
                {
                TERROR(("Can't resolve Path: %s",Cwd));
                asWorkspace = Cwd;
                }
        else asWorkspace = Resolved->GetDirectory();
        return asWorkspace.GetString();
        }

// ****************************
// **** JSON Trace Control ****
// ****************************

void setACPJSONTrace(bool TraceOn)
        {
        ACPJSONTraceDEFAULT = TraceOn; // Trace at On-Demand Initialization.
        ACPAgentMutex.Take();
        if (ACPAgent)
                {
                ACPAgent->JSONTron(TraceOn);    // Dynamic trace.
                }
        ACPAgentMutex.Release();
        return;
        }

// ************************************
// ***** InitSampleACP (On-Demand) ****
// ************************************

bool InitSampleACP(ACPSyncSampler &Sampler, unsigned TimeoutMS)
        {
        static const char *ProcName = "InitSampleACP";
        AutoString asCommand;
        ACPAgentMutex.Take();
        bool Status = (ACPAgent != NULL);       // Somebody got here first.
        do      {
                if (Status) break;
                const char *Command = getACPCMD(asCommand);
                if (!Command || !*Command)
                        {
                        TERROR(("%s: MCPSample_ACP_CMD is not set",ProcName));
                        break;
                        }
                // -------------------
                // Start Agent Process
                // -------------------
                ACPClientThread *Agent = new ACPClientThread(Command);
                if (!Agent)
                        {
                        TERROR(("%s: Memory Error - no Agent",ProcName));
                        break;
                        }
                Status = Agent->Start();
                if (!Status)
                        {
                        TERROR(("%s: Can't start: %s",ProcName,Command));
                        delete Agent;
                        break;
                        }
                Agent->JSONTron(ACPJSONTraceDEFAULT);
                // ---------------
                // Agent Handshake
                // ---------------
                ACPInitRequester InitRequester(*Agent);
                bool wasCanceled = Sampler.setCancelMonitor(&InitRequester);
                if (wasCanceled)
                        {
                        TERROR(("%s: Canceled via Sampler",ProcName));
                        delete Agent;           // End dormant server.
                        break;
                        }
                Status = InitRequester.BlockingInitialize(TimeoutMS);
                wasCanceled =Sampler.setCancelMonitor(NULL);
                if (!Status)
                        {
                        if (wasCanceled)
                                {
                                TERROR(("%s: Canceled via Sampler",ProcName));
                                }
                        TERROR(("%s: BlockingInitialize() Failed",ProcName));
                        delete Agent;           // End dormant server.
                        break;
                        }
                ACPAgent = Agent;                       // Ready to use!
                TINFO(("%s: Agent is up: %s",ProcName,get_AgentName()));
                } while (0);
        ACPAgentMutex.Release();
        return Status;
        }

// **************************
// ***** DeInitSampleACP ****
// **************************

void DeInitSampleACP()  // Safe to call when all ACPSyncSampler are gone. 
        {
        static const char *ProcName = "DeInitSampleACP";
        AutoString asAgentName = get_AgentName();       // Copy: it is wiped.
        bool StopStatus = true;
        ACPAgentMutex.Take();
        if (ACPAgent)
                {
                StopStatus = ACPAgent->Stop(true);
                delete ACPAgent;
                ACPAgent = NULL;
                ACPAgentName = "";
                ACPAgentVersion = "";
                }
        ACPAgentMutex.Release();
        const char *AgentName = asAgentName.GetString();
        if (StopStatus && *AgentName)
                {
                TINFO(("%s: Agent Ended: %s",ProcName,AgentName));
                }
        else if (!StopStatus)
                {
                TERROR(("%s: Agent Stop() Failed: %s",ProcName,AgentName));
                }
        return;
        }

// ***************************************************************************
// **** ACPSyncSampler *******************************************************
// ***************************************************************************

ACPSyncSampler::ACPSyncSampler()
        {
        Session = NULL;
        ActiveRequester = NULL;
        Canceled = false;
        return;
        }

ACPSyncSampler::~ACPSyncSampler() 
        {
        setCancelMonitor(NULL);
        BlockingClose(ACPTimeoutMS);
        return;
        }

bool ACPSyncSampler::isCanceled() 
        {
        CancelMutex.Take();
        bool wasCanceled = Canceled;
        CancelMutex.Release();
        return wasCanceled;
        }

bool ACPSyncSampler::isOpen()
        {
        SessionMutex.Take();
        bool Open = (Session != NULL);
        SessionMutex.Release();
        return Open;
        }

bool ACPSyncSampler::CancelPrompt()
        {
        SessionMutex.Take();
        bool Status = Session ? Session->CancelPrompt() : false;
        SessionMutex.Release();
        return Status;
        }

bool ACPSyncSampler::CancelWait()
        {
        static const char *ProcName = "ACPSyncSampler::CancelWait";
        const char *AgentName = get_AgentName(); // Fetch before CancelMutex.
        bool rc = false;
        CancelMutex.Take();
        if (ActiveRequester) 
                {
                TINFO(("%s: Cancel Issued: %s",ProcName,AgentName));
                rc = ActiveRequester->CancelWait();
                }
        else    {
                TINFO(("%s: Nothing Active: %s",ProcName,AgentName));
                }
        Canceled = true;
        CancelMutex.Release();
        return rc;
        }

void ACPSyncSampler::BlockingClose(unsigned TimeoutMS)
        {
        static const char *ProcName = "ACPSyncSampler::BlockingClose";
        // ------------------------------------------------
        // Unhooked first, so CancelPrompt() cannot see it.
        // ------------------------------------------------
        SessionMutex.Take();
        ACPSessionSample *Old = Session;
        Session = NULL;
        SessionMutex.Release();
        if (!Old) return;
        TINFO(("%s: Ending Sampler Session: %s",ProcName,get_AgentName()));
        ACPSessionCloser SessionCloser(*Old);
        SessionCloser.BlockingClose(TimeoutMS);
        delete Old;
        return;
        }

bool ACPSyncSampler::BlockingOpen(const char *Cwd, unsigned TimeoutMS)
        {
        static const char *ProcName = "ACPSyncSampler::BlockingOpen";
        ErrorReason = "";
        if (isOpen())
                {
                ErrorReason = "already_open";
                TERROR(("%s: Session is already open",ProcName));
                return false;
                }
        // ------------------------------------------------------
        // A current working directory is required. Resolve path.
        // ------------------------------------------------------
        AutoString asWorkspace;
        if (!Cwd || !*Cwd)
                {
                ErrorReason = "no_workspace";
                TERROR(("%s: No Workspace (Cwd) given",ProcName));
                return false;
                }
        const char *workspaceDir = getACPCWD(asWorkspace,Cwd);
        if (workspaceDir) Cwd = workspaceDir;
        TDEBUG(("%s: ACP Workspace: %s",ProcName,workspaceDir));
        // ---------------------
        // ---- Start Agent ----
        // ---------------------
        if (!InitSampleACP(*this,TimeoutMS))
                {
                // -----------------------------------------------
                // Agent build is a blocking, cancelable operation
                // -----------------------------------------------
                ErrorReason = isCanceled() ? "wait_cancelled" : "no_agent";
                TERROR(("%s: InitSampleACP() Failed",ProcName));
                return false;
                }
        if (!ACPAgent)
                {
                // -----------------------------------------------
                // Should never happen: DeInitSampleACP() may only
                // be called at process-end when no ACPSyncSampler 
                // could exist. 
                // -----------------------------------------------
                ErrorReason = "no_agent";
                TERROR(("%s: The Agent went away",ProcName));
                return false;
                }
        ACPClientThread &Agent = *ACPAgent; // Reference is now safe.
        // ---------------------------
        // ---- Establish Session ----
        // ---------------------------
        ACPSessionSample *NewSession = new ACPSessionSample(Agent);
        if (!NewSession)
                {
                ErrorReason = "no_memory";
                TERROR(("%s: Memory Error - no Session",ProcName));
                return false;
                }
        ACPSessionRequester SessionRequester(*NewSession,
                                             Cwd,
                                             NULL);
        bool wasCanceled = setCancelMonitor(&SessionRequester);
        if (wasCanceled)
                {
                ErrorReason = "wait_cancelled";
                TERROR(("%s: BlockingNewSession Canceled",ProcName));
                delete NewSession;      // Nothing to close: it never opened.
                return false;
                }
        bool Status = SessionRequester.BlockingNewSession(TimeoutMS);
        wasCanceled = setCancelMonitor(NULL);
        if (!Status)
                {
                ErrorReason = wasCanceled ? "wait_cancelled" : "no_session";
                TERROR(("%s: BlockingNewSession() Failed",ProcName));
                delete NewSession;      // Nothing to close: it never opened.
                return false;
                }
        TINFO(("%s: Sampler Session Started: %s",ProcName,get_AgentName()));
        SessionMutex.Take();
        Session = NewSession;
        SessionMutex.Release();
        return true;
        }

const char* ACPSyncSampler::BlockingSample(const char *SystemPrompt,
                                           const char *UserMessage,
                                           unsigned TimeoutMS)
        {
        static const char *ProcName = "ACPSyncSampler::BlockingSample";
        ErrorReason = "";
        if (!Session)
                {
                ErrorReason = "not_open";
                TERROR(("%s: No Session -- BlockingOpen() first",ProcName));
                return NULL;
                }
        // -----------------------------------------------------------------
        // Merge system and user prompt because session/prompt has no roles.
        // -----------------------------------------------------------------
        MemoryPrintf Prompt;
        if (SystemPrompt && *SystemPrompt)
                {
                Prompt.printf("%s\n\n",SystemPrompt);
                }
        Prompt.printf("%s",UserMessage ? UserMessage : "");
        const char *PromptText = Prompt.GetBuffer();
        if (!PromptText)
                {
                ErrorReason = "no_memory";
                TERROR(("%s: Memory Error - no PromptText",ProcName));
                return NULL;
                }
        // -------------------------------------
        // Session accumulator cleared per turn.
        // -------------------------------------
        ACPPromptRequester PromptRequester(*Session);
        // -----------------------
        // Begin ACP agent work...
        // -----------------------
        bool wasCanceled = setCancelMonitor(&PromptRequester);
        if (wasCanceled)
                {
                ErrorReason = "wait_cancelled";
                TERROR(("%s: PromptRequester Canceled",ProcName));
                return NULL;
                }
        Session->clear_Reply();
        bool Status = PromptRequester.BlockingPrompt(PromptText,TimeoutMS);
        wasCanceled = setCancelMonitor(NULL);
        if (!Status)
                {
                ErrorReason = PromptRequester.get_ErrorReason();
                TERROR(("%s: BlockingPrompt() Failed: %s",ProcName,
                                                          get_ErrorReason()));
                return NULL;
                }
        return Session->get_Reply();
        }

bool ACPSyncSampler::setCancelMonitor(RPCSyncRequester *Active)
        {
        // ------------------------------------------------------
        // setCancelMonitor Use Protocol:
        // 1. Caller sets the Active requester before operation.
        // 2. If the Sampler 'wasCanceled' the caller must abort.
        // ------------------------------------------------------
        CancelMutex.Take();
        bool wasCanceled = Canceled;
        ActiveRequester = wasCanceled ? NULL : Active;
        CancelMutex.Release();
        return wasCanceled;
        }

const char* ACPSyncSampler::get_ErrorReason()
        {
        const char *Reason = ErrorReason.GetString();
        return Reason ? Reason : "";
        }

const char* ACPSyncSampler::get_AgentName()
        {
        return ::get_AgentName();
        }

const char* ACPSyncSampler::get_AgentVersion()
        {
        return ::get_AgentVersion();
        }

// ***************************************************************************
// ******************************* End of File *******************************
// ***************************************************************************
