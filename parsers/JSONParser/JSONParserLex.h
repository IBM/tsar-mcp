// JSON Language Lexar: JSONParserLex.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParserLex.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2016 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __MarkupLanague_Lex

        #define __MarkupLanague_Lex

#include <LexicalParser.h>

class JSONSymbolStateMachine : public SymbolStateMachine
        {
        public:
                JSONSymbolStateMachine();
        };

#define JSONNumUserStates 15

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

// String
#define JSON_ST_InString                     JSON_ST_DoubleQuote
#define JSON_ST_String                       (FirstUserState+12)
#define JSON_ST_StringESC                    (FirstUserState+13)

// Floating point
#define JSON_ST_Exponent                     (FirstUserState+14)

/* **************************************************************************
 ****************************************************************************

 Notes:

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __MarkupLanague_Lex */
