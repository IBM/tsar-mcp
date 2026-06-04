// JSON Language Parser: JSONParser.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParser.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2016 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//

#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include <JSONParser.h>

#if defined(ExCharacterUNICODE)
        #define KeywordCmp wcsnicmp
#else
        #define KeywordCmp strnicmp
#endif

// ****************************************************************************
// **** Expression Parser w/ Partial Expression Return ************************
// ****************************************************************************

class JSONExpressionSN : public ExpressionSN
        {
        public:
                JSONExpressionSN(StateMachine &machine);
                JSONExpressionSN(StateMachine &machine,
                               Node *StartNode,
                               const char *NonTerminalGoal);
        };

JSONExpressionSN::JSONExpressionSN(StateMachine &machine)
        :
        ExpressionSN(machine)
        {
        return;
        }

JSONExpressionSN::JSONExpressionSN(StateMachine &machine,
                               Node *StartNode,
                               const char *NonTerminalGoal)
        :
        ExpressionSN(machine,StartNode,NonTerminalGoal)
        {
        return;
        }

// ****************************************************************************
// **** Throwable Parse Error *************************************************
// ****************************************************************************

Error_ParseJSON::Error_ParseJSON(const char *Text,
                                 JSONLexicalItem *item,
                                 JSONExpressionSN *Parser)
        :
        ErrorBase(Text)
        {
        ItemInit = item != NULL;
        if (ItemInit) Item = *item;
        ParserState = Parser ? Parser->GetCurrentState() : Error_NOSTATE;
        return;
        }

// ****************************************************************************
// **** Debugging Procedures **************************************************
// ****************************************************************************

static void (*WatchDataTransition)(Transition*, inputT) = NULL;

static void (*WatchNodeTransition)(Transition*, Node*) = NULL;

void ParseJSONBuffer_SetWatch(void (*fn)(Transition*, inputT))
        {
        WatchDataTransition = fn;
        return;
        }

void ParseJSONBuffer_SetWatch(void (*fn)(Transition*, Node*))
        {
        WatchNodeTransition = fn;
        return;
        }

// ****************************************************************************
// **** Parse JSON Buffer *******************************************************
// ****************************************************************************

Node* ParseJSONBuffer(StateMachine &LRMachine,
                      JSONSymbolStateMachine &LexMachine,
                      const char *JSONBuffer,
                      bool AllowHTJSONSingletons,
                      Node *StartNode,
                      const char *NonTerminalGoal)
	{
        LexicalStream Lex(LexMachine);
	JSONExpressionSN Parser(LRMachine,StartNode,NonTerminalGoal);

        if (WatchDataTransition) Parser.SetWatch(WatchDataTransition);
        if (WatchNodeTransition) Parser.SetWatch(WatchNodeTransition);

        JSONLexicalItem *Item = NULL;
        parseStatus Status = pS_Good;
        const char* Pos = JSONBuffer;
        Lex.Start(Pos,strlen(Pos)+1);
        while (Status == pS_Good)
                {
                Item = new JSONLexicalItem;
                if (!Item)
                        {
                        throw Error_ParseJSON("ERROR: Memory: JSONLexicalItem");
                        }
                Pos = Lex.NextItem(Item);
                if (!Pos)
                        {
                        Error_ParseJSON Err("ERROR: End Not Expected");
                        delete Item;
                        throw Err;
                        }
                bool rc = ClasifyParserNodeID(Item,AllowHTJSONSingletons);
                if (!rc)
                        {
                        Error_ParseJSON Err("ERROR: Unable to Classify",Item);
                        delete Item;
                        throw Err;
                        }
                if (Item->SymbolClass == SC_Space)       // Ignore items.
                        {
                        delete Item;
                        Item = NULL;
                        }
                else if (Item->SymbolClass == SC_Null)
                        {
                        Status = Parser.NextInputEOL(Item);
                        if (Status == pS_Good) Item = NULL;
                        }
                else    {
                        Status = Parser.NextInput(Item);
                        if (Status == pS_Good) Item = NULL;
                        }
		}
        if (Status == pS_SyntaxError)
                {
                Error_ParseJSON Err("ERROR: Syntax",Item,&Parser);
                delete Item;
                throw Err;
                }
        if (Item) delete Item;                          // End of Line.
	return Status == pS_Accepted ? Parser.AquireExpression() : NULL;
	}

// ***************************************************************************
// **** Print Parsed JSON Document *******************************************
// ***************************************************************************
                                        // ---------------------------------
struct PrintDocStateJSON                // EXACT DUPLICATE IN JSONParser.cpp
        {                               // ---------------------------------
        unsigned Indent;
        bool InText;                    // Try to keep text within tags.
        bool NewLine;
        bool FirstLine;
        bool Pretty;
        PrintJSONDocumentFn_t PrintFn;
        void *PrintUserPtr;
        int nPrintBytes;
        // *****************
        // **** Methods ****
        // *****************
        PrintDocStateJSON(PrintJSONDocumentFn_t PrintFn, 
                          void *PrintUserPtr,
                          bool Pretty);
        int printf(const char *Format, ...);
        int printIndent();
        int printNL();
        };

PrintDocStateJSON::PrintDocStateJSON(PrintJSONDocumentFn_t printFn,
                                     void *printUserPtr,
                                     bool pretty)
        {
        Indent = 0;
        InText = false;
        NewLine = false;
        FirstLine = true;
        PrintFn = printFn;
        PrintUserPtr = printUserPtr;
        Pretty = pretty;
        nPrintBytes = 0;
        return;
        }

int PrintDocStateJSON::printf(const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        int rc = PrintFn(PrintUserPtr,Format,Args);
        va_end(Args);
        if (rc > 0) nPrintBytes += rc;
        return rc;
        }

int PrintDocStateJSON::printIndent()
        {
        int rc = 0;
        if (Pretty && Indent) rc = printf("%*c",Indent,' ');
        return rc;
        }

int PrintDocStateJSON::printNL()
        {
        int rc = 0;
        if (Pretty) rc = printf("\n");
        return rc;
        }

static void PrintTextItem(PrintDocStateJSON &State,
                          const char *String,
                          size_t Length)
        {
        size_t i;
        for (i=0; i < Length && isspace(String[i]); i++);
        if (i == Length)
                {                              // Nothing to print.
                State.InText = false;          // No-longer in text.
                return;
                }
        if (State.NewLine)
                {
                if (!State.FirstLine) State.printNL();
                State.printIndent();
                State.NewLine = false;
                }
        State.FirstLine = false;
        while (Length)
                {
                size_t EOLPos;
                for (EOLPos=0; EOLPos < Length; EOLPos++)
                        {
                        if (String[EOLPos] == '\n') break;
                        }
                if (EOLPos) State.printf("%.*s",EOLPos,String);
                Length -= EOLPos;
                String += EOLPos;
                if (Length)
                        {
                        for (i=0; i < Length && isspace(String[i]); i++);
                        if (i == Length)
                                {                       // Nothing to print.
                                State.InText = false;   // No-longer in text.
                                String += Length;
                                Length = 0;
                                }
                        else    {
                                State.printNL();
                                State.printIndent();
                                Length--;
                                String++;
                                }
                        }
                }
        return;
        }

static void _PrintDocument(Node *Expression, PrintDocStateJSON &State)
	{
        unsigned TypeID = Expression->GetTypeID();
        switch (TypeID)
                {
                case Ew_JSONMemberID:
                        State.NewLine = true;
                        break;
                case Ew_OpenBraceID: 
                        State.Indent += 2;
                        State.NewLine = true;
                        break;
                case Ew_CloseBraceID: 
                        State.NewLine = true;
                        break;

                }
        if (Expression->isTerminal())
                {
                TerminalNode *Node = (TerminalNode *)Expression;
                LexicalItem *Item = (LexicalItem *)Node->GetValue();
                if (Item->SymbolClass != SC_Null)
                        {
                        PrintTextItem(State,Item->String,Item->Length);
                        }
                }
        for (unsigned iChild = 0; iChild < MAX_CHILDREN_NODES; iChild++)
                {
                Node *Child = Expression->GetChild(iChild);
                if (!Child) break;
                _PrintDocument(Child,State);
                }
        switch (TypeID)
                {
                case Ew_CloseBraceID: State.Indent -= 2;
                                      break;
                }
	return;
	}

int PrintJSONDocument(Node *Expression,                   // Returns number
                      PrintJSONDocumentFn_t PrintFn,      // of bytes output.
                      void *PrintUserPtr,
                      bool Pretty)
        {
        PrintDocStateJSON State(PrintFn,PrintUserPtr,Pretty);
        _PrintDocument(Expression,State);
        if (State.NewLine || State.InText) State.printNL();
        return State.nPrintBytes;
        }

int PrintJSONDocument(Node *Expression, bool Pretty)  // Returns bytes output.
        {
        PrintJSONDocumentFn_t PrintFn = (PrintJSONDocumentFn_t)vfprintf;
        void *PrintUserPtr = stdout;
        return PrintJSONDocument(Expression,PrintFn,PrintUserPtr,Pretty);
        }         
                  
// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************

