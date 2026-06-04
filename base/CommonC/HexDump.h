// Print HexDump: HexDump.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: HexDump.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2004 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Print_Hex_Dump

        #define __Print_Hex_Dump

#include <stdarg.h>

typedef void (*HexDumpFunction_t)(void *UserPtr, const char *Format, ...);

void HexDump(HexDumpFunction_t PrintFn, 
             void *UserPtr,
             unsigned LineTab,
             const unsigned char *Buffer, size_t Length);

enum HD_Mode_t {HD_AUTO, HD_EBCDIC, HD_ASCII};

void SetHexDumpMode(HD_Mode_t Mode);

// **************************
// **** Hex Dump to File ****
// **************************

void HexDump(FILE *out, const unsigned char *Buffer, size_t Length);

void HexDump(FILE *out, const char *Buffer, size_t Length);

// *********************************
// **** Hex Dump via LevelTrace ****
// *********************************

void THexDump(const unsigned char *Buffer, size_t Length); 

void THexDump(const char *Buffer, size_t Length);

#endif /* __Print_Hex_Dump */

