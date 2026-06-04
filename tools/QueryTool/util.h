///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: util.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved.
//                    
// Author(s) : Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#ifndef __UTIL_H
        #define __UTIL_H

#include <SQLLex.h>

#define MAX_STMT_NAME_LENGTH 18

bool sqltok(LexicalStream &Lex, const char **Token, size_t *TokenLen);

char* appendspace(char* dest);

bool isdelim(char c);

bool GetUniqueStatementName(char* c_ptr);

const char* stristr(const char *String, const char *ToFind);

#endif /* __UTIL_H */