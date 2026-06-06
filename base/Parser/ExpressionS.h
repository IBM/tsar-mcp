// Expression Parser (Stuct): ExpressionS.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ExpressionS.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1998 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Defines classes: Node
//                       TerminalNode
//                       NonTerminalNode
//                       Transition
//                       StateMachine
//                       Expression
//
//      Defines macros: DefineNonTerminalNode
//                      DefineReadTransition
//                      DefineApplyTransition
//                      DefineSimpleTransition(ClassName,NodeTypeID)
//
//      Memory allocation: 
//
//                      SetExpAlloc(Alloc,Dealloc)
//
//      Utility functions:
//
//                      ReplaceNodeEdit(Node *ToReplace, Node *NewNode)
//                      SequentialGetNext(Node *Current, unsigned &Level)
//

#ifndef __Expression_Parser_Struct

        #define __Expression_Parser_Struct

#define TerminalNodeType 1
#define UndefinedNodeType 0

#ifndef MAX_CHILDREN_NODES
        #define MAX_CHILDREN_NODES 8
#endif

#define NOTACHILD_NODE (MAX_CHILDREN_NODES)

#ifndef ExNAMESPACE
        #define ExNAMESPACE(x) ExNS_##x
#endif

#ifndef TerminalNodeBase                        // For NewTerminalNode().
        #define TerminalNodeBase TerminalNode
#endif

#ifndef NonTerminalNodeBase                     // for NewNonTerminalNode().
        #define NonTerminalNodeBase NonTerminalNode
#endif

#ifdef ExCharacterUNICODE
        #define Exchar_t wchar_t
        #define ExU(x) L###x
#else
        #define Exchar_t char
        #define ExU(x) x
#endif

typedef void* inputT;

enum parseStatus {pS_Good = 0, pS_SyntaxError, pS_Accepted};

class ScanStack;

// ****************************************************************************
// **** Nodes *****************************************************************
// ****************************************************************************

// *************************
// **** Node Info Table ****
// *************************

struct NodeInfo
        {
        const Exchar_t *ClassName;
        unsigned TypeID;
        bool isTerminal;
        };

// ***************
// **** Nodes ****
// ***************

class Node;
class TerminalNode;
class NonTerminalNode;

static Node* NewTerminalNode(unsigned);
static Node* NewNonTerminalNode(unsigned);

class Node
        {
        private:
                NodeInfo *Info;
                NonTerminalNode *Parent;
                unsigned IndexInParent;
                friend class NonTerminalNode;
                friend Node* NewTerminalNode(unsigned);
                friend Node* NewNonTerminalNode(unsigned);
                friend Node* ReplaceNodeEdit(Node *ToReplace, Node *NewNode);
                friend Node* SequentialGetNext(Node *Current, unsigned &Level);
        protected:
                void SetNodeInfo(NodeInfo *info) {Info = info;}
        public:
                Node() {Parent = NULL;}
                virtual ~Node() {}
                NonTerminalNode* GetParent() {return Parent;}
                unsigned GetIndexInParent()
                        {
                        return Parent ? IndexInParent : NOTACHILD_NODE;
                        }
                Node* GetChild(unsigned i);
                Node* GetLeft() {return GetChild(0);}
                Node* GetCenter() {return GetChild(1);}
                Node* GetRight() {return GetChild(2);}
                const Exchar_t* GetName() {return Info->ClassName;}
                unsigned GetTypeID() {return Info->TypeID;}
                inputT GetValue();
                bool isTerminal() {return Info->isTerminal;}
                bool isNonTerminal() {return !Info->isTerminal;}
                // ***************************
                // **** Memory Management ****
                // ***************************
                void* operator new(size_t Size) {return ExpAlloc(Size);}
                void operator delete(void *Item) {ExpDealloc(Item);}
        };

class TerminalNode : public Node
        {
        private:
                inputT Value;
                friend class Node;
        public:
                void Read(inputT Input) {Value = Input;}
        };

class NonTerminalNode : public Node
        {
        private:
                Node *Children[MAX_CHILDREN_NODES];
                friend class Node;
                friend Node* ReplaceNodeEdit(Node *ToReplace, Node *NewNode);
        protected:
                bool AddChild(unsigned Index, Node *Child);
                void ChainSaw();
        public:
                NonTerminalNode();
                ~NonTerminalNode();
                bool Apply(ScanStack &Stack, unsigned Count);
        };

inline Node* Node::GetChild(unsigned i)
        {
        if (Info->isTerminal) return NULL;
        return ((NonTerminalNode *)this)->Children[i];
        }

inline inputT Node::GetValue()
        {
        if (!Info->isTerminal) return NULL;
        return ((TerminalNode *)this)->Value;
        }

// **********************
// **** Node Factory ****
// **********************

#define DefineNodesBEGIN                                                     \
        NodeInfo ExNAMESPACE(NodeInfoTable)[] =                              \
                {                                                            \
                DefineTerminalNode(ExU("Undefined"),UndefinedNodeType)       \
                DefineTerminalNode(ExU("Terminal"),TerminalNodeType)

#define DefineTerminalNode(ClassName,TypeID) {ExU(#ClassName),TypeID,true},
#define DefineNonTerminalNode(ClassName,TypeID) {ExU(#ClassName),TypeID,false},

#define DefineNodeFactory                                                    \
        extern NodeInfo ExNAMESPACE(NodeInfoTable)[];                        \
                                                                             \
        static Node* NewTerminalNode(unsigned TID)                           \
                {                                                            \
                TerminalNode *NewNode = new TerminalNodeBase;                \
                if (NewNode)                                                 \
                        {                                                    \
                        NewNode->Info = &ExNAMESPACE(NodeInfoTable)[TID];    \
                        }                                                    \
                return NewNode;                                              \
                }                                                            \
                                                                             \
        static Node* NewNonTerminalNode(unsigned TID)                        \
                {                                                            \
                NonTerminalNode *NewNode = new NonTerminalNodeBase;          \
                if (NewNode)                                                 \
                        {                                                    \
                        NewNode->Info = &ExNAMESPACE(NodeInfoTable)[TID];    \
                        }                                                    \
                return NewNode;                                              \
                }

#define DefineNodesEND {NULL,0,false}};                                      \
        DefineNodeFactory

// ****************************************************************************
// **** Utilities *************************************************************
// ****************************************************************************
        
// **********************
// **** Edit Utility ****
// **********************

Node* ReplaceNodeEdit(Node *ToReplace, Node *NewNode);

// ************************************
// **** Sequential Iterate Utility ****
// ************************************

Node* SequentialGetNext(Node *Current, unsigned &Level);

// ****************************************************************************
// **** Parser State Stack ****************************************************
// ****************************************************************************

struct StackItem : Link
        {
        unsigned State;
        Node* Item;
        StackItem(unsigned state, Node* item) {State=state; Item=item;}
        ~StackItem() {if (Item) delete Item;}
        // ***************************
        // **** Memory Management ****
        // ***************************
        void* operator new(size_t Size) {return ExpAlloc(Size);}
        void operator delete(void *Item) {ExpDealloc(Item);}
        };

DefineLinkList(StackItemList,StackItem);

class ScanStack : public StackItemList
	{
        private:
                unsigned EOSState;
	public:
		ScanStack() {EOSState = 0; SetAutoCleanup(true);}
		bool Drop();
		StackItem* GetTOS() {return GetLast();}
                unsigned GetTOSState() 
                        {
                        StackItem *TOS = GetLast();
                        return TOS ? TOS->State : EOSState;
                        }
		void Push(StackItem *Item) {AddBottom(Item);}
		StackItem* Pop();
		Node* PopNode();
                void SetEOSState(unsigned State) {EOSState = State;}
	};

// ******************************
// **** Inline Stack Methods ****
// ******************************

inline bool ScanStack::Drop()           // Deletes the top-most item.
        {
        StackItem *Last = GetLast();
        if (Last)
                {
                Remove(Last);
                delete Last;
                }
        return Last != NULL;
        }

inline StackItem* ScanStack::Pop()      // Returns the item from the top.
        {
        StackItem *Last = GetLast();
        if (Last) Remove(Last);
        return Last;
        }

inline Node* ScanStack::PopNode()       // Returns the node from the top-most
        {                               // stack item and deletes the stack
        Node *TopNode = NULL;           // item.
        StackItem *Last = GetLast();
        if (Last)
                {
                Remove(Last);
                TopNode = Last->Item;
                Last->Item = NULL;
                delete Last;
                }
        return TopNode;
        }

// ****************************************************************************
// **** Transitions ***********************************************************
// ****************************************************************************

enum TransitionType_t {TT_NULL=0, TT_Read, TT_Apply, TT_Simple};

struct TransitionInfo
        {
        TransitionType_t TType;
        const Exchar_t* ClassName;
        unsigned NodeTypeID;
        bool (*MatchFn)(inputT input);
        unsigned ApplyN;
        Node* (*NewNodeFn)(unsigned TypeID);
        };

class Transition : public Link
        {
        private:
                TransitionInfo &Info;
                unsigned FromState, ToState;
        protected:
                void PushNode(ScanStack &Stack, unsigned State, Node *Item);
                friend class StateMachine;
        public:
                Transition(TransitionInfo &info, unsigned From, unsigned To);
                Node* DoAction(ScanStack &ScanCB, inputT *Input);
                bool DoAction(ScanStack &Stack, Node *Input);
                unsigned GetFromState() {return FromState;}
                unsigned GetToState() {return ToState;}
                const Exchar_t* GetClassName() {return Info.ClassName;}
                bool isMatch(unsigned NodeTypeID)
                        {
                        if (Info.TType != TT_Simple) return false;
                        return Info.NodeTypeID == NodeTypeID;
                        }
                bool isMatch(inputT Input)
                        {
                        if (Info.TType == TT_Simple) return false;
                        return Info.MatchFn(Input);
                        }
                // ***************************
                // **** Memory Management ****
                // ***************************
                void* operator new(size_t Size) {return ExpTransAlloc(Size);}
                void operator delete(void *Item) {ExpTransDealloc(Item);}
        };

// ****************************
// **** Transition Factory ****
// ****************************

Transition* NewTransition(TransitionInfo *TransitionInfoTable,
                          const Exchar_t *ClassName, 
                          unsigned FromState,
                          unsigned ToState);

#define DefineTransitionsBEGIN                                               \
        static TransitionInfo ExNAMESPACE(TransitionInfoTable)[] = {

#define DefineReadTransition(ClassName,NodeClass,MatchFn)                    \
        {                                                                    \
        TT_Read,                                                             \
        ExU(#ClassName),                                                     \
        Ew_##NodeClass##ID,                                                  \
        MatchFn,                                                             \
        0,                                                                   \
        NewTerminalNode                                                      \
        },
                                                           
#define DefineApplyTransition(ClassName,NodeClass,MatchFn,ApplyN)            \
        {                                                                    \
        TT_Apply,                                                            \
        ExU(#ClassName),                                                     \
        Ew_##NodeClass##ID,                                                  \
        MatchFn,                                                             \
        ApplyN,                                                              \
        NewNonTerminalNode                                                   \
        },

#define DefineSimpleTransition(ClassName,NodeTypeID)                         \
        {                                                                    \
        TT_Simple,                                                           \
        ExU(#ClassName),                                                     \
        NodeTypeID,                                                          \
        NULL,                                                                \
        0,                                                                   \
        NULL                                                                 \
        },                                                                    

#define DefineTransitionsEND {TT_NULL,NULL,0,NULL,0}};

#define ConstructTransition(Machine,ClassName,FromState,ToState)             \
        Machine.AddTransition(NewTransition(ExNAMESPACE(TransitionInfoTable),\
                                            ExU(#ClassName),                 \
                                            FromState,                       \
                                            ToState))

DefineLinkList(_TransitionList,Transition);

class TransitionList : public _TransitionList
        {
        public:
                TransitionList() {SetAutoCleanup(true);}
                Transition* FindTransition(inputT Input);
                Transition* FindTransition(Node *Input);
        };

// ****************************************************************************
// **** State Machine *********************************************************
// ****************************************************************************

class StateMachine
        {
        private:
                unsigned NumStates;
                unsigned GoalReductionState;
                TransitionList *TransitionMap;
        public:
                StateMachine(unsigned NumStates=0);
                ~StateMachine();
                bool AddTransition(Transition *change);
                Transition* FindTransition(ScanStack &Stack, inputT Input);
                Transition* FindTransition(ScanStack &Stack, Node *Input);
                unsigned GetGoalReductionState() {return GoalReductionState;}
                void Reset(unsigned NumStates);
                void SetGoalReductionState(unsigned S) {GoalReductionState=S;}
        };

class Expression
        {
        protected:
                ScanStack Stack;
                Node *CurrentNode;
                StateMachine &Machine;
                void (*WatchNodeInput)(Transition*, Node*);
                void (*WatchDataInput)(Transition*, inputT);
        public:
                Expression(StateMachine &machine);
                ~Expression() {Reset();}
                Node* AquireExpression();
                unsigned GetCurrentState() {return Stack.GetTOSState();}
                Node* GetExpression() {return CurrentNode;}
                parseStatus NextInput(inputT Input);
                void Reset();
                void SetWatch(void (*fn)(Transition*, inputT));
                void SetWatch(void (*fn)(Transition*, Node*));
        };

/* **************************************************************************
 ****************************************************************************

 Notes:

             I. See Expression.h

            II. Creating Node Classes:

                        ExpressionCPP.h defines a Class for each node type.

                        ExpressionS.h only defines one Node class, and each
                        node instance contains a pointer to the NodeInfo
                        entry representing the Nodes characteristic.

                        Example:

                        #ifdef __Expression_Parser_Struct

                        extern NodeInfo ExNAMESPACE(NodeInfoTable)[];
                        #define NodeInfoTable ExNAMESPACE(NodeInfoTable);

                        class CommentNode : public TerminalNodeBase
                                {
                                public:
                                        CommentNode()
                                                {
                                                NodeInfo *Info;
                                                unsigned ID = Ew_CommentID;
                                                Info = &NodeInfoTable[ID];
                                                SetNodeInfo(Info);
                                                }
                                };

                        #endif // __Expression_Parser_Struct

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif
