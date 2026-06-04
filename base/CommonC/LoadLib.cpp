// Load Dynamic Library: LoadLib.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: LoadLib.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2003 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <LoadLib.h>

static int LastLoadError = 0;

static char LastLoadMessage[256] = {0};

int GetLastLoadError()
        {
        return LastLoadError;
        }

const char* GetLastLoadMessage()
        {
        return LastLoadMessage;
        }

// ***************************************************************************
// **** Load Library *********************************************************
// ***************************************************************************

#if defined(_WIN32)

LIBHANDLE LoadLib(const char *Path)
        {
        if (!Path || !*Path) return NOLIBHANDLE;
        LIBHANDLE hLib = LoadLibrary(Path);
        if (!hLib) 
                {
                LastLoadError = GetLastError();
                sprintf(LastLoadMessage,"LoadLib failed (%u): %.100s",
                                        LastLoadError,
                                        Path);
                }
        return hLib;
        }

void* LoadSymbol(LIBHANDLE hLib, const char *Symbol)
        {
        if (!hLib) return NULL;
        void *hSymbol = GetProcAddress(hLib,Symbol);
        if (!hSymbol)
                {
                LastLoadError = GetLastError();
                sprintf(LastLoadMessage,"LoadSymbol failed (%u): %.100s",
                                        LastLoadError,
                                        Symbol);
                return NULL;
                }
        return hSymbol;
        }

void CloseLib(LIBHANDLE *hLib)
        {
        if (*hLib != NOLIBHANDLE) 
                {
                FreeLibrary(*hLib);
                *hLib = NOLIBHANDLE;
                }
        return;
        }

#elif defined(__OS400__)

#include <except.h>
#include <qleawi.h>
#include <mih/rslvsp.h>

#ifndef AS4_OBJ_LEN
        #define AS4_OBJ_LEN 10
#endif

LIBHANDLE LoadLib(const char *Path)             // "LIBRARY/SRVPGM"
        {
        if (!Path || !*Path) return NOLIBHANDLE;
        LIBHANDLE hLib = 0;
        _SYSPTR ServiceProgramPtr = 0;
        char ErrorMemory[256];
        Qus_EC_t* ErrorCode = (Qus_EC_t*)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);

        // **************************************
        // **** Build Library / Object Names ****
        // **************************************
        char LibnameZ[AS4_OBJ_LEN+1];
        char ObjnameZ[AS4_OBJ_LEN+1];
        char *Delimiter = strchr(Path,'/');
        if (!Delimiter) return false;

        size_t LibLen = Delimiter - Path;
        if (LibLen > AS4_OBJ_LEN) LibLen = AS4_OBJ_LEN;
        memcpy(LibnameZ,Path,LibLen);
        LibnameZ[LibLen] = '\0';

        size_t ObjLen = strlen(Delimiter+1);
        if (ObjLen > AS4_OBJ_LEN) ObjLen = AS4_OBJ_LEN;
        memcpy(ObjnameZ,Delimiter+1,ObjLen);
        ObjnameZ[ObjLen] = '\0';

        // **********************************************
        // **** Resolve and Activate Service Program ****
        // **********************************************
        _INTRPT_Hndlr_Parms_T ca = {0};
        #pragma exception_handler (LS_Error,                                 \
                                   ca,                                       \
                                   0,                                        \
                                   _C2_MH_ESCAPE | _C2_MH_FUNCTION_CHECK,    \
                                   _CTLA_HANDLE)

        ServiceProgramPtr = rslvsp(WLI_SRVPGM,
                                   ObjnameZ,
                                   LibnameZ,
                                   _AUTH_EXECUTE);
        if (!ServiceProgramPtr)
                {
                LastLoadError = ENOENT;
                sprintf(LastLoadMessage,
                        "LoadLib: rslvsp() failed (ENOENT): %.100s",
                        Path);
                return NULL;
                }
        QleActBndPgm(&ServiceProgramPtr,
                     &hLib, 
                     NULL, 
                     NULL,
                     ErrorCode);
        if (ErrorCode->Bytes_Available)
                {
                LastLoadError = atoi(&ErrorCode->Exception_Id[3]);
                sprintf(LastLoadMessage,
                        "LoadLib: QleActBndPgm() failed (%.7s): %.100s",
                        ErrorCode->Exception_Id,
                        Path);
                hLib = NULL;
                }
        #pragma disable_handler
        goto LS_Exit; 

  LS_Error:
         
        LastLoadError = atoi(&ca.Msg_Id[3]);
        sprintf(LastLoadMessage,
                "LoadLib: Exception (%.7s): %.100s",
                ca.Msg_Id,
                Path);
        hLib = NULL;
        
  LS_Exit:

        return hLib;
        }

void* LoadSymbol(LIBHANDLE hLib, const char *Symbol)
        {
        void* hSymbol = NULL;
        int exportType = 0;
        int export_id = 0;
        int export_name_len = 0;
        char ErrorMemory[256];
        Qus_EC_t* ErrorCode = (Qus_EC_t*)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);

        _INTRPT_Hndlr_Parms_T ca = {0};
        #pragma exception_handler (LS_Error,                                 \
                                   ca,                                       \
                                   0,                                        \
                                   _C2_MH_ESCAPE | _C2_MH_FUNCTION_CHECK,    \
                                   _CTLA_HANDLE)
        QleGetExp(&hLib,
                  &export_id,
                  &export_name_len,
                  (char *)Symbol,
                  &hSymbol,
                  &exportType,
                  ErrorCode);
        #pragma disable_handler

        if (ErrorCode->Bytes_Available)
                {
                LastLoadError = atoi(&ErrorCode->Exception_Id[3]);
                sprintf(LastLoadMessage,
                        "LoadSymbol: QleGetExp() failed (%.7s): %.100s",
                        ErrorCode->Exception_Id,
                        Symbol);
                hSymbol = NULL;
                }
        else if (exportType == QLE_EX_NOT_FOUND || 
                 exportType == QLE_EX_NO_ACCESS)
                {
                LastLoadError = exportType;
                sprintf(LastLoadMessage,
                        "LoadSymbol: QleGetExp() failed (%s): %.100s",
                        exportType == QLE_EX_NOT_FOUND
                        ? "QLE_EX_NOT_FOUND"
                        : "QLE_EX_NO_ACCESS",
                        Symbol);
                hSymbol = NULL;
                }
        goto LS_Exit;

  LS_Error:
         
        LastLoadError = atoi(&ca.Msg_Id[3]);
        sprintf(LastLoadMessage,
                "LoadSymbol: Exception (%.7s): %.100s",
                ca.Msg_Id,
                Symbol);
        hSymbol = NULL;

  LS_Exit:

        return hSymbol;
        }

void CloseLib(LIBHANDLE *hLib)
        {
        if (*hLib != NOLIBHANDLE) 
                {
                *hLib = NOLIBHANDLE;
                }
        return;
        }

#else /* UNIX */

#include <dlfcn.h>

#ifndef RTLD_NOW
        #define RTLD_NOW 0
#endif

#ifndef RTLD_GLOBAL
        #define RTLD_GLOBAL 0
#endif

#ifndef RTLD_MEMBER
        #define RTLD_MEMBER 0
#endif

LIBHANDLE LoadLib(const char *Path)
        {
        if (!Path || !*Path) return NOLIBHANDLE;
        unsigned Flags = RTLD_NOW | RTLD_GLOBAL;
        if (Path[strlen(Path)-1] == /*Archive.a(Member.o*/')') 
                {
                Flags |= RTLD_MEMBER;
                }
        LIBHANDLE hLib = dlopen(Path,Flags);
        if (!hLib) 
                {
                LastLoadError = errno;
                sprintf(LastLoadMessage,"LoadLib failed (%d): %.100s",
                                        LastLoadError,
                                        Path);
                }
        return hLib;
        }

void* LoadSymbol(LIBHANDLE hLib, const char *Symbol)
        {
        void* hSymbol = dlsym(hLib,Symbol);
        if (!hSymbol) 
                {
                LastLoadError = errno;
                sprintf(LastLoadMessage,"LoadSymbol failed (%u): %.100s",
                                        LastLoadError,
                                        Symbol);
                }
        return hSymbol;
        }

void CloseLib(LIBHANDLE *hLib)
        {
        if (*hLib != NOLIBHANDLE) 
                {
                dlclose(*hLib);
                *hLib = NOLIBHANDLE;
                }
        return;
        }

#endif /* UNIX */

// ***************************************************************************
// ***************************** End of File *********************************
// ***************************************************************************
