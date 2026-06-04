///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: LoadLib.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.                                                
//                    
// Author(s) : Eric Kass 
//
///////////////////////////////////////////////////////////////////////////////
#ifndef __LoadDynamicLibrary

        #define __LoadDynamicLibrary

int GetLastLoadError();

const char* GetLastLoadMessage();

#ifdef _WIN32
        #include <windows.h>
        typedef HMODULE LIBHANDLE;
        #define NOLIBHANDLE NULL
#endif

#ifdef __OS400__
        typedef int LIBHANDLE;
        #define NOLIBHANDLE -1
#endif

#ifdef _AIX
        typedef void* LIBHANDLE;
        #define NOLIBHANDLE NULL
#endif

LIBHANDLE LoadLib(const char *Path);

void* LoadSymbol(LIBHANDLE hLib, const char *Symbol);

void CloseLib(LIBHANDLE *hLib);

#endif /* __LoadDynamicLibrary */
