// EBCDIC to ASCII to UNICODE Conversions: ebcdic.H
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ebcdic.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef _EBCDIC_Conversions_

        #define _EBCDIC_Conversions_

#include <stddef.h>
#ifndef _WIN32
        #include <wchar.h>
#endif

// **************************
// **** ASCII <-> EBCDIC ****
// **************************

extern const unsigned ebcdic2ascii[];

inline char e2a(char e)
        {
	return (char)ebcdic2ascii[(unsigned char)e];
        }

void estr2astr(char *Dest, const char *Src);
void emem2amem(void *Dest, const void *Src, size_t Length);

extern const unsigned ascii2ebcdic[];

inline char a2e(char a)
        {
	return (char)ascii2ebcdic[(unsigned char)a];
        }

void astr2estr(char *Dest, const char *Src);
void amem2emem(void *Dest, const void *Src, size_t Length);

// ************************************
// **** ASCII <-> UNICODE (Latin1) ****
// ************************************

inline char u2a(wchar_t u) {return (char)u;}
void ustr2astr(char *Dest, const wchar_t *Src);
void umem2amem(void *Dest, const void *Src, size_t Length);

inline wchar_t a2u(char a) {return (wchar_t)(unsigned char)a;}
void astr2ustr(wchar_t *Dest, const char *Src);
void amem2umem(void *Dest, const void *Src, size_t Length);

#ifdef __cplusplus

// **************************************************************************
// 
// Use the following for INPLACE conversion:
//
//      For example: 
//
//              Test(A2E(MyString));
//
//              A2E get_BrownFox_EBCDIC()
//                      {
//                      return "Brown Fox";
//                      }
//
// DO NOT DO:
// ----------
//
//      char *MyStringE = E2A(MyStringA);
//      Test(MyStringE);
//
// **************************************************************************

// **************************
// **** ASCII <-> EBCDIC ****
// **************************

class A2E
        {
        private:
                char* EBCDICString;
        public:
                A2E(const char *ASCIIString);
                A2E(const char *ASCIIString, size_t Length);
                A2E(char *EBCDICDest, const char *ASCIIString, size_t Length);
                ~A2E();
                operator char* () {return EBCDICString;}
                operator const char* () {return EBCDICString;}
        };

class E2A
        {
        private:
                char* ASCIIString;
        public:
                E2A(const char *EBCDICString);
                E2A(const char *EBCDICString, size_t Length);
                E2A(char *ASCIIDest, const char *EBCDICString, size_t Length);
                ~E2A();
                operator char* () {return ASCIIString;}
                operator const char* () {return ASCIIString;}
        };

// ************************************
// **** ASCII <-> UNICODE (Latin1) ****
// ************************************

class A2U
        {
        private:
                wchar_t* UNICODEString;
        public:
                A2U(const char *ASCIIString);
                A2U(const char *ASCIIString, size_t Length);
                A2U(wchar_t *UNICODEDest, const char *ASCII, size_t Length);
                ~A2U();
                operator wchar_t* () {return UNICODEString;}
                operator const wchar_t* () {return UNICODEString;}
        };

class U2A
        {
        private:
                char* ASCIIString;
        public:
                U2A(const wchar_t *UNICODEString);
                U2A(const wchar_t *UNICODEString, size_t Length);
                U2A(char *ASCIIDest, const wchar_t *UNICODE, size_t Length);
                ~U2A();
                operator char* () {return ASCIIString;}
                operator const char* () {return ASCIIString;}
        };

#endif /* __cplusplus */

// ***************************************************************************
// **** Unicode <-> Unicode **************************************************
// **************************
//
//      INPUT: Length in array units (e.g. sizeof(Array)/sizeof(u2char_t)).
//      RETURN: Length of destination (in array units).
//
// ***************************************************************************

#ifndef U2CHAR_IS_WCHAR
        #if defined(_WIN32) || defined(__OS400_TGTVRM__)
                typedef wchar_t u2char_t;
                #define U2CHAR_IS_WCHAR 1
        #else
                typedef short u2char_t;
                #define U2CHAR_IS_WCHAR 0
        #endif
#endif

#ifndef U4CHAR_IS_WCHAR
        #if defined(_WIN32)
                typedef int u4char_t;
                #define U4CHAR_IS_WCHAR 0
        #elif defined(__64BIT__) || __WORDSIZE == 64
                typedef wchar_t u4char_t;
                #define U4CHAR_IS_WCHAR 1
        #else
                typedef int u4char_t;
                #define U4CHAR_IS_WCHAR 0
        #endif
#endif

#if U4CHAR_IS_WCHAR
        #define WCHAR_to_UTF8(a,b,c,d)                                      \
                UTF32_to_UTF8((a),(b),(const u4char_t*)(c),(d))
        #define UTF8_to_WCHAR(a,b,c,d)                                      \
                UTF8_to_UTF32((u4char_t*)(a),(b),(c),(d))
#else
        #define WCHAR_to_UTF8(a,b,c,d)                                      \
                UTF16_to_UTF8((a),(b),(const u2char_t*)(c),(d))
        #define UTF8_to_WCHAR(a,b,c,d)                                      \
                UTF8_to_UTF16((u2char_t*)(a),(b),(c),(d))
#endif

size_t CESU8_to_UCS2(u2char_t *Dest, 
                     size_t DestLen,
                     const char *Source, 
                     size_t SourceLen);

size_t UCS2_to_CESU8(char *Dest,
                     size_t DestLen, 
                     const u2char_t *Source, 
                     size_t SourceLen);

size_t UTF16_to_UTF32(u4char_t *Dest, 
                      size_t DestLen, 
                      const u2char_t *Source, 
                      size_t SourceLen);

size_t UTF32_to_UTF16(u2char_t *Dest, 
                      size_t DestLen, 
                      const u4char_t *Source, 
                      size_t SourceLen);

size_t UTF8_to_UTF32(u4char_t *Dest, 
                     size_t DestLen,
                     const char *Source, 
                     size_t SourceLen);

size_t UTF32_to_UTF8(char *Dest, 
                     size_t DestLen, 
                     const u4char_t *Source, 
                     size_t SourceLen);

size_t UTF16_to_UTF8(char *Dest,
                     size_t DestLen,
                     const u2char_t *Source,
                     size_t SourceLen);

size_t UTF8_to_UTF16(u2char_t *Dest, 
                     size_t DestLen,
                     const char *Source,
                     size_t SourceLen);

#ifdef __cplusplus

// **************************************************************************
// 
// Use the following for INPLACE conversion:
//
//      For example: 
//
//              Test(W2UTF8(MyStringW));
//          or
//              W2UTF8 MyStringUTF8(MyStringW)
//
// DO NOT DO:
// ----------
//
//      char *MyStringUTF8 = W2UTF8(MyStringW);
//      Test(MyStringUTF8);
//
// **************************************************************************

// ************************
// **** WCHAR <-> UTF8 ****
// ************************

class UTF82W
        {
        private:
                wchar_t* WCHARString;
        public:
                UTF82W(const char *UTF8String);
                UTF82W(const char *UTF8String, size_t Length);
                ~UTF82W();
                operator wchar_t* () {return WCHARString;}
                operator const wchar_t* () {return WCHARString;}
        };

class W2UTF8
        {
        private:
                char* UTF8String;
        public:
                W2UTF8(const wchar_t *WCHARString);
                W2UTF8(const wchar_t *WCHARString, size_t Length);
                ~W2UTF8();
                operator char* () {return UTF8String;}
                operator const char* () {return UTF8String;}
        };

#endif /* __cplusplus */

// ***************************************************************************
// **** API with Wide Character Semantics ************************************
// ***************************************************************************

#define mallocW(x) (wchar_t *)malloc((x) * sizeof(wchar_t))
#define memcpyW(a,b,c) memcpy((a),(b),(c) * sizeof(wchar_t))
#define memmoveW(a,b,c) memmove((a),(b),(c) * sizeof(wchar_t))

#define sizeofW(a) (sizeof(a) / sizeof(wchar_t))

/* ***************************************************************************
 Notes:


******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

*****************************************************************************/

#endif /* _EBCDIC_Conversions_ */
