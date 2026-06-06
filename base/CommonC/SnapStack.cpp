// Call Stack Snaphots: SnapStack.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: SnapStack.cpp
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

#include <stdio.h>
#include <stdlib.h>

#ifdef __OS400_TGTVRM__
        #include <except.h>
        #include <mih/matptrif.h>
        #include <mimchobs.h>
#endif

#ifdef _WIN32
        #include <windows.h>
        #include <process.h>
        #include <imagehlp.h>
        #include <tchar.h>
        #include <setjmp.h>

        #ifdef __BORLANDC__
                typedef unsigned long DWORD_PTR;
                #define STACKFRAME64 STACKFRAME
                #define StackWalk64 StackWalk
                #define SymFunctionTableAccess64 SymFunctionTableAccess
                #define SymGetModuleBase64 SymGetModuleBase
        #endif
#endif

#include <SnapStack.h>
#include <LevelTrace.h>

#ifdef _AIX
        #include <string.h>
        #include <execargs.h>
        #include <ucontext.h>
        #include <sys/debug.h>
        //
        // Move AIX stack pointer (r1) into return register (r3)
        //
        #if !defined (__clang__)
                void** getstkptr(void);
                #pragma mc_func getstkptr {"7c230b78"}          // ASM: mr r3,r1
                #pragma reg_killed_by getstkptr gr3
        #endif

        #define is_sp_valid(sp,sp_min)                                        \
                (((sp) <= (unsigned long *)(TopOfBaseStack->main_reg[1])) &&  \
                 ((sp) >= (sp_min)) &&                                        \
                 (((unsigned long)(sp) & (unsigned long)0x7) == 0))

        #define MAX_FUNC_SEARCH_LEN 10000

#elif defined(__GNUC__) /* Linux */

        #include <string.h>
        #include <execinfo.h>
        #include <cxxabi.h> 
#endif

// ***************************************************************************
// **** Utilities ************************************************************
// ***************************************************************************

static void CHAR2UC(char *string, 
                    const char *array, 
                    int length)                 // Requires length + 1 bytes.
        {
        if (array)
                {
                while (length && array[length-1] == ' ') length--;
                }
        else length = 0;
        while (length--) *string++ = *array++;
        *string = '\0';
        return;
        }

static const char* ShortName(const char *Program)
        {
        const char *Pos = Program + strlen(Program);
        while (Pos != Program && *Pos != '\\' && *Pos != '/') Pos--;
        if (*Pos == '\\' || *Pos == '/') Pos++;
        return Pos;
        }

#ifdef _WIN32

static char* GetApplicationDir()
        {
        static char ModulePath[MAX_PATH+1] = {0};
        if (ModulePath[0]) return ModulePath;
        size_t ModulePathLen = GetModuleFileName(NULL,ModulePath,MAX_PATH);
        if (!ModulePathLen || ModulePathLen > MAX_PATH) return NULL;
        char *Program = ModulePath + ModulePathLen;
        while (Program != ModulePath && *Program != '\\') Program--;
        if (*Program == '\\') *Program = '\0';
        return ModulePath;
        }

static char* GetPDBPath()
        {
        static char PDBPath[MAX_PATH+1] = {0};
        if (PDBPath[0]) return PDBPath;
        const char *ApplicationDir = GetApplicationDir();
        if (!ApplicationDir || !ApplicationDir[0])
                {                               // Use 'DEFAULT' PDB Path:
                return NULL;                    // cwd, _NT_SYMBOL_PATH, 
                }                               // _NT_ALTERNATE_SYMBOL_PATH.
        const char *NTSymbolPath = getenv("_NT_SYMBOL_PATH");
        const char *NTAltSymbolPath = getenv("_NT_ALTERNATE_SYMBOL_PATH");
        char *Pos = PDBPath;
        size_t Remain = MAX_PATH;
        size_t Length = strlen(ApplicationDir);
        if (Length > Remain)
                {
                return NULL;                    // Something is wrong!
                }
        memcpy(Pos,ApplicationDir,Length);
        Pos += Length;
        Remain -= Length;
        if (NTSymbolPath)
                {
                Length = strlen(NTSymbolPath);
                if (Length + 1 <= Remain)
                        {
                        *Pos = ';';
                        memcpy(Pos+1,NTSymbolPath,Length);
                        Pos += Length + 1;
                        Remain -= Length + 1;
                        }
                }
        if (NTAltSymbolPath)
                {
                Length = strlen(NTAltSymbolPath);
                if (Length + 1 <= Remain)
                        {
                        *Pos = ';';
                        memcpy(Pos+1,NTAltSymbolPath,Length);
                        Pos += Length + 1;
                        Remain -= Length + 1;
                        }
                }
        *Pos = '\0';
        return PDBPath;
        }

#endif /* _WIN32 */

// ***************************************************************************
// **** InvocationCallStack (Generic) ****************************************
// ***************************************************************************

void InvocationCallStack::FreeEntry(InvocationEntry *Current)
        {
        if (Current) delete Current;
        return;
        }

InvocationEntry* InvocationCallStack::GetFirstEntry()
        {
        return GetNumEntries() ? GetEntry(0) : NULL;
        }

InvocationEntry* InvocationCallStack::GetLastEntry()
        {
        int nEntries = GetNumEntries();
        return nEntries ? GetEntry(nEntries-1) : NULL;
        }

InvocationEntry* InvocationCallStack::GetNextEntry(InvocationEntry *Current)
        {
        if (!Current) return NULL;
        int index = Current->EntryIndex + 1;
        delete Current;
        return index < GetNumEntries() ? GetEntry(index) : NULL;
        }

InvocationEntry* InvocationCallStack::GetPrevEntry(InvocationEntry *Current)
        {
        if (!Current) return NULL;
        if (Current->EntryIndex == 0)
                {
                delete Current;
                return NULL;
                }
        int index = Current->EntryIndex - 1;
        delete Current;
        return GetEntry(index);
        }

#ifdef __OS400_TGTVRM__

// ***************************************************************************
// **** iSeries **************************************************************
// ***************************************************************************

const char* InvocationEntry::GetContext()
        {
        return Context;
        }

const char* InvocationEntry::GetProgram()
        {
        return Program;
        }

const char* InvocationEntry::GetModule()
        {
        return Module;
        }

const char* InvocationEntry::GetFunction()
        {
        return Function;
        }

unsigned InvocationEntry::GetLineNumber()
        {
        return StatementIDs[0];
        }

// *****************************************
// **** Invocation Call Stack (iSeries) ****
// *****************************************

InvocationCallStack::InvocationCallStack()
        {
        CallStack = NULL;
        return;
        }

InvocationCallStack::~InvocationCallStack()
        {
        if (CallStack) free(CallStack);
        return;
        }

InvocationEntry* InvocationCallStack::GetEntry(int index)
        {
        if (!CallStack || index >= CallStack->Num_Inv_Ent) return NULL;

        char Function[255];
        int SelectionMask = 0x1A280000;  // Lib, Program, Module, Function...
        _MPTIF_Template_T EntryInfo;
        _MPTIF_Selection_T SelectionTemplate;

        InvocationEntry *Entry = new InvocationEntry;
        if (!Entry) return NULL;
        Entry->EntryIndex = index;

        memcpy(&SelectionTemplate,&SelectionMask,4);
        EntryInfo.Template_Size = sizeof(EntryInfo);
        EntryInfo.PointerType = 0x08;
        EntryInfo.LenProcNameReq = sizeof(Function);
        EntryInfo.LenProcNameAvail = 0;
        EntryInfo.ProcName = Function;
        EntryInfo.NumStmtIdsReq = MAX_NUMBER_STMT_ID;
        EntryInfo.StmtIds = Entry->StatementIDs;
                
        _MIS_Inv_Ent *EntryPtr = CallStack->_Inv_Ent;
        _SUSPENDPTR SuspendPoint = EntryPtr[index].Sus_Point;

        strcpy(Entry->Function,"*");

  #pragma exception_handler (GetEntry_Handler,                              \
                             0,                                             \
                             0,                                             \
                             _C2_MH_ESCAPE | _C2_MH_FUNCTION_CHECK,         \
                             _CTLA_HANDLE)

  #pragma exception_handler (GetEntry_Domain_Handler,                       \
                             0,                                             \
                             0,                                             \
                             _C2_MH_ESCAPE,                                 \
                             _CTLA_HANDLE,                                  \
                             "MCH6801")

        _MATPTRIF(&EntryInfo,&SuspendPoint,&SelectionTemplate);

  #pragma disable_handler

        size_t FunctionLen = MAX_SYMNAME_SIZE;
        if (EntryInfo.LenProcNameAvail < FunctionLen) 
                {
                FunctionLen = EntryInfo.LenProcNameAvail;
                }
        CHAR2UC(Entry->Function,Function,FunctionLen);
        CHAR2UC(Entry->Program,EntryInfo.ProgName,MAX_PROGNAME_SIZE);
        CHAR2UC(Entry->Context,EntryInfo.ProgContextName,MAX_CONTEXT_SIZE);
        CHAR2UC(Entry->Module,EntryInfo.ModName,MAX_MODNAME_SIZE);
        goto GetEntry_Exit;

  GetEntry_Domain_Handler:

        strcpy(Entry->Function, "* (System Domain)");

  GetEntry_Handler:

        strcpy(Entry->Program,"*");
        strcpy(Entry->Context,"*");
        strcpy(Entry->Module, "*");
        Entry->StatementIDs[0] = 0;

  GetEntry_Exit:

        return Entry;
        }

int InvocationCallStack::GetNumEntries()
        {
        return CallStack->Num_Inv_Ent;
        }

bool InvocationCallStack::Snapshot()
        {
        unsigned nEntries = 32;
  Snap: if (CallStack) free(CallStack);
        size_t CallStackSize = sizeof(_MATINVS_Template_T) +
                               (nEntries-1) * sizeof(_MIS_Inv_Ent);
        CallStack = (_MATINVS_Template_T *)malloc(CallStackSize);
        if (!CallStack) return false;
        CallStack->Template_Size = CallStackSize;

  #pragma exception_handler (Snapshot_Handler,                         \
                             0,                                        \
                             0,                                        \
                             _C2_MH_ESCAPE | _C2_MH_FUNCTION_CHECK,    \
                             _CTLA_HANDLE)

        _MATINVS2(CallStack);

  #pragma disable_handler

        if (CallStack->Template_Size < CallStack->Bytes_Used)
                {
                nEntries = (CallStack->Bytes_Used - 
                            sizeof(_MATINVS_Template_T)) /
                            sizeof(_MIS_Inv_Ent) + 1;
                goto Snap;
                }
        return true;

  Snapshot_Handler:

        return false;
        }

#elif defined(_AIX)

// ***************************************************************************
// **** AIX ******************************************************************
// ***************************************************************************

const char* InvocationEntry::GetContext()
        {
        return "";
        }

const char* InvocationEntry::GetProgram()
        {
        return "";
        }

const char* InvocationEntry::GetModule()
        {
        return "";
        }

const char* InvocationEntry::GetFunction()
        {
        strcpy(Function,"(unknown)"); /*CCQ_SECURE_LIB_OK*/
        unsigned int *ip = InstructionPointer;
        // -------------------------------------------------------
        // Look for traceback table at begining of function. It is
        // identified by a word-aligned word of zeroes.
        // -------------------------------------------------------
        unsigned int searchcount = 0;
        while((*ip != NULL) && (searchcount++ < MAX_FUNC_SEARCH_LEN)) ip++;
        if(*ip != 0) return Function;
        tbtable *tb = (struct tbtable *)(ip+1);
        if (tb->tb.globallink != true)  // If not global linkage routine:
                {
                // -------------------------------------------------------
                // Walk the traceback table looking for the funciton name.
                // -------------------------------------------------------
                ip = (unsigned int *) tb + 
                     sizeof(struct tbtable_short)/sizeof(int);

                if (tb->tb.fixedparms != 0 || tb->tb.floatparms != 0 ) ip++;
                if (tb->tb.has_tboff) ip++;
                if (tb->tb.int_hndl) ip++;
                if (tb->tb.has_ctl) ip += (*ip) + 1; /* don't care */

                if (tb->tb.name_present == true)
                        {
                        unsigned name_len = *(short *)ip;
                        if (name_len > MAX_SYMNAME_SIZE - 2)
                                {
                                name_len = MAX_SYMNAME_SIZE - 2;
                                }
                        memcpy(Function,(char *)ip+sizeof(short),name_len);
                        Function[name_len] = '(';
                        Function[name_len+1] =')';
                        Function[name_len+2] = '\0';
                        }
                }
        return Function;
        }

unsigned InvocationEntry::GetLineNumber()
        {
        return 0;
        }

// *************************************
// **** Invocation Call Stack (AIX) ****
// *************************************

InvocationCallStack::InvocationCallStack()
        {
        nEntries = 0;
        return;
        }

InvocationCallStack::~InvocationCallStack()
        {
        return;
        }

InvocationEntry* InvocationCallStack::GetEntry(int index)
        {
        InvocationEntry *Entry = new InvocationEntry;
        if (!Entry) return NULL;
        Entry->EntryIndex = index;
        Entry->InstructionPointer = (unsigned int *)CallStack[index];
        return Entry;
        }

int InvocationCallStack::GetNumEntries()
        {
        return nEntries;
        }

bool InvocationCallStack::Snapshot()
        {
        static const char *ProcName = "InvocationCallStack::Snapshot";
        nEntries = 0;
        unsigned long *sp_min;
  #if defined (__clang__)
        __asm__ __volatile__
                (
                "mr %0,1\n\t"
                :"=r"( sp_min )::
		);
  #else
        sp_min = (unsigned long *)getstkptr();
  #endif
        unsigned long *sp = sp_min;
        unsigned long *lr = NULL;
        //
        //                              | Current lr       |
        //      Previous frame:         | Backchain sp     |
        //                              |                  |
        //                              |                  |
        //      Current frame:          | sp               |
        //      Next frame:             | sp               |
        //
        //      
        // Note lr from the next frame is the next instruction in
        // the current frame. When current fame calls a function
        // the called function places lr (the next instruction in
        // the current frame) into the current frame lr position;
        // then the 
        //
        while(is_sp_valid(sp,sp_min) && nEntries < MAX_CALLSTACK_ENTRIES)
                {
                if (lr)
                        {                           // Link Register stored in n-1
                        CallStack[nEntries++] = lr; // position; 16 byte aligned.
                        }                           // lr points to the next ip. 
                //
                // Prevoius frame...
                //
                sp_min = sp; 
                sp = (unsigned long *)*sp;
                lr = (unsigned long *)*(sp+2);
                }
        if (sp)
                {
                // The call to TERROR() also serves as an external call
                // so SnapStack() itself builds a frame; otherwise we 
                // wouldn't see our caller.
                TERROR(("%s: Invalid stack address: 0x%lx",ProcName,sp));
                }
        return true;
        }

#elif defined(_WIN32)

// ***************************************************************************
// **** WinNT ****************************************************************
// ***************************************************************************

const char* InvocationEntry::GetContext()
        {
        return "";
        }

const char* InvocationEntry::GetProgram()
        {
        IMAGEHLP_MODULE ModuleInfo;
        memset(&ModuleInfo,0,sizeof(ModuleInfo));
        ModuleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
        BOOL Status = SymGetModuleInfo(hProcess,
                                       AddrPC_Offset,
                                       &ModuleInfo);
        if (!Status) return "*";
        CHAR2UC(Program,ModuleInfo.ImageName,MAX_PROGNAME_SIZE);
        return Program;
        }

const char* InvocationEntry::GetModule()
        {
  #ifndef __BORLANDC__
        DWORD Displacement = 0;
        IMAGEHLP_LINE ImageInfo;
        BOOL Status = SymGetLineFromAddr(hProcess,
                                         AddrPC_Offset,
                                         &Displacement,
                                         &ImageInfo);
        if (!Status) return "*";
        CHAR2UC(Module,ShortName(ImageInfo.FileName),MAX_MODNAME_SIZE);
        return Module;
  #else
        IMAGEHLP_MODULE ModuleInfo;
        memset(&ModuleInfo,0,sizeof(ModuleInfo));
        ModuleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
        BOOL Status = SymGetModuleInfo(hProcess,
                                       AddrPC_Offset,
                                       &ModuleInfo);
        if (!Status) return "*";
        CHAR2UC(Module,ModuleInfo.ModuleName,MAX_MODNAME_SIZE);
  #endif  
        return Module;
        }

const char* InvocationEntry::GetFunction()
        {
        DWORD_PTR Displacement = 0;
        struct IMAGEHLP_SYMBOL_MEM : IMAGEHLP_SYMBOL 
                {
                char NameMemory[MAX_SYMNAME_SIZE+1];
                } Symbol;

        memset(&Symbol,0,sizeof(Symbol)); 
        Symbol.SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
        Symbol.MaxNameLength = MAX_SYMNAME_SIZE;

        BOOL Status = SymGetSymFromAddr(hProcess,
                                        AddrPC_Offset,
                                        &Displacement,
                                        &Symbol);
        if (Status) 
                {
                CHAR2UC(Function,Symbol.Name,Symbol.MaxNameLength);
                }
        else    {
                sprintf(Function,"* (ip=0x%p)",AddrPC_Offset);
                }
        return Function;
        }

unsigned InvocationEntry::GetLineNumber()
        {
  #ifndef __BORLANDC__
        DWORD Displacement = 0;
        IMAGEHLP_LINE ImageLine;
        BOOL Status = SymGetLineFromAddr(hProcess,
                                         AddrPC_Offset,
                                         &Displacement,
                                         &ImageLine);
        return Status ? ImageLine.LineNumber : 0;
  #else
        return 0;
  #endif  
        }

// ************************************
// **** Invocation Call Stack (NT) ****
// ************************************

HANDLE InvocationCallStack::hProcess = NULL;

// -------------------------
// ---- ICS_Initializer ----
// -------------------------

struct ICS_Initializer
        {
        ~ICS_Initializer() {Close();}
        void Close();
        bool NTEnableDebug();
        } static _ICS_Initializer;

void ICS_Initializer::Close()
        {
        if (InvocationCallStack::hProcess)
                {
                CloseHandle(InvocationCallStack::hProcess);
                InvocationCallStack::hProcess = NULL;
                }
        return;
        }

bool ICS_Initializer::NTEnableDebug()
        {
        static const char *ProcName = "ICS_Initializer::NTEnableDebug";
        if (InvocationCallStack::hProcess) return true;
        BOOL Status;
        HANDLE hProcessToken;
        LUID DebugPrivilege;
        TOKEN_PRIVILEGES Privileges;
        TOKEN_PRIVILEGES PrevPrivileges;
        InvocationCallStack::hProcess = OpenProcess(PROCESS_ALL_ACCESS,
                                                    FALSE,
                                                    GetCurrentProcessId());
        if (!InvocationCallStack::hProcess)
                {
                TERROR(("%s: OpenProcess failed: %d",ProcName,
                                                     GetLastError()));
                
                return false;
                }
        if (!OpenProcessToken(InvocationCallStack::hProcess,
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                              &hProcessToken)) 
                {
                TERROR(("%s: OpenProcessToken failed: %d",ProcName,
                                                          GetLastError()));
                Close();
                return false;
                }
        if (!LookupPrivilegeValue("","SeDebugPrivilege",&DebugPrivilege))
                {
                TERROR(("%s: LookupPrivilegeValue failed: %d",
                        ProcName,
                        GetLastError()));
                CloseHandle(hProcessToken);
                Close();
                return false;
                }
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Luid = DebugPrivilege;
        Privileges.Privileges[0].Attributes = 0;
        DWORD PrevPrivilegesLen = sizeof(TOKEN_PRIVILEGES);
        Status = AdjustTokenPrivileges(hProcessToken,
                                       FALSE,
                                       &Privileges,
                                       sizeof(TOKEN_PRIVILEGES),
                                       &PrevPrivileges,
                                       &PrevPrivilegesLen);
        if (!Status) 
                {
                TERROR(("%s: AdjustTokenPrivileges failed: %d",
                        ProcName,
                        GetLastError()));
                CloseHandle(hProcessToken);
                Close();
                return false;
                }
        DWORD PrevAttributes;
        PrevAttributes = Privileges.Privileges[0].Attributes;
        Privileges.Privileges[0].Attributes = PrevAttributes |
                                              SE_PRIVILEGE_ENABLED;
        Status = AdjustTokenPrivileges(hProcessToken,
                                       FALSE,
                                       &Privileges,
                                       sizeof(TOKEN_PRIVILEGES),
                                       NULL,
                                       0);
        if (!Status) 
                {
                TERROR(("%s: AdjustTokenPrivileges failed: %d",
                        ProcName,
                        GetLastError()));
                CloseHandle(hProcessToken);
                Close();
                return false;
                }
  #ifndef __BORLANDC__
        SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
  #endif
        const char *PDBPath = GetPDBPath();
        Status = SymInitialize(InvocationCallStack::hProcess,
                               GetPDBPath(),
                               TRUE);
        if (!Status) 
                {
                TERROR(("%s: SymInitialize failed: %d",
                        ProcName,
                        GetLastError()));
                Close();
                return false;
                }
        CloseHandle(hProcessToken);
        return true;
        }

// -----------------------------
// ---- InvocationCallStack ----
// -----------------------------

InvocationCallStack::InvocationCallStack()
        {
        nEntries = 0;
        return;
        }

InvocationCallStack::~InvocationCallStack()
        {
        return;
        }

InvocationEntry* InvocationCallStack::GetEntry(int index)
        {
        InvocationEntry *Entry = new InvocationEntry;
        if (!Entry) return NULL;
        Entry->EntryIndex = index;
        Entry->hProcess = hProcess;
        Entry->AddrPC_Offset = CallStack[index].AddrPC_Offset;
        return Entry;
        }

int InvocationCallStack::GetNumEntries()
        {
        return nEntries;
        }

bool InvocationCallStack::Snapshot(EXCEPTION_POINTERS *SEHInfo)
        {
        static const char *ProcName = "ICS::Snapshot";
        BOOL Status;
        STACKFRAME64 StackFrame = {0};
        if (!hProcess && !_ICS_Initializer.NTEnableDebug()) 
                {
                TERROR(("%s: NTEnableDebug() Failed",ProcName));
                return false;
                }
        if (!SEHInfo || !SEHInfo->ContextRecord)
                {
                TERROR(("%s: Missing SEHInfo",ProcName));
                return false;
                }
        HANDLE hThread = GetCurrentThread();
        CONTEXT *Context = SEHInfo->ContextRecord;
  #if _M_IX86
        StackFrame.AddrPC.Offset = Context->Eip;
        StackFrame.AddrStack.Offset = Context->Esp;
        StackFrame.AddrFrame.Offset = Context->Ebp;
  #else
        StackFrame.AddrPC.Offset = Context->Rip;
        StackFrame.AddrStack.Offset = Context->Rsp;
        StackFrame.AddrFrame.Offset = Context->Rbp;
  #endif
        StackFrame.AddrPC.Mode = AddrModeFlat;
        StackFrame.AddrStack.Mode = AddrModeFlat;
        StackFrame.AddrFrame.Mode = AddrModeFlat;
        for (nEntries = 0; nEntries < MAX_CALLSTACK_ENTRIES; nEntries++)
                {
                Status = StackWalk64(
  #if _M_IX86
                                     IMAGE_FILE_MACHINE_I386,
  #else 
                                     IMAGE_FILE_MACHINE_AMD64,
  #endif
                                     hProcess,
                                     hThread,
                                     &StackFrame,
                                     Context,
                                     NULL,
                                     SymFunctionTableAccess64,
                                     SymGetModuleBase64,
                                     NULL);
                if(!Status) break;
                size_t AddrPC_Offset = (size_t)StackFrame.AddrPC.Offset;
                CallStack[nEntries].AddrPC_Offset = AddrPC_Offset;
                }
        return true;
        }

bool InvocationCallStack::Snapshot(DWORD ThreadID)
        {
        static const char *ProcName = "ICS::Snapshot";
        HANDLE hThread = NULL;
        char _context[sizeof(CONTEXT) + 15];
        char *_aligned_context = (char *)(((size_t)_context + 15) & ~0x0F);
        CONTEXT &Context = *(CONTEXT *)_aligned_context;
        STACKFRAME64 StackFrame = {0};
        if (!hProcess && !_ICS_Initializer.NTEnableDebug()) 
                {
                TERROR(("%s: NTEnableDebug() Failed",ProcName));
                return false;
                }
        if (ThreadID != GetCurrentThreadId()) 
                {
  #ifdef __BORLANDC__
                TERROR(("%s: Only Current Thread Supported",ProcName));
  #else
                hThread = OpenThread(THREAD_ALL_ACCESS,FALSE,ThreadID);
  #endif
                if (!hThread)
                        {
                        TERROR(("%s: OpenThread(0x%X) Failed",ProcName,
                                                              ThreadID));
                        return false;
                        }
                DWORD SuspendCount = SuspendThread(hThread);
                if (SuspendCount == -1)
                        {
                        TERROR(("%s: SuspendThread() Failed (%d)",
                                ProcName,
                                GetLastError()));
                        CloseHandle(hThread);
                        return false;
                        }
                BOOL Status = GetThreadContext(hThread,&Context);
                if (Status)
                        {
    #if _M_IX86
                        StackFrame.AddrPC.Offset = Context.Eip;
                        StackFrame.AddrStack.Offset = Context.Esp;
                        StackFrame.AddrFrame.Offset = Context.Ebp;
    #else
                        StackFrame.AddrPC.Offset = Context.Rip;
                        StackFrame.AddrStack.Offset = Context.Rsp;
                        StackFrame.AddrFrame.Offset = Context.Rbp;
    #endif
                        }
                }
        else    {
  #ifndef __BORLANDC__
                RtlCaptureContext(&Context);
    #if _M_IX86
                StackFrame.AddrPC.Offset = Context.Eip;
                StackFrame.AddrStack.Offset = Context.Esp;
                StackFrame.AddrFrame.Offset = Context.Ebp;
    #else
                StackFrame.AddrPC.Offset = Context.Rip;
                StackFrame.AddrStack.Offset = Context.Rsp;
                StackFrame.AddrFrame.Offset = Context.Rbp;
             // StackFrame.AddrFrame.Offset = Context.Rsp;
    #endif
  #else /* __BORLANDC__ */
                jmp_buf jmpbuf;
                setjmp(jmpbuf);
                StackFrame.AddrPC.Offset = jmpbuf[0].j_ret;
                StackFrame.AddrStack.Offset = jmpbuf[0].j_esp;
                StackFrame.AddrFrame.Offset = jmpbuf[0].j_ebp;
  #endif
                }
        StackFrame.AddrPC.Mode = AddrModeFlat;
        StackFrame.AddrStack.Mode = AddrModeFlat;
        StackFrame.AddrFrame.Mode = AddrModeFlat;
        for (nEntries = 0; nEntries < MAX_CALLSTACK_ENTRIES; nEntries++)
                {
                BOOL Status = StackWalk64(
  #if _M_IX86                             
                                          IMAGE_FILE_MACHINE_I386,
  #else                                   
                                          IMAGE_FILE_MACHINE_AMD64,
  #endif                                  
                                          hProcess,
                                          hThread,
                                          &StackFrame,
                                          &Context,
                                          NULL,
                                          SymFunctionTableAccess64,
                                          SymGetModuleBase64,
                                          NULL);
                if(!Status) break;
                size_t AddrPC_Offset = (size_t)StackFrame.AddrPC.Offset;
                CallStack[nEntries].AddrPC_Offset = AddrPC_Offset;
                }
        if (hThread) 
                {
                ResumeThread(hThread);
                CloseHandle(hThread);
                }
        return true;
        }

bool InvocationCallStack::Snapshot()
        {
        return Snapshot(GetCurrentThreadId());
        }

#else /* Linux */

// ***************************************************************************
// **** Linux ****************************************************************
// ***********
//
//      Note: If all symbols do not appear in program, compile with -rdynamic
//
// ***************************************************************************

#define hexval(x) (((x) <= '9') ? ((x) - '0') : ((x) - 'A' + 10))

#define atoiXC(x) (hexval((x)[0]) * 16 + hexval((x)[1]))

inline unsigned axtoul(const char *s)
        {
        unsigned rc = 0;
        if (s[0] == '0' && s[1] == 'x')
                {
                for (s+=2; *s; s++) rc = rc * 16 + hexval(*s);
                }
        return rc;
        }

const char* InvocationEntry::GetContext()
        {
        return "";
        }

const char* InvocationEntry::GetProgram()
        {
        return "";
        }

const char* InvocationEntry::GetModule()
        {
        return Module;
        }

const char* InvocationEntry::GetFunction()
        {
        return Function;
        }

unsigned InvocationEntry::GetLineNumber()
        {
        return FunctionOffset;
        }

bool InvocationEntry::Init(void *InstructionPtr)
        {
        Module[0] = '\0';
        Function[0] = '\0';
        FunctionOffset = 0;
        void* const *buffer = (void* const *)&InstructionPtr;
        char **StackEntry = backtrace_symbols(buffer,1);
        if (StackEntry && *StackEntry)
                {
                char *ModulePTR = StackEntry[0];
                char *Pos = ModulePTR;
                char *FunctionPTR = NULL;
                char *OffsetPTR = NULL;
                while (*Pos && *Pos != '(') Pos++;
                if (*Pos == '(') 
                        {
                        *Pos = '\0';
                        FunctionPTR = ++Pos;
                        while (*Pos && *Pos != ')') Pos++;
                        *Pos = '\0';
                        while (Pos != FunctionPTR) 
                                {
                                if (*Pos == '-' || *Pos == '+') 
                                        {
                                        OffsetPTR = Pos + 1;
                                        *Pos = '\0';
                                        break;
                                        }
                                Pos--;
                                }
                        }
                if (ModulePTR)
                        {
                        size_t MinLength = strlen(ModulePTR);
                        size_t MaxLength = sizeof(Module) - 1;
                        if (MinLength > MaxLength) MinLength = MaxLength;
                        memcpy(Module,ModulePTR,MinLength);
                        Module[MinLength] = '\0';
                        }
                if (FunctionPTR)
                        {
                        int Status = 0;
                        char *DemangleFn = abi::__cxa_demangle(FunctionPTR,
                                                               NULL,
                                                               0,
                                                               &Status);
                        if (DemangleFn) FunctionPTR = DemangleFn;
                        size_t MinLength = strlen(FunctionPTR);
                        size_t MaxLength = sizeof(Function) - 1;
                        if (MinLength > MaxLength) MinLength = MaxLength;
                        memcpy(Function,FunctionPTR,MinLength);
                        Function[MinLength] = '\0';
                        if (DemangleFn) free(DemangleFn);
                        }
                if (OffsetPTR)
                        {
                        FunctionOffset = axtoul(OffsetPTR);
                        }
                free(StackEntry);
                }
        return StackEntry != NULL;
        }

// ***************************************
// **** Invocation Call Stack (Linux) ****
// ***************************************

InvocationCallStack::InvocationCallStack()
        {
        nEntries = 0;
        return;
        }

InvocationCallStack::~InvocationCallStack()
        {
        return;
        }

InvocationEntry* InvocationCallStack::GetEntry(int index)
        {
        InvocationEntry *Entry = new InvocationEntry;
        if (!Entry) return NULL;
        Entry->EntryIndex = index;
        Entry->Init(CallStack[index]);
        return Entry;
        }

int InvocationCallStack::GetNumEntries()
        {
        return nEntries;
        }

bool InvocationCallStack::Snapshot()
        {
        nEntries = backtrace(CallStack,MAX_CALLSTACK_ENTRIES);
        for (unsigned i=0; i < nEntries/2; i++)
                {
                void *Hold = CallStack[i];
                CallStack[i] = CallStack[nEntries - i - 1];
                CallStack[nEntries - i - 1] = Hold;
                }
        return nEntries > 0 ? true : false;
        }

#endif /* Linux */

// ***************************************************************************
// ***************************** End of File *********************************
// ***************************************************************************
