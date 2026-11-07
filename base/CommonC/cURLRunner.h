// cURLRunner.h - Run cURL with Cancel
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: cURLRunner.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2024 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __cURL_Runner_H

        #define __cURL_Runner_H

#include <stdio.h>
#include <ASThread.h>

class cURLRunner
        {
        private:
                CriticalSection CancelHandleCS;
                FILE *CancelHandle;
                bool Canceled;
                char *cURLout;
        public:
                cURLRunner();
                ~cURLRunner();
                bool Run(const char *Options, 
                         const char *URL, 
                         const char *Headers = NULL);
                char* AquireBuffer() 
                        {
                        char *rcURLout = cURLout;
                        cURLout = NULL;
                        return rcURLout;
                        }
                bool Cancel();
                const char* GetBuffer() {return cURLout;}
                bool wasCanceled() {return Canceled;}
        };

void cURL_FreeResponseBuffer(char **Buffer);    // Only if AquireBuffer().

// ***************************************************************************
// **** cURL_Check ***********************************************************
// ***************************************************************************

bool cURL_Check();                      // 'true' if version check worked.

/* **************************************************************************
 ****************************************************************************

 Notes:
                   
        A. Run() Headers:

           Secrets (API keys, Bearer tokens) must NEVER ride on the cURL
           command line: the OS process list (e.g. ps/Task Manager)
           hands a running process's arguments to anybody else on the box,
           and Run() also echoes the whole command into the trace log when
           the spawn fails.

           Nor should they be staged in a file, where a crash between the
           spawn and the cleanup strands the key on disk.

           Pass them as 'Headers' instead. Run() adds "-H @-" and writes the
           text down the child's stdin, so the key crosses a pipe between two
           processes and touches neither argv nor a filesystem. One header per
           line, no quoting, no escaping. Requires cURL 7.55.0 or later.

           Note: Writing to a pipe whose reader has died raises SIGPIPE on
                 Unix, which by default terminates the process. Run() holds
                 CancelHandleCS across the write, so Cancel() cannot abort
                 the child out from under it - but a cURL that exits early
                 of its own accord (a malformed option, say) still can. A
                 library must not alter process-wide signal state, so the
                 host program owns the remedy:

                      #ifndef _WIN32
                              signal(SIGPIPE,SIG_IGN);   // Once, in main().
                      #endif

 ****************************************************************************
 **** Example 1 *************************************************************
 ***************
 
   Simple API POST using a request body file:

 ****************************************************************************
  
        cURLRunner cURL;
        const char *PostFile = "Post_httpbin.json";
        FILE *outPost = fopen(PostFile,"w");
        if (outPost)
                {
                fprintf(outPost,"{\"message\":\"hello from cURLRunner\"}\n");
                fclose(outPost);
                }
     
        MemoryPrintf Options;
        Options.printf("-H \"Content-Type: application/json\" "
                       "--request POST "
                       "--data @%s",PostFile);
     
        const char *URL = "https://httpbin.org/post";
        bool Status = cURL.Run(Options.GetBuffer(),URL);
        if (Status)
                {
                printf("HTTPBIN Result: %s\n",cURL.GetBuffer());
                }

 ****************************************************************************
 **** Example 2 *************************************************************
 ****************
 
   Secure call with API Key

 ****************************************************************************

        MemoryPrintf HeaderCanvas;
        HeaderCanvas.printf("Content-Type: application/json\n"
                            "Authorization: Bearer %s\n",APIKey);
        MemoryPrintf Options;
        Options.printf("--request POST --data @%s",PostFile);
        bool Status = cURL.Run(Options.GetBuffer(),
                               URL,
                               HeaderCanvas.GetBuffer());
 
 ****************************************************************************

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __cURL_Runner_H */
