///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SqlToken.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved.     
//                    
// Author(s) : Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <stdlib.h>

#include <util.h>

#include <SqlToken.h>

#define trimquotes true

SqlToken::SqlToken(const char* String, size_t length)
        {
        TokenLen = length;
        if(trimquotes)
                {
                if(*String == '\'' || *String == '\"') 
                        {
                        String++;
                        TokenLen--;
                        }
                if(String[TokenLen-1] == '\'' || String[TokenLen-1] == '\"') TokenLen--; 
                }
        TokenString = (char*) malloc((TokenLen+1)*sizeof(char));
        TokenString = strncpy(TokenString, String, TokenLen);
        TokenString[TokenLen] = '\0';
        TokenLen = strlen(TokenString);
        return;
        }


SqlToken::~SqlToken(void)
        {
        free(TokenString);
        }

char* SqlToken::getToken() 
        {
        return TokenString;
        }
