// AIcURL.cpp - cURL to OpenAI/Gemini/Anthropic
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: AIcURL.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2024 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
        #include <process.h>
#else
        #include <unistd.h>
#endif

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <popenNT.h>

#include <AIcURL.h>

#include <FindNode.h>

#ifdef _WIN32
        #define cURLExe "curl.exe"
#else
        #define cURLExe "/usr/bin/curl"
#endif

#define RETAINPostFile false                    // Debug Mode.

// ***************************************************************************
// **** Load File ************************************************************
// ***************************************************************************

static char* LoadFile(const char *Filename)
        {
        static const char *ProcName = "LoadFile";
        struct stat FileInfo;
        int rc = stat(Filename,&FileInfo);
        if (rc < 0)
                {
                TERROR(("%s: Unable to stat %s",ProcName,Filename));
                return NULL;
                }
        size_t BufferSize = FileInfo.st_size;
        if (!BufferSize)
                {
                TERROR(("%s: File %s is empty",ProcName,Filename));
                return NULL;
                }
        char *WebPage = (char *)malloc(BufferSize+1);
        if (!WebPage)
                {
                TERROR(("%s: malloc(%u) Failed for %s",ProcName,
                                                        (unsigned)(BufferSize+1),
                                                        Filename));
                return NULL;
                }
        TINFO(("%s: Loading from: %s",ProcName,Filename));
        FILE *in = fopen(Filename,"r");
        if (!in)
                {
                TERROR(("%s: Can't Open %s (errno = %u)",ProcName,
                                                         Filename,
                                                         errno));
                free(WebPage);
                return NULL;
                }
        rc = fread(WebPage,1,BufferSize,in);
        fclose(in);
        if (rc < 0)
                {
                TERROR(("%s: Read Error %s (errno = %u)",ProcName,
                                                         Filename,
                                                         errno));
                free(WebPage);
                return NULL;
                }
        WebPage[rc] = '\0';
        return WebPage;
        }

// ***************************************************************************
// **** AI APIs **************************************************************
// **************
//
//      APIs: cURL_OpenAI()             // Supports OpenAI and Google
//            cURL_GoogleAI()           // Google native supports search.
//            cURL_Anthropic()          // 
//
//      All output is JSON and to be freed with cURL_FreeResponseBuffer()
//
// ***************************************************************************

// *********************
// **** cURL_OpenAI ****
// *********************

char* cURL_OpenAI(cURLRunner &cURL,
                  const char *URLEndpoint,
                  const char *AIModel,
                  const char *APIKey,
                  const char *SystemMessage,
                  const char *UserMessage,
                  double Temperature,
                  const char *CacheFile)
        {
        static const char *ProcName = "cURL_OpenAI";
        // --------------------
        // ---- Cache Load ----
        // --------------------
        struct stat FileInfo;
        if (CacheFile)
                {
                int rc = stat(CacheFile,&FileInfo);
                if (rc >= 0)
                        {
                        char *Result = LoadFile(CacheFile);
                        return Result;
                        }
                }
        // ----------------------------
        // ---- Write Post Content ----
        // ----------------------------
        MemoryPrintf PostNameCanvas;
        PostNameCanvas.printf("Post_%u_%u",getpid(),(unsigned)GetThreadID());
        const char *PostContentFile = PostNameCanvas.GetBuffer();
        FILE *outPost = fopen(PostContentFile,"w");
        if (!outPost)
                {
                TERROR(("%s: fopen(\"%s\",\"w\") Failed (%d)",ProcName,
                                                              PostContentFile,
                                                              errno));
                return NULL;
                }
        char *EscSystem = EscapeJSONString(SystemMessage);
        char *EscUser = EscapeJSONString(UserMessage);
        int rc = fprintf(outPost,
                         "{\n"
                         "  \"model\": \"%s\",\n"
                         "  \"temperature\": %.2f,\n"
                         "  \"messages\": [\n"
                         "     {\n"
                         "         \"role\": \"system\",\n"
                         "         \"content\": \"%s\"\n"
                         "     },\n"
                         "     {\n"
                         "         \"role\": \"user\",\n"
                         "         \"content\": \"%s\"\n"
                         "     }]\n"
                         "}\n",AIModel,
                         Temperature,
                         EscSystem ? EscSystem : "",
                         EscUser ? EscUser : "");
        FreeEscapeJSONString(&EscSystem);
        FreeEscapeJSONString(&EscUser);
        if (rc < 0)
                {
                TERROR(("%s: Post Content fprintf(%s) Failed (%d)",
                        ProcName,
                        PostContentFile,
                        errno));
                fclose(outPost);
                unlink(PostContentFile);
                return NULL;
                }
        fclose(outPost);
        // -------------------------
        // ---- Request Headers ----
        // -------------------------
        MemoryPrintf HeaderCanvas;
        HeaderCanvas.printf("Content-Type: application/json\n"
                            "Authorization: Bearer %s\n",APIKey);
        // -------------------------
        // ---- cURL AI Request ----
        // -------------------------
        MemoryPrintf Options;
        if (CacheFile) Options.printf("-o %s ",CacheFile);
        Options.printf("--data @%s",PostContentFile);
        bool Status = cURL.Run(Options.GetBuffer(),
                               URLEndpoint,
                               HeaderCanvas.GetBuffer());
        if (!RETAINPostFile) unlink(PostContentFile);
        if (!Status)
                {
                TERROR(("%s: cURL.Run() Failed",ProcName));
                return NULL;
                }
        char *Result = CacheFile ? LoadFile(CacheFile) : cURL.AquireBuffer();
        return Result;
        }

// ***********************
// **** cURL_GoogleAI ****
// ***********************

char* cURL_GoogleAI(cURLRunner &cURL,
                    const char *URLEndpoint,
                    const char *AIModel,
                    const char *APIKey,
                    const char *SystemMessage,
                    const char *UserMessage,
                    bool GoogleSearch,
                    double Temperature,
                    const char *CacheFile)
        {
        static const char *ProcName = "cURL_GoogleAI";
        // --------------------
        // ---- Cache Load ----
        // --------------------
        struct stat FileInfo;
        if (CacheFile)
                {
                int rc = stat(CacheFile,&FileInfo);
                if (rc >= 0)
                        {
                        char *Result = LoadFile(CacheFile);
                        return Result;
                        }
                }
        // ----------------------------
        // ---- Write Post Content ----
        // ----------------------------
        MemoryPrintf PostNameCanvas;
        PostNameCanvas.printf("Post_%u_%u",getpid(),(unsigned)GetThreadID());
        const char *PostContentFile = PostNameCanvas.GetBuffer();
        FILE *outPost = fopen(PostContentFile,"w");
        if (!outPost)
                {
                TERROR(("%s: fopen(\"%s\",\"w\") Failed (%d)",ProcName,
                                                              PostContentFile,
                                                              errno));
                return NULL;
                }
        char *EscSystem = EscapeJSONString(SystemMessage);
        char *EscUser = EscapeJSONString(UserMessage);
        int rc = fprintf(outPost,
                         "{\n"
                         "  \"system_instruction\": {\n"
                         "    \"parts\": [ {\"text\": \"%s\"} ]\n"
                         "  },\n"
                         "  \"contents\": [\n"
                         "    {\n"
                         "      \"role\": \"user\",\n"
                         "      \"parts\": [ {\"text\": \"%s\"} ]\n"
                         "    }\n"
                         "  ]"
                         ",\n  \"generationConfig\": {\"temperature\": %.2f}"
                         "%s"
                         "}\n", 
                        EscSystem ? EscSystem : "", 
                        EscUser ? EscUser : "",
                        Temperature,
                        GoogleSearch
                        ? ",\n  \"tools\":[{\"google_search\":{}}]\n"
                        : "\n");
        FreeEscapeJSONString(&EscSystem);
        FreeEscapeJSONString(&EscUser);
        if (rc < 0)
                {
                TERROR(("%s: Post Content fprintf(%s) Failed (%d)",
                        ProcName,
                        PostContentFile,
                        errno));
                fclose(outPost);
                unlink(PostContentFile);
                return NULL;
                }
        fclose(outPost);
        // --------------------------
        // ---- Form URLEndpoint ----
        // --------------------------
        MemoryPrintf URLCanvas;
        URLCanvas.printf(URLEndpoint,AIModel);
        URLEndpoint = URLCanvas.GetBuffer();
        // -------------------------
        // ---- Request Headers ----
        // -------------------------
        MemoryPrintf HeaderCanvas;
        HeaderCanvas.printf("Content-Type: application/json\n"
                            "X-goog-api-key: %s\n",APIKey);
        // -------------------------
        // ---- cURL AI Request ----
        // -------------------------
        MemoryPrintf Options;
        if (CacheFile) Options.printf("-o %s ",CacheFile);
        Options.printf("--data @%s",PostContentFile);
        bool Status = cURL.Run(Options.GetBuffer(),
                               URLEndpoint,
                               HeaderCanvas.GetBuffer());
        if (!RETAINPostFile) unlink(PostContentFile);
        if (!Status)
                {
                TERROR(("%s: cURL.Run() Failed",ProcName));
                return NULL;
                }
        char *Result = CacheFile ? LoadFile(CacheFile) : cURL.AquireBuffer();
        return Result;
        }

// ************************
// **** cURL_Anthropic ****
// ************************

char* cURL_Anthropic(cURLRunner &cURL,
                     const char *URLEndpoint,
                     const char *AIModel,
                     const char *APIKey,
                     const char *SystemMessage,
                     const char *UserMessage,
                     double Temperature,
                     const char *CacheFile)
        {
        static const char *ProcName = "cURL_Anthropic";
        // --------------------
        // ---- Cache Load ----
        // --------------------
        struct stat FileInfo;
        if (CacheFile)
                {
                int rc = stat(CacheFile,&FileInfo);
                if (rc >= 0)
                        {
                        char *Result = LoadFile(CacheFile);
                        return Result;
                        }
                }
        // ----------------------------
        // ---- Write Post Content ----
        // ----------------------------
        MemoryPrintf PostNameCanvas;
        PostNameCanvas.printf("Post_%u_%u",getpid(),(unsigned)GetThreadID());
        const char *PostContentFile = PostNameCanvas.GetBuffer();
        FILE *outPost = fopen(PostContentFile,"w");
        if (!outPost)
                {
                TERROR(("%s: fopen(\"%s\",\"w\") Failed (%d)",ProcName,
                                                              PostContentFile,
                                                              errno));
                return NULL;
                }
        char *EscSystem = EscapeJSONString(SystemMessage);
        char *EscUser = EscapeJSONString(UserMessage);
        int rc = fprintf(outPost,
                         "{\n"
                         "  \"model\": \"%s\",\n"
                         "  \"max_tokens\": 1024,\n"
                         "  \"temperature\": %.2f,\n"
                         "  \"system\": \"%s\",\n"
                         "  \"messages\": [\n"
                         "     {\n"
                         "         \"role\": \"user\",\n"
                         "         \"content\": \"%s\"\n"
                         "     }\n"
                         "  ]\n"
                         "}\n", AIModel,
                         Temperature,
                         EscSystem ? EscSystem : "",
                         EscUser ? EscUser : "");
        FreeEscapeJSONString(&EscSystem);
        FreeEscapeJSONString(&EscUser);
        if (rc < 0)
                {
                TERROR(("%s: Post Content fprintf(%s) Failed (%d)",
                        ProcName,
                        PostContentFile,
                        errno));
                fclose(outPost);
                unlink(PostContentFile);
                return NULL;
                }
        fclose(outPost);
        // -------------------------
        // ---- Request Headers ----
        // -------------------------
        MemoryPrintf HeaderCanvas;
        HeaderCanvas.printf("Content-Type: application/json\n"
                            "x-api-key: %s\n"
                            "anthropic-version: 2023-06-01\n",APIKey);
        // -------------------------
        // ---- cURL AI Request ----
        // -------------------------
        MemoryPrintf Options;
        if (CacheFile) Options.printf("-o %s ",CacheFile);
        Options.printf("--data @%s",PostContentFile);
        bool Status = cURL.Run(Options.GetBuffer(),
                               URLEndpoint,
                               HeaderCanvas.GetBuffer());
        if (!RETAINPostFile) unlink(PostContentFile);
        if (!Status)
                {
                TERROR(("%s: cURL.Run() Failed",ProcName));
                return NULL;
                }
        char *Result = CacheFile ? LoadFile(CacheFile) : cURL.AquireBuffer();
        return Result;
        }

// ****************************************************************************
// **** cURL_AI ***************************************************************
// ****************************************************************************

char* cURL_AI(cURLRunner &cURL,
              const char *URLEndpoint,
              const char *AIModel,
              const char *APIKey,
              const char *SystemMessage,
              const char *UserMessage,
              double Temperature,
              const char *CacheFile)
        {
        static const char *ProcName = "cURL_AI";
        char *AIResponse = NULL;
        if (!URLEndpoint || !AIModel || !APIKey)
                {
                TERROR(("%s: Missing Environment: URL, Model, Key",ProcName));
                return NULL;
                }
        if (!SystemMessage)
                {
                TERROR(("%s: Missing AI Request: SystemMessage",ProcName));
                return NULL;
                }
        if (!UserMessage)
                {
                TERROR(("%s: Missing AI Request: UserMessage",ProcName));
                return NULL;
                }
        bool UseGoogle = strstr(AIModel,"gemini") != NULL;
        bool UseAnthropic = strstr(AIModel,"anthropic") != NULL;
        bool UseOpenAI = strstr(URLEndpoint,"openai") != NULL;
        bool UseGoogleNative = UseGoogle && !UseOpenAI;
        if (UseOpenAI || (UseGoogle && !UseGoogleNative))
                {
                AIResponse = cURL_OpenAI(cURL,
                                         URLEndpoint,
                                         AIModel,
                                         APIKey,
                                         SystemMessage,
                                         UserMessage,
                                         Temperature,
                                         CacheFile);
                }
        else if (UseAnthropic)
                {
                AIResponse = cURL_Anthropic(cURL,
                                            URLEndpoint,
                                            AIModel,
                                            APIKey,
                                            SystemMessage,
                                            UserMessage,
                                            Temperature,
                                            CacheFile);
                }
        else if (UseGoogle) // Google Native   
                {
                AIResponse = cURL_GoogleAI(cURL,
                                            URLEndpoint,
                                            AIModel,
                                            APIKey,
                                            SystemMessage,
                                            UserMessage,
                                            false,
                                            Temperature,
                                            CacheFile);
                }
        else    {
                TERROR(("%s: No AI Platform Selected",ProcName));
                }
        if (!AIResponse)
                {
                TERROR(("%s: cURL_AI(\"%.20s\") Failed",
                        ProcName,
                        UserMessage));
                }
        return AIResponse;
        }

// ****************************************************************************
// **** ExtractAIContent ******************************************************
// ****************************************************************************

const char* ExtractAIContent(const char *URLEndpoint,
                           const char *AIModel, 
                           Node *AIResponseNode,
                           size_t *Content_Length)
        {
        static const char *ProcName = "ExtractAIContent";
        if (!Content_Length)
                {
                TERROR(("%s: Missing Content_Length parameter",ProcName));
                return NULL;
                }
        if (!AIResponseNode) 
                {
                TERROR(("%s: Missing AIResponseNode parameter",ProcName));
                return NULL;
                }
        *Content_Length = 0; 
        const char* Content = NULL;
        bool UseGoogle = strstr(AIModel,"gemini") != NULL;
        bool UseAnthropic = strstr(AIModel,"anthropic") != NULL;
        bool UseOpenAI = strstr(URLEndpoint,"openai") != NULL;
        bool UseGoogleNative = UseGoogle && !UseOpenAI;
        TerminalNode* Found = NULL;
        if (UseOpenAI || (UseGoogle && !UseGoogleNative))
                {
                TDEBUG(("%s: OpenAI: Looking for: \"content\"",ProcName));
                Found = (TerminalNode *)FindNode(AIResponseNode,
                         4,
                         Ew_JSONNameID,0,0,
                         Ew_StringID,FN_LEX_strcmp,"\"content\"",
                         Ew_JSONValueID,0,0,
                         Ew_StringID,0,0);
                }
        else if (UseAnthropic)
                {
                TDEBUG(("%s: Anthropic: Looking for: \"content\"--"
                                                    "\"text\"",ProcName));
                Found = (TerminalNode *)FindNode(AIResponseNode,
                         8,
                         Ew_JSONNameID,0,0,
                         Ew_StringID,FN_LEX_strcmp,"\"content\"",
                         Ew_JSONMemberID,0,0,
                         // -----------------
                         WILDCARD_TypeID,0,0,
                         Ew_JSONNameID,0,0,
                         Ew_StringID,FN_LEX_strcmp,"\"text\"",
                         Ew_JSONValueID,0,0,
                         Ew_StringID,0,0);
                }
        else if (UseGoogle)   
                {
                TDEBUG(("%s: Google: Looking for: \"content\"--"
                                                 "\"parts\"--"
                                                 "\"text\"",ProcName));
                Found = (TerminalNode *)FindNode(AIResponseNode,
                         12,
                         Ew_JSONNameID,0,0,
                         Ew_StringID,FN_LEX_strcmp,"\"content\"",
                         Ew_JSONMemberID,0,0,
                         // -----------------
                         WILDCARD_TypeID,0,0,
                         Ew_JSONNameID,0,0,
                         Ew_StringID,FN_LEX_strcmp,"\"parts\"",
                         Ew_JSONMemberID,0,0,
                         // -----------------
                         WILDCARD_TypeID,0,0,
                         Ew_JSONNameID,0,0,
                         Ew_StringID,FN_LEX_strcmp,"\"text\"",
                         Ew_JSONValueID,0,0,
                         Ew_StringID,0,0);
                }
        if (Found)
                {
                LexicalItem *Item = (LexicalItem *)Found->GetValue();
                *Content_Length = Item->Length;
                Content = Item->String;
                }
        return Content;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
