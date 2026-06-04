// Find Node in Tree: FindNode.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: FindNode.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2006 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Find_Node_In_Tree

#include <Expression.h>

#define WILDCARD_TypeID ((unsigned)-1)

// ************************************
// **** Compare Function Prototype ****
// ************************************

typedef bool (*FindNodeCompareFn_t)(TerminalNode *Compare, const void *Value);

// ************************
// **** >> FindNode << ****
// ************************

Node* FindNode(Node *Expression, unsigned nTerms, ...);

// *************************************************************
// **** Compare Functions for Nodes containing LexicalItems ****
// *************************************************************

bool FN_LEX_strcmp(TerminalNode *ToCompare, const void *Value);
bool FN_LEX_stricmp(TerminalNode *ToCompare, const void *Value);

/* **************************************************************************
 ****************************************************************************

 Notes:

     I. FindNode:

              - TerminalNode* FindNode(Node *Expression, unsigned nTerms, ...)

                        Search down 'Expression' tree looking for a
                        sequence of ordered search terms. The function 
                        returns the node equal to the last term found.

                        '...' is a list search term specifiers; one for 
                              each 'nTerm':

                                TypeID - Identifier of Node

                                TermCompareFn - Function to compare terminals.
                                                (Can be used to just "visit")

                                TermCompareValue - Compare value sent to Fn.
                                                   (Can be any pointer)


                        Algorithm:

                             1. The search proceeds to match sebsequent 
                                terms, skipping nodes in the tree that 
                                do not match by TermID. 

                             2. When a terminal compare is false, the 
                                search continues at the previous wild-card 

                             3. Wild-cards (WILDCARD_TypeID) in the search
                                sequence match any number of terms in the
                                tree (As in step 1); but dean as roll-back
                                points for false matches (As in step 2).

                                A Wild-card is "active" as long as the
                                branches in the tree being searched are
                                deeper than the matched-term just below
                                the previous wild-card. Once the search
                                routine walks higher, the previous
                                wild-card is used, etc. ("Smart" wild-cards).

 ****************************************************************************
 **** Examples **************************************************************
 *************

     I. Given: MLClause                                          
                BracketExpression                                
                 SingletonClause                                 
                  [<]                                            
                  SingletonAttrList                              
                   [ReplacePoint] (Ew_SingletonAttributeID)      
                   AttributeList                                 
                    Attribute                                    
                     AttrName                                    
                      [Script] (Ew_TokenID)                      
                     [=]                                         
                     ValueSign                                   
                      Value                                      
                       ["UpdateScript"] (Ew_StringID)            
                  [/>]                                           

        __________________________
     A. Look for: '* ReplacePoint'

           FindNode(MLExpression,
                        2,
                        Ew_SingletonAttrListID,0,0,
                        Ew_SingletonAttributeID,FN_LEX_stricmp,"ReplacePoint");

        __________________________________________________
     B. Look for: '* ReplacePoint * Script="UpdateScript"'

           FindNode(MLExpression,
                        7,
                        Ew_SingletonAttrListID,0,0,
                        Ew_SingletonAttributeID,FN_LEX_stricmp,"ReplacePoint",
                        WILDCARD_TypeID,0,0,
                        Ew_AttrNameID,0,0,
                        Ew_TokenID,FN_LEX_stricmp,"Script",
                        Ew_ValueID,0,0,
                        Ew_StringID,FN_LEX_strcmp,"\"UpdateScript\"");

 ****************************************************************************

    II. Given:  SELECT FieldA, FieldB, FieldC "
                       FROM Eric.Addresses Addr 
                       WHERE Addr.FieldA = FieldB AND FieldC > 3 
                       ORDER BY FieldA ASC, FieldB ASC, FieldC ASC

        Resulting in the following WHERE parse tree:           

                SClause                         
                 [WHERE]                        
                 WE                             
                  PredicateList                 
                    Predicate                   
                        ...                    
                        ExpValue                
                         FieldName              
                          TableQualifier        
                           [Addr]               
                          [.]                   
                          FieldName             
                           [FieldA]             
                     cmp                        
                      [=]                       
                     Expression                 
                        ...                    
                        ExpValue                
                         FieldName              
                          [FieldB]              
                   bop                          
                    [AND]                       
                   Predicate                    
                        ...                    
                        FieldName               
                         [FieldC]               
                    cmp                         
                     [>]                        
                    Expression                  
                       ...                    
                       ExpValue                 
                        [3]                     
                SClauseList                     
                 SClause                        
                  [ORDER]                       
                  [BY]                          
                  OE                            
                   OrderList                    
                         ...                    
                         ExpValueSign           
                          ExpValue              
                           FieldName            
                            [FieldA]            
                       [ASC]                    
                     [,]                        
                     OrderEntry                 
                      Expression                
                         ...                    
                         ExpValue               
                          FieldName             
                           [FieldB]             
                      [ASC]                     
                    [,]                         
                    OrderEntry                  
                     Expression                 
                        ...                    
                        ExpValue                
                         FieldName              
                          [FieldC]              
                     [ASC]       
                     
        _______________________________
     A. Look for: '* ORDER BY * FieldB'

                TerminalNode* Found = (TerminalNode *)FindNode(SQLExpression,
                                4,
                                Ew_ORDERID,0,0,
                                WILDCARD_TypeID,0,0,
                                Ew_FieldNameID,0,0,
                                Ew_TokenTextID,FN_LEX_stricmp,"FieldB");

                if (Found)
                        {
                        LexicalItem *Item = (LexicalItem *)Found->GetValue();
                        printf("%.*s\n",Item->Length,Item->String);
                        }
                else printf("<None>\n");

        _______________________________________
     B. LIST all Fields in the WHERE Condition:

                class ListClass
                        {
                        public:
                                void Add(TerminalNode *ToCompare);
                        };

                void ListClass::Add(TerminalNode *ToCompare)
                        {
                        void *Value = ToCompare->GetValue();
                        LexicalItem *Item = (LexicalItem *)Value;
                        const char *Token = Item->String;
                        size_t TokenLength = Item->Length;
                        printf("Added: %.*s\n",TokenLength,Token);
                        return;
                        }

                bool FN_ListNodes(TerminalNode *ToCompare, const void *Value)
                        {
                        ListClass *List = (ListClass *)Value;
                        Node *Parent = ToCompare->GetParent();
                        unsigned ParentTypeID = Parent->GetTypeID()
                        if (ParentTypeID != Ew_TableQualifierID)
                                {
                                List->Add(ToCompare);
                                }
                        return false;           // Do not stop!
                        }

                main()  {
                        ListClass List;
                        FindNode(SQLExpression,
                                        4,
                                        Ew_WHEREID,0,0,
                                        WILDCARD_TypeID,0,0,
                                        Ew_FieldNameID,0,0,
                                        Ew_TokenTextID,FN_ListNodes,&List);
                        }

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __Find_Node_In_Tree */

