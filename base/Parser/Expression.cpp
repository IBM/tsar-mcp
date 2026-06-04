// Expression Parser: Expression.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: Expression.cpp
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

#define DEFINED_ExpAlloc 1
#ifdef _WIN32
        #define __ExpDECL __cdecl
#else
        #define __ExpDECL
#endif
void* (__ExpDECL *ExpAlloc)(size_t Size) = malloc;
void (__ExpDECL *ExpDealloc)(void *Memory) = free;
void* (__ExpDECL *ExpTransAlloc)(size_t Size) = malloc;
void (__ExpDECL *ExpTransDealloc)(void *Memory) = free;

#include <Expression.h>

#if defined(ExCharacterUNICODE)
        #define ClassCmp wcscmp
#else
        #define ClassCmp strcmp
#endif

// ****************************************************************************
// **** Memory Management *****************************************************
// ****************************************************************************

void SetExpAlloc(void* (__ExpDECL *Alloc)(size_t), 
                 void (__ExpDECL *Dealloc)(void *))
        {
        ExpAlloc = Alloc ? Alloc : malloc;
        ExpDealloc = Dealloc ? Dealloc : free;
        return;
        }

void SetExpTransAlloc(void* (__ExpDECL *Alloc)(size_t), 
                      void (__ExpDECL *Dealloc)(void *))
        {
        ExpTransAlloc = Alloc ? Alloc : malloc;
        ExpTransDealloc = Dealloc ? Dealloc : free;
        return;
        }

// ****************************************************************************
// **** Nodes *****************************************************************
// ****************************************************************************

NonTerminalNode::NonTerminalNode()
	{
	memset(Children,0,sizeof(Node*) * MAX_CHILDREN_NODES);
	return;
	}

NonTerminalNode::~NonTerminalNode()
	{
	ChainSaw();
	return;
	}

bool NonTerminalNode::AddChild(unsigned Index, Node *Child)
	{
	assert(!Children[Index]);
	if (!Child) return false;
	Child->Parent = this;
        Child->IndexInParent = Index;
	Children[Index] = Child;
	return true;
	}

bool NonTerminalNode::Apply(ScanStack &Stack, unsigned Count)
	{
	bool Status = true;
	while (Count && Status) Status = AddChild(--Count,Stack.PopNode());
	return Status;
	}

void NonTerminalNode::ChainSaw()
	{
	for (unsigned i=0; i < MAX_CHILDREN_NODES; i++)
		{
		if (Children[i]) delete Children[i];
		}
	return;
	}

// **********************
// **** Edit Utility ****
// **********************

Node* ReplaceNodeEdit(Node *ToReplace, Node *Replacement)
        {
        NonTerminalNode *Parent = ToReplace->Parent;
        if (!Parent) return NULL;                       // Topmost Node.
        unsigned IndexInParent = ToReplace->IndexInParent;
        ToReplace->Parent = NULL;
        Parent->Children[IndexInParent] = NULL;
        Parent->AddChild(IndexInParent,Replacement);
        return ToReplace;
        }

// ************************************
// **** Sequential Iterate Utility ****
// ************************************

Node* SequentialGetNext(Node *Current, unsigned &Level)
        {
        if (!Current) return NULL;
        Node *Child = Current->GetChild(0);             // Send first child.
        if (Child)
                {
                Level++;
                return Child;
                }
        while (Current && Level)                        // Send siblings.
                {
                NonTerminalNode *Parent = Current->GetParent();
                if (!Parent) return NULL;                       // All done.
                unsigned i = Current->IndexInParent;
                if (++i < MAX_CHILDREN_NODES)
                        {
                        Node *Child = Parent->GetChild(i);
                        if (Child) return Child;
                        }
                Current = Parent;
                Level--;
                }
        return NULL;
        }

// ****************************************************************************
// **** Transitions ***********************************************************
// ****************************************************************************

Transition* TransitionList::FindTransition(inputT Input)
	{
        Transition *Found = NULL;
	Transition *Current = GetFirst();
	while (Current)
		{
		if (Current->isMatch(Input))
                        {
                        Found = Current;
                        break;
                        }
		Current = GetNext(Current);
		}
	return Found;
	}

Transition* TransitionList::FindTransition(Node *Input)
	{
        Transition *Found = NULL;
	Transition *Current = GetFirst();
	unsigned NodeTypeID = Input->GetTypeID();
	while (Current)
		{
		if (Current->isMatch(NodeTypeID))
                        {
                        Found = Current;
                        break;
                        }
		Current = GetNext(Current);
		}
	return Found;
	}

// ****************************************************************************
// **** Transitions ***********************************************************
// ****************************************************************************

#ifdef __Expression_Parser_CPlusPlus

Transition::Transition(unsigned fromState, unsigned toState)
	{
	FromState = fromState;
	ToState = toState;
	return;
	}

#else /* __Expression_Parser_Struct */

Transition::Transition(TransitionInfo &info, unsigned From, unsigned To)
        :  
        Info(info)
        {
        FromState = From;
        ToState = To;
        return;
        }

Node* Transition::DoAction(ScanStack &ScanCB, inputT *Input)
        {
        if (Info.TType == TT_Read)
                {
                TerminalNode *NewNode;
                NewNode = (TerminalNode *)Info.NewNodeFn(Info.NodeTypeID);
                if (!NewNode) throw Error_Memory();
                NewNode->Read(*Input);
                PushNode(ScanCB,GetToState(),NewNode);
                *Input = NULL;
                return NULL;
                }
        if (Info.TType == TT_Apply)
                {
                NonTerminalNode *NewNode;
                NewNode = (NonTerminalNode *)Info.NewNodeFn(Info.NodeTypeID);
                if (!NewNode) throw Error_Memory();
                bool Status = NewNode->Apply(ScanCB,Info.ApplyN);
                if (!Status)
                        {
                        delete NewNode; 
                        throw Error_Syntax(); 
                        }
                return NewNode;
                }
        return NULL;
        }

bool Transition::DoAction(ScanStack &Stack, Node *Input)
        {
        if (Info.TType == TT_Simple)
                {
                PushNode(Stack,GetToState(),Input);
                return true;
                }
        return false;
        }

#endif /* __Expression_Parser_Struct */

void Transition::PushNode(ScanStack &Stack, unsigned State, Node *Item)
	{
	StackItem *sItem = new StackItem(State,Item);
	if (!sItem) throw Error_Memory();
	Stack.Push(sItem);
	return;
	}

// ****************************************************************************
// **** Transition Factory ****************************************************
// ****************************************************************************

#ifdef __Expression_Parser_Struct

static TransitionInfo* FindInfo(TransitionInfo *TransitionInfoTable,
                                const Exchar_t *ClassName)
        {
        for (unsigned i=0; TransitionInfoTable[i].TType; i++)
                {
                if (ClassCmp(TransitionInfoTable[i].ClassName,ClassName) == 0)
                        {
                        return &TransitionInfoTable[i];
                        }
                }
        return NULL;
        }

Transition* NewTransition(TransitionInfo *TransitionInfoTable,
                          const Exchar_t *ClassName,
                          unsigned FromState,
                          unsigned ToState)
        {
        TransitionInfo *Info = FindInfo(TransitionInfoTable,ClassName);
        assert(Info);
        Transition *NewTransition = new Transition(*Info,FromState,ToState);
        return NewTransition;
        }

#endif /* __Expression_Parser_Struct */        

// ****************************************************************************
// **** State Machine *********************************************************
// ****************************************************************************

StateMachine::StateMachine(unsigned NumStates)
	{
        GoalReductionState = 0;
	TransitionMap = NULL;
	Reset(NumStates);
	return;
	}

StateMachine::~StateMachine()
	{
	if (TransitionMap) delete[] TransitionMap;
        return;
	}

bool StateMachine::AddTransition(Transition *Change)
	{
	if (!TransitionMap || !Change) return false;
	unsigned FromState = Change->GetFromState();
	assert(FromState < NumStates);
	TransitionMap[FromState].AddBottom(Change);
	return true;
	}

Transition* StateMachine::FindTransition(ScanStack &Stack, inputT Input)
	{
	unsigned CurrentState = Stack.GetTOSState();
	assert(CurrentState < NumStates);
	return TransitionMap[CurrentState].FindTransition(Input);
	}

Transition* StateMachine::FindTransition(ScanStack &Stack, Node *Input)
	{
	unsigned CurrentState = Stack.GetTOSState();
	assert(CurrentState < NumStates);
	return TransitionMap[CurrentState].FindTransition(Input);
	}

void StateMachine::Reset(unsigned numStates)
	{
	if (TransitionMap) delete[] TransitionMap;
	NumStates = numStates;
	if (numStates)
		{
		TransitionMap = new TransitionList[NumStates];
		if (!TransitionMap) throw Error_Memory();
		}
        else TransitionMap = NULL;
	return;
	}

// ****************************************************************************
// **** Expression ************************************************************
// ****************************************************************************

Expression::Expression(StateMachine &machine) : Machine(machine)
	{
	CurrentNode = NULL;
	WatchDataInput = NULL;
	WatchNodeInput = NULL;
	return;
	}

Node* Expression::AquireExpression()
	{
	Node *node = CurrentNode;
        CurrentNode = NULL;
        return node;
	}

parseStatus Expression::NextInput(inputT Input)
	{
	Transition *Change;
	parseStatus Status = pS_Good;
        unsigned GoalReductionState = Machine.GetGoalReductionState();
  try	{
	while (Status == pS_Good && Input)	// Evaluate until input read.
		{
		if (CurrentNode)
			{
			Change = Machine.FindTransition(Stack,CurrentNode);
			if (!Change) return pS_SyntaxError;
			if (WatchNodeInput) WatchNodeInput(Change,CurrentNode);
			Change->DoAction(Stack,CurrentNode);
			CurrentNode = NULL;
			}
		else	{
			Change = Machine.FindTransition(Stack,Input);
			if (!Change) return pS_SyntaxError;
			if (WatchDataInput) WatchDataInput(Change,Input);
			CurrentNode = Change->DoAction(Stack,&Input);
                        if (Stack.GetTOS() == NULL && 
                            Change->GetFromState() == GoalReductionState)
                                {
                                Status = pS_Accepted;
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

void Expression::Reset()
	{
	Stack.ChainSaw();
	if (CurrentNode) delete CurrentNode;
	CurrentNode = NULL;
	return;
	}

void Expression::SetWatch(void (*fn)(Transition*, Node*))
	{
	WatchNodeInput = fn;
	return;
	}

void Expression::SetWatch(void (*fn)(Transition*, inputT))
	{
	WatchDataInput = fn;
	return;
	}

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
