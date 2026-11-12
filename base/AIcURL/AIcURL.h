// AIcURL.h - cURL to OpenAI/Gemini/Anthropic
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: AIcURL.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2024 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//

#ifndef __AI_via_cURL_H

        #define __AI_via_cURL_H

#include <stdio.h>
#include <ASThread.h>
#include <cURLRunner.h>
#include <JSONObject.h>

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

// **********************
// **** Self-Routing ****
// **********************

char* cURL_AI(cURLRunner &cURL,
              const char *URLEndpoint,
              const char *AIModel,
              const char *APIKey,
              const char *SystemMessage,
              const char *UserMessage,
              double Temperature = 1.0,
              const char *CacheFile = NULL);

// *********************
// **** cURL_OpenAI ****
// *********************

char* cURL_OpenAI(cURLRunner &cURL,
                  const char *URLEndpoint,
                  const char *AIModel,
                  const char *APIKey,
                  const char *SystemMessage,
                  const char *UserMessage,
                  double Temperature = 1.0,
                  const char *CacheFile = NULL);

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
                    double Temperature = 1.0,
                    const char *CacheFile = NULL);

// ************************
// **** cURL_Anthropic ****
// ************************

char* cURL_Anthropic(cURLRunner &cURL,
                     const char *URLEndpoint,
                     const char *AIModel,
                     const char *APIKey,
                     const char *SystemMessage,
                     const char *UserMessage,
                     double Temperature = 1.0,
                     const char *CacheFile = NULL);

// ****************************************************************************
// **** ExtractAIContent ******************************************************
// ***********************
//
//      Steps:
//              1. Parse the JSON AIResponse from a flavor of cURL_xxx():
//
//                      Node *AIResponseNode = ParseJSON_mt(AIResponse);
//
//              2. Extract the content (oringally escaped and quoted):
//
//                      size_t ContentLength = 0;
//                      const char *Content = ExtractAIContent(URLEndpoint,
//                                                             AIModel,
//                                                             AIResponseNode,
//                                                             &ContentLength);
// 
//                     Note: The URL and Model are deciphered for routing to the 
//                           correct extractor respective of the AI provider.
//
//              3. Use the content:
//
//                      printf("Content: %.*s",ContentLength,Content);
//
//              4. Free the Node parse tree and AIResponse buffer:
//
//                      DeleteJSON_mt(AIResponseNode);
//                      cURL_FreeResponseBuffer(&AIResponse);
//
// ****************************************************************************

const char* ExtractAIContent(const char *URLEndpoint,
                             const char *AIModel, 
                             Node *AIResponseNode,
                             size_t *Content_Length);

/* **************************************************************************
 ****************************************************************************

 Notes:

 ****************************************************************************
 **** Example ***************************************************************
 *************

 ****************************************************************************

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __AI_via_cURL_H */
