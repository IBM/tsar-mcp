// Find Node in Tree: FindNode.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: FindNode.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2006 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <LexicalParser.h>
#include <FindNode.h>

#if defined(ExCharacterUNICODE)
        #define TokenCmp wcsncmp
        #ifdef _WIN32
                #define TokenCaseCmp wcsnicmp
        #else
                #define TokenCaseCmp wcsncasecmp
        #endif
#else
        #define TokenCmp strncmp
        #ifdef _WIN32
                #define TokenCaseCmp strnicmp
        #else
                #define TokenCaseCmp strncasecmp
        #endif
#endif

struct FindList_t
        {
        unsigned TypeID;
        FindNodeCompareFn_t TermCompareFn;
        const void *TermCompareValue;
        Node *NodeMatched;                      // Filled in during search.
        unsigned MatchLevel;                    // Filled in during search.
        };

#define INCREMENT                                                            \
        {                                                                    \
        Look->MatchLevel = Level;                                            \
        Look->NodeMatched = Current;                                         \
        Cursor = Look + 1;                                                   \
        if (!Cursor->TypeID) return Current;                                 \
        if (Cursor->TypeID == WILDCARD_TypeID)                               \
                {                               /* Look up through search */ \
                unsigned ParentLevel = Level;   /* terms looking for the  */ \
                FindList_t *Parent = Look;      /* highest match-level    */ \
                while (Parent != &ToFind[0] &&                               \
                       Parent->TypeID != WILDCARD_TypeID)                    \
                        {                                                    \
                        ParentLevel = Parent->MatchLevel;                    \
                        Parent--;                                            \
                        }                                                    \
                if (Parent == &ToFind[0]) ParentLevel = Parent->MatchLevel;  \
                Cursor->MatchLevel = ParentLevel;                            \
                }                                                            \
        }

#define ROLLBACK                                                             \
        {                                                                    \
        while (Cursor != &ToFind[0] &&                                       \
               (Cursor->TypeID != WILDCARD_TypeID ||                         \
                Cursor->MatchLevel >= Level)) Cursor--;                      \
        }

#define WILDCARD_STEPBACK                                                    \
        {                                                                    \
        if (Cursor != &ToFind[0] && Cursor->TypeID == WILDCARD_TypeID)       \
                {                                                            \
                ROLLBACK;                                                    \
                }                                                            \
        }

static Node* _FindNode(Node *Current, FindList_t *ToFind)
        {
        unsigned Level = 0;
        FindList_t *Look;
        FindList_t *Cursor = &ToFind[0];
        Cursor->MatchLevel = Level;
        while (Current)
                {
                if (Cursor->TypeID == WILDCARD_TypeID)
                        {
                        Look = Cursor + 1;
                        if (!Look->TypeID) return Current; // Any match.
                        }
                else Look = Cursor;
                unsigned TypeID = Current->GetTypeID();
                if (TypeID == Look->TypeID)
                        {
                        bool Compare;
                        FindNodeCompareFn_t CmpFn = Look->TermCompareFn;
                        if (Current->isTerminal() && CmpFn)
                                {
                                TerminalNode *Term = (TerminalNode*)Current;
                                const void *Value = Look->TermCompareValue;
                                Compare = CmpFn(Term,Value);
                                if (!Compare) ROLLBACK;
                                }
                        else Compare = true;
                        if (Compare) INCREMENT;
                        }
                Current = SequentialGetNext(Current,Level);
                WILDCARD_STEPBACK;
                }
        return NULL;
        }

Node* FindNode(Node *Expression, unsigned nItems, ...)
        {
        va_list Args;
        if (!nItems) return Expression;
        size_t FindListSize = sizeof(FindList_t) * (nItems + 1);
        FindList_t *ToFind = (FindList_t *)malloc(FindListSize);
        if (!ToFind) return NULL;
        memset(ToFind,0,FindListSize);
        va_start(Args,nItems);
        for (unsigned i=0; i < nItems; i++)
                {
                ToFind[i].TypeID = va_arg(Args,int);
                ToFind[i].TermCompareFn = va_arg(Args,FindNodeCompareFn_t);
                ToFind[i].TermCompareValue = va_arg(Args,void*);
                }
        va_end(Args);
        ToFind[nItems].TypeID = UndefinedNodeType;      /* Zero (0) */
        Node *Found = _FindNode(Expression,ToFind);
        free(ToFind);
        return Found;
        }

// *************************************************************
// **** Compare Functions for Nodes containing LexicalItems ****
// *************************************************************

bool FN_LEX_strcmp(TerminalNode *ToCompare, const void *Value)
        {
        const char *String = (const char *)Value;
        LexicalItem *Item = (LexicalItem *)ToCompare->GetValue();
        const char *Token = Item->String;
        size_t TokenLength = Item->Length;
        bool Compare = TokenLength == strlen(String) &&
                       TokenCmp(Token,String,TokenLength) == 0;
        return Compare;
        }

bool FN_LEX_stricmp(TerminalNode *ToCompare, const void *Value)
        {
        const char *String = (const char *)Value;
        LexicalItem *Item = (LexicalItem *)ToCompare->GetValue();
        const char *Token = Item->String;
        size_t TokenLength = Item->Length;
        bool Compare = TokenLength == strlen(String) &&
                       TokenCaseCmp(Token,String,TokenLength) == 0;
        return Compare;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
