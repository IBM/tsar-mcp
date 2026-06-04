///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SQLLex.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.                                                
//                    
// Author(s) : Eric Kass 
//
///////////////////////////////////////////////////////////////////////////////
#ifndef __SQL_Lexical_Parser

        #define __SQL_Lexical_Parser

#include "LexicalParser.h"

// ****************************************************************************
// **** SQL Lexical States ****************************************************
// ****************************************************************************

#define SQLNumUserStates 36

#define ST_Comma                        (FirstUserState+0)
#define ST_Period                       (FirstUserState+1)
#define ST_Minus                        (FirstUserState+2)
#define ST_MinusMinus                   (FirstUserState+3)
#define ST_Plus                         (FirstUserState+4)
#define ST_Slash                        (FirstUserState+5)
#define ST_Multiply                     (FirstUserState+6)
#define ST_Equal                        (FirstUserState+7)
#define ST_Bang                         (FirstUserState+8)
#define ST_BangEqual                    (FirstUserState+9)
#define ST_Or                           (FirstUserState+10)
#define ST_And                          (FirstUserState+11)
#define ST_Percent                      (FirstUserState+12)
#define ST_Caret                        (FirstUserState+13)
#define ST_Less                         (FirstUserState+14)
#define ST_LessEqual                    (FirstUserState+15)
#define ST_LessGreater                  (FirstUserState+16)
#define ST_Greater                      (FirstUserState+17)
#define ST_GreaterEqual                 (FirstUserState+18)
#define ST_Quote                        (FirstUserState+19)
#define ST_DoubleQuote                  (FirstUserState+20)
#define ST_ParenOpen                    (FirstUserState+21)
#define ST_ParenClose                   (FirstUserState+22)
#define ST_QuestionMark                 (FirstUserState+23)
#define ST_SlashMultiply                (FirstUserState+24)
#define ST_MultiplySlash                (FirstUserState+25)
#define ST_OrOr                         (FirstUserState+26)
#define ST_Colon                        (FirstUserState+27)

// ***********************************************
// **** Strings, Literals, Hints and Comments ****
// ***********************************************

#define ST_InLiteral                    ST_Quote
#define ST_QuoteQuote                   (FirstUserState+28)
#define ST_InLiteralQuote               ST_QuoteQuote
#define ST_X                            (FirstUserState+29)
#define ST_U                            (FirstUserState+30)

#define ST_InString                     ST_DoubleQuote
#define ST_DoubleQuoteDoubleQuote       (FirstUserState+31)
#define ST_InStringDoubleQuote          ST_DoubleQuoteDoubleQuote

#define ST_Literal                      ST_QuoteQuote
#define ST_String                       ST_DoubleQuoteDoubleQuote
#define ST_LineComment                  ST_MinusMinus
#define ST_InComment                    ST_SlashMultiply
#define ST_CommentEnd                   ST_MultiplySlash
#define ST_Comment                      (FirstUserState+32)

// Special: DBSL Hints in Comments

#define ST_InCommentPercent             (FirstUserState+33)
#define ST_LineCommentPercent           (FirstUserState+34)

// Special: ABAP Exec-SQL Variables

#define ST_ExecSQLVar                   ST_Colon

// ************************
// **** Floating point ****
// ************************

#define ST_Exponent                     (FirstUserState+35)

// ****************************************************************************
// **** SQL Lexical Stream ****************************************************
// ****************************************************************************

class SQLSymbolStateMachine : public SymbolStateMachine
        {
        public:
                SQLSymbolStateMachine();
        };

/* **************************************************************************
 ****************************************************************************

 Notes:

     1. Literals and Strings:

             A. Literals can contain embeded single quotes as ''
             B. Strings can contain embeded double quotes as ""
             C. All Literals return with ST_Literal
             D. All Strings return with ST_String


 ****************************************************************************
 **** Example ***************************************************************
 *************

 ****************************************************************************


 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __SQL_Lexical_Parser */
