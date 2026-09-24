// popen for NT : popenNT.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: popenNT.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2009 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Note: Doesn't Require a Console like VS c-rutime popen.
//
//            

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
        #include <io.h>
#else
        #include <sys/wait.h>
        #include <signal.h>
        #include <unistd.h>
        #define HANDLE int
#endif

#include <ASThread.h>
#include <LinkList.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <popenNT.h>

// ***************************************************************************
// **** ShortName ************************************************************
// ***************************************************************************

static const char* ShortName(const char *Program)
        {
        if (!Program) return "<null>";
        const char *Pos = Program + strlen(Program);
        while (Pos != Program && *Pos != '\\' && *Pos != '/') Pos--;
        if (*Pos == '\\' || *Pos == '/') Pos++;
        return Pos;
        }

// ***************************************************************************
// **** Command2argv *********************************************************
// ***************************************************************************

static size_t cmdtokspc(char *Dest, const char* &End)
        {
        enum State_t {InChar, InSpace, InEscape, InQuote, InDQuote} State;
        if (!End || !*End) return 0;
        size_t Length = 0;
        if (isspace(*End)) State = InSpace;
        else if (*End == '\'') 
                {
                State = InQuote;
                End++;
                }
        else if (*End == '"') 
                {
                State = InDQuote;
                End++;
                }
        else State = InChar;
        while (*End)
                {
                if (State == InQuote)
                        {
                        if (*End == '\'')
                                {
                                End++;
                                break;
                                }
                        if (Dest) *Dest++ = *End;
                        Length++;
                        End++;
                        }
                else if (State == InEscape)
                        {
                        if (*End != '$' && 
                            *End != '`' && 
                            *End != '\\' && 
                            *End != '"')
                                {
                                if (Dest) *Dest++ = '\\';
                                Length++;
                                }
                        else    {
                                if (Dest) *Dest++ = *End;
                                Length++;
                                End++;
                                }
                        State = InDQuote;
                        }
                else if (State == InDQuote)
                        {
                        if (*End == '\\')
                                {
                                State = InEscape;
                                End++;
                                continue;
                                }
                        else if (*End == '"')
                                {
                                End++;
                                break;
                                }
                        if (Dest) *Dest++ = *End;
                        Length++;
                        End++;
                        }
                else if (State == InChar)
                        {
                        if (isspace(*End)) break;
                        else if (*End == '\'') 
                                {
                                State = InQuote;
                                End++;
                                }
                        else if (*End == '"') 
                                {
                                State = InDQuote;
                                End++;
                                }
                        else    {
                                if (Dest) *Dest++ = *End;
                                Length++;
                                End++;
                                }
                        }
                else if (State == InSpace)
                        {
                        if (!isspace(*End)) break;
                        Length++;
                        End++;
                        }
                }
        return Length;
	}

// ************************
// **** ExtractProgram ****
// ************************

static char* ExtractProgram(const char *Command)
        {
        char *ProgramName = NULL;
        const char *End = Command;
        size_t ProgramLen = cmdtokspc(NULL,End);
        if (ProgramLen)
                {
                ProgramName = (char *)malloc(ProgramLen+1);
                if (ProgramName)
                        {
                        cmdtokspc(ProgramName,Command);
                        ProgramName[ProgramLen] = '\0';
                        }
                }
        return ProgramName;
        }

// **********************
// **** Command2argv ****
// **********************

static int _Command2argv(const char *Command, char **argv)
        {
        static const char *ProcName = "_Command2argv";
        const char *End = Command;
        int argc = 0;
        while (*End)
                {
                const char *Start = End;
                size_t Length = cmdtokspc(NULL,End);
                if (!Length && Start[0] != '\'' && Start[0] != '"') break;
                if (isspace(*Start)) continue;
                if (argv)
                        {
                        char *Arg = (char *)malloc(Length+1);
                        if (!Arg)
                                {
                                TERROR(("%s: malloc() failed",ProcName));
                                return -1;
                                }
                        End = Start;
                        cmdtokspc(Arg,End);
                        Arg[Length] = '\0';
                        argv[argc] = Arg;
                        }
                argc++;
                }
        return argc;
        }

void FreeargvC(int argc, char ***argv)
        {
        if (!argv) return;
        for (int i=0; i < argc; i++)
                {
                if ((*argv)[i]) free((*argv)[i]);
                }
        if (*argv)
                {
                free(*argv);
                *argv = NULL;
                }
        return;
        }

int Command2argv(const char *Command, char ***argv)
        {
        static const char *ProcName = "Command2argv";
        int argc = _Command2argv(Command,NULL);
        if (argc)
                {
                *argv = (char **)calloc(argc,sizeof(char *)); // all slots NULL.
                if (!*argv)
                        {
                        TERROR(("%s: malloc() failed",ProcName));
                        return -1;
                        }
                int rc = _Command2argv(Command,*argv);
                if (rc < 0)
                        {
                        TERROR(("%s: _Command2argv failed",ProcName));
                        FreeargvC(argc,argv);
                        argc = -1;
                        }
                }
        return argc;
        }

// **************************
// **** ShellPrefix2argv ****
// **************************

int ShellPrefix2argv(const char *ShellPrefix, 
                     int argcIn, 
                     const char* argvIn[], 
                     char ***argv)
        {
        static const char *ProcName = "ShellPrefix2argv";
        MemoryPrintf Spine;
        if (argcIn < 0) return -1;
        if (ShellPrefix && *ShellPrefix) 
                {
                Spine.printf("%s%s",ShellPrefix,argcIn ? argvIn[0] : "");
                }
        else if (argcIn) Spine.printf("%s",argvIn[0]);
        const char *PrefixCmd = Spine.GetBuffer();
        if (!PrefixCmd)
                {
                TERROR(("%s: Unable to form command: %s",ProcName,
                                                         argcIn 
                                                         ? argvIn[0]
                                                         : "<none>"));
                return -1;
                }
        int argcShell = _Command2argv(PrefixCmd,NULL);
        int argc = argcShell + (argcIn ? argcIn-1 : 0);
        if (argc)
                {
                *argv = (char **)calloc(argc,sizeof(char *));
                if (!*argv)
                        {
                        TERROR(("%s: malloc() failed",ProcName));
                        return -1;
                        }
                int rc = _Command2argv(PrefixCmd,*argv);
                if (rc < 0)
                        {
                        TERROR(("%s: _Command2argv failed",ProcName));
                        FreeargvC(argc,argv);
                        argc = -1;
                        }
                else    {
                        for (int i=1; i < argcIn; i++)
                                {
                                (*argv)[i+argcShell-1] = strdup(argvIn[i]);
                                if (!(*argv)[i+argcShell-1])
                                        {
                                        TERROR(("%s: strdup failed",ProcName));
                                        FreeargvC(argc,argv);
                                        argc = -1;
                                        break;
                                        }
                                }
                        }
                }
        return argc;
        }

// ***************************************************************************
// **** AppendQuotedArg for CreateProcess ************************************
// ***************************************
//
// Escapes a single argument per MSVC C-runtime parsing rules and appends
// it (space-prefixed, double-quoted) to Canvas.
//
// ***************************************************************************

#ifdef _WIN32

static void AppendQuotedArg(MemoryPrintf &Canvas, const char *Arg)
        {
        const char *Src = Arg;
        char put_c[2] = {0};
        Canvas.fputs(" \"");
        size_t nSeq = 0;              // Consecutive backslashes.
        while (*Src)
                {
                char c0 = *Src;
                if (c0 == '"')
                        {
                        while (nSeq--) Canvas.fputs("\\"); // Double preceding \.
                        Canvas.fputs("\\\"");
                        nSeq = 0;
                        }
                else    {
                        put_c[0] = c0;
                        Canvas.fputs(put_c);
                        if (c0 == '\\') nSeq++;
                        else nSeq = 0;
                        }
                Src++;
                }
        while (nSeq--) Canvas.fputs("\\"); // Double trailing \ before close ".
        Canvas.fputs("\"");
        return;
        }

#endif

// ***************************************************************************
// **** popenFILESet : Pending popen()'s *************************************
// ***************************************************************************

struct popenFILESet : public Link 
        {
        FILE *ReadStream;
        FILE *ErrorStream;
        HANDLE hThread;
        HANDLE hProcess;
        char *ProcessName;
        popenFILESet() {ProcessName = NULL;}
        ~popenFILESet() {if (ProcessName) free(ProcessName);}
        }; 

DefineLinkList(_popenFILESetList,popenFILESet);

class popenFILESetList : public _popenFILESetList
        {
        protected:
                Mutex ListMutex;
                popenFILESet* _FindFILESet(FILE *ToCompare);
                friend bool pabortNT(FILE*);
                friend void pabortALL();
        public:
                ~popenFILESetList();
                void AddFILESet(popenFILESet *Set);
                HANDLE FindProcess(FILE *ToCompare);
                popenFILESet* RemoveFILESet(FILE *ToCompare);
        }; 

static popenFILESetList FILESetList;

popenFILESetList::~popenFILESetList()
        {
        static const char *ProcName = "~popenFILESetList";
        if (GetFirst())
                {
                TERROR(("%s: pcloseNT() not issued for popenNT()",ProcName));
                ChainSaw();
                }
        return;
        }

void popenFILESetList::AddFILESet(popenFILESet *Set)
        {
        ListMutex.Take();
        AddBottom(Set);
        ListMutex.Release();
        return;
        }

popenFILESet* popenFILESetList::_FindFILESet(FILE *ToCompare)
        {
        popenFILESet *Current = GetFirst();
        while (Current)
                {
                if (Current->ReadStream == ToCompare || 
                    Current->ErrorStream == ToCompare)
                        {
                        return Current;
                        } 
                Current = GetNext(Current);
                }
        return NULL;        
        }

HANDLE popenFILESetList::FindProcess(FILE *ToCompare)
        {
        HANDLE hProcess = 0;
        ListMutex.Take();
        popenFILESet *Current = _FindFILESet(ToCompare); 
        if (Current) hProcess = Current->hProcess;
        ListMutex.Release();
        return hProcess;
        }

popenFILESet* popenFILESetList::RemoveFILESet(FILE *ToCompare)
        {
        ListMutex.Take();
        popenFILESet *Current = _FindFILESet(ToCompare); 
        if (Current) Remove(Current);
        ListMutex.Release();
        return Current;
        }
                
// ***************************************************************************
// **** popenNT Implementation (Generic) *************************************
// ***************************************************************************

bool popenNToe(const char *Command, const char *Mode, FILE **out, FILE **err)
	{
        return popenNTioe(Command,Mode,NULL,out,err);
        }

bool popenNTio(const char *Command, const char *Mode, FILE **in, FILE **out)
	{
        return popenNTioe(Command,Mode,in,out,NULL);
        }

FILE* popenNT(const char *Command, const char *Mode)
	{
        static const char *ProcName = "popenNT";
        FILE *out = NULL;
        bool rc = popenNToe(Command,Mode,&out,NULL);
        if (!rc)
                {
                TERROR(("%s: popenNToe(%s) Failed",ProcName,Command));
                }
        return out;
        }

// -----------------------------------
// ---- argv convenience wrappers ----
// -----------------------------------

bool popenNToe(int argc, const char *argv[], 
               const char *Mode, 
               FILE **out, 
               FILE **err)
	{
        return popenNTioe(argc,argv,Mode,NULL,out,err);
        }

bool popenNTio(int argc, const char *argv[], 
               const char *Mode, 
               FILE **in, 
               FILE **out)
	{
        return popenNTioe(argc,argv,Mode,in,out,NULL);
        }

// ***************************************************************************
// **** popenNT Implementation (Windows) *************************************
// ***************************************************************************

#ifdef _WIN32

// ********************
// **** popenNTioe ****
// ********************

bool popenNTioe(const char *Command, 
                const char *Mode, 
                FILE **in,
                FILE **out,
                FILE **err)
	{
        static const char *ProcName = "popenNTioe";
        int hin = -1;
        int hout = -1;
        int herr = -1;
	HANDLE ReadPipe = 0;
        HANDLE ErrorPipe = 0;
        HANDLE WritePipe = 0;
        HANDLE StdoutHandle = 0;
        HANDLE StderrHandle = 0;
	HANDLE StdinHandle = 0;
	STARTUPINFO StartupInfo = {0};
	PROCESS_INFORMATION ProcessInfo = {0};
	SECURITY_ATTRIBUTES SecurityAttr = {0};
	StartupInfo.cb = sizeof(StartupInfo);
	SecurityAttr.nLength = sizeof(SecurityAttr);
	SecurityAttr.bInheritHandle = true;
        // *******************
        // **** Open Pipe ****
        // *******************
        BOOL rc = false;
        popenFILESet *FILESet = new popenFILESet;
        if (!FILESet)
                {
                TERROR(("%s: Unable to allocate FILEset",ProcName));
                return false;
                }
        FILESet->ProcessName = ExtractProgram(Command);
        if (out)
                {
	        rc = CreatePipe(&ReadPipe,&StdoutHandle,&SecurityAttr,0);
	        if (!rc)
		        {
		        TERROR(("%s: CreatePipe [ReadPipe] Failed",ProcName));
		        goto pNT_Error;
		        }
                SetHandleInformation(ReadPipe,HANDLE_FLAG_INHERIT,0);
                }
        if (err)
                {
	        rc = CreatePipe(&ErrorPipe,&StderrHandle,&SecurityAttr,0);
	        if (!rc)
		        {
		        TERROR(("%s: CreatePipe [ErrorPipe] Failed",ProcName));
		        goto pNT_Error;
		        }
                SetHandleInformation(ErrorPipe,HANDLE_FLAG_INHERIT,0);
                }
	rc = CreatePipe(&StdinHandle,&WritePipe,&SecurityAttr,0);
	if (!rc)
		{
		TERROR(("%s: CreatePipe [WritePipe] Failed",ProcName));
                goto pNT_Error;
		}
        rc = SetHandleInformation(WritePipe,HANDLE_FLAG_INHERIT,0);
	if (!rc)
		{
		TERROR(("%s: SetHandleInformation [WritePipe] Failed",ProcName));
                goto pNT_Error;
		}
        StartupInfo.hStdInput = StdinHandle;
        StartupInfo.hStdOutput = StdoutHandle;
        StartupInfo.hStdError = StderrHandle ? StderrHandle : StdoutHandle;
        // ************************
        // **** Create Process ****
        // ************************
        StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        StartupInfo.wShowWindow = SW_HIDE;
        rc = CreateProcess(NULL,
                           (LPTSTR)Command,
                           NULL,
                           NULL,
                           TRUE,
                           CREATE_NO_WINDOW,
                           NULL,
                           NULL,
                           &StartupInfo,
                           &ProcessInfo);
        if (StdinHandle)
                {
                CloseHandle(StdinHandle);
                StdinHandle = 0;
                }
        if (StderrHandle)
                {
                CloseHandle(StderrHandle);
                StderrHandle = 0;
                }
        if (StdoutHandle)
                {
                CloseHandle(StdoutHandle);
                StdoutHandle = 0;
                }
        if (!rc)
                {
                TERROR(("%s: CreateProcess Failed (%u)",ProcName,
                                                        GetLastError()));
  pNT_Error:    if (StdinHandle) CloseHandle(StdinHandle);
                if (StdoutHandle) CloseHandle(StdoutHandle);
                if (StderrHandle) CloseHandle(StderrHandle);
                if (in && *in)
                        {
                        fclose(*in);
                        *in = NULL;
                        }
                else if (hin >= 0) close(hin);
                else if (WritePipe) CloseHandle(WritePipe);
                if (out && *out) 
                        {
                        fclose(*out);
                        *out = NULL;
                        }
                else if (hout >= 0) close(hout);
                else if (ReadPipe) CloseHandle(ReadPipe);
                if (err && *err) 
                        {
                        fclose(*err);
                        *err = NULL;
                        }
                else if (herr >= 0) close(herr);
                else if (ErrorPipe) CloseHandle(ErrorPipe);
                TERROR(("%s: popenNToe(%s) Failed",
                        ProcName,
                        ShortName(FILESet->ProcessName)));
                if (FILESet) delete FILESet;
                return false;
                }
        // *************************************
        // **** Build c-Runtime FILE Stream ****
        // *************************************
        FILESet->hProcess = ProcessInfo.hProcess;
        FILESet->hThread = ProcessInfo.hThread;
        int flags = strchr(Mode,'t') ? O_TEXT : 0;
        if (in)
                {
                hin = _open_osfhandle((size_t)WritePipe,flags);
                if (hin < 0)
                        {
                        TERROR(("%s: _open_osfhandle [hin] Failed",ProcName));
                        goto pNT_Error; 
                        }
                *in = fdopen(hin,(char *)"w");
                if (!*in)
                        {
                        TERROR(("%s: fdopen [in] Failed",ProcName));
                        goto pNT_Error; 
                        }
                }
        else    {
                // ------------------------------------------------------
                // Send EOF if the caller didn't request an input stream.
                // ------------------------------------------------------
                if (WritePipe) CloseHandle(WritePipe);
                }
        if (ReadPipe)
                {
                hout = _open_osfhandle((size_t)ReadPipe,flags);
                if (hout < 0)
                        {
                        TERROR(("%s: _open_osfhandle [hout] Failed",ProcName));
                        goto pNT_Error; 
                        }
                *out = fdopen(hout,(char *)"r");
                if (!*out)
                        {
                        TERROR(("%s: fdopen [out] Failed",ProcName));
                        goto pNT_Error; 
                        }
                FILESet->ReadStream = *out;
                }
        else FILESet->ReadStream = NULL;
        if (ErrorPipe)
                {
                herr = _open_osfhandle((size_t)ErrorPipe,flags);
                if (herr < 0)
                        {
                        TERROR(("%s: _open_osfhandle [herr] Failed",ProcName));
                        goto pNT_Error; 
                        }
                *err = fdopen(herr,(char *)"r");
                if (!*err)
                        {
                        TERROR(("%s: fdopen [err] Failed",ProcName));
                        goto pNT_Error; 
                        }
                FILESet->ErrorStream = *err;
                }
        else FILESet->ErrorStream = NULL;
        FILESetList.AddFILESet(FILESet);
        return true;
	}

// **************************************************
// **** popenNTioe (argv[]) *************************
// *************************
//
// Flattens argv using MSVC C-runtime quoting rules
// then delegates to the string-based popenNTioe().
//
// **************************************************

bool popenNTioe(int argc, const char *argv[],
                const char *Mode,
                FILE **in,
                FILE **out,
                FILE **err)
        {
        static const char *ProcName = "popenNTioe(argv)";
        if (argc < 1 || !argv || !argv[0])
                {
                TERROR(("%s: No command specified",ProcName));
                return false;
                }
        MemoryPrintf Canvas;
        Canvas.fputs("\"");     // argv[0] is the program -- quote it.
        Canvas.fputs(argv[0]);
        Canvas.fputs("\"");
        for (int i = 1; i < argc; i++)
                {
                if (!argv[i]) break;
                AppendQuotedArg(Canvas, argv[i]);
                }
        const char *Command = Canvas.GetBuffer();
        if (!Command)
                {
                TERROR(("%s: Unable to form command line",ProcName));
                return false;
                }
        return popenNTioe(Command, Mode, in, out, err);
        }

// ******************
// **** pcloseNT ****
// ******************

int pcloseNT(FILE *stream)
	{
        static const char *ProcName = "pcloseNT";
        DWORD ExitCode;
        popenFILESet *FILESet = FILESetList.RemoveFILESet(stream);
        if (!FILESet)
                {
                TERROR(("%s: GetFILESet Failed",ProcName));
                return 0;
                }
	// ********************************
        // **** Wait for Child Process ****
        // ********************************
        WaitForSingleObject(FILESet->hProcess,INFINITE);
        GetExitCodeProcess(FILESet->hProcess,&ExitCode);
        // *****************************
        // **** Close Child Handles ****
        // *****************************
        if (FILESet->ReadStream) fclose(FILESet->ReadStream);
        if (FILESet->ErrorStream) fclose(FILESet->ErrorStream);
        if (FILESet->hThread) CloseHandle(FILESet->hThread);
        if (FILESet->hProcess) CloseHandle(FILESet->hProcess);
        delete FILESet;
	return ExitCode;
	}

// ===========================================================================
// ==== pabort Extension =====================================================
// ===========================================================================

// ******************
// **** pabortNT ****
// ******************

bool pabortNT(FILE *stream)
        {
        static const char *ProcName = "pabortNT";
        bool Status = true;
        FILESetList.ListMutex.Take();
        popenFILESet *Found = FILESetList._FindFILESet(stream);
        HANDLE hProcess = Found ? Found->hProcess : 0;
        if (hProcess)
                {
                TERROR(("%s: Aborting Process: %s (0x%X)",
                        ProcName,
                        ShortName(Found->ProcessName),
                        hProcess));
                Status = TerminateProcess(hProcess,666) == TRUE;
                if (Status) WaitForSingleObject(hProcess,1000);
                }
        FILESetList.ListMutex.Release();
        return Status;
        }

// *******************
// **** pabortALL ****
// *******************

void pabortALL()
        {
        static const char *ProcName = "pabortALL";
        bool Status = true;
        FILESetList.ListMutex.Take();
        popenFILESet *Current = FILESetList.GetFirst();
        // ----------------------------
        // --- Terminate Processes ----
        // ----------------------------
        while (Current)
                {
                HANDLE hProcess = Current->hProcess;
                if (hProcess)
                        {
                        TERROR(("%s: Aborting Process: %s (0x%X)",
                                ProcName,
                                ShortName(Current->ProcessName),
                                hProcess));
                        Status = TerminateProcess(hProcess,666) == TRUE;
                        }
                Current = FILESetList.GetNext(Current);
                }
        FILESetList.ListMutex.Release();
        // --------------------
        // --- Slight Wait ----
        // --------------------
        FILESetList.ListMutex.Take();
        Current = FILESetList.GetFirst();
        while (Current)
                {
                HANDLE hProcess = Current->hProcess;
                if (hProcess) WaitForSingleObject(hProcess,1000);
                Current = FILESetList.GetNext(Current);
                }
        FILESetList.ListMutex.Release();
        return;
        }

// ***************************************************************************
// **** popenNT Implementation (Unix) ****************************************
// ***************************************************************************

#else /*!_WIN32 - UNIX */

// *****************************
// **** pipe2 shim for AIX ****
// *****************************

#ifdef _AIX

#ifndef O_CLOEXEC
        #define O_CLOEXEC 02000000 
#endif

static Mutex pipe2Mutex;

static inline int pipe2(int pipefd[2], int flags)
        {
        pipe2Mutex.Take();
        int rc = pipe(pipefd);
        if (rc != 0)
                {
                pipe2Mutex.Release();
                return rc;
                }
        if (flags & O_CLOEXEC)
                {
                int f0 = fcntl(pipefd[0], F_GETFD);
                fcntl(pipefd[0], F_SETFD, f0 | FD_CLOEXEC);
                int f1 = fcntl(pipefd[1], F_GETFD);
                fcntl(pipefd[1], F_SETFD, f1 | FD_CLOEXEC);
                }
        pipe2Mutex.Release();
        return 0;
        }

#endif

// *****************************
// **** popenNTioe (argv[]) ****
// *****************************

bool popenNTioe(int argc, const char *argv[],
                const char *Mode,
                FILE **in,
                FILE **out,
                FILE **err)
	{
        static const char *ProcName = "popenNTioe";
	HANDLE ReadPipe = -1;
        HANDLE ErrorPipe = -1;
        HANDLE WritePipe = -1;
        HANDLE StdoutHandle = -1;
        HANDLE StderrHandle = -1;
	HANDLE StdinHandle = -1;
        int pid = -1;  // Declare at function scope for goto pNT_Error
        if (argc < 1 || !argv || !argv[0])
                {
                TERROR(("%s: No command specified",ProcName));
                return false;
                }
        // *******************
        // **** Open Pipe ****
        // *******************
        int rc = -1;
        popenFILESet *FILESet = new popenFILESet;
        if (!FILESet)
                {
                TERROR(("%s: Unable to allocate FILEset",ProcName));
                return false;
                }
        FILESet->ProcessName = strdup(argv[0]);
        if (out)
                {
                int pipefd[2];
                rc = pipe2(pipefd, O_CLOEXEC);
	        if (rc != 0)
		        {
		        TERROR(("%s: pipe2 [ReadPipe] Failed (errno=%d)",ProcName,errno));
		        goto pNT_Error;
		        }
                ReadPipe = pipefd[0];
                StdoutHandle = pipefd[1];
                }
        if (err)
                {
                int pipefd[2];
	        rc = pipe2(pipefd, O_CLOEXEC);
	        if (rc != 0)
		        {
		        TERROR(("%s: pipe2 [ErrorPipe] Failed (errno=%d)",ProcName,errno));
		        goto pNT_Error;
		        }
                ErrorPipe = pipefd[0];
                StderrHandle = pipefd[1];
                }
        int pipefd[2];
	rc = pipe2(pipefd, O_CLOEXEC);
	if (rc != 0)
		{
		TERROR(("%s: pipe2 [WritePipe] Failed (errno=%d)",ProcName,errno));
                goto pNT_Error;
		}
        StdinHandle = pipefd[0];
        WritePipe = pipefd[1];
        // ************************
        // **** Create Process ****
        // ************************
  #ifdef _AIX
        pipe2Mutex.Take();
  #endif
        pid = fork();
  #ifdef _AIX
        pipe2Mutex.Release();
  #endif
        if (pid == 0)                           // In Child:
                {
                if (StdinHandle >= 0)
                        {
                        dup2(StdinHandle,STDIN_FILENO);
                        close(StdinHandle);
                        StdinHandle = -1;
                        }
                if (StderrHandle >= 0)
                        {
                        dup2(StderrHandle,STDERR_FILENO);
                        close(StderrHandle);
                        StderrHandle = -1;
                        }
                else if (StdoutHandle >= 0)
                        {
                        dup2(StdoutHandle,STDERR_FILENO);
                        }
                if (StdoutHandle >= 0)
                        {
                        dup2(StdoutHandle,STDOUT_FILENO);
                        close(StdoutHandle);
                        StdoutHandle = -1;
                        }
                if (ReadPipe >= 0) close(ReadPipe);
                if (ErrorPipe >= 0) close(ErrorPipe);
                if (WritePipe >= 0) close(WritePipe);
                // Build NULL-terminated argv copy for execvp.
                char **argvExec = (char **)alloca((argc+1) * sizeof(char *));
                memcpy(argvExec, argv, argc * sizeof(char *));
                argvExec[argc] = NULL;
                rc = execvp(argvExec[0],argvExec);
                TERROR(("%s: Unable to exec(%s) (errno=%d)",ProcName,
                                                            argv[0],
                                                            errno));
                _exit(-1);                      // _exit in child.
                }
        else if (pid > 0)                       // In Parent:
                {
                if (StdinHandle >= 0)
                        {
                        close(StdinHandle);
                        StdinHandle = -1;
                        }
                if (StderrHandle >= 0)
                        {
                        close(StderrHandle);
                        StderrHandle = -1;
                        }
                if (StdoutHandle >= 0)
                        {
                        close(StdoutHandle);
                        StdoutHandle = -1;
                        }
                rc = 0;  // Success
                }
        if (pid < 0)
                {
                TERROR(("%s: fork() Failed (errno=%d)",ProcName,errno));
  pNT_Error:    if (StdinHandle >= 0) close(StdinHandle);
                if (StdoutHandle >= 0) close(StdoutHandle);
                if (StderrHandle >= 0) close(StderrHandle);
                if (in && *in)
                        {
                        fclose(*in);
                        *in = NULL;
                        }
                else if (WritePipe >= 0) close(WritePipe);
                if (out && *out) 
                        {
                        fclose(*out);
                        *out = NULL;
                        }
                else if (ReadPipe >= 0) close(ReadPipe);
                if (err && *err) 
                        {
                        fclose(*err);
                        *err = NULL;
                        }
                else if (ErrorPipe >= 0) close(ErrorPipe);
                TERROR(("%s: popenNTioe(%s) Failed",
                        ProcName,
                        ShortName(FILESet->ProcessName)));
                if (FILESet) delete FILESet;
                return false;
                }
        // *************************************
        // **** Build c-Runtime FILE Stream ****
        // *************************************
        FILESet->hProcess = pid;
        FILESet->hThread = 0;
        if (in)
                {
                *in = fdopen(WritePipe,(char *)"w");
                if (!*in)
                        {
                        TERROR(("%s: fdopen [in] Failed",ProcName));
                        goto pNT_Error; 
                        }
                }
        else    {
                // ------------------------------------------------------
                // Send EOF if the caller didn't request an input stream.
                // ------------------------------------------------------
                if (WritePipe >= 0) close(WritePipe);
                }
        if (ReadPipe >= 0)
                {
                *out = fdopen(ReadPipe,(char *)"r");
                if (!*out)
                        {
                        TERROR(("%s: fdopen [out] Failed",ProcName));
                        goto pNT_Error; 
                        }
                FILESet->ReadStream = *out;
                }
        else FILESet->ReadStream = NULL;
        if (ErrorPipe >= 0)
                {
                *err = fdopen(ErrorPipe,(char *)"r");
                if (!*err)
                        {
                        TERROR(("%s: fdopen [err] Failed",ProcName));
                        goto pNT_Error; 
                        }
                FILESet->ErrorStream = *err;
                }
        else FILESet->ErrorStream = NULL;
        FILESetList.AddFILESet(FILESet);
        return true;
	}

// **********************************
// **** popenNTioe (Command str) ****
// **********************************

bool popenNTioe(const char *Command,
                const char *Mode,
                FILE **in,
                FILE **out,
                FILE **err)
	{
        static const char *ProcName = "popenNTioe";
        char **argvParsed = NULL;
        int argc = Command2argv(Command, &argvParsed);
        if (argc < 1 || !argvParsed)
                {
                TERROR(("%s: Command2argv Failed: %s",ProcName,Command));
                return false;
                }
        bool rc = popenNTioe(argc, (const char **)argvParsed, Mode, in, out, err);
        FreeargvC(argc, &argvParsed);
        return rc;
	}

// ******************
// **** pcloseNT ****
// ******************

int pcloseNT(FILE *stream)
	{
        static const char *ProcName = "pcloseNT";
        int ExitCode;
        popenFILESet *FILESet = FILESetList.RemoveFILESet(stream);
        if (!FILESet)
                {
                TERROR(("%s: GetFILESet Failed",ProcName));
                return 0;
                }
	// ********************************
        // **** Wait for Child Process ****
        // ********************************
        int Status = -1;
        int rc = waitpid(FILESet->hProcess,&Status,0);
        if (rc == -1)
                {
                TERROR(("%s: waitpid(%d) failed (errno=%d) for: %s",
                        ProcName,
                        FILESet->hProcess,
                        errno,
                        ShortName(FILESet->ProcessName)));
                ExitCode = -1;
                }
        else if (!WIFEXITED(Status))
                {
                TERROR(("%s: %s Ended Abnormally",
                        ProcName,
                        ShortName(FILESet->ProcessName)));
                if (WIFSIGNALED(Status))
                        {
                        int Signal = WTERMSIG(Status);
                        TERROR(("%s: %s Ended by Signal: %d",ProcName,
                                ShortName(FILESet->ProcessName),
                                Signal));
                        }
                ExitCode = -1;
                }
        else    {
                ExitCode = WEXITSTATUS(Status);
                }
        // *****************************
        // **** Close Child Handles ****
        // *****************************
        if (FILESet->ReadStream) fclose(FILESet->ReadStream);
        if (FILESet->ErrorStream) fclose(FILESet->ErrorStream);
        delete FILESet;
	return ExitCode;
	}

// ===========================================================================
// ==== pabort Extension =====================================================
// ===========================================================================

// ******************
// **** pabortNT ****
// ******************

bool pabortNT(FILE *stream)
        {
        static const char *ProcName = "pabortNT";
        bool Status = true;
        FILESetList.ListMutex.Take();
        popenFILESet *Found = FILESetList._FindFILESet(stream);
        HANDLE hProcess = Found ? Found->hProcess : 0;
        if (hProcess > 0)
                {
                TERROR(("%s: Aborting Process: %s (pid=%d)",
                        ProcName,
                        ShortName(Found->ProcessName),
                        hProcess));
                int rc = kill(hProcess,SIGTERM);
                if (rc != 0)
                        {
                        TERROR(("%s: kill(%d,SIGTERM) failed (errno=%d)",
                                ProcName,hProcess,errno));
                        Status = false;
                        }
                }
        FILESetList.ListMutex.Release();
        return Status;
        }

// *******************
// **** pabortALL ****
// *******************

void pabortALL()
        {
        static const char *ProcName = "pabortALL";
        FILESetList.ListMutex.Take();
        popenFILESet *Current = FILESetList.GetFirst();
        // ----------------------------
        // --- Terminate Processes ----
        // ----------------------------
        while (Current)
                {
                HANDLE hProcess = Current->hProcess;
                if (hProcess > 0)
                        {
                        TERROR(("%s: Aborting Process: %s (pid=%d)",
                                ProcName,
                                ShortName(Current->ProcessName),
                                hProcess));
                        int rc = kill(hProcess,SIGTERM);
                        if (rc != 0)
                                {
                                TERROR(("%s: kill(%d,SIGTERM) failed (errno=%d)",
                                        ProcName,hProcess,errno));
                                }
                        }
                Current = FILESetList.GetNext(Current);
                }
        FILESetList.ListMutex.Release();
        // --------------------
        // --- Slight Wait ----
        // --------------------
        bool PendingWait = false;
        FILESetList.ListMutex.Take();
        Current = FILESetList.GetFirst();
        while (Current)
                {
                HANDLE hProcess = Current->hProcess;
                if (hProcess > 0)
                        {
                        int Status = -1;
                        int rc = waitpid(hProcess,&Status,WNOHANG);
                        if (rc == 0) PendingWait = true;
                        }
                Current = FILESetList.GetNext(Current);
                }
        if (PendingWait) sleep(1);
        FILESetList.ListMutex.Release();
        return;
        }

#endif /* !_WIN32 */

// ***************************************************************************
// **************************** End of File **********************************
// ***************************************************************************
