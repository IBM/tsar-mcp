///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SimpleSqlParser.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved.         
//                    
// Author(s) : Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#ifndef __SIMPLE_SQL_PARSER_HPP
        #define __SIMPLE_SQL_PARSER_HPP

#include <SqlToken.h>

#define STMT_MAX_LENGTH 32000

typedef enum StatementType
        {
        INSERT,
        MERGE
        } StatementType_t;

class SimpleSqlParser
        {
        private:
                StatementType_t Type;
                SqlTokenList Tokens;
                char* UserStatement;
                char GenericStatement[STMT_MAX_LENGTH];
                char*** Values;
                size_t* MaxSizes;
                int RowCount;
                int ColumnCount;
                bool ParsingFailed;
                char* TableString;
                void createTokenList();
                void createGenericInsertStatement();
                void createGenericMergeStatement();
                void createValueArray(SqlToken* valueString);
                void setUserStatement(const char* String);
        public:
                SimpleSqlParser(const char* String, StatementType_t type);
                ~SimpleSqlParser(void);
                char* getUserStatement() {return UserStatement;};
                char* getGenericStatement() {return GenericStatement;};
                char*** getValues() {return Values;};
                size_t* getMaxSizes() {return MaxSizes;};
                char* getTableString() {return TableString;};
                int getRowCount() {return RowCount;};
                int getColumnCount() {return ColumnCount;};
                bool Failed() {return ParsingFailed;};
        };

#endif /* __SIMPLE_SQL_PARSER_HPP */