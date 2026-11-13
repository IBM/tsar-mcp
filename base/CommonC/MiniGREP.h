// Provides simple wild-card comparisions and searches: MiniGREP.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MiniGREP.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __MiniGREP_Search

        #define __MiniGREP_Search

// *************************
// **** IsWildCardmatch ****
// *************************
                  
bool IsWildCardMatch(const char *ToMatch,
                     const char *Query, 
                     size_t ToMatchLen,
                     bool Case=false);
                  
bool IsWildCardMatch(const wchar_t *ToMatch,
                     const wchar_t *Query, 
                     size_t ToMatchLen,
                     bool Case=false);

bool IsWildCardMatch(const char *ToMatch, 
                     const char *Query,
                     bool Case=false);

bool IsWildCardMatch(const wchar_t *ToMatch, 
                     const wchar_t *Query,
                     bool Case=false);

// ***********************
// **** FindWildCards ****
// ***********************

typedef void (*OnWildCardFn_t)(const char *Start, // [IN] Begining of match.
                               const char *End,   // [IN] End of match+1.   
                               void *UserPtr);    // [IN] Passthrough value.

bool FindWildCards(const char *ToMatch,
                   const char *Query, 
                   size_t ToMatchLen,
                   OnWildCardFn_t OnWildCardFn,
                   void *UserPtr,
                   bool Case=false);

bool FindWildCards(const char *ToMatch,
                   const char *Query,
                   OnWildCardFn_t OnWildCardFn,
                   void *UserPtr,
                   bool Case=false);

// ***************************
// **** FindWildCardMatch ****
// ***************************

bool FindWildCardMatch(const char *InString, // [IN] Input string to search.
                       const char *Query,    // [IN] Search string.
                       size_t InStringLen,   // [IN] Length of InString.
                       const char **Start,   // [OUT] Begining of match.
                       const char **End,     // [OUT] End of match+1.
                       bool Case=false);     // [IN] Case senstive.

bool FindWildCardMatch(const char *InString, // [IN] Input string to search.
                       const char *Query,    // [IN] Search string.
                       const char **Start,   // [OUT] Begining of match.
                       const char **End,     // [OUT] End of match+1.
                       bool Case=false);     // [IN] Case senstive.

/* ****************************************************************************
 Notes:

        - IsWildCardMatch(ToMatch, Query, Case=false)

                Compares the string 'ToMatch' to the string 'Query'. 'Query'
                may contain * and ? characters. Where * matches any number of
                any character and ? matches any one character.

                Returns TRUE if 'Query' matches 'ToMatch' exactly.

                The string comparision is case sensitive if 'Case' is true.

                Example
                        String = "The quick brown fox jumped over the lazy dog"
                        IsWildCardMatch(String,"The q*k *ver *dog") => TRUE

        - FindWildCards(InString, Query, OnWildCardFn, UserPtr, Case=false)

                Finds "variables" within InString. For each variable found
                (the associated value of '*' or '?'), the callback function
                OnWildCardFn() is called with the 'Start' and 'End' position
                (match + 1). 'UserPtr' is passed through to the callback 
                function.

        - FindWildCardMatch(InString, Query, Start, End, Case=false)

                Returns pointers to the 'Start' and 'End' (match + 1) inside 
                'InString' where 'Query' is found (like strstr()). 'Query' can 
                contain wild-cards as described for IsWildCardMatch().

*******************************************************************************
**** Example String Editor ****************************************************
***************************

static bool EditString(char **Source, const char *Find, const char *Replace)
        {
        if (!Source || !*Source || !Find || !Replace) return true;
        bool Found;
        const char *Start, *End;
        size_t ReplaceLen = strlen(Replace);
        Start = *Source;
        Found = FindWildCardMatch(Start,Find,&Start,&End);
        while (Found)
                {
                size_t SrcLength = Start - *Source;
                size_t EndLength = strlen(End);
                size_t TotalLength = SrcLength + ReplaceLen + EndLength + 1;
                char *Dest = (char *)malloc(TotalLength);
                if (!Dest) return false;
                memcpy(Dest,*Source,SrcLength);
                memcpy(Dest+SrcLength,Replace,ReplaceLen);
                char *EndPos = Dest + SrcLength + ReplaceLen;
                strcpy(EndPos,End);
                free(*Source);
                *Source = Dest;
                Start = EndPos;
                Found = FindWildCardMatch(Start,Find,&Start,&End);
                }
        return true;
        }

*******************************************************************************
**** Example: Find Variables **************************************************
*****************************

#define MAXWILDCARDVARS 10

struct WildCardVars
        {
        unsigned nVars;
        struct  {
                const char *Value;
                size_t Length;
                } Vars[MAXWILDCARDVARS];
        WildCardVars() {nVars = 0;}
        };

static void OnWildCard(const char *Start, const char *End, void *UserPtr)
        {
        WildCardVars &Vars = *(WildCardVars *)UserPtr;
        unsigned nVars = Vars.nVars;
        if (nVars < MAXWILDCARDVARS) 
                {
                Vars.Vars[nVars].Value = Start;
                Vars.Vars[nVars].Length = End - Start;
                Vars.nVars++;
                }
        return;
        }

main()
        {
        bool Match;
        WildCardVars Vars;
        const char *Query = "*h? * ?*ox*";
        const char *ToMatch = "The Quick Brown Fox";
        
        Match = FindWildCards(ToMatch,Query,OnWildCard,&Vars);
        printf("FindWildCards = %s\n",Match ? "Yes" : "No");
        for (unsigned i=0; i < Vars.nVars; i++)
                {
                printf("Varible %d: '%.*s'\n",i,
                                              Vars.Vars[i].Length,
                                              Vars.Vars[i].Value);
                }
        return 0;
        }

**************************************************************************** */

#endif
