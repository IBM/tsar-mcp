///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: PRCEDPrint.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
/**
 * @file PRCEDPrint.h
 * PRCED Print Utilities
 */
#ifndef __PRCED_Print_Utilities

	#define __PRCED_Print_Utilities

#include <PRCEDpp.h>

/**
 * @param Heading Header to prepend
 * @param SQLText Text to print
 * @param nChars Number of characters to print,
 *        excludes Heading
 * @param CCSID CCSID from \ref ccsids
 */
void PrintTextCCSID(const char *Heading, 
                    const void *SQLText, 
                    size_t nChars, 
                    int CCSID = LOCAL_CCSID);

/**
 * Print type information of a Column
 * @param Variables SQLDA 
 * @param Col Column number
 */
void PrintColumn(DAVariables &Variables, int Col);

/**
 * Print value of Variable
 * @param Variables SQLDA
 * @param Row Row number
 * @param Col Column number
 */
void PrintVariable(DAVariables &Variables, int Row, int Col);

/**
 * Print LOB
 * @param ColumnName Column Name
 * @param Locator Locator object
 * @param MaxLen Maximum print out length
 * @return Size of printed string, 
 *         either length of LOB or MaxLen
 */
size_t PrintLOB(const char *ColumnName, 
                PRCED_Locator &Locator, 
                size_t MaxLen = (size_t)-1);

/**
 * Print LOB
 * @param Variables SQLDA
 * @param Row Row number
 * @param Col Column number
 * @param MaxLen Maximum print out length
 * @return Size of printed string, 
 *         either length of LOB or MaxLen
 */
size_t PrintLOB(DAVariables &Variables, 
                int Row, 
                int Col, 
                size_t MaxLen = (size_t)-1);

/** 
 * Prints error message of parameter SQLCode, if known 
 * @param SQLCode SQL return code
 */
void PrintSQLCodeText(int SQLCode);

/** Print statement text of parameter Statement */
void PrintSQLStatementText(PRCED_Statement &Statement);

#endif /* __PRCED_Print_Utilities */
