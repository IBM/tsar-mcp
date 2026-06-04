// JSONParserDetect.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParserDetect.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2005 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Replacements for Generated JSONParser detection functions.
//
//

#include <_JSONParser.h>
#include <JSONParserDetect.h>

#define EKExtension_ReplacePoint 1      // Define Singleton: ReplacePoint

#if 'A' != 65
        #error KeywordToNodeID table sort assumes ASCII or UNICODE
#endif

#if defined(ExCharacterUNICODE)
        #define KeywordCmp wcsncmp
#else
        #define KeywordCmp strncmp
#endif

// **************************************************************************
// **** Lexical State ID to JSONParser NodeID *********************************
// **************************************************************************

struct KeywordToNodeID_t
        {
        const Exchar_t *Keyword;
        unsigned ParserNodeID;
        };

KeywordToNodeID_t HTJSONSingletonNodeID[] =       // Must be Alphabetical.
        {
        {ExU("false"),Ew_falseID},
        {ExU("null"),Ew_nullID},
        {ExU("true"),Ew_trueID}
        };

#define TotalLexicalStates (NumCoreStates + JSONNumUserStates)

static unsigned _SymbolStateToNodeID[TotalLexicalStates] = {0};


#define JSON_ST_Period                       (FirstUserState+0)
#define JSON_ST_Plus                         (FirstUserState+1)
#define JSON_ST_Minus                        (FirstUserState+2)
#define JSON_ST_Colon                        (FirstUserState+5)
#define JSON_ST_Comma                        (FirstUserState+6)

#define JSON_ST_OpenBrace                    (FirstUserState+7)
#define JSON_ST_CloseBrace                   (FirstUserState+8)
#define JSON_ST_OpenBracket                  (FirstUserState+9)
#define JSON_ST_CloseBracket                 (FirstUserState+10)

#define JSON_ST_DoubleQuote                  (FirstUserState+11)

bool BuildJSONDetectTables()
        {
        // Symbols
        _SymbolStateToNodeID[JSON_ST_Period] = Ew_periodID;
        _SymbolStateToNodeID[JSON_ST_Plus] = Ew_plusID;
        _SymbolStateToNodeID[JSON_ST_Minus] = Ew_minusID;
        _SymbolStateToNodeID[JSON_ST_Colon] = Ew_colonID;
        _SymbolStateToNodeID[JSON_ST_Comma] = Ew_commaID;

        _SymbolStateToNodeID[JSON_ST_OpenBrace] = Ew_OpenBraceID;
        _SymbolStateToNodeID[JSON_ST_CloseBrace] = Ew_CloseBraceID;
        _SymbolStateToNodeID[JSON_ST_OpenBracket] = Ew_OpenBracketID;
        _SymbolStateToNodeID[JSON_ST_CloseBracket] = Ew_CloseBracketID;

        // Strings, DTD, Character Streams, Pragmas and Comments
        _SymbolStateToNodeID[JSON_ST_String] = Ew_StringID;
        return true;
        }

static KeywordToNodeID_t* BinarySearch(KeywordToNodeID_t *Keywords,
                                       int nKeywords,
                                       LexicalItem *ToFind)
	{
        int Compare;
        int Min = 0;
        int Max = nKeywords;
        int Center = Max - 1;
        KeywordToNodeID_t *Record;
	while (Min != Center)
		{
		Record = &Keywords[Center];
                size_t Length = ToFind->Length;
		Compare = KeywordCmp(Record->Keyword,
                                     (const Exchar_t *)ToFind->String,
                                     Length);
                if (Compare == 0) Compare = Record->Keyword[Length];
		if (Compare > 0) Max = Center;
		else if (Compare < 0) Min = Center;
		else return Record;
		Center = (Max - Min) / 2 + Min;
		}
	Record = &Keywords[Center];
	Compare = KeywordCmp(Record->Keyword,
                             (const Exchar_t *)ToFind->String,
                             ToFind->Length);
        if (Compare == 0) Compare = Record->Keyword[ToFind->Length];
	return Compare == 0 ? Record : NULL;
	}

bool ClasifyParserNodeID(JSONLexicalItem *ToClasify, bool AllowHTJSONSingtons)
        {
        bool Status = false;
        ToClasify->ParserNodeID = 0;
        if (ToClasify->SymbolClass == SC_Token)
                {
                if (AllowHTJSONSingtons)
                        {
                        KeywordToNodeID_t *Keywords = HTJSONSingletonNodeID;
                        int nKeywords = sizeof(HTJSONSingletonNodeID) / 
                                        sizeof(KeywordToNodeID_t);
                        KeywordToNodeID_t *Match = BinarySearch(Keywords,
                                                                nKeywords,
                                                                ToClasify);
                        if (!Match) ToClasify->ParserNodeID = Ew_TokenID;
                        else ToClasify->ParserNodeID = Match->ParserNodeID;
                        }
                else ToClasify->ParserNodeID = Ew_TokenID;
                Status = true;
                }
        else if (ToClasify->SymbolClass == SC_Number)
                {
                ToClasify->ParserNodeID = Ew_NumberID;
                Status = true;
                }
        else if (ToClasify->SymbolClass == SC_Space)
                {
                ToClasify->ParserNodeID = 0;
                Status = true;
                }
        else if (ToClasify->SymbolClass == SC_Null)
                {
                ToClasify->ParserNodeID = 0;
                Status = true;
                }
        else    {
                SymbolState_t SymbolID = ToClasify->SymbolID;
                ToClasify->ParserNodeID = _SymbolStateToNodeID[SymbolID];
                if (ToClasify->ParserNodeID != 0) Status = true;
                }
        return Status;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************

