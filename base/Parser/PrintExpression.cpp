// Print Expression of Nodes containing LexicalItems PrintExpression.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: PrintExpression.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2005 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdio.h>
#include <Expression.h>
#include <LexicalParser.h>

static void PrintTerminal(unsigned Tab, const char *String, size_t Length)
        {
        printf("[");
        while (Length)
                {
                size_t EOLPos;                
                for (EOLPos=0; EOLPos < Length; EOLPos++)
                        {
                        if (String[EOLPos] == '\n') break;
                        }
                if (EOLPos) printf("%.*s",(int)EOLPos,String);
                Length -= EOLPos;
                String += EOLPos;
                if (Length)
                        {
                        printf("\n");
                        if (Tab) printf("%-*c",Tab-1,' ');
                        Length--;
                        String++;
                        if (Length) printf(" "); // Bracket placeholder.
                        }
                }
        printf("]");
        return;
        }

static void _PrintExpression(Node *Current, bool Flat)
	{
        bool Branch = true;
        unsigned Level = 0;
        unsigned FlatLevel = 0;
        while (Current)
                {
                if (Flat)               // Break line if node has leaves.
                        {
                        unsigned IndexInParent = Current->GetIndexInParent();
                        if (IndexInParent != NOTACHILD_NODE)
                                {
                                Node *Parent = Current->GetParent();
                                Branch = IndexInParent || Parent->GetChild(1);
                                }
                        else Branch = true;
                        }
                if (!Branch && Current->isNonTerminal())
                        {
                        printf(".");
                        FlatLevel++;
                        }
                else    {
                        printf("\n");
                        unsigned Tab = Level - FlatLevel;
                        if (Tab) printf("%-*c",Tab-1,' ');
                        FlatLevel = 0;
                        }
        	if (Current->isNonTerminal()) printf("%s",Current->GetName());
        	else    {
                        TerminalNode *Node = (TerminalNode *)Current;
                        LexicalItem *Item = (LexicalItem *)Node->GetValue();
                        if (Item->SymbolClass == SC_Null) printf("EOL");
                        else    {
                                unsigned Tab = Level - FlatLevel;
                                PrintTerminal(Tab,Item->String,Item->Length);
                                }
                        }
                Current = SequentialGetNext(Current,Level);
                }
        printf("\n");
	return;
	}


void PrintExpression(Node *Expression)
        {
        _PrintExpression(Expression,false);
        return;
        }

void PrintExpressionFlat(Node *Expression)
        {
        _PrintExpression(Expression,true);
        return;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************

