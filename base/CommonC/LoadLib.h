// Load Dynamic Library: LoadLib.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: LoadLib.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2003 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __LoadDynamicLibrary

        #define __LoadDynamicLibrary

int GetLastLoadError();

const char* GetLastLoadMessage();

#ifdef _WIN32
        #include <windows.h>
        typedef HMODULE LIBHANDLE;
        #define NOLIBHANDLE NULL
#elif defined(__OS400__)
        typedef int LIBHANDLE;
        #define NOLIBHANDLE -1
#else
        typedef void* LIBHANDLE;
        #define NOLIBHANDLE NULL
#endif

LIBHANDLE LoadLib(const char *Path);

void* LoadSymbol(LIBHANDLE hLib, const char *Symbol);

void CloseLib(LIBHANDLE *hLib);

#endif /* __LoadDynamicLibrary */
