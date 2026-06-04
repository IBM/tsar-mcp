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

#include <JSONObject.h>

// *************************
// **** MCPInputRequest ****
// *************************

class MCPInputRequest
        {
        private:
                JSON_Object *JSONRequest;
                friend MCPInputRequest* ParseJSON_MSCPInput(const char*);
        protected:
                JSON_Member* FindMember(const char *Key);
                JSON_Value* FindMemberValue(const char *Key);
        public:
                MCPInputRequest() {JSONRequest = NULL;}
                MCPInputRequest(MCPInputRequest &Source);
                ~MCPInputRequest() {Clear();}
                void Clear();
                const char* Get_jsonrpc();
                JSON_Value* Get_id();
                const char* Get_method();
                JSON_Object* Get_params();
                JSON_Object* Get_result();
                JSON_Value* Get_param(const char *Key);
                MCPInputRequest& operator =(MCPInputRequest &Source);
                // ----------------------------------
                // ---- Get type specific params ----
                // ----------------------------------
                bool Get_param_int(int *iParam, const char *Key);
                const char* Get_param_string(const char *Key);
                JSON_Object* Get_param_object(const char *Key);
        };

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

// *****************************
// **** ParseJSON_MSCPInput ****
// *****************************

MCPInputRequest* ParseJSON_MSCPInput(const char *JSONBuffer);

bool ValidateJSON(const char *JSONBuffer);      // Parses and Discards.

bool TraceJSON(const char *JSONBuffer, bool Pretty=true);

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

// ********************************************************
// **** BuildFQFN (Construct a Fully Qualified Path) ******
// ********************************************************

#define FILE_SEPERATOR_WIN '\\'             
#define FILE_SEPERATOR_UNIX '/'
#ifdef _WIN32
        #define FILE_SEPERATOR FILE_SEPERATOR_WIN
#else
        #define FILE_SEPERATOR FILE_SEPERATOR_UNIX
#endif

// ********************************************************
// **** BuildFQFN (Construct a Fully Qualified Path) ******
// ********************************************************

char* BuildFQFN(const char *Path, const char *Filename);

void FreeFQFN(char **Fullname);

// ***************************************************************************
// **** MCP id Utilites ******************************************************
// ***************************************************************************

// -----------------------------------
// ---- id2string (Print Utility) ----
// -----------------------------------

class _MCPIdString
        {
        private:
                char idBuffer[64];
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

/* ***************************************************************************
 Notes:

******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

*****************************************************************************/

#endif
