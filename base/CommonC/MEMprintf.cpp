// printf to Memory Stream:  MEMprintf.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MEMprintf.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2006 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ebcdic.h>
#include <MEMprintf.h>

#ifdef _WIN32
        #include <excpt.h>
        #ifdef __BORLANDC__
                #define __try try
        #endif
#else
        #undef  __try
        #define __try
        #define __except(x)
        #define EXCEPTION_EXECUTE_HANDLER 1
#endif

typedef long ilong_t;
typedef unsigned long ulong_t;

#ifdef _WIN32
        typedef __int64 ilonglong_t;
        typedef unsigned __int64 ulonglong_t;
        #ifndef wtoi
                #define wtoi _wtoi
        #endif
#else
        #include <wchar.h>
        #include <wctype.h>
        #ifndef wtoi
                static int wtoi(const wchar_t *nptr)
                        {
                        wchar_t *endptr = (wchar_t *)nptr;
                        while (isdigit(*endptr)) endptr++;
                        return wcstol(nptr,&endptr,10);
                        }
        #endif
        typedef long long ilonglong_t;
        typedef unsigned long long ulonglong_t;
#endif

#define REALLOC_CONST_GROWTH 1024               // Can be one (1) or more.

#define BINRadix 2
#define DECRadix 10
#define HEXRadix 16
#define PRINTF_EOF -1
#define nDecDefault 6
#define MaxFloatLength 320                              // Max Zeros = 308
#define MaxNumberLength (sizeof(ulonglong_t) * 8)

const char Digits[] =
        {
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F','G','H','I','J',
        'K','L','M','N','O','P','Q','R','S','T',
        'U','V','W','X','Y','Z'
        };

const wchar_t DigitsW[] =
        {
        L'0',L'1',L'2',L'3',L'4',L'5',L'6',L'7',L'8',L'9',
        L'A',L'B',L'C',L'D',L'E',L'F',L'G',L'H',L'I',L'J',
        L'K',L'L',L'M',L'N',L'O',L'P',L'Q',L'R',L'S',L'T',
        L'U',L'V',L'W',L'X',L'Y',L'Z'
        };

static int strlenN(const char *String, int MaxLength)
        {
        int Length = 0;
        while (Length < MaxLength && *String++) Length++;
        return Length;
        }

static int strlenN(const wchar_t *String, int MaxLength)
        {
        int Length = 0;
        while (Length < MaxLength && *String++) Length++;
        return Length;
        }

// ***************************************************************************
// **** Memory Printf Implementation *****************************************
// ***************************************************************************

struct FormatSpecifier
        {
        struct  {
                bool LeftJustify;       // Set if a '-' bool is specified.
                bool Signed;            // Set if a '+' bool is specified.
                bool MinusSigned;       // Set if a ' ' bool is specified.
                bool AlternateForm;     // Set if a '#' bool is specified.
                } bools;
        int Width;              // Width = Width, None(-1).
        bool ZeroFill;          // Fill with zeros or blanks (false).
        int Precision;          // Precision = Precision, None(-1).
        char SizeModifier;      // One of 'h', 'l', 'L', or none (0).
        char Type;              // One of 'd','i','u','x','X','c','s','p','n'.
        unsigned Radix;         // Determined by type.
        };

struct BufferCB
        {
        size_t Index;
        char *Buffer;
        size_t BufferSize;
        BufferCB() 
                {
                Index = 0;
                Buffer = NULL;
                BufferSize = 0;
                }
        };

class MemoryPrintfIMPL
        {
        private:
                BufferCB BCB;
                bool CharUTF8;
                bool WideUTF8;
                void ReallocBuffer();
        protected:
                MemoryPrintf &ParentInterface;
                void FreeBuffer(void *Buffer);
                void* ReallocBuffer(void *Buffer, size_t Size);
  #ifdef _WIN32
                void DecodeFormat(const char* &Format, 
                                  va_list &Params, 
                                  FormatSpecifier &Spec);
                void DecodeFormatW(const wchar_t* &Format, 
                                   va_list &Params, 
                                   FormatSpecifier &Spec);
  #endif
                void PrintString(const char *String, 
                                 FormatSpecifier &Specifier);
                void PrintString8(const wchar_t *String, 
                                  FormatSpecifier &Specifier);
                void PrintStringW(const wchar_t *String, 
                                  FormatSpecifier &Specifier);
                void PrintString2W(const char *String, 
                                   FormatSpecifier &Specifier);
                void PrintNumber(ulonglong_t Number, 
                                 bool Negitive, 
                                 FormatSpecifier &Spec);
                void PrintNumberW(ulonglong_t Number, 
                                  bool Negitive, 
                                  FormatSpecifier &Spec);
                void PrintFloat(double Number, FormatSpecifier &Spec);
                void PrintFloatW(double Number, FormatSpecifier &Spec);
                void PrintUTF8(wchar_t c);
        public:
                MemoryPrintfIMPL(MemoryPrintf &Interface);
                ~MemoryPrintfIMPL() {Clear();}
                char* AquireBuffer();
                void _AttachBuffer(char *Buffer, size_t Size);
                void Clear();
                const char* GetBuffer();
                void SetUTF8(bool CharUTF8, bool WideUTF8);
                // ***************
                // **** stdio ****
                // ***************
                int vprintf(const char *Format, va_list Params);
                int vwprintf(const wchar_t *Format, va_list Params);
                int fputs(const char *String);
                int fputws(const wchar_t *String);
        };


MemoryPrintfIMPL::MemoryPrintfIMPL(MemoryPrintf &Interface)
        :
        ParentInterface(Interface)
        {
        CharUTF8 = false;
        WideUTF8 = false;
        return;
        }

char* MemoryPrintfIMPL::AquireBuffer()
        {
        char *Buffer = BCB.Buffer;
        BCB.Index = 0;
        BCB.Buffer = NULL;
        BCB.BufferSize = 0;
        return Buffer;
        }

void MemoryPrintfIMPL::_AttachBuffer(char *Buffer, size_t Size)
        {
        Clear();
        if (Buffer)
                {
                BCB.Index = strlen(Buffer);
                BCB.Buffer = Buffer;
                BCB.BufferSize = Size;
                }
        return;
        }

void MemoryPrintfIMPL::Clear()
        {
        char *Buffer = AquireBuffer();
        if (Buffer) FreeBuffer(Buffer);
        return;
        }

const char* MemoryPrintfIMPL::GetBuffer()
        {
        return BCB.Buffer;
        }

void MemoryPrintfIMPL::FreeBuffer(void *Buffer)
        {
        ParentInterface.FreeBuffer(Buffer);
        return;
        }
 
int MemoryPrintfIMPL::fputs(const char *String)
        {
        size_t StringLength = strlen(String);
        size_t StringLengthZ = StringLength + sizeof(char);
        if (BCB.BufferSize - BCB.Index < StringLengthZ)
                {
                size_t MinInc = StringLengthZ;
                size_t MaxInc = REALLOC_CONST_GROWTH;
                if (MinInc > MaxInc) MaxInc = MinInc;
                size_t NewSize = BCB.BufferSize * 2 + MaxInc;
                void *NewBuffer = ReallocBuffer(BCB.Buffer,NewSize);
                if (!NewBuffer && BCB.Buffer) FreeBuffer(BCB.Buffer);
                BCB.Buffer = (char *)NewBuffer;
                BCB.BufferSize = NewSize;       // Still counting...
                }
        if (BCB.Buffer)
                {
                if (BCB.Index && BCB.Buffer[BCB.Index-1] == '\0') BCB.Index--;
                memcpy(&BCB.Buffer[BCB.Index],String,StringLengthZ);
                }
        BCB.Index += StringLengthZ;
        return (int)StringLength;
        }

int MemoryPrintfIMPL::fputws(const wchar_t *String)
        {
        size_t StringLength = wcslen(String) * sizeof(wchar_t);
        size_t StringLengthZ = StringLength + sizeof(wchar_t);
        if (BCB.BufferSize - BCB.Index < StringLengthZ)
                {
                size_t MinInc = StringLengthZ;
                size_t MaxInc = REALLOC_CONST_GROWTH;
                if (MinInc > MaxInc) MaxInc = MinInc;
                size_t NewSize = BCB.BufferSize * 2 + MaxInc;
                void *NewBuffer = ReallocBuffer(BCB.Buffer,NewSize);
                if (!NewBuffer && BCB.Buffer) FreeBuffer(BCB.Buffer);
                BCB.Buffer = (char *)NewBuffer;
                BCB.BufferSize = NewSize;       // Still counting...
                }
        if (BCB.Buffer)
                {
                if (BCB.Index >= 2 && 
                    BCB.Buffer[BCB.Index-1] == '\0' &&
                    BCB.Buffer[BCB.Index-2] == '\0') BCB.Index -= 2;
                memcpy(&BCB.Buffer[BCB.Index],String,StringLengthZ);
                }
        BCB.Index += StringLengthZ;
        return (int)StringLength / sizeof(wchar_t);
        }

void MemoryPrintfIMPL::ReallocBuffer()
        {
        size_t NewSize = BCB.BufferSize * 2 + REALLOC_CONST_GROWTH;
        void *NewBuffer = ReallocBuffer(BCB.Buffer,NewSize);
        if (!NewBuffer && BCB.Buffer) FreeBuffer(BCB.Buffer);
        BCB.Buffer = (char *)NewBuffer;
        BCB.BufferSize = NewSize;               // Still counting...
        return;
        }

void* MemoryPrintfIMPL::ReallocBuffer(void *Buffer, size_t Size)
        {
        return ParentInterface.ReallocBuffer(Buffer,Size);
        }

void MemoryPrintfIMPL::SetUTF8(bool charUTF8, bool wideUTF8)
        {
        CharUTF8 = charUTF8;
        WideUTF8 = wideUTF8;
        return;
        }

// ************************
// ***** Print String *****
// ************************

#define WriteBuffer(Character)                                                \
        {                                                                     \
        if (BCB.Index == BCB.BufferSize) ReallocBuffer();                     \
        if (BCB.Buffer) BCB.Buffer[BCB.Index] = (Character);                  \
        ++BCB.Index;                                                          \
        }

void MemoryPrintfIMPL::PrintString(const char *String, 
                                   FormatSpecifier &Specifier)
        {
        int &Prec = Specifier.Precision;        // Copies String to Buffer.
        int &Width = Specifier.Width;
        int Length = Prec >= 0 ? (int)strlenN(String,Prec) 
                               : (int)strlen(String);
        int BufferLength = Width > Length ? Width : Length;
        int i, Delta = BufferLength - Length;
        if (Specifier.bools.LeftJustify)
                {
                for (i=0; i < Length; i++) 
                        {
                        unsigned char c = String[i];
                        if (CharUTF8 && c > 127) PrintUTF8(c);  // UTF-8.
                        else WriteBuffer(c);                    // Latin-1.
                        }
                for (i=0; i < Delta; i++) WriteBuffer(' ');
                }
        else    {
                char FillChar = Specifier.ZeroFill ? '0' : ' ';
                for (i=0; i < Delta; i++) WriteBuffer(FillChar);
                for (i=0; i < Length; i++)
                        {
                        unsigned char c = String[i];
                        if (CharUTF8 && c > 127) PrintUTF8(c);  // UTF-8.
                        else WriteBuffer(c);                    // Latin-1.
                        }
                }
        return;
        }

void MemoryPrintfIMPL::PrintString8(const wchar_t *String, 
                                    FormatSpecifier &Specifier)
        {
        int &Prec = Specifier.Precision;       // Copies String to utf8 Buffer.
        int &Width = Specifier.Width;
        int Length = Prec >= 0 ? strlenN(String,Prec) : wcslen(String);
        int BufferLength = Width > Length ? Width : Length;
        int i, Delta = BufferLength - Length;
        if (Specifier.bools.LeftJustify)
                {
                for (i=0; i < Length; i++) 
                        {
                        wchar_t w = String[i];
                        if (WideUTF8 && (w > 127 || w < 0)) PrintUTF8(w);
                        else WriteBuffer(u2a(w));
                        }
                for (i=0; i < Delta; i++) WriteBuffer(' ');
                }
        else    {
                char FillChar = Specifier.ZeroFill ? '0' : ' ';
                for (i=0; i < Delta; i++) WriteBuffer(FillChar);
                for (i=0; i < Length; i++)
                        {
                        wchar_t w = String[i];
                        if (WideUTF8 && (w > 127 || w < 0)) PrintUTF8(w);
                        else WriteBuffer(u2a(w));
                        }
                }
        return;
        }

#define WriteBufferW(Character)                                               \
        {                                                                     \
        if (BCB.Index == BCB.BufferSize) ReallocBuffer();                     \
        if (BCB.Buffer) *(wchar_t *)(BCB.Buffer + BCB.Index) = (Character);   \
        BCB.Index += sizeof(wchar_t);                                         \
        }

void MemoryPrintfIMPL::PrintStringW(const wchar_t *String,
                                    FormatSpecifier &Specifier)
        {
        int &Prec = Specifier.Precision;       // Copies Wide String to Buffer.
        int &Width = Specifier.Width;
        int Length = Prec >= 0 ? (int)strlenN(String,Prec) 
                               : (int)wcslen(String);
        int BufferLength = Width > Length ? Width : Length;
        int i, Delta = BufferLength - Length;
        if (Specifier.bools.LeftJustify)
                {
                for (i=0; i < Length; i++) 
                        {
                        wchar_t c = String[i];
                        WriteBufferW(c);
                        }
                for (i=0; i < Delta; i++) WriteBufferW(L' ');
                }
        else    {
                wchar_t FillChar = Specifier.ZeroFill ? L'0' : L' ';
                for (i=0; i < Delta; i++) WriteBufferW(FillChar);
                for (i=0; i < Length; i++)
                        {
                        wchar_t c = String[i];
                        WriteBufferW(c);
                        }
                }
        return;
        }

void MemoryPrintfIMPL::PrintString2W(const char *String, 
                                     FormatSpecifier &Specifier)
        {
        int &Prec = Specifier.Precision;       // Copies String to Wide Buffer.
        int &Width = Specifier.Width;
        int Length = Prec >= 0 ? (int)strlenN(String,Prec) 
                               : (int)strlen(String);
        int BufferLength = Width > Length ? Width : Length;
        int i, Delta = BufferLength - Length;
        if (Specifier.bools.LeftJustify)
                {
                for (i=0; i < Length; i++) 
                        {
                        wchar_t c = (unsigned char)String[i];
                        WriteBufferW(c);
                        }
                for (i=0; i < Delta; i++) WriteBufferW(L' ');
                }
        else    {
                wchar_t FillChar = Specifier.ZeroFill ? L'0' : L' ';
                for (i=0; i < Delta; i++) WriteBufferW(FillChar);
                for (i=0; i < Length; i++)
                        {
                        wchar_t c = (unsigned char)String[i];
                        WriteBufferW(c);
                        }
                }
        return;
        }

inline void MemoryPrintfIMPL::PrintUTF8(wchar_t c)
        {
        char UTF8[8];
        size_t n = WCHAR_to_UTF8(UTF8,sizeof(UTF8),&c,1);
        for (size_t i=0; i<n; i++) WriteBuffer(UTF8[i]);
        return;
        }

// *************************
// ***** Print Integer *****
// *************************

void MemoryPrintfIMPL::PrintNumber(ulonglong_t Number, 
                                   bool Negitive,
                                   FormatSpecifier &Spec)
        {
        ulonglong_t Reduced, Remainder;
        char Stack[MaxNumberLength+2];          // Number plus Sign + NULL.
        unsigned SP = MaxNumberLength+2;        // Copies Number to Buffer.
        Stack[--SP] = '\0';                     // Terminating NULL.
        int Precision = Spec.Precision;
        int Prec = Precision > -1 ? Precision : 1;
        while ((Prec || Number) && SP > 1)
                {
                Reduced = Number / Spec.Radix;
                Remainder = Number - Reduced * Spec.Radix;
                Stack[--SP] = Digits[(unsigned)Remainder];
                Number = Reduced;
                if (Prec) Prec--;
                }
        if (Negitive) Stack[--SP] = '-';
        else if (Spec.bools.Signed) Stack[--SP] = '+';
        Spec.Precision = -1;                    // Deactivate for PrintString.
        PrintString(Stack + SP,Spec);
        Spec.Precision = Precision;             // Restore.
        return;
        }

void MemoryPrintfIMPL::PrintNumberW(ulonglong_t Number, 
                                    bool Negitive,
                                    FormatSpecifier &Spec)
        {
        ulonglong_t Reduced, Remainder;
        wchar_t Stack[MaxNumberLength+2];       // Number plus Sign + NULL.
        unsigned SP = MaxNumberLength+2;        // Copies Number to Buffer.
        Stack[--SP] = L'\0';                    // Terminating NULL.
        int Precision = Spec.Precision;
        int Prec = Precision > -1 ? Precision : 1;
        while ((Prec || Number) && SP > 1)
                {
                Reduced = Number / Spec.Radix;
                Remainder = Number - Reduced * Spec.Radix;
                Stack[--SP] = Digits[(unsigned)Remainder];
                Number = Reduced;
                if (Prec) Prec--;
                }
        if (Negitive) Stack[--SP] = L'-';
        else if (Spec.bools.Signed) Stack[--SP] = L'+';
        Spec.Precision = -1;                    // Deactivate for PrintString.
        PrintStringW(Stack + SP,Spec);
        Spec.Precision = Precision;             // Restore.
        return;
        }

// ***********************
// ***** Print Float *****
// ***********************

void MemoryPrintfIMPL::PrintFloat(double Number, FormatSpecifier &Spec)
        {
        char Buffer[MaxFloatLength+1];          // Number plus NULL.
        int nDec = Spec.Precision >= 0 ? Spec.Precision : nDecDefault;
        if (tolower(Spec.Type) == 'e')
                {
                sprintf(Buffer,"%.*e",nDec,Number);  // Always exponent.
                }
        else if (tolower(Spec.Type) == 'g')
                {
                sprintf(Buffer,"%.*g",nDec,Number);  // Smart exponent.
                }
        else    {
                sprintf(Buffer,"%.*f",nDec,Number);  // No exponent.
                }
        Spec.Precision = -1;                    // Deactivate for PrintString.
        PrintString(Buffer,Spec);
        Spec.Precision = nDec;                  // Restore.
        return;
        }

void MemoryPrintfIMPL::PrintFloatW(double Number, FormatSpecifier &Spec)
        {
        wchar_t Buffer[MaxFloatLength+1];       // Number plus NULL.
        int nDec = Spec.Precision >= 0 ? Spec.Precision : nDecDefault;
        if (tolower(Spec.Type) == 'e')
                {                               // Always exponent.
  #ifdef _WIN32
                swprintf(Buffer,L"%.*e",nDec,Number);  
  #else
                size_t nBuffer = sizeof(Buffer) / sizeof(wchar_t);
                swprintf(Buffer,nBuffer,L"%.*e",nDec,Number);
  #endif
                }
        else if (tolower(Spec.Type) == 'g')     // Smart exponent.
                {
  #ifdef _WIN32
                swprintf(Buffer,L"%.*g",nDec,Number);  
  #else
                size_t nBuffer = sizeof(Buffer) / sizeof(wchar_t);
                swprintf(Buffer,nBuffer,L"%.*g",nDec,Number);
  #endif
                }
        else    {                               // No exponent.
  #ifdef _WIN32
                swprintf(Buffer,L"%.*f",nDec,Number);  
  #else
                size_t nBuffer = sizeof(Buffer) / sizeof(wchar_t);
                swprintf(Buffer,nBuffer,L"%.*f",nDec,Number);
  #endif
                }
        Spec.Precision = -1;                    // Deactivate for PrintString.
        PrintStringW(Buffer,Spec);
        Spec.Precision = nDec;                  // Restore.
        return;
        }

// ***********************
// ***********************
// ***********************

#ifndef _WIN32                   /* The Linux version uses this although,  */
                                 /* a DecodeFormat(...,va_list Params,...) */
                                 /* also functions.                        */
#define DecodeFormat(Format,Params,Spec)        /* Macro on OS400, because */ \
        {                                       /* it can't handle passing */ \
        Format++;                               /* an "active" va_list.    */ \
        Spec.Width = -1;                /* Width = Width, None(-1).        */ \
        Spec.Precision = -1;            /* Precision = Precision, None(-1).*/ \
        Spec.ZeroFill = false;                                                \
        memset(&Spec.bools,false,sizeof(Spec.bools));                         \
        while (true)                    /* Decode bools.                   */ \
                {                                                             \
                if (*Format == '-') Spec.bools.LeftJustify = true;            \
                else if (*Format == '+') Spec.bools.Signed = true;            \
                else if (*Format == ' ') Spec.bools.MinusSigned = true;       \
                else if (*Format == '#') Spec.bools.AlternateForm = true;     \
                else break;                                                   \
                Format++;                                                     \
                }                                                             \
        if (isdigit(*Format))                   /* Decode Width.           */ \
                {                                                             \
                Spec.Width = atoi(Format);                                    \
                if (*Format == '0') Spec.ZeroFill = true;                     \
                while (isdigit(*Format)) Format++;                            \
                }                                                             \
        else if (*Format == '*')                                              \
                {                                                             \
                Spec.Width = va_arg(Params,unsigned);                         \
                Format++;                                                     \
                }                                                             \
        if (*Format == '.')                     /* Decode Precision.       */ \
                {                                                             \
                Format++;                                                     \
                if (isdigit(*Format))                                         \
                        {                                                     \
                        Spec.Precision = atoi(Format);                        \
                        while (isdigit(*Format)) Format++;                    \
                        }                                                     \
                else if (*Format == '*')                                      \
                        {                                                     \
                        Spec.Precision = va_arg(Params,unsigned);             \
                        Format++;                                             \
                        }                                                     \
                }                                                             \
        if (*Format == 'h' ||                                                 \
            *Format == 'l' ||                                                 \
            *Format == 'w' ||                                                 \
            *Format == 'L')                     /* Decode Size Modifier.   */ \
                {                                                             \
                if (Format[1] == 'l')                                         \
                        {                                                     \
                        Spec.SizeModifier = 'L';                              \
                        Format += 2;                                          \
                        }                                                     \
                else Spec.SizeModifier = *Format++;                           \
                }                                                             \
        else if (*Format == 'I')                                              \
                {                                                             \
                if (Format[1] == '6' && Format[2] == '4')                     \
                        {                                                     \
                        Spec.SizeModifier = 'L';                              \
                        Format += 3;                                          \
                        }                                                     \
                else if (Format[1] == '3' && Format[2] == '2')                \
                        {                                                     \
                        Format += 3;                                          \
                        }                                                     \
                else    {                                                     \
                        Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';  \
                        Format++;                                             \
                        }                                                     \
                }                                                             \
        else if (*Format == 't' || *Format == 'z')                            \
                {                                                             \
                Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';          \
                Format++;                                                     \
                }                                                             \
        else Spec.SizeModifier = 0;                                           \
        Spec.Type = *Format++;                  /* Decode Type.            */ \
        }

#define DecodeFormatW(Format,Params,Spec)       /* Macro on OS400, because */ \
        {                                       /* it can't handle passing */ \
        Format++;                               /* an "active" va_list.    */ \
        Spec.Width = -1;                /* Width = Width, None(-1).        */ \
        Spec.Precision = -1;            /* Precision = Precision, None(-1).*/ \
        Spec.ZeroFill = false;                                                \
        memset(&Spec.bools,false,sizeof(Spec.bools));                         \
        while (true)                    /* Decode bools.                   */ \
                {                                                             \
                if (*Format == L'-') Spec.bools.LeftJustify = true;           \
                else if (*Format == L'+') Spec.bools.Signed = true;           \
                else if (*Format == L' ') Spec.bools.MinusSigned = true;      \
                else if (*Format == L'#') Spec.bools.AlternateForm = true;    \
                else break;                                                   \
                Format++;                                                     \
                }                                                             \
        if (isdigit(*Format))                   /* Decode Width.           */ \
                {                                                             \
                Spec.Width = wtoi(Format);                                    \
                if (*Format == L'0') Spec.ZeroFill = true;                    \
                while (iswdigit(*Format)) Format++;                           \
                }                                                             \
        else if (*Format == L'*')                                             \
                {                                                             \
                Spec.Width = va_arg(Params,unsigned);                         \
                Format++;                                                     \
                }                                                             \
        if (*Format == L'.')                     /* Decode Precision.      */ \
                {                                                             \
                Format++;                                                     \
                if (isdigit(*Format))                                         \
                        {                                                     \
                        Spec.Precision = wtoi(Format);                        \
                        while (isdigit(*Format)) Format++;                    \
                        }                                                     \
                else if (*Format == L'*')                                     \
                        {                                                     \
                        Spec.Precision = va_arg(Params,unsigned);             \
                        Format++;                                             \
                        }                                                     \
                }                                                             \
        if (*Format == L'h' ||                                                \
            *Format == L'l' ||                                                \
            *Format == L'w' ||                                                \
            *Format == L'L')                     /* Decode Size Modifier.  */ \
                {                                                             \
                if (Format[1] == L'l')                                        \
                        {                                                     \
                        Spec.SizeModifier = 'L';                              \
                        Format += 2;                                          \
                        }                                                     \
                else Spec.SizeModifier = (char)*Format++;                     \
                }                                                             \
        else if (*Format == L'I')                                             \
                {                                                             \
                if (Format[1] == L'6' && Format[2] == L'4')                   \
                        {                                                     \
                        Spec.SizeModifier = 'L';                              \
                        Format += 3;                                          \
                        }                                                     \
                else if (Format[1] == L'3' && Format[2] == L'2')              \
                        {                                                     \
                        Format += 3;                                          \
                        }                                                     \
                else    {                                                     \
                        Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';  \
                        Format++;                                             \
                        }                                                     \
                }                                                             \
        else if (*Format == L't' || *Format == L'z')                          \
                {                                                             \
                Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';          \
                Format++;                                                     \
                }                                                             \
        else Spec.SizeModifier = 0;                                           \
        Spec.Type = (char)*Format++;            /* Decode Type.            */ \
        }

#else /* !OS400 */

void MemoryPrintfIMPL::DecodeFormat(const char* &Format, 
                                    va_list &Params, 
                                    FormatSpecifier &Spec)
        {
        Format++;
        Spec.Width = -1;                // Width = Width, None(-1).
        Spec.Precision = -1;            // Precision = Precision, None(-1).
        Spec.ZeroFill = false;
        memset(&Spec.bools,false,sizeof(Spec.bools));
        while (true)                    // Decode bools.
                {
                if (*Format == '-') Spec.bools.LeftJustify = true;
                else if (*Format == '+') Spec.bools.Signed = true;
                else if (*Format == ' ') Spec.bools.MinusSigned = true;
                else if (*Format == '#') Spec.bools.AlternateForm = true;
                else break;
                Format++;
                }
        if (isdigit(*Format))                   // Decode Width.
                {
                Spec.Width = atoi(Format);
                if (*Format == '0') Spec.ZeroFill = true;
                while (isdigit(*Format)) Format++;
                }
        else if (*Format == '*')
                {
                Spec.Width = va_arg(Params,unsigned);
                Format++;
                }
        if (*Format == '.')                     // Decode Precision.
                {
                Format++;
                if (isdigit(*Format))
                        {
                        Spec.Precision = atoi(Format);
                        while (isdigit(*Format)) Format++;
                        }
                else if (*Format == '*')
                        {
                        Spec.Precision = va_arg(Params,unsigned);
                        Format++;
                        }
                }
        if (*Format == 'h' ||
            *Format == 'l' ||
            *Format == 'w' ||
            *Format == 'L')                     // Decode Size Modifier.
                {
                if (Format[1] == 'l') 
                        {
                        Spec.SizeModifier = 'L';
                        Format += 2;
                        }
                else Spec.SizeModifier = *Format++;
                }
        else if (*Format == 'I')
                {
                if (Format[1] == '6' && Format[2] == '4') 
                        {
                        Spec.SizeModifier = 'L';
                        Format += 3;
                        }
                else if (Format[1] == '3' && Format[2] == '2')
                        {
                        Format += 3;
                        }
                else    {
                        Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';
                        Format++;
                        }
                }
        else if (*Format == 't' || *Format == 'z')
                {
                Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';
                Format++;
                }
        else Spec.SizeModifier = 0;
        Spec.Type = *Format++;                  // Decode Type.
        return;
        }

void MemoryPrintfIMPL::DecodeFormatW(const wchar_t* &Format, 
                                     va_list &Params, 
                                     FormatSpecifier &Spec)
        {
        Format++;
        Spec.Width = -1;                // Width = Width, None(-1).
        Spec.Precision = -1;            // Precision = Precision, None(-1).
        Spec.ZeroFill = false;
        memset(&Spec.bools,false,sizeof(Spec.bools));
        while (true)                    // Decode bools.
                {
                if (*Format == L'-') Spec.bools.LeftJustify = true;
                else if (*Format == L'+') Spec.bools.Signed = true;
                else if (*Format == L' ') Spec.bools.MinusSigned = true;
                else if (*Format == L'#') Spec.bools.AlternateForm = true;
                else break;
                Format++;
                }
        if (iswdigit(*Format))                  // Decode Width.
                {
                Spec.Width = wtoi(Format);
                if (*Format == L'0') Spec.ZeroFill = true;
                while (iswdigit(*Format)) Format++;
                }
        else if (*Format == L'*')
                {
                Spec.Width = va_arg(Params,unsigned);
                Format++;
                }
        if (*Format == L'.')                    // Decode Precision.
                {
                Format++;
                if (iswdigit(*Format))
                        {
                        Spec.Precision = wtoi(Format);
                        while (iswdigit(*Format)) Format++;
                        }
                else if (*Format == L'*')
                        {
                        Spec.Precision = va_arg(Params,unsigned);
                        Format++;
                        }
                }
        if (*Format == L'h' ||
            *Format == L'l' ||
            *Format == L'w' ||
            *Format == L'L')                    // Decode Size Modifier.
                {
                if (Format[1] == L'l') 
                        {
                        Spec.SizeModifier = 'L';
                        Format += 2;
                        }
                else Spec.SizeModifier = (char)*Format++;
                }
        else if (*Format == L'I')
                {
                if (Format[1] == L'6' && Format[2] == L'4') 
                        {
                        Spec.SizeModifier = 'L';
                        Format += 3;
                        }
                else if (Format[1] == L'3' && Format[2] == L'2')
                        {
                        Format += 3;
                        }
                else    {
                        Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';
                        Format++;
                        }
                }
        else if (*Format == L't' || *Format == L'z')
                {
                Spec.SizeModifier = sizeof(size_t) == 8 ? 'L' : 'l';
                Format++;
                }
        else Spec.SizeModifier = 0;
        Spec.Type = (char)*Format++;            // Decode Type.
        return;
        }

#endif /* !OS400 */

int MemoryPrintfIMPL::vprintf(const char *Format, va_list Params)
        {
        FormatSpecifier Specifier;
        size_t Index0 = BCB.Index;
        if (BCB.Buffer && BCB.Index && BCB.Buffer[BCB.Index-1] == '\0')
                {
                BCB.Index--;
                }
        while (*Format)
                {
                char formatChar = *Format;
                if (formatChar != '%')
                        {
                        if (CharUTF8 && (unsigned char)formatChar > 127)
                                {
                                PrintUTF8(a2u(formatChar));
                                }
                        else WriteBuffer(formatChar);
                        Format++;
                        continue;
                        }
                DecodeFormat(Format,Params,Specifier);
                switch (Specifier.Type)
                        {
                        case 'S':                   // Wide Character.
                                {
                                Specifier.SizeModifier = 'l';
                                }
                                /* Fall through... */
                        case 's':                   // String.
                                {
                                void *Value = va_arg(Params,void *);
                                if (!Value)
                                        {
                                        PrintString("(null)",Specifier);
                                        }
                                else if (Specifier.SizeModifier == 'l' ||
                                         Specifier.SizeModifier == 'w')
                                        {
                                        wchar_t *String = (wchar_t *)Value;
                                        __try   {
                                                PrintString8(String,Specifier);
                                                }
                                        __except (EXCEPTION_EXECUTE_HANDLER)
                                                {
                                                }
                                        }
                                else    {
                                        char *String = (char *)Value;
                                        __try   {
                                                PrintString(String,Specifier);
                                                }
                                        __except (EXCEPTION_EXECUTE_HANDLER)
                                                {
                                                }
                                        }
                                break;
                                }
                        case 'C':                   // Wide Character.
                                {
                                Specifier.SizeModifier = 'l';
                                }
                                /* Fall through... */
                        case 'c':                   // Character.
                                {
                                int Value = va_arg(Params,int);
                                if (Specifier.SizeModifier == 'l' ||
                                    Specifier.SizeModifier == 'w')
                                        {
                                        wchar_t String[2];
                                        String[0] = (wchar_t)Value;
                                        String[1] = '\0';
                                        PrintString8(String,Specifier);
                                        }
                                else    {
                                        char String[2];
                                        String[0] = (char)Value;
                                        String[1] = '\0';
                                        PrintString(String,Specifier);
                                        }
                                break;
                                }
                        case 'x':                   // Hex Number.
                        case 'X':
                                {
                                ulonglong_t Value;
                                if (Specifier.SizeModifier == 'l')
                                        {
                                        Value = va_arg(Params,ulong_t);
                                        }
                                else if (Specifier.SizeModifier == 'L')
                                        {
                                        Value = va_arg(Params,ulonglong_t);
                                        }
                                else Value = va_arg(Params,unsigned);
                                Specifier.Radix = HEXRadix;
                                PrintNumber(Value,false,Specifier);
                                break;
                                }
                        case 'u':                   // Unsigned Number.
                                {
                                ulonglong_t Value;
                                if (Specifier.SizeModifier == 'l')
                                        {
                                        Value = va_arg(Params,ulong_t);
                                        }
                                else if (Specifier.SizeModifier == 'L')
                                        {
                                        Value = va_arg(Params,ulonglong_t);
                                        }
                                else Value = va_arg(Params,unsigned);
                                Specifier.Radix = DECRadix;
                                PrintNumber(Value,false,Specifier);
                                break;
                                }
                        case 'i':                   // Integer.
                        case 'd':
                                {
                                ilonglong_t Value;
                                if (Specifier.SizeModifier == 'l')
                                        {
                                        Value = va_arg(Params,ilong_t);
                                        }
                                else if (Specifier.SizeModifier == 'L')
                                        {
                                        Value = va_arg(Params,ilonglong_t);
                                        }
                                else Value = va_arg(Params,int);
                                Specifier.Radix = DECRadix;
                                if (Value < 0)
                                        {
                                        ulonglong_t uValue;
                                        uValue = (ulonglong_t)-Value;
                                        PrintNumber(uValue,true,Specifier);
                                        }
                                else    {
                                        ulonglong_t uValue;
                                        uValue = (ulonglong_t)Value;
                                        PrintNumber(uValue,false,Specifier);
                                        }
                                break;
                                }
                        case 'f':                   // Floating point.
                        case 'F':
                        case 'g':
                        case 'e':
                        case 'E':
                                {
                                double Value = va_arg(Params,double);
                                PrintFloat(Value,Specifier);
                                break;
                                }
                        case 'p':                   // Pointer.
                                {
                                size_t Ptr = (size_t)va_arg(Params,void *);
                                Specifier.Radix = HEXRadix;
                                PrintNumber(Ptr,false,Specifier);
                                break;
                                }
                        case '%':                   // Percent.
                                {
                                char String[2];
                                String[0] = '%';
                                String[1] = '\0';
                                PrintString(String,Specifier);
                                break;
                                }
                        case 'n':   // Assigns characters written so far.
                                {
                                int *CharsWritten = va_arg(Params,int *);
                                if (CharsWritten)
                                        {
                                        size_t nWritten = BCB.Index - Index0;
                                        *CharsWritten = (int)nWritten;
                                        }
                                break;
                                }
                        default: return PRINTF_EOF;
                        }
                }
        WriteBuffer('\0');                 // String terminator.
        return (int)(BCB.Index - Index0);  // Return: Number of bytes output.
        }

int MemoryPrintfIMPL::vwprintf(const wchar_t *Format, va_list Params)
        {
        FormatSpecifier Specifier;
        size_t Index0 = BCB.Index;
        if (BCB.Buffer && 
            BCB.Index >= 2 && 
            BCB.Buffer[BCB.Index-1] == '\0' &&
            BCB.Buffer[BCB.Index-2] == '\0')
                {
                BCB.Index -= 2;
                }
        while (*Format)
                {
                wchar_t formatChar = *Format;
                if (formatChar != L'%')
                        {
                        WriteBufferW(formatChar);
                        Format++;
                        continue;
                        }
                DecodeFormatW(Format,Params,Specifier);
                switch (Specifier.Type)
                        {
                        case 'S':                   // Byte Character.
                                {
                                Specifier.SizeModifier = 'h';
                                }
                                /* Fall through... */
                        case 's':                   // String.
                                {
                                void *Value = va_arg(Params,void *);
                                if (!Value)
                                        {
                                        PrintStringW(L"(null)",Specifier);
                                        }
                                else if (Specifier.SizeModifier == 'h')
                                        {
                                        char *String = (char *)Value;
                                        __try   {
                                                PrintString2W(String,Specifier);
                                                }
                                        __except (EXCEPTION_EXECUTE_HANDLER)
                                                {
                                                }
                                        }
                                else    {
                                        wchar_t *String = (wchar_t *)Value;
                                        __try   {
                                                PrintStringW(String,Specifier);
                                                }
                                        __except (EXCEPTION_EXECUTE_HANDLER)
                                                {
                                                }
                                        }
                                break;
                                }
                        case 'C':                   // Byte Character.
                                {
                                Specifier.SizeModifier = 'h';
                                }
                                /* Fall through... */
                        case 'c':                   // Character.
                                {
                                int Value = va_arg(Params,int);
                                if (Specifier.SizeModifier == 'h')
                                        {
                                        char String[2];
                                        String[0] = (char)Value;
                                        String[1] = '\0';
                                        PrintString2W(String,Specifier);
                                        }
                                else    {
                                        wchar_t String[2];
                                        String[0] = (wchar_t)Value;
                                        String[1] = L'\0';
                                        PrintStringW(String,Specifier);
                                        }
                                break;
                                }
                        case 'x':                   // Hex Number.
                        case 'X':
                                {
                                ulonglong_t Value;
                                if (Specifier.SizeModifier == 'l')
                                        {
                                        Value = va_arg(Params,ulong_t);
                                        }
                                else if (Specifier.SizeModifier == 'L')
                                        {
                                        Value = va_arg(Params,ulonglong_t);
                                        }
                                else Value = va_arg(Params,unsigned);
                                Specifier.Radix = HEXRadix;
                                PrintNumberW(Value,false,Specifier);
                                break;
                                }
                        case 'u':                   // Unsigned Number.
                                {
                                ulonglong_t Value;
                                if (Specifier.SizeModifier == 'l')
                                        {
                                        Value = va_arg(Params,ulong_t);
                                        }
                                else if (Specifier.SizeModifier == 'L')
                                        {
                                        Value = va_arg(Params,ulonglong_t);
                                        }
                                else Value = va_arg(Params,unsigned);
                                Specifier.Radix = DECRadix;
                                PrintNumberW(Value,false,Specifier);
                                break;
                                }
                        case 'i':                   // Integer.
                        case 'd':
                                {
                                ilonglong_t Value;
                                if (Specifier.SizeModifier == 'l')
                                        {
                                        Value = va_arg(Params,ilong_t);
                                        }
                                else if (Specifier.SizeModifier == 'L')
                                        {
                                        Value = va_arg(Params,ilonglong_t);
                                        }
                                else Value = va_arg(Params,int);
                                Specifier.Radix = DECRadix;
                                if (Value < 0)
                                        {
                                        ulonglong_t uValue;
                                        uValue = (ulonglong_t)-Value;
                                        PrintNumberW(uValue,true,Specifier);
                                        }
                                else    {
                                        ulonglong_t uValue;
                                        uValue = (ulonglong_t)Value;
                                        PrintNumberW(uValue,false,Specifier);
                                        }
                                break;
                                }
                        case 'f':                   // Floating point.
                        case 'F':
                        case 'g':
                        case 'e':
                        case 'E':
                                {
                                double Value = va_arg(Params,double);
                                PrintFloatW(Value,Specifier);
                                break;
                                }
                        case 'p':                   // Pointer.
                                {
                                size_t Ptr = (size_t)va_arg(Params,void *);
                                Specifier.Radix = HEXRadix;
                                PrintNumberW(Ptr,false,Specifier);
                                break;
                                }
                        case '%':                   // Percent.
                                {
                                wchar_t String[2];
                                String[0] = L'%';
                                String[1] = '\0';
                                PrintStringW(String,Specifier);
                                break;
                                }
                        case 'n':   // Assigns characters written so far.
                                {
                                int *CharsWritten = va_arg(Params,int *);
                                if (CharsWritten)
                                        {
                                        size_t nWritten = BCB.Index - Index0;
                                        *CharsWritten = (int)nWritten;
                                        }
                                break;
                                }
                        default: return PRINTF_EOF;
                        }
                }
        WriteBufferW(L'\0');                    // String terminator.
        size_t nBytes = BCB.Index - Index0;
        size_t nChars = nBytes / sizeof(wchar_t);
        return (int)nChars;
        }

// ***************************************************************************
// **** Memory Printf Interface **********************************************
// ***************************************************************************

MemoryPrintf::MemoryPrintf()
        {
        PrintfIMPL = new MemoryPrintfIMPL(*this);
        return;
        }

MemoryPrintf::~MemoryPrintf()
        {
        if (PrintfIMPL) 
                {
                MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
                delete IMPL;
                }
        return;
        }

char* MemoryPrintf::AquireBuffer()
        {
        if (!PrintfIMPL) return NULL;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        return IMPL->AquireBuffer();
        }

void MemoryPrintf::_AttachBuffer(char *Buffer, size_t Size)
        {
        if (!PrintfIMPL) return;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        IMPL->_AttachBuffer(Buffer,Size);
        return;
        }

void MemoryPrintf::Clear()
        {
        if (!PrintfIMPL) return;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        IMPL->Clear();
        return;
        }

const char* MemoryPrintf::GetBuffer()
        {
        if (!PrintfIMPL) return NULL;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        return IMPL->GetBuffer();
        }

void MemoryPrintf::FreeBuffer(void *Buffer)
        {
        if (Buffer) free(Buffer);
        return;
        }

void* MemoryPrintf::ReallocBuffer(void *Buffer, size_t Size)
        {
        return realloc(Buffer,Size);
        }

void MemoryPrintf::SetUTF8(bool CharUTF8, bool WideUTF8)
        {
        if (!PrintfIMPL) return;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        IMPL->SetUTF8(CharUTF8,WideUTF8);
        return;
        }

int MemoryPrintf::fputs(const char *String)
        {
        if (!PrintfIMPL) return -1;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        return IMPL->fputs(String);
        }

int MemoryPrintf::fputws(const wchar_t *String)
        {
        if (!PrintfIMPL) return -1;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        return IMPL->fputws(String);
        }

int MemoryPrintf::printf(const char *Format, ...)
        {
        va_list Params;
        if (!PrintfIMPL) return -1;
        va_start(Params,Format);
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        int rc = IMPL->vprintf(Format,Params);
        va_end(Params);
        return rc;
        }

int MemoryPrintf::wprintf(const wchar_t *Format, ...)
        {
        va_list Params;
        if (!PrintfIMPL) return -1;
        va_start(Params,Format);
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        int rc = IMPL->vwprintf(Format,Params);
        va_end(Params);
        return rc;
        }

int MemoryPrintf::puts(const char *String)
        {
        return fputs(String) + fputs("\n");
        }

int MemoryPrintf::putws(const wchar_t *String)
        {
        return fputws(String) + fputws(L"\n");
        }

int MemoryPrintf::vprintf(const char *Format, va_list Args)
        {
        if (!PrintfIMPL) return -1;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        return IMPL->vprintf(Format,Args);
        }

int MemoryPrintf::vwprintf(const wchar_t *Format, va_list Args)
        {
        if (!PrintfIMPL) return -1;
        MemoryPrintfIMPL *IMPL = (MemoryPrintfIMPL *)PrintfIMPL;
        return IMPL->vwprintf(Format,Args);
        }

// ***************************************************************************
// **** Dimensioning MemoryPrintf ********************************************
// ***************************************************************************

DimMemoryPrintf::DimMemoryPrintf() 
        {
        MinSize = 0; 
        WriteMode = false;
        return;
        }

void* DimMemoryPrintf::ReallocBuffer(void *Buffer, size_t Size)
        {
        if (!WriteMode) 
                {
                if (MinSize < Size) MinSize = Size;
                return NULL;
                }
        if (Size < MinSize) Size = MinSize;
        Buffer = realloc(Buffer,Size);
        return Buffer;
        }

void DimMemoryPrintf::SetWriteClear(bool Mode)
        {    
        if (!Mode) MinSize = 0;
        WriteMode = Mode;
        Clear();
        return;
        }

// ***************************************************************************
// **************************** End of File **********************************
// ***************************************************************************
