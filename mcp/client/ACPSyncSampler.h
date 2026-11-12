// ACP Blocking Sampler: ACPSyncSampler.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ACPSyncSampler.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Environment: MCPSample_ACP_CMD -- Agent executable to spawn.
//

#ifndef __ACPSyncSampler_H

        #define __ACPSyncSampler_H

#include <ASThread.h>
#include <AutoString.h>

#include <ACPClient.h>

#define ACPTimeoutMS 20000                      // Default ACP TimeoutMS.

bool isACPSamplingSupported();

void setACPJSONTrace(bool TraceOn);             // Dynamic tracing control.

// ***************************************************************************
// **** ACP Sampler Lifecycle ************************************************
// ***************************************************************************
//
// The Agent is started on the fly, at the first BlockingOpen(), and
// is shared: spawning it costs seconds and a sample should not.
//
// It is held by reference for the whole life of a session, so no lock
// can protect it and none tries. The rule is a lifetime rule:
//
//      No ACPSyncSampler may outlive the Agent.
//
//      DeInitSampleACP() -- At process end, or when it is certain no
//                           ACPSyncSampler exists.
//
// ***************************************************************************

void DeInitSampleACP();

// ***************************************************************************
// **** ACPSyncSampler *******************************************************
// ********************
//
// ACPSyncSampler sampler shares a ACP agent.
// Each ACPSyncSampler owns a session.
//
// Only one thread my use ACPSyncSampler at a time, and the following
// sequence must be invoked in the same thread:
//
//      Open Session: BlockingOpen() -- Call once (many times ok).
//      Sample: BlockingSample() -- Call n-times.
//              Returns: Unescaped ACP agent answer.
//                       NULL: Use get_ErrorReason() for reason.
//
//              NOTICE: Return string lives in the 'Session' and 
//                      is valid until the next method call. 
//
//      Close Session: BlockingClose() -- Call once (many times ok).
//
// CancelPrompt() may be called in a different thread anytime.
//
// ***************************************************************************

class ACPSyncSampler
        {
        private:
                AutoString ErrorReason;
                Mutex SessionMutex;
                ACPSessionSample *Session;
                // --------------
                // Cancel Monitor
                // --------------
                Mutex CancelMutex;
                bool Canceled;
                RPCSyncRequester *ActiveRequester;
                bool setCancelMonitor(RPCSyncRequester *Active);
                friend bool InitSampleACP(ACPSyncSampler&,unsigned);
                // -----------------------
                // Knock out -- no copies.
                // -----------------------
                ACPSyncSampler(ACPSyncSampler &Source);
                ACPSyncSampler& operator =(ACPSyncSampler &Source);
        public:
                ACPSyncSampler();
                ~ACPSyncSampler();
                bool isCanceled();
                bool isOpen();
                // ---------------------
                // ---- ACP Actions ----
                // ---------------------
                bool BlockingOpen(const char *Cwd,
                                  unsigned TimeoutMS = ACPTimeoutMS);
                void BlockingClose(unsigned TimeoutMS = ACPTimeoutMS);
                const char* BlockingSample(const char *SystemPrompt,
                                           const char *UserMessage,
                                           unsigned TimeoutMS = ACPTimeoutMS);
                bool CancelPrompt();
                bool CancelWait();
                // --------------                
                // ---- Info ----
                // --------------
                const char* get_ErrorReason();
                const char* get_AgentName();
                const char* get_AgentVersion();
        };

/* **************************************************************************
 ****************************************************************************

 Notes:

 ****************************************************************************
 **** Example ***************************************************************
 *************

 ****************************************************************************

static const char *MCPSample_ACP_CMD_Env =
        // -----------------------------------------------------------
        // Note: 'bob' is an npm shim: bob.cmd -> cmd.exe -> node.exe. 
        //       Spawning node directly so popenNT_getpid() is the 
        //       agent and pabortNT() can terminate it.
        // -----------------------------------------------------------
        "MCPSample_ACP_CMD="
        "node \"%APPDATA%\\npm\\node_modules\\@ibm\\bob\\dist\\bob.js\" "
        "acp --trust";

static const char *ACP_Workspace = "C:\\Temp\\Bob";

bool ACPSamplerTest()
        {
        static const char *ProcName = "ACPSamplerTest";
        bool Status = false;
        do      {
                if (_putenv(MCPSample_ACP_CMD_Env))
                        {
                        TERROR(("%s: _putenv() Failed",ProcName));
                        break;
                        }
                // -----------------
                // Start ACP Sampler
                // -----------------
                ACPSyncSampler Sampler;
                Status = Sampler.BlockingOpen(ACP_Workspace);
                if (!Status)
                        {
                        TERROR(("%s: BlockingOpen() Failed: %s",
                                ProcName,
                                Sampler.get_ErrorReason()));
                        break;
                        }
                TINFO(("%s: Agent: %s/%s",ProcName,
                                          Sampler.get_AgentName(),
                                          Sampler.get_AgentVersion()));
                // ------------
                // A first turn
                // ------------
                const char *Reply = Sampler.BlockingSample(
                        "You are a calculator. Answer with only the number.",
                        "What is 17 multiplied by 23?");
                Status = (Reply != NULL);
                if (!Status)
                        {
                        TERROR(("%s: BlockingSample() Failed: %s",
                                ProcName,
                                Sampler.get_ErrorReason()));
                        break;
                        }
                TPRINT(("---- Sampled ---------------------------"));
                TPRINT(("%s",Reply));
                TPRINT(("----------------------------------------"));
                // ---------------------------------
                // A second turn on the same session
                // ---------------------------------
                Reply = Sampler.BlockingSample(NULL,"And add 9 to that?");
                Status = (Reply != NULL);
                if (!Status)
                        {
                        TERROR(("%s: Second BlockingSample() Failed: %s",
                                ProcName,
                                Sampler.get_ErrorReason()));
                        break;
                        }
                TPRINT(("---- Sampled Again ---------------------"));
                TPRINT(("%s",Reply));
                TPRINT(("----------------------------------------"));
                // ---------------
                // End ACP Sampler
                // ---------------
                Sampler.BlockingClose();
                } while (0);
        return Status;
        }

int main(int argc, const char *argv[])
        {
        static const char *ProcName = "main";
        // ********************
        // **** Initialize ****
        // ********************
        SetMaxTraceLevel(TRACELEVEL_DEBUG);
        InitJSONParser();
        // ***********************
        // **** ACPClient Run ****
        // ***********************
        bool Status = false;
        do      {
                Status = ACPSamplerTest();
                if (!Status)
                        {
                        TERROR(("%s: ACPSamplerTest() Failed",ProcName));
                        break;
                        }
                } while (0);
        // *******************
        // **** Terminate ****
        // *******************
        DeInitSampleACP();      // Every sampler is gone by here.
        DeinitJSONParser();
        return Status ? 0 : 1;
        }

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __ACPSyncSampler_H */
