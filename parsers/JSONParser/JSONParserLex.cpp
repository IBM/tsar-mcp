// JSON Language Lexar: JSONParserLex.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParserLex.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2016 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//

#include <JSONParserLex.h>

JSONSymbolStateMachine::JSONSymbolStateMachine()
        :
        SymbolStateMachine(JSONNumUserStates)
        {
        char c;
        // Period
        AssignStateTable(JSON_ST_Period,ST_CoreSYMBOL,10); // floating point
        AddTransition(ST_CoreALL,'.',JSON_ST_Period,true);              // .
        // Plus
        AssignStateTable(JSON_ST_Plus,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'+',JSON_ST_Plus,true);                // +
        // Minus
        AssignStateTable(JSON_ST_Minus,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'-',JSON_ST_Minus,true);               // -
        // Colon
        AssignStateTable(JSON_ST_Colon,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,':',JSON_ST_Colon,true);               // :
        // Comma
        AssignStateTable(JSON_ST_Comma,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,',',JSON_ST_Comma,true);               // ,

        // OpenBrace
        AssignStateTable(JSON_ST_OpenBrace,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'{',JSON_ST_OpenBrace,true);           // {
        // CloseBrace
        AssignStateTable(JSON_ST_CloseBrace,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'}',JSON_ST_CloseBrace,true);          // }
        // OpenBracket
        AssignStateTable(JSON_ST_OpenBracket,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'[',JSON_ST_OpenBracket,true);         // [
        // CloseBracket
        AssignStateTable(JSON_ST_CloseBracket,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,']',JSON_ST_CloseBracket,true);        // ]

        // DoubleQuote (Accept String)
        AssignStateDefault(JSON_ST_InString,JSON_ST_InString,2);        // "
        AssignStateTable(JSON_ST_String,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'"',JSON_ST_InString,true);
        AddTransition(JSON_ST_InString,'"',JSON_ST_String,false);       

        AssignStateDefault(JSON_ST_StringESC,JSON_ST_InString,0);       // \"        
        AddTransition(JSON_ST_InString,'\\',JSON_ST_StringESC,false);    

        // Floating point numbers
        AssignStateTable(JSON_ST_Exponent,ST_CoreDIGIT,2);
        for (c='0';c<='9';c++)
                {
                AddTransition(JSON_ST_Period,c,ST_CoreDIGIT,false);
                }
        AddTransition(ST_CoreDIGIT,'.',ST_CoreDIGIT,false);
        AddTransition(ST_CoreDIGIT,'E',JSON_ST_Exponent,false);
        AddTransition(ST_CoreDIGIT,'e',JSON_ST_Exponent,false);
        AddTransition(JSON_ST_Exponent,'+',ST_CoreDIGIT,false);
        AddTransition(JSON_ST_Exponent,'-',ST_CoreDIGIT,false);

        // Tokens with Digits
        for (c='0';c<='9';c++)
                {
                AddTransition(ST_CoreCHAR,c,ST_CoreCHAR,false);
                }

        // Symbols as Characters
        AddTransition(ST_CoreALL,'_',ST_CoreCHAR,true);               // _
        AddTransition(ST_CoreCHAR,'_',ST_CoreCHAR,false);
        AddTransition(ST_CoreCHAR,'-',ST_CoreCHAR,false);
        return;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
