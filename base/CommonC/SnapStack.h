// Call Stack Snaphots: SnapStack.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: SnapStack.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2007-2008 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//
//
// Note: For Microsoft, link with: Microsoft SDK\lib\DbgHelp.Lib
//

#ifndef __Snap_CallStack

        #define __Snap_CallStack

#ifdef _WIN32
        #include <windows.h>
#endif

#ifdef __OS400_TGTVRM__ 
        #include <mih/matinvs.h>
#endif

#if defined(_WIN32)
        #define MAX_CALLSTACK_ENTRIES 32
        #define MAX_SYMNAME_SIZE 255
        #define MAX_MODNAME_SIZE 32
        #define MAX_PROGNAME_SIZE 256
#elif defined(_AIX)
        #define MAX_CALLSTACK_ENTRIES 32
        #define MAX_SYMNAME_SIZE 255
        #define MAX_MODNAME_SIZE 255
        #define MAX_PROGNAME_SIZE 255
#elif defined(__OS400_TGTVRM__)
        #define MAX_SYMNAME_SIZE 255
        #define MAX_MODNAME_SIZE 30
        #define MAX_PROGNAME_SIZE 30
        #define MAX_CONTEXT_SIZE 30
        #define MAX_NUMBER_STMT_ID 4
#else /* Linux */
        #define MAX_CALLSTACK_ENTRIES 32
        #define MAX_SYMNAME_SIZE 255
        #define MAX_MODNAME_SIZE 255
        #define MAX_PROGNAME_SIZE 256
#endif

class InvocationEntry
        {
        private:
                unsigned EntryIndex;
  #if defined(_WIN32)
                HANDLE hProcess;
                size_t AddrPC_Offset;
  #elif defined(_AIX)
                unsigned int *InstructionPointer;
  #elif defined(__OS400_TGTVRM__)
                char Context[MAX_CONTEXT_SIZE + 1];
                unsigned StatementIDs[MAX_NUMBER_STMT_ID];
  #else /* Linux */
                unsigned FunctionOffset;
                bool Init(void *InstructionPtr);
  #endif
                char Module[MAX_MODNAME_SIZE + 1];
                char Function[MAX_SYMNAME_SIZE + 1];
                char Program[MAX_PROGNAME_SIZE + 1];
                friend class InvocationCallStack;
        public:
                const char* GetContext();
                const char* GetProgram();
                const char* GetModule();
                const char* GetFunction();
                unsigned GetLineNumber();
        };

class InvocationCallStack
        {
        private:
  #if defined(_WIN32)
                static HANDLE hProcess;
                int nEntries;
                struct  {
                        size_t AddrPC_Offset;
                        }  CallStack[MAX_CALLSTACK_ENTRIES];
                friend struct ICS_Initializer; 
  #elif defined(__OS400_TGTVRM__)
                _MATINVS_Template_T *CallStack;
  #else /* Unix */
                int nEntries;
                void* CallStack[MAX_CALLSTACK_ENTRIES];
  #endif
        protected:
  #ifdef _WIN32
                bool NTEnableDebug();
                bool Snapshot(DWORD ThreadID);
  #endif
                InvocationEntry* GetEntry(int index);
        public:
                InvocationCallStack();
                ~InvocationCallStack();
                bool Snapshot();
  #ifdef _WIN32
                bool Snapshot(EXCEPTION_POINTERS *SEHInfo);
  #endif
                int GetNumEntries();
                void FreeEntry(InvocationEntry *Current);
                InvocationEntry* GetFirstEntry();
                InvocationEntry* GetLastEntry();
                InvocationEntry* GetNextEntry(InvocationEntry *Current);
                InvocationEntry* GetPrevEntry(InvocationEntry *Current);
        };

/* ****************************************************************************
 Notes:

     1. Windows SEH:

          __try
                {
                Test2();
                }
          __except (Stack.Snapshot(GetExceptionInformation()),
                    EXCEPTION_EXECUTE_HANDLER)
                {
                TERROR(("In Exception"));
                }

*******************************************************************************
**** Example: *****************************************************************
***************

*******************************************************************************
*******************************************************************************

#include <stdio.h>
#include <stdlib.h>

#include <SnapStack.h>
#include <LevelTrace.h>

void PrintStack(InvocationCallStack &Stack)
        {
        InvocationEntry *Current = Stack.GetFirstEntry();
        while (Current)
                {
                if (*Current->GetFunction())
                        {
                        TPRINT(("%s (%s:%u)",
                                Current->GetFunction(),
                                Current->GetModule(),
                                Current->GetLineNumber()));
                        }
                else    {
                        TPRINT(("%s (%u)",Current->GetProgram(),
                                          Current->GetLineNumber()));
                        }
                Current = Stack.GetNextEntry(Current);
                }
        return;
        }

void Test(InvocationCallStack &Stack)
        {
        Stack.Snapshot();
        return;        
        }

main()
        {
        InvocationCallStack Stack;
        Test(Stack);
        PrintStack(Stack);

  #ifdef __BORLANDC__
        printf("Press <enter> to continue\n");
        getc(stdin);
  #endif
        return 0;
        }

**************************************************************************** */

#endif /* __Snap_CallStack */
