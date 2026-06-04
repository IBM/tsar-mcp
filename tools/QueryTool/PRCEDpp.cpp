///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: PRCEDpp.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
/* Function: C++ Classes into the AS/400 QSQPRCED Database API */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __BORLANDC__
        #include <wchar.h>
#endif

#include <ebcdic.h>
#include <LevelTrace.h>
#include <HexDump.h>

#ifdef __OS400_TGTVRM__
        #include <SnapStack.h>
#endif

#include <PRCEDpp.h>

#define AS4_OBJ_LEN 10
#define AUTO_DIRECT_MAP true                            // true/false.
#define MAX_DESCRIBE_COLUMNS 100
#define MAX_PACKED_DIGITS 127                           // Must be odd.

#define LOBPACKAGELIB_DEFAULT "QTEMP"
#define LOBPACKAGE_DEFAULT "LOBACCESS"

#define IS_VAR(x) (x==DA_VARCHAR_TYPE || x==DA_VARWCHAR_TYPE)
#define IS_LOB(x) (x==DA_BLOB_TYPE ||                                        \
                   x==DA_CLOB_TYPE ||                                        \
                   x==DA_DBCLOB_TYPE ||                                      \
                   x==DA_XML_TYPE)
#define IS_CHAR(x) (x==DA_CLOB_TYPE ||                                       \
                    x==DA_CHAR_TYPE ||                                       \
                    x==DA_VARCHAR_TYPE ||                                    \
                    x==DA_XML_TYPE)
#define IS_WCHAR(x) (x==DA_DBCLOB_TYPE ||                                    \
                     x==DA_WCHAR_TYPE ||                                     \
                     x==DA_VARWCHAR_TYPE)
#define IS_TEXT(x) (IS_CHAR(x) || IS_WCHAR(x))
#define IS_PACKED(x) (x==DA_PACKED_TYPE)

#define TRACE_SQLCODE(Procedure,sqlca)                                       \
        {                                                                    \
        if ((sqlca).sqlcode < 0)                                             \
                {                                                            \
                TERROR((#Procedure ": %d", (sqlca).sqlcode));                \
                THexDump((char *)(sqlca).sqlerrmc,(sqlca).sqlerrml);         \
                }                                                            \
        else if ((sqlca).sqlcode != 0)                                       \
                {                                                            \
                TDEBUG((#Procedure ": %d", (sqlca).sqlcode));                \
                }                                                            \
        }

#define ExDOHeaderLen (2*sizeof(int)) 

/* ************************************************************************ */
/* **** "XDA" Emulation when using QSQPRCED ******************************* */
/* ************************************************************************ */

#ifndef __QSQPRCED_VIA_XDA

        #define QSQPRCEDEx(sqlca,                                            \
                           sqlda,                                            \
                           format,                                           \
                           qsq,                                              \
                           exdo,                                             \
                           exdo_size,                                        \
                           extdynopts,                                       \
                           ErrorCode)                                        \
                {                                                            \
                if (exdo) (exdo)->Bytes_Returned = 0;                        \
                QSQPRCED(sqlca,                                              \
                         sqlda,                                              \
                         format,                                             \
                         qsq,                                                \
                         ErrorCode);                                         \
                }

        #define QSQPRCEDOpt(sqlca,                                           \
                            sqlda,                                           \
                            format,                                          \
                            qsq,                                             \
                            extdynopts,                                      \
                            ErrorCode)                                       \
                {                                                            \
                QSQPRCED(sqlca,                                              \
                         sqlda,                                              \
                         format,                                             \
                         qsq,                                                \
                         ErrorCode);                                         \
                }

        typedef struct Qxda_EXDO0100
        {
           int     Bytes_Returned;
           int     Bytes_Available;
           char    Statement_Name[18];
        } Qxda_EXDO0100_t;

        #define QXDA_EXTDYN_NOOPTS 0
        #define QXDA_CREATE_OBJECTS 0
        #define QXDA_FIND_STMT 0

#endif /* __QSQPRCED_VIA_XDA */                                               

/* ************************************************************************ */
/* **** FindTopQSYSProgram ************************************************ */
/* ************************************************************************ */
                                  
#ifdef __OS400_TGTVRM__

static const char* FindTopQSYSProgram()
        {
        static const char *ProcName = "FindTopQSYSProgram";
        static char QSYSProgram[AS4_OBJ_LEN+1] = {0};
        if (*QSYSProgram) return QSYSProgram;
        InvocationCallStack Stack;
        bool Status = Stack.Snapshot();
        if (!Status)
                {
                TERROR(("%s: Stack.Snapshot() Failed",ProcName));
                return "";
                }
        InvocationEntry *Current = Stack.GetLastEntry();
        while (Current)
                {
                if (strcmp(Current->GetContext(),"QSYS") == 0) break;
                Current = Stack.GetPrevEntry(Current);
                }
        if (Current)
                {
                size_t Length = strlen(Current->GetProgram());
                if (Length > AS4_OBJ_LEN) Length = AS4_OBJ_LEN;
                memcpy(QSYSProgram,Current->GetProgram(),Length);
                QSYSProgram[Length] = '\0';
                Stack.FreeEntry(Current);
                }
        return QSYSProgram;
        }

#endif /* __OS400_TGTVRM__ */

/* ************************************************************************ */
/* **** Utilities ********************************************************* */
/* ************************************************************************ */

static void padcpy(char *Dest, const char *Src, size_t DestLen, size_t SrcLen)
        {
        for (size_t i=0; i < DestLen; i++)
                {
                if (SrcLen && *Src)
                        {
                        *Dest++ = *Src++;
                        SrcLen--;
                        }
                else *Dest++ = ' ';
                }
        return;
        }

static void padcpy(char *Dest, const char *Source, size_t DestLen)
        {
        for (size_t i=0; i < DestLen; i++)
                {
                if (*Source) *Dest++ = *Source++;
                else *Dest++ = ' ';
                }
        return;
        }

static char *rtrim(char *string)
        {
        if (!string) return NULL;
        char *pos = string + strlen(string);
        while (pos != string && isspace(*(pos-1))) pos--;
        *pos = '\0';
        return string;
        }

static unsigned RoundLog2(size_t Value)         // 2^Log2 Greater or Equal.
        {
        if (Value == 0) return 0;
        Value--;
        unsigned Log2Value = 0;
        while (Value) 
                {
                Value >>= 1;
                Log2Value++;
                }
        return Log2Value;
        }

static size_t RoundPower2(size_t Value)         // Power2 Greater or Equal.
        {
        unsigned Log2Value = RoundLog2(Value);
        return (size_t)1 << Log2Value;
        }

bool isLobLocator(DAVariables &Variables, int Col)
        {
        int Type = Variables.GetVariableType(Col);
        bool isLocator = Type == DA_CLOBLOC_TYPE ||
                         Type == DA_BLOBLOC_TYPE ||
                         Type == DA_DBCLOBLOC_TYPE;
        return isLocator;
        }

#if U2CHAR_IS_WCHAR
        #define ustrlen wcslen
#else
        static size_t ustrlen(const u2char_t *string)
                {
                size_t i = 0;
                while (*string++) i++;
                return i;
                }
#endif

inline void a2ucpy(u2char_t *dest, const char *src, size_t len)
        {
        for (unsigned i=0; i<len; i++) *dest++ = *src++;
        }

// ***************************************************************************
// **** Operating Mode *******************************************************
// ***************************************************************************

PRCEDpp_VIA_t GetPRCEDvia()
        {
  #if defined(__QXDAEDRS_XDN_BRIDGE)
        return PRCEDpp_VIA_XDN;
  #elif defined(__QSQPRCED_VIA_XDA)
        return PRCEDpp_VIA_XDA;
  #else 
        return PRCEDpp_VIA_QSQPRCED;
  #endif
        }

/* ************************************************************************ */
/* **** SQLCtrlBlock ****************************************************** */
/* ************************************************************************ */

SQLCtrlBlock::SQLCtrlBlock(SQLCtrlBlock &CB) 
        {
        Qsq = NULL; 
        *this = CB;
        return;
        }

SQLCtrlBlock::~SQLCtrlBlock()
        {
        free(Qsq);
        return;
        }

unsigned SQLCtrlBlock::GetStatementCCSID()
        {
        return Qsq->Statement_Text_CCSID;
        }

bool SQLCtrlBlock::HasStatementName()
        {
        size_t NameLength;
        const char* StatementName = GetStatementName(&NameLength);
        return NameLength && StatementName[0] && StatementName[0] != ' ';
        }

void SQLCtrlBlock::SetBlockingFactor(int Rows)
        {
        Qsq->Blocking_Factor = Rows;
        return;
        }

void SQLCtrlBlock::SetDirectMap(bool DirectMap)
        {
        if (!Qsq) return;
        Qsq->Direct_Map = DirectMap ? 'Y' : 'N';
        return;
        }

void SQLCtrlBlock::SetFunction(char Function)
        {
        if (!Qsq) return;
        Qsq->Function = Function;
        return;
        }

void SQLCtrlBlock::SetReopen(bool Reopen)
        {
        Qsq->Reopen = Reopen ? '1' : '0';
        return;
        }

void SQLCtrlBlock::SetRowsToInsert(int nInsertRows)
        {
        if (!Qsq) return;
        Qsq->Rows_To_Insert = nInsertRows;
        return;
        }

void SQLCtrlBlock::SetPackage(const char *Library, const char *Package)
        {
        if (!Qsq) return;
        padcpy(Qsq->Library_Name,Library,OBJNAMELEN);
        padcpy(Qsq->SQL_Package_Name,Package,OBJNAMELEN);
        return;
        }

void SQLCtrlBlock::SetProgram(const char *Library, const char *Program)
        {
        if (!Qsq) return;
        padcpy(Qsq->Main_Lib,Library,OBJNAMELEN);
        padcpy(Qsq->Main_Pgm,Program,OBJNAMELEN);  
        return;
        }

void SQLCtrlBlock::SetSortSeq(const char *Library, const char *SortSeq)
        {
        if (!Qsq) return;
        padcpy(Qsq->Sort_Sequence_Table, SortSeq, OBJNAMELEN);
        padcpy(Qsq->Sort_Sequence_Library, Library, OBJNAMELEN);
        return;
        }

void SQLCtrlBlock::SetStatementName(const char *Name)
        {
        size_t NameLength = Name ? strlen(Name) : 0;
        SetStatementName(Name,NameLength);
        return;
        }

void SQLCtrlBlock::SetStatementText(const char *Text)
        {
        size_t TextLength = Text ? strlen(Text) : 0;
        SetStatementText(Text,TextLength);
        return;
        }

/* ************************************************************************ */
/* **** SQLCtrlBlock 400 ************************************************** */
/* ************************************************************************ */

#if !PRCEDpp_USE410

SQLCtrlBlock::SQLCtrlBlock()
        {
        Qsq = (Qsq_SQLP0400_t *)malloc(sizeof(Qsq_SQLP0400_t));
        if (!Qsq) throw MemoryError("SQLCtrlBlock");
        memset(Qsq, 0, sizeof(Qsq_SQLP0400_t));
        Qsq->Open_Options = (char)0x80;
        Qsq->Commitment_Control = 'C';
        memcpy(Qsq->Date_Format,"USA",3);
        Qsq->Date_Separator = '/';
        memcpy(Qsq->Time_Format,"USA",3);
        Qsq->Time_Separator  = ':';
        memcpy(Qsq->Naming_Option,"SYS",3);
        Qsq->Decimal_Point = '.';
        Qsq->Reval = 'Y';
        memcpy(Qsq->Language_ID, "*JOB      ", 10);
        Qsq->Allow_Copy_Data = 'S';
        Qsq->Allow_Blocking = 'L';
        Qsq->Statement_Text_CCSID = LOCAL_CCSID; // SQL0104: Delete Package!
  #ifdef __OS400_TGTVRM__
        SetProgram("QSYS",FindTopQSYSProgram());
  #endif
        Qsq->Clause_For_Describe = 'N';
        Qsq->Blocking_Factor = 1;
        return;
        }

const char* SQLCtrlBlock::GetStatementName(size_t *NameLength)
        {
        if (!Qsq) 
                {
                if (NameLength) *NameLength = 0;
                return NULL;
                }
        if (NameLength) *NameLength = 18;
        return Qsq->Statement_Name;
        }

size_t SQLCtrlBlock::GetStatementLength()
        {
        return Qsq->Statement_Length;
        }

const char* SQLCtrlBlock::GetStatementText()
        {
        char *Statement = (char *)((byte_t *)&Qsq->Statement_Length +
                                   sizeof(Qsq->Statement_Length));
        return Statement;
        }

void SQLCtrlBlock::SetCursor(const char *CursorName)
        {
        if (!Qsq) return;
        padcpy(Qsq->Cursor_Name,CursorName,sizeof(Qsq->Cursor_Name));
        return;
        }

void SQLCtrlBlock::SetStatementName(const char *Name, size_t Length)
        {
        if (!Qsq) return;
        size_t MinLength = Length;
        size_t MaxLength = sizeof(Qsq->Statement_Name);
        if (MinLength > MaxLength) MinLength = MaxLength;
        padcpy(Qsq->Statement_Name,Name,MaxLength,MinLength);
        return;
        }

void SQLCtrlBlock::SetStatementText(const char *Text, size_t TextLength)
        {
        size_t Length = sizeof(Qsq_SQLP0400_t) + TextLength;
        Qsq = (Qsq_SQLP0400_t *)realloc(Qsq,Length);
        if (!Qsq) throw MemoryError("SetStatementText");
        char *Statement = (char *)((byte_t *)&Qsq->Statement_Length +
                                   sizeof(Qsq->Statement_Length));
        memcpy(Statement,Text,TextLength);
        Qsq->Statement_Length = TextLength;
        return;
        }

SQLCtrlBlock& SQLCtrlBlock::operator = (SQLCtrlBlock &CB)
        {
        if (this == &CB) return CB;
        size_t Length = sizeof(Qsq_SQLP0400_t) + CB.Qsq->Statement_Length;
        Qsq = (Qsq_SQLP0400_t *)realloc(Qsq,Length);
        if (!Qsq) throw MemoryError("SQLCtrlBlock operator =");
        memcpy(Qsq,CB.Qsq,Length);
        return *this;
        }

#else /* PRCEDpp_USE410 */
            
/* ************************************************************************ */
/* **** Qsq_SQLP0410 Allocation Functions ********************************* */
/* ************************************************************************ */

static Qsq_SQLP0410_t* AllocateQsqP410(size_t Statement_Length,
                                       size_t StatementID_Length,
                                       size_t CursorName_Length,
                                       size_t UserDefField_Length,
                                       const Qsq_SQLP0410_t *Template)
        {
        static const char *ProcName = "AllocateQsqP410";
        char Statement_Length_Type = 0;
        size_t Statement_Offset = 0;
        size_t StatementID_Offset = 0;
        size_t CursorName_Offset = 0;
        size_t UserDefField_Offset = 0;
        size_t TotalLength = sizeof(Qsq_SQLP0410_t);
        // ************************
        // **** Statement Text ****
        // ************************
        Statement_Offset = TotalLength;
        TotalLength += Statement_Length;
        if (Statement_Length > 0xFFFF) 
                {
                TotalLength += 4;
                Statement_Length_Type = '1';
                }
        else    {
                TotalLength += 2;
                Statement_Length_Type = '0';
                }
        // **********************
        // **** Statement ID ****
        // **********************
        if (StatementID_Length > 18) 
                {
                StatementID_Offset = TotalLength;
                TotalLength += StatementID_Length;
                }
        else StatementID_Length = 0;
        // *********************
        // **** Cursor Name ****
        // *********************
        if (CursorName_Length > 18) 
                {
                CursorName_Offset = TotalLength;
                TotalLength += CursorName_Length;
                }
        else CursorName_Length = 0;
        // ****************************
        // **** User Defined Field ****
        // ****************************
        if (UserDefField_Length > 18) 
                {
                UserDefField_Offset = TotalLength;
                TotalLength += UserDefField_Length;
                }
        else UserDefField_Length = 0;
        Qsq_SQLP0410_t *QsqRC = (Qsq_SQLP0410_t *)malloc(TotalLength);
        if (!QsqRC)
                {
                TERROR(("%s: Can't malloc %u Bytes",ProcName,TotalLength));
                return NULL;
                }
        if (Template)
                {
                memcpy(QsqRC,Template,sizeof(Qsq_SQLP0410_t));
                }
        else    {
                memset(QsqRC,0,sizeof(Qsq_SQLP0410_t));
                QsqRC->Open_Options = (char)0x80;
                QsqRC->Commitment_Control = 'C';
                memcpy(QsqRC->Date_Format,"USA",3);
                QsqRC->Date_Separator = '/';
                memcpy(QsqRC->Time_Format,"USA",3);
                QsqRC->Time_Separator  = ':';
                memcpy(QsqRC->Naming_Option,"SYS",3);
                QsqRC->Decimal_Point = '.';
                QsqRC->Reval = 'Y';
                memcpy(QsqRC->Language_ID, "*JOB      ", 10);
                QsqRC->Allow_Copy_Data = 'S';
                QsqRC->Allow_Blocking = 'L';
                QsqRC->Statement_Text_CCSID = LOCAL_CCSID;
          #ifdef __OS400_TGTVRM__
                padcpy(QsqRC->Main_Lib,"QSYS",OBJNAMELEN);
                padcpy(QsqRC->Main_Pgm,FindTopQSYSProgram(),OBJNAMELEN);
          #endif
                QsqRC->Clause_For_Describe = 'N';
                QsqRC->Blocking_Factor = 1;
                }
        size_t AdditionalLength = sizeof(Qsq_SQLP0410_t) - 
                                  offsetof(Qsq_SQLP0410_t,Connection_Handle);
        QsqRC->Length_Of_Additional_Fields = AdditionalLength;
        QsqRC->Ext_User_Defined_Field_Length = UserDefField_Length; 
        QsqRC->Ext_User_Defined_Field_Offset = UserDefField_Offset;
        QsqRC->Extended_Cursor_Name_Length = CursorName_Length;
        QsqRC->Extended_Cursor_Name_Offset = CursorName_Offset;
        QsqRC->Extended_Statement_Name_Length = StatementID_Length;
        QsqRC->Extended_Statement_Name_Offset = StatementID_Offset;
        QsqRC->Statement_Length_Type = Statement_Length_Type;
        QsqRC->Statement_Offset = Statement_Offset;
        if (Statement_Length_Type == '1') 
                {
                unsigned char *Pos = (unsigned char *)QsqRC;
                Pos += QsqRC->Statement_Offset;
                *(unsigned *)Pos = Statement_Length;
                }
        else    {
                unsigned char *Pos = (unsigned char *)QsqRC;
                Pos += QsqRC->Statement_Offset;
                *(unsigned short *)Pos = Statement_Length;
                }
        return QsqRC;
        }

static Qsq_SQLP0410_t* _ReallocateQsqP410(const Qsq_SQLP0410_t *Qsq,
                                          size_t Statement_Length,
                                          size_t StatementID_Length,
                                          size_t CursorName_Length,
                                          size_t UserDefField_Length)
        {
        static const char *ProcName = "_ReallocateQsqP410";
        Qsq_SQLP0410_t *QsqRC = AllocateQsqP410(Statement_Length,      
                                                StatementID_Length,
                                                CursorName_Length,    
                                                UserDefField_Length,
                                                Qsq);
        if (!QsqRC)
                {
                TERROR(("%s: Can't Allocate new Qsq",ProcName));
                return NULL;
                }
        if (!Qsq)
                {
                return QsqRC;
                }
        unsigned char *Dest = (unsigned char *)QsqRC; 
        unsigned char *Source = (unsigned char *)Qsq; 
        // ************************
        // **** Statement Text ****
        // ************************
        unsigned char *SourcePos = Source + Qsq->Statement_Offset;
        unsigned char *DestPos = Dest + QsqRC->Statement_Offset;
        size_t MinLength;
        size_t MaxLength;
        if (Qsq->Statement_Length_Type == '1')
                {
                MinLength = *(unsigned *)SourcePos;
                SourcePos += sizeof(unsigned);
                }
        else    {
                MinLength = *(unsigned short *)SourcePos;
                SourcePos += sizeof(unsigned short);
                }
        if (QsqRC->Statement_Length_Type == '1')
                {
                MaxLength = *(unsigned *)DestPos;
                DestPos += sizeof(unsigned);
                }
        else    {
                MaxLength = *(unsigned short *)DestPos;
                DestPos += sizeof(unsigned short);
                }
        if (MinLength > MaxLength) MinLength = MaxLength;
        memcpy(DestPos,SourcePos,MinLength);
        // **********************
        // **** Statement ID ****
        // **********************
        if (Qsq->Extended_Statement_Name_Length > 18)
                {
                MinLength = Qsq->Extended_Statement_Name_Length;
                SourcePos = Source + Qsq->Extended_Statement_Name_Offset;
                }
        else    {
                MinLength = 18;
                SourcePos = (unsigned char *)&Qsq->Statement_Name;
                }
        if (QsqRC->Extended_Statement_Name_Length > 18)
                {
                MaxLength = QsqRC->Extended_Statement_Name_Length;
                DestPos = Dest + QsqRC->Extended_Statement_Name_Offset;
                }
        else    {
                MaxLength = 18;
                DestPos = (unsigned char *)&QsqRC->Statement_Name;
                }
        if (MinLength > MaxLength) MinLength = MaxLength;
        memcpy(DestPos,SourcePos,MinLength);
        // *********************
        // **** Cursor Name ****
        // *********************
        if (Qsq->Extended_Cursor_Name_Length > 18)
                {
                MinLength = Qsq->Extended_Cursor_Name_Length;
                SourcePos = Source + Qsq->Extended_Cursor_Name_Offset;
                }
        else    {
                MinLength = 18;
                SourcePos = (unsigned char *)&Qsq->Cursor_Name;
                }
        if (QsqRC->Extended_Cursor_Name_Length > 18)
                {
                MaxLength = QsqRC->Extended_Cursor_Name_Length;
                DestPos = Dest + QsqRC->Extended_Cursor_Name_Offset;
                }
        else    {
                MaxLength = 18;
                DestPos = (unsigned char *)&QsqRC->Cursor_Name;
                }
        if (MinLength > MaxLength) MinLength = MaxLength;
        memcpy(DestPos,SourcePos,MinLength);
        // ****************************
        // **** User Defined Field ****
        // ****************************
        if (Qsq->Ext_User_Defined_Field_Length > 18)
                {
                MinLength = Qsq->Ext_User_Defined_Field_Length;
                SourcePos = Source + Qsq->Ext_User_Defined_Field_Offset;
                }
        else    {
                MinLength = 18;
                SourcePos = (unsigned char *)&Qsq->User_Defined_Field;
                }
        if (QsqRC->Ext_User_Defined_Field_Length > 18)
                {
                MaxLength = QsqRC->Ext_User_Defined_Field_Length;
                DestPos = Dest + QsqRC->Ext_User_Defined_Field_Offset;
                }
        else    {
                MaxLength = 18;
                DestPos = (unsigned char *)&QsqRC->User_Defined_Field;
                }
        if (MinLength > MaxLength) MinLength = MaxLength;
        memcpy(DestPos,SourcePos,MinLength);
        return QsqRC;
        }

static Qsq_SQLP0410_t* ReallocateQsqP410(Qsq_SQLP0410_t *Qsq,
                                         size_t Statement_Length,
                                         size_t StatementID_Length,
                                         size_t CursorName_Length,
                                         size_t UserDefField_Length)
        {
        Qsq_SQLP0410_t *QsqRC = _ReallocateQsqP410(Qsq,
                                                   Statement_Length,      
                                                   StatementID_Length,
                                                   CursorName_Length,    
                                                   UserDefField_Length);
        if (QsqRC && Qsq) free(Qsq);
        return QsqRC;
        }

static Qsq_SQLP0410_t* _ResizeQsqP410(const Qsq_SQLP0410_t *Qsq,
                                      int Statement_Length,
                                      int StatementID_Length,
                                      int CursorName_Length,
                                      int UserDefField_Length)
        {
        static const char *ProcName = "_ResizeQsqP410";
        unsigned char *Source = (unsigned char *)Qsq; 
        unsigned char *Pos = Source + Qsq->Statement_Offset;
        if (!Statement_Length)
                {
                Statement_Length = Qsq->Statement_Length_Type == '1'
                                   ? *(unsigned *)Pos
                                   : *(unsigned short *)Pos;
                }
        if (!StatementID_Length)
                {
                if (Qsq->Extended_Statement_Name_Length > 18)
                        {
                        size_t Length = Qsq->Extended_Statement_Name_Length;
                        StatementID_Length = Length;
                        }
                }
        if (!CursorName_Length)
                {
                if (Qsq->Extended_Cursor_Name_Length > 18)
                        {
                        CursorName_Length = Qsq->Extended_Cursor_Name_Length;
                        }
                }
        if (!UserDefField_Length)
                {
                if (Qsq->Ext_User_Defined_Field_Length > 18)
                        {
                        size_t Length = Qsq->Ext_User_Defined_Field_Length;
                        UserDefField_Length = Length;
                        }
                }
        Qsq_SQLP0410_t *QsqRC = _ReallocateQsqP410(Qsq,
                                                   Statement_Length,      
                                                   StatementID_Length,
                                                   CursorName_Length,    
                                                   UserDefField_Length);
        if (!QsqRC)
                {
                TERROR(("%s: Can't Reallocate new Qsq",ProcName));
                }
        return QsqRC;
        }

static Qsq_SQLP0410_t* ResizeQsqP410(Qsq_SQLP0410_t *Qsq,
                                     int Statement_Length,
                                     int StatementID_Length,
                                     int CursorName_Length,
                                     int UserDefField_Length)
        {
        Qsq_SQLP0410_t *QsqRC = _ResizeQsqP410(Qsq,
                                               Statement_Length,      
                                               StatementID_Length,
                                               CursorName_Length,    
                                               UserDefField_Length);
        if (QsqRC && Qsq) free(Qsq);
        return QsqRC;
        }

/* ************************************************************************ */
/* **** SQLCtrlBlock 410 ************************************************** */
/* ************************************************************************ */

SQLCtrlBlock::SQLCtrlBlock()
        {
        Qsq = AllocateQsqP410(0,0,0,0,NULL);
        if (!Qsq) throw MemoryError("SQLCtrlBlock");
        return;
        }

const char* SQLCtrlBlock::GetStatementName(size_t *NameLength)
        {
        if (!Qsq) 
                {
                if (NameLength) *NameLength = 0;
                return NULL;
                }
        const char *StatementName;
        size_t ExtendedLength = Qsq->Extended_Statement_Name_Length;
        if (ExtendedLength)
                {
                StatementName = (const char *)Qsq;
                StatementName += Qsq->Extended_Statement_Name_Offset;
                if (NameLength) *NameLength = ExtendedLength;
                }
        else    {
                StatementName = Qsq->Statement_Name;
                if (NameLength) *NameLength = 18;
                }                
        return Qsq->Statement_Name;
        }

size_t SQLCtrlBlock::GetStatementLength()
        {
        unsigned char *Pos = (unsigned char *)Qsq;
        Pos += Qsq->Statement_Offset;
        size_t Length = Qsq->Statement_Length_Type == '1'
                        ? *(unsigned *)Pos
                        : *(unsigned short *)Pos;
        return Length;
        }

const char* SQLCtrlBlock::GetStatementText()
        {
        const char *Pos = (const char *)Qsq;
        Pos += Qsq->Statement_Offset;
        Pos += Qsq->Statement_Length_Type == '1' ? 4 : 2;
        return Pos;
        }

void SQLCtrlBlock::SetCursor(const char *CursorName)
        {
        if (!Qsq) return; 
        size_t NameLength = strlen(CursorName);
        Qsq = ResizeQsqP410(Qsq,0,NameLength,0,0);
        if (!Qsq) throw MemoryError("SetCursor");
        if (Qsq->Extended_Cursor_Name_Length)
                {
                unsigned char *Pos = (unsigned char *)Qsq;
                Pos += Qsq->Extended_Cursor_Name_Offset;
                memcpy(Pos,CursorName,NameLength);
                memset(Qsq->Cursor_Name,' ',sizeof(Qsq->Cursor_Name));
                }
        else    {
                padcpy(Qsq->Cursor_Name,CursorName,sizeof(Qsq->Cursor_Name));
                }
        return;
        }

void SQLCtrlBlock::SetStatementName(const char *Name, size_t Length)
        {
        static const char *ProcName = "SQLCtrlBlock::SetStatementName";
        if (!Qsq) return;
        Qsq = ResizeQsqP410(Qsq,0,0,Length,0);
        if (!Qsq) throw MemoryError("SetStatementName");
        if (!Qsq->Extended_Statement_Name_Offset)
                {
                size_t MinLength = Length;
                size_t MaxLength = sizeof(Qsq->Statement_Name);
                if (MinLength > MaxLength) MinLength = MaxLength;
                padcpy(Qsq->Statement_Name,Name,MaxLength,MinLength);
                }
        else if (Qsq->Extended_Statement_Name_Length != (int)Length)
                {
                TERROR(("%s: Name Length Mismatch",ProcName));
                throw Error("Name Length Mismatch");
                }
        else    {
                unsigned char *Pos = (unsigned char *)Qsq;
                Pos += Qsq->Extended_Statement_Name_Offset;
                memcpy(Pos,Name,Length);
                }
        return;
        }

void SQLCtrlBlock::SetStatementText(const char *Text, size_t TextLength)
        {
        if (!Qsq) return;
        Qsq = ResizeQsqP410(Qsq,TextLength,0,0,0);
        if (!Qsq) throw MemoryError("SetStatementText");
        unsigned char *Pos = (unsigned char *)Qsq;
        Pos += Qsq->Statement_Offset;
        Pos += Qsq->Statement_Length_Type == '1' ? 4 : 2;
        memcpy(Pos,Text,TextLength);
        return;
        }

SQLCtrlBlock& SQLCtrlBlock::operator = (SQLCtrlBlock &CB)
        {
        if (this == &CB) return CB;
        Qsq = _ResizeQsqP410(CB.Qsq,0,0,0,0);
        if (!Qsq) throw MemoryError("SQLCtrlBlock operator =");
        return *this;
        }

#endif /* PRCEDpp_USE410 */

/* ************************************************************************ */
/* **** SQLCtrlBlockCache ************************************************* */
/* ************************************************************************ */

SQLCtrlBlock* SQLCtrlBlockCache::GetCB()
        {
        ListMutex.Take();
        SQLCtrlBlock *CB = GetFirst();
        if (CB) Remove(CB);
        ListMutex.Release();
        if (CB) 
                {
                CB->SetCursor("");
                CB->SetStatementName("");
                CB->SetStatementText("");
                }
        else    {
                CB = new SQLCtrlBlock;
                }
        return CB;
        }

void SQLCtrlBlockCache::ReturnCB(SQLCtrlBlock* CB) 
        {
        ListMutex.Take();
        AddTop(CB);
        ListMutex.Release();
        return;
        }

/* ************************************************************************ */
/* **** DAVariables ******************************************************* */
/* ************************************************************************ */

// ********************
// **** DAVariable ****
// ********************

DAVariable::DAVariable(int type, 
                       const char *name, 
                       size_t length,
                       unsigned ccsid)
        {
        Type = type;
        Name = name ? strdup(name) : NULL;
        Digits = 0;
        Scale = 0;
        if (length) Length = length;
        else switch (Type)   
                {
                case DA_BIG_TYPE: Length = 8; break;
                case DA_INT_TYPE: Length = 4; break;
                case DA_SHORT_TYPE: Length = 2; break;
                case DA_FLOAT_TYPE: Length = 8; break;
                case DA_XML_TYPE:
                case DA_BLOBLOC_TYPE:
                case DA_CLOBLOC_TYPE:
                case DA_DBCLOBLOC_TYPE: Length = 4; break;
                case DA_PACKED_TYPE:
                        {
                        unsigned HighOrder = length >> 8;
                        unsigned LowOrder = length & 0xFF;
                        Length = (HighOrder + 2) / 2;
                        Digits = HighOrder;
                        Scale = LowOrder;
                        }
                default: assert(0);
                }
        if (ccsid) CCSID = ccsid;
        else if (IS_TEXT(Type))         // Default Requested Data CCSIDs.
                {
                if (IS_WCHAR(Type)) CCSID = UCS2_CCSID;
                //else CCSID = EBCDIC_CCSID;            // PRCEDpp Converts.
                else CCSID = LOCAL_CCSID;               // Database Converts.
                } 
        else CCSID = 0;
        DataPtr = NULL;
        IndicatorPtr = NULL;
        LobLenPtr = NULL;
        return;
        }

DAVariable::DAVariable(DAVariable &Src)
        {
        Name = NULL;
        *this = Src;
        return;
        }

void DAVariable::SetBuffer(char *Data, short *Indicator, unsigned *LobLen)
        {
        DataPtr = Data;
        IndicatorPtr = Indicator;
        LobLenPtr = LobLen;
        return;
        }

DAVariable& DAVariable::operator =(DAVariable &Src)
        {
        if (this == &Src) return *this;
        Type = Src.Type;
        if (Name) free(Name);
        Name = Src.Name ? strdup(Src.Name) : NULL;
        Digits = Src.Digits;
        Scale = Src.Scale;
        Length = Src.Length;
        CCSID = Src.CCSID;
        DataPtr = Src.DataPtr;
        IndicatorPtr = Src.IndicatorPtr;
        LobLenPtr = Src.LobLenPtr;
        return *this;
        }
           
// *********************
// **** DAVariables ****
// *********************
         
DAVariables::DAVariables()
        {
        DA = NULL;
        DirectMap = false;
        VarCount = 0;
        VarListSize = 0;
        VarList = NULL;
        RecordSize = 0;
        return;
        }

DAVariables::DAVariables(DAVariables &Src)
        {
        DA = NULL;
        VarCount = 0;
        VarListSize = 0;
        VarList = NULL;
        RecordSize = 0;
        *this = Src;
        return;
        }

DAVariables::DAVariables(DAVariables &Src1, DAVariables &Src2)
        {
        DA = NULL;
        VarCount = 0;
        VarListSize = 0;
        VarList = NULL;
        RecordSize = 0;
        for (unsigned i = 0; i < Src1.VarCount; i++)
                {
                AddVariable(*Src1.VarList[i]);
                }
        for (unsigned j = 0; j < Src2.VarCount; j++)
                {
                AddVariable(*Src2.VarList[j]);
                }
        return;
        }

DAVariables::~DAVariables() 
        {
        Clear();
        if (VarList) free(VarList);
        return;
        }

void DAVariables::AddPackedVariable(const char *Name, 
                                    unsigned Digits, 
                                    unsigned Scale)
        {
        if (Digits % 2 == 0) Digits++;          // Digits must be odd.
        size_t Length = (Digits << 8) + Scale;
        DAVariable *Variable = new DAVariable(DA_PACKED_TYPE,Name,Length);
        AddVariable(Variable);
        return;
        }

bool DAVariables::AddVariable(DAVariable *Var)
        {
        return AddVariable(VarCount,Var);
        }

bool DAVariables::AddVariable(int Col, DAVariable *Var)
        {
        static const char *ProcName = "DAVariables::AddVariable";
        if (Col >= (int)VarListSize)
                {
                size_t VarListSizeNEW = VarListSize * 2 + 16;
                size_t AllocSize = VarListSizeNEW * sizeof(DAVariable*);
                char *VarListNEW = (char *)realloc(VarList,AllocSize);
                if (!VarListNEW)
                        {
                        TERROR(("%s: realloc(%u) Failed",ProcName,AllocSize));
                        return false;
                        }
                size_t CurrentSize = VarListSize * sizeof(DAVariable*);
                memset(VarListNEW + CurrentSize,0,AllocSize- CurrentSize);
                VarListSize = VarListSizeNEW;
                VarList = (DAVariable **)VarListNEW;
                }
        if (VarList[Col]) delete VarList[Col];
        VarList[Col] = Var;
        if (Col >= (int)VarCount) VarCount = Col + 1;
        return true;
        }

bool DAVariables::AddVariable(DAVariable &Src)
        {
        DAVariable *Variable = new DAVariable(Src);
        return AddVariable(Variable);
        }

bool DAVariables::AddVariable(int DataType, 
                              const char *Name, 
                              size_t Length,
                              unsigned CCSID)
        {
        DAVariable *Variable = new DAVariable(DataType,Name,Length,CCSID);
        return AddVariable(Variable);
        }

bool DAVariables::AddVariable(int Col,
                              int DataType, 
                              const char *Name, 
                              size_t Length,
                              unsigned CCSID)
        {
        DAVariable *Variable = new DAVariable(DataType,Name,Length,CCSID);
        return AddVariable(Col,Variable);
        }

void DAVariables::Clear()
        {
        while (VarCount)
                {
                --VarCount;
                if (VarList[VarCount]) 
                        {
                        delete VarList[VarCount];
                        VarList[VarCount] = NULL;
                        }
                }
        if (DA)
                {
                free(DA);
                DA = NULL;
                }
        RecordSize = 0;
        return;
        }

void DAVariables::ConstructDA()
        {
        static const char *ProcName = "DAVariables::ConstructDA";
        if (DA) return;
        ConstructNullDA(CountVars());
        for (unsigned i = 0; i < VarCount; i++)
                {
                DAVariable *Current = VarList[i];
                if (!Current)
                        {
                        TERROR(("%s: No VarList Entry: %u",ProcName,i));
                        throw ErrorClass("ConstructDA: No VarList Entry");
                        }
                SetColumn(i,Current->Type,Current->Length,Current->CCSID);
                if (Current->DataPtr || 
                    Current->IndicatorPtr || 
                    Current->LobLenPtr)
                        {
		        char *Buffer = Current->DataPtr;
		        short *Indicator = Current->IndicatorPtr;
		        unsigned *LobLen = Current->LobLenPtr;
                        SetHostVar(i,&Buffer,&Indicator,&LobLen,0);
                        }
                }
        return;
        }

void DAVariables::ConstructNullDA(unsigned nVars)
        {
        if (DA) return;
        size_t Size = sizeof(Qsq_sqlda_t) + 
                      sizeof(sqlvar) * nVars +
                      sizeof(sqlvar2) * nVars;
        DA = (Qsq_sqlda_t *)calloc(1,Size);
        if (!DA) throw MemoryError("ConstructDA");
        DA->sqldabc = Size;
        DA->sqld = nVars;
        DA->sqln = 2 * nVars;  
        memcpy(DA->sqldaid,"SQLDA 2 ",8);
        return;
        }

void DAVariables::ConvertLobs2Locators()
        {
        for (unsigned i = 0; i < VarCount; i++)
                {
                DAVariable *Current = VarList[i];
                int Type = Current->Type;
                if (Type == DA_CLOB_TYPE)
                        {
                        Current->Type = DA_CLOBLOC_TYPE;
                        Current->Length = 4;
                        }
                else if (Type == DA_BLOB_TYPE)
                        {
                        Current->Type = DA_BLOBLOC_TYPE;
                        Current->Length = 4;
                        }
                else if (Type == DA_DBCLOB_TYPE)
                        {
                        Current->Type = DA_DBCLOBLOC_TYPE;
                        Current->Length = 4;
                        }
                else if (Type == DA_XML_TYPE)
                        {
                        if (GetCCSID(i) == 13488)
                                {
                                Current->Type = DA_DBCLOBLOC_TYPE;
                                }
                        else    {
                                Current->Type = DA_CLOBLOC_TYPE;
                                }
                        Current->Length = 4;
                        }
                }
        return;
        }

unsigned DAVariables::GetCCSID(int Col)
        {
        unsigned CCSID = 0;
        if (DA)
                {
                unsigned *CCSIDPtr = (unsigned *)DA->sqlvar[Col].sqlname.data;
                if (DA->sqlvar[Col].sqlname.length == 8)
                        {
                        CCSIDPtr = (unsigned *)DA->sqlvar[Col].sqlname.data;
                        CCSID = *CCSIDPtr;
                        }
                else if (Col < (int)VarCount)
                        {
                        CCSID = VarList[Col]->CCSID;
                        }
                }
        else if (Col < (int)VarCount)
                {
                CCSID = VarList[Col]->CCSID;
                }
        return CCSID;
        }

unsigned DAVariables::GetColumnCount()
        {
        return DA ? DA->sqld : CountVars();
        }

Qsq_sqlda_t* DAVariables::GetDA()
        {
        if (!DA) ConstructDA();
        return DA;
        }

short DAVariables::GetIndicator(int Row, int Col)
        {
        short *IndicatorPtr = GetIndicatorPtr(Row,Col);
        return IndicatorPtr ? *IndicatorPtr : false;
        }

short* DAVariables::GetIndicatorPtr(int Row, int Col)
        {
        unsigned nVars = GetColumnCount();
        if (RecordSize == 0 && Row != 0)
                {
                TERROR(("DAVariables::GetIndicatorPtr: Not Blocked"));
                return NULL;
                }
        short *IndBuffer = (short *)DA->sqlvar[Col].sqlind;
        return IndBuffer ? IndBuffer + nVars * Row : NULL;
        }

size_t DAVariables::GetRecordSize()
        {
        if (RecordSize) return RecordSize;
        if (!DA) ConstructDA();
        unsigned nVars = GetColumnCount();
        for (unsigned i=0; i < nVars; i++)
                {
                RecordSize += HostVarByteSize(i);
                }
        return RecordSize;
        }

void* DAVariables::GetVariable(int Row, int Col)
        {
        if (RecordSize == 0 && Row != 0)
                {
                TERROR(("DAVariables::GetVariable: Not a Blocked Buffer"));
                return NULL;
                }
        byte_t *Buffer = (byte_t *)DA->sqlvar[Col].sqldata;
        void *Variable = Buffer + RecordSize * Row;
        return Variable;
        }

char* DAVariables::GetVariableData(int Row, int Col)
        {
        byte_t *VariableData = (byte_t *)GetVariable(Row,Col);
        if (IS_LOB(DA->sqlvar[Col].sqltype))
                {
                sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
                if (!Esqlvar[Col].sqldatalen) 
                        {
                        VariableData += sizeof(unsigned int);
                        }
                }
        else if (IS_VAR(DA->sqlvar[Col].sqltype))
                {
                VariableData += sizeof(unsigned short);
                }
        return (char *)VariableData;
        }

size_t DAVariables::GetVariableLength(int Row, int Col)
        {
        size_t Length;                          // Number of characters
        void* Variable = GetVariable(Row,Col);  // (Char,WChar) or bytes.
        if (IS_LOB(DA->sqlvar[Col].sqltype))
                {
                sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
                if (Esqlvar[Col].sqldatalen)
                        {
                        unsigned int *Array;
                        Array = (unsigned int *)Esqlvar[Col].sqldatalen;
                        size_t Size = (size_t)Array[Row];
                        if (IS_WCHAR(DA->sqlvar[Col].sqltype))
                                {
                                // sqldatalen in bytes.
                                Length =  Size / sizeof(u2char_t);
                                }
                        else Length = Size;
                        }
                else Length = (size_t)*(unsigned int *)Variable;
                }
        else if (IS_VAR(DA->sqlvar[Col].sqltype))
                {
                Length = (size_t)*(unsigned short *)Variable;
                }
        else if (IS_PACKED(DA->sqlvar[Col].sqltype))
                {
                Length = ((DA->sqlvar[Col].sqllen >> 8) + 2) / 2;
                }            
        else Length = DA->sqlvar[Col].sqllen;
        return Length;
        }

unsigned DAVariables::GetVariableDigits(int i)
        {
        unsigned Digits = 0;
        if (DA)
                {
                if (IS_PACKED(DA->sqlvar[i].sqltype))
                        {
                        Digits = DA->sqlvar[i].sqllen  >> 8;
                        }
                }
        else if (i < (int)VarCount)
                {
                Digits = VarList[i]->Digits;
                }
        return Digits;
        }

unsigned DAVariables::GetVariableScale(int i)
        {
        unsigned Scale = 0;
        if (DA)
                {
                if (IS_PACKED(DA->sqlvar[i].sqltype))
                        {
                        Scale = DA->sqlvar[i].sqllen & 0xFF;
                        }
                }
        else if (i < (int)VarCount)
                {
                Scale = VarList[i]->Scale;
                }
        return Scale;
        }

size_t DAVariables::GetVariableMaxLength(int i)
        {
        size_t MaxLength = 0;                   // Number of characters  
        if (DA)                                 // (Char,WChar) or bytes.
                {
                if (IS_LOB(DA->sqlvar[i].sqltype))
                        {
                        sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
                        MaxLength = (size_t)Esqlvar[i].len.sqllonglen;
                        }
                else if (IS_PACKED(DA->sqlvar[i].sqltype))
                        {
                        MaxLength = ((DA->sqlvar[i].sqllen >> 8) + 2) / 2;
                        } 
                else MaxLength = (size_t)DA->sqlvar[i].sqllen;
                }
        else if (i < (int)VarCount)
                {
                MaxLength = VarList[i]->Length;
                }
        return MaxLength;
        }

const char* DAVariables::GetVariableName(int i)
        {
        return i < (int)VarCount && VarList[i]->Name ? VarList[i]->Name : "";
        }

int DAVariables::GetVariableType(int i)
        {
        int Type = 0;
        if (DA)
                {
                Type = DA->sqlvar[i].sqltype;
                }
        else if (i < (int)VarCount)
                {
                Type = VarList[i]->Type;
                }
        return Type;
        }

size_t DAVariables::HostVarByteSize(int i)    // Buffer length (bytes)
        {
        size_t Length;
        sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
        if (IS_LOB(DA->sqlvar[i].sqltype)) 
                {
                Length = Esqlvar[i].len.sqllonglen;
                }
        else if (IS_PACKED(DA->sqlvar[i].sqltype))
                {
                Length = ((DA->sqlvar[i].sqllen >> 8) + 2) / 2;
                }
        else Length = DA->sqlvar[i].sqllen;
        if (IS_WCHAR(DA->sqlvar[i].sqltype)) Length *= sizeof(u2char_t);
        if (IS_LOB(DA->sqlvar[i].sqltype) && !Esqlvar[i].sqldatalen)
                {
                Length += 4;
                }
        else if (IS_VAR(DA->sqlvar[i].sqltype)) Length += 2;
        return Length;
        }

void DAVariables::SetBuffer(int Col,
                            char *Buffer,
                            short *Indicator,
                            unsigned *LobLen)
        {
        if (DA)
                {
                SetHostVar(Col,&Buffer,&Indicator,&LobLen,0);
                }
        if (Col < (int)VarCount && VarList[Col])
                {
                VarList[Col]->SetBuffer(Buffer,Indicator,LobLen);
                } 
        return;
        }

void DAVariables::SetBuffers(char *Buffer, short *Indicate)
        {
        ConstructDA();
        byte_t *Start = (byte_t *)Buffer;
        for (unsigned i = 0; i < VarCount; i++)
                {
                SetHostVar(i,&Buffer,&Indicate,NULL,0);
                }
        RecordSize = (byte_t *)Buffer - Start;
        return;
        }

void DAVariables::SetBuffers(char *Buffer, 
                             short *Indicate, 
                             unsigned *LobLens,
                             unsigned nRows)
        {
        ConstructDA();
        byte_t *Start = (byte_t *)Buffer;
        for (unsigned i = 0; i < VarCount; i++)
                {
                SetHostVar(i,&Buffer,&Indicate,&LobLens,nRows);
                }
        RecordSize = (byte_t *)Buffer - Start;
        return;
        }

void DAVariables::SetCCSID(int Col, unsigned CCSID)
        {
        if (DA)
                {
                unsigned *CCSIDPtr = (unsigned *)DA->sqlvar[Col].sqlname.data;
                if (IS_TEXT(DA->sqlvar[Col].sqltype))
                        {
                        DA->sqlvar[Col].sqlname.length = 8;
                        CCSIDPtr = (unsigned *)DA->sqlvar[Col].sqlname.data;
                        CCSIDPtr[0] = CCSID;
                        CCSIDPtr[1] = 0;
                        }
                }
        if (Col < (int)VarCount && VarList[Col])
                {
                VarList[Col]->CCSID = CCSID;
                }
        DirectMap = false;                      // Not == Described.
        return;
        }

void DAVariables::SetColumn(int i, 
                            int DataType, 
                            size_t Length,
                            unsigned CCSID)
        {
        DA->sqlvar[i].sqltype = DataType;
        DA->sqlvar[i].sqlind = NULL;
        if (IS_TEXT(DataType))
                {
                DA->sqlvar[i].sqlname.length = 8;
                unsigned *CCSIDPtr = (unsigned *)DA->sqlvar[i].sqlname.data;
                CCSIDPtr[0] = CCSID;
                CCSIDPtr[1] = 0;
                }
        else DA->sqlvar[i].sqlname.length = 0;
        if (IS_LOB(DataType))
                {
                sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
                DA->sqlvar[i].sqllen  = 0;
                Esqlvar[i].len.sqllonglen = Length;
                }
        else DA->sqlvar[i].sqllen = Length;
        return;
        }

void DAVariables::SetHostVar(int i,
                             char **Buffer, 
                             short **Indicators, 
                             unsigned **LobLens,
                             unsigned nRows)
        {
        if (!DA) return;
        sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
        DA->sqlvar[i].sqldata = (byte_t *)*Buffer;
        if (IS_LOB(DA->sqlvar[i].sqltype))
                {
                DA->sqlvar[i].sqllen = 0;
                if (LobLens && *LobLens)
                        {
                        Esqlvar[i].sqldatalen = (char *)*LobLens;
                        *LobLens += nRows;
                        }
                else Esqlvar[i].sqldatalen = NULL;
                }
        if (DA->sqlvar[i].sqltype % 2)          /* Odd types have NULL */
                {                               /* value indicators    */
                DA->sqlvar[i].sqlind = *Indicators;
                *Indicators += 1;
                }
        *Buffer += HostVarByteSize(i);
        return;
        }

void DAVariables::SetIndicator(int Row, int Col, short Indicator)
        {
        short *IndicatorPtr = GetIndicatorPtr(Row,Col);
        if (IndicatorPtr) 
                {
                *IndicatorPtr = Indicator;
                }
        return;
        }

void DAVariables::SetPackedVariable(int Row, int Col, int Value)
        {
        if (!IS_PACKED(DA->sqlvar[Col].sqltype)) 
                {
                TERROR(("DAVariables::SetVariablePacked(%d) Wrong Type",Col));
                return;
                }
        size_t nBytes = GetVariableMaxLength(Col);
        if (nBytes == 0) return;                        // Nothing to set.
        unsigned nDigits = GetVariableDigits(Col);
        if (nDigits > MAX_PACKED_DIGITS)
                {
                TERROR(("DAVariables::SetVariablePacked: Field to Long"));
                return;
                }
        byte_t WorkSpace[MAX_PACKED_DIGITS+1];
        byte_t *Pos = WorkSpace + nDigits;
        if (Value < 0)
                {
                *Pos = 0x0D;                    // Minus Sign
                Value *= -1;
                }
        else *Pos = 0x0C;
        while (Pos != WorkSpace)
                {
                --Pos;
                unsigned Digit = Value % 10;
                *Pos = (byte_t)Digit;
                Value /= 10;
                }
        byte_t Packed[(MAX_PACKED_DIGITS+2)/2];
        for (unsigned i=0; i < nBytes; i++)
                {
                unsigned Nibble1 = WorkSpace[2*i];
                unsigned Nibble0 = WorkSpace[2*i+1];
                unsigned Byte = (Nibble1 << 4) + Nibble0;
                Packed[i] = (byte_t)Byte;
                }
        SetVariable(Row,Col,Packed,nBytes);
        return;
        }

void DAVariables::SetVariable(int Row, 
                              int Col, 
                              void *Input, 
                              size_t Length,
                              unsigned SourceCCSID)
        {
        size_t Size;                                       // Size is bytes.
        size_t BufferSize;
        unsigned short Blank = 0;
        unsigned DestCCSID = GetCCSID(Col);
        if (DestCCSID == EBCDIC_CCSID) Blank = 0x40;
        else if (DestCCSID == 819) Blank = 0x20;
        else if (DestCCSID == 1208) Blank = 0x20;
        else if (DestCCSID == 13488) Blank = 0x20;
        unsigned SQLType = DA->sqlvar[Col].sqltype;
        if (!Input)
                {
                SetIndicator(Row,Col,-1);       // Cell is NULL.
                return;
                }
        // *************************
        // **** Compute Lengths ****
        // *************************
        size_t BufferLength = GetVariableMaxLength(Col);   // Length is chars.
        if (Length > BufferLength) 
                {
                TERROR(("DAVariables::SetVariable: Input Truncated"));
                Length = BufferLength;
                }
        if (IS_WCHAR(SQLType))
                {
                BufferSize = BufferLength / sizeof(u2char_t);
                }
        else BufferSize = BufferLength;
        if (IS_WCHAR(SQLType))
                {
                Size = Length * sizeof(u2char_t);
                }
        else Size = Length;
        // ***********************************
        // **** Set variable input length ****
        // ***********************************
        void* Dest = GetVariable(Row,Col);
        sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
        if (IS_LOB(SQLType))
                {
                if (Esqlvar[Col].sqldatalen)
                        {
                        unsigned int *Array;
                        Array = (unsigned int *)Esqlvar[Col].sqldatalen;
                        Array[Row] = Size;
                        }
                else    {
                        *(unsigned int *)Dest = Length;
                        Dest = (byte_t *)Dest + sizeof(unsigned int);
                        }
                BufferSize = Size;
                BufferLength = Length;
                }
        else if (IS_VAR(SQLType))
                {
                *(unsigned short *)Dest = Length;
                Dest = (byte_t *)Dest + sizeof(unsigned short);
                BufferSize = Size;
                BufferLength = Length;
                }
        // *******************
        // **** Copy data ****
        // *******************
        if (IS_CHAR(SQLType)) 
                {
                if (DestCCSID==EBCDIC_CCSID && SourceCCSID==ASCII_CCSID)
                        {
                        amem2emem(Dest,Input,Size);
                        }
                else if (DestCCSID==UCS2_CCSID && SourceCCSID==ASCII_CCSID)
                        {
                        a2ucpy((u2char_t *)Dest,(const char *)Input,Length);
                        }
                else memcpy(Dest,Input,Size);
                }
        else memcpy(Dest,Input,Size);
        // ************************************
        // **** Pad the rest of the column ****
        // ************************************
        if (!IS_VAR(SQLType) && !IS_LOB(SQLType))
                {
                if (IS_CHAR(SQLType))
                        {
                        char *String = (char *)((byte_t *)Dest + Length);
                        size_t nChars = BufferLength - Length;
                        if (nChars) memset(String,(char)Blank,nChars);
                        }
                else if (IS_WCHAR(SQLType))
                        {
                        u2char_t *String = (u2char_t*)((byte_t *)Dest + Size);
                        size_t nCharacters = BufferLength - Length;
                        for (size_t i=0; i < nCharacters; i++) 
                                {
                                String[i] = (u2char_t)Blank;
                                }
                        }
                else    {
                        size_t nBytes = BufferSize - Size;
                        if (nBytes) memset((byte_t *)Dest + Size,0,nBytes);
                        }
                }
        SetIndicator(Row,Col,0);        // This cell is not NULL.
        return;
        }

void DAVariables::SetVariable(int Row, int Col, int Integer)
        {
        SetVariable(Row,Col,&Integer,sizeof(int));
        return;
        }

void DAVariables::SetVariable(int Row, int Col, unsigned Integer)
        {
        SetVariable(Row,Col,&Integer,sizeof(unsigned));
        return;
        }

void DAVariables::SetVariable(int Row, int Col, double Double)
        {
        SetVariable(Row,Col,&Double,sizeof(double));
        return;
        }

void DAVariables::SetVariable(int Row, int Col, const char *String)
        {
        if (!String) SetVariable(Row,Col,NULL,0);
        else SetVariable(Row,Col,(void *)String,strlen(String),LOCAL_CCSID);
        return;
        }

void DAVariables::SetVariable(int Row, int Col, const u2char_t *String)
        {
        if (!String) SetVariable(Row,Col,NULL,0);
        else SetVariable(Row,Col,(void *)String,ustrlen(String));
        return;
        }

#if !U2CHAR_IS_WCHAR

void DAVariables::SetVariable(int Row, int Col, const wchar_t *String)
        {
        if (!String) SetVariable(Row,Col,NULL,0);
        else    {
                size_t Length = wcslen(String);
                size_t BufferSize = (Length + 1) * sizeof(u2char_t);
                u2char_t *DoubleBuffer = (u2char_t *)malloc(BufferSize);
                if (!DoubleBuffer)
                        {
                        TERROR(("DAVariables::SetVariable: Alloc Failed"));
                        return;
                        }
                u2char_t *Pos = DoubleBuffer;
                for (size_t i=0; i < Length; i++) 
                        {
                        *Pos++ = (u2char_t)*String++;
                        }
                DoubleBuffer[Length] = 0;
                SetVariable(Row,Col,(void *)DoubleBuffer,Length);
                free(DoubleBuffer);
                }
        return;
        }

#endif /* !U2CHAR_IS_WCHAR */

void DAVariables::SetVariableLength(int Row, int Col, size_t Length)
        {
        void* Variable = GetVariable(Row,Col);  // Number of characters
        if (IS_LOB(DA->sqlvar[Col].sqltype))    // (Char,WChar) or bytes.
                {
                sqlvar2 *Esqlvar = (sqlvar2 *)&(DA->sqlvar[DA->sqld]);
                if (Esqlvar[Col].sqldatalen)
                        {
                        size_t Size;
                        unsigned int *Array;
                        Array = (unsigned int *)Esqlvar[Col].sqldatalen;
                        if (IS_WCHAR(DA->sqlvar[Col].sqltype))
                                {
                                // sqldatalen in bytes.
                                Size = Length * sizeof(u2char_t);
                                }
                        else Size = Length;
                        Array[Row] = (unsigned int)Size;
                        }
                else *(unsigned int *)Variable = (unsigned int)Length;
                }
        else if (IS_VAR(DA->sqlvar[Col].sqltype))
                {
                *(unsigned short *)Variable = (unsigned short)Length;
                }
        else DA->sqlvar[Col].sqllen = (unsigned short)Length;
        return;
        }

DAVariables& DAVariables::operator =(DAVariables &Src)
        {
        static const char *ProcName = "DAVariables::operator=";
        if (this == &Src) return *this;
        Clear();
        for (unsigned i = 0; i < Src.VarCount; i++)
                {
                if (!Src.VarList[i])
                        {
                        TERROR(("%s: No VarList Entry: %u",ProcName,i));
                        throw ErrorClass("DAVariables::operator=()");
                        }
                AddVariable(*Src.VarList[i]);
                }
        return *this;
        }

/* ************************************************************************ */
/* **** PRCED_Statement *************************************************** */
/* ************************************************************************ */

SQLCtrlBlockCache PRCED_Statement::CBCache;

PRCED_Statement::PRCED_Statement(const char *StatementName,
                                 const char *PackageLib, const char *Package,
                                 const char *SortSeqLib, const char *SortSeq)
        {
        CB = CBCache.GetCB();
        if (!CB) throw ErrorClass("PRCED_Statement::PRCED_Statement");
        CB->SetStatementName(StatementName ? StatementName : "");
        CB->SetPackage(PackageLib,Package);
        CB->SetSortSeq(SortSeqLib ? SortSeqLib : "",
                       SortSeq ? SortSeq : "*JOB");
        return;        
        }

PRCED_Statement::~PRCED_Statement()
        {
        CBCache.ReturnCB(CB);
        return;
        }

bool PRCED_Statement::CheckStatementID()
        {
        ASError Error("CheckStatementID");
        DAVariables DummyDA;
        CB->SetFunction('A');                    // Check Statement
        QSQPRCED(GetClearCA(),
                 DummyDA.GetDA(),
                 (char *)CB->GetFormat(),
                 CB->GetQsq(),
                 Error.GetErrorCode());
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode == 0;
        }

int PRCED_Statement::CreatePackage()
        {
        ASError Error("CreatePackage");
        DAVariables DummyDA;
        CB->SetFunction('1');                    // Build a new package 
        QSQPRCED(GetClearCA(),
                 DummyDA.GetDA(),
                 (char *)CB->GetFormat(),
                 CB->GetQsq(),
                 Error.GetErrorCode());
        if (sqlca.sqlcode == -601)
                {
                TINFO(("Package already exists"));
                }
        else if (sqlca.sqlcode)
                {
                TRACE_SQLCODE("Create Package",sqlca);
                }
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

int PRCED_Statement::DeletePackage()
        {
        ASError Error("DeletePackage");
        DAVariables DummyDA;
        CB->SetFunction('C');                    // Delete a package 
        QSQPRCED(GetClearCA(),
                 DummyDA.GetDA(),
                 (char *)CB->GetFormat(),
                 CB->GetQsq(),
                 Error.GetErrorCode());
        if (sqlca.sqlcode)
                {
                TRACE_SQLCODE("Delete Package",sqlca);
                }
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

int PRCED_Statement::Describe(DAVariables &DescribeVars)
        {
        ASError Error("Describe");
        DAVariables TempDA;
        TempDA.ConstructNullDA(MAX_DESCRIBE_COLUMNS);
        Qsq_sqlda_t *DA = TempDA.GetDA();
        CB->SetFunction('7');                            // Describe
        QSQPRCED(GetClearCA(),
                 DA,
                 (char *)CB->GetFormat(),
                 CB->GetQsq(),
                 Error.GetErrorCode());
        TRACE_SQLCODE("Describe",sqlca);
        if (Error.IsError()) throw Error;
        if (sqlca.sqlcode == 0) LoadVars(DescribeVars,DA);
        return sqlca.sqlcode;
        }

int PRCED_Statement::Execute(DAVariables &Input, int nInsertRows)
        {
        ASError Error("Execute");
        Qsq_sqlda_t *DA = Input.GetDA();
        CB->SetFunction('3');                   // Execute:
        CB->SetRowsToInsert(nInsertRows);
        QSQPRCED(GetClearCA(),                  // Insert: Vars = Input Block
                 DA,                            // Update: Vars = Set + Where
                 (char *)CB->GetFormat(),
                 CB->GetQsq(),
                 Error.GetErrorCode());
        TRACE_SQLCODE("Execute",sqlca);
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

bool PRCED_Statement::FindStatementName(const char *Text)
        {
        return FindStatementName(Text,strlen(Text)); 
        }

bool PRCED_Statement::FindStatementName(const char *Text, size_t TextLength)
        {
        static const char *ProcName = "PRCED_Statement::FindStatementName";
        ASError Error("FindStatementName");           // Native QSQPRCED only;
        if (GetPRCEDvia() != PRCEDpp_VIA_QSQPRCED)    // XDN/XDA is automatic.
                {
                TERROR(("%s: Only supported by native QSQPRCED",ProcName));
                return false;
                }
        DAVariables DummyDA;                    
        CB->SetStatementName("");
        CB->SetStatementText(Text);
        CB->SetFunction('A');                   // Check Statement
        QSQPRCED(GetClearCA(),
                 DummyDA.GetDA(),
                 (char *)CB->GetFormat(),
                 CB->GetQsq(),
                 Error.GetErrorCode());
        if (Error.IsError()) throw Error;
        if (sqlca.sqlcode == 0)
                {
                char *FoundID = (char *)sqlca.sqlerrmc;
                size_t FoundLen = sizeof(sqlca.sqlerrmc);
                CB->SetStatementName(FoundID,FoundLen);
                }
        return sqlca.sqlcode == 0;
        }

Qsq_sqlca_t* PRCED_Statement::GetClearCA()
        {
        memset(&sqlca,0,sizeof(sqlca));
        sqlca.sqlcabc = sizeof(sqlca);
        return &sqlca;
        }

unsigned PRCED_Statement::GetStatementCCSID()
        {
        return CB->GetStatementCCSID();
        }

size_t PRCED_Statement::GetStatementLength()
        {
        return CB->GetStatementLength();
        }

const char* PRCED_Statement::GetStatementName(size_t *NameLength)
        {
        return CB->GetStatementName(NameLength);
        }

const char* PRCED_Statement::GetStatementText()
        {
        return CB->GetStatementText();
        }

bool PRCED_Statement::LoadVars(DAVariables &DestVars, const Qsq_sqlda_t *DA)
        {
        DestVars.Clear();
        unsigned nVars = DA->sqld;
        bool DirectMap = AUTO_DIRECT_MAP;
        for (unsigned i=0; i < nVars; i++)
                {
                size_t Length;
                int DataType = DA->sqlvar[i].sqltype | 0x01;
                if (DataType == 457) DataType = DA_VARCHAR_TYPE;
                else if (DataType == 473) DataType = DA_VARWCHAR_TYPE;
                if (IS_LOB(DataType))
                        {
                        DirectMap = false;
                        sqlvar2 *Esqlvar = (sqlvar2 *)
                                           &(DA->sqlvar[DA->sqld]);
                        Length = (size_t)Esqlvar[i].len.sqllonglen;
                        }
                else Length = DA->sqlvar[i].sqllen;
                char Name[sizeof(DA->sqlvar[i].sqlname.data) + 1];
                size_t NameLen = sizeof(DA->sqlvar[i].sqlname.data);
                memcpy(Name,
                       (char *)DA->sqlvar[i].sqlname.data,
                       sizeof(DA->sqlvar[i].sqlname.data));
                Name[sizeof(DA->sqlvar[i].sqlname.data)] = '\0';
                rtrim(Name);
                unsigned *CCSID = (unsigned *)&DA->sqlvar[i].sqldata;
                DestVars.AddVariable(DataType,Name,Length,*CCSID);
                }
        DestVars.DirectMap = DirectMap;
        return true;
        }

int PRCED_Statement::Prepare(const char *StatementText, size_t Length)
        {
        ASError Error("Prepare");
        static DAVariables NULLDA;
        Qsq_sqlda_t *DA = NULLDA.GetDA();
        CB->SetFunction('2');                            // Prepare
        CB->SetStatementText(StatementText,Length);
        Qxda_EXDO0100_t exdo = {0};
        int exdo_size = sizeof(Qxda_EXDO0100_t);
        int extdynopts = QXDA_EXTDYN_NOOPTS | QXDA_CREATE_OBJECTS;
        if (!CB->HasStatementName()) extdynopts |= QXDA_FIND_STMT;
        QSQPRCEDEx(GetClearCA(),
                   DA,
                   (char *)CB->GetFormat(),
                   CB->GetQsq(),
                   &exdo,
                   &exdo_size,
                   &extdynopts,
                   Error.GetErrorCode());
        if (!Error.IsError() &&
            (extdynopts & QXDA_FIND_STMT) &&            // Aquire generated
            exdo.Bytes_Returned >= ExDOHeaderLen)       // statement ID.
                {
                size_t NameLength = exdo.Bytes_Returned - ExDOHeaderLen;
                CB->SetStatementName(exdo.Statement_Name,NameLength);
                }
        TRACE_SQLCODE("Prepare",sqlca);
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

int PRCED_Statement::Prepare(const char *StatementText)
        {
        size_t StatementLength = strlen(StatementText);
        return Prepare(StatementText,StatementLength);
        }

int PRCED_Statement::PrepareDescribe(const char *StatementText,
                                     size_t StatementLength,
                                     DAVariables &DescribeVars)
        {
        ASError Error("Prepare-Describe");
        DAVariables TempDA;
        TempDA.ConstructNullDA(MAX_DESCRIBE_COLUMNS);
        Qsq_sqlda_t *DA = TempDA.GetDA();
        CB->SetFunction('9');                    // Prepare-Describe
        CB->SetStatementText(StatementText,StatementLength);
        Qxda_EXDO0100_t exdo = {0};
        int exdo_size = sizeof(Qxda_EXDO0100_t);
        int extdynopts = QXDA_EXTDYN_NOOPTS | QXDA_CREATE_OBJECTS;
        if (!CB->HasStatementName()) extdynopts |= QXDA_FIND_STMT;
        QSQPRCEDEx(GetClearCA(),
                   DA,
                   (char *)CB->GetFormat(),
                   CB->GetQsq(),
                   &exdo,
                   &exdo_size,
                   &extdynopts,
                   Error.GetErrorCode());
        if (!Error.IsError() &&
            (extdynopts & QXDA_FIND_STMT) &&            // Aquire generated
            exdo.Bytes_Returned >= ExDOHeaderLen)       // statement ID.
                {
                size_t NameLength = exdo.Bytes_Returned - ExDOHeaderLen;
                CB->SetStatementName(exdo.Statement_Name,NameLength);
                }
        TRACE_SQLCODE("Prepare-Describe",sqlca);
        if (Error.IsError()) throw Error;
        if (sqlca.sqlcode == 0) LoadVars(DescribeVars,DA);
        return sqlca.sqlcode;
        }

int PRCED_Statement::PrepareDescribe(const char *StatementText,
                                     DAVariables &DescribeVars)
        {
        size_t StatementLength = strlen(StatementText);
        return PrepareDescribe(StatementText,StatementLength,DescribeVars);
        }

/* ************************************************************************ */
/* **** PRCED_Cursor ****************************************************** */
/* ************************************************************************ */

PRCED_Cursor::PRCED_Cursor(PRCED_Statement &S, char *CursorName) 
        : 
        Statement(S)
        {
        memset(&sqlca,0,sizeof(sqlca));
        sqlca.sqlcabc = sizeof(sqlca);
        SetCursorName(CursorName);
        AllRowsRead = false;
        BlockingFactor = 0;
        DeferOpen = false;
        return;
        }

int PRCED_Cursor::Close(bool HardClose)
        {
        ASError Error("Close");
        DAVariables DummyVars;
        SQLCtrlBlock &CB = Statement.GetSQLCtrlBlock();
        CB.SetFunction(HardClose ? '8' : '6');
        QSQPRCED(GetClearCA(),
                 DummyVars.GetDA(),
                 (char *)CB.GetFormat(),
                 CB.GetQsq(),
                 Error.GetErrorCode());
        TRACE_SQLCODE("Close",sqlca);
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

const char* PRCED_Cursor::GetCursorName()
        {
        SQLCtrlBlock &CB = Statement.GetSQLCtrlBlock();
        return CB.GetCursor();
        }

int PRCED_Cursor::GetRowsProcessed()    // Valid after sqlcode == 0.
        {
        return (int)sqlca.sqlerrd[2];
        }

Qsq_sqlca_t* PRCED_Cursor::GetClearCA()
        {
        memset(&sqlca,0,sizeof(sqlca));
        sqlca.sqlcabc = sizeof(sqlca);
        return &sqlca;
        }

int PRCED_Cursor::Fetch(DAVariables &OutputVars)
        {
        ASError Error("Fetch");
        Qsq_sqlda_t *DA = OutputVars.GetDA();
        SQLCtrlBlock &CB = Statement.GetSQLCtrlBlock();
        CB.SetFunction('5');                            // Fetch
        CB.SetBlockingFactor(BlockingFactor);
        CB.SetDirectMap(OutputVars.DirectMap);
        QSQPRCED(GetClearCA(),
                 DA,
                 (char *)CB.GetFormat(),
                 CB.GetQsq(),
                 Error.GetErrorCode());
        TRACE_SQLCODE("Fetch",sqlca);
        if (Error.IsError()) throw Error;
        if (sqlca.sqlcode == 0 && BlockingFactor)
                {
                AllRowsRead = sqlca.sqlerrd[4] == 100;
                }
        else if (sqlca.sqlcode == 100)
                {
                AllRowsRead = true; 
                }
        return sqlca.sqlcode;
        }

int PRCED_Cursor::Open(DAVariables &InputVars, int nBlockedRows, bool Reopen)
        {
        ASError Error("Open");
        AllRowsRead = false;
        BlockingFactor = nBlockedRows;
        Qsq_sqlda_t *DA = InputVars.GetDA();
        SQLCtrlBlock &CB = Statement.GetSQLCtrlBlock();
        CB.SetFunction('4');                            // Open
        CB.SetBlockingFactor(BlockingFactor);
        CB.SetReopen(Reopen);
  #ifdef __QSQPRCED_VIA_XDA
        QSQPRCED_XDADeferOpen = DeferOpen;
  #endif
        QSQPRCED(GetClearCA(),
                 DA,
                 (char *)CB.GetFormat(),
                 CB.GetQsq(),
                 Error.GetErrorCode());
        TRACE_SQLCODE("Open",sqlca);
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

int PRCED_Cursor::Open(int nBlockedRows, bool Reopen)
        {
        ASError Error("Open");
        static DAVariables NULLDA;
        AllRowsRead = false;
        BlockingFactor = nBlockedRows;
        Qsq_sqlda_t *DA = NULLDA.GetDA();
        SQLCtrlBlock &CB = Statement.GetSQLCtrlBlock();
        CB.SetFunction('4');                            // Open
        CB.SetBlockingFactor(BlockingFactor);
        CB.SetReopen(Reopen);
        QSQPRCED(GetClearCA(),
                 DA,
                 (char *)CB.GetFormat(),
                 CB.GetQsq(),
                 Error.GetErrorCode());
        TRACE_SQLCODE("Open",sqlca);
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

void PRCED_Cursor::SetCursorName(char *CursorName)
        {
        SQLCtrlBlock &CB = Statement.GetSQLCtrlBlock();
        CB.SetCursor(CursorName);
        return;
        }

/* ************************************************************************ */
/* **** PRCED_Locator ***************************************************** */
/* ************************************************************************ */

/* ************************************** */
/* **** PRCED_Locator SQL Statements **** */
/* ************************************** */

        // Note: CLOBS are CCSID 500 on DB.

static char *LengthSTMT_BLOB = "VALUES LENGTH(CAST(? AS BLOB(1G))) INTO ?";
static char *LengthSTMT_CLOB = "VALUES LENGTH(CAST(? AS CLOB(1G))) INTO ?";
static char *LengthSTMT_DBCLOB = "VALUES LENGTH(CAST(? AS DBCLOB(512M) "
                                                         "CCSID 13488)) "
                                        "INTO ?";

static char *ValuesSTMT_BLOB = "VALUES SUBSTRING(CAST(? AS BLOB(%u)), "
                                                "CAST(? AS INT), "
                                                "CAST(? AS INT)) INTO ?";
static char *ValuesSTMT_CLOB = "VALUES SUBSTRING(CAST(? AS CLOB(%u)), "
                                                "CAST(? AS INT), "
                                                "CAST(? AS INT)) INTO ?";
static char *ValuesSTMT_DBCLOB = "VALUES SUBSTRING(CAST(? AS DBCLOB(%u) "
                                                            "CCSID 13488), "
                                                  "CAST(? AS INT), "
                                                  "CAST(? AS INT)) INTO ?";

/*
Not clear whether these have the same performace as the above.

static char *ValuesSTMT_BLOB = "VALUES SUBSTR(CAST(? AS BLOB), "
                                             "CAST(? AS INT), "
                                             "CAST(? AS INT)) INTO ?";
static char *ValuesSTMT_CLOB = "VALUES SUBSTR(CAST(? AS CLOB), "
                                             "CAST(? AS INT), "
                                             "CAST(? AS INT)) INTO ?";
static char *ValuesSTMT_DBCLOB = "VALUES SUBSTR(CAST(? AS DBCLOB "
                                                         "CCSID 13488), "
                                               "CAST(? AS INT), "
                                               "CAST(? AS INT)) INTO ?";
*/

static char *AppendSTMT_BLOB = "VALUES CONCAT(CAST(? AS BLOB), "
                                             "CAST(? AS BLOB)) INTO ?";
static char *AppendSTMT_CLOB = "VALUES CONCAT(CAST(? AS CLOB), "
                                             "CAST(? AS CLOB)) INTO ?";
static char *AppendSTMT_DBCLOB = "VALUES CONCAT(CAST(? AS DBCLOB "
                                                        "CCSID 13488), "
                                               "CAST(? AS DBCLOB "
                                                        "CCSID 13488)) INTO ?";

static char *AssignSTMT_BLOB = "VALUES CAST(? AS BLOB) INTO ?";
static char *AssignSTMT_CLOB = "VALUES CAST(? AS CLOB) INTO ?";
static char *AssignSTMT_DBCLOB = "VALUES CAST(? AS DBCLOB CCSID 13488) INTO ?";

static char *UpdateLobSTMT_BLOB = "UPDATE %s SET %s = CAST(? AS BLOB) "
                                            "WHERE CURRENT OF %.18s";
static char *UpdateLobSTMT_CLOB = "UPDATE %s SET %s = CAST(? AS CLOB) "
                                            "WHERE CURRENT OF %.18s";
static char *UpdateLobSTMT_DBCLOB = "UPDATE %s SET %s = CAST(? AS DBCLOB "
                                                            "CCSID 13488) "
                                              "WHERE CURRENT OF %.18s";

// **************************************************************************
// **** PRCED_Locator *******************************************************
// **************************************************************************

static const char *LobPackageLibDefault = LOBPACKAGELIB_DEFAULT;
static const char *LobPackageDefault = LOBPACKAGELIB_DEFAULT;

void SetLobPackageDefault(const char *Library, const char *Package)
        {
        LobPackageLibDefault = Library;
        LobPackageDefault = Package;
        return;
        }

// ***********************
// **** PRCED_Locator ****
// ***********************

PRCED_Locator::PRCED_Locator(const char *Library,
                             const char *Package,
                             int LocType,
                             unsigned CCSID)
        {
        AutoFree = false;
        LobPackageLib = Library;
        LobPackage = Package;
        LobLocType = LocType;
        Locator = 0;
        LobSize = 0;
        if (CCSID) LobCCSID = CCSID;
        else if (LocType == DA_BLOBLOC_TYPE) LobCCSID = 0;
        else if (LocType == DA_CLOBLOC_TYPE) LobCCSID = EBCDIC_CCSID;
        else if (LocType == DA_DBCLOBLOC_TYPE) LobCCSID = UCS2_CCSID;
        return;
        }

PRCED_Locator::PRCED_Locator(int LocType, unsigned CCSID)
        {
        AutoFree = false;
        LobPackageLib = LobPackageLibDefault;
        LobPackage = LobPackageDefault;
        LobLocType = LocType;
        Locator = 0;
        LobSize = 0;
        if (CCSID) LobCCSID = CCSID;
        else if (LocType == DA_BLOBLOC_TYPE) LobCCSID = 0;
        else if (LocType == DA_CLOBLOC_TYPE) LobCCSID = EBCDIC_CCSID;
        else if (LocType == DA_DBCLOBLOC_TYPE) LobCCSID = UCS2_CCSID;
        return;
        }

PRCED_Locator::PRCED_Locator(const char *Library,
                             const char *Package,
                             LobLocator_t Loc, 
                             int LocType,
                             unsigned CCSID)
        {
        AutoFree = false;
        LobPackageLib = Library;
        LobPackage = Package;
        LobLocType = LocType;
        Locator = Loc;
        LobSize = 0;
        if (CCSID) LobCCSID = CCSID;
        else if (LocType == DA_BLOBLOC_TYPE) LobCCSID = 0;
        else if (LocType == DA_CLOBLOC_TYPE) LobCCSID = EBCDIC_CCSID;
        else if (LocType == DA_DBCLOBLOC_TYPE) LobCCSID = UCS2_CCSID;
        return;
        }

PRCED_Locator::PRCED_Locator(LobLocator_t Loc, int LocType, unsigned CCSID)
        {
        AutoFree = false;
        LobPackageLib = LobPackageLibDefault;
        LobPackage = LobPackageDefault;
        LobLocType = LocType;
        Locator = Loc;
        LobSize = 0;
        if (CCSID) LobCCSID = CCSID;
        else if (LocType == DA_BLOBLOC_TYPE) LobCCSID = 0;
        else if (LocType == DA_CLOBLOC_TYPE) LobCCSID = EBCDIC_CCSID;
        else if (LocType == DA_DBCLOBLOC_TYPE) LobCCSID = UCS2_CCSID;
        return;
        }

PRCED_Locator::PRCED_Locator(const char *Library,
                             const char *Package,
                             DAVariables &Variables, 
                             int Row, 
                             int Col)
        {
        AutoFree = false;
        LobPackageLib = Library;
        LobPackage = Package;
        InitFromDAVars(Variables,Row,Col);
        LobSize = 0;
        return;
        }

PRCED_Locator::PRCED_Locator(DAVariables &Variables, int Row, int Col)
        {
        AutoFree = false;
        LobPackageLib = LobPackageLibDefault;
        LobPackage = LobPackageDefault;
        InitFromDAVars(Variables,Row,Col);
        LobSize = 0;
        return;
        }

PRCED_Locator::~PRCED_Locator()
        {
        if (Locator && AutoFree) PRCED_Locator::FreeLocator();
        }

bool PRCED_Locator::_Append(const byte_t *Buffer, size_t nUnits)
        {
        if (!nUnits) return false;
        try     {
                const char *AppendSTMT;
                const char *AppendSTMTID;
                switch (LobLocType)
                        {
                        case DA_CLOBLOC_TYPE: 
                                AppendSTMT = AppendSTMT_CLOB;
                                AppendSTMTID = "CLOBAPPEND";
                                break;
                        case DA_BLOBLOC_TYPE: 
                                AppendSTMT = AppendSTMT_BLOB;
                                AppendSTMTID = "BLOBAPPEND";
                                break;
                        case DA_DBCLOBLOC_TYPE: 
                                AppendSTMT = AppendSTMT_DBCLOB;
                                AppendSTMTID = "DBCLOBAPPEND";
                                break;
                        default: assert(false);
                        }
                PRCED_Statement Statement(AppendSTMTID,
                                          LobPackageLib,LobPackage);
                DAVariables DAVars;
                LobLocator_t NewLocator = 0;
                unsigned nUnitsInt = (unsigned)nUnits;
                short int Indicators[3] = {0};
                unsigned LobLen = 0;
                size_t CurrentSize = GetSize();

                DAVars.AddVariable(LobLocType,"LOCATOR",0);
                DAVars.AddVariable(GetLobType(),"DATA",nUnits,LobCCSID);
                DAVars.AddVariable(LobLocType,"LOCATOR",0);
                DAVars.SetBuffer(0,(char *)&Locator,&Indicators[0]);
                DAVars.SetBuffer(1,(char *)Buffer,&Indicators[1],&LobLen);
                DAVars.SetBuffer(2,(char *)&NewLocator,&Indicators[2]);
                DAVars.SetVariableLength(0,1,nUnits);

                int rc = Statement.Execute(DAVars,0);
                if (rc)
                        {
                        rc = Statement.Prepare(AppendSTMT);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::_Append: "
                                        "Can't Prepare (%d)",rc));
                                return false;
                                }
                        rc = Statement.Execute(DAVars,0);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::_Append: "
                                        "Can't Execute (%d)",rc));
                                return false;
                                }
                        }
                if (AutoFree) FreeLocator();
                Locator = NewLocator;
                LobSize = CurrentSize + Length2Size(LobLen); 
                AutoFree = true;
                }
        catch (ASError &Error)
                {
                TERROR(("PRCED_Locator::_Append: %s: %.7s",
                        Error.GetText(),
                        Error.GetExceptionID()));
                throw;
                }
        catch (ErrorClass &Error)
                {
                TERROR(("PRCED_Locator::_Append: %s",
                        Error.GetText()));
                throw;
                }
        return true;
        }

bool PRCED_Locator::Append(const char *Buffer, size_t nChars)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nChars); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Append((byte_t *)DoubleBuffer,nChars);
        free(DoubleBuffer);
        return rc;
        }

bool PRCED_Locator::Append(const byte_t *Buffer, size_t nBytes)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nBytes); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Append((byte_t *)DoubleBuffer,nBytes);
        free(DoubleBuffer);
        return rc;
        }

bool PRCED_Locator::Append(const u2char_t *Buffer, size_t nChars)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nChars); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Append((byte_t *)DoubleBuffer,nChars);
        free(DoubleBuffer);
        return rc;
        }

#if !U2CHAR_IS_WCHAR

bool PRCED_Locator::Append(const wchar_t *Buffer, size_t nChars)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nChars); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Append((byte_t *)DoubleBuffer,nChars);
        free(DoubleBuffer);
        return rc;
        }

#endif /* !U2CHAR_IS_WCHAR */

bool PRCED_Locator::_Assign(const byte_t *Buffer, size_t nUnits)
        {
        if (AutoFree) FreeLocator();
        if (!nUnits) return false;
        try     {
                const char *AssignSTMT;
                const char *AssignSTMTID;
                switch (LobLocType)
                        {
                        case DA_CLOBLOC_TYPE: 
                                AssignSTMT = AssignSTMT_CLOB;
                                AssignSTMTID = "CLOBASSIGN";
                                break;
                        case DA_BLOBLOC_TYPE: 
                                AssignSTMT = AssignSTMT_BLOB;
                                AssignSTMTID = "BLOBASSIGN";
                                break;
                        case DA_DBCLOBLOC_TYPE: 
                                AssignSTMT = AssignSTMT_DBCLOB;
                                AssignSTMTID = "DBCLOBASSIGN";
                                break;
                        default: assert(false);
                        }
                PRCED_Statement Statement(AssignSTMTID,
                                          LobPackageLib,LobPackage);
                DAVariables DAVars;
                unsigned nUnitsInt = (unsigned)nUnits;
                short int Indicators[2] = {0};
                unsigned LobLen = 0;
                size_t CurrentSize = 0;

                DAVars.AddVariable(GetLobType(),"DATA",nUnits,LobCCSID);
                DAVars.AddVariable(LobLocType,"LOCATOR",0);
                DAVars.SetBuffer(0,(char *)Buffer,&Indicators[0],&LobLen);
                DAVars.SetBuffer(1,(char *)&Locator,&Indicators[1]);
                DAVars.SetVariableLength(0,0,nUnits);

                int rc = Statement.Execute(DAVars,0);
                if (rc)
                        {
                        rc = Statement.Prepare(AssignSTMT);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::_Assign: "
                                        "Can't Prepare (%d)",rc));
                                return false;
                                }
                        rc = Statement.Execute(DAVars,0);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::_Assign: "
                                        "Can't Execute (%d)",rc));
                                return false;
                                }
                        }
                LobSize = Length2Size(LobLen);
                AutoFree = true;
                }
        catch (ASError &Error)
                {
                TERROR(("PRCED_Locator::_Assign: %s: %.7s",
                        Error.GetText(),
                        Error.GetExceptionID()));
                throw;
                }
        catch (ErrorClass &Error)
                {
                TERROR(("PRCED_Locator::_Assign: %s",
                        Error.GetText()));
                throw;
                }
        return true;
        }

bool PRCED_Locator::Assign(const u2char_t *Buffer, size_t nChars)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nChars); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Assign((byte_t *)DoubleBuffer,nChars);
        free(DoubleBuffer);
        return rc;
        }

bool PRCED_Locator::Assign(const char *Buffer, size_t nChars)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nChars); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Assign((byte_t *)DoubleBuffer,nChars);
        free(DoubleBuffer);
        return rc;
        }

bool PRCED_Locator::Assign(const byte_t *Buffer, size_t nBytes)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nBytes); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Assign((byte_t *)DoubleBuffer,nBytes);
        free(DoubleBuffer);
        return rc;
        }

#if !U2CHAR_IS_WCHAR

bool PRCED_Locator::Assign(const wchar_t *Buffer, size_t nChars)
        {
        size_t Size = 0;                                      // DoubleBuffer
        void *DoubleBuffer = BuildInputBuffer(Buffer,nChars); // because XDA 
        if (!DoubleBuffer) return false;                      // writes back.
        bool rc = _Assign((byte_t *)DoubleBuffer,nChars);
        free(DoubleBuffer);
        return rc;
        }

#endif /* !U2CHAR_IS_WCHAR */

void* PRCED_Locator::BuildInputBuffer(const char *Buffer, size_t nChars)
        {
        size_t Size = 0;
        switch (GetLobLocType())
                {
                case DA_CLOBLOC_TYPE: Size = nChars; break;
                case DA_BLOBLOC_TYPE: assert(false); break;
                case DA_DBCLOBLOC_TYPE: Size = nChars*sizeof(u2char_t); 
                                        break;
                }
        void *InputBuffer = malloc(Size);
        if (!InputBuffer) return NULL;
        if (LobCCSID == ASCII_CCSID && LOCAL_CCSID == EBCDIC_CCSID)
                {
                emem2amem((char *)InputBuffer,Buffer,Size);
                }
        else if (LobCCSID == EBCDIC_CCSID && LOCAL_CCSID == ASCII_CCSID)
                {
                amem2emem((char *)InputBuffer,Buffer,Size);
                }
        else if (LobCCSID == UCS2_CCSID && LOCAL_CCSID == ASCII_CCSID)
                {
                const char *PosSrc = Buffer;
                u2char_t *PosDest = (u2char_t *)InputBuffer;
                for (size_t i=0; i < nChars; i++)
                        {
                        *PosDest = *PosSrc;
                        PosSrc++;
                        PosDest++;
                        }
                }
        else if (LobCCSID == UCS2_CCSID && LOCAL_CCSID == EBCDIC_CCSID)
                {
                const char *PosSrc = Buffer;
                u2char_t *PosDest = (u2char_t *)InputBuffer;
                for (size_t i=0; i < nChars; i++)
                        {
                        *PosDest = e2a(*PosSrc);
                        PosSrc++;
                        PosDest++;
                        }
                }
        else memcpy(InputBuffer,Buffer,Size);
        return InputBuffer;
        }

void* PRCED_Locator::BuildInputBuffer(const byte_t *Buffer, size_t nChars)
        {
        return BuildInputBuffer((const char*)Buffer,nChars);
        }

void* PRCED_Locator::BuildInputBuffer(const u2char_t *Buffer, size_t nChars)
        {
        size_t Size = 0;
        switch (GetLobLocType())
                {
                case DA_CLOBLOC_TYPE: Size = nChars; break;
                case DA_BLOBLOC_TYPE: assert(false); break;
                case DA_DBCLOBLOC_TYPE: Size = nChars*sizeof(u2char_t); 
                                        break;
                }
        void *InputBuffer = malloc(Size);
        if (!InputBuffer) return NULL;
        if (LobCCSID == ASCII_CCSID)
                {
                const u2char_t *PosSrc = Buffer;
                char *PosC = (char *)InputBuffer;
                for (size_t i=0; i < nChars; i++)
                        {
                        *PosC = e2a((char)*PosSrc);
                        PosSrc++;
                        PosC++;
                        }
                }
        else if (LobCCSID == EBCDIC_CCSID)
                {
                const u2char_t *PosSrc = Buffer;
                char *PosC = (char *)InputBuffer;
                for (size_t i=0; i < nChars; i++)
                        {
                        *PosC = a2e((char)*PosSrc);
                        PosSrc++;
                        PosC++;
                        }
                }
        else memcpy(InputBuffer,Buffer,Size);
        return InputBuffer;
        }

#if !U2CHAR_IS_WCHAR

void* PRCED_Locator::BuildInputBuffer(const wchar_t *Buffer, size_t nChars)
        {
        size_t Size = 0;
        switch (GetLobLocType())
                {
                case DA_CLOBLOC_TYPE: Size = nChars; break;
                case DA_BLOBLOC_TYPE: assert(false); break;
                case DA_DBCLOBLOC_TYPE: Size = nChars*sizeof(u2char_t); 
                                        break;
                }
        void *InputBuffer = malloc(Size);
        if (!InputBuffer) return NULL;
        if (LobCCSID == ASCII_CCSID)
                {
                const wchar_t *PosSrc = Buffer;
                char *PosC = (char *)InputBuffer;
                for (size_t i=0; i < nChars; i++)
                        {
                        *PosC = e2a((char)*PosSrc);
                        PosSrc++;
                        PosC++;
                        }
                }
        else if (LobCCSID == EBCDIC_CCSID)
                {
                const wchar_t *PosSrc = Buffer;
                char *PosC = (char *)InputBuffer;
                for (size_t i=0; i < nChars; i++)
                        {
                        *PosC = a2e((char)*PosSrc);
                        PosSrc++;
                        PosC++;
                        }
                }
        else    {
                const wchar_t *PosSrc = Buffer;
                u2char_t *PosC = (u2char_t *)InputBuffer;
                for (size_t i=0; i < nChars; i++)
                        {
                        *PosC++ = (u2char_t)*PosSrc++;
                        }
                }
        return InputBuffer;
        }

#endif /* !U2CHAR_IS_WCHAR */

bool PRCED_Locator::FreeLocator()
        {
        if (!Locator) return true;
        try     {
                static char *FreeSTMTID = "FREELOCATOR";
                static char *FreeSTMT = "FREE LOCATOR ?";
                PRCED_Statement Statement(FreeSTMTID,
                                          LobPackageLib,LobPackage);
                DAVariables DAVars;
                short int LocatorInd = 0;
                DAVars.AddVariable(LobLocType,"LOCATOR",0);
                DAVars.SetBuffers((char *)&Locator,&LocatorInd);
                DAVars.SetVariable(0,0,&Locator,sizeof(Locator));
                int rc = Statement.Execute(DAVars,0);
                if (rc)
                        {
                        rc = Statement.Prepare(FreeSTMT);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::FreeLocator: "
                                        "Can't Prepare (%d)",rc));
                                return false;
                                }
                        rc = Statement.Execute(DAVars,0);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::FreeLocator: "
                                        "Can't Execute (%d)",rc));
                                return false;
                                }
                        }
                LobSize = 0;
                Locator = 0;
                }
        catch (ASError &Error)
                {
                TERROR(("PRCED_Locator::FreeLocator: %s: %.7s",
                        Error.GetText(),
                        Error.GetExceptionID()));
                throw;
                }
        catch (ErrorClass &Error)
                {
                TERROR(("PRCED_Locator::FreeLocator: %s",Error.GetText()));
                throw;
                }
        return true;
        }

int PRCED_Locator::GetLobLocType()
        {
        return LobLocType;
        }

int PRCED_Locator::GetLobType()
        {
        int LobType;
        switch (LobLocType)
                {
                case DA_CLOBLOC_TYPE: LobType = DA_CLOB_TYPE; break;
                case DA_BLOBLOC_TYPE: LobType = DA_BLOB_TYPE; break;
                case DA_DBCLOBLOC_TYPE: LobType = DA_DBCLOB_TYPE; break;
                default: assert(false);
                }
        return LobType;
        }

size_t PRCED_Locator::GetLength()                  // Length in chars.
        {
        return Size2Length(GetSize());        
        }

size_t PRCED_Locator::GetSize()                    // Length in bytes.
        {
        if (!Locator) return 0;
        if (LobSize) return LobSize;
        try     {
                const char *LengthSTMT;
                const char *LengthSTMTID;
                switch (LobLocType)
                        {
                        case DA_CLOBLOC_TYPE: 
                                LengthSTMT = LengthSTMT_CLOB; 
                                LengthSTMTID = "CLOBLENGTH";
                                break;
                        case DA_BLOBLOC_TYPE: 
                                LengthSTMT = LengthSTMT_BLOB; 
                                LengthSTMTID = "BLOBLENGTH";
                                break;
                        case DA_DBCLOBLOC_TYPE: 
                                LengthSTMT = LengthSTMT_DBCLOB; 
                                LengthSTMTID = "DBCLOBLENGTH";
                                break;
                        default: assert(false);
                        }
                PRCED_Statement Statement(LengthSTMTID,
                                          LobPackageLib,LobPackage);
                DAVariables DAVars;
                struct  {
                        LobLocator_t Locator;
                        int LobSizeInt;
                        } Buffer;
                short int Indicators[2] = {0};
                DAVars.AddVariable(LobLocType,"LOCATOR",0);
                DAVars.AddVariable(DA_INT_TYPE,"LENGTH",0);
                DAVars.SetBuffers((char *)&Buffer,Indicators);
                DAVars.SetVariable(0,0,&Locator,sizeof(Locator));
                Buffer.LobSizeInt = 0;

                int rc = Statement.Execute(DAVars,0);
                if (rc)
                        {
                        rc = Statement.Prepare(LengthSTMT);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::GetSize: "
                                        "Can't Prepare (%d)",rc));
                                return false;
                                }
                        rc = Statement.Execute(DAVars,0);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::GetSize: "
                                        "Can't Execute (%d)",rc));
                                return false;
                                }
                        }
                LobSize = Length2Size(Buffer.LobSizeInt);
                }
        catch (ASError &Error)
                {
                TERROR(("PRCED_Locator::GetSize: %s: %.7s",
                        Error.GetText(),
                        Error.GetExceptionID()));
                throw;
                }
        catch (ErrorClass &Error)
                {
                TERROR(("PRCED_Locator::GetSize: %s",Error.GetText()));
                throw;
                }
        return LobSize;
        }

size_t PRCED_Locator::_GetSubString(byte_t *Buffer, 
                                    size_t nStart, 
                                    size_t nUnits)
        {
        size_t rcLength = 0;                            // Returns Length in
        if (!nUnits) return 0;                          // number of Units.
        char ValuesSTMTID[18+1];

        unsigned nUnitsLog2 = RoundLog2(nUnits + nStart);
        size_t nUnitsROUNDED = (size_t)1 << nUnitsLog2;
        try     {
                const char *ValuesSTMT;
                switch (LobLocType)
                        {
                        case DA_CLOBLOC_TYPE: 
                                ValuesSTMT = ValuesSTMT_CLOB;
                                sprintf(ValuesSTMTID,"CLOBSVALUES%u",nUnitsLog2);
                                break;
                        case DA_BLOBLOC_TYPE: 
                                ValuesSTMT = ValuesSTMT_BLOB; 
                                sprintf(ValuesSTMTID,"BLOBSVALUES%u",nUnitsLog2);
                                break;
                        case DA_DBCLOBLOC_TYPE: 
                                ValuesSTMT = ValuesSTMT_DBCLOB; 
                                sprintf(ValuesSTMTID,"DBCLOBSVALUES%u",nUnitsLog2);
                                break;
                        default: assert(false);
                        }
                PRCED_Statement Statement(ValuesSTMTID,
                                          LobPackageLib,LobPackage);
                DAVariables DAVars;
                unsigned nStartInt = (unsigned)nStart + 1;  // DB is 1 biased.
                unsigned nUnitsInt = (unsigned)nUnits;
                short int Indicators[4] = {0};
                unsigned LobLen = 0;

                DAVars.AddVariable(LobLocType,"LOCATOR",0);
                DAVars.AddVariable(DA_INT_TYPE,"START",0);
                DAVars.AddVariable(DA_INT_TYPE,"COUNT",0);
                DAVars.AddVariable(GetLobType(),"LOBDATA",nUnits,GetLobCCSID());
                DAVars.SetBuffer(0,(char *)&Locator,&Indicators[0]);
                DAVars.SetBuffer(1,(char *)&nStartInt,&Indicators[1]);
                DAVars.SetBuffer(2,(char *)&nUnitsInt,&Indicators[2]);
                DAVars.SetBuffer(3,(char *)Buffer,&Indicators[3],&LobLen);

                int rc = Statement.Execute(DAVars,0);
                if (rc)
                        {
                        char ValuesSTMTText[255+1];
                        sprintf(ValuesSTMTText,ValuesSTMT,nUnitsROUNDED);
                        rc = Statement.Prepare(ValuesSTMTText);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::_GetSubString: "
                                        "Can't Prepare (%d)",rc));
                                return 0;
                                }
                        rc = Statement.Execute(DAVars,0);
                        if (rc)
                                {
                                TERROR(("PRCED_Locator::_GetSubString: "
                                        "Can't Execute (%d)",rc));
                                return 0;
                                }
                        }
                rcLength = DAVars.GetVariableLength(0,3);
                }
        catch (ASError &Error)
                {
                TERROR(("PRCED_Locator::_GetSubString: %s: %.7s",
                        Error.GetText(),
                        Error.GetExceptionID()));
                throw;
                }
        catch (ErrorClass &Error)
                {
                TERROR(("PRCED_Locator::_GetSubString: %s",
                        Error.GetText()));
                throw;
                }
        return rcLength;
        }

size_t PRCED_Locator::GetSubString(char *Buffer, 
                                   size_t nStart, 
                                   size_t nChars)
        {
        return _GetSubString((byte_t *)Buffer,nStart,nChars);
        }

size_t PRCED_Locator::GetSubString(byte_t *Buffer, 
                                   size_t nStart,
                                   size_t nBytes)
        {
        return _GetSubString((byte_t *)Buffer,nStart,nBytes);
        }

size_t PRCED_Locator::GetSubString(u2char_t *Buffer, 
                                   size_t nStart, 
                                   size_t nChars)
        {
        return _GetSubString((byte_t *)Buffer,nStart,nChars);
        }

#if !U2CHAR_IS_WCHAR

size_t PRCED_Locator::GetSubString(wchar_t *Buffer, 
                                   size_t nStart, 
                                   size_t nChars)
        {
        size_t BufferSize = nChars * sizeof(u2char_t);
        u2char_t *DoubleBuffer = (u2char_t *)malloc(BufferSize);
        if (!DoubleBuffer)
                {
                TERROR(("PRCED_Locator::GetSubString: Alloc Failed"));
                return 0;
                }
        size_t rcLen = _GetSubString((byte_t *)DoubleBuffer,nStart,nChars);
        if (rcLen)
                {
                u2char_t *SPos = DoubleBuffer;
                wchar_t *DPos = Buffer;
                for (size_t i=0; i < rcLen; i++) *DPos++ = *SPos++;
                }
        free(DoubleBuffer);
        return rcLen;
        }

#endif /* !U2CHAR_IS_WCHAR */

void PRCED_Locator::InitFromDAVars(DAVariables &Variables, int Row, int Col)
        {
        if (!isLobLocator(Variables,Col)) 
                {
                TINFO(("InitFromDAVars: Var %i is not a Lob Locator",Col));
                return;
                }
        LobLocType = Variables.GetVariableType(Col);
        size_t Length = Variables.GetVariableLength(Row,Col);
        assert(Length == sizeof(LobLocator_t));
        char* Data = Variables.GetVariableData(Row,Col);
        Locator = *(LobLocator_t *)Data;
        LobCCSID = Variables.GetCCSID(Col);
        return;
        }

size_t PRCED_Locator::Length2Size(size_t Length)
        {
        size_t Size;
        if (LobLocType != DA_DBCLOBLOC_TYPE) Size = Length;
        else Size = Length * sizeof(u2char_t);
        return Size;
        }

size_t PRCED_Locator::Size2Length(size_t Size)
        {
        size_t Length;
        if (LobLocType != DA_DBCLOBLOC_TYPE) Length = Size;
        else Length = Size / sizeof(u2char_t);
        return Length;
        }

/* ************************************************************************ */
/* **** CleanupODPs ******************************************************* */
/* ************************************************************************ */

struct ODPCounts_t
        {
        unsigned ODPCount;              // Threshold or Number to close.
        unsigned nRemaining;            // Number remaining after close.
        unsigned short nClosed;         // Number closed.
        };

int CleanupODPs(CloseMethod_t CloseMethod,
                unsigned ODPCount,              
                unsigned *nRemaining,                   // Input / Output
                unsigned short *nClosed)                // Output
        {
        Qsq_sqlca_t sqlca = {0};
        Qsq_sqlda_t sqlda = {0};
        Qsq_SQLP04NN_t qsq = {0};
        ASError Error("CleanupODPs");

        *nClosed = 0;
        qsq.Function = 'B';                             // Cleanup ODPs.
        ODPCounts_t *Counts = (ODPCounts_t*)qsq.Close_File_Name;
        if (CloseMethod == CM_NUMBER)                           // :Method
                {
                memcpy(qsq.Close_Library_Name,"*NUMBER   ",10); 
                }
        else    {
                memcpy(qsq.Close_Library_Name,"*THRESHOLD",10);
                }
        Counts->ODPCount = ODPCount;                            // :ODPCount
        Counts->nClosed = 0;                                    // :nClosed
        QSQPRCED(&sqlca,&sqlda,SQLP04NN_Format,&qsq,Error.GetErrorCode());
        if (Error.IsError()) throw Error;
        if (sqlca.sqlcode)
                {
                TRACE_SQLCODE("CleanupODPs",sqlca);
                return sqlca.sqlcode;
                }
        else    {                                       // Success:
                if (nClosed)
                        {
                        if ((int)Counts->ODPCount < 0)  // Not documented, but
                                {                       // needed to return a
                                *nClosed = 0;           // valid closed count
                                }                       // value to caller.
                        else *nClosed = Counts->nClosed;
                        }
                if (nRemaining) *nRemaining = Counts->nRemaining;
                }
        return sqlca.sqlcode;
        }

// **************************************************************************
// **** Utility: UpdateCurrentLob *******************************************
// **************************************************************************

bool UpdateCurrentLob(const char *PackageLib,
                      const char *Package,
                      const char *Table,
                      const char *ColumnName,
                      PRCED_Cursor &Cursor,
                      PRCED_Locator &Locator) 
        {
        try     {
                const char *UpdateSTMTFMT;
                switch (Locator.GetLobLocType())
                        {
                        case DA_CLOBLOC_TYPE: 
                                UpdateSTMTFMT = UpdateLobSTMT_CLOB;
                                break;
                        case DA_BLOBLOC_TYPE:
                                UpdateSTMTFMT = UpdateLobSTMT_BLOB;
                                break;
                        case DA_DBCLOBLOC_TYPE: 
                                UpdateSTMTFMT = UpdateLobSTMT_DBCLOB;
                                break;
                        default: assert(false);
                        }
                char *CursorName = (char *)Cursor.GetCursorName();
                char *UpdateSTMT = (char *)malloc(strlen(UpdateLobSTMT_CLOB) +
                                                  strlen(Table) +
                                                  strlen(ColumnName) +
                                                  18 /* CursorName */ + 
                                                  1 /* TerminatingNULL */);
                if (!UpdateSTMT)
                        {
                        TERROR(("UpdateCurrentLob: Can't alloc UpdateSTMT"));
                        return false;
                        }
                sprintf(UpdateSTMT,UpdateSTMTFMT,Table,ColumnName,CursorName);

                char *StatementID = CursorName;         // Unique until closed.
                PRCED_Statement Statement(StatementID,PackageLib,Package);
                
                int rc;
                DAVariables DAVars;
                short int Indicators[1] = {0};
                LobLocator_t LocatorBuffer = Locator.GetLocator();
                DAVars.AddVariable(Locator.GetLobLocType(),"LOCATOR",0);
                DAVars.SetBuffer(0,(char *)&LocatorBuffer,&Indicators[0]);

                rc = Statement.Prepare(UpdateSTMT);         // Always prepare,
                if (rc)                                     // as statement 
                        {                                   // text changes.
                        TERROR(("UpdateCurrentLob: Can't Prepare (%d)",rc));
                        return false;
                        }
                rc = Statement.Execute(DAVars,0);
                if (rc)
                        {
                        TERROR(("UpdateCurrentLob: Can't Execute (%d)",rc));
                        return false;
                        }
                }
        catch (ASError &Error)
                {
                TERROR(("UpdateCurrentLob: %s: %.7s",
                        Error.GetText(),
                        Error.GetExceptionID()));
                throw;
                }
        catch (ErrorClass &Error)
                {
                TERROR(("UpdateCurrentLob: %s",Error.GetText()));
                throw;
                }
        return true;
        }

/* ************************************************************************ */
/* **************************** End of File ******************************* */
/* ************************************************************************ */
