// EBCDIC to ASCII to UNICODE Conversions: ebcdic.H
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ebcdic.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifdef __cplusplus
        #include <stdlib.h>
        #include <string.h>
        #include "Errors.h"
#endif

#include <ebcdic.h>
#include <LevelTrace.h>

// ***************************************************************************
// **** ASCII <-> EBCDIC (Latin 1) *******************************************
// ***************************************************************************

const unsigned ebcdic2ascii[] =
        {
        0x00, 0x01, 0x02, 0x03, 0x9c, 0x09, 0x86, 0x7f,
        0x97, 0x8d, 0x8e, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x9d, 0x85, 0x08, 0x87,
        0x18, 0x19, 0x92, 0x8f, 0x1c, 0x1d, 0x1e, 0x1f,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x0a, 0x17, 0x1b,
        0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x05, 0x06, 0x07,
        0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04,
        0x98, 0x99, 0x9a, 0x9b, 0x14, 0x15, 0x9e, 0x1a,
        0x20, 0xa0, 0xe2, 0xe4, 0xe0, 0xe1, 0xe3, 0xe5,
        0xe7, 0xf1, 0x5b, 0x2e, 0x3c, 0x28, 0x2b, 0x21,
        0x26, 0xe9, 0xea, 0xeb, 0xe8, 0xed, 0xee, 0xef,
        0xec, 0xdf, 0x5d, 0x24, 0x2a, 0x29, 0x3b, 0x5e,
        0x2d, 0x2f, 0xc2, 0xc4, 0xc0, 0xc1, 0xc3, 0xc5,
        0xc7, 0xd1, 0xa6, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,
        0xf8, 0xc9, 0xca, 0xcb, 0xc8, 0xcd, 0xce, 0xcf,
        0xcc, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,
        0xd8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0xab, 0xbb, 0xf0, 0xfd, 0xfe, 0xb1,
        0xb0, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
        0x71, 0x72, 0xaa, 0xba, 0xe6, 0xb8, 0xc6, 0xa4,
        0xb5, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0xa1, 0xbf, 0xd0, 0xdd, 0xde, 0xae,
        0xa2, 0xa3, 0xa5, 0xb7, 0xa9, 0xa7, 0xb6, 0xbc,
        0xbd, 0xbe, 0xac, 0x7c, 0xaf, 0xa8, 0xb4, 0xd7,
        0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0xad, 0xf4, 0xf6, 0xf2, 0xf3, 0xf5,
        0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
        0x51, 0x52, 0xb9, 0xfb, 0xfc, 0xf9, 0xfa, 0xff,
        0x5c, 0xf7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0xb2, 0xd4, 0xd6, 0xd2, 0xd3, 0xd5,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0xb3, 0xdb, 0xdc, 0xd9, 0xda, 0x9f
        };

const unsigned ascii2ebcdic[] =
        {
        0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f,
        0x16, 0x05, 0x25, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26,
        0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
        0x40, 0x4f, 0x7f, 0x7b, 0x5b, 0x6c, 0x50, 0x7d,
        0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
        0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
        0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
        0xe7, 0xe8, 0xe9, 0x4a, 0xe0, 0x5a, 0x5f, 0x6d,
        0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
        0xa7, 0xa8, 0xa9, 0xc0, 0xbb, 0xd0, 0xa1, 0x07,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x15, 0x06, 0x17,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x09, 0x0a, 0x1b,
        0x30, 0x31, 0x1a, 0x33, 0x34, 0x35, 0x36, 0x08,
        0x38, 0x39, 0x3a, 0x3b, 0x04, 0x14, 0x3e, 0xff,
        0x41, 0xaa, 0xb0, 0xb1, 0x9f, 0xb2, 0x6a, 0xb5,
        0xbd, 0xb4, 0x9a, 0x8a, 0xba, 0xca, 0xaf, 0xbc,
        0x90, 0x8f, 0xea, 0xfa, 0xbe, 0xa0, 0xb6, 0xb3,
        0x9d, 0xda, 0x9b, 0x8b, 0xb7, 0xb8, 0xb9, 0xab,
        0x64, 0x65, 0x62, 0x66, 0x63, 0x67, 0x9e, 0x68,
        0x74, 0x71, 0x72, 0x73, 0x78, 0x75, 0x76, 0x77,
        0xac, 0x69, 0xed, 0xee, 0xeb, 0xef, 0xec, 0xbf,
        0x80, 0xfd, 0xfe, 0xfb, 0xfc, 0xad, 0xae, 0x59,
        0x44, 0x45, 0x42, 0x46, 0x43, 0x47, 0x9c, 0x48,
        0x54, 0x51, 0x52, 0x53, 0x58, 0x55, 0x56, 0x57,
        0x8c, 0x49, 0xcd, 0xce, 0xcb, 0xcf, 0xcc, 0xe1,
        0x70, 0xdd, 0xde, 0xdb, 0xdc, 0x8d, 0x8e, 0xdf
        };

void emem2amem(void *Dest, const void *Src, size_t Length)
{
	while (Length--)
                {
                *(char*)Dest = (char)ebcdic2ascii[*(unsigned char*)Src];
                Dest = (char *)Dest + 1;
                Src = (char *)Src + 1;
                }
	return;
}

void estr2astr(char *Dest, const char *Src)
{
	while (*Src) *Dest++ = (char)ebcdic2ascii[*(unsigned char *)Src++];
	*Dest = 0;
	return;
}

void amem2emem(void *Dest, const void *Src, size_t Length)
{
	while (Length--)
                {
                *(char*)Dest = ascii2ebcdic[*(unsigned char *)Src];
                Dest = (char *)Dest + 1;
                Src = (char *)Src + 1;
                }
	return;
}

void astr2estr(char *Dest, const char *Src)
{
	while (*Src) *Dest++ = ascii2ebcdic[*(unsigned char *)Src++];
	*Dest = 0;
	return;
}

// ***************************************************************************
// **** ASCII <-> UNICODE (Latin1) *******************************************
// ***************************************************************************

void umem2amem(void *Dest, const void *Src, size_t Length)
{
	while (Length--)
                {
                *(char*)Dest = (char)*(wchar_t *)Src;
                Dest = (char *)Dest + 1;
                Src = (wchar_t *)Src + 1;
                }
	return;
}

void ustr2astr(char *Dest, const wchar_t *Src)
{
	while (*Src) *Dest++ = (char)*Src++;
	*Dest = 0;
	return;
}

void amem2umem(void *Dest, const void *Src, size_t Length)
{
	while (Length--)
                {
                *(wchar_t*)Dest = (wchar_t)*(unsigned char *)Src;
                Dest = (wchar_t *)Dest + 1;
                Src = (char *)Src + 1;
                }
	return;
}

void astr2ustr(wchar_t *Dest, const char *Src)
{
	while (*Src) *Dest++ = (wchar_t)*(unsigned char *)Src++;
	*Dest = 0;
	return;
}

#ifdef __cplusplus

// ***************************************************************************
// **** ASCII <-> EBCDIC *****************************************************
// ***************************************************************************

A2E::A2E(const char *ASCIIString)
        {
        if (!ASCIIString) 
                {
                EBCDICString = NULL;
                return;
                }
        EBCDICString = (char *)malloc(strlen(ASCIIString)+1);
        if (!EBCDICString) throw Error_Memory("A2E");
        astr2estr(EBCDICString,ASCIIString);
        return;
        }

A2E::A2E(const char *ASCIIString, size_t Length)
        {
        if (!ASCIIString || !Length) 
                {
                EBCDICString = NULL;
                return;
                }
        EBCDICString = (char *)malloc(Length);
        if (!EBCDICString) throw Error_Memory("A2E");
        amem2emem(EBCDICString,ASCIIString,Length);
        return;
        }

A2E::A2E(char *EBCDICDest, const char *ASCIIString, size_t Length)
        {
        EBCDICString = NULL;                            // Behave like
        amem2emem(EBCDICDest,ASCIIString,Length);       // a function.
        return;
        }

A2E::~A2E()
        {
        if (EBCDICString) free(EBCDICString);
        return;
        }

E2A::E2A(const char *EBCDICString)
        {
        if (!EBCDICString) 
                {
                ASCIIString = NULL;
                return;
                }
        ASCIIString = (char *)malloc(strlen(EBCDICString)+1);
        if (!ASCIIString) throw Error_Memory("E2A");
        estr2astr(ASCIIString,EBCDICString);
        return;
        }

E2A::E2A(const char *EBCDICString, size_t Length)
        {
        if (!EBCDICString || !Length) 
                {
                ASCIIString = NULL;
                return;
                }
        ASCIIString = (char *)malloc(Length);
        if (!ASCIIString) throw Error_Memory("E2A");
        emem2amem(ASCIIString,EBCDICString,Length);
        return;
        }

E2A::E2A(char *ASCIIDest, const char *EBCDICString, size_t Length)
        {
        ASCIIString = NULL;                             // Behave like
        emem2amem(ASCIIDest,EBCDICString,Length);       // a function.
        return;
        }

E2A::~E2A()
        {
        if (ASCIIString) free(ASCIIString);
        return;
        }

// ***************************************************************************
// **** ASCII <-> UNICODE (Latin 1) ******************************************
// ***************************************************************************

A2U::A2U(const char *ASCIIString)
        {
        if (!ASCIIString) 
                {
                UNICODEString = NULL;
                return;
                }
        UNICODEString = (wchar_t *)malloc(sizeof(wchar_t) *
                                          (strlen(ASCIIString)+1));
        if (!UNICODEString) throw Error_Memory("A2U");
        astr2ustr(UNICODEString,ASCIIString);
        return;
        }

A2U::A2U(const char *ASCIIString, size_t Length)
        {
        if (!ASCIIString || !Length) 
                {
                UNICODEString = NULL;
                return;
                }
        UNICODEString = (wchar_t *)malloc(Length * sizeof(wchar_t));
        if (!UNICODEString) throw Error_Memory("A2U");
        amem2umem(UNICODEString,ASCIIString,Length);
        return;
        }

A2U::A2U(wchar_t *UNICODEDest, const char *ASCIIString, size_t Length)
        {
        UNICODEString = NULL;                           // Behave like
        amem2umem(UNICODEDest,ASCIIString,Length);      // a function.
        return;
        }

A2U::~A2U()
        {
        if (UNICODEString) free(UNICODEString);
        return;
        }

U2A::U2A(const wchar_t *UNICODEString)
        {
        if (!UNICODEString) 
                {
                ASCIIString = NULL;
                return;
                }
        ASCIIString = (char *)malloc(wcslen(UNICODEString)+1);
        if (!ASCIIString) throw Error_Memory("U2A");
        ustr2astr(ASCIIString,UNICODEString);
        return;
        }

U2A::U2A(const wchar_t *UNICODEString, size_t Length)
        {
        if (!UNICODEString || !Length) 
                {
                ASCIIString = NULL;
                return;
                }
        ASCIIString = (char *)malloc(Length);
        if (!ASCIIString) throw Error_Memory("U2A");
        umem2amem(ASCIIString,UNICODEString,Length);
        return;
        }

U2A::U2A(char *ASCIIDest, const wchar_t *UNICODEString, size_t Length)
        {
        ASCIIString = NULL;                             // Behave like
        umem2amem(ASCIIDest,UNICODEString,Length);      // a function.
        return;
        }

U2A::~U2A()
        {
        if (ASCIIString) free(ASCIIString);
        return;
        }

#endif /* __cplusplus */

// ***************************************************************************
// **** Unicode <-> Unicode **************************************************
// ***************************************************************************

// ***********************
// **** CESU8_to_UCS2 ****
// ***********************

size_t CESU8_to_UCS2(u2char_t *Dest, 
                     size_t DestLen,
                     const char *Source, 
                     size_t SourceLen)
        {
        static const char *ProcName = "CESU8_to_UCS2";
        size_t RequiredLen = 0;
        while (SourceLen)
                {
                u2char_t DChar;
                unsigned SChar0 = *(const unsigned char*)Source++;
                SourceLen--;
                if (SChar0 < 0x80) DChar = SChar0;
                else if (SChar0 > 0xBF && SChar0 < 0xE0)
                        {
                        if (SourceLen < 1) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        SourceLen--;
                        DChar = (u2char_t)(((SChar0 & 0x1F) << 6) | 
                                            (SChar1 & 0x3F));
                        }
                else    {
                        if (SourceLen < 2) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        unsigned SChar2 = *(const unsigned char*)Source++;
                        SourceLen -= 2;
                        DChar = (u2char_t)(((SChar0 & 0x0F) << 12) | 
                                           ((SChar1 & 0x3F) << 6) | 
                                            (SChar2 & 0x3F));
                        }
                RequiredLen++;
                if (RequiredLen <= DestLen) *Dest++ = DChar;
                }
        return RequiredLen;
        }

// **********************
// **** UCS2_to_CESU8 ****
// **********************

size_t UCS2_to_CESU8(char *Dest, 
                     size_t DestLen, 
                     const u2char_t *Source, 
                     size_t SourceLen)
        {
        static const char *ProcName = "UCS2_to_CESU8";
        size_t RequiredLen = 0;
        while (SourceLen)
                {
                unsigned SChar = *(const unsigned short*)Source++;
                SourceLen--;
                if (SChar < 0x80) 
                        {
                        RequiredLen++;
                        if (RequiredLen <= DestLen) *Dest++ = (char)SChar;
                        }
                else if (SChar > 0x7F && SChar < 0x800)
                        {
                        RequiredLen += 2;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (char)((SChar >> 6) | 0xC0);
                                *Dest++ = (char)((SChar & 0x3F) | 0x80);
                                }
                        }
                else    {
                        RequiredLen += 3;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (char)((SChar >> 12) | 0xE0);
                                *Dest++ = (char)(((SChar >> 6) & 0x3F) | 0x80);
                                *Dest++ = (char)((SChar & 0x3F) | 0x80);
                                }
                        }
                }
        return RequiredLen;
        }

// ************************
// **** UTF16_to_UTF32 ****
// ************************

size_t UTF16_to_UTF32(u4char_t *Dest, 
                      size_t DestLen, 
                      const u2char_t *Source, 
                      size_t SourceLen)
        {
        static const char *ProcName = "UTF16_to_UTF32";
        size_t RequiredLen = 0;
        while (SourceLen)
                {
                unsigned SChar0 = *(const unsigned short*)Source++;
                SourceLen--;
                if (SChar0 < 0xD800 || SChar0 > 0xDFFF) 
                        {
                        RequiredLen++;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (u4char_t)SChar0;
                                }
                        }
                else if (SChar0 > 0xDBFF)
                        {
                        TERROR(("%s: Invalid Leading: %X",ProcName,SChar0));
                        return 0;
                        }
                else    {
                        if (SourceLen < 1) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned short*)Source++;
                        SourceLen--;
                        RequiredLen++;
                        if (RequiredLen <= DestLen)
                                {
                                unsigned High = (SChar0 - 0xD800) << 10;
                                unsigned Low = (SChar1 - 0xDC00);
                                *Dest++ = (u4char_t)((High | Low) + 0x10000);
                                }
                        }
                }
        return RequiredLen;
        }

// ************************
// **** UTF32_to_UTF16 ****
// ************************

size_t UTF32_to_UTF16(u2char_t *Dest, 
                      size_t DestLen, 
                      const u4char_t *Source, 
                      size_t SourceLen)
        {
        static const char *ProcName = "UTF32_to_UTF16";
        size_t RequiredLen = 0;
        while (SourceLen)
                {
                unsigned SChar = *(const unsigned*)Source++;
                SourceLen--;
                if (SChar < 0x10000)
                        {
                        RequiredLen++;
                        if (RequiredLen <= DestLen) *Dest++ = (u2char_t)SChar;
                        }
                else if (SChar > 0x10FFFF)
                        {
                        TERROR(("%s: Invalid Character (%X)",ProcName,SChar));
                        return 0;
                        }
                else    {
                        unsigned TwentyBits = SChar - 0x10000;
                        unsigned High = TwentyBits >> 10;
                        unsigned Low = TwentyBits & 0x3FF;
                        RequiredLen += 2;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (u2char_t)(0xD800 + High);
                                *Dest++ = (u2char_t)(0xDC00 + Low);
                                }
                        }
                }
        return RequiredLen;
        }

// ***********************
// **** UTF8_to_UTF32 ****
// ***********************

size_t UTF8_to_UTF32(u4char_t *Dest, 
                     size_t DestLen,
                     const char *Source, 
                     size_t SourceLen)
        {
        static const char *ProcName = "UTF8_to_UTF32";
        size_t RequiredLen = 0;
        while (SourceLen)
                {
                u4char_t DChar;
                unsigned SChar0 = *(const unsigned char*)Source++;
                SourceLen--;
                if (SChar0 < 0x80) DChar = SChar0;
                else if (SChar0 > 0xBF && SChar0 < 0xE0)
                        {
                        if (SourceLen < 1) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        SourceLen--;
                        DChar = (u4char_t)(((SChar0 & 0x1F) << 6) | 
                                            (SChar1 & 0x3F));
                        }
                else if (SChar0 > 0xDF && SChar0 < 0xF0)
                        {
                        if (SourceLen < 2) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        unsigned SChar2 = *(const unsigned char*)Source++;
                        SourceLen -= 2;
                        DChar = (u4char_t)(((SChar0 & 0x0F) << 12) | 
                                           ((SChar1 & 0x3F) << 6) | 
                                            (SChar2 & 0x3F));
                        }
                else    {
                        if (SourceLen < 3) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        unsigned SChar2 = *(const unsigned char*)Source++;
                        unsigned SChar3 = *(const unsigned char*)Source++;
                        SourceLen -= 3;
                        DChar = (u4char_t)(((SChar0 & 0x0F) << 18) | 
                                           ((SChar1 & 0x3F) << 12) | 
                                           ((SChar2 & 0x3F) << 6) | 
                                            (SChar3 & 0x3F));
                        }
                RequiredLen++;
                if (RequiredLen <= DestLen) *Dest++ = DChar;
                }
        return RequiredLen;
        }

// ***********************
// **** UTF32_to_UTF8 ****
// ***********************

size_t UTF32_to_UTF8(char *Dest, 
                     size_t DestLen, 
                     const u4char_t *Source, 
                     size_t SourceLen)
        {
        static const char *ProcName = "UTF32_to_UTF8";
        size_t RequiredLen = 0;
        while (SourceLen)
               {
               unsigned SChar = *(const unsigned*)Source++;
               SourceLen--;
               if (SChar < 0x80) 
                      {
                      RequiredLen++;
                      if (RequiredLen <= DestLen) *Dest++ = (char)SChar;
                      }
               else if (SChar > 0x7F && SChar < 0x800)
                      {
                      RequiredLen += 2;
                      if (RequiredLen <= DestLen)
                              {
                              *Dest++ = (char)((SChar >> 6) | 0xC0);
                              *Dest++ = (char)((SChar & 0x3F) | 0x80);
                              }
                      }
               else if (SChar < 0x10000)   
                      {
                      RequiredLen += 3;
                      if (RequiredLen <= DestLen)
                              {
                              *Dest++ = (char)((SChar >> 12) | 0xE0);
                              *Dest++ = (char)(((SChar >> 6) & 0x3F) | 0x80);
                              *Dest++ = (char)((SChar & 0x3F) | 0x80);
                              }
                      }
               else   {
                      RequiredLen += 4;
                      if (RequiredLen <= DestLen)
                              {
                              *Dest++ = (char)((SChar >> 18) | 0xF0);
                              *Dest++ = (char)(((SChar >> 12) & 0x3F) | 0x80);
                              *Dest++ = (char)(((SChar >> 6) & 0x3F) | 0x80);
                              *Dest++ = (char)((SChar & 0x3F) | 0x80);
                              }
                      }
               }
        return RequiredLen;
        }

// ***********************
// **** UTF8_to_UTF16 ****
// ***********************

size_t UTF8_to_UTF16(u2char_t *Dest, 
                     size_t DestLen,
                     const char *Source,
                     size_t SourceLen)
        {
        static const char *ProcName = "UTF8_to_UTF16";
        size_t RequiredLen = 0;
        while (SourceLen)
                {
                unsigned SChar0 = *(const unsigned char*)Source++;
                SourceLen--;
                if (SChar0 < 0x80) 
                        {
                        RequiredLen++;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (u2char_t)SChar0;
                                }
                        }
                else if (SChar0 > 0xBF && SChar0 < 0xE0)
                        {
                        if (SourceLen < 1) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        SourceLen--;
                        RequiredLen++;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (u2char_t)(((SChar0 & 0x1F) << 6) |
                                                      (SChar1 & 0x3F));
                                }
                        }
                else if (SChar0 > 0xDF && SChar0 < 0xF0)
                        {
                        if (SourceLen < 2) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        unsigned SChar2 = *(const unsigned char*)Source++;
                        SourceLen -= 2;
                        RequiredLen++;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (u2char_t)(((SChar0 & 0x0F) << 12) |
                                                     ((SChar1 & 0x3F) << 6) |
                                                      (SChar2 & 0x3F));
                                }
                        }
                else    {
                        if (SourceLen < 3) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned char*)Source++;
                        unsigned SChar2 = *(const unsigned char*)Source++;
                        unsigned SChar3 = *(const unsigned char*)Source++;
                        SourceLen -= 3;
                        unsigned SChar = ((SChar0 & 0x0F) << 18) | 
                                         ((SChar1 & 0x3F) << 12) | 
                                          ((SChar2 & 0x3F) << 6) | 
                                           (SChar3 & 0x3F);
                        unsigned TwentyBits = SChar - 0x10000;
                        unsigned High = TwentyBits >> 10;
                        unsigned Low = TwentyBits & 0x3FF;
                        RequiredLen += 2;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (u2char_t)(0xD800 + High);
                                *Dest++ = (u2char_t)(0xDC00 + Low);
                                }
                        }
                }
        return RequiredLen;
        }

// ***********************
// **** UTF16_to_UTF8 ****
// ***********************

size_t UTF16_to_UTF8(char *Dest, 
                     size_t DestLen,
                     const u2char_t *Source,
                     size_t SourceLen)
        {
        static const char *ProcName = "UTF16_to_UTF8";
        size_t RequiredLen = 0;
        while (SourceLen)
                {
                unsigned SChar0 = *(const unsigned short*)Source++;
                SourceLen--;
                if (SChar0 < 0x80) 
                        {
                        RequiredLen++;
                        if (RequiredLen <= DestLen) *Dest++ = (char)SChar0;
                        }
                else if (SChar0 > 0x7F && SChar0 < 0x800)
                        {
                        RequiredLen += 2;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (char)((SChar0 >> 6) | 0xC0);
                                *Dest++ = (char)((SChar0 & 0x3F) | 0x80);
                                }
                        }
                else if (SChar0 < 0xD800 || SChar0 > 0xDFFF)
                        {
                        RequiredLen += 3;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (char)((SChar0 >> 12) | 0xE0);
                                *Dest++ = (char)(((SChar0 >> 6) & 0x3F) | 0x80);
                                *Dest++ = (char)((SChar0 & 0x3F) | 0x80);
                                }
                        }
                else    {
                        if (SourceLen < 1) 
                                {
                                TERROR(("%s: Improper Length",ProcName));
                                return 0;
                                }
                        unsigned SChar1 = *(const unsigned short*)Source++;
                        SourceLen--;
                        unsigned High = (SChar0 - 0xD800) << 10;
                        unsigned Low = (SChar1 - 0xDC00);
                        unsigned SChar = (High | Low) + 0x10000;
                        RequiredLen += 4;
                        if (RequiredLen <= DestLen)
                                {
                                *Dest++ = (char)((SChar >> 18) | 0xF0);
                                *Dest++ = (char)(((SChar >> 12) & 0x3F) | 0x80);
                                *Dest++ = (char)(((SChar >> 6) & 0x3F) | 0x80);
                                *Dest++ = (char)((SChar & 0x3F) | 0x80);
                                }
                        }
                }
        return RequiredLen;
        }

// ***************************************************************************
// **** WCHAR <-> UTF8 *******************************************************
// ***************************************************************************

UTF82W::UTF82W(const char *UTF8String)
        {
        if (!UTF8String) 
                {
                WCHARString = NULL;
                return;
                }
        size_t Length = strlen(UTF8String);
        WCHARString = mallocW(Length+1);
        if (!WCHARString) throw Error_Memory("UTF82W");
        size_t n = UTF8_to_WCHAR(WCHARString,Length,UTF8String,Length);
        WCHARString[n] = L'\0';
        return;
        }

UTF82W::UTF82W(const char *UTF8String, size_t Length)
        {
        if (!UTF8String || !Length) 
                {
                WCHARString = NULL;
                return;
                }
        WCHARString = mallocW(Length+1);
        if (!WCHARString) throw Error_Memory("UTF82W");
        size_t n = UTF8_to_WCHAR(WCHARString,Length,UTF8String,Length);
        WCHARString[n] = L'\0';
        return;
        }

UTF82W::~UTF82W()
        {
        if (WCHARString) free(WCHARString);
        return;
        }

W2UTF8::W2UTF8(const wchar_t *WCHARString)
        {
        if (!WCHARString) 
                {
                UTF8String = NULL;
                return;
                }
        size_t Length = wcslen(WCHARString);
        size_t UTF8Length = 4 * Length;
        UTF8String = (char *)malloc(UTF8Length+1);
        if (!UTF8String) throw Error_Memory("W2UTF8");
        size_t n = WCHAR_to_UTF8(UTF8String,UTF8Length,WCHARString,Length);
        UTF8String[n] = '\0';
        return;
        }

W2UTF8::W2UTF8(const wchar_t *WCHARString, size_t Length)
        {
        if (!WCHARString || !Length) 
                {
                UTF8String = NULL;
                return;
                }
        size_t UTF8Length = 4 * Length;
        UTF8String = (char *)malloc(UTF8Length+1);
        if (!UTF8String) throw Error_Memory("W2UTF8");
        size_t n = WCHAR_to_UTF8(UTF8String,UTF8Length,WCHARString,Length);
        UTF8String[n] = '\0';
        return;
        }

W2UTF8::~W2UTF8()
        {
        if (UTF8String) free(UTF8String);
        return;
        }

// ***************************************************************************
// **************************** End of File **********************************
// ***************************************************************************
