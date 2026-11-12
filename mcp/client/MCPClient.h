// MCPClient: MCPClient.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPClient.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __MCP_Client_Main

        #define __MCP_Client_Main

#include <AutoString.h>
#include <RPCClient.h>
#include <MCPClientJSON.h>

// ***************************************************************************
// **** MCPClientThread ******************************************************
// ***************************************************************************

class MCPClientThread : public RPCClientThread
        {
        private:
                int ProtocolVersion;
                JSON_Object ServerInfo;
                JSON_Object Capabilities;
        public:
                MCPClientThread(const char *Command);
                ~MCPClientThread() {Stop(true);}
                int get_ProtocolVersion() {return ProtocolVersion;}
                JSON_Object& get_ServerInfo() {return ServerInfo;}
                JSON_Object& get_Capabilities() {return Capabilities;}
                // -------------------------------------------------------
                // Written by MCPInitRequester, once, from the initialize
                // response. JSON_Object assignment deep copies, so these
                // outlive the response they came from.
                // -------------------------------------------------------
                void set_ProtocolVersion(int Version)
                        {
                        ProtocolVersion = Version;
                        }
                void set_ServerInfo(JSON_Object &Info) {ServerInfo = Info;}
                void set_Capabilities(JSON_Object &Caps)
                        {
                        Capabilities = Caps;
                        }
        };

// ***************************************************************************
// **** MCPInitRequester *****************************************************
// ***********************
//
// MCP opening: initialize, then notifications/initialized.
//
// ***************************************************************************

class MCPInitRequester : public RPCSyncRequester
        {
        private:
                MCPClientThread &MCPClient;     // Reaches connection state.
                int ProtocolVersion;
                AutoString Capabilities;
                AutoString ClientName;
                AutoString ClientVersion;
        protected:
                virtual char* OnRequest(JSON_Value &idRequest);
        public:

                MCPInitRequester(MCPClientThread &Client,
                                 int protocolVersion,
                                 const char *capabilities,
                                 const char *clientName,
                                 const char *clientVersion);
                bool BlockingInitialize(unsigned TimeoutMS=0);
        };

/* **************************************************************************
 ****************************************************************************

 Notes:

        1. MCPInitRequester:
        
                protocolVersion: integer form of a defined date:
                
                        ex: MCPProtocolVersion_20241105 and friends.

                capabilities: spliced verbatim into the request as
                              the *body* of a JSON object, so it must 
                              be valid JSON and carry no outer braces. 
                
                              Set to NULL or "" declares none. A client 
                              declaring none won't be asked to sample.

        2. BlockingInitialize:

                Records what was negotiated in the MCPClientThread.
                then sends notifications/initialized. Returns whether
                the server answered.

                The MCPClientThread contains the negotiated protocol
                version via get_ProtocolVersion().

        Protocol version negotiation: 

                Initialize sends the requested version but the server
                may grant LESS than the client asked for. The negotiated
                version that comes from the server: get_ProtocolVersion().

 ****************************************************************************
 **** Example ***************************************************************
 *************

        Loads the 'MCPServer_helloWorld' built from:

                mcp\servers\helloWorld>MCPServer_helloWorld.cpp
                
        Performs a helloWorld request.

 ****************************************************************************

#include <stdio.h>

#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPClient.h> 

static const char *MCPServerPath = "U:\\MCPServers\\MCPServer_helloWorld.exe";

// ***********************************
// **** MCPClient helloWorld Test ****
// ***********************************

class MCPHelloRequester : public RPCSyncRequester
        {
        private:
                AutoString HelloToWhom;
        protected:
                virtual char* OnRequest(JSON_Value &idRequest);
        public:
                MCPHelloRequester(MCPClientThread &Client) 
                        : 
                        RPCSyncRequester(Client) 
                        {
                        return;        
                        }
                MCPOutputResponse* BlockingHello(const char *ToWhom,
                                                 unsigned TimeoutMS=0)
                        {
                        HelloToWhom = ToWhom;
                        return RPCSyncRequester::BlockingRequest(TimeoutMS);
                        }
        };

static char* FormMCPRequest_helloWorld(JSON_Value &id, const char *ToWhom)
        {
        static const char *ProcName = "FormMCPRequest_helloWorld";
        char *EscToWhom = EscapeJSONString(ToWhom ? ToWhom : "");
        if (!EscToWhom)
                {
                TERROR(("%s: EscapeJSONString() Failed",ProcName));
                return NULL;
                }
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf(" \"jsonrpc\": \"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(id));
        Canvas.printf("\"method\":\"tools/call\",");
        Canvas.printf("\"params\":{");
        Canvas.printf("\"name\":\"helloWorld\",");
        Canvas.printf("\"arguments\":{\"name\":\"%s\"}",EscToWhom);
        Canvas.printf("}");
        Canvas.printf("}");
        char *MCPOutput = Canvas.AquireBuffer();
        FreeEscapeJSONString(&EscToWhom);
        return MCPOutput;
        }

char* MCPHelloRequester::OnRequest(JSON_Value &idRequest)
        {
        return FormMCPRequest_helloWorld(idRequest,HelloToWhom.GetString());
        }

// ****************
// **** main() ****
// ****************

int main(int argc, const char *argv[])
        {
        static const char *ProcName = "main";
        // ********************
        // **** Initialize ****
        // ********************
        SetMaxTraceLevel(TRACELEVEL_DEBUG);
        InitJSONParser();
        // ***********************
        // **** MCPClient Run ****
        // ***********************
        bool Status = true;
        MCPClientThread Client(MCPServerPath);
        do      {
                Status = Client.Start();
                if (!Status)
                        {
                        TERROR(("%s: Can't start: %s",ProcName,MCPServerPath));
                        break;
                        }
                // --------------------------
                // ---- Initalize Server ----
                // --------------------------
                MCPInitRequester InitRequester(
                                        Client,
                                        MCPProtocolVersion_20241105,
                                        "",     // No capabilities declared.
                                        "vscode-copilot",
                                        "1.0.0");
                Status = InitRequester.BlockingInitialize();
                if (!Status)
                        {
                        TERROR(("%s: BlockingInitialize() Failed",ProcName));
                        break;
                        }
                // -----------------------------
                // ---- Say Hello to Server ----
                // -----------------------------
                MCPHelloRequester HelloRequester(Client);
                MCPOutputResponse *MCPResponse;
                MCPResponse = HelloRequester.BlockingHello("Eric");
                if (!MCPResponse)
                        {
                        TERROR(("%s: BlockingRequest() Failed",ProcName));
                        Status = false;
                        break;
                        }
                TraceJSON(*MCPResponse);
                delete MCPResponse;
                } while (0);
        // *******************
        // **** Terminate ****
        // *******************
        Client.Stop(true);              // Join: Waits for MCP client to end.
        DeinitJSONParser();
        return Status ? 0 : 1;
        }

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __MCP_Client_Main */
