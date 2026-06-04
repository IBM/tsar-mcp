///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: qusec.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others 
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#if defined(_AIX)
        #include "/sapmnt/depot/apps/IBM/DB4/v5r3m0/Include/qusec.h"
#elif defined(__OS400_TGTVRM__)
        #include "/QIBM/include/qusec.h"
#else
        #include <qxdaedrsnt.h>
#endif
