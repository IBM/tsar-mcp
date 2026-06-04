// <<<<Generated>>> _JSONParser.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: _JSONParser.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __GUARD_JSONParser

        #define __GUARD_JSONParser

#if !defined(_USE_EXPRESSION_S) && !defined(_USE_EXPRESSION_SNS)
        #error Wrong _USE_EXPRESSION Defined
#endif

// ************************************************************
// ************************************************************

// 1) G -> .JSONDocument 
//
//	First Set: OpenBrace
//	Follow Set: EOL
//
// 2) JSONDocument -> .JSONObject 
//
//	First Set: OpenBrace
//	Follow Set: EOL
//
// 3) JSONObject -> .OpenBrace JSONMemberList CloseBrace 
//
//	First Set: OpenBrace
//	Follow Set: EOL, CloseBrace, comma, CloseBracket
//
// 4) JSONObject -> .OpenBrace CloseBrace 
//
//	First Set: OpenBrace
//	Follow Set: EOL, CloseBrace, comma, CloseBracket
//
// 5) JSONMemberList -> .JSONMemberList comma JSONMember 
//
//	First Set: String
//	Follow Set: CloseBrace, comma
//
// 6) JSONMemberList -> .JSONMember 
//
//	First Set: String
//	Follow Set: CloseBrace, comma
//
// 7) JSONMember -> .JSONName colon JSONValue 
//
//	First Set: String
//	Follow Set: CloseBrace, comma
//
// 8) JSONName -> .String 
//
//	First Set: String
//	Follow Set: colon
//
// 9) JSONValue -> .String 
//
//	First Set: String
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 10) JSONValue -> .Number 
//
//	First Set: Number
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 11) JSONValue -> .sign Number 
//
//	First Set: plus, minus
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 12) JSONValue -> .JSONObject 
//
//	First Set: OpenBrace
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 13) JSONValue -> .JSONArray 
//
//	First Set: OpenBracket
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 14) JSONValue -> .true 
//
//	First Set: true
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 15) JSONValue -> .false 
//
//	First Set: false
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 16) JSONValue -> .null 
//
//	First Set: null
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 17) JSONArray -> .OpenBracket JSONArrayList CloseBracket 
//
//	First Set: OpenBracket
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 18) JSONArray -> .OpenBracket CloseBracket 
//
//	First Set: OpenBracket
//	Follow Set: CloseBrace, comma, CloseBracket
//
// 19) JSONArrayList -> .JSONArrayList comma JSONValue 
//
//	First Set: 
//
//		String, Number, plus, minus, OpenBrace, OpenBracket, true, 
//		false, null
//
//	Follow Set: CloseBracket, comma
//
// 20) JSONArrayList -> .JSONValue 
//
//	First Set: 
//
//		String, Number, plus, minus, OpenBrace, OpenBracket, true, 
//		false, null
//
//	Follow Set: CloseBracket, comma
//
// 21) sign -> .plus 
//
//	First Set: plus
//	Follow Set: Number
//
// 22) sign -> .minus 
//
//	First Set: minus
//	Follow Set: Number
//

#include "JSONParserExp.h"

#if MAX_CHILDREN_NODES < 3
        #error MAX_CHILDREN_NODES Less than Required.
#endif

void SetupJSONTransitions(StateMachine &Machine);

// *************************
// **** Node Idenifiers ****
// *************************

#define Ew_GID 2
#define Ew_JSONDocumentID 3
#define Ew_JSONObjectID 4
#define Ew_JSONMemberListID 5
#define Ew_JSONMemberID 6
#define Ew_JSONNameID 7
#define Ew_JSONValueID 8
#define Ew_JSONArrayID 9
#define Ew_JSONArrayListID 10
#define Ew_signID 11
#define Ew_StringID 12
#define Ew_NumberID 13
#define Ew_TokenID 14
#define Ew_OpenBracketID 15
#define Ew_CloseBracketID 16
#define Ew_OpenBraceID 17
#define Ew_CloseBraceID 18
#define Ew_periodID 19
#define Ew_plusID 20
#define Ew_minusID 21
#define Ew_colonID 22
#define Ew_commaID 23
#define Ew_trueID 24
#define Ew_falseID 25
#define Ew_nullID 26

// ***********************************************************
#endif /* __GUARD_JSONParser */

