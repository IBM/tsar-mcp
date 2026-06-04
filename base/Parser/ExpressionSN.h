// Expression Parser with State Nodes: ExpressionSN.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ExpressionSN.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1998 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Defines classes: NonTerminalNodeSN
//                       ExpressionSN
//
//      Defines functions: FindGoalInTree()
//

#ifndef __Expression_Parser_StateNode

        #define __Expression_Parser_StateNode

#include <Expression.h>

#define ExpNOSTATE ((unsigned)-1)

#ifndef NonTerminalNodeBaseSN                   // for DefineNonTerminalNode().
        #define NonTerminalNodeBaseSN NonTerminalNodeSN
#endif

#undef NonTerminalNodeBase

#define NonTerminalNodeBase NonTerminalNodeBaseSN

class NonTerminalNodeSN : public NonTerminalNode
        {
        private:
                unsigned short AcceptingToState;
                unsigned short AcceptingFromState;
        public:
                NonTerminalNodeSN() 
                        {
                        AcceptingToState = AcceptingFromState = 0;
                        }
                unsigned GetAcceptingToState() {return AcceptingToState;}
                unsigned GetAcceptingFromState() {return AcceptingFromState;}
                void SetAcceptingToState(unsigned ToState)
                        {
                        AcceptingToState = (short)ToState;
                        }
                void SetAcceptingFromState(unsigned FromState)
                        {
                        AcceptingFromState = (short)FromState;
                        }
        };

class ExpressionSN : public Expression
        {
        private:
                unsigned DynamicGoalState;
                unsigned DynamicStartState;
                TerminalNode *DynamicTerminal;
                NonTerminalNodeSN *GoalInTree;
        protected:
                TerminalNode* FindFirstTerminal(Node* StartNode);
                TerminalNode* FindFollowingTerminal(Node* StartNode);
                parseStatus NextInput(inputT Input, bool _staticInput);
        public:
                ExpressionSN(StateMachine &machine);
                ExpressionSN(StateMachine &machine,
                             Node *StartNode,
                             const char *NonTerminalGoal);
                NonTerminalNodeSN* GetGoalInTree() {return GoalInTree;}
                parseStatus NextInput(inputT Input)
                        {
                        return NextInput(Input,false);
                        }
                parseStatus NextInputEOL(inputT Input);
                void Reset();
                void Reset(Node *StartNode, const char *NonTerminalGoal);
        };

NonTerminalNodeSN* FindGoalInTree(Node *Leaf, const char *NonTerminalGoal);

/* **************************************************************************
 ****************************************************************************

 Notes:

     I. Nodes with States:

                The NonTerminalNode with state:

                        NonTerminalNodeSN

                retains the initial and aquiring state of the machine 
                during initial parsing. The retained state allows reparsing 
                to begin begining within a Branch node.

    II. Expression:

             A. Parsing:

                To be able to use the "Reparse" capablity; The ExpressionSN 
                parser must be used both to build the the initial tree and 
                re-parse subsequent trees.

                WARNING: There is no checking in ExpressionSN to
                         validate that the initial tree was build with
                         ExpressionSN instead of Expression.

                In general ExpressionSN parsing is exactly the same as 
                Expression parsing, except for the last token representing 
                the end of line/input (EOL).
                        
                      - NextInput()

                                (See Expression.h)

                      - NextInputEOL()

                                When "Reparsing", NextInputEOL() must be 
                                called instead of NextInput() when the 
                                (end of input stream) inputT has been 
                                detected. If not "Reparsing", NextInputEOL() 
                                is the same as NextInput().

                                Ex: if (Item->SymbolClass == SC_Null)
                                        {
                                        Parser.NextInputEOL(Item);
                                        }

             B. Methods:

                        - GetGoalInTree() 

                                Returns the node in the original tree 
                                equivalant to "NonTerminalGoal" specified 
                                during construction.

                                This node can be passed into 
                                ReplaceNodeEdit() as the 'ToReplace' value.

   III. Grammer modification with Common-Parse-Point:

        A common-parse-point can be inserted to the grammer when a desired 
        non-terminal production may result in multiple state branches. 

        For example, the following productions would have multiple state 
        flows depending on whether the parsed expression is a single term 
        or a sequence of terms. With a single common-parse-point inserted
        into the grammer, all state flows reduce to a common state. The
        common-parse-point is then specified for 'NonTerminalGoal' in 
        the ExpressionSN constructor. 

        Before:

                Expression -> ExpValueSign
                Expression -> Expression op ExpValueSign

        After (Common Non-Terminal = "Expression")

                Expression -> ExpressionSequence

                ExpressionSequence -> ExpValueSign
                ExpressionSequence -> ExpressionSequence op ExpValueSign  

    IV. Utility Functions:

              - FindGoalInTree(Node *Leaf, char *NonTerminalGoal)

                        Searches upward of 'Leaf' looking for the
                        NonTerminal of type 'NonTerminalGoal'.

                        It is expected to be called to retrieve the 
                        GoalNode for passing to ReplaceNodeEdit() when 
                        the ExpressionSN instance is not longer available 
                        to inquire GetGoalInTree().                                

 ****************************************************************************
 **** Example ***************************************************************
 *************

 ****************************************************************************


 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __Expression_Parser_StateNode */
