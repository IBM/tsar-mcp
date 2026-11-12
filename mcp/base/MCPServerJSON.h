// MCPServerJSON.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServerJSON.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// JSON MCP Input Request Format:
//      {
//        "jsonrpc": "2.0",
//        "id": 3,
//        "method": "tools/call",
//        "params": {
//          "name": "helloWorld",
//          "arguments": { "name": "Eric" }
//        }
//      }
//

#ifndef __MCPServer_JSON

        #define __MCPServer_JSON

#include <time.h>

#include <JSONObject.h>
#include <MEMprintf.h>

// ******************************
// **** MCP Protocol Version ****
// ******************************

#define MCPProtocolVersion_Invalid   0          // protocolVersion malformed.
#define MCPProtocolVersion_20241105  20241105
#define MCPProtocolVersion_20251125  20251125   // "stateful"
#define MCPProtocolVersion_20260728  20260728   // "stateless"

int MCPProtocolVersionInt(const char *Version); // "YYYY-MM-DD" to YYYYMMDD. 

// *************************
// **** MCPInputRequest ****
// *************************

class MCPInputRequest
        {
        private:
                JSON_Object *JSONRequest;
        protected:
                JSON_Member* FindMember(const char *Key);
                JSON_Value* FindMemberValue(const char *Key);
        public:
                MCPInputRequest(JSON_Object **Request);
                MCPInputRequest(MCPInputRequest &Source);
                ~MCPInputRequest() {Clear();}
                JSON_Object* _getJSONRequest() {return JSONRequest;}
                void Clear();
                const char* Get_jsonrpc();
                JSON_Value* Get_id();
                const char* Get_method();
                JSON_Object* Get_params();
                JSON_Object* Get_result();  // A response has either a result,
                JSON_Object* Get_error();   // or when NULL, then an error.
                JSON_Value* Get_param(const char *Key);
                MCPInputRequest& operator =(MCPInputRequest &Source);
                // ----------------------------------
                // ---- Get type specific params ----
                // ----------------------------------
                bool Get_param_int(int *iParam, const char *Key);
                const char* Get_param_string(const char *Key);
                JSON_Object* Get_param_object(const char *Key);
                // ------------------------------------------
                // ---- _meta protocolVersion, or Legacy ----
                // ------------------------------------------
                int Version();          // Raw per-request report (no policy).
        };

typedef MCPInputRequest RPCInputRequest;

// ***************************************
// **** JSON Parser Init/Deinit **********
// ***************************************

void InitJSONParser();
void DeinitJSONParser();

// *******************************************************************
// **** ParseJSON_mt / DeleteJSON_mt *********************************
// *******************************************************************
//
//      JSON parsing must be accomplished using the multithreaded
//      safe functions. ParseJSON_mt returns a Node* expression 
//      tree; call DeleteJSON_mt when done.
//
//      BuildJSONObject(const char*) is the high-level convenience 
//      wrapper: parse → build JSON_Object → delete expression.
//
// *******************************************************************

Node* ParseJSON_mt(const char *JSONBuffer);
void DeleteJSON_mt(Node *JSONExpression);

// *********************************************
// **** BuildJSONObject (Thread-Safe Parse) ****
// *********************************************

JSON_Object* BuildJSONObject(const char *JSONBuffer);

// ****************************
// **** ParseJSON_MCPInput ****
// ****************************

MCPInputRequest* ParseJSON_MCPInput(const char *JSONBuffer);

bool ValidateJSON(const char *JSONBuffer);      // Parses and Discards.

bool TraceJSON(const char *JSONBuffer, bool Pretty=true);

void TraceJSON(MCPInputRequest &MCPRequest, bool Pretty=true);

// *******************************************************************
// **** ExtractSampleText ********************************************
// ************************
//
//      LLM text from a sampling response, spanning both protocol
//      shapes so callers stay version-agnostic:
//        Classic (2024-11-05 / external): result.content.text
//        MRTR    (2026-07-28 resume):     inputResponses[0]
//                                         .content.content.text
//      A flattened .content.text is tolerated. NULL if absent.
//
// *******************************************************************

const char* ExtractSampleText(MCPInputRequest &MCPRequest);

// ***************************************************************************
// **** JSON Object Print Function (See JSONObject.h for others) *************
// ***************************************************************************

int PrintJSONObject(MemoryPrintf &Canvas,       // Returns bytes output.
                    JSON_Object &Object,        
                    bool asJSON,
                    bool Pretty);

int PrintJSONValue(MemoryPrintf &Canvas,        // Returns bytes output.
                   JSON_Value &Value,           
                   bool asJSON,
                   bool Pretty);

// ***************************************************************************
// **** MCP id Utilites ******************************************************
// ***************************************************************************

// -----------------------------------
// ---- id2string (Print Utility) ----
// -----------------------------------

class _MCPIdString
        {
        private:
                char idBuffer[128];     // UUID ids with room for a prefix.
        protected:
                void PrintToBuffer(JSON_Value *id);
        public:
                _MCPIdString(JSON_Value *id) {PrintToBuffer(id);}
                _MCPIdString(JSON_Value &id) {PrintToBuffer(&id);}
                operator const char* () {return idBuffer;}
        };

#define id2string(id) ((const char*)_MCPIdString(id))

// -----------------------------------
// ---- id Compare -------------------
// -----------------------------------

bool idcmp(JSON_Value &cmpA, JSON_Value &cmpB);

// -----------------------------------------
// ---- FuzzyPeekJSONId (Debug Utility) ----
// -----------------------------------------

const char* FuzzyPeekJSONId(const char *JSON, char *Buf, size_t BufLen);

// ***************************************************************************
// **** Independent File, Time, String Utilties ******************************
// ***************************************************************************

// ------------------------------------------------------
// ---- BuildFQFN (Construct a Fully Qualified Path) ----
// ------------------------------------------------------

#define FILE_SEPERATOR_WIN '\\'             
#define FILE_SEPERATOR_UNIX '/'
#ifdef _WIN32
        #define FILE_SEPERATOR FILE_SEPERATOR_WIN
#else
        #define FILE_SEPERATOR FILE_SEPERATOR_UNIX
#endif

char* BuildFQFN(const char *Path, const char *Filename);

void FreeFQFN(char **Fullname);

// ----------------------------------
// ---- Extract delimited tokens ----
// ----------------------------------

const char* MCP_strtok(const char *Start, 
                       size_t *Length, 
                       const char *Delimiters);

const char* MCP_ltrim(const char *Start, size_t *Length);

size_t* MCP_rtrim(const char *Start, size_t *Length);

// ---------------------------------------------------
// ---- Case insenstive strstr - Completion match ----
// ---------------------------------------------------

const char* MCP_stristr(const char *String, const char *ToFind);

// -------------------------------------------------
// ---- MCP_localtime - Thread safe localtime() ----
// -------------------------------------------------

struct tm* MCP_localtime(struct tm *tmTime, time_t tTime);

/* ***************************************************************************
 Notes:

******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

*****************************************************************************/

#endif
