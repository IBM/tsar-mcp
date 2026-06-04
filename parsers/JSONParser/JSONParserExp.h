// JSONParserExp.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParserExp.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2006 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __JSONParser_Expression

        #define __JSONParser_Expression

#ifdef __Expression_Parser
        #error JSONParserExp.h must be included before Expression.h
#endif

#define TerminalNodeBase JSONTerminalNode

#include <ExpressionSN.h>

class JSONTerminalNode : public TerminalNode
        {
        public:
                virtual ~JSONTerminalNode();
        };

void* JSONExpAlloc(size_t Size);
void JSONExpDealloc(void *Memory);
bool JSONExpPrealloc(unsigned nItems);
unsigned JSONExpGetDepth();

void SetMultithreadSafeAlloc(bool beSafe);      // Default: Compiler __MT__.

/* **************************************************************************
 ****************************************************************************

 Notes:

             I. Quick Memory Heap for JSON Expression Parser:

                        Call the following *BEFORE* calling NextInput()

                                - SetExpAlloc(JSONExpAlloc,JSONExpDealloc)

                        The heap can be Primed with nItems by calling

                                - JSONExpPrealloc()

                                        This function can be called anytime
                                        but only ONCE. The prime is much
                                        faster than individual allocations
                                        and is usefull for short lived
                                        programs that parse few trees.

                        The number of items remaining in the heap is

                                - JSONExpGetDepth()

                                        Call this after parsing and freeing
                                        an average sized document to determine
                                        a good priming value.

 ****************************************************************************
 **** Example ***************************************************************
 *************

 ****************************************************************************


 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __JSONParser_Expression */

