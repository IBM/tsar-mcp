// Level Trace : LevelTrace.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: LevelTrace.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2004 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Defines: SetLevelTraceFunction()
//
//               TERROR(())
//               TINFO(())
//               TDEBUG(())
//
//               GetMaxTraceLevel()
//               SetMaxTraceLevel()
//

#ifndef __Level_Trace

        #define __Level_Trace

#include <stdarg.h>

#define TRACELEVEL_NONE 0
#define TRACELEVEL_ERROR 1
#define TRACELEVEL_INFO 2
#define TRACELEVEL_DEBUG 3

typedef int (*LevelTraceFunction_t)(void *UserPtr,
                                    unsigned Level,
                                    const char *Format,
                                    va_list Args);

bool SetLevelTraceFunction(LevelTraceFunction_t Function, void *UserPtr);

void LTWriteLogPrint(const char *Format, ...);
void LTWriteLogError(const char *Format, ...);
void LTWriteLogInfo(const char *Format, ...);
void LTWriteLogDebug(const char *Format, ...);

#define TPRINT(x) LTWriteLogPrint x
#define TERROR(x) LTWriteLogError x
#define TINFO(x) LTWriteLogInfo x
#define TDEBUG(x) LTWriteLogDebug x

// **********************************************************
// **** Maximum Trace Level for DEFAULT Trace Funnctions ****
// **********************************************************

unsigned GetMaxTraceLevel();
void SetMaxTraceLevel(unsigned Level);  

// ***********************************
// **** Default TraceLevelFuncion ****
// ***********************************

int Default_TraceLevelFunction(void *,                  // No UserPtr.
                               unsigned Level,
                               const char *Format,
                               va_list Args);

/* ***************************************************************************
 Notes:

     I. Using Trace:

                Trace macros work like printf() functions. The input
                to the trace functions must be surrounded by a two
                sets of parenthesis.

                        TINFO(("Trace output of a %s","string"));

    II. User Trace Function:

             A. A user defined Trace Function can be set with:
             
                      - SetLevelTraceFunction()

                The user trace function can also call back to the
                default console trace function:

                      - Default_TraceLevelFunction(NULL,Level,Format,Args);

******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

*****************************************************************************/

#endif
