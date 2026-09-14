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
                bool Run(const char *Options, const char *URL);
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

 ****************************************************************************
 **** Example ***************************************************************
 *************
 
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

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __cURL_Runner_H */
