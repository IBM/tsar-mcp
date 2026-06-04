// Master Expression Control Include: Expression.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: Expression.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Define:
//
//              <none> : CPlusPlus
//              _USE_EXPRESSION_SN : CPlusPlus + State Nodes
//              _USE_EXPRESSION_S : Structures
//              _USE_EXPRESSION_SNS : Structures + State Nodes
//

#ifndef __Expression_Parser_StateNode
        #if defined(_USE_EXPRESSION_SN) || defined(_USE_EXPRESSION_SNS)
                #include <ExpressionSN.h>
        #endif
#endif

#include <string.h>
#include <Errors.h>
#include <LinkList.h>

// ****************************************************************************
// **** Memory Management *****************************************************
// ****************************************************************************

#ifndef DEFINED_ExpAlloc
        #ifdef _WIN32
                #define __ExpDECL __cdecl
        #else
                #define __ExpDECL
        #endif
        extern void* (__ExpDECL *ExpAlloc)(size_t Size);
        extern void (__ExpDECL *ExpDealloc)(void *Memory);
        extern void* (__ExpDECL *ExpTransAlloc)(size_t Size);
        extern void (__ExpDECL *ExpTransDealloc)(void *Memory);
#endif

void SetExpAlloc(void* (__ExpDECL *Alloc)(size_t), 
                 void (__ExpDECL *Dealloc)(void *));

void SetExpTransAlloc(void* (__ExpDECL *Alloc)(size_t), 
                      void (__ExpDECL *Dealloc)(void *));

// ****************************************************************************
// **** Class or Structure Definitions ****************************************
// ****************************************************************************

#if defined(_USE_EXPRESSION_S) || defined(_USE_EXPRESSION_SNS)
	#include <ExpressionS.h>
#else
        #include <ExpressionCPP.h>
#endif

