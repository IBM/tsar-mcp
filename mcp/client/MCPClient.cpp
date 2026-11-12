// MCP Client: MCPClient.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPClient.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <AutoString.h>
#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <popenNT.h>

#include <MCPClient.h>

// ***************************************************************************
// **** MCPClientThread ******************************************************
// ***************************************************************************

MCPClientThread::MCPClientThread(const char *Command) 
        :
        RPCClientThread(Command)
        {
        // -----------------------------------------------------
        // MCPProtocolVersion_Invalid until initialize response.
        // -----------------------------------------------------
        ProtocolVersion = MCPProtocolVersion_Invalid;
        return;
        }

// ***************************************************************************
// **** initialize ***********************************************************
// ***************************************************************************

static char* FormMCPRequest_initialize(JSON_Value &idRequest,
                                       int protocolVersion,
                                       const char *capabilities,
                                       const char *clientName,
                                       const char *clientVersion)
        {
        static const char *ProcName = "FormMCPRequest_initialize";
        /* ----- OUTPUT ----------------------------------------------
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
        char *EscName = EscapeJSONString(clientName ? clientName : "");
        char *EscVersion = EscapeJSONString(clientVersion
                                          ? clientVersion : "");
        if (!EscName || !EscVersion)
                {
                TERROR(("%s: EscapeJSONString() Failed",ProcName));
                FreeEscapeJSONString(&EscName);
                FreeEscapeJSONString(&EscVersion);
                return NULL;
                }
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"id\":%s,",id2string(idRequest));
        Canvas.printf("\"method\":\"initialize\",");
        // ----------------
        // ---- params ----
        // ----------------
        Canvas.printf("\"params\":{");
        Canvas.printf("\"protocolVersion\":\"%d-%.2d-%.2d\",",
                      protocolVersion / 10000,
                      protocolVersion / 100 % 100,
                      protocolVersion % 100);
        // -------------------------------------------------------------
        // ---- capabilities: JSON object body with no outer braces ----
        // -------------------------------------------------------------
        Canvas.printf("\"capabilities\":{%s},",capabilities ? capabilities
                                                            : "");
        Canvas.printf("\"clientInfo\":{"
                                       "\"name\":\"%s\","
                                       "\"version\":\"%s\"}",
                      EscName,
                      EscVersion);
        Canvas.printf("}");
        // ----------------
        // ----------------
        Canvas.printf("}");
        char *MCPOutput = Canvas.AquireBuffer();
        FreeEscapeJSONString(&EscName);
        FreeEscapeJSONString(&EscVersion);
        return MCPOutput;
        }

static char* FormMCPNotification_initialized()
        {
        /* ----- OUTPUT ----------------------------------------------
        {
          "jsonrpc": "2.0",
          "method": "notifications/initialized"
        }
        ----------------------------------------------------------- */
        MemoryPrintf Canvas;
        Canvas.printf("{");
        Canvas.printf("\"jsonrpc\":\"2.0\",");
        Canvas.printf("\"method\":\"notifications/initialized\"");
        Canvas.printf("}");
        return Canvas.AquireBuffer();   // A notification carries no id.
        }

MCPInitRequester::MCPInitRequester(MCPClientThread &Client,
                                   int protocolVersion,
                                   const char *capabilities,
                                   const char *clientName,
                                   const char *clientVersion)
        :
        RPCSyncRequester(Client),
        MCPClient(Client)
        {
        ProtocolVersion = protocolVersion;
        Capabilities = capabilities;
        ClientName = clientName;
        ClientVersion = clientVersion;
        return;
        }

char* MCPInitRequester::OnRequest(JSON_Value &idRequest)
        {
        return FormMCPRequest_initialize(idRequest,
                                         ProtocolVersion,
                                         Capabilities.GetString(),
                                         ClientName.GetString(),
                                         ClientVersion.GetString());
        }

bool MCPInitRequester::BlockingInitialize(unsigned TimeoutMS)
        {
        static const char *ProcName = "MCPInitRequester::BlockingInitialize";
        RPCOutputResponse *MCPResponse;
        MCPResponse = RPCSyncRequester::BlockingRequest(TimeoutMS);
        if (!MCPResponse)
                {
                TERROR(("%s: No initialize Response",ProcName));
                return false;
                }
        // -----------------------------------------
        // ---- Record what the server agreed to ----
        // -----------------------------------------
        bool Status = false;
        JSON_Object *Result = MCPResponse->Get_result();
        if (!Result)
                {
                TERROR(("%s: initialize Response has no result",ProcName));
                TraceRPCError(ProcName,*MCPResponse);
                }
        else    {
                const char *Version;
                Version = Result->Find_member_string("protocolVersion");
                MCPClient.set_ProtocolVersion(MCPProtocolVersionInt(Version));
                JSON_Object *ServerInfo;
                ServerInfo = Result->Find_member_object("serverInfo");
                if (ServerInfo) MCPClient.set_ServerInfo(*ServerInfo);
                JSON_Object *Capabilities;
                Capabilities = Result->Find_member_object("capabilities");
                if (Capabilities) MCPClient.set_Capabilities(*Capabilities);
                const char *Name = ServerInfo
                                 ? ServerInfo->Find_member_string("name")
                                 : NULL;
                const char *serverVersion = ServerInfo
                                 ? ServerInfo->Find_member_string("version")
                                 : NULL;
                TINFO(("%s: Server: name=%s, Version=%s",ProcName,
                                                         Name ? Name : "?",
                                                         serverVersion
                                                         ? serverVersion
                                                         : "?"));
                TINFO(("%s: Negotiated protocolVersion=%d",
                       ProcName,
                       MCPClient.get_ProtocolVersion()));
                Status = true;
                }
        delete MCPResponse;
        // ----------------------------------------
        // ---- Return 'initialized' to server ----
        // ----------------------------------------
        char *MCPNotify = FormMCPNotification_initialized();
        if (!MCPNotify)
                {
                TERROR(("%s: FormMCPNotification_initialized() Failed",
                        ProcName));
                }
        else    {
                bool Sent = PostXCPOutput(&MCPNotify);
                if (!Sent || MCPNotify)
                        {
                        TERROR(("%s: PostXCPOutput() Failed",ProcName));
                        FreeXCPRequestBuffer(&MCPNotify);
                        }
                }
        return Status;
        }

// ***************************************************************************
// ******************************* End of File *******************************
// ***************************************************************************
