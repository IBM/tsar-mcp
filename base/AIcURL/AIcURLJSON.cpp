// AIcURLJSON.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: AIcURLJSON.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2024 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//


#include <ctype.h>
#include <stdio.h>

#include <ASThread.h>
#include <LevelTrace.h>

#include <AIcURLJSON.h>

#ifdef _MSC_VER
        #define snprintf _snprintf
#endif

// ***************************************************************************
// **** Error Diagnostics ****************************************************
// ***************************************************************************

#define SurroundCHARS 30

static void ShowJSONError(const char *Text, size_t Length, size_t ErrorPos)
        {
        char Buffer[2*SurroundCHARS + 1];
        size_t Start = ErrorPos < SurroundCHARS ? ErrorPos : SurroundCHARS;
        size_t Rest = Length - ErrorPos;
        size_t Count = Start + (Rest < SurroundCHARS ? Rest : SurroundCHARS);
        const char *Pos = Text + ErrorPos - Start;
        char *BufferPos = Buffer;
        while (Count--)
                {
                *BufferPos++ = isspace(*Pos) ? ' ' : *Pos;
                Pos++;
                }
        *BufferPos = '\0';
        TPRINT(("Context:     %s",Buffer));
        TPRINT(("            %*c^",Start+1,' '));
        return;
        }

static const char* FormatJSONError(char *Buffer,
                                   const size_t BufferLen,
                                   const char *JSONBuffer,
                                   size_t JSONBufferLength,
                                   Error_ParseJSON &PE)
        {
        int rc;
        if (!BufferLen) return "";
        char *Pos = Buffer;
        size_t PosLen = BufferLen;
        // **********************
        // **** Message Text ****
        // **********************
        rc = snprintf(Pos,PosLen,"%s : ",PE.GetErrorText());
        Pos += ((unsigned)rc >= PosLen) ? PosLen : rc;
        PosLen -= ((unsigned)rc >= PosLen) ? PosLen : rc;
        JSONLexicalItem *Item = PE.GetItem();
        if (Item)
                {
                // *********************
                // **** Error Token ****
                // *********************
                rc = snprintf(Pos,PosLen,"%.*s : ",(int)Item->Length,
                                                   Item->String);
                Pos += ((unsigned)rc >= PosLen) ? PosLen : rc;
                PosLen -= ((unsigned)rc >= PosLen) ? PosLen : rc;
                if (JSONBuffer)
                        {
                        // *************************
                        // **** Show JSON Context ****
                        // *************************
                        size_t ErrorPos = Item->String - JSONBuffer;
                        size_t Start = ErrorPos < SurroundCHARS ?
                                       ErrorPos : 
                                       SurroundCHARS;
                        size_t Rest = JSONBufferLength - ErrorPos;
                        size_t Count = Start + (Rest < SurroundCHARS ? 
                                                Rest : 
                                                SurroundCHARS);
                        rc = snprintf(Pos,PosLen,"%.*s^%.*s^%.*s",
                                      (int)Start,
                                      JSONBuffer + ErrorPos - Start,
                                      (int)Item->Length,
                                      JSONBuffer + ErrorPos,
                                      (int)(Count - Item->Length - Start),
                                      JSONBuffer + ErrorPos + Item->Length);
                        }
                }
        Buffer[BufferLen-1] = '\0';
        return Buffer;
        }

void PrintFormatedJSONError(const char *JSONBuffer,
                          size_t JSONBufferLength,
                          Error_ParseJSON &PE)
        {
        char Buffer[256];
        FormatJSONError(Buffer,sizeof(Buffer),JSONBuffer,JSONBufferLength,PE);
        TPRINT(("%s",Buffer));
        return;
        }

// ***************************************************************************
// **** JSON Parser State Machines (File-Static) *****************************
// ***************************************************************************

static JSONSymbolStateMachine LexMachine;
static StateMachine LRMachine(0);
static Mutex csJSONParser;                      // Serializes all parsing.

void InitJSONParser()
        {
        SetMultithreadSafeAlloc(false);         // csJSONParser serializes.
        SetExpAlloc(JSONExpAlloc,JSONExpDealloc);
        SetupJSONTransitions(LRMachine);
        BuildJSONDetectTables();
        return;
        }

void DeinitJSONParser()
        {
        static const char *ProcName = "DeinitJSONParser";
        LRMachine.Reset(0);      
        TDEBUG(("%s: JSON Heap Size: %u (items)",ProcName,JSONExpGetDepth()));
        return;
        }

// ***************************************************************************
// **** ParseJSON_mt (Low-Level Parser) **************************************
// ***********************************
//
//      Acquires csJSONParser for the duration of the parse. The
//      parser's node allocator (JSONParserExp) is a shared pool
//      that is not thread-safe. The mutex is released before 
//      returning, so the caller owns the Node* tree outright and
//      may walk it freely in any thread. Call DeleteJSON_mt() to
//      release the tree back to the pool (re-acquires the mutex).
//
// ***************************************************************************

Node* ParseJSON_mt(const char *JSONBuffer)
        {
        csJSONParser.Take();
        Node *StartNode = NULL;
        const char *NonTerminalGoal = NULL;
        Node* JSONExpression = NULL;
        try     {
                JSONExpression = ParseJSONBuffer(LRMachine,
                                                 LexMachine,
                                                 JSONBuffer,
                                                 true,
                                                 StartNode,
                                                 NonTerminalGoal);
                }
        catch (Error_ParseJSON &PE)
                {
                TPRINT(("****************************************"));
                TPRINT(("Parse Error: %s",PE.GetErrorText()));
                if (PE.GetParserState() != Error_NOSTATE)
                        {
                        TPRINT(("Parse State: %u",PE.GetParserState()));
                        }
                JSONLexicalItem *Item = PE.GetItem();
                if (Item)
                        {
                        size_t ErrorPos = Item->String - JSONBuffer;
                        TPRINT(("Position:    %u",ErrorPos));
                        TPRINT(("SymbolClass: %u",Item->SymbolClass));
                        TPRINT(("SymbolID:    %u",Item->SymbolID));
                        TPRINT(("NodeID(Ew_): %u",Item->ParserNodeID));
                        TPRINT(("Token:       %.*s",Item->Length,
                                                    Item->String));
                        ShowJSONError(JSONBuffer,strlen(JSONBuffer),ErrorPos);
                        }
                TPRINT(("****************************************"));
                PrintFormatedJSONError(JSONBuffer,strlen(JSONBuffer),PE);
                TPRINT(("****************************************"));
                }
        catch (ErrorBase &E)
                {
                TPRINT(("****************************************"));
                TPRINT(("Cought Error: %s",E.GetErrorText()));
                TPRINT(("****************************************"));
                }
        csJSONParser.Release();
        return JSONExpression;
        }

// ***************************************************************************
// **** DeleteJSON_mt (Thread-Safe Expression Deallocation) *****************
// ***************************************************************************
//
//      Acquires csJSONParser to safely return parse-tree nodes to
//      the shared pool allocator, then releases the mutex.
//
// ***************************************************************************

void DeleteJSON_mt(Node *JSONExpression)
        {
        if (!JSONExpression) return;
        csJSONParser.Take();
        delete JSONExpression;
        csJSONParser.Release();
        return;
        }

// ***************************************************************************
// **** BuildJSONObject (Thread-Safe JSON Parse + Build) *********************
// ***************************************************************************

JSON_Object* BuildJSONObject(const char *JSONBuffer)
        {
        static const char *ProcName = "BuildJSONObject";
        Node *JSONExpression = ParseJSON_mt(JSONBuffer);
        JSON_Object *JSONObj = NULL;
        if (JSONExpression)
                {
                JSONObj = BuildJSONObject(JSONExpression);
                DeleteJSON_mt(JSONExpression);
                }
        return JSONObj;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
