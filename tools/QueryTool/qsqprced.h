///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: qsqprced.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others 
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#ifndef __QSQPRCED_H

        #define __QSQPRCED_H
           
#define __QSQPRCED_VIA_XDA 1 

#if defined(_AIX)
        #include <qusec.h>
        #include "/sapmnt/depot/apps/IBM/PASE/v5r4m0/Include/qsqprced.h"
        #include <qxdaedrs.h>
#elif defined(__OS400_TGTVRM__)
        #include "/QIBM/include/qsqprced.h"
        #include <qxdaedrs.h>
#else
        #include <qxdaedrsnt.h>
#endif

// ----------------------------------------------------------
// Define following in any module (or link with QxdaUTIL.cpp)
// ----------------------------------------------------------

extern bool QSQPRCED_XDADeferOpen;

int Get_QSQPRCED_XDAHandle();

bool Set_QSQPRCED_XDAHandle(int Handle);

// -------------------------------
// ------ QSQPRCED via XDA -------
// -------------------------------

#define QSQPRCEDEx(sqlca,                                                   \
                   sqlda,                                                   \
                   format,                                                  \
                   qsq,                                                     \
                   exdo,                                                    \
                   exdo_size,                                               \
                   extdynopts,                                              \
                   ErrorCode)                                               \
        {                                                                   \
        int QSQPRCED_XDAHandle = Get_QSQPRCED_XDAHandle();                  \
        QxdaProcessExtDynEDRS(&QSQPRCED_XDAHandle,                          \
                              (Qsq_sqlda_t *)(sqlda),                       \
                              (Qsq_sqlca_t *)(sqlca),                       \
                              (char *)(format),                             \
                              (Qsq_SQLP0400_t *)(qsq),                      \
                              exdo,                                         \
                              exdo_size,                                    \
                              "EXDO0100",                                   \
                              extdynopts,                                   \
                              (Qus_EC_t *)(ErrorCode));                     \
        }

#define QSQPRCEDOpt(sqlca,                                                  \
                    sqlda,                                                  \
                    format,                                                 \
                    qsq,                                                    \
                    extdynopts,                                             \
                    ErrorCode)                                              \
        {                                                                   \
        Qxda_EXDO0100_t exdo;                                               \
        int exdo_size = sizeof(Qxda_EXDO0100_t);                            \
        int QSQPRCED_XDAHandle = Get_QSQPRCED_XDAHandle();                  \
        QSQPRCEDEx(sqlda,                                                   \
                   sqlca,                                                   \
                   format,                                                  \
                   qsq,                                                     \
                   &exdo,                                                   \
                   &exdo_size,                                              \
                   extdynopts,                                              \
                   ErrorCode);                                              \
        }

#define QSQPRCED(sqlca,                                                     \
                 sqlda,                                                     \
                 format,                                                    \
                 qsq,                                                       \
                 ErrorCode)                                                 \
        {                                                                   \
        int extdynopts = QXDA_EXTDYN_NOOPTS;                                \
        if ((qsq)->Function == '1' ||                                       \
            (qsq)->Function == '2' ||                                       \
            (qsq)->Function == '9')                                         \
                {                                                           \
                extdynopts |= QXDA_CREATE_OBJECTS;                          \
                }                                                           \
        else if ((qsq)->Function == '4' && QSQPRCED_XDADeferOpen)           \
                {                                                           \
                extdynopts |= QXDA_DEFER_OPEN;                              \
                }                                                           \
        int QSQPRCED_XDAHandle = Get_QSQPRCED_XDAHandle();                  \
        QSQPRCEDOpt(sqlda,                                                  \
                    sqlca,                                                  \
                    format,                                                 \
                    qsq,                                                    \
                    &extdynopts,                                            \
                    ErrorCode);                                             \
        }

#endif /* __QSQPRCED_H */
