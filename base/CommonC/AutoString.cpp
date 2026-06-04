// AutoString.C
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: AutoString.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdlib.h>
#include <string.h>

#include <ebcdic.h>
#include <AutoString.h>

#define StringSizeIncrement 64                  // Number of characters.

// ***************************************************************************
// **** ASCII ****************************************************************
// ***************************************************************************

AutoString::AutoString(const char *string)
        {
        Buffer = NULL;
        AllocSize = Size = 0;
        *this = string;
        return;
        }

AutoString::AutoString(const AutoString &string)
        {
        if (this == &string) return;
        Buffer = NULL;
        AllocSize = Size = 0;
        *this = string;
        return;
        }

AutoString::~AutoString()
        {
        if (Buffer) free(Buffer);
        return;
        }

bool AutoString::Realloc(size_t NewSize)
        {
        if (NewSize <= AllocSize) return true;
        char *NewMem = (char *)malloc(NewSize);
        if (!NewMem) return false;
        if (AllocSize)
                {
                memcpy(NewMem,Buffer,AllocSize);
                free(Buffer);
                }
        Buffer = NewMem;
        AllocSize = NewSize;
        return true;
        }

void AutoString::Reset(size_t InitSize)
        {
        Size = Realloc(InitSize) ? InitSize : 0;
        if (Buffer) *Buffer = '\0';
        return;
        }

void AutoString::operator =(const char *Source)
        {
        Size = 0;
        *this <<= Source;
        return;
        }

void AutoString::operator =(const AutoString &Source)
        {
        if (this == &Source) return;
        Size = 0;
        *this <<= Source.Buffer;
        return;
        }

bool AutoString::operator ==(const char *ToCompare)
        {
        if (!Size) return ToCompare[0] == '\0';
        return strcmp(Buffer,ToCompare) == 0;
        }

bool AutoString::operator ==(const AutoString &ToCompare)
        {
        if (!Size) return ToCompare.Size == 0;
        return strcmp(Buffer,ToCompare.Buffer) == 0;
        }

bool AutoString::operator >(const char *ToCompare)
        {
        if (!Size) return false;
        return strcmp(Buffer,ToCompare) > 0;
        }

bool AutoString::operator >(const AutoString &ToCompare)
        {
        if (!Size) return false;
        if (!ToCompare.Size) return true;
        return strcmp(Buffer,ToCompare.Buffer) > 0;
        }

bool AutoString::operator <(const char *ToCompare)
        {
        if (!Size) return ToCompare[0] != '\0';
        return strcmp(Buffer,ToCompare) < 0;
        }

bool AutoString::operator <(const AutoString &ToCompare)
        {
        if (!Size) return ToCompare.Size > 0;
        if (!ToCompare.Size) return false;
        return strcmp(Buffer,ToCompare.Buffer) < 0;
        }

void AutoString::operator <<=(const char *Source)
        {
        if (!Source) return;
        size_t Length = strlen(Source) + 1;
        if (Length + Size > AllocSize)
                {
                size_t NewSizeA = Size + Length + StringSizeIncrement;
                size_t NewSizeB = 2 * AllocSize;
                size_t NewSize = NewSizeA > NewSizeB ? NewSizeA : NewSizeB;
                if (!Realloc(NewSize))
                        {
                        throw AutoString_Error_Memory();
                        }
                }
        if (Size) Size--;                       // Remove ending null.
        strncpy(Buffer+Size,Source,Length);     // Append.
        Size += Length;                         // Resize.
        return;
        }

// ***************************************************************************
// **** UNICODE **************************************************************
// ***************************************************************************

AutoStringW::AutoStringW(const char *string)
        {
        BufferW = NULL;
        AllocSizeW = SizeW = 0;
        *this = string;
        return;
        }

AutoStringW::AutoStringW(const wchar_t *string)
        {
        BufferW = NULL;
        AllocSizeW = SizeW = 0;
        *this = string;
        return;
        }

AutoStringW::AutoStringW(const AutoStringW &string)
        {
        if (this == &string) return;
        BufferW = NULL;
        AllocSizeW = SizeW = 0;
        *this = string;
        return;
        }

AutoStringW::~AutoStringW()
        {
        if (BufferW) free(BufferW);
        return;
        }

bool AutoStringW::ReallocW(size_t NewSizeW)
        {
        if (NewSizeW <= AllocSizeW) return true;
        wchar_t *NewMemW = (wchar_t *)mallocW(NewSizeW);
        if (!NewMemW) return false;
        if (AllocSizeW)
                {
                memcpyW(NewMemW,BufferW,AllocSizeW);
                free(BufferW);
                }
        BufferW = NewMemW;
        AllocSizeW = NewSizeW;
        return true;
        }

void AutoStringW::Reset(size_t InitSizeW)
        {
        SizeW = ReallocW(InitSizeW) ? InitSizeW : 0;
        if (BufferW) *BufferW = L'\0';
        return;
        }

void AutoStringW::operator =(const char *Source)
        {
        SizeW = 0;
        *this <<= Source;
        return;
        }

void AutoStringW::operator =(const wchar_t *Source)
        {
        SizeW = 0;
        *this <<= Source;
        return;
        }

void AutoStringW::operator =(const AutoStringW &Source)
        {
        if (this == &Source) return;
        SizeW = 0;
        *this <<= Source.BufferW;
        return;
        }

bool AutoStringW::operator ==(const wchar_t *ToCompare)
        {
        if (!SizeW) return false;
        return wcscmp(BufferW,ToCompare) == 0;
        }

bool AutoStringW::operator ==(const AutoStringW &ToCompare)
        {
        if (!SizeW) return false;
        return wcscmp(BufferW,ToCompare.BufferW) == 0;
        }

bool AutoStringW::operator >(const wchar_t *ToCompare)
        {
        if (SizeW != wcslen(ToCompare)) return false;
        if (SizeW == 0) return true;
        return wcscmp(BufferW,ToCompare) > 0;
        }

bool AutoStringW::operator >(const AutoStringW &ToCompare)
        {
        if (SizeW != ToCompare.SizeW) return false;
        if (SizeW == 0) return true;
        return wcscmp(BufferW,ToCompare.BufferW) > 0;
        }

bool AutoStringW::operator <(const wchar_t *ToCompare)
        {
        if (SizeW != wcslen(ToCompare)) return false;
        if (SizeW == 0) return true;
        return wcscmp(BufferW,ToCompare) < 0;
        }

bool AutoStringW::operator <(const AutoStringW &ToCompare)
        {
        if (SizeW != ToCompare.SizeW) return false;
        if (SizeW == 0) return true;
        return wcscmp(BufferW,ToCompare.BufferW) < 0;
        }

void AutoStringW::operator <<=(const char *Source)
        {
        if (!Source) return;
        size_t LengthW = strlen(Source) + 1;
        if (LengthW + SizeW > AllocSizeW)
                {
                size_t NewSizeA = SizeW + LengthW + StringSizeIncrement;
                size_t NewSizeB = 2 * AllocSizeW;
                size_t NewSizeW = NewSizeA > NewSizeB ? NewSizeA : NewSizeB;
                if (!ReallocW(NewSizeW))
                        {
                        throw AutoString_Error_Memory();
                        }
                }
        if (SizeW) SizeW--;                     // Remove ending null.
        astr2ustr(BufferW + SizeW,Source);      // Append.
        SizeW += LengthW;                       // Resize.
        return;
        }

void AutoStringW::operator <<=(const wchar_t *Source)
        {
        if (!Source) return;
        size_t LengthW = wcslen(Source) + 1;
        if (LengthW + SizeW > AllocSizeW)
                {
                size_t NewSizeA = SizeW + LengthW + StringSizeIncrement;
                size_t NewSizeB = 2 * AllocSizeW;
                size_t NewSizeW = NewSizeA > NewSizeB ? NewSizeA : NewSizeB;
                if (!ReallocW(NewSizeW))
                        {
                        throw AutoString_Error_Memory();
                        }
                }
        if (SizeW) SizeW--;                     // Remove ending null.
        wcsncpy(BufferW+SizeW,Source,LengthW);  // Append.
        SizeW += LengthW;                       // Resize.
        return;
        }

// ***************************************************************************
// **************************** End of File **********************************
// ***************************************************************************
