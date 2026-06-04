// JSON Parser: JSONParser.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParser.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2016 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//

#ifndef __MarkupLanguage_Parser

        #define __MarkupLanguage_Parser

#include <stdarg.h>

#include <_JSONParser.h>
#include <JSONParserDetect.h>
#include <JSONParserExp.h>

#define Error_NOSTATE ((unsigned)-1)

class Error_ParseJSON : public ErrorBase
        {
        private:
                bool ItemInit;
                JSONLexicalItem Item;
                unsigned ParserState;
        public:
                Error_ParseJSON(const char *Text,
                                JSONLexicalItem *Item=NULL,
                                class JSONExpressionSN *Parser=NULL);
                JSONLexicalItem* GetItem() {return ItemInit ? &Item : NULL;}
                unsigned GetParserState() {return ParserState;}
        };

Node* ParseJSONBuffer(StateMachine &LRMachine,
                      JSONSymbolStateMachine &LexMachine,
                      const char *JSONBuffer,
                      bool AllowHTJSONSingletons = false,
                      Node *StartNode = NULL,
                      const char *NonTerminalGoal = NULL);

// ***************
// **** Debug ****
// ***************

void ParseJSONBuffer_SetWatch(void (*fn)(Transition*, inputT));
void ParseJSONBuffer_SetWatch(void (*fn)(Transition*, Node*));

// ************************************
// **** Print Parsed JSON Document ****
// ************************************

typedef int (*PrintJSONDocumentFn_t)(void *UserPtr,
                                     const char *Format,
                                     va_list Args);

int PrintJSONDocument(Node *Expression,                 // Returns number of
                      PrintJSONDocumentFn_t PrintFn,    // bytes output.
                      void *PrintUserPtr,
                      bool Pretty=true);

int PrintJSONDocument(Node *Expression, bool Pretty=true);      // To stdout.

/* **************************************************************************
 ****************************************************************************

 Notes:

     I. Setup:

                JSONSymbolStateMachine LexMachine;
                StateMachine LRMachine(0);

                SetupJSONTransitions(LRMachine);
                BuildJSONDetectTables();

                ParseJSONBuffer_SetWatch(WatchNodeTransition);
                ParseJSONBuffer_SetWatch(WatchDataTransition);

    II. Use:

                Node* JSONExpression = NULL;
                try     {
                        JSONExpression = ParseJSONBuffer(LRMachine,
                                                         LexMachine,
                                                         JSONBuffer,
                                                         true,
                                                         StartNode,
                                                         NonTerminalGoal);
                        }
                catch (Error_ParseJSON &PE)
                        {
                        }
                if (JSONExpression)
                        {
                        PrintExpression(JSONExpression);
                        delete JSONExpression;
                        }
        	return JSONExpression != NULL;

   III. Functions:

              - Node* ParseJSONBuffer(StateMachine &LRMachine,
                                      JSONSymbolStateMachine &LexMachine,
                                      const char *JSONBuffer,
                                      bool AllowHTJSONSingletons = false,
                                      Node *StartNode = NULL,
                                      const char *NonTerminalGoal = NULL)

                        Parses a well-formed XJSON or HTJSON document contained
                        in a null terminated 'JSONBuffer'. ParseJSONBuffer() can
                        also parse a sub-document as described in
                        ExpressionSN.h; sugest to use "JSONClause" as the
                        'NonTerminalGoal' when replacing a BracketExpression.

                        With 'AllowHTJSONSingletons' set to true, HTJSON special
                        singleton keywords like <BR> are accepted. Otherwise
                        only well-formed documents will parse without error.

              - ParseJSONBuffer_SetWatch(void (*fn)(Transition*, inputT))
                ParseJSONBuffer_SetWatch(void (*fn)(Transition*, Node*))

                        Set Debug Watch Points.


    IV. Print Document:

              - int PrintJSONDocument(Node *Expression,
                                      PrintJSONDocumentFn_t PrintFn,
                                      void *PrintUserPtr)

                        Prints a parsed JSON document. The 'PrintFn' takes
                        as first parameter 'PrintUserPtr', a format string
                        and an argument list.

              - int PrintJSONDocument(Node *Expression,
                                    PJSONCompression_t Compression)

                        Same as PrintJSONDocuemnt(Expression,vfprintf,stdout);

 ****************************************************************************
 **** Example ***************************************************************
 *************

        (See JSONParserTest.cpp)

 ****************************************************************************

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __MarkupLanguage_Parser */

