// JSON Parsed Objects: JSONObject.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONObject.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2016 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __JSON_Parsed_Objects

        #define __JSON_Parsed_Objects

#include <math.h>

#include <Errors.h>
#include <JSONParser.h>

#ifdef _WIN32
        typedef __int64 longlong_t;
        #define atoll _atoi64
#else
        typedef long long longlong_t;
#endif

// ***************************************************************************
// **** JSON Object Structures ***********************************************
// ***************************************************************************

class JSON_Object_Error : public ErrorBase 
        {
        public:
                JSON_Object_Error() : ErrorBase() {}
                JSON_Object_Error(const char *error) : ErrorBase(error) {}
        };

struct JSON_Object
        {
        unsigned maxMembers;
        unsigned nMembers;
        struct JSON_Member* *Members;
        // ----------------        
        JSON_Object() {Members = NULL; maxMembers = nMembers = 0;}
        JSON_Object(JSON_Object &Source);
        ~JSON_Object() {Clear();}
        bool AddMember(JSON_Member **MemberToAdd);
        void Clear();
        JSON_Member& operator [] (unsigned i);
        JSON_Object& operator =(JSON_Object &Source);
        // ----------=====--------------
        // ---- Find Member Helpers ----
        // ----------=====--------------
        struct JSON_Member* FindMember(const char *Key);
        struct JSON_Value* FindMemberValue(const char *Key);
        // ------------------------
        struct JSON_Value_Array* Find_member_array(const char *Key);
        struct JSON_Object* Find_member_object(const char *Key);
        const char* Find_member_string(const char *Key);
        struct JSON_Value_Number* Find_member_number(const char *Key);
        struct JSON_Value_Boolean* Find_member_bool(const char *Key);
        // ----------------------------
        // ---- Add Member Helpers ----
        // ----------------------------
        struct JSON_Value_Array* Add_member_array(const char *Key);
        struct JSON_Object* Add_member_object(const char *Key);
        bool Add_member_value(const char *Key, struct JSON_Value &Value);
        bool Add_member_string(const char *Key, const char *String);
        bool Add_member_string(const char *Key, const wchar_t *String);
        bool Add_member_int(const char *Key, int Number);
        bool Add_member_long(const char *Key, longlong_t Number);
        bool Add_member_double(const char *Key, double Number);
        bool Add_member_bool(const char *Key, bool Boolean);
        bool Add_member_null(const char *Key);
        };

struct JSON_Member
        {
        char *Key;
        struct JSON_Value *Value;
        // ----------------
        JSON_Member() {Key = NULL; Value = NULL;}
        JSON_Member(JSON_Member &Source);
        ~JSON_Member() {Clear();}
        void Clear();
        bool SetKey(const char *Source, size_t Length);
        JSON_Member& operator =(JSON_Member &Source);
        };

struct JSON_Value 
        {
        virtual ~JSON_Value() {}
        virtual bool isString() {return false;}
        virtual bool isNumber() {return false;}
        virtual bool isBoolean() {return false;}
        virtual bool isObject() {return false;}
        virtual bool isArray() {return false;}
        };

struct JSON_Value_Array : public JSON_Value
        {
        unsigned maxValues;
        unsigned nValues;
        JSON_Value* *Values;
        // ----------------
        JSON_Value_Array() {Values = NULL; maxValues = nValues = 0;}
        JSON_Value_Array(JSON_Value_Array &Source);
        ~JSON_Value_Array() {Clear();}
        bool AddValue(JSON_Value **ValueToAdd);
        void Clear();
        virtual bool isArray() {return true;}
        JSON_Value* operator [] (unsigned i);
        JSON_Value_Array& operator =(JSON_Value_Array &Source);
        // ---------------------------
        // ---- Add Value Helpers ----
        // ---------------------------
        struct JSON_Value_Array* Add_value_array();
        struct JSON_Object* Add_value_object();
        bool Add_value_string(const char *String);
        bool Add_value_string(const wchar_t *String);
        bool Add_value_int(int Number);
        bool Add_value_long(longlong_t Number);
        bool Add_value_double(double Number);
        bool Add_value_bool(bool Boolean);
        bool Add_value_null();
        };

struct JSON_Value_String : public JSON_Value
        {
	char *string;
        // ----------------
        JSON_Value_String() {string = NULL;}
        JSON_Value_String(JSON_Value_String &Source);
        ~JSON_Value_String() {if (string) free(string);}
        bool SetString(const char *Source, size_t Length);
        virtual bool isString() {return true;}
        JSON_Value_String& operator =(JSON_Value_String &Source);
        };

struct JSON_Value_Number : public JSON_Value
        {
        longlong_t long_number;                 // All values are filled.
        double double_number;                   
        int int_number;
        int dec2_number;                        // Value*100 (eg. "cents").
        // ----------------
        virtual bool isNumber() {return true;}
        bool isLong() {return !isDouble();}
        bool isInt() {return long_number == int_number && !isDouble();}
        bool isDouble() {return double_number != floor(double_number);}
        bool isDEC2() {return !isInt() &&
                              (double)dec2_number/100 == double_number;}
        };

struct JSON_Value_Boolean : public JSON_Value
        {
        bool boolean;
        // ----------
        virtual bool isBoolean() {return true;}
        };

struct JSON_Value_Object : public JSON_Value
        {
        JSON_Object object;
        // ----------------
        virtual bool isObject() {return true;}
        };

// ***************************************************************************
// **** JSON Object Functions ************************************************
// ***************************************************************************

JSON_Object* BuildJSONObject(Node* JSONExpression);

int PrintJSONObject(JSON_Object &Object,                // Returns number of
                    PrintJSONDocumentFn_t PrintFn,      // bytes output.
                    void *PrintUserPtr,
                    bool asJSON=false,
                    bool Pretty=true);

int PrintJSONValue(JSON_Value &Value,                   // Returns number of
                   PrintJSONDocumentFn_t PrintFn,       // bytes output.
                   void *PrintUserPtr,
                   bool asJSON,
                   bool Pretty);

// -------------------
// ---- To stdout ----
// -------------------

int PrintJSONObject(JSON_Object &Object,                // To stdout.
                    bool asJSON=false,
                    bool Pretty=true);

// ***************************************************************************
// **** EscapeJSONString / UnescapeJSONString ********************************
// ***************************************************************************

char* EscapeJSONString(const char *Source, size_t SourceLen);
char* EscapeJSONString(const char *Source);
char* EscapeJSONString(const wchar_t *Source, size_t SourceLen);
char* EscapeJSONString(const wchar_t *Source);

void FreeEscapeJSONString(char **Buffer);

char* UnescapeJSONString(const char *JSONText,
                         size_t Length,
                         bool FilterU=false,
                         bool FilterSurrogate=false,
                         bool FilterCR=true);

char* UnescapeJSONString(const char *Source);

#define FreeUnescapeJSONString FreeEscapeJSONString

/* **************************************************************************
 ****************************************************************************
 Notes:

   I. Extracting Members from a JSON_Object
 
       1. Strings: returns NULL if key missing or not a string.
                        
            const char *Host = obj.Find_member_string("host");
            if (!Host) 
                { 
                // Error: missing required param 
                }
      
       2. Numbers: returns NULL if key missing or not a number.

            JSON_Value_Number *val = obj.Find_member_number("port");
            
            a. For optional parameters with defaults:
            
                 int Port = val ? val->int_number : -1; 

            b. For required parameters:

                 if (!val)
                     { 
                     // Error: missing required param 
                     }
                 int Port = val->int_number;
      
       3. Booleans: returns NULL if key missing or not a boolean.

            JSON_Value_Boolean *flag = obj.Find_member_bool("verbose");
            bool Verbose = flag ? flag->boolean : false; // optional w/default
      
       4. Nested objects: returns NULL if key missing or not an object.
         
            JSON_Object *db = obj.Find_member_object("database");
            if (db) const char *dbHost = db->Find_member_string("host");
      
       5. Arrays: returns NULL if key missing or not an array.
         
            JSON_Value_Array *items = obj.Find_member_array("items");
            if (items) 
                {
                for (unsigned i = 0; i < items->nValues; i++) 
                    {
                    JSON_Value *elem = (*items)[i];
                    // use elem->isString(), isNumber(), etc. to dispatch
                    }
                }

  II. Building a JSON_Object

       1. Simple members:

            JSON_Object Response;
            Response.Add_member_string("name", "example");
            Response.Add_member_int("count", 42);
            Response.Add_member_double("ratio", 3.14);
            Response.Add_member_bool("enabled", true);
            Response.Add_member_null("optional");
     
       2. Nested object:

            JSON_Object *child = Response.Add_member_object("details");
            child->Add_member_string("status", "ok");
     
       3. Array:

            JSON_Value_Array *arr = Response.Add_member_array("ports");
            arr->Add_value_int(8080);
            arr->Add_value_int(8443);
            arr->Add_value_string("named-port");

 III. JSON_Value_Number Fields:

        JSON_Value_Number has four pre-computed fields:

          long_number    - 64-bit integer (use for large IDs, timestamps)
          int_number     - 32-bit integer truncation (use for ports, counts)
          double_number  - full precision floating point
          dec2_number    - value*100 (eg. dollars->cents)
     
        bool isInt()    - true if fits in 32 bits and no fractional part
        bool isLong()   - true if no fractional part (superset of isInt)
        bool isDouble() - true if value has a fractional part
        bool isDEC2()   - true if exactly representable as cents

  IV. String Escaping

        Add_member_string() auto-escapes on insertion.
        Find_member_string() returns the escaped (stored) form.

        Use UnescapeJSONString() when you need the raw value.
        Use EscapeJSONString() when building strings for manual JSON output.

******************************************************************************
**** End Usage Patterns ******************************************************
*****************************************************************************/

#endif /* __JSON_Parsed_Objects */
