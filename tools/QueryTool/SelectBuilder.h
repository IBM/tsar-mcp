///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SelectBuilder.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved.
//                    
// Author(s) : Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#ifndef __SELECT_BUILDER_HPP
        #define __SELECT_BUILDER_HPP

#include <PRCEDpp.h>

typedef enum DBType {
        BIG_TYPE = DA_BIG_TYPE,
        INT_TYPE = DA_INT_TYPE,
        SHORT_TYPE = DA_SHORT_TYPE,
        CHAR_TYPE = DA_CHAR_TYPE,
        VARCHAR_TYPE = DA_VARCHAR_TYPE,
        WCHAR_TYPE = DA_WCHAR_TYPE,
        VARWCHAR_TYPE = DA_VARWCHAR_TYPE,
        FLOAT_TYPE = DA_FLOAT_TYPE,
        PACKED_TYPE = DA_PACKED_TYPE,
        TIMESTAMP_TYPE = DA_TIMESTAMP_TYPE,
        // Lob
        BLOB_TYPE = DA_BLOB_TYPE, 
        CLOB_TYPE = DA_CLOB_TYPE, 
        DBCLOB_TYPE = DA_DBCLOB_TYPE,
        /*
        // Lob-Locator
        BLOBLOC_TYPE = DA_BLOBLOC_TYPE,
        CLOBLOC_TYPE = DA_CLOBLOC_TYPE,
        DBCLOBLOC_TYPE = DA_DBCLOBLOC_TYPE,
        */
        // XML
        XML_TYPE = DA_XML_TYPE
        } DBType_t;

class SelectBuilder
        {
        private:
                char* TableString;
                char* SelectStatement;
                DBType_t* ColumnTypes;
                DAVariables DescribeVars;
                bool BuilderFailed;
                void BuildSelect();
                void PerformSelect();
                void ProcessDA();
                void PrintTypes();
        public:
                SelectBuilder(char* String);
                ~SelectBuilder(void);
                DBType_t* getColumnTypes() {return ColumnTypes;};
                bool Failed() {return BuilderFailed;};
        };

#endif /* __SELECT_BUILDER_HPP */