///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SqlToken.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved.   
//                    
// Author(s) : Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#ifndef __SQL_TOKEN_HPP
        #define __SQL_TOKEN_HPP

#include <LinkList.h>

class SqlToken : public Link
        {
        private:
                char* TokenString;
                size_t TokenLen;
        public:
                SqlToken(const char* TokenString, size_t TokenLen);
                ~SqlToken(void);
                char* getToken();
                size_t getLength() {return TokenLen;};
        };

DefineLinkList(SqlTokenList, SqlToken);

#endif /* __SQL_TOKEN_HPP */