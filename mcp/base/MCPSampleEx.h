// MCP Server Hybrid Blocking Sampler : MCPSampleEx.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPSampleEx.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Environment for External AI Sampling: MCPSample_AI_URL
//                                            MCPSample_AI_MODEL
//                                            MCPSample_AI_KEY
//                                       and: MCPSample_EXTERN_PRIORITY
//      Environment for ACP Sampling: MCPSample_ACP_CWD - Workspace
//                                    MCPSample_ACP_CMD - Command
//      

#ifndef __MCPExternalSample_H

        #define __MCPExternalSample_H

#include <MCPSampleCore.h>
#include <MCPMultiTripCore.h>
#include <AIcURL.h>
#include <ACPSyncSampler.h>

// *******************************
// **** Sampling Support Checks **
// *******************************
//
//      isSamplingSupported()               native MCP (MCPSampleCore.h)
//      isExternSamplingSupported()         external AI (env + cURL)
//      isMRTRSamplingSupported(MCPRequest) 2026-07-28 _meta capabilities
//      isACPSamplingSupported()            local agent (ACPSyncSampler.h)
//
//      isSamplingExSupported(MCPRequest)   any of the four paths

bool isExternSamplingSupported();
bool isMRTRSamplingSupported(MCPInputRequest &MCPRequest);

bool isSamplingExSupported(MCPInputRequest &MCPRequest);

// ********************************************************
// **** MRTR Blocking Sampler : MCPSyncMRTRSampler ********
// ********************************************************

class MCPSyncMRTRSampler : public MCPSyncRoundTrip
        {
        public:
                MCPSyncMRTRSampler(JSON_Value &inId)
                        :
                        MCPSyncRoundTrip(inId)
                        {
                        return;
                        }
        protected:
                virtual char* FormInputRequest(JSON_Value &inId,
                                               JSON_Value_String &requestState,
                                               va_list Params)
                        {
                        const char *SystemPrompt = va_arg(Params,const char *);
                        const char *UserMessage = va_arg(Params,const char *);
                        int MaxTokens = va_arg(Params,int);
                        return FormMCPRequest_sampling(inId,
                                                       requestState,
                                                       SystemPrompt,
                                                       UserMessage,
                                                       MaxTokens);
                        }
        };

// ****************************************************
// **** Hybrid Blocking Sampler : MCPSyncSamplerEx ****
// ****************************************************

class MCPSyncSamplerEx
        {
        private:
                bool ExternPriority;
                bool MRTRSupported;     // Caller sets via SetMRTRSupported.
                AutoString ACPWorkspace;                
                // -----------------------------------------------
                // Id local storage (mimics MCPSyncRoundTrip): the
                // MRTR path adopts the resume id in BlockingSample.
                // -----------------------------------------------
                JSON_Value *Id;         // Points to sId or nId or zId.
                JSON_Value_String sId;
                JSON_Value_Number nId;
                JSON_Value_Null zId;
                void set_Id(JSON_Value &inId);
        protected:
                cURLRunner cURL;
                MCPSyncSampler SyncSampler;
                MCPSyncMRTRSampler SyncRoundTrip;
                ACPSyncSampler SyncACPSampler;
        public:
                MCPSyncSamplerEx(JSON_Value &inId);
                ~MCPSyncSamplerEx();
                // ------------------------
                // ---- BlockingSample ----
                // ----------------------------------------------------
                // Output: Copy of MCPClient response (caller deletes).
                //         Answer the client with get_resumeId() -- it
                //         adopts the resume id when the MRTR path runs.
                // ----------------------------------------------------
                MCPInputRequest* BlockingSample(const char *SystemPrompt,
                                                const char *UserMessage,
                                                int MaxTokens = 1024,
                                                unsigned TimeoutMS=0);
                bool CancelWait();
                void EndSampling();     // Nice: Shutdown in the worker.                
                JSON_Value& get_resumeId() {return *Id;}
                void SetExternPriority(bool ExP) {ExternPriority = ExP;}
                void SetMRTRSupported(bool MS) {MRTRSupported = MS;}
                // -----------------------
                // ---- ACP Workspace ----
                // ---------------------------------------------------
                // When using ACP, the ACP agent requires a workspace.
                // Set to "" to fall back to MCPSample_ACP_CWD.
                // ---------------------------------------------------
                void SetWorkspace(const char *Cwd) {ACPWorkspace = Cwd;}
        };

// *****************************************************
// **** Stack Scope Early Cleanup: SamplerEnder ********
// *********************************************
// Helper to automatically EndSampling on stack exit.
//
// Functions as an "early MCPSyncSamplerEx destructor"
// so EndSampling() runs in the thread which is using it. 
// In the ACP case, it performs a BlockingClose().
//
// Declare at the top of a thread main or BeginHandler() 
//
//      SamplerExEnder EndSamplingExit(Sampler);
//
// to avoid having to manually call EndSampling() on
// every return path.
//
// Note: Calling EndSampling() explicitly or via 
//       SamplerExEnder is optional. 
// *****************************************************

struct SamplerExEnder
        {
        MCPSyncSamplerEx &Sampler;
        SamplerExEnder(MCPSyncSamplerEx &_Sampler) : Sampler(_Sampler) {}
        ~SamplerExEnder() {Sampler.EndSampling();}
        };

/* ***************************************************************************
 Notes:

     Transparent Hybrid Sampling
     ----------------------------
     MCPSyncSamplerEx composes four samplers behind one BlockingSample()
     call and routes to whichever path is available. Answer the client
     with get_resumeId(), and read the reply with ExtractSampleText():
     
       - If the MCP client advertises sampling support the request routes 
         through the standard MCP sampling/createMessage protocol via 
         MCPSyncSampler, or the MRTR round trip for 2026-07-28 clients
         (SetMRTRSupported(isMRTRSamplingSupported(MCPRequest))).
     
       - Otherwise, if MCPSample_ACP_CMD names a local ACP agent, the
         request routes to it over ACP. ACP has no system role, so the
         system prompt is prepended to the user message. The agent
         needs a workspace: SetWorkspace(), else MCPSample_ACP_CWD.
     
       - Otherwise, if external AI environment variables are configured,
         the request routes to a cURL call against the configured AI 
         provider. The raw provider response is converted to MCP sampling 
         response so the caller sees the same MCPInputRequest* either way.
     
       - If no path is available, BlockingSample() returns NULL.
     
     MCPSample_EXTERN_PRIORITY steps the client aside for an external
     engine (ACP or cURL). It is a priority and not a prohibition: with
     no external engine configured, the client still answers.
     
     Environment Variables (External AI path):
     
       MCPSample_AI_URL    - Provider endpoint (OpenAI, Google, Anthropic)
       MCPSample_AI_MODEL  - Model identifier (e.g. gpt-4o, gemini-2.0-flash)
       MCPSample_AI_KEY    - API key for the provider
     
     Environment Variables (ACP path):
     
       MCPSample_ACP_CMD   - Command line that starts the local ACP agent
       MCPSample_ACP_CWD   - Workspace, when SetWorkspace() was not called
     
     Environment Variables (Routing):
     
       MCPSample_EXTERN_PRIORITY - 1/T/t/Y/y = Favor external sampling.
                                   Applies to both ACP and cURL.
     
     Use isSamplingExSupported(MCPRequest) to verify the hybrid sampler is
     likely to produce a response -- returns true if MCP client sampling,
     the MRTR path, the ACP path, or the external AI path is available.
     Only "likely": it is a free function with no sampler to ask, so it
     cannot know whether the caller will supply an ACP workspace.
     
     CancelWait:
     
       CancelWait() cancels all four paths: aborts any in-flight cURL
       request, unblocks a pending classic or MRTR wait, and both asks
       the ACP agent to stop the turn and abandons the wait for it.
       Returns true if any cancellation succeeded.
     
******************************************************************************
**** Example: ****************************************************************
******************************************************************************

        MCPSyncSamplerEx Sampler(idRequest);
        Sampler.SetMRTRSupported(isMRTRSamplingSupported(MCPRequest));

        MCPInputRequest *Response = Sampler.BlockingSample(
                "You are a spell checker.",
                UserText,
                200);
        if (!Response)
                return;   // cancelled or no sampling available

        const char *Corrected = ExtractSampleText(*Response);
        // ... build the tool result, answering Sampler.get_resumeId() ...
        delete Response;

*****************************************************************************/

#endif /* __MCPExternalSample_H */
