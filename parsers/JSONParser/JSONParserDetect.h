// JSONParserDetect.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParserDetect.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2005 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Inline replacements for Generated JSONParser detection functions:
//
//      1. Search for section:
//
//              ******************************************************
//              **** Input Detecting Functions (Manually Defined) ****
//              ******************************************************
//
//      2. Replace inline functions in that section with:
//
//              #include <JSONParserDetect.h>
//

#ifndef __JSONParser_Detect

        #define __JSONParser_Detect

#include <JSONParserLex.h>

// **************************************************************************
// **** JSONLexicalItem *******************************************************
// ********************
//
//  --> Lexical Item with NodeID
//
//      Memory allocation 'borrowed' from Expresion.cpp. When set with:
//
//              - SetExpAlloc(JSONParserExpAlloc,JSONParserExpDealloc)
//
//      uses the allocation routines in JSONExpression.cpp.
//
// **************************************************************************

#ifndef __Expression_Parser
        extern void* (__ExpDECL *ExpAlloc)(size_t Size);
        extern void (__ExpDECL *ExpDealloc)(void *Memory);
#endif

struct JSONLexicalItem : public LexicalItem /* inputT */
        {
        unsigned ParserNodeID;                  // Used by JSONParserDetect.
        // ***************************
        // **** Memory Management ****
        // ***************************
        void* operator new(size_t Size) {return ExpAlloc(Size);}
        void operator delete(void *Item) {ExpDealloc(Item);}
        };

// **************************************************************************
// **** Lexical State ID to SQLParser NodeID Functions **********************
// **************************************************************************

bool BuildJSONDetectTables();

bool ClasifyParserNodeID(JSONLexicalItem *ToClasify, bool AllowHTJSONSingtons);

// **************************************************************************
// **** Token and Symbol Parser Identification : Used by _JSONParser **********
// **************************************************************************

#ifdef __GUARD_JSONParser

namespace _JSONParser {

inline bool IsEOL(inputT x) 
        {return ((JSONLexicalItem *)(x))->SymbolClass == SC_Null;}

inline bool IsToken(inputT x) 
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_TokenID;}

inline bool IsString(inputT x) 
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_StringID;}

inline bool IsNumber(inputT x) 
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_NumberID;}

inline bool Isfalse(inputT x)                                                \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_falseID;}

inline bool Isnull(inputT x)                                                 \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_nullID;}

inline bool Istrue(inputT x)                                                 \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_trueID;}

// **************************************
// **** Symbols and Symbol Sequences ****
// **************************************

inline bool IsOpenBrace(inputT x)                                            \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_OpenBraceID;}

inline bool IsCloseBrace(inputT x)                                           \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_CloseBraceID;}

inline bool IsOpenBracket(inputT x)                                          \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_OpenBracketID;}

inline bool IsCloseBracket(inputT x)                                         \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_CloseBracketID;}

inline bool Isperiod(inputT x)                                               \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_periodID;}

inline bool Isplus(inputT x)                                                 \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_plusID;}

inline bool Isminus(inputT x)                                                \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_minusID;}

inline bool Iscolon(inputT x)                                                \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_colonID;}

inline bool Iscomma(inputT x)                                                \
        {return ((JSONLexicalItem *)(x))->ParserNodeID == Ew_commaID;}

} /* _JSONParser */

#endif /* __GUARD_JSONParser */

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

#endif /* __JSONParser_Detect */


