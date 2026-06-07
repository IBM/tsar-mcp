// LexicalParser.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: LexicalParser.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1998 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LexicalParser.h>

// ****************************************************************************
// **** Symbol State Table ****************************************************
// ****************************************************************************

SymbolStateTableInfo::SymbolStateTableInfo()
        {
        NumOverrides = 0;
        MaxOverrides = 0;
        Overrides = NULL;
        CoreDefault = NULL;
        StateDefault = ST_CoreSTART;
        return;
        }

SymbolStateTableInfo::~SymbolStateTableInfo()
        {
        if (Overrides) free(Overrides);
        return;
        }

bool SymbolStateTableInfo::AddTransition(char Symbol,
                                         SymbolState_t NewState,
                                         bool Accept)
        {
        if (NumOverrides == MaxOverrides)
                {
                printf("ERROR: AddTransition(%u,%u) - Too many States\n",
                       Symbol,
                       NewState);
                return false;
                }
        Overrides[NumOverrides].Symbol = Symbol;
        if (Accept) NewState |= StateACCEPTMask;
        else NewState &= StateSTATEMask;
        Overrides[NumOverrides].NewState = NewState;
        NumOverrides++;
        return true;
        }

bool SymbolStateTableInfo::AssignStateDefault(SymbolState_t Default, 
                                              size_t nOverrides)
        {
        bool Status = true;
        StateDefault = Default;
        NumOverrides = MaxOverrides = 0;
        if (nOverrides)
                {
                Overrides = (TransitionOverride_t *)
                            calloc(nOverrides,sizeof(TransitionOverride_t));
                if (Overrides)
                        {
                        MaxOverrides = nOverrides;
                        Status = true;
                        }
                }
        return Status;
        }

bool SymbolStateTableInfo::AssignStateTable(SymbolState_t *Core,
                                            size_t nOverrides)
        {
        bool Status = true;
        CoreDefault = Core;
        NumOverrides = MaxOverrides = 0;
        if (nOverrides)
                {
                Overrides = (TransitionOverride_t *)
                            calloc(nOverrides,sizeof(TransitionOverride_t));
                if (Overrides)
                        {
                        MaxOverrides = nOverrides;
                        Status = true;
                        }
                }
        return Status;
        }

// ****************************************************************************
// **** Symbol State Machine **************************************************
// ****************************************************************************

SymbolStateMachine::SymbolStateMachine(unsigned NumUserStates)
        {
        NumStates = NumUserStates + NumCoreStates;
        StateTables = new SymbolStateTableInfo[NumStates];
        if (StateTables)
                {
                SymbolState_t *Core = CoreStateTables[ST_CoreSTART];
                StateTables[ST_CoreSTART].AssignStateTable(Core,0);
                Core = CoreStateTables[ST_CoreSPACE];
                StateTables[ST_CoreSPACE].AssignStateTable(Core,0);
                Core = CoreStateTables[ST_CoreCHAR];
                StateTables[ST_CoreCHAR].AssignStateTable(Core,0);
                Core = CoreStateTables[ST_CoreDIGIT];
                StateTables[ST_CoreDIGIT].AssignStateTable(Core,0);
                Core = CoreStateTables[ST_CoreSYMBOL];
                StateTables[ST_CoreSYMBOL].AssignStateTable(Core,0);
                }
        BuildCoreTables();
        return;
        }

SymbolStateMachine::~SymbolStateMachine()
        {
        if (StateTables) delete [] StateTables;
        return;
        }

bool SymbolStateMachine::AddTransition(SymbolState_t CurrentState,
                                       char Symbol, 
                                       SymbolState_t NewState,
                                       bool Accept)
        {
        bool rc;
        unsigned iSymbol = (unsigned char)Symbol;
        if (Accept && CurrentState != ST_CoreSTART)     // Accept from states
                {                                       // other than the
                NewState |= StateACCEPTMask;            // start state. Set
                }                                       // high order bit.
        else NewState &= StateSTATEMask;
        if (CurrentState < NumCoreStates)       // Direct state table change.
                {
                CoreStateTables[CurrentState][iSymbol] = NewState;
                return true;
                }
        else if (CurrentState == ST_CoreALL)
                {
                CoreStateTables[ST_CoreSTART][iSymbol] = StateSTATE(NewState);
                CoreStateTables[ST_CoreSPACE][iSymbol] = NewState;
                CoreStateTables[ST_CoreCHAR][iSymbol] = NewState;
                CoreStateTables[ST_CoreDIGIT][iSymbol] = NewState;
                CoreStateTables[ST_CoreSYMBOL][iSymbol] = NewState;
                return true;
                }
        else if ((unsigned)CurrentState >= NumStates)
                {
                printf("ERROR: AddTransition(%u,%u,%u) - Too many States\n",
                       CurrentState,
                       Symbol,
                       NewState);
                return false;
                }
        rc = StateTables[CurrentState].AddTransition(Symbol,NewState,Accept);
        return rc;
        }

bool SymbolStateMachine::AssignStateDefault(SymbolState_t State,
                                            SymbolState_t StateDefault,
                                            size_t nOverrides)
        {
        if ((unsigned)State >= NumStates) return false;
        return StateTables[State].AssignStateDefault(StateDefault,nOverrides);
        }

bool SymbolStateMachine::AssignStateTable(SymbolState_t State,
                                          SymbolState_t CoreDefault,
                                          size_t nOverrides)
        {
        if ((unsigned)State >= NumStates) return false;
        SymbolState_t *Core = CoreStateTables[CoreDefault];
        return StateTables[State].AssignStateTable(Core,nOverrides);
        }

void SymbolStateMachine::BuildCoreTables()
        {
        for (int i=0; i < LexNumSymbols; i++)
                {
                SymbolState_t NewState;
                SymbolState_t CoreState;
                if (i == 0) CoreState = ST_CoreSTART;
                else if (isalpha(i)) CoreState = ST_CoreCHAR;
                else if (isdigit(i)) CoreState = ST_CoreDIGIT;
                else if (isspace(i)) CoreState = ST_CoreSPACE;
                else CoreState = ST_CoreSYMBOL;
                // START Table
                CoreStateTables[ST_CoreSTART][i] = CoreState;
                // SPACE Table
                NewState = CoreState;
                if (CoreState != ST_CoreSPACE) NewState |= StateACCEPTMask;
                CoreStateTables[ST_CoreSPACE][i] = NewState;
                // CHAR Table
                NewState = CoreState;
                if (CoreState != ST_CoreCHAR) NewState |= StateACCEPTMask;
                CoreStateTables[ST_CoreCHAR][i] = NewState;
                // DIGIT Table
                NewState = CoreState;
                if (CoreState != ST_CoreDIGIT) NewState |= StateACCEPTMask;
                CoreStateTables[ST_CoreDIGIT][i] = NewState;
                // SYMBOL Table
                NewState = (SymbolState_t)(CoreState | StateACCEPTMask);
                CoreStateTables[ST_CoreSYMBOL][i] = NewState;
                }
        return;
        }

// ****************************************************************************
// **** Lexical Parser ********************************************************
// ****************************************************************************

LexicalParser::LexicalParser(SymbolStateMachine &StateMachine)
        :
        Machine(StateMachine)
        {
        Reset();
        WideSubstitutionSymbol = WideSubstitutionSYMBOL;
        return;
        }

bool LexicalParser::DecodeSymbol(char Symbol)
        {
        SymbolState_t NewState;
        unsigned iSymbol = (unsigned char)Symbol;
        SymbolStateTableInfo *Table = Machine.GetStateTable(CurrentState);
        if (Table->CoreDefault) NewState = Table->CoreDefault[iSymbol];
        else if (Symbol) NewState = Table->StateDefault;
        else NewState = ST_CoreSTART | StateACCEPTMask;
        for (unsigned i=0; i < Table->NumOverrides; i++)
                {
                if (Table->Overrides[i].Symbol == Symbol)
                        {
                        NewState = Table->Overrides[i].NewState;
                        break;
                        }
                }
        if (NewState == ST_CoreERROR) return false;
        bool Accept = StateACCEPT(NewState) == StateACCEPTMask;
        NewState = StateSTATE(NewState);
        if (Accept)                             // Accept previous.
                {
                OnAccept(CurrentState,Buffer,Count);
                Count = 0;
                }
        if (NewState != ST_CoreSTART) Count++;
        CurrentState = NewState;
        return true;
        }

bool LexicalParser::DecodeSymbol(wchar_t wSymbol)
        {
        char Symbol;
        if (wSymbol >> 8) Symbol = WideSubstitutionSymbol;
        else Symbol = (char)wSymbol;
        return DecodeSymbol(Symbol);        
        }

void LexicalParser::Reset()
        {
        Count = 0;
        Buffer = NULL;
        CurrentState = ST_CoreSTART;
        return;
        }

void LexicalParser::Reset(SymbolState_t StartState)
        {
        Count = 0;
        Buffer = NULL;
        CurrentState = StartState;
        return;
        }

void LexicalParser::SetBuffer(const void *buffer)
        {
        Buffer = buffer;                // Just passed to Onxx() for info.
        return;
        }

void LexicalParser::SetWideSubstitutionSymbol(char Symbol)
        {
        WideSubstitutionSymbol = Symbol;
        return;
        }

// **************************************
// **** Default Handlers for Testing ****
// **************************************

void LexicalParser::OnAccept(SymbolState_t State,
                             const void *Buffer,
                             size_t Count)
        {
        printf("%.*s [%u]",(int)Count,(const char *)Buffer,State);
        return;
        }

// ****************************************************************************
// **** Lexical Stream ********************************************************
// ****************************************************************************

LexicalStream::LexicalStream(SymbolStateMachine &Machine)
        :
        LexicalParser(Machine)
        {
        Start((const char *)NULL,0);
        return;
        }

const char* LexicalStream::NextItem(LexicalItem *item)
        {
        if (!Continue || Item != item)          // Returns end parse position
                {                               // or NULL if input exhausted
                Item = item;                    // before end of item.
                Item->SymbolClass = SC_Null;
                Item->SymbolID = ST_CoreSTART;
                Item->String = InputString;
                Item->Length = 0;
                SetBuffer(InputString);
                }
        if (!Continue)                  // Current symbol already read.
                {
                if (GetCurrentState() == ST_CoreSTART)  // Previous input was
                        {                               // terminating ('\0').
                        return Item->String; 
                        }
                InputString += Wide ? sizeof(wchar_t) : sizeof(char);
                InputLength--;
                Item->Length++;
                Continue = true;
                }
        while (Continue && InputLength)
                {
                bool Status;
                if (!Wide) Status = DecodeSymbol(*InputString);
                else Status = DecodeSymbol(*(wchar_t *)InputString);
                if (!Status)
                        {
                        Item->SymbolClass = SC_Unknown;
                        Item->SymbolID = ST_CoreERROR;
                        break;
                        }
                if (!Continue) break;
                InputString += Wide ? sizeof(wchar_t) : sizeof(char);
                InputLength--;
                Item->Length++;
                }
        return Continue ? NULL : Item->String;
        }

void LexicalStream::OnAccept(SymbolState_t State, const void*, size_t)
        {
        if (State == ST_CoreSTART) Item->SymbolClass = SC_Null;
        else if (State == ST_CoreSPACE) Item->SymbolClass = SC_Space;
        else if (State == ST_CoreCHAR) Item->SymbolClass = SC_Token;
        else if (State == ST_CoreDIGIT) Item->SymbolClass = SC_Number;
        else if (State == ST_CoreSYMBOL) Item->SymbolClass = SC_Symbol;
        else Item->SymbolClass = SC_Unknown;
        Item->SymbolID = State;        
        Continue = false;
        return;
        }

void LexicalStream::Restart(const char *Input, size_t Length)
        {
        Wide = false;
        Continue = true;
        InputString = Input;
        InputLength = Length;
        return;
        }

void LexicalStream::Restart(const wchar_t *Input, size_t Length)
        {
        Restart((const char *)Input,Length);
        Wide = true;
        return;
        }

void LexicalStream::Start(const char *Input, size_t Length)
        {
        Reset();
        Item = NULL;
        Restart(Input,Length);
        return;
        }

void LexicalStream::Start(const wchar_t *Input, size_t Length)
        {
        Reset();
        Item = NULL;
        Restart(Input,Length);
        return;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************

