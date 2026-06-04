///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: PRCEDPrint.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
/* PRCED Print Utilities */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <PRCEDPrint.h>

#include <ebcdic.h>
#include <HexDump.h>
#include <LevelTrace.h>
                     
#define PrintLOB_LINELEN 50
#define PrintPACKED_LENGTH 127                  // Sign + Digits + Point.

#define MAX_PRINT_LENGTH 256                    // Show this number of bytes.
#define MAX_PRINT_WIDTH 65                      // Print characters per line.
#define MAX_PRINT_BREAK 15                      // Search for word break.
#define MAX_NUMERIC_LENGTH 127                  // Sign + Digits + Point.

/* ************************************************************************ */
/* **** Utilities ********************************************************* */
/* ************************************************************************ */

static size_t strlenCHAR(const char *Source, size_t MaxLength, int CCSID)
        {
        size_t Len;
        char Space = CCSID == 819 || CCSID == 1208 ? (char)0x20 : (char)0x40;
        for (Len=0; Len < MaxLength && Source[Len]; Len++);
        while (Len && Source[Len-1] == Space) Len--;
        return Len;
        }

static size_t strlenCHARU(const u2char_t *Source, size_t MaxLength)
        {
        size_t Len;
        for (Len=0; Len < MaxLength && Source[Len]; Len++);
        while (Len && Source[Len-1] == (u2char_t)0x20) Len--;
        return Len;
        }

/* ************************************************************************ */
/* **** Printing Routines ************************************************* */
/* ************************************************************************ */

static void PPrint(const char *Format, ...)
        {
        va_list Args;
        va_start(Args,Format);
        vprintf(Format,Args);
        printf("\n");
        va_end(Args);
        return;
        }

#define PPRINT(x) PPrint x

static void PHexDump(const unsigned char *Buffer, size_t Length)
        {
        HexDump(stdout,Buffer,Length);
        return;
        }

static void PHexDump(const char *Buffer, size_t Length)
        {
        HexDump(stdout,(const unsigned char*)Buffer,Length);
        return;
        }

/* ************************************************************************ */
/* **** Print Data ******************************************************** */
/* ************************************************************************ */

static bool PrintPacked(const char *Name, 
                        unsigned nDigits,
                        unsigned Scale,
                        unsigned char *Data)
        {
        char NData[PrintPACKED_LENGTH+1];
        if (nDigits == 0) 
                {
                PPRINT(("%s: <none>",Name));
                return true;
                }
        if (nDigits % 2 == 0) nDigits++;
        if (nDigits > PrintPACKED_LENGTH - 2)           // 2: Sign + Point.
                {
                TERROR(("%s: <Too Long>",Name));
                return false;
                }
        char *NPos = NData;
        size_t Width = (nDigits + 2) / 2;
        *NPos++ = (Data[Width-1] & 0xF) == 0x0D ? '-' : '+';
        for (unsigned i=0; i < nDigits; i++) 
                {
                if (nDigits - i == Scale) *NPos++ = '.'; 
                *NPos++ = (Data[i/2] >> 4) + '0';
                if (++i == nDigits) break;
                if (nDigits - i == Scale) *NPos++ = '.';
                *NPos++ = (Data[i/2] & 0xF) + '0';
                }
        *NPos = '\0';
        PPRINT(("%s: %s",Name,NData));
        return true;
        }

void PrintTextCCSID(const char *Heading, 
                    const void *SQLText, 
                    size_t nChars, 
                    int CCSID)
        {
        size_t nRemain = 0;
        unsigned Margin = 0;
        char CData[MAX_PRINT_WIDTH+1];
        unsigned char *SPos = (unsigned char *)SQLText;
        if (!nChars) PPRINT(("%s: ''",Heading));
        else while (nChars)
                {
                char *DPos = CData + nRemain;
                size_t Width = nChars;
                if (Width > MAX_PRINT_WIDTH) Width = MAX_PRINT_WIDTH;
                for (unsigned i=nRemain; i < Width; i++) 
                        {
  #ifdef __OS400_TGTVRM__
                        if (CCSID == 819)
                                {
                                char ASCII = *(char *)SPos;
                                *DPos++ = a2e(ASCII);
                                SPos++;
                                }
                        else if (CCSID == 1208)
                                {
                                char ASCII;
                                ASCII = *(char *)SPos;
                                *DPos++ = (ASCII > 127) ? '.' : a2e(ASCII);
                                SPos++;
                                }
                        else if (CCSID == 13488)
                                {
                                u2char_t UCS2 = *(u2char_t *)SPos;
                                char ASCII = (char)UCS2;
                                *DPos++ = (UCS2 > 127) ? '.' : a2e(ASCII);
                                SPos += 2;
                                }
                        else *DPos++ = *(char *)SPos++;
  #else /* ASCII OS */
                        if (CCSID == 819)
                                {
                                *DPos++ = *(char *)SPos++;
                                }
                        else if (CCSID == 1208)
                                {
                                char ASCII;
                                ASCII = (*SPos > 127) ? '.' : *(char *)SPos;
                                *DPos++ = ASCII;
                                SPos++;
                                }
                        else if (CCSID == 13488)
                                {
                                u2char_t UCS2 = *(u2char_t *)SPos;
                                char ASCII = (UCS2 > 127) ? '.' : (char)UCS2;
                                *DPos++ = ASCII;
                                SPos += 2;
                                }
                        else    {
                                char EBCDIC = *(char *)SPos;
                                *DPos++ = e2a(EBCDIC);
                                SPos++;
                                }                                
  #endif
                        }
                nRemain = 0;
                if (Width != nChars)                    // Full line length...
                        {
                        for (size_t i=0; i < MAX_PRINT_BREAK; i++)
                                {
                                if (*(DPos - i - 1) == ' ') 
                                        {
                                        Width -= i;
                                        nRemain = i;
                                        break;
                                        }
                                }
                        }
                if (Heading)
                        {
                        PPRINT(("%s: '%.*s'",Heading,Width,CData));
                        Margin = strlen(Heading);
                        Heading = NULL;
                        }
                else    {
                        PPRINT(("%*c '%.*s'",Margin+1,' ',Width,CData));
                        }
                memmove(CData,DPos-nRemain,nRemain);
                nChars -= Width;
                }
        return;
        }

void PrintVariable(DAVariables &Variables, int Row, int Col)
        {
        size_t Digits = Variables.GetVariableDigits(Col);
        size_t Scale = Variables.GetVariableScale(Col);
        size_t Length = Variables.GetVariableLength(Row,Col);
        const char* Data = Variables.GetVariableData(Row,Col);
        int Type = Variables.GetVariableType(Col);
        const char* Name = Variables.GetVariableName(Col);
        unsigned CCSID = Variables.GetCCSID(Col);
        if ((Type & 0x01) && Variables.GetIndicator(Row,Col) == -1) 
                {
                PPRINT(("%s: (null)",Name));
                }
        else switch (Type)
                {
                case DA_BIG_TYPE:
  #ifdef _WIN32
                        PPRINT(("%s: %I64d",Name,*(__int64 *)Data));
  #else
                        PPRINT(("%s: %lld",Name,*(long long *)Data));
  #endif
                        break;
                case DA_INT_TYPE: 
                        PPRINT(("%s: %i",Name,*(int *)Data));
                        break;
                case DA_SHORT_TYPE: 
                        PPRINT(("%s: %i",Name,*(short *)Data));
                        break;
                case DA_FLOAT_TYPE:
                        if (Length == 4)
                                {
                                PPRINT(("%s: %G",Name,*(float *)Data));
                                }
                        else    {
                                PPRINT(("%s: %G",Name,*(double *)Data));
                                }
                        break;
                case DA_PACKED_TYPE:
                        PrintPacked(Name,Digits,Scale,(unsigned char *)Data);
                        break;
                case DA_BLOB_TYPE:
                        PPRINT(("%s: ",Name));
                        PHexDump(Data,Length);
                        break;
                case DA_CHAR_TYPE:
                case DA_CLOB_TYPE:
                case DA_VARCHAR_TYPE:
                        {
                        if (CCSID == 65535)
                                {
                                PPRINT(("%s: ",Name));
                                PHexDump(Data,Length);
                                }
                        else    {
                                size_t TrimLen;
                                TrimLen = strlenCHAR(Data,Length,CCSID);
                                PrintTextCCSID(Name,Data,TrimLen,CCSID);
                                }
                        break;
                        }
                case DA_DBCLOB_TYPE:
                case DA_WCHAR_TYPE:
                case DA_VARWCHAR_TYPE:
                        {
                        size_t TrimLen = strlenCHARU((u2char_t*)Data,Length);
                        PrintTextCCSID(Name,Data,TrimLen,CCSID);
                        break;
                        }
                case DA_CLOBLOC_TYPE:
                case DA_BLOBLOC_TYPE:
                case DA_DBCLOBLOC_TYPE:
                        {
                        LobLocator_t Locator = *(LobLocator_t *)Data;
                        PPRINT(("%s: Locator(0x%X)",Name,Locator)); 
                        break;
                        }
                case DA_TIMESTAMP_TYPE:
                        PrintTextCCSID(Name,Data,19,500);
                        break;
                default:
                        PPRINT(("%s: ",Name));
                        PHexDump(Data,Length);
                        break;
                }
        return;
        }

void PrintColumn(DAVariables &Variables, int Col)
        {
        int ColumnCount = Variables.GetColumnCount();
        if (Col >= ColumnCount)
                {
                TERROR(("Column %i doesn't exist (ColumnCount = %i)",
                        Col,
                        ColumnCount));
                return;
                }
        int Type = Variables.GetVariableType(Col);
        const char* Name = Variables.GetVariableName(Col);
        size_t MaxLength = Variables.GetVariableMaxLength(Col);
        unsigned Digits = Variables.GetVariableDigits(Col);
        unsigned Scale = Variables.GetVariableScale(Col);
        switch (Type + (Type % 2 ? 0 : 1))
                {
                case DA_BIG_TYPE: PPRINT(("%s: BIG(%i)",Name,MaxLength)); 
                                  break;
                case DA_INT_TYPE: PPRINT(("%s: INT(%i)",Name,MaxLength)); 
                                  break;
                case DA_SHORT_TYPE: PPRINT(("%s: SHORT(%i)",Name,MaxLength));
                                    break;
                case DA_FLOAT_TYPE: PPRINT(("%s: FLOAT(%i)",Name,MaxLength));
                                     break;
                case DA_PACKED_TYPE: 
                        {
                        unsigned nDigits = MaxLength * 2 - 1;
                        PPRINT(("%s: PACKED(%i,%i)",Name,Digits,Scale));
                        break;
                        }
                case DA_BLOB_TYPE: PPRINT(("%s: BLOB(%i)",Name,MaxLength));
                                   break;
                case DA_CHAR_TYPE: PPRINT(("%s: CHAR(%i)",Name,MaxLength));
                                   break;
                case DA_CLOB_TYPE: PPRINT(("%s: CLOB(%i)",Name,MaxLength));
                                   break;
                case DA_VARCHAR_TYPE: PPRINT(("%s: VARCHAR(%i)",Name,
                                                                MaxLength));
                                      break;
                case DA_DBCLOB_TYPE: PPRINT(("%s: DBCLOB(%i)",Name,
                                                              MaxLength));
                                     break;
                case DA_WCHAR_TYPE: PPRINT(("%s: WCHAR(%i)",Name,MaxLength));
                                     break;
                case DA_VARWCHAR_TYPE: PPRINT(("%s: VARWCHAR(%i)",Name,
                                                                  MaxLength));
                                       break;
                case DA_XML_TYPE: PPRINT(("%s: XML(%i)",Name,MaxLength));
                                  break;
                case DA_CLOBLOC_TYPE: PPRINT(("%s: CLOB Locator",Name));
                                      break;
                case DA_BLOBLOC_TYPE: PPRINT(("%s: BLOB Locator",Name));
                                      break;
                case DA_DBCLOBLOC_TYPE: PPRINT(("%s: DBCLOB Locator"));
                                        break;
                default: PPRINT(("%s: Unknown Type: %u(%u)",Name,
                                                            Type,
                                                            MaxLength));
                }
        return;
        }

size_t PrintLOB(const char *ColumnName, PRCED_Locator &Locator, size_t MaxLen)
        {
        union   {
                unsigned char BLOB[PrintLOB_LINELEN];
                char CLOB[PrintLOB_LINELEN];
                u2char_t DBCLOB[PrintLOB_LINELEN];
                } Buffer;
        size_t LobLength = Locator.GetLength();
        PPRINT(("LobLength: %u",LobLength));
        size_t ReadPos = 0;
        size_t Remaining = LobLength > MaxLen ? MaxLen : LobLength;
        unsigned LobCCSID = Locator.GetLobCCSID();
        while (Remaining)
                {         
                size_t ReadLen = Remaining;
                if (ReadLen > PrintLOB_LINELEN) ReadLen = PrintLOB_LINELEN;
                switch (Locator.GetLobType())
                        {
                        case DA_CLOB_TYPE: 
                                Locator.GetSubString(Buffer.CLOB,
                                                     ReadPos,
                                                     ReadLen);
                                PrintTextCCSID(ColumnName,
                                               Buffer.CLOB,
                                               ReadLen,
                                               LobCCSID);
                                break;
                        case DA_BLOB_TYPE:
                                Locator.GetSubString(Buffer.BLOB,
                                                     ReadPos,
                                                     ReadLen);
                                PPRINT(("%s: ",ColumnName));
                                PHexDump(Buffer.BLOB,ReadLen);
                                break;
                        case DA_DBCLOB_TYPE:
                                Locator.GetSubString(Buffer.DBCLOB,
                                                     ReadPos,
                                                     ReadLen);
                                PrintTextCCSID(ColumnName,
                                               Buffer.DBCLOB,
                                               ReadLen,
                                               LobCCSID);
                                break;
                        }
                ReadPos += ReadLen;
                Remaining -= ReadLen;
                }
        return LobLength > MaxLen ? MaxLen : LobLength;
        }

size_t PrintLOB(DAVariables &Variables, int Row, int Col, size_t MaxLen)
        {
        const char *ColumnName = Variables.GetVariableName(Col);
        PRCED_Locator Locator(Variables,Row,Col);
        return PrintLOB(ColumnName,Locator,MaxLen);
        }

void PrintSQLCodeText(int SQLCode)
        {
        const char *Text = NULL;
        switch (SQLCode < 0 ? -SQLCode : SQLCode)
                {
                case 100: Text = "Record not found."; break;
                case 104: Text = "Token not valid."; break;
                case 204: Text = "Object not found."; break;
                case 206: Text = "Column not in specified tables."; break;
                case 501: Text = "Cursor not open."; break;
                case 502: Text = "Cursor already open."; break;
                case 514: Text = "Prepared statement not found."; break;
                case 516: Text = "Prepared statement not found."; break;
                case 601: Text = "Object already exists."; break;
                }
        if (Text) 
                {
                if (SQLCode < 0) TERROR(("\"%s\"",Text));
                else TINFO(("\"%s\"",Text));
                }
        return;
        }

void PrintSQLStatementText(PRCED_Statement &Statement)
        {
        const char *SQLText = Statement.GetStatementText();
        size_t nChars = Statement.GetStatementLength();
        unsigned CCSID = Statement.GetStatementCCSID();
        PrintTextCCSID("SQLText",SQLText,nChars,CCSID);
        return;
        }

// ***************************************************************************
// **************************** End of File **********************************
// ***************************************************************************
