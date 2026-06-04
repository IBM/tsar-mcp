// Expression Parser with State Nodes: ExpressionSN.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ExpressionSN.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1998 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <ExpressionSN.h>

#ifndef STATE_STORE_DEBUG
        #define STATE_STORE_DEBUG 0
#endif

// ****************************************************************************
// **** Expression ************************************************************
// ****************************************************************************

ExpressionSN::ExpressionSN(StateMachine &machine) : Expression(machine)
        {
        DynamicStartState = 0;
        DynamicGoalState = Machine.GetGoalReductionState();
        DynamicTerminal = NULL;
        GoalInTree = NULL;
        return;
        }

ExpressionSN::ExpressionSN(StateMachine &machine,
                           Node *StartNode,
                           const char *NonTerminalGoal)
        : 
        Expression(machine)
        {
        Reset(StartNode,NonTerminalGoal);
        return;
        }

TerminalNode* ExpressionSN::FindFirstTerminal(Node* StartNode)
        {
        if (StartNode->isTerminal()) return (TerminalNode *)StartNode;
        for (unsigned iChild = 0; iChild < MAX_CHILDREN_NODES; iChild++)
                {
                Node *Child = StartNode->GetChild(iChild);
                if (!Child) break;
                TerminalNode *First = FindFirstTerminal(Child);
                if (First) return First;
                }
        return NULL;
        }

TerminalNode* ExpressionSN::FindFollowingTerminal(Node* StartNode)
        {
        unsigned iChild;                     // Search "right" of StartNode.
        Node *Parent = StartNode->GetParent();
        if (!Parent) return NULL;
        for (iChild = 0; iChild < MAX_CHILDREN_NODES; iChild++)
                {
                Node *Child = Parent->GetChild(iChild);
                if (!Child) return NULL;
                if (Child == StartNode) break;
                }
        if (++iChild >= MAX_CHILDREN_NODES) return NULL;
        for ( ; iChild < MAX_CHILDREN_NODES; iChild++)
                {
                Node *Child = Parent->GetChild(iChild);
                if (!Child) break;
                TerminalNode *First = FindFirstTerminal(Child);
                if (First) return First;
                }
        return FindFollowingTerminal(Parent);
        }

parseStatus ExpressionSN::NextInput(inputT Input, bool _staticInput)
        {
        Transition *Change;
        parseStatus Status = pS_Good;
  try   {
        while (Status == pS_Good && Input)      // Evaluate until input read.
                {
                if (CurrentNode)
                        {
                        Change = Machine.FindTransition(Stack,CurrentNode);
                        if (!Change) return pS_SyntaxError;
                        if (WatchNodeInput) WatchNodeInput(Change,CurrentNode);
                        Change->DoAction(Stack,CurrentNode);
                        CurrentNode = NULL;
                        }
                else    {
                        Change = Machine.FindTransition(Stack,Input);
                        if (!Change) return pS_SyntaxError;
                        if (WatchDataInput) WatchDataInput(Change,Input);
                        CurrentNode = Change->DoAction(Stack,&Input);
                        if (CurrentNode && CurrentNode->isNonTerminal())
                                {
                                NonTerminalNodeSN *AcceptNode;
                                AcceptNode = (NonTerminalNodeSN *)CurrentNode;
                                unsigned To = Change->GetFromState();
                                unsigned From = Stack.GetTOSState(); 
                                AcceptNode->SetAcceptingToState(To);
                                AcceptNode->SetAcceptingFromState(From);
  #if STATE_STORE_DEBUG
                                printf("+++ ToState: %u\n", To);
                                printf("+++ FromState: %u\n", From);
  #endif
                                }
                        if (Stack.GetTOS() == NULL &&
                            Change->GetFromState() == DynamicGoalState)
                                {
                                Status = pS_Accepted;
                                }
                        else if (_staticInput && Input == NULL) // Input acts
                                {                               // as EOL but
                                TerminalNode *ReadNode;         // was read.
                                ReadNode = (TerminalNode *)Stack.PopNode();
                                if (ReadNode)
                                        {                     // Find & clear
                                        ReadNode->Read(NULL); // re-read
                                        delete ReadNode;      // DynamicTerm.
                                        }
                                Status = pS_SyntaxError;
                                }
                        }
                }
        }
        catch(Error_Syntax&)
                {
                Status = pS_SyntaxError;
                }
        return Status;
        };

parseStatus ExpressionSN::NextInputEOL(inputT Input)
        {
        parseStatus rc;
        if (!DynamicTerminal) rc = NextInput(Input,false);
        else rc = NextInput(DynamicTerminal->GetValue(),true);
        return rc;
        }

void ExpressionSN::Reset()
        {
        DynamicStartState = 0;
        DynamicGoalState = Machine.GetGoalReductionState();
        DynamicTerminal = NULL;
        GoalInTree = NULL;
        Expression::Reset();
        return;
        }

void ExpressionSN::Reset(Node *StartNode, const char *NonTerminalGoal)
        {
        if (StartNode)
                {
                GoalInTree = FindGoalInTree(StartNode,NonTerminalGoal);
                }
        else GoalInTree = NULL;
        if (GoalInTree)
                {
                DynamicStartState = GoalInTree->GetAcceptingFromState();
                DynamicGoalState = GoalInTree->GetAcceptingToState();
                DynamicTerminal = FindFollowingTerminal(GoalInTree);
                }
        else    {
                DynamicStartState = 0;
                DynamicGoalState = Machine.GetGoalReductionState();
                DynamicTerminal = NULL;
                }
        Stack.SetEOSState(DynamicStartState);
        Expression::Reset();
        return;
        }

// ****************************************************************************
// **** FindGoalInTree ********************************************************
// ****************************************************************************

NonTerminalNodeSN* FindGoalInTree(Node *Leaf, const char *NonTerminalGoal)
        {
        Node *StartNode = Leaf->isNonTerminal() ? Leaf : Leaf->GetParent();
        while (StartNode)
                {
                if (strcmp(StartNode->GetName(),NonTerminalGoal) == 0)
                        {
                        break;
                        }
                StartNode = StartNode->GetParent();
                }
        return (NonTerminalNodeSN *)StartNode;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
