// LexicalParser.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: LexicalParser.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1998 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Lexical_Parser

        #define __Lexical_Parser

#include <stdlib.h>

#define LexNumSymbols 256

typedef unsigned char SymbolState_t;

#define NumCoreStates 5
#define FirstUserState (NumCoreStates)

#define ST_CoreERROR 0xFF
#define ST_CoreALL 0x7F

#define ST_CoreSTART 0
#define ST_CoreSPACE 1                  // Space sequence
#define ST_CoreCHAR 2                   // Character sequence
#define ST_CoreDIGIT 3                  // Digit sequence
#define ST_CoreSYMBOL 4                 // Single symbol

#define StateACCEPTMask 0x80            // Accept the <PREVIOUS> State
#define StateSTATEMask 0x7F
#define StateACCEPT(State) ((SymbolState_t)((State) & StateACCEPTMask))
#define StateSTATE(State) ((SymbolState_t)((State) & StateSTATEMask))

#define WideSubstitutionSYMBOL '_'      // Default for wchar_t -> char.

// ****************************************************************************
// **** Symbol State Table ****************************************************
// ****************************************************************************

struct TransitionOverride_t
        {
        char Symbol;
        SymbolState_t NewState;
        };

struct SymbolStateTableInfo
        {
        SymbolState_t *CoreDefault;
        SymbolState_t StateDefault;
        unsigned MaxOverrides;
        unsigned NumOverrides;
        TransitionOverride_t *Overrides;
        SymbolStateTableInfo();
        ~SymbolStateTableInfo();
        bool AssignStateDefault(SymbolState_t Default, size_t nOverrides);
        bool AssignStateTable(SymbolState_t *Core, size_t nOverrides);
        bool AddTransition(char Symbol, SymbolState_t NewState, bool Accept);
        };

// ****************************************************************************
// **** Symbol State Machine **************************************************
// ****************************************************************************

class SymbolStateMachine
        {
        private:
                unsigned NumStates;
                SymbolStateTableInfo *StateTables;
                SymbolState_t CoreStateTables[NumCoreStates][LexNumSymbols];
        protected:
                void BuildCoreTables();
        public:
                SymbolStateMachine(unsigned NumUserStates);
                ~SymbolStateMachine();
                bool AssignStateDefault(SymbolState_t State,
                                        SymbolState_t StateDefault,
                                        size_t nOverrides);
                bool AssignStateTable(SymbolState_t State,
                                      SymbolState_t CoreDefault,
                                      size_t nOverrides);
                bool AddTransition(SymbolState_t CurrentState,
                                   char Symbol,
                                   SymbolState_t NewState,
                                   bool Accept);
                SymbolStateTableInfo *GetStateTable(unsigned State)
                        {
                        return &StateTables[State];
                        }
        };

// ****************************************************************************
// **** Lexical Parser ********************************************************
// ****************************************************************************

class LexicalParser
        {
        private:
                size_t Count;
                const void *Buffer;
                SymbolState_t CurrentState;
                char WideSubstitutionSymbol;
        protected:
                SymbolStateMachine &Machine;
                virtual void OnAccept(SymbolState_t State,
                                      const void *Buffer,
                                      size_t Count);
        public:
                LexicalParser(SymbolStateMachine &Machine);
                bool DecodeSymbol(char Symbol);
                bool DecodeSymbol(wchar_t wSymbol);
                SymbolState_t GetCurrentState() {return CurrentState;}
                void Reset();
                void Reset(SymbolState_t StartState);
                void SetBuffer(const void *Buffer);
                void SetWideSubstitutionSymbol(char Symbol);
        };

// ****************************************************************************
// **** Lexical Stream ********************************************************
// ****************************************************************************

enum SymbolClass_t
{
        SC_Unknown, SC_Null, SC_Space,
        SC_Token, SC_Number, SC_Symbol
};

struct LexicalItem
        {
        SymbolClass_t SymbolClass;
        SymbolState_t SymbolID;
        const char *String;             // Begining position in input stream.
        size_t Length;                  // Length of String.
        };

class LexicalStream : public LexicalParser
        {
        protected:
                bool Wide;
                bool Continue;
                LexicalItem *Item;
                size_t InputLength;
                const char *InputString;
                virtual void OnAccept(SymbolState_t State,
                                      const void *Buffer,
                                      size_t Count);
        public:
                LexicalStream(SymbolStateMachine &Machine);
                const char* NextItem(LexicalItem *Item);
                void Restart(const char *Input, size_t Length);
                void Restart(const wchar_t *Input, size_t Length);
                void Start(const char *Input, size_t Length);
                void Start(const wchar_t *Input, size_t Length);
        };

/* **************************************************************************
 ****************************************************************************

 Notes:

             I. Symbol State Machine:

                     A. Description:

                        Lexical Parser and Lexical Stream use the transition
                        information contained in a SymbolStateMachine.

                     B. Defining:

                        The default configuration of the SymbolStateMachine
                        tokenizes sequences of characters, sequences of spaces
                        and sequences of digits. Other symbols are tokenized
                        indivitually.

                        To define new token sequences, a new state must be
                        defined for each step of a token sequence.

                        For example '==' requires two new states, ST_Equal
                        and ST_EqualEqual.

                             #define ST_Equal (FirstUserState+0)
                             #define ST_EqualEqual (FirstUserState+1)

                        For each new state, a default transition table or 
                        default value is assigned:

                             AssignStateTable(ST_Equal,ST_CoreSYMBOL,1);
                             AssignStateTable(ST_EqualEqual,ST_CoreSYMBOL,0);

                        Then the "overridden" transitions are inserted:

                             AddTransition(ST_CoreALL,'=',ST_Equal,true);
                             AddTransition(ST_Equal,'=',ST_EqualEqual,false);

                        ----------

                        Single symbols can also be assigned single states
                        (e.g. for '+') to make identification easier:

                            Before:

                                PARSER: OnAccept(ST_CoreSYMBOL,"+",1)

                            After:

                                #define ST_Plus (FirstUserState+9)
                                AssignStateTable(ST_Plus,ST_CoreSYMBOL,0);
                                AddTransition(ST_CoreALL,'+',ST_Plus,true);

                                PARSER: OnAccept(ST_Plus,"+",1)

                     C. Methods:

                           - SymbolStateMachine(NumUserStates)  [Constructor]

                                'NumUserStates' - Number of additional
                                                  state tables to be
                                                  added via:

                                                  AssignStateDefault()
                                                  AssignStateTable()

                           - AssignStateDefault(SymbolState_t State,
                                                SymbolState_t StateDefault,
                                                size_t nOverrides);

                                'State' - New state number.

                                'StateDefault' - All symbol inputs result in 
                                                 this state (except '\0')
                                                 unless explicitly overridden.

                                                 An input of '\0' results
                                                 in an ACCEPTing ST_CoreSTART
                                                 unless explicitly overridden.

                                          To have the default ACCEPT the
                                          previous token, combine the default
                                          state number with:

                                                StateACCEPTMask

                                          Example: ST_Equal
                                                   ST_Equal | StateACCEPTMask

                                'nOverrides' - Number of transitions that will
                                               be defined that are different
                                               than the 'CoreDefault' table.

                           - AssignStateTable(SymbolState_t State,
                                              SymbolState_t CoreDefault,
                                              size_t nOverrides)

                                        Inserts a new state into the state
                                        machine.

                                'State' - New state number.

                                'CoreDefault' - Base core state whoes state
                                                table is used for input that
                                                is not explicitly overridden.

                                'nOverrides' - Number of transitions that will
                                               be defined that are different
                                               than the 'CoreDefault' table.

                           - AddTransition(SymbolState_t CurrentState,
                                           char Symbol,
                                           SymbolState_t NewState,
                                           bool Accept)

                                        Insert a new transition rule from
                                        'CurrentState' to 'NewState' when
                                        encountering 'Symbol'.

                                'CurrentState' - State of machine used to
                                                 select this table.

                                'Symbol' - Input to trigger on.

                                'NewState' - Machine enters this state.

                                'Accept' - Machine accepts <Previous> token.

            II. Lexical Parser:

                     A. Description:

                        A lexical parser that provides tokenized output.

                        Each symbol is passed into the Parser via
                        Decode() and the virtual method OnAccept() is
                        invoked for each token.

                     C. Callback @ Accept:

                           - OnAccept(State,Buffer,Count)

                                Called when the input token is accepted.

                                'State' - The "type" of token based on
                                          the last decision of the state
                                          machine.

                                'Buffer' - Pointer set with SetBuffer()

                                'Count' - Character length of 'Bufer' in
                                          the token.

                     C. Methods:

                           - LexicalParser(SymbolStateMachine)  [Constructor]

                                'SymbolStateMachine' - State tables defining
                                                       how the input is
                                                       tokenized.

                           - DecodeSymbol(char Symbol)

                                        Call for each 'Symbol' to input.

                                        Returns 'false' if the 'Symbol' was
                                        rejected. Use GetCurrentState() to
                                        determine the current machine state.

                           - DecodeSymbol(wchar_t Symbol)

                                        Uses the low-order portion of the
                                        symbol. If the symbol contains a
                                        high-order portion not zero, the
                                        'WideSubstitutionSymbol' is used
                                        as deciding input.

                           - GetCurrentState()

                                        Returns the state of the machine.

                           - Reset()

                                        Reset the machine to parse new input.

                           - Reset(SymbolState_t StartState)

                                        Set or Reset the machine to parse new 
                                        input using 'StartState' instead of
                                        ST_CoreSTART.

                           - SetBuffer(const void *buffer)

                                        Assign a 'Buffer' to be passed back
                                        to OnAccept(). Typically, the buffer
                                        is set after each accepted token.

                           - SetWideSubstitutionSymbol(char Symbol)

                                        Character to use for decoding
                                        decisions when DecodeSymbol(wchar_t)
                                        detects input with a high-order
                                        portion not equal to zero.

           III. Lexical Stream:

                     A. Description:

                        Based on the Lexical Parser, a Lexical Stream
                        reads input from a string residing in memory and
                        does not return until a token is accepted or the
                        input is exhausted.

                     B. Use:

                        For each input sequence:

                             1. Call Start() to set the input 'String'.
                             2. Call NextItem() to return one token
                                at a time.
                             3. If the input is exhausted before a token
                                is complete, NextItem() returns NULL.

                             Restart:

                             4. Call Restart() to set the next input.

                             5. Goto step 2, but pass in the SAME 'Item'
                                as had been passed in step 3 before.

                                Note that the result of the next successful
                                NextItem(); 'String' will point to the
                                beginning of the first input, and the length
                                will reflect the length sum of ALL inputs.

                     C. Methods:

                           - LexicalStream(SymbolStateMachine)  [Constructor]

                                'SymbolStateMachine' - State tables defining
                                                       how the input is
                                                       tokenized.

                           - NextItem(LexicalItem *Item)

                                Returns for each token parsed, the information
                                is stored in 'Item'. If NextItem() returns
                                NULL, the input is exhausted (see above for
                                how to continue).

                                The returned 'Item' contains:

                                      - SymbolClass : Value based on 
                                                      CoreStates:

                                                ST_CoreSTART:  SC_Null
                                                ST_CoreSPACE:  SC_Space
                                                ST_CoreCHAR:   SC_Token
                                                ST_CoreDIGIT:  SC_Number
                                                ST_CoreSYMBOL: SC_Symbol
                                                (All Others):  SC_Unknown: 


                                      - SymbolID : Ending state that 
                                                   accepted token.

                                      - String : Pointer to begining 
                                                 of token.

                                      - Length : Length of token.

                                Example: For Plus (+):

                                        Item.SymbolClass = SC_Symbol 
                                        Item.SymbolID = ST_CoreSYMBOL
                                        Item.String = "+"
                                        Item.Length = 1

                                Example: For EqualEqual (==):

                                        Item.SymbolClass = SC_Unknown 
                                        Item.SymbolID = ST_EqualEqual
                                        Item.String = "=="
                                        Item.Length = 2

                                If the input is UNICODE (i.e. Start() for
                                wchar_t input was called), then both the
                                return of Next() and 'Item->String' can
                                be cast to (wchar_t*).

                           - Reset(SymbolState_t StartState)  [LexicalParser]

                                        Start parsing at 'StartState' instead
                                        of ST_CoreSTART. It is only valid
                                        to call this immediately after
                                        calling Start(..).

                           - Restart(const char *Input, size_t Length)
                           - Restart(const wchar_t *Input, size_t Length)

                                 Set continuation of the input stream. Call
                                 when NextItem() returns NULL, and more
                                 input is available.

                                 Can also be used to set the beginning of
                                 the input stream if:

                                      - Start((char *)NULL,0)
                                      
                                 was called previously.

                           - Start(const char *Input, size_t Length)

                                 Set beginining of an character or wide
                                 character input stream and prepares the
                                 state machine for new input.

                           - Start(const wchar_t *Input, size_t Length)

                                 Set beginining of a UNICODE input stream and
                                 prepares the state machine for new input.

                                 Following calls to NextItem() assume the
                                 input is UNICODE (see DecodeSymbol() above).
                                 
 ****************************************************************************
 **** Example ***************************************************************
 *************


 ****************************************************************************

#include <stdio.h>
#include <string.h>
#include <LexicalParser.h>
 
class CPPSymbolStateMachine : public SymbolStateMachine
        {
        public:
                CPPSymbolStateMachine();
        };

// ******************************************************************
// **** C++ Lexical States ******************************************
// ******************************************************************

#define CPPNumUserStates 51

#define ST_Underscore                   ST_CoreCHAR
#define ST_Colon                        (FirstUserState+0)
#define ST_DoubleColon                  (FirstUserState+1)
#define ST_Period                       (FirstUserState+2)
#define ST_PeriodMultiply               (FirstUserState+3)
#define ST_Minus                        (FirstUserState+4)
#define ST_MinusMinus                   (FirstUserState+5)
#define ST_MinusEqual                   (FirstUserState+6)
#define ST_MinusGreater                 (FirstUserState+7)
#define ST_MinusGreaterMultiply         (FirstUserState+8)
#define ST_Plus                         (FirstUserState+9)
#define ST_PlusPlus                     (FirstUserState+10)
#define ST_PlusEqual                    (FirstUserState+11)
#define ST_Slash                        (FirstUserState+12)
#define ST_SlashSlash                   (FirstUserState+13)
#define ST_SlashEqual                   (FirstUserState+14)
#define ST_SlashMultiply                (FirstUserState+15)
#define ST_Multiply                     (FirstUserState+16)
#define ST_MultiplyEqual                (FirstUserState+17)
#define ST_MultiplySlash                (FirstUserState+18)
#define ST_Equal                        (FirstUserState+19)
#define ST_EqualEqual                   (FirstUserState+20)
#define ST_Bang                         (FirstUserState+21)
#define ST_BangEqual                    (FirstUserState+22)
#define ST_Or                           (FirstUserState+23)
#define ST_OrOr                         (FirstUserState+24)
#define ST_OrEqual                      (FirstUserState+25)
#define ST_And                          (FirstUserState+26)
#define ST_AndAnd                       (FirstUserState+27)
#define ST_AndEqual                     (FirstUserState+28)
#define ST_Percent                      (FirstUserState+29)
#define ST_PercentEqual                 (FirstUserState+30)
#define ST_Caret                        (FirstUserState+31)
#define ST_CaretEqual                   (FirstUserState+32)
#define ST_Less                         (FirstUserState+33)
#define ST_LessEqual                    (FirstUserState+34)
#define ST_Greater                      (FirstUserState+35)
#define ST_GreaterEqual                 (FirstUserState+36)
#define ST_BackSlash                    (FirstUserState+37)
#define ST_BackSlashBackSlash           (FirstUserState+38)
#define ST_Quote                        (FirstUserState+39)
#define ST_DoubleQuote                  (FirstUserState+40)
#define ST_Comma                        (FirstUserState+41)
#define ST_ParenOpen                    (FirstUserState+42)
#define ST_ParenClose                   (FirstUserState+43)
#define ST_QuestionMark                 (FirstUserState+44)

// **************************
// **** Numeric Literals ****
// **************************

#define ST_Zero                         (FirstUserState+44)
#define ST_Hex                          (FirstUserState+45)
#define ST_Octal                        (FirstUserState+46)

// ****************************************
// **** Strings, Literals and Comments ****
// ****************************************

#define ST_InLiteral                    ST_Quote
#define ST_Literal                      (FirstUserState+47)
#define ST_InString                     ST_DoubleQuote
#define ST_String                       (FirstUserState+48)
#define ST_LineComment                  ST_SlashSlash
#define ST_InComment                    ST_SlashMultiply
#define ST_CommentEnd                   ST_MultiplySlash
#define ST_Comment                      (FirstUserState+49)

// ************************
// **** Floating point ****
// ************************

#define ST_Exponent                     (FirstUserState+50)

// ******************************************************************
// **** C++ Lexical Stream ******************************************
// ******************************************************************

CPPSymbolStateMachine::CPPSymbolStateMachine()
        :
        SymbolStateMachine(CPPNumUserStates)
        {
        char c;
        // Comma
        AssignStateTable(ST_Comma,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,',',ST_Comma,true);                    // ,
        // Underscore (is a character)
        AddTransition(ST_CoreALL,'_',ST_Underscore,true);               // _
        AddTransition(ST_CoreCHAR,'_',ST_Underscore,false);
        // Colon
        AssignStateTable(ST_Colon,ST_CoreSYMBOL,1);
        AddTransition(ST_CoreALL,':',ST_Colon,true);                    // :
        AddTransition(ST_Colon,':',ST_DoubleColon,false);               // ::
        // Period
        AssignStateTable(ST_Period,ST_CoreSYMBOL,11);   // pointer, float
        AssignStateTable(ST_PeriodMultiply,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'.',ST_Period,true);                   // .
        AddTransition(ST_Period,'*',ST_PeriodMultiply,false);           // .*
        // Minus
        AssignStateTable(ST_Minus,ST_CoreSYMBOL,3);                     // op
        AssignStateTable(ST_MinusMinus,ST_CoreSYMBOL,0);
        AssignStateTable(ST_MinusEqual,ST_CoreSYMBOL,0);
        AssignStateTable(ST_MinusGreater,ST_CoreSYMBOL,1);
        AssignStateTable(ST_MinusGreaterMultiply,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'-',ST_Minus,true);                    // -
        AddTransition(ST_Minus,'-',ST_MinusMinus,false);                // --
        AddTransition(ST_Minus,'=',ST_MinusEqual,false);                // -=
        AddTransition(ST_Minus,'>',ST_MinusGreater,false);              // ->
        AddTransition(ST_MinusGreater,'*',ST_MinusGreaterMultiply,false);
        // Plus
        AssignStateTable(ST_Plus,ST_CoreSYMBOL,2);
        AssignStateTable(ST_PlusPlus,ST_CoreSYMBOL,0);
        AssignStateTable(ST_PlusEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'+',ST_Plus,true);                     // +
        AddTransition(ST_Plus,'+',ST_PlusPlus,false);                   // ++
        AddTransition(ST_Plus,'+',ST_PlusEqual,false);                  // +=
        // Slash
        AssignStateTable(ST_Slash,ST_CoreSYMBOL,3);      // +2 Comments
        AssignStateTable(ST_SlashEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'/',ST_Slash,true);                    // /
        AddTransition(ST_Slash,'=',ST_SlashEqual,false);                // /=
        // Multiply
        AssignStateTable(ST_Multiply,ST_CoreSYMBOL,1);
        AddTransition(ST_CoreALL,'*',ST_Multiply,true);                 // *
        AddTransition(ST_Multiply,'=',ST_MultiplyEqual,false);          // *=
        // Equal
        AssignStateTable(ST_Equal,ST_CoreSYMBOL,1);
        AssignStateTable(ST_EqualEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'=',ST_Equal,true);                    // =
        AddTransition(ST_Equal,'=',ST_EqualEqual,false);                // ==
        // Bang
        AssignStateTable(ST_Bang,ST_CoreSYMBOL,1);
        AssignStateTable(ST_BangEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'!',ST_Bang,true);                     // !
        AddTransition(ST_Bang,'=',ST_BangEqual,false);                  // !=
        // Or
        AssignStateTable(ST_Or,ST_CoreSYMBOL,2);
        AssignStateTable(ST_OrOr,ST_CoreSYMBOL,0);
        AssignStateTable(ST_OrEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'|',ST_Or,true);                       // |
        AddTransition(ST_Or,'|',ST_OrOr,false);                         // ||
        AddTransition(ST_Or,'=',ST_OrEqual,false);                      // |=
        // And
        AssignStateTable(ST_And,ST_CoreSYMBOL,2);
        AssignStateTable(ST_AndAnd,ST_CoreSYMBOL,0);
        AssignStateTable(ST_AndEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'&',ST_And,true);                      // &
        AddTransition(ST_And,'&',ST_AndAnd,false);                      // &&
        AddTransition(ST_And,'=',ST_AndEqual,false);                    // &=
        // Percent
        AssignStateTable(ST_Percent,ST_CoreSYMBOL,1);
        AssignStateTable(ST_PercentEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'%',ST_Percent,true);                  // %
        AddTransition(ST_Percent,'=',ST_PercentEqual,false);            // %=
        // Caret
        AssignStateTable(ST_Caret,ST_CoreSYMBOL,1);
        AssignStateTable(ST_CaretEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'^',ST_Caret,true);                    // ^
        AddTransition(ST_Caret,'=',ST_CaretEqual,false);                // ^=
        // Less
        AssignStateTable(ST_Less,ST_CoreSYMBOL,1);
        AssignStateTable(ST_LessEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'<',ST_Less,true);                     // <
        AddTransition(ST_Less,'=',ST_LessEqual,false);                  // <=
        // Greater
        AssignStateTable(ST_Greater,ST_CoreSYMBOL,1);
        AssignStateTable(ST_GreaterEqual,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'>',ST_Greater,true);                  // >
        AddTransition(ST_Greater,'=',ST_GreaterEqual,false);            // >=
        // BackSlash
        AssignStateTable(ST_BackSlash,ST_CoreSYMBOL,1);
        AssignStateTable(ST_BackSlashBackSlash,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'\\',ST_BackSlash,true);
        AddTransition(ST_BackSlash,'\\',ST_BackSlashBackSlash,false);
        // Parenthesis
        AssignStateTable(ST_ParenOpen,ST_CoreSYMBOL,0);                 // (
        AddTransition(ST_CoreALL,'(',ST_ParenOpen,true);
        AssignStateTable(ST_ParenClose,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,')',ST_ParenClose,true);               // )
        // QuestionMark
        AssignStateTable(ST_QuestionMark,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'?',ST_QuestionMark,true);             // ?
        // Quote (Accept Literal)
        AssignStateDefault(ST_InLiteral,ST_InLiteral,1);
        AssignStateTable(ST_Literal,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'\'',ST_InLiteral,true);
        AddTransition(ST_InLiteral,'\'',ST_Literal,false);
        // DoubleQuote (Accept String)
        AssignStateDefault(ST_InString,ST_InString,1);
        AssignStateTable(ST_String,ST_CoreSYMBOL,0);
        AddTransition(ST_CoreALL,'"',ST_InString,true);
        AddTransition(ST_InString,'"',ST_String,false);
        // Floating point numbers
        AssignStateTable(ST_Exponent,ST_CoreDIGIT,2);
        for (c='0';c<='9';c++) AddTransition(ST_Period,c,ST_CoreDIGIT,false);
        AddTransition(ST_CoreDIGIT,'.',ST_CoreDIGIT,false);
        AddTransition(ST_CoreDIGIT,'E',ST_Exponent,false);
        AddTransition(ST_Exponent,'+',ST_CoreDIGIT,false);
        AddTransition(ST_Exponent,'-',ST_CoreDIGIT,false);
        // Tokens with Digits
        for (c='0';c<='9';c++) AddTransition(ST_CoreCHAR,c,ST_CoreCHAR,false);
        // Decimal, Hex, and Octal Literals
        AssignStateTable(ST_Zero,ST_CoreDIGIT,12);
        AssignStateTable(ST_Hex,ST_CoreDIGIT,16);
        AssignStateTable(ST_Octal,ST_CoreDIGIT,10);
        AddTransition(ST_CoreSTART,'0',ST_Zero,true);
        AddTransition(ST_CoreSPACE,'0',ST_Zero,true);
        AddTransition(ST_Zero,'x',ST_Hex,false);
        AddTransition(ST_Zero,'X',ST_Hex,false);
        for (c='0';c<='7';c++) AddTransition(ST_Zero,c,ST_Octal,false);
        for (c='8';c<='9';c++) AddTransition(ST_Zero,c,ST_CoreERROR,false);
        for (c='0';c<='9';c++) AddTransition(ST_Hex,c,ST_Hex,false);
        for (c='A';c<='F';c++) AddTransition(ST_Hex,c,ST_Hex,false);
        for (c='0';c<='7';c++) AddTransition(ST_Octal,c,ST_Octal,false);
        for (c='8';c<='9';c++) AddTransition(ST_Octal,c,ST_CoreERROR,false);
        // Comment
        AssignStateDefault(ST_InComment,ST_InComment,1);
        AssignStateDefault(ST_CommentEnd,ST_InComment,1);
        AssignStateTable(ST_Comment,ST_CoreSYMBOL,0);
        AddTransition(ST_Slash,'*',ST_InComment,false);
        AddTransition(ST_InComment,'*',ST_CommentEnd,false);
        AddTransition(ST_CommentEnd,'/',ST_Comment,false);
        // Line Comments
        AssignStateDefault(ST_LineComment,ST_LineComment,2);
        AddTransition(ST_Slash,'/',ST_LineComment,false);               // //
        AddTransition(ST_LineComment,'\n',ST_CoreSYMBOL,true);
        AddTransition(ST_LineComment,'\0',ST_CoreSTART,true);
        return;
        }

// ******************************************************************
// **** Main ********************************************************
// ******************************************************************

main()
        {
        CPPSymbolStateMachine StateMachine;
        LexicalStream Lex(StateMachine);
        const char* Pos = "void Test(int x, char y);";
        Lex.Start(Pos,strlen(Pos)+1);
        while (*Pos)
                {
                LexicalItem Item;
                Pos = Lex.NextItem(&Item);
                if (!Pos)
                        {
                        printf("ERROR: Unexpected End of String\n");
                        return 1;
                        }
                switch (Item.SymbolClass)
                        {
                        case SC_Space:  printf("Space:  "); break;
                        case SC_Token:  printf("Token:  "); break;
                        case SC_Number: printf("Number: "); break;
                        case SC_Symbol: printf("Symbol: "); break;
                        default:        printf("Other:  "); break;
                        }
                printf("[%u]: ",Item.SymbolID);
                printf("'%.*s'\n",Item.Length,Item.String);
                }
        return 0;
        }

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __Lexical_Parser */

