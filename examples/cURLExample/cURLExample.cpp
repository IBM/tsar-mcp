// cURLExample.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: cURLExample.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdio.h>
#ifndef _WIN32
        #include <unistd.h>
#endif

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <cURLRunner.h>

#include <AIcURLJSON.h>

// ***************************************************************************
// **** main() ***************************************************************
// ***************************************************************************

int cURLExample()
        {
        const char *URL = "http://httpbin.org/post";
        const char *PostFile = "Post_httpbin.json";
        // ---------------------------------
        // ---- Write HTTP Post Content ----
        // ---------------------------------
        TINFO(("Filling POST Data: %s",PostFile));
        FILE *outPost = fopen(PostFile,"w");
        if (!outPost)
                {
                TERROR(("Unable to open: %s",PostFile));
                return 1;
                }
        fprintf(outPost,"{\"message\":\"hello from cURLRunner\"}\n");
        fclose(outPost);
        // ------------------------------
        // ---- Prepare cURL Options ----
        // ------------------------------
        MemoryPrintf Options;
        Options.printf("-H \"Content-Type: application/json\" "
                       "--data @%s",PostFile);
        // ---------------------
        // ---- Invoke cURL ----
        // ---------------------
        TINFO(("Invoking cURL: %s",URL));
        cURLRunner cURL;
        bool Status = cURL.Run(Options.GetBuffer(),URL);
        unlink(PostFile);
        if (!Status)
                {
                TERROR(("Call to cURL Failed"));
                return 2;
                }
        // -------------------------------------------
        // ---- Turn API results into JSON_Object ----
        // -------------------------------------------
        TINFO(("Raw HTTP Result:"));
        const char *HTTPResult = cURL.GetBuffer();
        printf("HTTPBIN Result: %s\n",HTTPResult);
        TINFO(("Build JSON Object:"));
        JSON_Object *HTTPBINObject = BuildJSONObject(HTTPResult);
        if (!HTTPBINObject)
                {
                TERROR(("Unable to build JSON_Object"));
                return 3;
                }
        PrintJSONObject(*HTTPBINObject);
        // ----------------------------------------
        // ---- Retrieve Data from JSON Object ----
        // ----------------------------------------
        const char *Origin = HTTPBINObject->Find_member_string("origin");
        const char *URLFound = HTTPBINObject->Find_member_string("url");
        const char *Message = "<No Message>";
        JSON_Object *jsonObj = HTTPBINObject->Find_member_object("json");
        if (jsonObj)
                {
                Message = jsonObj->Find_member_string("message");
                }
        TINFO(("HTTPBIN Origin:  %s", Origin ? Origin : ""));
        TINFO(("HTTPBIN URL:     %s", URLFound ? URLFound : ""));
        TINFO(("HTTPBIN Message: %s", Message ? Message : ""));
        delete HTTPBINObject; 
        return 0;
        }

int main()
        {
        InitJSONParser();
        int rc = cURLExample();
        DeinitJSONParser();
        return rc;
        }
