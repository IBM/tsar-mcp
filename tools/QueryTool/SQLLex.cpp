///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SQLLex.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.                                                
//                    
// Author(s) : Eric Kass 
//
///////////////////////////////////////////////////////////////////////////////
#include <SQLLex.h>

#define HintsInComments 0               // %_HINTS inside SQL Comments. 
#define HintsInLineComments 1           // %_HINTS override SQL Line Comments.

// ****************************************************************************
// **** SQL Lexical Stream ****************************************************
// ****************************************************************************

SQLSymbolStateMachine::SQLSymbolStateMachine()
        :
        SymbolStateMachine(SQLNumUserStates)
        {
        unsigned char c;
        // Symbols as Characters
        AddTransition(ST_CoreALL,'_',ST_CoreCHAR,true);             // _
        AddTransition(ST_CoreCHAR,'_',ST_CoreCHAR,false);
        AddTransition(ST_CoreALL,'$',ST_CoreCHAR,true);             // $
        AddTransition(ST_CoreCHAR,'$',ST_CoreCHAR,false);
        AddTransition(ST_CoreALL,'#',ST_CoreCHAR,true);             // #
        AddTransition(ST_CoreCHAR,'#',ST_CoreCHAR,false);
        AddTransition(ST_CoreALL,'@',ST_CoreCHAR,true);             // @
        AddTransition(ST_CoreCHAR,'@',ST_CoreCHAR,false);
        // Comma
        AssignStateTable(ST_Comma,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,',',ST_Comma,true);                // ,
        // Period
        AssignStateTable(ST_Period,ST_CoreSYMBOL,10);   // floating point
        AddTransition(ST_CoreALL,'.',ST_Period,true);               // .
        // Minus
        AssignStateTable(ST_Minus,ST_CoreSYMBOL,1);     // comment
        AddTransition(ST_CoreALL,'-',ST_Minus,true);                // -
        // Plus
        AssignStateTable(ST_Plus,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'+',ST_Plus,true);                 // +
        // Slash
        AssignStateTable(ST_Slash,ST_CoreSYMBOL,2);     // 2: comments
        AddTransition(ST_CoreALL,'/',ST_Slash,true);                // /
        // Multiply
        AssignStateTable(ST_Multiply,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'*',ST_Multiply,true);             // *
        // Equal
        AssignStateTable(ST_Equal,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'=',ST_Equal,true);                // =
        // Bang
        AssignStateTable(ST_Bang,ST_CoreSYMBOL,1);
        AssignStateTable(ST_BangEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'!',ST_Bang,true);                 // !
        AddTransition(ST_Bang,'=',ST_BangEqual,false);              // !=
        // Or
        AssignStateTable(ST_Or,ST_CoreSYMBOL,1);
        AssignStateTable(ST_OrOr,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'|',ST_Or,true);                   // |
        AddTransition(ST_Or,'|',ST_OrOr,false);                     // ||
        // And
        AssignStateTable(ST_And,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'&',ST_And,true);                  // &
        // Percent
        AssignStateTable(ST_Percent,ST_CoreSYMBOL,1);   // 1 for Underbar
        AddTransition(ST_CoreALL,'%',ST_Percent,true);              // %
        // Caret
        AssignStateTable(ST_Caret,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'^',ST_Caret,true);                // ^
        // Less
        AssignStateTable(ST_Less,ST_CoreSYMBOL,2);
        AssignStateTable(ST_LessEqual,ST_CoreSYMBOL,0);
        AssignStateTable(ST_LessGreater,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'<',ST_Less,true);                 // <
        AddTransition(ST_Less,'=',ST_LessEqual,false);              // <=
        AddTransition(ST_Less,'>',ST_LessGreater,false);            // <>
        // Greater
        AssignStateTable(ST_Greater,ST_CoreSYMBOL,1);
        AssignStateTable(ST_GreaterEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'>',ST_Greater,true);              // >
        AddTransition(ST_Greater,'=',ST_GreaterEqual,false);        // >=
        // Parenthesis
        AssignStateTable(ST_ParenOpen,ST_CoreSYMBOL,0);                 // (
        AddTransition(ST_CoreALL,'(',ST_ParenOpen,true);
        AssignStateTable(ST_ParenClose,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,')',ST_ParenClose,true);           // )
        // QuestionMark
        AssignStateTable(ST_QuestionMark,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'?',ST_QuestionMark,true);         // ?
        // Quote (Accept Literal) - Two quotes '' remains in literal.
        AssignStateDefault(ST_InLiteral,ST_InLiteral,1);
        AssignStateTable(ST_InLiteralQuote,ST_CoreSYMBOL,1);
        AddTransition(ST_CoreALL,'\'',ST_InLiteral,true);
        AddTransition(ST_InLiteral,'\'',ST_InLiteralQuote,false);
        AddTransition(ST_InLiteralQuote,'\'',ST_InLiteral,false);
        // Floating point numbers
        AssignStateTable(ST_Exponent,ST_CoreDIGIT,2);
        for (c = '0'; c <= '9'; c++) 
                {
                AddTransition(ST_Period,c,ST_CoreDIGIT,false);
                }
        AddTransition(ST_CoreDIGIT,'.',ST_CoreDIGIT,false);
        AddTransition(ST_CoreDIGIT,'E',ST_Exponent,false);
        AddTransition(ST_CoreDIGIT,'e',ST_Exponent,false);
        AddTransition(ST_Exponent,'+',ST_CoreDIGIT,false);
        AddTransition(ST_Exponent,'-',ST_CoreDIGIT,false);
        // Literals X''), and UX''
        AssignStateTable(ST_X,ST_CoreCHAR,1);
        AssignStateTable(ST_U,ST_CoreCHAR,2);
        AddTransition(ST_CoreALL,'U',ST_U,true);                    // U
        AddTransition(ST_CoreALL,'u',ST_U,true);
        AddTransition(ST_CoreCHAR,'U',ST_CoreCHAR,false);
        AddTransition(ST_CoreCHAR,'u',ST_CoreCHAR,false);
        AddTransition(ST_CoreALL,'X',ST_X,true);                    // X
        AddTransition(ST_CoreALL,'x',ST_X,true);
        AddTransition(ST_CoreCHAR,'X',ST_CoreCHAR,false);
        AddTransition(ST_CoreCHAR,'x',ST_CoreCHAR,false);
        AddTransition(ST_U,'X',ST_X,false);                         // UX
        AddTransition(ST_U,'x',ST_X,false);
        AddTransition(ST_X,'\'',ST_InLiteral,false);                // X'
        // DoubleQuote (Accept String) - Two quotes "" remains in string..
        AssignStateDefault(ST_InString,ST_InString,1);
        AssignStateTable(ST_InStringDoubleQuote,ST_CoreSYMBOL,1);
        AddTransition(ST_CoreALL,'"',ST_InString,true);
        AddTransition(ST_InString,'"',ST_InStringDoubleQuote,false);
        AddTransition(ST_InStringDoubleQuote,'"',ST_InString,false);
        // Tokens with Digits
        for (c = '0'; c <= '9'; c++) 
                {
                AddTransition(ST_CoreCHAR,c,ST_CoreCHAR,false);
                }
        // Comment /* C-Type */
        AssignStateDefault(ST_InComment,ST_InComment,2);
        AssignStateDefault(ST_CommentEnd,ST_InComment,1);
        AssignStateTable(ST_Comment,ST_CoreSYMBOL,0);
        AddTransition(ST_Slash,'*',ST_InComment,false);
        AddTransition(ST_InComment,'*',ST_CommentEnd,false);
        AddTransition(ST_CommentEnd,'/',ST_Comment,false);
        // Line Comments
        AssignStateDefault(ST_LineComment,ST_LineComment,2);
        AddTransition(ST_Minus,'-',ST_LineComment,false);           // --
        AddTransition(ST_LineComment,'\n',ST_CoreSPACE,true);
        // Hints (%_Token --> %_HINTS)
        AddTransition(ST_Percent,'_',ST_CoreCHAR,false);            // %_
        // -------------------------------------------------------------
        // Special: DBSL Hints in Comments (/* %/ %_HINTS ... /% */)
  #if HintsInComments
        AssignStateDefault(ST_InCommentPercent,ST_InComment,1);
        //          Alternate Comment End: %/
        AddTransition(ST_InComment,'%',ST_InCommentPercent,false);
        AddTransition(ST_InCommentPercent,'/',ST_Comment,false);
        //          Alternate Comment Start: /%
        AddTransition(ST_Slash,'%',ST_InComment,false);
  #endif
        // -------------------------------------------------------------
        // Special: Hints in Line Comments (-- ... %_HINTS)
  #if HintsInLineComments
        AddTransition(ST_LineComment,'%',ST_LineCommentPercent,true);
        AssignStateDefault(ST_LineCommentPercent,ST_LineComment,1);
        AddTransition(ST_LineCommentPercent,'_',ST_CoreCHAR,false);
  #endif
        // -------------------------------------------------------------
        // Special: ABAP Exec-SQL Variables (:Variable :Struct-Member)
        AssignStateDefault(ST_ExecSQLVar,ST_ExecSQLVar,6);
        AddTransition(ST_CoreALL,':',ST_ExecSQLVar,true);           // :
        AddTransition(ST_CoreCHAR,':',ST_CoreCHAR,false);
        AddTransition(ST_ExecSQLVar,'-',ST_ExecSQLVar,false);
        AddTransition(ST_ExecSQLVar,',',ST_Comma,true);
        AddTransition(ST_ExecSQLVar,')',ST_ParenClose,true);
        AddTransition(ST_ExecSQLVar,' ',ST_CoreSPACE,true);
        AddTransition(ST_ExecSQLVar,'\t',ST_CoreSPACE,true);
        AddTransition(ST_ExecSQLVar,'\n',ST_CoreSPACE,true);
        return;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
