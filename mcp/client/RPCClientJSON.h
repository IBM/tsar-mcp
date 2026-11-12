// JSON Parser for RPCClient: RPCClientJSON.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: RPCClientJSON.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// JSON-RPC 2.0 envelopes: jsonrpc, id, method, params, result, error.
// Nothing protocol-specific lives here.
//
// RPCInputRequest is MCPServer's reader under its own name -- the wire
// format is the same one, and that header is shared with the Server
// project, so it is aliased rather than copied.
//

#ifndef __RPC_Client_JSON

        #define __RPC_Client_JSON

#include <LevelTrace.h>
#include <MCPServerJSON.h>

class RPCOutputResponse : public RPCInputRequest
        {
        public:
                RPCOutputResponse(JSON_Object **Response) 
                        : 
                        RPCInputRequest(Response) 
                        {
                        return;
                        }
                RPCOutputResponse(RPCOutputResponse &Source)
                        :
                        RPCInputRequest(Source)
                        {
                        return;
                        }
        };

// ***************************************************************************
// **** Parse JSON-RPC *******************************************************
// ***************************************************************************

inline RPCInputRequest* ParseJSON_RPCInput(const char *JSONBuffer)
        {
        return ParseJSON_MCPInput(JSONBuffer);
        }

inline RPCOutputResponse* ParseJSON_RPCOutput(const char *JSONBuffer)
        {
        static const char *ProcName = "ParseJSON_RPCOutput";
        JSON_Object *JSONResponse = BuildJSONObject(JSONBuffer);
        if (!JSONResponse)
                {
                TERROR(("%s: BuildJSONObject() FAILED",ProcName));
                return NULL;
                }
        RPCOutputResponse *RPCResponse = new RPCOutputResponse(&JSONResponse);
        if (!RPCResponse)
                {
                TERROR(("%s: Unable to allocate RPCResponse",ProcName));
                if (JSONResponse) delete JSONResponse;
                return NULL;
                }
        return RPCResponse;
        }

#endif /* __RPC_Client_JSON */
