// <<<Generated>>> _JSONParser.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: _JSONParser.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */

#define ExNAMESPACE(x) ExJSONNS_##x

#include <_JSONParser.h>

#include <JSONParserDetect.h>

// ***********************************************************

// **************************
// **** Node Definitions ****
// **************************

DefineNodesBEGIN
	DefineNonTerminalNode(G,Ew_GID)
	DefineNonTerminalNode(JSONDocument,Ew_JSONDocumentID)
	DefineNonTerminalNode(JSONObject,Ew_JSONObjectID)
	DefineNonTerminalNode(JSONMemberList,Ew_JSONMemberListID)
	DefineNonTerminalNode(JSONMember,Ew_JSONMemberID)
	DefineNonTerminalNode(JSONName,Ew_JSONNameID)
	DefineNonTerminalNode(JSONValue,Ew_JSONValueID)
	DefineNonTerminalNode(JSONArray,Ew_JSONArrayID)
	DefineNonTerminalNode(JSONArrayList,Ew_JSONArrayListID)
	DefineNonTerminalNode(sign,Ew_signID)
	DefineTerminalNode(String,Ew_StringID)
	DefineTerminalNode(Number,Ew_NumberID)
	DefineTerminalNode(Token,Ew_TokenID)
	DefineTerminalNode(OpenBracket,Ew_OpenBracketID)
	DefineTerminalNode(CloseBracket,Ew_CloseBracketID)
	DefineTerminalNode(OpenBrace,Ew_OpenBraceID)
	DefineTerminalNode(CloseBrace,Ew_CloseBraceID)
	DefineTerminalNode(period,Ew_periodID)
	DefineTerminalNode(plus,Ew_plusID)
	DefineTerminalNode(minus,Ew_minusID)
	DefineTerminalNode(colon,Ew_colonID)
	DefineTerminalNode(comma,Ew_commaID)
	DefineTerminalNode(true,Ew_trueID)
	DefineTerminalNode(false,Ew_falseID)
	DefineTerminalNode(null,Ew_nullID)
DefineNodesEND

// ************************************************************
// ****  ============ Source File - Cut Here ============  ****
// ************************************************************

// ******************************************************
/*
// **** Input Detecting Functions (Manually Defined) ****
// ******************************************************

inline bool IsEOL(inputT input)
	{
	return false;
	}

inline bool IsString(inputT input)
	{
	return false;
	}

inline bool IsNumber(inputT input)
	{
	return false;
	}

inline bool IsToken(inputT input)
	{
	return false;
	}

inline bool IsOpenBracket(inputT input)
	{
	return false;
	}

inline bool IsCloseBracket(inputT input)
	{
	return false;
	}

inline bool IsOpenBrace(inputT input)
	{
	return false;
	}

inline bool IsCloseBrace(inputT input)
	{
	return false;
	}

inline bool Isperiod(inputT input)
	{
	return false;
	}

inline bool Isplus(inputT input)
	{
	return false;
	}

inline bool Isminus(inputT input)
	{
	return false;
	}

inline bool Iscolon(inputT input)
	{
	return false;
	}

inline bool Iscomma(inputT input)
	{
	return false;
	}

inline bool Istrue(inputT input)
	{
	return false;
	}

inline bool Isfalse(inputT input)
	{
	return false;
	}

inline bool Isnull(inputT input)
	{
	return false;
	}

// ****************************************
*/
using namespace _JSONParser;

// ****************************************
// **** Production Detecting Functions ****
// ****************************************

static bool DetectG1(inputT input)
	{
	bool Result = IsEOL(input);
	return Result;
	}

static bool DetectJSONDocument2(inputT input)
	{
	bool Result = IsEOL(input);
	return Result;
	}

static bool DetectJSONObject3(inputT input)
	{
	bool Result = IsEOL(input)
	         ||   IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONObject4(inputT input)
	{
	bool Result = IsEOL(input)
	         ||   IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONMemberList5(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input);
	return Result;
	}

static bool DetectJSONMemberList6(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input);
	return Result;
	}

static bool DetectJSONMember7(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input);
	return Result;
	}

static bool DetectJSONName8(inputT input)
	{
	bool Result = Iscolon(input);
	return Result;
	}

static bool DetectJSONValue9(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONValue10(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONValue11(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONValue12(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONValue13(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONValue14(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONValue15(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONValue16(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONArray17(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONArray18(inputT input)
	{
	bool Result = IsCloseBrace(input)
	         ||   Iscomma(input)
	         ||   IsCloseBracket(input);
	return Result;
	}

static bool DetectJSONArrayList19(inputT input)
	{
	bool Result = IsCloseBracket(input)
	         ||   Iscomma(input);
	return Result;
	}

static bool DetectJSONArrayList20(inputT input)
	{
	bool Result = IsCloseBracket(input)
	         ||   Iscomma(input);
	return Result;
	}

static bool Detectsign21(inputT input)
	{
	bool Result = IsNumber(input);
	return Result;
	}

static bool Detectsign22(inputT input)
	{
	bool Result = IsNumber(input);
	return Result;
	}

// ********************************
// **** Transition Definitions ****
// ********************************

DefineTransitionsBEGIN
	DefineSimpleTransition(ChangeG,Ew_GID)
	DefineSimpleTransition(ChangeJSONDocument,Ew_JSONDocumentID)
	DefineSimpleTransition(ChangeJSONObject,Ew_JSONObjectID)
	DefineSimpleTransition(ChangeJSONMemberList,Ew_JSONMemberListID)
	DefineSimpleTransition(ChangeJSONMember,Ew_JSONMemberID)
	DefineSimpleTransition(ChangeJSONName,Ew_JSONNameID)
	DefineSimpleTransition(ChangeJSONValue,Ew_JSONValueID)
	DefineSimpleTransition(ChangeJSONArray,Ew_JSONArrayID)
	DefineSimpleTransition(ChangeJSONArrayList,Ew_JSONArrayListID)
	DefineSimpleTransition(Changesign,Ew_signID)
	DefineReadTransition(StringRead,String,IsString)
	DefineReadTransition(NumberRead,Number,IsNumber)
	DefineReadTransition(TokenRead,Token,IsToken)
	DefineReadTransition(OpenBracketRead,OpenBracket,IsOpenBracket)
	DefineReadTransition(CloseBracketRead,CloseBracket,IsCloseBracket)
	DefineReadTransition(OpenBraceRead,OpenBrace,IsOpenBrace)
	DefineReadTransition(CloseBraceRead,CloseBrace,IsCloseBrace)
	DefineReadTransition(periodRead,period,Isperiod)
	DefineReadTransition(plusRead,plus,Isplus)
	DefineReadTransition(minusRead,minus,Isminus)
	DefineReadTransition(colonRead,colon,Iscolon)
	DefineReadTransition(commaRead,comma,Iscomma)
	DefineReadTransition(trueRead,true,Istrue)
	DefineReadTransition(falseRead,false,Isfalse)
	DefineReadTransition(nullRead,null,Isnull)
	DefineApplyTransition(ReduceG1,G,DetectG1,1) /* [GOAL Reduction] */
	DefineApplyTransition(ReduceJSONDocument2,JSONDocument,DetectJSONDocument2,1)
	DefineApplyTransition(ReduceJSONObject3,JSONObject,DetectJSONObject3,3)
	DefineApplyTransition(ReduceJSONObject4,JSONObject,DetectJSONObject4,2)
	DefineApplyTransition(ReduceJSONMemberList5,JSONMemberList,DetectJSONMemberList5,3)
	DefineApplyTransition(ReduceJSONMemberList6,JSONMemberList,DetectJSONMemberList6,1)
	DefineApplyTransition(ReduceJSONMember7,JSONMember,DetectJSONMember7,3)
	DefineApplyTransition(ReduceJSONName8,JSONName,DetectJSONName8,1)
	DefineApplyTransition(ReduceJSONValue9,JSONValue,DetectJSONValue9,1)
	DefineApplyTransition(ReduceJSONValue10,JSONValue,DetectJSONValue10,1)
	DefineApplyTransition(ReduceJSONValue11,JSONValue,DetectJSONValue11,2)
	DefineApplyTransition(ReduceJSONValue12,JSONValue,DetectJSONValue12,1)
	DefineApplyTransition(ReduceJSONValue13,JSONValue,DetectJSONValue13,1)
	DefineApplyTransition(ReduceJSONValue14,JSONValue,DetectJSONValue14,1)
	DefineApplyTransition(ReduceJSONValue15,JSONValue,DetectJSONValue15,1)
	DefineApplyTransition(ReduceJSONValue16,JSONValue,DetectJSONValue16,1)
	DefineApplyTransition(ReduceJSONArray17,JSONArray,DetectJSONArray17,3)
	DefineApplyTransition(ReduceJSONArray18,JSONArray,DetectJSONArray18,2)
	DefineApplyTransition(ReduceJSONArrayList19,JSONArrayList,DetectJSONArrayList19,3)
	DefineApplyTransition(ReduceJSONArrayList20,JSONArrayList,DetectJSONArrayList20,1)
	DefineApplyTransition(Reducesign21,sign,Detectsign21,1)
	DefineApplyTransition(Reducesign22,sign,Detectsign22,1)
DefineTransitionsEND

// ********************************
// **** Transition State Table ****
// ********************************

// State 0:
//
//	G -> .JSONDocument 
//	JSONDocument -> .JSONObject 
//	JSONObject -> .OpenBrace JSONMemberList CloseBrace 
//	JSONObject -> .OpenBrace CloseBrace 
//
//	(0 -> 1) Change JSONDocument
//	(0 -> 2) Change JSONObject
//	(0 -> 3) Read OpenBrace
//
// State 1:
//
//	G -> JSONDocument .
//
//	Reduce G1
//
// State 2:
//
//	JSONDocument -> JSONObject .
//
//	Reduce JSONDocument2
//
// State 3:
//
//	JSONObject -> OpenBrace .JSONMemberList CloseBrace 
//	JSONObject -> OpenBrace .CloseBrace 
//	JSONMemberList -> .JSONMemberList comma JSONMember 
//	JSONMemberList -> .JSONMember 
//	JSONMember -> .JSONName colon JSONValue 
//	JSONName -> .String 
//
//	(3 -> 4) Change JSONMemberList
//	(3 -> 5) Change JSONMember
//	(3 -> 6) Change JSONName
//	(3 -> 7) Read String
//	(3 -> 8) Read CloseBrace
//
// State 4:
//
//	JSONObject -> OpenBrace JSONMemberList .CloseBrace 
//	JSONMemberList -> JSONMemberList .comma JSONMember 
//
//	(4 -> 9) Read CloseBrace
//	(4 -> 10) Read comma
//
// State 5:
//
//	JSONMemberList -> JSONMember .
//
//	Reduce JSONMemberList6
//
// State 6:
//
//	JSONMember -> JSONName .colon JSONValue 
//
//	(6 -> 11) Read colon
//
// State 7:
//
//	JSONName -> String .
//
//	Reduce JSONName8
//
// State 8:
//
//	JSONObject -> OpenBrace CloseBrace .
//
//	Reduce JSONObject4
//
// State 9:
//
//	JSONObject -> OpenBrace JSONMemberList CloseBrace .
//
//	Reduce JSONObject3
//
// State 10:
//
//	JSONMemberList -> JSONMemberList comma .JSONMember 
//	JSONMember -> .JSONName colon JSONValue 
//	JSONName -> .String 
//
//	(10 -> 12) Change JSONMember
//	(10 -> 6) Change JSONName
//	(10 -> 7) Read String
//
// State 11:
//
//	JSONMember -> JSONName colon .JSONValue 
//	JSONValue -> .String 
//	JSONValue -> .Number 
//	JSONValue -> .sign Number 
//	JSONValue -> .JSONObject 
//	JSONValue -> .JSONArray 
//	JSONValue -> .true 
//	JSONValue -> .false 
//	JSONValue -> .null 
//	sign -> .plus 
//	sign -> .minus 
//	JSONObject -> .OpenBrace JSONMemberList CloseBrace 
//	JSONObject -> .OpenBrace CloseBrace 
//	JSONArray -> .OpenBracket JSONArrayList CloseBracket 
//	JSONArray -> .OpenBracket CloseBracket 
//
//	(11 -> 13) Change JSONObject
//	(11 -> 14) Change JSONValue
//	(11 -> 15) Change JSONArray
//	(11 -> 16) Change sign
//	(11 -> 17) Read String
//	(11 -> 18) Read Number
//	(11 -> 19) Read OpenBracket
//	(11 -> 3) Read OpenBrace
//	(11 -> 20) Read plus
//	(11 -> 21) Read minus
//	(11 -> 22) Read true
//	(11 -> 23) Read false
//	(11 -> 24) Read null
//
// State 12:
//
//	JSONMemberList -> JSONMemberList comma JSONMember .
//
//	Reduce JSONMemberList5
//
// State 13:
//
//	JSONValue -> JSONObject .
//
//	Reduce JSONValue12
//
// State 14:
//
//	JSONMember -> JSONName colon JSONValue .
//
//	Reduce JSONMember7
//
// State 15:
//
//	JSONValue -> JSONArray .
//
//	Reduce JSONValue13
//
// State 16:
//
//	JSONValue -> sign .Number 
//
//	(16 -> 25) Read Number
//
// State 17:
//
//	JSONValue -> String .
//
//	Reduce JSONValue9
//
// State 18:
//
//	JSONValue -> Number .
//
//	Reduce JSONValue10
//
// State 19:
//
//	JSONArray -> OpenBracket .JSONArrayList CloseBracket 
//	JSONArray -> OpenBracket .CloseBracket 
//	JSONArrayList -> .JSONArrayList comma JSONValue 
//	JSONArrayList -> .JSONValue 
//	JSONValue -> .String 
//	JSONValue -> .Number 
//	JSONValue -> .sign Number 
//	JSONValue -> .JSONObject 
//	JSONValue -> .JSONArray 
//	JSONValue -> .true 
//	JSONValue -> .false 
//	JSONValue -> .null 
//	sign -> .plus 
//	sign -> .minus 
//	JSONObject -> .OpenBrace JSONMemberList CloseBrace 
//	JSONObject -> .OpenBrace CloseBrace 
//	JSONArray -> .OpenBracket JSONArrayList CloseBracket 
//	JSONArray -> .OpenBracket CloseBracket 
//
//	(19 -> 13) Change JSONObject
//	(19 -> 26) Change JSONValue
//	(19 -> 15) Change JSONArray
//	(19 -> 27) Change JSONArrayList
//	(19 -> 16) Change sign
//	(19 -> 17) Read String
//	(19 -> 18) Read Number
//	(19 -> 19) Read OpenBracket
//	(19 -> 28) Read CloseBracket
//	(19 -> 3) Read OpenBrace
//	(19 -> 20) Read plus
//	(19 -> 21) Read minus
//	(19 -> 22) Read true
//	(19 -> 23) Read false
//	(19 -> 24) Read null
//
// State 20:
//
//	sign -> plus .
//
//	Reduce sign21
//
// State 21:
//
//	sign -> minus .
//
//	Reduce sign22
//
// State 22:
//
//	JSONValue -> true .
//
//	Reduce JSONValue14
//
// State 23:
//
//	JSONValue -> false .
//
//	Reduce JSONValue15
//
// State 24:
//
//	JSONValue -> null .
//
//	Reduce JSONValue16
//
// State 25:
//
//	JSONValue -> sign Number .
//
//	Reduce JSONValue11
//
// State 26:
//
//	JSONArrayList -> JSONValue .
//
//	Reduce JSONArrayList20
//
// State 27:
//
//	JSONArray -> OpenBracket JSONArrayList .CloseBracket 
//	JSONArrayList -> JSONArrayList .comma JSONValue 
//
//	(27 -> 29) Read CloseBracket
//	(27 -> 30) Read comma
//
// State 28:
//
//	JSONArray -> OpenBracket CloseBracket .
//
//	Reduce JSONArray18
//
// State 29:
//
//	JSONArray -> OpenBracket JSONArrayList CloseBracket .
//
//	Reduce JSONArray17
//
// State 30:
//
//	JSONArrayList -> JSONArrayList comma .JSONValue 
//	JSONValue -> .String 
//	JSONValue -> .Number 
//	JSONValue -> .sign Number 
//	JSONValue -> .JSONObject 
//	JSONValue -> .JSONArray 
//	JSONValue -> .true 
//	JSONValue -> .false 
//	JSONValue -> .null 
//	sign -> .plus 
//	sign -> .minus 
//	JSONObject -> .OpenBrace JSONMemberList CloseBrace 
//	JSONObject -> .OpenBrace CloseBrace 
//	JSONArray -> .OpenBracket JSONArrayList CloseBracket 
//	JSONArray -> .OpenBracket CloseBracket 
//
//	(30 -> 13) Change JSONObject
//	(30 -> 31) Change JSONValue
//	(30 -> 15) Change JSONArray
//	(30 -> 16) Change sign
//	(30 -> 17) Read String
//	(30 -> 18) Read Number
//	(30 -> 19) Read OpenBracket
//	(30 -> 3) Read OpenBrace
//	(30 -> 20) Read plus
//	(30 -> 21) Read minus
//	(30 -> 22) Read true
//	(30 -> 23) Read false
//	(30 -> 24) Read null
//
// State 31:
//
//	JSONArrayList -> JSONArrayList comma JSONValue .
//
//	Reduce JSONArrayList19
//

static void SetupTransitions0(StateMachine &Machine)
	{
	ConstructTransition(Machine,ChangeJSONDocument,0,1);
	ConstructTransition(Machine,ChangeJSONObject,0,2);
	ConstructTransition(Machine,OpenBraceRead,0,3);
	ConstructTransition(Machine,ReduceG1,1,0);
	ConstructTransition(Machine,ReduceJSONDocument2,2,0);
	ConstructTransition(Machine,ChangeJSONMemberList,3,4);
	ConstructTransition(Machine,ChangeJSONMember,3,5);
	ConstructTransition(Machine,ChangeJSONName,3,6);
	ConstructTransition(Machine,StringRead,3,7);
	ConstructTransition(Machine,CloseBraceRead,3,8);
	ConstructTransition(Machine,CloseBraceRead,4,9);
	ConstructTransition(Machine,commaRead,4,10);
	ConstructTransition(Machine,ReduceJSONMemberList6,5,0);
	ConstructTransition(Machine,colonRead,6,11);
	ConstructTransition(Machine,ReduceJSONName8,7,0);
	ConstructTransition(Machine,ReduceJSONObject4,8,0);
	ConstructTransition(Machine,ReduceJSONObject3,9,0);
	ConstructTransition(Machine,ChangeJSONMember,10,12);
	ConstructTransition(Machine,ChangeJSONName,10,6);
	ConstructTransition(Machine,StringRead,10,7);
	ConstructTransition(Machine,ChangeJSONObject,11,13);
	ConstructTransition(Machine,ChangeJSONValue,11,14);
	ConstructTransition(Machine,ChangeJSONArray,11,15);
	ConstructTransition(Machine,Changesign,11,16);
	ConstructTransition(Machine,StringRead,11,17);
	ConstructTransition(Machine,NumberRead,11,18);
	ConstructTransition(Machine,OpenBracketRead,11,19);
	ConstructTransition(Machine,OpenBraceRead,11,3);
	ConstructTransition(Machine,plusRead,11,20);
	ConstructTransition(Machine,minusRead,11,21);
	ConstructTransition(Machine,trueRead,11,22);
	ConstructTransition(Machine,falseRead,11,23);
	ConstructTransition(Machine,nullRead,11,24);
	ConstructTransition(Machine,ReduceJSONMemberList5,12,0);
	ConstructTransition(Machine,ReduceJSONValue12,13,0);
	ConstructTransition(Machine,ReduceJSONMember7,14,0);
	ConstructTransition(Machine,ReduceJSONValue13,15,0);
	ConstructTransition(Machine,NumberRead,16,25);
	ConstructTransition(Machine,ReduceJSONValue9,17,0);
	ConstructTransition(Machine,ReduceJSONValue10,18,0);
	ConstructTransition(Machine,ChangeJSONObject,19,13);
	ConstructTransition(Machine,ChangeJSONValue,19,26);
	ConstructTransition(Machine,ChangeJSONArray,19,15);
	ConstructTransition(Machine,ChangeJSONArrayList,19,27);
	ConstructTransition(Machine,Changesign,19,16);
	ConstructTransition(Machine,StringRead,19,17);
	ConstructTransition(Machine,NumberRead,19,18);
	ConstructTransition(Machine,OpenBracketRead,19,19);
	ConstructTransition(Machine,CloseBracketRead,19,28);
	ConstructTransition(Machine,OpenBraceRead,19,3);
	ConstructTransition(Machine,plusRead,19,20);
	ConstructTransition(Machine,minusRead,19,21);
	ConstructTransition(Machine,trueRead,19,22);
	ConstructTransition(Machine,falseRead,19,23);
	ConstructTransition(Machine,nullRead,19,24);
	ConstructTransition(Machine,Reducesign21,20,0);
	ConstructTransition(Machine,Reducesign22,21,0);
	ConstructTransition(Machine,ReduceJSONValue14,22,0);
	ConstructTransition(Machine,ReduceJSONValue15,23,0);
	ConstructTransition(Machine,ReduceJSONValue16,24,0);
	ConstructTransition(Machine,ReduceJSONValue11,25,0);
	ConstructTransition(Machine,ReduceJSONArrayList20,26,0);
	ConstructTransition(Machine,CloseBracketRead,27,29);
	ConstructTransition(Machine,commaRead,27,30);
	ConstructTransition(Machine,ReduceJSONArray18,28,0);
	ConstructTransition(Machine,ReduceJSONArray17,29,0);
	ConstructTransition(Machine,ChangeJSONObject,30,13);
	ConstructTransition(Machine,ChangeJSONValue,30,31);
	ConstructTransition(Machine,ChangeJSONArray,30,15);
	ConstructTransition(Machine,Changesign,30,16);
	ConstructTransition(Machine,StringRead,30,17);
	ConstructTransition(Machine,NumberRead,30,18);
	ConstructTransition(Machine,OpenBracketRead,30,19);
	ConstructTransition(Machine,OpenBraceRead,30,3);
	ConstructTransition(Machine,plusRead,30,20);
	ConstructTransition(Machine,minusRead,30,21);
	ConstructTransition(Machine,trueRead,30,22);
	ConstructTransition(Machine,falseRead,30,23);
	ConstructTransition(Machine,nullRead,30,24);
	ConstructTransition(Machine,ReduceJSONArrayList19,31,0);
	}

void SetupJSONTransitions(StateMachine &Machine)
	{
	Machine.Reset(32);
	SetupTransitions0(Machine);
	Machine.SetGoalReductionState(1);
	}

