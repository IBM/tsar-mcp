///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SelectBuilder.cpp
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
#include <util.h>
#include <stdlib.h>

#include <LevelTrace.h>

#include <SelectBuilder.h>

#define SELECT_PACKAGE_LIB "QUERYTOOL"
#define SELECT_PACKAGE_NAME "SELECT"

SelectBuilder::SelectBuilder(char* String)
        {
        const char* ProcName = "SelectBuilder::SelectBuilder";
        TableString = NULL;
        SelectStatement = NULL;
        ColumnTypes = NULL;
        BuilderFailed = false;
        size_t allocSize = 0;
        if(String)
                {
                allocSize = (strlen(String)+1) * sizeof(char);
                TableString = (char*) malloc(allocSize);
                if(TableString == NULL)
                        {
                        TERROR(("%s: malloc(%d) Failed",ProcName,allocSize));
                        BuilderFailed = true;
                        return;
                        }
                strcpy(TableString, String);
                }
        else BuilderFailed = true;
        if(!BuilderFailed) BuildSelect();
        if(!BuilderFailed) PerformSelect();
        if(!BuilderFailed) ProcessDA();
        //if(!BuilderFailed) PrintTypes();
        return;
        }


SelectBuilder::~SelectBuilder(void)
        {
        free(TableString);
        free(SelectStatement);
        free(ColumnTypes);
        }


void SelectBuilder::BuildSelect()
        {
        const char* ProcName = "SelectBuilder::BuildSelect";
        size_t allocSize = 0;
        // check if columns are specified
        if(strchr(TableString, '(') == NULL &&
           strchr(TableString, ')') == NULL)
                {
                char* AllColumnSelect = "SELECT * FROM ";
                allocSize = (strlen(AllColumnSelect) + strlen(TableString) + 1) * sizeof(char);
                SelectStatement = (char*) malloc(allocSize);
                if(SelectStatement == NULL)
                        {
                        TERROR(("%s: malloc(%d) Failed",ProcName,allocSize));
                        BuilderFailed = true;
                        return;
                        }
                SelectStatement[0] = '\0';
                strcat(SelectStatement, AllColumnSelect);
                strcat(SelectStatement, TableString);
                return;
                }
        else
                {
                char* Select = "SELECT ";
                char* From = " FROM ";
                char* endOfTable = strchr(TableString, '(');
                unsigned int tableOffset = endOfTable - TableString;
                allocSize = (strlen(Select) + strlen(From) + strlen(TableString-1));
                SelectStatement = (char*) calloc(allocSize, sizeof(char));
                if(SelectStatement == NULL)
                        {
                        TERROR(("%s: calloc(%d, sizeof(char)) Failed",ProcName,allocSize));
                        BuilderFailed = true;
                        return;
                        }
                SelectStatement[0] = '\0';
                strcat(SelectStatement, Select);
                strncat(SelectStatement, TableString+tableOffset+1, strlen(TableString)-(tableOffset+2));
                strcat(SelectStatement, From);
                strncat(SelectStatement, TableString, tableOffset);
                //SelectStatement[allocSize-1] = '\0'; 
                }
        return;
        }

void SelectBuilder::PerformSelect()
        {
        const char* ProcName = "SelectBuilder::PerformSelect";
        char StatementName[MAX_STMT_NAME_LENGTH+1];
        int rc = 0;
        if(!GetUniqueStatementName(StatementName))
                {
                BuilderFailed = true;
                return;
                }
        PRCED_Statement Dummy("", SELECT_PACKAGE_LIB, SELECT_PACKAGE_NAME);
        bool alreadyPrepared = Dummy.FindStatementName(SelectStatement);
        if(!alreadyPrepared)
                {
                PRCED_Statement Statement(StatementName, SELECT_PACKAGE_LIB, SELECT_PACKAGE_NAME);
                rc = Statement.CreatePackage();
                if (rc == 0)
                        {
                        TINFO(("%s: Package %s/%s Created",ProcName,SELECT_PACKAGE_LIB,SELECT_PACKAGE_NAME));
                        }
                else if (rc == -601)
                        {
                        TINFO(("%s: %s/%s Already Exists",ProcName,SELECT_PACKAGE_LIB,SELECT_PACKAGE_NAME));
                        }
                else 
                        {
                        TERROR(("%s: Could neither create nor use Package %s/%s",ProcName,SELECT_PACKAGE_LIB,SELECT_PACKAGE_NAME));
                        BuilderFailed = true;
                        return;
                        }
                        rc = Statement.PrepareDescribe(SelectStatement, DescribeVars);
                        if (rc == 0)
                                {
                                TINFO(("%s: Performed PrepareDescribe of %s",ProcName,SelectStatement));
                                }
                        else
                                {
                                BuilderFailed = true;
                                return;
                                }
                } else {
                        rc = Dummy.Describe(DescribeVars);
                        if (rc == 0)
                                {
                                TINFO(("%s: Performed Describe of %s",ProcName,SelectStatement));
                                }
                        else
                                {
                                BuilderFailed = true;
                                return;
                                }
                }
                /*
		rc = Statement.DeletePackage();
		if (rc == 0)
			{
			TINFO(("%s: Successfully Deleted SQL Package %s/%s",ProcName,SELECT_PACKAGE_LIB,SELECT_PACKAGE_NAME));
			}
		else
			{
			TERROR(("%s: Error Deleting Temporary Package %s/%s",ProcName,SELECT_PACKAGE_LIB,SELECT_PACKAGE_NAME));
			}
                */
        return;
        }

void SelectBuilder::ProcessDA()
        {
        const char* ProcName = "SelectBuilder::ProcessDA";
        size_t allocSize = DescribeVars.GetColumnCount() * sizeof(DBType_t);
        ColumnTypes = (DBType_t*) malloc(allocSize);
        if (ColumnTypes == NULL)
                {
                TERROR(("%s: malloc(%d) failed",ProcName,allocSize));
                BuilderFailed = true;
                return;
                }
        for(int i = 0; i < DescribeVars.GetColumnCount(); i++)
                {
                ColumnTypes[i] = (DBType_t) DescribeVars.GetVariableType(i);
                }
        return;
        }

void SelectBuilder::PrintTypes()
        {
        for(int i = 0; i < DescribeVars.GetColumnCount(); i++)
                {
                switch(ColumnTypes[i])
                        {
                        case BIG_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"BIG_TYPE",ColumnTypes[i]));
                                break;
                        case INT_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"INT_TYPE",ColumnTypes[i]));
                                break;
                        case SHORT_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"SHORT_TYPE",ColumnTypes[i]));
                                break;
                        case CHAR_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"CHAR_TYPE",ColumnTypes[i]));
                                break;
                        case VARCHAR_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"VARCHAR_TYPE",ColumnTypes[i]));
                                break;
                        case WCHAR_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"WCHAR_TYPE",ColumnTypes[i]));
                                break;
                        case VARWCHAR_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"VARWCHAR_TYPE",ColumnTypes[i]));
                                break;
                        case FLOAT_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"FLOAT_TYPE",ColumnTypes[i]));
                                break;
                        case PACKED_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"PACKED_TYPE",ColumnTypes[i]));
                                break;
                        case TIMESTAMP_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"TIMESTAMP_TYPE",ColumnTypes[i]));
                                break;
                        // Lob
                        case BLOB_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"BLOB_TYPE",ColumnTypes[i]));
                                break;
                        case CLOB_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"CLOB_TYPE",ColumnTypes[i]));
                                break;
                        case DBCLOB_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"DBCLOB_TYPE",ColumnTypes[i]));
                                break;
                        // XML
                        case XML_TYPE:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"XML_TYPE",ColumnTypes[i]));
                                break;
                        default:
                                TINFO(("Column %d contains variable of type %s (%d)",i+1,"UNKNOWN",ColumnTypes[i]));
                                break;
                        }
                }
        return;
        }