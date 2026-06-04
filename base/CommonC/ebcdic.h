///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: ebcdic.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.                                                
//                    
// Author(s) : Eric Kass 
//
///////////////////////////////////////////////////////////////////////////////
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

inline wchar_t a2u(char a) {return (wchar_t)a;}
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

// ***************************
// **** ASCII <-> UNICODE ****
// ***************************

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

/* ***************************************************************************
 Notes:


******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

*****************************************************************************/

#endif /* _EBCDIC_Conversions_ */
