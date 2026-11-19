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

void SetJSONExpAllocLimit(unsigned MaxMBytes);  // 0 = Unlimited.
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

                        The memory the heap may take from malloc is bounded by

                                - SetJSONExpAllocLimit(MaxMBytes)

                                        In megabytes. The default is
                                        ExpAllocLIMITMB (JSONParserExp.cpp,
                                        override with -DExpAllocLIMITMB=n).
                                        0 means unlimited. Once the limit
                                        has been taken, an item the free
                                        list cannot supply fails the same
                                        way malloc failure does: the parser
                                        throws Error_Memory (or
                                        Error_ParseJSON for a lexical item).
                                        The heap never returns memory to the
                                        OS before exit, so the limit also
                                        bounds the memory it keeps between
                                        requests. The limit is shared by
                                        every thread using the heap.
                                        It counts parser items only: every
                                        item costs the same (the largest
                                        item size plus a header), and
                                        token text is not copied (items
                                        point into the input buffer).
                                        Set it BEFORE parsing.

 ****************************************************************************
 **** Example ***************************************************************
 *************

 ****************************************************************************


 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __JSONParser_Expression */

