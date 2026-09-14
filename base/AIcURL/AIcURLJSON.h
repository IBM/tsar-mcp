// AIcURLJSON.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: AIcURLJSON.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2024 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//


#ifndef __AI_cURL_JSON_H

        #define __AI_cURL_JSON_H

#include <JSONObject.h>

// ***************************************
// **** JSON Parser Init/Deinit **********
// ***************************************

void InitJSONParser();
void DeinitJSONParser();

// *******************************************************************
// **** ParseJSON_mt / DeleteJSON_mt *********************************
// *******************************************************************
//
//      JSON parsing must be accomplished using the multithreaded
//      safe functions. ParseJSON_mt returns a Node* expression 
//      tree; call DeleteJSON_mt when done.
//
//      BuildJSONObject(const char*) is the high-level convenience 
//      wrapper: parse -> build JSON_Object -> delete expression.
//
// *******************************************************************

Node* ParseJSON_mt(const char *JSONBuffer);
void DeleteJSON_mt(Node *JSONExpression);

// *********************************************
// **** BuildJSONObject (Thread-Safe Parse) ****
// *********************************************

JSON_Object* BuildJSONObject(const char *JSONBuffer);
                                                     
/* **************************************************************************
 ****************************************************************************

 Notes:

 ****************************************************************************
 **** Example ***************************************************************
 *************

 ****************************************************************************

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __AI_cURL_JSON_H */
