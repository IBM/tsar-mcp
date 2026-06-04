///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: HexDump.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.                                                
//                    
// Author(s) : Eric Kass 
//
///////////////////////////////////////////////////////////////////////////////
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include <ASThread.h>
#include <ebcdic.h>
#include <LevelTrace.h>

#include <HexDump.h>

#define LINEBREAK 4
#define LINEWIDTH 16
#define LINETAB 4

#ifdef __OS400_TGTVRM__ /* iSeries */
        #define ascii2host(c) ((unsigned char)a2e(c))
        #define ebcdic2host(c) (c)
        #define TLINETAB 4
#else
        #define ascii2host(c) (c)
        #define ebcdic2host(c) ((unsigned char)e2a(c))
        #define TLINETAB 7
#endif 

static HD_Mode_t HD_Mode = HD_AUTO;

void SetHexDumpMode(HD_Mode_t Mode)
        {
        HD_Mode = Mode;
        return;
        }

void HexDump(HexDumpFunction_t PrintFn, 
             void *UserPtr,
             unsigned LineTab,
             const unsigned char *Buffer, size_t Length)
        {
        HD_Mode_t Mode = HD_Mode;
        bool ModePrinted = false;
        // **********************************
        // **** EBCDIC - ASCII Detection ****
        // **********************************
        if (Mode == HD_AUTO)
                {
                unsigned nASCIIChar = 0;
                unsigned nEBCDICChar = 0;
                for (size_t i=0; i < Length; i++)
                        {
                        unsigned char Byte = Buffer[i];
                        if (Byte == 0x40) nEBCDICChar++;
                        else if (Byte == 0x20) nASCIIChar++;
                        else    {
                                unsigned char ByteASCII = ascii2host(Byte);
                                unsigned char ByteEBCDIC = ebcdic2host(Byte);
                                if (isalnum(ByteASCII)) nASCIIChar++;
                                if (isalnum(ByteEBCDIC)) nEBCDICChar++;
                                }
                        }
                Mode = (nEBCDICChar > nASCIIChar) ? HD_EBCDIC : HD_ASCII;
                }
        // **********************************
        for (size_t Addr=0; Addr < Length; Addr += LINEWIDTH)
                {
                if (LineTab) PrintFn(UserPtr,"%*c",LineTab,' ');
                PrintFn(UserPtr,"%4.4X    ",Addr);
                size_t i, Count = Length - Addr;
                // *******************
                // **** Print Hex ****
                // *******************
                for (i=0; i < LINEWIDTH; i++)
                        {
                        if (Count) 
                                {
                                PrintFn(UserPtr,"%2.2X",Buffer[Addr+i]);
                                Count--;
                                }
                        else PrintFn(UserPtr,"  ");
                        if ((i+1) % LINEBREAK == 0) PrintFn(UserPtr," ");
                        }
                // ********************
                // **** Print Text ****
                // ********************
                if (!ModePrinted)
                        {
                        PrintFn(UserPtr,Mode == HD_ASCII ? 
                                        " ASCII " : 
                                        "EBCDIC ");
                        ModePrinted = true;
                        }
                else PrintFn(UserPtr,"       ");
                Count = Length - Addr;
                for (i=0; Count && i < LINEWIDTH; i++, Count--)
                        {
                        unsigned char Byte = Buffer[Addr+i];
                        if (Mode == HD_EBCDIC) Byte = ebcdic2host(Byte);
                        else Byte = ascii2host(Byte);
                        if (!isgraph(Byte) && !ispunct(Byte) && Byte != ' ') 
                                {
                                Byte = '.';     // Non-printable character.
                                }
                        PrintFn(UserPtr,"%c",Byte);
                        }
                PrintFn(UserPtr,"\n");
                }
        return;
        }

// **************************
// **** Hex Dump to File ****
// **************************

static void HexDumpFilePrint(void *UserPtr, const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        vfprintf((FILE *)UserPtr,Format,Args);
        va_end(Args);
        return;
        }

void HexDump(FILE *out, const unsigned char *Buffer, size_t Length)
        {
        HexDump(HexDumpFilePrint,out,LINETAB,Buffer,Length);
        return;
        }

void HexDump(FILE *out, const char *Buffer, size_t Length)
        {
        HexDump(HexDumpFilePrint,
                out,
                LINETAB,
                (const unsigned char*)Buffer,Length);
        return;
        }

// *********************************
// **** Hex Dump via LevelTrace ****
// *********************************

void _LTWriteLogTAKE();
void _LTWriteLogRELEASE();
void _LTWriteLogPrintNOLOCK(const char *Format, ...);

static void HexDumpTraceFn(void *, const char *Format, ...)
        {
        va_list Args;
        char StringBuffer[128];
        static char LineBuffer[128];
        char *StringPos = StringBuffer;
        static char *LinePos = LineBuffer;
        va_start(Args,Format);
        vsprintf(StringBuffer,Format,Args);
        va_end(Args);
        while (*StringPos)
                {
                if (*StringPos != '\n') *LinePos++ = *StringPos;
                if (*StringPos == '\n' ||
                    LinePos - LineBuffer == sizeof(LineBuffer) - 1)
                        {
                        *LinePos = '\0';
                        _LTWriteLogPrintNOLOCK("%s",LineBuffer);
                        LinePos = LineBuffer;
                        }
                StringPos++;
                }
        return;
        }

void THexDump(const unsigned char *Buffer, size_t Length)
        {
        _LTWriteLogTAKE();
        HexDump(HexDumpTraceFn,NULL,TLINETAB,Buffer,Length);
        _LTWriteLogRELEASE();
        return;
        }

void THexDump(const char *Buffer, size_t Length)
        {
        _LTWriteLogTAKE();
        HexDump(HexDumpTraceFn,
                NULL,
                TLINETAB,
                (const unsigned char*)Buffer,Length);
        _LTWriteLogRELEASE();
        return;
        }

// ***************************************************************************
// **************************** End of File **********************************
// ***************************************************************************
