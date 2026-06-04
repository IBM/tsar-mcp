///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: util.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved.     
//                    
// Author(s) : Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <LevelTrace.h>

#include "util.h"

// data type for time stamp in case it is not defined in stdint.h or inttypes.h
#ifndef UINT64_MAX
        typedef unsigned long long int uint64_t;
#endif

/*===================================================================*/
/*                                                                   */
/* FUNCTION           sqltok                                         */
/*                                                                   */
/*===================================================================*/
/*                                                                   */
/*      Once per Program:       SQLSymbolStateMachine LexMachine;    */
/*      Tokenize State:         LexicalStream Lex(LexMachine);       */
/*      Start:                  Lex.Start(SQLText,strlen(SQLText));  */
/*      Loop while true:        sqltok(Lex,&Token,&TokenLen);        */
/*                                                                   */
/*      Example:                                                     */
/*              size_t TokenLen;                                     */
/*              const char *Token;                                   */
/*              LexicalStream Lex(LexMachine);                       */
/*                                                                   */
/*              Lex.Start(Alter,strlen(Alter));                      */
/*              while (sqltok(Lex,&Token,&TokenLen))                 */
/*                      {                                            */
/*                      printf("%.*s\n",TokenLen,Token);             */
/*                      }                                            */
/*                                                                   */
/*===================================================================*/

bool sqltok(LexicalStream &Lex, const char **Token, size_t *TokenLen)
        {
        LexicalItem Item;
        do      {
                const char *Pos = (char *)Lex.NextItem(&Item);
                if (!Pos)                               // End of input:
                        {
                        Lex.Restart("",1);              //      Push EOS.
                        Pos = Lex.NextItem(&Item);      //      Last item.
                        }
                } while (Item.SymbolClass != SC_Null &&
                         (Item.SymbolClass == SC_Space ||
                          Item.SymbolID == ST_Comment));
        if (Item.SymbolClass != SC_Null)
                {
                *Token = Item.String;
                *TokenLen = Item.Length;
                }
        return Item.SymbolClass != SC_Null;
        }

char* appendspace(char* dest) 
        {
        return strcat(dest, " ");
        }

bool isdelim(char c)
        {
        return (c == '/' || c == ',' || c == '.');
        }

bool GetUniqueStatementName(char* c_ptr)
        {
        const char* ProcName = "GetUniqueStatementName";

        clock_t Clock = clock();
        time_t Time = time(NULL);

        uint64_t timestamp = (Time * CLOCKS_PER_SEC) + (Clock % CLOCKS_PER_SEC);

        int src = sprintf(c_ptr, "ST%.16llX", timestamp);
        if(src < 0 || src > MAX_STMT_NAME_LENGTH) 
                {
                TERROR(("%s: Unable to create unique statement name!", ProcName));
                return false;
                }
        c_ptr[MAX_STMT_NAME_LENGTH] = '\0';
        return true;
        }

/**
 * stristr
 * Find string in string - case insensitive
 */
const char* stristr(const char *String, const char *ToFind)
    {
    const char *spos = String;
    const char *fpos = ToFind;
    while (*spos && *fpos)
        {
        String = spos;
        while (*spos && *fpos && toupper(*spos) == toupper(*fpos))
            {
            spos++;
            fpos++;
            }
        if (*fpos) // No match...
            {
            spos = String + 1;
            fpos = ToFind;
            }
        }
    return *fpos == '\0' ? String : NULL;
    }

