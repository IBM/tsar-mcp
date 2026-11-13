// Provides simple wild-card comparisions and searches: MiniGREP.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MiniGREP.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <ctype.h>
#include <string.h>
#ifndef _WIN32
        #include <wchar.h>
#endif

#include <MiniGREP.h>

#define charcmp(x,y,c)                                                       \
        ( (c)                                                                \
          ? ((x) - (y))                                                      \
          : (( ((x) >= 'a' && (x) <= 'z') ? ((x) - 'a' + 'A') : (x) ) -      \
             ( ((y) >= 'a' && (y) <= 'z') ? ((y) - 'a' + 'A') : (y) )) )

#define wcharcmp(x,y,c)                                                      \
        ( (c)                                                                \
          ? ((x) - (y))                                                      \
          : (( ((x) >= L'a' && (x) <= L'z') ? ((x) - L'a' + L'A') : (x) ) -  \
             ( ((y) >= L'a' && (y) <= L'z') ? ((y) - L'a' + L'A') : (y) )) )

enum MatchState {MS_Exact,MS_Once,MS_Any};

// ****************************************************************************
// **** IsWildCardmatch *******************************************************
// ****************************************************************************

bool IsWildCardMatch(const char *ToMatch, 
                     const char *Query, 
                     size_t ToMatchLen,
                     bool Case)
        {                                               
        bool Finished = false;                          // 'Query' can contain
        MatchState State = MS_Exact;                    // wildcards ('*','?').
        const char *QueryRollback = NULL;               // Back to latest '*'.
        const char *WCEnd = NULL;

        while (!Finished)
                {
                if (*Query == '?') State = MS_Once;
                else if (*Query == '*') State = MS_Any;
                else if (*Query == '\0')
                        {
                        if (ToMatchLen) goto IWCM_EndOfQuery;
                        break;
                        }
                if (!ToMatchLen)
                        {
                        if (Query[1]) State = MS_Exact;
                        break;
                        }
                else switch (State)
                        {
                        case MS_Exact:
                                {
                                if (charcmp(*ToMatch,*Query,Case) != 0)
                                        {
  IWCM_EndOfQuery:                      if (QueryRollback)
                                                {
                                                Query = QueryRollback;
                                                ToMatchLen += ToMatch - WCEnd - 1;
                                                ToMatch = ++WCEnd;
                                                State = MS_Any;
                                                QueryRollback = NULL;
                                                continue;
                                                }
                                        Finished = true;
                                        break;
                                        }
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                break;
                                }
                        case MS_Once:
                                {
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                State = MS_Exact;
                                QueryRollback = NULL;
                                break;
                                }
                        case MS_Any:
                                {
                                if (!Query[1]) Finished = true;
                                else if (charcmp(*ToMatch,Query[1],Case) == 0)
                                        {
                                        QueryRollback = Query++;
                                        WCEnd = ToMatch;
                                        State = MS_Exact;
                                        }
                                else    {
                                        ToMatchLen--;
                                        ToMatch++;
                                        }
                                break;
                                }
                        }
                }
        return State == MS_Any ? true : !ToMatchLen && !*Query;
        }

bool IsWildCardMatch(const wchar_t *ToMatch, 
                     const wchar_t *Query, 
                     size_t ToMatchLen,         // Length in characters.
                     bool Case)
        {                                               
        bool Finished = false;                          // 'Query' can contain
        MatchState State = MS_Exact;                    // wildcards ('*','?').
        const wchar_t *QueryRollback = NULL;            // Back to latest '*'.
        const wchar_t *WCEnd = NULL;

        while (!Finished)
                {
                if (*Query == L'?') State = MS_Once;
                else if (*Query == L'*') State = MS_Any;
                else if (*Query == L'\0')
                        {
                        if (ToMatchLen) goto IWCM_EndOfQuery;
                        break;
                        }
                if (!ToMatchLen)
                        {
                        if (Query[1]) State = MS_Exact;
                        break;
                        }
                else switch (State)
                        {
                        case MS_Exact:
                                {
                                if (wcharcmp(*ToMatch,*Query,Case) != 0)
                                        {
  IWCM_EndOfQuery:                      if (QueryRollback)
                                                {
                                                Query = QueryRollback;
                                                ToMatchLen += ToMatch - WCEnd - 1;
                                                ToMatch = ++WCEnd;
                                                State = MS_Any;
                                                QueryRollback = NULL;
                                                continue;
                                                }
                                        Finished = true;
                                        break;
                                        }
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                break;
                                }
                        case MS_Once:
                                {
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                State = MS_Exact;
                                QueryRollback = NULL;
                                break;
                                }
                        case MS_Any:
                                {
                                if (!Query[1]) Finished = true;
                                else if (wcharcmp(*ToMatch,Query[1],Case)==0)
                                        {
                                        QueryRollback = Query++;
                                        WCEnd = ToMatch;
                                        State = MS_Exact;
                                        }
                                else    {
                                        ToMatchLen--;
                                        ToMatch++;
                                        }
                                break;
                                }
                        }
                }
        return State == MS_Any ? true : !ToMatchLen && !*Query;
        }

bool IsWildCardMatch(const char *ToMatch, const char *Query, bool Case)
        {
        return IsWildCardMatch(ToMatch,Query,strlen(ToMatch),Case);
        }

bool IsWildCardMatch(const wchar_t *ToMatch, const wchar_t *Query, bool Case)
        {
        return IsWildCardMatch(ToMatch,Query,wcslen(ToMatch),Case);
        }

// ****************************************************************************
// **** FindWildCards *********************************************************
// ****************************************************************************

bool FindWildCards(const char *ToMatch, 
                   const char *Query, 
                   size_t ToMatchLen,
                   OnWildCardFn_t OnWildCardFn,
                   void *UserPtr,
                   bool Case)
        {                                               
        bool Finished = false;                          // 'Query' can contain
        MatchState State = MS_Exact;                    // wildcards ('*','?').
        const char *QueryRollback = NULL;               // Back to latest '*'.
        const char *WCStart = NULL;
        const char *WCEnd = NULL;

        while (!Finished)
                {
                if (*Query == '?') State = MS_Once;
                else if (*Query == '*') State = MS_Any;
                else if (*Query == '\0')
                        {
                        if (ToMatchLen) goto FWC_EndOfQuery;
                        break;
                        }
                if (!ToMatchLen)
                        {
                        if (Query[1]) State = MS_Exact;
                        break;
                        }
                else switch (State)
                        {
                        case MS_Exact:
                                {
                                if (charcmp(*ToMatch,*Query,Case) != 0)
                                        {
  FWC_EndOfQuery:                       if (QueryRollback)
                                                {
                                                Query = QueryRollback;
                                                ToMatchLen += ToMatch - WCEnd - 1;
                                                ToMatch = ++WCEnd;
                                                State = MS_Any;
                                                QueryRollback = NULL;
                                                continue;
                                                }
                                        Finished = true;
                                        break;
                                        }
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                break;
                                }
                        case MS_Once:
                                {
                                if (WCStart)
                                        {
                                        OnWildCardFn(WCStart,WCEnd,UserPtr);
                                        WCStart = WCEnd = NULL;
                                        }
                                OnWildCardFn(ToMatch,ToMatch+1,UserPtr);
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                State = MS_Exact;
                                QueryRollback = NULL;
                                break;
                                }
                        case MS_Any:
                                {
                                if (QueryRollback)
                                        {
                                        OnWildCardFn(WCStart,WCEnd,UserPtr);
                                        WCStart = WCEnd = ToMatch;
                                        QueryRollback = NULL;
                                        }
                                else if (!WCStart) WCStart = WCEnd = ToMatch;
                                if (charcmp(*ToMatch,Query[1],Case) == 0)
                                        {
                                        QueryRollback = Query++;
                                        State = MS_Exact;
                                        }
                                else    {
                                        ToMatchLen--;
                                        ToMatch++;
                                        WCEnd = ToMatch;
                                        }
                                break;
                                }
                        }
                }
        if (WCStart) OnWildCardFn(WCStart,WCEnd,UserPtr);
        return State == MS_Any ? true : !ToMatchLen && !*Query;
        }

bool FindWildCards(const char *ToMatch, 
                   const char *Query, 
                   OnWildCardFn_t OnWildCardFn,
                   void *UserPtr,
                   bool Case)   
        {
        size_t ToMatchLen = strlen(ToMatch);
        bool rc = FindWildCards(ToMatch,
                                Query,
                                ToMatchLen,
                                OnWildCardFn,
                                UserPtr,
                                Case);
        return rc;
        }

// ****************************************************************************
// **** FindWildCardMatch *****************************************************
// ****************************************************************************

static const char* FindWildCardEnd(const char *ToMatch, 
                                   const char *Query,
                                   size_t ToMatchLen,
                                   bool Case)
        {                                               // 'Query' can contain
        bool Finished = false;                          // wildcards ('*','?').
        MatchState State = MS_Exact;                    // Returns pointer to
        const char *QueryRollback = NULL;               // end of matched data
        const char *WCEnd = NULL;                       // or NULL.

        while (!Finished)                     
                {
                if (*Query == '?') State = MS_Once;
                else if (*Query == '*') State = MS_Any;
                else if (*Query == '\0') break;
                if (!ToMatchLen)
                        {
                        if (Query[1]) State = MS_Exact;
                        break;
                        }
                else switch (State)
                        {
                        case MS_Exact:
                                {
                                if (charcmp(*ToMatch,*Query,Case) != 0)
                                        {
                                        if (QueryRollback)
                                                {
                                                Query = QueryRollback;
                                                ToMatchLen += ToMatch - WCEnd - 1;
                                                ToMatch = ++WCEnd;
                                                State = MS_Any;
                                                QueryRollback = NULL;
                                                continue;
                                                }
                                        Finished = true;
                                        break;
                                        }
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                break;
                                }
                        case MS_Once:
                                {
                                ToMatchLen--;
                                ToMatch++;
                                Query++;
                                State = MS_Exact;
                                QueryRollback = NULL;
                                break;
                                }
                        case MS_Any:
                                {
                                if (!Query[1])  // * to EOL - Quick finish.
                                        {
                                        ToMatch += ToMatchLen;
                                        ToMatchLen = 0;
                                        Finished = true;
                                        }
                                else if (charcmp(*ToMatch,Query[1],Case) == 0)
                                        {
                                        QueryRollback = Query++;
                                        WCEnd = ToMatch;
                                        State = MS_Exact;
                                        }
                                else    {
                                        ToMatchLen--;
                                        ToMatch++;
                                        }
                                break;
                                }
                        }
                }
        return State == MS_Any ? ToMatch : (*Query ? NULL : ToMatch);
        }

static const char* FindWildCardEnd(const char *ToMatch,
                                   const char *Query,
                                   bool Case)
        {
        return FindWildCardEnd(ToMatch,Query,strlen(ToMatch),Case);
        }

bool FindWildCardMatch(const char *InString, // [IN] Input string to search.
                       const char *Query,    // [IN] Search string.
                       const char **Start,   // [OUT] Begining of match.
                       const char **End,     // [OUT] End of match+1.
                       bool Case)
        {                                    // Returns true if match found.
        while (*InString)
                {
                *Start = InString;
                *End = FindWildCardEnd(InString,Query,Case);
                if (*End) return true;
                InString++;
                }
        *Start = *End = NULL;
        return false;
        }

bool FindWildCardMatch(const char *InString, // [IN] Input string to search.
                       const char *Query,    // [IN] Search string.
                       size_t InStringLen,   // [IN] Length of InString.
                       const char **Start,   // [OUT] Begining of match.
                       const char **End,     // [OUT] End of match+1.
                       bool Case)

        {                                    // Returns true if match found.
        while (InStringLen)
                {
                *Start = InString;
                *End = FindWildCardEnd(InString,Query,InStringLen,Case);
                if (*End) return true;
                InStringLen--;
                InString++;
                }
        *Start = *End = NULL;
        return false;
        }

// ****************************************************************************        
// ******************************* End of File ********************************
// ****************************************************************************
