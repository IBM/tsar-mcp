///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: SimpleSqlParser.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved.
//                    
// Author(s) : Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <util.h>
#include <LevelTrace.h>

#include <SimpleSqlParser.h>

#define UseSelectBuilder true 

SimpleSqlParser::SimpleSqlParser(const char* String, StatementType_t type)
        {
        static const char* ProcName = "SimpleSqlParser::SimpleSqlParser";
        Type = type;
        UserStatement = NULL;
        GenericStatement[0] = '\0';
        Values = NULL;
        MaxSizes = NULL;
        Tokens.SetAutoCleanup(true);
        RowCount = 0;
        ColumnCount = 0;
        ParsingFailed = false;
        TableString = NULL;
        if(Type == INSERT || Type == MERGE) setUserStatement(String);
        else
                {
                TERROR(("%s: Invalid Statement type in constructor.",ProcName));
                ParsingFailed = true;
                return;
                }
        }


SimpleSqlParser::~SimpleSqlParser(void)
        {
        free(UserStatement);
        free(MaxSizes);
        for(int i = RowCount-1; i >= 0; i--)
                {
                for(int j = ColumnCount-1; j >= 0; j--)
                        {
                        if(Values)free(Values[i][j]);
                        }
                if(Values)free(Values[i]);
                }
        free(Values);
        free(TableString);
        }

void SimpleSqlParser::setUserStatement(const char* String) 
        {
        static const char* ProcName = "SimpleSqlParser::setUserStatement";
        size_t allocSize = (strlen(String)+1)*sizeof(char);
        UserStatement = (char*) malloc(allocSize);
        if(!UserStatement)
                {
                TERROR(("%s: malloc(%d) Failed",ProcName,allocSize));
                ParsingFailed = true;
                return;
                }
        UserStatement = strcpy(UserStatement, String);
        createTokenList();
        if(Type == INSERT) createGenericInsertStatement();
        else if (Type == MERGE) createGenericMergeStatement();
        else ParsingFailed = true;
        return;
        }

void SimpleSqlParser::createTokenList() 
        {
        SQLSymbolStateMachine LexMachine;
        LexicalStream Lex(LexMachine);
        Lex.Start(UserStatement,strlen(UserStatement));
        const char* token;
        size_t tokenLen;
        while(sqltok(Lex, &token, &tokenLen)) 
                {
                SqlToken* New = new SqlToken(token, tokenLen);
                Tokens.AddBottom(New);
                }
        return;
        }

void SimpleSqlParser::createGenericMergeStatement()
        {
        const char* ProcName = "SimpleSqlParser::createGenericMergeStatement";
        size_t allocSize = 0;
        SqlToken* Current = Tokens.GetFirst();
        strcpy(GenericStatement, Current->getToken());
        appendspace(GenericStatement);
        if (Current) Current = Tokens.GetNext(Current);
        else
                {
                TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                ParsingFailed = true;
                return;
                }

        while(strnicmp(Current->getToken(), "USING", Current->getLength()) != 0)
                {
                strcat(GenericStatement, Current->getToken());
                if(Tokens.GetNext(Current) != NULL)
                        {
                        // only append space if token is no delimiter and not followed by a delimiter
                        if(!isdelim(Current->getToken()[Current->getLength()-1]) &&
                           !isdelim(Tokens.GetNext(Current)->getToken()[Tokens.GetNext(Current)->getLength()-1]))
                                {
                                appendspace(GenericStatement);
                                }
                        }
                else
                        {
                        TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                        ParsingFailed = true;
                        return;
                        }
                Current = Tokens.GetNext(Current);
                if(Current == NULL)
                        {
                        TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                        ParsingFailed = true;
                        return;
                        }
                }
        if(Current == NULL)
                {
                TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                ParsingFailed = true;
                return;
                }

        bool isSelect = false;
        while((strnicmp(Current->getToken(), "SELECT", Current->getLength()) != 0) &&
              (strnicmp(Current->getToken(), "VALUES", Current->getLength()) != 0)    )
                {
                strcat(GenericStatement, Current->getToken());
                if(Tokens.GetNext(Current) != NULL)
                        {
                        // only append space if token is no delimiter and not followed by a delimiter
                        if(!isdelim(Current->getToken()[Current->getLength()-1]) &&
                           !isdelim(Tokens.GetNext(Current)->getToken()[Tokens.GetNext(Current)->getLength()-1]))
                                {
                                appendspace(GenericStatement);
                                }
                        }
                else
                        {
                        TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                        ParsingFailed = true;
                        return;
                        }
                Current = Tokens.GetNext(Current);
                }
        if(Current == NULL)
                {
                TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                ParsingFailed = true;
                return;
                }

        SqlToken* ValueList = NULL;
        bool ContainsValues = false;
        if(strnicmp(Current->getToken(), "VALUES", Current->getLength()) == 0)
                {
                strcat(GenericStatement, Current->getToken());
                unsigned int open = 1;
                bool ColumnCountSet = false;
                Current = Tokens.GetNext(Current);
                if(Current == NULL)
                        {
                        TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                        ParsingFailed = true;
                        return;
                        }
                ValueList = Current;
                
                // exits loop if AS token OR 
                // closing parantheses for VALUES clause appears
                while( !((strnicmp(Current->getToken(), "AS", 2) == 0) && 
                       (Current->getLength() == 2)))
                        {                      
                        if(Current->getToken()[0] == '(') open++;
                        else if(Current->getToken()[0] == ')') open--;
                        else if(open == 1 && Current->getToken()[0] == ',')
                                {
                                RowCount++;
                                ColumnCountSet = true;
                                }
                        else if(open == 2 && Current->getToken()[0] == ',' && !ColumnCountSet) ColumnCount++;
                        else if(open == 0) break;
                        else ContainsValues = true;
                        Current = Tokens.GetNext(Current);
                        if(Current == NULL)
                                {
                                TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                                ParsingFailed = true;
                                return;
                                }
                        }
                }
        else isSelect = true;

        if(Current == NULL)
                {
                TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                ParsingFailed = true;
                return;
                }

        ColumnCount++; // we only counted commas not elements
        RowCount++; // same here

        if(!isSelect && ContainsValues) createValueArray(ValueList);
        
        // replace value statements with parameter markers
        char* marker = NULL;
        if(!isSelect)
                {
                if(!ContainsValues)
                        {
                        TERROR(("%s: No Values Specified.", ProcName));
                        ParsingFailed = true;
                        return;
                        }
                char* param_marker_s = "(CAST(? AS CLOB(32767))";                
                char* param_marker = ", CAST(? AS CLOB(32767))";
                char* param_marker_e = ")) ";
                
                allocSize = strlen(param_marker_s);
                allocSize += (ColumnCount-1) * strlen(param_marker);
                allocSize += strlen(param_marker_e);

                marker = (char*) calloc(allocSize+1, sizeof(char));
                if(!marker)
                        {
                        TERROR(("%s: calloc(%d, sizeof(char)) Failed",ProcName,allocSize));
                        ParsingFailed = true;
                        return;
                        }
                marker[0] = '\0';
                marker = strcat(marker, param_marker_s);                
                for(int i = 0; i < ColumnCount-1; i++)
                        {
                        marker = strcat(marker, param_marker);
                        }
                marker = strcat(marker, param_marker_e);

                strcat(GenericStatement, marker);
                free(marker);
                }

        appendspace(GenericStatement);

        while(Tokens.GetNext(Current) != NULL)
                {
                strcat(GenericStatement, Current->getToken());
                // only append space if token is no delimiter and not followed by a delimiter
                if(!isdelim(Current->getToken()[Current->getLength()-1]) &&
                   !isdelim(Tokens.GetNext(Current)->getToken()[Tokens.GetNext(Current)->getLength()-1]))
                        {
                        appendspace(GenericStatement);
                        }
                Current = Tokens.GetNext(Current);
                }
        strcat(GenericStatement, Current->getToken());
        return;
        }

void SimpleSqlParser::createGenericInsertStatement()
        {
        const char* ProcName = "SimpleSqlParser::createGenericInsertStatement";
        size_t allocSize = (strlen(UserStatement)+1) * sizeof(char);
        if(UseSelectBuilder)
                {
                TableString = (char*) malloc(allocSize);
                if(TableString == NULL)
                        {
                        TERROR(("%s: malloc(%d) Failed",ProcName,allocSize));
                        ParsingFailed = true;
                        return;
                        }
                TableString[0] = '\0';
                }
        unsigned int open = 0;
        bool ColumnCountSet = false;
        SqlToken* Current = Tokens.GetFirst();
        strcpy(GenericStatement, Current->getToken());
        appendspace(GenericStatement);

        if (Current) Current = Tokens.GetNext(Current);
        else
                {
                TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                ParsingFailed = true;
                return;
                }

        while(strnicmp(Current->getToken(), "VALUES", Current->getLength()) != 0)
                {
                strcat(GenericStatement, Current->getToken());
                if(UseSelectBuilder)
                        {
                        if(strnicmp(Current->getToken(), "INTO", Current->getLength()) != 0)
                                {
                                TableString = strcat(TableString, Current->getToken());
                                }
                        }
                // only append space if token is no delimiter and not followed by a delimiter
                if(Tokens.GetNext(Current) != NULL)
                        {
                        if(!isdelim(Current->getToken()[Current->getLength()-1]) &&
                           !isdelim(Tokens.GetNext(Current)->getToken()[Tokens.GetNext(Current)->getLength()-1]))
                                {
                                appendspace(GenericStatement);
                                }                
                } 
                else 
                        {
                        TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                        ParsingFailed = true;
                        return;
                        }
                Current = Tokens.GetNext(Current);
                }
    
        if(Current == NULL)
                {
                TERROR(("%s: Statement %s too short. Aborting.",ProcName, UserStatement));
                ParsingFailed = true;
                return;
                }

        // append ? ROWS
        char* rowString = " ? ROWS ";
        strcat(GenericStatement, rowString);
        // append VALUES
        strcat(GenericStatement, Current->getToken());

        Current = Tokens.GetNext(Current);
        SqlToken* valueStart = Current;
        bool ContainsValues = false;
        while(Current)
                {
                if(Current->getToken()[0] == '(') open++;
                else if(Current->getToken()[0] == ')') open--;
                else if(Current->getToken()[0] == ',' && open == 0)
                        {
                        RowCount++;
                        ColumnCountSet = true;
                        }
                else if(Current->getToken()[0] == ',' && open != 0 && !ColumnCountSet) ColumnCount++;
                else ContainsValues = true;
                Current = Tokens.GetNext(Current);
                }
        ColumnCount++; // we only counted commas not elements
        RowCount++; // same here

        if(open != 0)
                {
                TERROR(("%s: ')' missing.", ProcName));
                ParsingFailed = true;
                return;
                }
        
        if(!ContainsValues)
                {
                TERROR(("%s: No Values Specified.", ProcName));
                ParsingFailed = true;
                return;
                }

        char* param_marker_s = " (?";
        char* param_marker = ", ?";
        char* param_marker_e = ") ";
        char* marker = NULL;

        allocSize = strlen(param_marker_s);
        allocSize += (ColumnCount-1) * strlen(param_marker);
        allocSize += strlen(param_marker_e) + 1;
        marker = (char*) calloc(allocSize, sizeof(char));
        if(marker == NULL)
                {
                TERROR(("%s: calloc(%d, sizeof(char)) Failed.",ProcName,allocSize));
                ParsingFailed = true;
                return;
                }
        marker[0] = '\0';
        marker = strcat(marker, param_marker_s);       
        for(unsigned int i = 1; i < ColumnCount; i++) 
                {
                marker = strcat(marker, param_marker);
                }
        marker = strcat(marker, param_marker_e);

        strcat(GenericStatement, marker);
        free(marker);

        createValueArray(valueStart);
        return;
        }

void SimpleSqlParser::createValueArray(SqlToken* valueStart)
        {
        static const char* ProcName = "SimpleSqlParser::createValueArray";

        if(valueStart == NULL)
                {
                TERROR(("%s: Value list empty.",ProcName));
                ParsingFailed = true;
                return;
                }

        MaxSizes = (size_t*) calloc(ColumnCount, sizeof(size_t));
        if(!MaxSizes)
                {
                TERROR(("%s: calloc(%d, sizeof(size_t)) Failed",ProcName,RowCount));
                ParsingFailed = true;
                return;
                }
        
        SqlToken* Current = valueStart;
        Values = (char***) calloc(RowCount, sizeof(char**));
        if(!Values) 
                {
                TERROR(("%s: calloc(%d, sizeof(char**)) Failed",ProcName,RowCount));
                ParsingFailed = true;
                return;
                }
        for(int i = 0; i < RowCount; i++)
                {
                Values[i] = (char**)calloc(ColumnCount, sizeof(char*));
                if(!Values[i]) 
                        {
                        TERROR(("%s: calloc(%d, sizeof(char*)) Failed",ProcName,ColumnCount));
                        ParsingFailed = true;
                        return;
                        }
                }
        unsigned int j = 0;
        unsigned int k = 0;
        while(Current) 
                {
                if((Type == MERGE) && 
                   strnicmp(Current->getToken(), "AS", 2) == 0 &&
                   Current->getLength() == 2)
                                {
                                //Current = Tokens.GetNext(Current); // Current = <identifier>
                                //if (Current) Current = Tokens.GetNext(Current); // Current = ',' || ')'
                                //else return;
                                return;
                                }
                if(Type == MERGE && 
                   strnicmp(Current->getToken(), "ON", 2) == 0 &&
                   Current->getLength() == 2) return;

                if(Current->getToken()[0] != '(' && 
                   Current->getToken()[0] != ')' && 
                   Current->getToken()[0] != ',')
                        {
                        Values[j][k] = (char*) calloc(Current->getLength()+1, sizeof(char));
                        if(!Values[j][k]) 
                                {
                                TERROR(("%s: calloc(%d, sizeof(char)) Failed",ProcName,Current->getLength()+1));
                                ParsingFailed = true;
                                return;
                                }
                        if(MaxSizes[k] < Current->getLength())
                                {
                                MaxSizes[k] = Current->getLength();
                                }
                        Values[j][k] = strncpy(Values[j][k], Current->getToken(), Current->getLength());
                        Values[j][k][Current->getLength()] = '\0';
                        k++;
                        if(k >= ColumnCount) 
                                {
                                k = 0;
                                j++;
                                if(j >= RowCount)
                                        {
                                        break;
                                        }
                                }
                        }
                Current = Tokens.GetNext(Current);
                }
        return;
        }
