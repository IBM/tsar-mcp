// JSON Parser for MCPClient: MCPClientJSON.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPClientJSON.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __MCPClient_JSON

        #define __MCPClient_JSON

#include <RPCClientJSON.h>

// ---------------------------------------------------------------------
// MCP's names for the JSON-RPC envelopes. One type, so an MCP handler
// and the RPC layer below it are talking about the same object.
// ---------------------------------------------------------------------

typedef RPCOutputResponse MCPOutputResponse;

inline MCPOutputResponse* ParseJSON_MCPOutput(const char *JSONBuffer)
        {
        return ParseJSON_RPCOutput(JSONBuffer);
        }

/* **************************************************************************
 ****************************************************************************

 Notes:

 ****************************************************************************
 **** Example ***************************************************************
 *************


 ****************************************************************************

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __MCPClient_JSON */
