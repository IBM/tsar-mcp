///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: LevelTrace.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.                                                
//                    
// Author(s) : Eric Kass 
//
///////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
        #include <windows.h>
#else
        #include <string.h>
        #include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef __OS400_TGTVRM__ /* iSeries */
        #include <qusec.h>
        #include <qmhsndpm.h>
        #include <stdarg.h>
		#if __OS400_TGTVRM__ < 530
		extern "C" vsnprintf(char *, size_t, const  char *, va_list);
		#endif
#endif

#include <ASThread.h>

#include <LevelTrace.h>

#define TRACELEVEL_PRINT TRACELEVEL_NONE

#ifndef MAXTRACELEVEL_DEFAULT
	#ifdef _DEBUG
		#define MAXTRACELEVEL_DEFAULT TRACELEVEL_DEBUG
	#else
		#define MAXTRACELEVEL_DEFAULT TRACELEVEL_INFO
	#endif
#endif

// ****************************************************************************
// **** Trace *****************************************************************
// ****************************************************************************

// **************************
// **** Built in Logging ****
// **************************

static unsigned MaxTraceLevel = MAXTRACELEVEL_DEFAULT;

#ifdef __OS400_TGTVRM__ /* iSeries */

static void WriteJoblog(const char *MSGData)
        {
        char MSGKey[4];
        char ErrorMemory[120];
        Qus_EC_t *ErrorCode = (Qus_EC_t *)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);
        QMHSNDPM("CPDA0FF",                  // Message ID
                 "QCPFMSG   *LIBL     ",     // Message File
                 (char *)MSGData,            // Replacement Text
                 strlen(MSGData),
                 "*INFO     ",               // Message Type
                 "*PGMBDY   ",               // Call Stack Entry
                 0,                          // Call Stack Counter
                 MSGKey,                     // Message Key
                 ErrorCode);
        return;
        }

static void WriteLog(unsigned Level, const char *Format, va_list Args)
        {
        if (Level > MaxTraceLevel) return;
        char Buffer[32+255+1];                  // Header + Message + NULL.
        if (Level == TRACELEVEL_ERROR) strcpy(Buffer,"ERROR: ");
        else if (Level == TRACELEVEL_INFO) strcpy(Buffer,"INFO:  ");
        else if (Level == TRACELEVEL_DEBUG) strcpy(Buffer,"DEBUG: ");
        else Buffer[0] = '\0';
        vsnprintf(Buffer+strlen(Buffer),255,Format,Args);
        Buffer[sizeof(Buffer)-1] = '\0';
        WriteJoblog(Buffer);
        return;
        }

#elif _WIN32

static unsigned GetConsoleColor()
        {
        CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;
        HANDLE Console = GetStdHandle(STD_ERROR_HANDLE);
        GetConsoleScreenBufferInfo(Console,&ConsoleInfo);
        return ConsoleInfo.wAttributes;
        }

static void SetConsoleColor(unsigned Color)
        {
        HANDLE Console = GetStdHandle(STD_ERROR_HANDLE);
        SetConsoleTextAttribute(Console,(WORD)Color);
        return;
        }

static void WriteLog(unsigned Level, const char *Format, va_list Args)
        {
        if (Level > MaxTraceLevel) return;
        unsigned OriginalColor = GetConsoleColor();
        if (Level == TRACELEVEL_ERROR) 
                {
                fprintf(stderr,"ERROR: ");
                SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
                }
        else if (Level == TRACELEVEL_INFO) 
                {
                fprintf(stderr,"INFO:  ");
                SetConsoleColor(FOREGROUND_BLUE | 
                                FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                }
        else if (Level == TRACELEVEL_DEBUG) 
                {
                fprintf(stderr,"DEBUG: ");
                SetConsoleColor(FOREGROUND_RED |
                                FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                }
        else if (Level == TRACELEVEL_PRINT) 
                {
                SetConsoleColor(FOREGROUND_RED | 
                                FOREGROUND_GREEN | 
                                FOREGROUND_BLUE);
                }
        vfprintf(stderr,Format,Args);
        fprintf(stderr,"\n");
        SetConsoleColor(OriginalColor);
        return;
        }

#else /* Unix */

#if 'A' == 65
        #define ESCAPE "\033"                   // ASCII
#else
        #define ESCAPE "\047"                   // EBCDIC
#endif

#define ANSI_ESCAPE_RED ESCAPE "[1;31m"
#define ANSI_ESCAPE_YELLOW ESCAPE "[1;33m"
#define ANSI_ESCAPE_CYAN ESCAPE "[1;36m"
#define ANSI_ESCAPE_WHITE ESCAPE "[1;37m"
#define ANSI_ESCAPE_RESET ESCAPE "[0m"

static void WriteLog(unsigned Level, const char *Format, va_list Args)
	{
	static enum {TT_None=0, TT_Mono, TT_Color} TermType = TT_None;
	if (Level > MaxTraceLevel) return;
	if (TermType == TT_None)
		{
                const char *TERM = getenv("TERM");
                if (!TERM || strcmp(TERM,"dumb") == 0) TermType = TT_Mono;
                else if (getenv("TT_Color")) TermType = TT_Color;
                else TermType = isatty(fileno(stderr)) ? TT_Color : TT_Mono;
		}
	if (Level == TRACELEVEL_ERROR)
		{
                fprintf(stderr,"ERROR: ");
		if (TermType == TT_Color) fprintf(stderr,ANSI_ESCAPE_RED);
                }
        else if (Level == TRACELEVEL_INFO)
                {
                fprintf(stderr,"INFO:  ");
		if (TermType == TT_Color) fprintf(stderr,ANSI_ESCAPE_CYAN);
                }
        else if (Level == TRACELEVEL_DEBUG)
                {
                fprintf(stderr,"DEBUG: ");
		if (TermType == TT_Color) fprintf(stderr,ANSI_ESCAPE_YELLOW);
                }
        else if (Level == TRACELEVEL_PRINT) 
                {
		if (TermType == TT_Color) fprintf(stderr,ANSI_ESCAPE_WHITE);
                }
        vfprintf(stderr,Format,Args);
        if (TermType == TT_Color) fprintf(stderr,ANSI_ESCAPE_RESET);
        fprintf(stderr,"\n");
        return;
        }

#endif /* Unix */

int Default_TraceLevelFunction(void *, 
                               unsigned Level,
                               const char *Format,
                               va_list Args)
        {
        WriteLog(Level,Format,Args);
        return 1;
        }

// **************************************
// **** Set Trace Function and Level ****
// **************************************

static Mutex WriteLogGuard;

static void *WriteLogFn_UserPtr = NULL;

static LevelTraceFunction_t WriteLogFn = Default_TraceLevelFunction;

unsigned GetMaxTraceLevel()
        {
        return MaxTraceLevel;
        }

void SetMaxTraceLevel(unsigned Level)
        {
        MaxTraceLevel = Level;  // Affects Default_TraceLevelFunction only.
        return;
        }

bool SetLevelTraceFunction(LevelTraceFunction_t Function, void *UserPtr)
        {
        WriteLogGuard.Take();
        WriteLogFn = Function;
        WriteLogFn_UserPtr = UserPtr;
        WriteLogGuard.Release();
        return true;
        }

// ***************************
// **** Logging Functions ****
// ***************************

void LTWriteLogPrint(const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        WriteLogGuard.Take();
        if (WriteLogFn) 
                {
                WriteLogFn(WriteLogFn_UserPtr,TRACELEVEL_PRINT,Format,Args);
                }
        WriteLogGuard.Release();
        va_end(Args);
        return;
        }

void LTWriteLogError(const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        WriteLogGuard.Take();
        if (WriteLogFn) 
                {
                WriteLogFn(WriteLogFn_UserPtr,TRACELEVEL_ERROR,Format,Args);
                }
        WriteLogGuard.Release();
        va_end(Args);
        return;
        }

void LTWriteLogInfo(const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        WriteLogGuard.Take();
        if (WriteLogFn) 
                {
                WriteLogFn(WriteLogFn_UserPtr,TRACELEVEL_INFO,Format,Args);
                }
        WriteLogGuard.Release();
        va_end(Args);
        return;
        }

void LTWriteLogDebug(const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        WriteLogGuard.Take();
        if (WriteLogFn) 
                {
                WriteLogFn(WriteLogFn_UserPtr,TRACELEVEL_DEBUG,Format,Args);
                }
        WriteLogGuard.Release();
        va_end(Args);
        return;
        }

// ***************************************************
// **** Block Logging Functions (See HexDump.cpp) ****
// ***************************************************

void _LTWriteLogTAKE()
        {
        WriteLogGuard.Take();
        }

void _LTWriteLogRELEASE()
        {
        WriteLogGuard.Release();
        }

void _LTWriteLogPrintNOLOCK(const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        if (WriteLogFn) 
                {
                WriteLogFn(WriteLogFn_UserPtr,TRACELEVEL_PRINT,Format,Args);
                }
        va_end(Args);
        return;
        }

// ****************************************************************************
// ***************************** End of File **********************************
// ****************************************************************************
