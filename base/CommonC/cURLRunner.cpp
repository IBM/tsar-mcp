// cURLRunner.cpp - Run cURL with Cancel
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: cURLRunner.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2024 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdlib.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <popenNT.h>

#include <cURLRunner.h>

#ifdef _WIN32
        #define cURLExe "curl.exe"
#else
        #define cURLExe "curl"
#endif

// ***************************************************************************
// **** CURL Runner **********************************************************
// ***************************************************************************

cURLRunner::cURLRunner() 
        {
        cURLout = NULL; 
        Canceled = false;
        CancelHandle = NULL;
        return;
        }

cURLRunner::~cURLRunner() 
        {
        cURL_FreeResponseBuffer(&cURLout);
        return;
        }
       
bool cURLRunner::Cancel()
        {
        static const char *ProcName = "cURLRunner::Cancel";
        bool Status = true;
        CancelHandleCS.Take();
        Canceled = true;
        if (CancelHandle)
                {
                TDEBUG(("%s: Canceling via pabortNT(0x%p)",ProcName,
                                                           CancelHandle));
                Status = pabortNT(CancelHandle);
                }
        CancelHandleCS.Release();
        return Status;
        }

bool cURLRunner::Run(const char *Options, const char *URL)
        {
        static const char *ProcName = "cURLRunner::Run"; 
        MemoryPrintf cURLCanvas;
        cURLCanvas.printf("%s %s %s %s",cURLExe,
                                        Options,
                                        "--silent --show-error",
                                        URL);
        const char *Command = cURLCanvas.GetBuffer();
        if (!Command)
                {
                TERROR(("%s: Unable to form cURL command",ProcName));
                return false;
                }
        // -----
        // Spawn
        // -----
        CancelHandleCS.Take();
        if (Canceled)
                {
                TDEBUG(("%s: Request CANCELED via Cancel()",ProcName));
                CancelHandleCS.Release();
                return false;
                }
        FILE *out = NULL;
        FILE *err = NULL;
        TDEBUG(("%s: %s %s",ProcName,cURLExe,URL));
        bool Status = popenNToe(Command,"r",&out,&err);
        if (!Status || !out || !err)
                {
                TERROR(("%s: popenNT(%s) Failed errno=%d",ProcName,
                                                          Command,
                                                          errno));
                CancelHandleCS.Release();
                return false;
                }
        CancelHandle = out;
        CancelHandleCS.Release();
        // ----------------
        // Read Pipe Output
        // ----------------
        cURLCanvas.Clear();
        cURL_FreeResponseBuffer(&cURLout);
        char Buffer[1024];
        while (out && !feof(out))
                {
                char *rc = fgets(Buffer,sizeof(Buffer),out);
                if (!rc) 
                        {
                        if (feof(out)) 
                                {
                                TDEBUG(("%s: fgetc(out) - EOF",ProcName));
                                }
                        else    {
                                TERROR(("%s: fgetc(out) Error",ProcName));
                                }
                        break;
                        }
                
                cURLCanvas.fputs(Buffer);
                }
        while (err && !feof(err))
                {
                char *rc = fgets(Buffer,sizeof(Buffer),err);
                if (!rc) 
                        {
                        if (feof(err)) 
                                {
                                TDEBUG(("%s: fgetc(err) - EOF",ProcName));
                                }
                        else    {
                                TERROR(("%s: fgetc(err) Error",ProcName));
                                }
                        break;
                        }
                TERROR(("%s: stderr: %s",ProcName,Buffer));
                }
        CancelHandleCS.Take();
        CancelHandle = NULL;
        CancelHandleCS.Release();
        pcloseNT(out);
        cURLout = cURLCanvas.AquireBuffer();
        return !Canceled;
        }

// *********************************
// **** cURL_FreeResponseBuffer ****
// *********************************

void cURL_FreeResponseBuffer(char **Buffer)
        {
        if (Buffer && *Buffer)
                {
                free(*Buffer);
                *Buffer = NULL;
                }
        return;
        }

// ***************************************************************************
// **** cURL_Check ***********************************************************
// ***************************************************************************

bool cURL_Check()
        {
        cURLRunner cURL;
        return cURL.Run("-V","");
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
