// JSON Parsed Objects: JSONObject.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONObject.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2016 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include <ebcdic.h>
#include <Errors.h>
#include <LevelTrace.h>

#include <JSONObject.h>

static JSON_Value* CloneJSONValue(JSON_Value *Source);

// ***************************************************************************
// **** Amount Conversion ****************************************************
// ************************
// Converts a string to an amount in cents,
// handling US and DE thousands and decimal separators.
// ***************************************************************************

#define isThousandsSep(c,USFormat) \
        ((USFormat && (c) == ',') || ((c) == '.' && !(USFormat)))

#define isDecimalSep(c,USFormat) \
        ((USFormat && (c) == '.') || ((c) == ',' && !(USFormat)))

int atoDEC2(const char *str, bool USformat)     // Returns amount in cents
        {
        // Skip leading whitespace
        while (isspace((unsigned char)*str)) str++;
        // Handle optional sign
        int sign = 1;
        if (*str == '-') { sign = -1; str++; }
        else if (*str == '+') { str++; }
        // Parse integer part
        int result = 0;
        while (*str && *str != ';')
                {
                int c = *str;
                if (isThousandsSep(c, USformat)) { str++; continue; } 
                if (!isdigit((unsigned char)c)) break;
                result = result * 10 + (c - '0');
                str++;
                }
        // Parse fractional part
        int frac = 0;
        if (isDecimalSep(*str, USformat))
               {
               str++; // Skip the decimal separator
               for (int idec=0; idec < 2; idec++)  // Two decimal places
                       {
                        frac *= 10;
                        if (isdigit((unsigned char)*str))
                                {
                                frac += (*str - '0');
                                str++;
                                }
                        }
               }
        return sign * (result * 100 + frac);
        }

int atoDEC2(const char *str)                    // Returns amount in cents
        {
        return atoDEC2(str,true);
        }

// ***************************************************************************
// **** EscapeJSONString (Output ASCII or UTF8) ******************************
// ***************************************************************************

#define hexchar(x) (((x) >= 0 && (x) <= 9) ? (x) + '0' : (x) - 10 + 'a')

static size_t _EscapeJSONString(char *Dest,
                                size_t DestLen,
                                const char *Source, 
                                size_t SourceLen)
        {
        size_t nWritten = 0;
        const char *sPos = Source;
        while (SourceLen)
                {
                unsigned char sChar = (unsigned char)*sPos++;
                switch (sChar)
                        {
                        case '\n' : 
                                if (DestLen >= 2)
                                        {
                                        *Dest++ = '\\';
                                        *Dest++ = 'n';
                                        DestLen -= 2;
                                        }
                                nWritten += 2;
                                break;
                        case '\t' :
                                if (DestLen >= 2)
                                        {
                                        *Dest++ = '\\';
                                        *Dest++ = 't';
                                        DestLen -= 2;
                                        }
                                nWritten += 2;
                                break;
                        case '"' :
                                if (DestLen >= 2)
                                        {
                                        *Dest++ = '\\';
                                        *Dest++ = '"';
                                        DestLen -= 2;
                                        }
                                nWritten += 2;
                                break;
                        case '\\' :
                                if (DestLen >= 2)
                                        {
                                        *Dest++ = '\\';
                                        *Dest++ = '\\';
                                        DestLen -= 2;
                                        }
                                nWritten += 2;
                                break;
                        case '\r' :
                                if (DestLen >= 2)
                                        {
                                        *Dest++ = '\\';
                                        *Dest++ = 'r';
                                        DestLen -= 2;
                                        }
                                nWritten += 2;
                                break;
                        case '\b' :
                                if (DestLen >= 2)
                                        {
                                        *Dest++ = '\\';
                                        *Dest++ = 'b';
                                        DestLen -= 2;
                                        }
                                nWritten += 2;
                                break;
                        case '\f' :
                                if (DestLen >= 2)
                                        {
                                        *Dest++ = '\\';
                                        *Dest++ = 'f';
                                        DestLen -= 2;
                                        }
                                nWritten += 2;
                                break;
                        default:
                                if (sChar && sChar < 0x20)
                                        {
                                        // Other control chars as \u00XX
                                        if (DestLen >= 6)
                                                {
                                                *Dest++ = '\\';
                                                *Dest++ = 'u';  
                                                *Dest++ = '0';
                                                *Dest++ = '0';
                                                *Dest++ = hexchar(sChar / 16);
                                                *Dest++ = hexchar(sChar % 16);
                                                DestLen -= 6;
                                                }
                                        nWritten += 6;
                                        }
                                else    {
                                        if (DestLen)
                                                {
                                                *Dest++ = sChar;
                                                DestLen -= 1;
                                                }
                                        nWritten += 1;
                                        }
                                break;
                        }
                SourceLen--;
                }
        return nWritten;
        }

char* EscapeJSONString(const char *Source, size_t SourceLen)
        {
        static const char *ProcName = "EscapeJSONString";
        char *Buffer = 0;
        size_t nWritten = _EscapeJSONString(NULL,0,Source,SourceLen);
        if (nWritten)
                {
                Buffer = (char *)malloc(nWritten+1);
                if (!Buffer)
                        {
                        TERROR(("%s: malloc(%u) failed",ProcName,nWritten));
                        }
                else    {
                        _EscapeJSONString(Buffer,
                                             nWritten,
                                             Source,
                                             SourceLen);
                        }
                Buffer[nWritten] = '\0';
                }
        return Buffer;
        }

char* EscapeJSONString(const char *Source)
        {
        return EscapeJSONString(Source,strlen(Source)+1);
        }

char* EscapeJSONString(const wchar_t *Source, size_t SourceLen)
        {
        static const char *ProcName = "EscapeJSONString";
        W2UTF8 utf8Source(Source,SourceLen);
        return EscapeJSONString(utf8Source,strlen(utf8Source));
        }

char* EscapeJSONString(const wchar_t *Source)
        {
        return EscapeJSONString(Source,wcslen(Source)+1);
        }

void FreeEscapeJSONString(char **Buffer)
        {
        if (Buffer && *Buffer)
                {
                free(*Buffer);
                *Buffer = NULL;
                }
        return;
        }

// ***************************************************************************
// **** UnescapeJSONString (Input and Output are UTF8) ***********************
// ***************************************************************************

#define hexval(x) (((x) >= '0' && (x) <= '9')                               \
                   ? (x) - '0'                                              \
                   : ((x) >= 'A' && (x) <= 'F')                             \
                     ? (x) - 'A' + 10                                       \
                     : (x) - 'a' + 10)

char* UnescapeJSONString(const char *JSONText,
                         size_t Length,
                         bool FilterU,
                         bool FilterSurrogate,
                         bool FilterCR)
        {
        static const char *ProcName = "UnescapeJSONString";
        char *Buffer = (char *)malloc(Length+1);
        if (!Buffer)
                {
                TERROR(("%s: malloc(%u) failed",ProcName,Length+1));
                return NULL;
                }
        UTF82W TextU(JSONText,Length);          // Compute in wchar_t.
        const wchar_t *Src = TextU;
        size_t SrcLength = wcslen(TextU);       // Source wchar_t count.
        char *Dest = Buffer;
        size_t DestLength = Length;             // Won't be more than source.
        while (SrcLength && DestLength)
                {
                if (SrcLength >= 2 && 
                    Src[0] == L'\\' && Src[1] == L'n')  // Add Newline.
                        {
                        *Dest++ = '\n';
                        DestLength -= 1;
                        Src += 2;
                        SrcLength -= 2;
                        }
                else if (SrcLength >= 2 &&              // Handle CR.
                         Src[0] == L'\\' && Src[1] == L'r')
                        {
                        if (!FilterCR)
                                {
                                *Dest++ = '\r';
                                DestLength -= 1;
                                }
                        Src += 2;
                        SrcLength -= 2;
                        }
                else if (SrcLength >= 2 &&      // Handle tab.
                         Src[0] == L'\\' && Src[1] == L't')
                        {
                        *Dest++ = '\t';
                        DestLength -= 1;
                        Src += 2;
                        SrcLength -= 2;
                        }
                else if (SrcLength >= 2 &&      // Handle backspace.
                         Src[0] == L'\\' && Src[1] == L'b')
                        {
                        *Dest++ = '\b';
                        DestLength -= 1;
                        Src += 2;
                        SrcLength -= 2;
                        }
                else if (SrcLength >= 2 &&      // Handle form feed.
                         Src[0] == L'\\' && Src[1] == L'f')
                        {
                        *Dest++ = '\f';
                        DestLength -= 1;
                        Src += 2;
                        SrcLength -= 2;
                        }
                else if (SrcLength >= 2 &&      // Handle escaped quotes.
                         Src[0] == L'\\' && Src[1] == L'"')
                        {
                        *Dest++ = '"';
                        DestLength -= 1;
                        Src += 2;
                        SrcLength -= 2;
                        }
                else if (SrcLength >= 2 &&      // Handle escaped escape.
                         Src[0] == L'\\' && Src[1] == L'\\')
                        {
                        *Dest++ = '\\';
                        DestLength -= 1;
                        Src += 2;
                        SrcLength -= 2;
                        }
                else if (SrcLength >= 6 &&      // Skip Binding-NonSpace.
                         Src[0] == L'\\' && 
                         Src[1] == L'u' &&
                         Src[2] == L'2' &&
                         Src[3] == L'0' &&
                         Src[4] == L'0' &&
                         Src[5] == L'd')
                        {
                        Src += 6;
                        SrcLength -= 6;
                        }
                else if (SrcLength >= 6 &&      // Convert Escaped Unicode.
                         Src[0] == L'\\' && 
                         Src[1] == L'u')
                        {
                        wchar_t uc = hexval(Src[2]) * 4096 +
                                     hexval(Src[3]) * 256 +
                                     hexval(Src[4]) * 16 +
                                     hexval(Src[5]);
                        size_t n = WCHAR_to_UTF8(Dest,DestLength,&uc,1);
                        Dest += n;
                        DestLength -= n;
                        Src += 6;
                        SrcLength -= 6;
                        }
                else if (*Src < 128)                    // ASCII.
                        {
                        *Dest++ = (char)*Src++;
                        DestLength--;
                        SrcLength--;
                        }
                else if (*Src == 0x2019)                // Single quote.
                        {
                        *Dest++ = '\'';                 
                        DestLength--;
                        Src++;
                        SrcLength--;
                        }
                else    {                               // Unicode.
                        size_t SrcLen = 1;
                        bool Surrogate = false;
  #if U2CHAR_IS_WCHAR /* UTF16 */
                        if (*Src >= 0xD800 && *Src <= 0xDFFF)
                                {
                                SrcLen = 2;
                                Surrogate = true;
                                }
  #else /* UTF32 */
                        if (*Src >= 0x10000 && *Src <= 0x20FFFF)
                                {
                                Surrogate = true;
                                }
  #endif
                        if ((!Surrogate || !FilterSurrogate) && !FilterU)
                                {
                                size_t DestLen = WCHAR_to_UTF8(Dest,
                                                               DestLength,
                                                               Src,
                                                               SrcLen);
                                Dest += DestLen;
                                DestLength -= DestLen;
                                }
                        Src += SrcLen;
                        SrcLength -= SrcLen;
                        }
                }
        *Dest++ = '\0';
        return Buffer;
        }

char* UnescapeJSONString(const char *Source)
        {
        return UnescapeJSONString(Source,strlen(Source)+1);
        }

// ***************************************************************************
// **** JSON_Object **********************************************************
// ***************************************************************************

JSON_Object::JSON_Object(JSON_Object &Source)
        {
        if (this == &Source) return;
        Members = NULL; 
        maxMembers = nMembers = 0;
        *this = Source;
        return;
        }

void JSON_Object::Clear()
        {
        while (nMembers)
                {
                --nMembers;
                delete Members[nMembers];
                Members[nMembers] = NULL;
                }
        free(Members);
        Members = NULL; 
        maxMembers = 0;
        return;
        }

bool JSON_Object::AddMember(JSON_Member **MemberToAdd)
        {
        static const char *ProcName = "JSON_Object::AddMember";
        if (!MemberToAdd || !*MemberToAdd) return false;
        if (nMembers == maxMembers)
                {
                JSON_Member* *newMembers;
                unsigned newCount = (maxMembers + 8) * 2;
                size_t newSize = newCount * sizeof(JSON_Member*);
                newMembers = (JSON_Member **)realloc(Members,newSize);
                if (!newMembers) 
                        {
                        TERROR(("%s: Failed to realloc(%d)",ProcName,newCount));
                        return false;
                        }
                maxMembers = newCount;
                Members = newMembers;
                }
        Members[nMembers++] = *MemberToAdd;
        *MemberToAdd = NULL;
        return true;
        }

JSON_Member& JSON_Object::operator [] (unsigned i)
        {
        static const char *ProcName = "JSON_Object::operator[]";
        if (i >= nMembers)
                {
                TERROR(("%s: JSON_Member beyond Limit (%d >= %d)",
                        ProcName,
                        i,
                        nMembers));
                throw JSON_Object_Error("JSON_Member beyond Limit");
                }
        return *Members[i];
        }

JSON_Object& JSON_Object::operator =(JSON_Object &Source)
        {
        static const char *ProcName = "JSON_Object::operator =";
        if (this == &Source) return *this;
        Clear();
        for (unsigned i=0; i < Source.nMembers; i++)
                {
                JSON_Member *newMember = new JSON_Member(Source[i]);
                if (!newMember)
                        {
                        TERROR(("%s: Unable to alloc Member: %u",ProcName,i));
                        continue;
                        }
                AddMember(&newMember);
                if (newMember)
                        {
                        TERROR(("%s: Unable to add Member: %u",ProcName,i));
                        delete newMember;
                        }
                }
        return *this;
        }

// ---------------------------------------------------------------------------
// ---- JSON_Object Member Helpers -------------------------------------------
// ---------------------------------------------------------------------------

JSON_Member* JSON_Object::FindMember(const char *Key)
        {
        static const char *ProcName = "JSON_Object::FindMember";
        if (!Key) return NULL;
        JSON_Member *Member = NULL;
        for (unsigned i=0; i < nMembers; i++)
                {
                const char *memberKey = Members[i]->Key;
                if (!memberKey)
                        {
                        TERROR(("%s: Missing Key %i",ProcName,i));
                        return NULL;
                        }
                if (strcmp(memberKey,Key) == 0)
                        {
                        Member = Members[i];
                        break;
                        }
                }
        return Member;
        }

JSON_Value* JSON_Object::FindMemberValue(const char *Key)
        {
        if (!Key) return NULL;
        JSON_Member *Member = FindMember(Key);
        return Member ? Member->Value : NULL;
        }

JSON_Value_Array* JSON_Object::Find_member_array(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Find_member_array";
        if (!Key) return NULL;
        JSON_Value_Array *Array = NULL;
        JSON_Value *Value = FindMemberValue(Key);
        if (Value && Value->isArray())
                {
                Array = (JSON_Value_Array*)Value;
                }
        return Array;
        }

JSON_Object* JSON_Object::Find_member_object(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Find_member_object";
        if (!Key) return NULL;
        JSON_Object *Object = NULL;
        JSON_Value *Value = FindMemberValue(Key);
        if (Value && Value->isObject())
                {
                JSON_Value_Object *vObject = (JSON_Value_Object*)Value;
                Object = &vObject->object;
                }
        return Object;
        }

const char* JSON_Object::Find_member_string(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Find_member_string";
        if (!Key) return NULL;
        const char *String = NULL;
        JSON_Value *Value = FindMemberValue(Key);
        if (Value && Value->isString())
                {
                JSON_Value_String *vString = (JSON_Value_String*)Value;
                String = vString->string;
                }
        return String;
        }

JSON_Value_Number* JSON_Object::Find_member_number(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Find_member_number";
        if (!Key) return NULL;
        JSON_Value_Number *Number = NULL;
        JSON_Value *Value = FindMemberValue(Key);
        if (Value && Value->isNumber())
                {
                Number = (JSON_Value_Number*)Value;
                }
        return Number;
        }

JSON_Value_Boolean* JSON_Object::Find_member_bool(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Find_member_bool";
        if (!Key) return NULL;
        JSON_Value_Boolean *Boolean = NULL;
        JSON_Value *Value = FindMemberValue(Key);
        if (Value && Value->isBoolean())
                {
                Boolean = (JSON_Value_Boolean*)Value;
                }
        return Boolean;
        }

// ---------------------------------------------------------------------------
// ---- JSON_Object Add Member Helpers ---------------------------------------
// ---------------------------------------------------------------------------

bool JSON_Object::Add_member_value(const char *Key, struct JSON_Value &Value)
        {
        static const char *ProcName = "JSON_Object::Add_member_value";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        member->Value = CloneJSONValue(&Value);
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

// ---------------------------------------------------------------------------
// ---- JSON_Object Add Member Helpers (Claude Sonnet 4.5) -------------------
// ---------------------------------------------------------------------------

JSON_Value_Array* JSON_Object::Add_member_array(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Add_member_array";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return NULL;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return NULL;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return NULL;
                }
        JSON_Value_Array *valueArray = new JSON_Value_Array();
        if (!valueArray)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Array",ProcName));
                delete member;
                return NULL;
                }
        member->Value = valueArray;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return NULL;
                }
        return valueArray;
        }

JSON_Object* JSON_Object::Add_member_object(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Add_member_object";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return NULL;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return NULL;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return NULL;
                }
        JSON_Value_Object *valueObj = new JSON_Value_Object();
        if (!valueObj)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Object",ProcName));
                delete member;
                return NULL;
                }
        member->Value = valueObj;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return NULL;
                }
        return &valueObj->object;
        }

bool JSON_Object::Add_member_string(const char *Key, const char *String)
        {
        static const char *ProcName = "JSON_Object::Add_member_string";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        if (!String)
                {
                TERROR(("%s: Missing String",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        JSON_Value_String *valueStr = new JSON_Value_String();
        if (!valueStr)
                {
                TERROR(("%s: Unable to alloc JSON_Value_String",ProcName));
                delete member;
                return false;
                }
        char *escapedString = EscapeJSONString(String);
        if (!escapedString)
                {
                TERROR(("%s: Unable to escape String",ProcName));
                delete valueStr;
                delete member;
                return false;
                }
        // Transfer ownership of escaped string to valueStr
        valueStr->string = escapedString;
        member->Value = valueStr;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

bool JSON_Object::Add_member_string(const char *Key, const wchar_t *String)
        {
        static const char *ProcName = "JSON_Object::Add_member_string";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        if (!String)
                {
                TERROR(("%s: Missing String",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        JSON_Value_String *valueStr = new JSON_Value_String();
        if (!valueStr)
                {
                TERROR(("%s: Unable to alloc JSON_Value_String",ProcName));
                delete member;
                return false;
                }
        char *escapedString = EscapeJSONString(String);
        if (!escapedString)
                {
                TERROR(("%s: Unable to escape String",ProcName));
                delete valueStr;
                delete member;
                return false;
                }
        // Transfer ownership of escaped string to valueStr
        valueStr->string = escapedString;
        member->Value = valueStr;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

bool JSON_Object::Add_member_int(const char *Key, int Number)
        {
        static const char *ProcName = "JSON_Object::Add_member_int";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        JSON_Value_Number *valueNum = new JSON_Value_Number();
        if (!valueNum)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Number",ProcName));
                delete member;
                return false;
                }
        valueNum->long_number = Number;
        valueNum->int_number = Number;
        valueNum->double_number = Number;
        valueNum->dec2_number = Number * 100;
        member->Value = valueNum;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

bool JSON_Object::Add_member_long(const char *Key, longlong_t Number)
        {
        static const char *ProcName = "JSON_Object::Add_member_long";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        JSON_Value_Number *valueNum = new JSON_Value_Number();
        if (!valueNum)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Number",ProcName));
                delete member;
                return false;
                }
        valueNum->long_number = Number;
        valueNum->int_number = (int)Number;
        valueNum->double_number = (double)Number;
        valueNum->dec2_number = (int)(Number * 100);
        member->Value = valueNum;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

bool JSON_Object::Add_member_double(const char *Key, double Number)
        {
        static const char *ProcName = "JSON_Object::Add_member_double";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        JSON_Value_Number *valueNum = new JSON_Value_Number();
        if (!valueNum)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Number",ProcName));
                delete member;
                return false;
                }
        valueNum->long_number = (longlong_t)Number;
        valueNum->double_number = Number;
        valueNum->int_number = (int)Number;
        valueNum->dec2_number = (int)(Number * 100);
        member->Value = valueNum;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

bool JSON_Object::Add_member_bool(const char *Key, bool Boolean)
        {
        static const char *ProcName = "JSON_Object::Add_member_bool";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        JSON_Value_Boolean *valueBool = new JSON_Value_Boolean();
        if (!valueBool)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Boolean",ProcName));
                delete member;
                return false;
                }
        valueBool->boolean = Boolean;
        member->Value = valueBool;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

bool JSON_Object::Add_member_null(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Add_member_null";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        JSON_Member *member = new JSON_Member();
        if (!member)
                {
                TERROR(("%s: Unable to alloc JSON_Member",ProcName));
                return false;
                }
        if (!member->SetKey(Key, strlen(Key)))
                {
                TERROR(("%s: Unable to set Key",ProcName));
                delete member;
                return false;
                }
        member->Value = NULL;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
        return true;
        }

// *********************
// **** JSON_Member ****
// *********************

JSON_Member::JSON_Member(JSON_Member &Source)
        {
        if (this == &Source) return;
        Key = NULL; 
        Value = NULL;
        *this = Source;
        return;
        }

void JSON_Member::Clear() 
        {
        if (Key) free(Key);
        if (Value) delete Value;
        Key = NULL; 
        Value = NULL;
        return;
        }

bool JSON_Member::SetKey(const char *Source, size_t Length)
        {
        static const char *ProcName = "JSON_Member::SetKey";
        if (Key) free(Key);
        Key = (char *)malloc(Length+1);
        if (!Key) 
                {
                TERROR(("%s: Unable to alloc(%d)",ProcName,Length+1));
                return false;
                }
        memcpy(Key,Source,Length);
        Key[Length] = '\0';
        return true;
        }

JSON_Member& JSON_Member::operator =(JSON_Member &Source)
        {
        static const char *ProcName = "JSON_Member::operator =";
        if (this == &Source) return *this;
        Clear();
        if (Source.Key) Key = strdup(Source.Key);
        if (Source.Value) Value = CloneJSONValue(Source.Value);
        return *this;
        }

// **************************
// **** JSON_Value_Array ****
// **************************

JSON_Value_Array::JSON_Value_Array(JSON_Value_Array &Source)
        {
        if (this == &Source) return;
        Values = NULL; 
        maxValues = nValues = 0;
        *this = Source;
        return;
        }

void JSON_Value_Array::Clear()
        {
        while (nValues)
                {
                --nValues;
                if (Values[nValues]) 
                        {
                        delete Values[nValues];
                        Values[nValues] = NULL;
                        }
                }
        free(Values);
        Values = NULL; 
        maxValues = 0;
        return;
        }

bool JSON_Value_Array::AddValue(JSON_Value **ValueToAdd)
        {
        static const char *ProcName = "JSON_Value_Array::AddValue";
        if (nValues == maxValues)
                {
                JSON_Value* *newValues;
                unsigned newCount = (maxValues + 8) * 2;
                size_t newSize = newCount * sizeof(JSON_Value*);
                newValues = (JSON_Value **)realloc(Values,newSize);
                if (!newValues) 
                        {
                        TERROR(("%s: Failed to realloc(%d)",ProcName,newCount));
                        return false;
                        }
                maxValues = newCount;
                Values = newValues;
                }
        Values[nValues++] = *ValueToAdd;
        *ValueToAdd = NULL;
        return true;
        }

JSON_Value* JSON_Value_Array::operator [] (unsigned i)
        {
        static const char *ProcName = "JSON_Value_Array::operator[]";
        if (i >= nValues)
                {
                TERROR(("%s: JSON_Value beyond Limit (%d >= %d)",
                        ProcName,
                        i,
                        nValues));
                throw JSON_Object_Error("JSON_Value beyond Limit");
                }
        return Values[i];
        }

JSON_Value_Array& JSON_Value_Array::operator =(JSON_Value_Array &Source)
        {
        static const char *ProcName = "JSON_Value_Array::operator =";
        if (this == &Source) return *this;
        Clear();
        for (unsigned i=0; i < Source.nValues; i++)
                {
                JSON_Value *newValue = CloneJSONValue(Source.Values[i]);
                if (!newValue && Source.Values[i])
                        {
                        TERROR(("%s: Unable to clone Value: %u",ProcName,i));
                        continue;
                        }
                AddValue(&newValue);
                if (newValue)
                        {
                        TERROR(("%s: Unable to add Value: %u",ProcName,i));
                        delete newValue;
                        }
                }
        return *this;
        }

// ---------------------------------------------------------------------------
// ---- JSON_Value_Array Add Value Helpers (Claude Sonnet 4.5) ---------------
// ---------------------------------------------------------------------------

JSON_Value_Array* JSON_Value_Array::Add_value_array()
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_array";
        JSON_Value_Array *valueArray = new JSON_Value_Array();
        if (!valueArray)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Array",ProcName));
                return NULL;
                }
        JSON_Value_Array *rcArray = valueArray;
        if (!AddValue((JSON_Value**)&valueArray))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueArray;
                return NULL;
                }
        return rcArray;
        }

JSON_Object* JSON_Value_Array::Add_value_object()
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_object";
        JSON_Value_Object *valueObj = new JSON_Value_Object();
        if (!valueObj)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Object",ProcName));
                return NULL;
                }
        JSON_Object *rcObject = &valueObj->object;
        if (!AddValue((JSON_Value**)&valueObj))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueObj;
                return NULL;
                }
        return rcObject;
        }

bool JSON_Value_Array::Add_value_string(const char *String)
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_string";
        if (!String)
                {
                TERROR(("%s: Missing String",ProcName));
                return false;
                }
        JSON_Value_String *valueStr = new JSON_Value_String();
        if (!valueStr)
                {
                TERROR(("%s: Unable to alloc JSON_Value_String",ProcName));
                return false;
                }
        char *escapedString = EscapeJSONString(String);
        if (!escapedString)
                {
                TERROR(("%s: Unable to escape String",ProcName));
                delete valueStr;
                return false;
                }
        // Transfer ownership of escaped string to valueStr
        valueStr->string = escapedString;
        if (!AddValue((JSON_Value**)&valueStr))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueStr;
                return false;
                }
        return true;
        }

bool JSON_Value_Array::Add_value_string(const wchar_t *String)
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_string";
        if (!String)
                {
                TERROR(("%s: Missing String",ProcName));
                return false;
                }
        JSON_Value_String *valueStr = new JSON_Value_String();
        if (!valueStr)
                {
                TERROR(("%s: Unable to alloc JSON_Value_String",ProcName));
                return false;
                }
        char *escapedString = EscapeJSONString(String);
        if (!escapedString)
                {
                TERROR(("%s: Unable to escape String",ProcName));
                delete valueStr;
                return false;
                }
        // Transfer ownership of escaped string to valueStr
        valueStr->string = escapedString;
        if (!AddValue((JSON_Value**)&valueStr))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueStr;
                return false;
                }
        return true;
        }

bool JSON_Value_Array::Add_value_int(int Number)
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_int";
        JSON_Value_Number *valueNum = new JSON_Value_Number();
        if (!valueNum)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Number",ProcName));
                return false;
                }
        valueNum->long_number = Number;
        valueNum->int_number = Number;
        valueNum->double_number = Number;
        valueNum->dec2_number = Number * 100;
        if (!AddValue((JSON_Value**)&valueNum))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueNum;
                return false;
                }
        return true;
        }

bool JSON_Value_Array::Add_value_long(longlong_t Number)
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_long";
        JSON_Value_Number *valueNum = new JSON_Value_Number();
        if (!valueNum)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Number",ProcName));
                return false;
                }
        valueNum->long_number = Number;
        valueNum->int_number = (int)Number;
        valueNum->double_number = (double)Number;
        valueNum->dec2_number = (int)(Number * 100);
        if (!AddValue((JSON_Value**)&valueNum))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueNum;
                return false;
                }
        return true;
        }

bool JSON_Value_Array::Add_value_double(double Number)
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_double";
        JSON_Value_Number *valueNum = new JSON_Value_Number();
        if (!valueNum)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Number",ProcName));
                return false;
                }
        valueNum->long_number = (longlong_t)Number;
        valueNum->double_number = Number;
        valueNum->int_number = (int)Number;
        valueNum->dec2_number = (int)(Number * 100);
        if (!AddValue((JSON_Value**)&valueNum))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueNum;
                return false;
                }
        return true;
        }

bool JSON_Value_Array::Add_value_bool(bool Boolean)
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_bool";
        JSON_Value_Boolean *valueBool = new JSON_Value_Boolean();
        if (!valueBool)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Boolean",ProcName));
                return false;
                }
        valueBool->boolean = Boolean;
        if (!AddValue((JSON_Value**)&valueBool))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueBool;
                return false;
                }
        return true;
        }

bool JSON_Value_Array::Add_value_null()
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_null";
        JSON_Value *valueNull = NULL;
        if (!AddValue(&valueNull))
                {
                TERROR(("%s: Unable to add null value",ProcName));
                return false;
                }
        return true;
        }

// ***************************
// **** JSON_Value_String ****
// ***************************

JSON_Value_String::JSON_Value_String(JSON_Value_String &Source)
        {
        if (this == &Source) return;
        string = NULL;
        *this = Source;
        return;
        }

JSON_Value_String& JSON_Value_String::operator =(JSON_Value_String &Source)
        {
        if (this == &Source) return *this;
        if (string) free(string);
        string = NULL;
        if (Source.string)
                string = strdup(Source.string);
        return *this;
        }

bool JSON_Value_String::SetString(const char *Source, size_t Length)
        {
        static const char *ProcName = "JSON_Value_String::SetString";
        if (string) free(string);
        string = (char *)malloc(Length+1);
        if (!string) 
                {
                TERROR(("%s: Unable to alloc(%d)",ProcName,Length+1));
                return false;
                }
        memcpy(string,Source,Length);
        string[Length] = '\0';
        return true;
        }

// ---------------------------------------------------------------------------
// ---- CloneJSONValue -------------------------------------------------------
// --------------------
// Polymorphic deep-copy of any JSON_Value subtype (IBM Bob - 2004-03-10)
// ---------------------------------------------------------------------------

static JSON_Value* CloneJSONValue(JSON_Value *Source)
        {
        static const char *ProcName = "CloneJSONValue";
        if (!Source) return NULL;
        JSON_Value *rcValue = NULL;
        if (Source->isString()) 
                {
                rcValue = new JSON_Value_String(*(JSON_Value_String*)Source);
                }
        else if (Source->isNumber())
                {
                rcValue = new JSON_Value_Number(*(JSON_Value_Number*)Source);
                }
        else if (Source->isBoolean())
                {
                rcValue = new JSON_Value_Boolean(*(JSON_Value_Boolean*)Source);
                }
        else if (Source->isArray())
                {
                rcValue = new JSON_Value_Array(*(JSON_Value_Array*)Source);
                }
        else if (Source->isObject())
                {
                rcValue = new JSON_Value_Object(*(JSON_Value_Object*)Source);
                }
        else    {
                TERROR(("%s: Unknown JSON_Value subtype",ProcName));
                }
        return rcValue;
        }

// ---------------------------------------------------------------------------
// ---- BuildJSONObject(Node* JSONExpression) --------------------------------
// ---------------------------------------------------------------------------

static void BuildJSONArray(JSON_Value_Array &Array, Node *Expression);
static void BuildJSONObject(JSON_Object &Object, Node *Expression);

static JSON_Value* BuildJSONValue(Node *ValueNode)
        {
        static const char *ProcName = "BuildJSONValue";
        int SignValue = 1;
        JSON_Value *ReturnValue = NULL;
        Node *Left = ValueNode->GetChild(0);
        switch (Left->GetTypeID())
                {
                case Ew_StringID: 
                        {
                        JSON_Value_String *newString = new JSON_Value_String;
                        if (!newString)
                                {
                                TERROR(("%s: Unable to alloc newString",ProcName));
                                throw JSON_Object_Error("Unable to alloc newString");
                                }
                        TerminalNode *TNode = (TerminalNode *)Left;
                        LexicalItem *Item = (LexicalItem *)TNode->GetValue();
                        if (Item->SymbolClass != SC_Null)
                                {
                                newString->SetString(Item->String+1,Item->Length-2);
                                }
                        ReturnValue = newString;
                        break;
                        }
                case Ew_NumberID: 
                        {
  onNumber:             JSON_Value_Number *newNumber = new JSON_Value_Number;
                        if (!newNumber)
                                {
                                TERROR(("%s: Unable to alloc newNumber",ProcName));
                                throw JSON_Object_Error("Unable to alloc newNumber");
                                }
                        TerminalNode *TNode = (TerminalNode *)Left;
                        LexicalItem *Item = (LexicalItem *)TNode->GetValue();
                        if (Item->SymbolClass != SC_Null)
                                {
                                newNumber->long_number = SignValue * 
                                                         atoll(Item->String);
                                newNumber->double_number = SignValue * 
                                                           atof(Item->String);
                                newNumber->int_number = (int)newNumber->long_number;
                                newNumber->dec2_number = SignValue * 
                                                         atoDEC2(Item->String);
                                }
                        ReturnValue = newNumber;
                        break;
                        }
                case Ew_signID:
                        {
                        // --------------
                        // ---- Sign ----
                        // --------------
                        TerminalNode *TNode = (TerminalNode *)Left->GetChild(0);
                        LexicalItem *Item = (LexicalItem *)TNode->GetValue();
                        if (Item->SymbolClass != SC_Null)
                                {
                                SignValue = Item->String[0] == '-' ? -1 : 1;
                                }
                        // ----------------
                        // ---- Number ----
                        // ----------------
                        Node *Right = ValueNode->GetChild(1); //number
                        assert(Right->GetTypeID() == Ew_NumberID);
                        Left = Right;
                        goto onNumber;
                        break;
                        }
                case Ew_JSONObjectID: 
                        {
                        JSON_Value_Object *newObject = new JSON_Value_Object;
                        if (!newObject)
                                {
                                TERROR(("%s: Unable to alloc newObject",ProcName));
                                throw JSON_Object_Error("Unable to alloc newObject");
                                }
                        BuildJSONObject(newObject->object,Left);
                        ReturnValue = newObject;
                        break;
                        }
                case Ew_JSONArrayID: 
                        {
                        JSON_Value_Array *newArray = new JSON_Value_Array;
                        if (!newArray)
                                {
                                TERROR(("%s: Unable to alloc newArray",ProcName));
                                throw JSON_Object_Error("Unable to alloc newArray");
                                }
                        BuildJSONArray(*newArray,Left);
                        ReturnValue = newArray;
                        break;
                        }
                case Ew_trueID: 
                        {
                        JSON_Value_Boolean *newBoolean = new JSON_Value_Boolean;
                        if (!newBoolean)
                                {
                                TERROR(("%s: Unable to alloc newBoolean",ProcName));
                                throw JSON_Object_Error("Unable to alloc newBoolean");
                                }
                        newBoolean->boolean = true;
                        ReturnValue = newBoolean;
                        break;
                        }
                case Ew_falseID: 
                        {
                        JSON_Value_Boolean *newBoolean = new JSON_Value_Boolean;
                        if (!newBoolean)
                                {
                                TERROR(("%s: Unable to alloc newBoolean",ProcName));
                                throw JSON_Object_Error("Unable to alloc newBoolean");
                                }
                        newBoolean->boolean = false;
                        ReturnValue = newBoolean;
                        break;
                        }
                }
        return ReturnValue;
        }

static void BuildJSONArray(JSON_Value_Array &Array, Node *Expression)
        {
        static const char *ProcName = "BuildJSONArray";
        unsigned TypeID = Expression->GetTypeID();
        if (TypeID == Ew_JSONValueID)
                {
                // ******************************
                // **** Add a Value to Array ****
                // ******************************
                JSON_Value* newValue = BuildJSONValue(Expression);
                Array.AddValue(&newValue);
                if (newValue) 
                        {
                        TERROR(("%s: Unable to add Value",ProcName));
                        delete newValue;
                        }
                return;
                }
        for (unsigned iChild = 0; iChild < MAX_CHILDREN_NODES; iChild++)
                {
                Node *Child = Expression->GetChild(iChild);
                if (!Child) break;
                BuildJSONArray(Array,Child);
                }
        return;
        }

static void BuildJSONObject(JSON_Object &Object, Node *Expression)
	{
        static const char *ProcName = "BuildJSONObject";
        unsigned TypeID = Expression->GetTypeID();
        if (TypeID == Ew_JSONMemberID)
                {
                // ********************************
                // **** Add a Member to Object ****
                // ********************************
                JSON_Member *newMember = new JSON_Member;
                if (!newMember)
                        {
                        TERROR(("%s: Unable to alloc newMember",ProcName));
                        throw JSON_Object_Error("Unable to alloc newMember");
                        }
                Node *NameNode = Expression->GetChild(0);
                Node *ValueNode = Expression->GetChild(2);
                // -------------
                // ---- Key ----
                // -------------
                assert(NameNode->GetTypeID() == Ew_JSONNameID);
                TerminalNode *TNode = (TerminalNode *)NameNode->GetChild(0);
                LexicalItem *Item = (LexicalItem *)TNode->GetValue();
                if (Item->SymbolClass != SC_Null)
                        {
                        newMember->SetKey(Item->String+1,Item->Length-2);
                        }
                // ---------------
                // ---- Value ----
                // ---------------
                newMember->Value = BuildJSONValue(ValueNode);
                Object.AddMember(&newMember);
                if (newMember) 
                        {
                        TERROR(("%s: Unable to add Member",ProcName));
                        delete newMember;
                        }
                return;
                }
        for (unsigned iChild = 0; iChild < MAX_CHILDREN_NODES; iChild++)
                {
                Node *Child = Expression->GetChild(iChild);
                if (!Child) break;
                BuildJSONObject(Object,Child);
                }
	return;
	}

JSON_Object* BuildJSONObject(Node* JSONExpression)
        {
        static const char *ProcName = "BuildJSONObjects";
        JSON_Object *Object = new JSON_Object;
        if (!Object)
                {
                TERROR(("%s: Unable to alloc JSON_Object",ProcName));
                return NULL;
                }
        BuildJSONObject(*Object,JSONExpression);
        return Object;
        }

// ***************************************************************************
// **** PrintJSONObject(JSON_Object *Object) *********************************
// ***************************************************************************
                                        // ---------------------------------
struct PrintDocStateJSON                // EXACT DUPLICATE IN JSONParser.cpp
        {                               // ---------------------------------
        unsigned Indent;
        bool InText;                    // Try to keep text within tags.
        bool NewLine;
        bool FirstLine;
        bool Pretty;
        PrintJSONDocumentFn_t PrintFn;
        void *PrintUserPtr;
        int nPrintBytes;
        // *****************
        // **** Methods ****
        // *****************
        PrintDocStateJSON(PrintJSONDocumentFn_t PrintFn, 
                          void *PrintUserPtr,
                          bool Pretty);
        int printf(const char *Format, ...);
        int printIndent();
        int printNL();
        };

struct PrintObjectStateJSON : PrintDocStateJSON
        {
        bool asJSON;
        PrintObjectStateJSON(PrintJSONDocumentFn_t PrintFn, 
                             void *PrintUserPtr,
                             bool asJSON,
                             bool Pretty)
                : PrintDocStateJSON(PrintFn,
                                    PrintUserPtr,
                                    asJSON ? Pretty : true)
                {
                PrintObjectStateJSON::asJSON = asJSON;
                return;
                }
        };

static void PrintJSONObject(PrintObjectStateJSON &State,
                            unsigned Depth, 
                            JSON_Object &Object);

static void PrintJSONValue(PrintObjectStateJSON &State,
                           unsigned Depth, 
                           JSON_Value *_Value)
        {
        if (!_Value)
                {
                if (State.asJSON)
                        {
                        State.printf("null");
                        }
                else    {
                        State.Indent = Depth;
                        State.printIndent();
                        State.printf("Value=null\n");
                        }
                }
        else if (_Value->isString())
                {
                JSON_Value_String &Value = *(JSON_Value_String*)_Value;
                if (State.asJSON)
                        {
                        State.printf("\"%s\"",Value.string);
                        }
                else    {
                        State.Indent = Depth;
                        State.printIndent();
                        State.printf("Value=%s\n",Value.string);
                        }
                }
        else if (_Value->isBoolean())
                {
                JSON_Value_Boolean &Value = *(JSON_Value_Boolean*)_Value;
                if (State.asJSON)
                        {
                        State.printf("%s",Value.boolean ? "true" : "false");
                        }
                else    {
                        State.Indent = Depth;
                        State.printIndent();
                        State.printf("Value=%s\n",Value.boolean 
                                                  ? "true" 
                                                  : "false");
                        }
                }
        else if (_Value->isNumber())
                {
                JSON_Value_Number &Value = *(JSON_Value_Number*)_Value;
                if (Value.isInt())
                        {
                        if (State.asJSON)
                                {
                                State.printf("%d",Value.int_number);
                                }
                        else    {
                                State.Indent = Depth;
                                State.printIndent();
                                State.printf("Value=%d\n",Value.int_number);
                                }
                        }
                else if (Value.isLong())
                        {
                        if (State.asJSON)
                                {
                                State.printf("%lld",Value.long_number);
                                }
                        else    {
                                State.Indent = Depth;
                                State.printIndent();
                                State.printf("Value=%lld\n",Value.long_number);
                                }
                        }
                else if (Value.isDEC2())
                        {
                        if (State.asJSON)
                                {
                                State.printf("%s%d.%02d",
                                             Value.dec2_number < 0 ? "-" : "",
                                             abs(Value.dec2_number) / 100, 
                                             abs(Value.dec2_number) % 100);
                                }
                        else    {
                                State.Indent = Depth;
                                State.printIndent();
                                State.printf("Value=%s%d.%02d\n",
                                             Value.dec2_number < 0 ? "-" : "",
                                             abs(Value.dec2_number) / 100, 
                                             abs(Value.dec2_number) % 100);
                                }
                        }
                else    {
                        if (State.asJSON)
                                {
                                State.printf("%g",Value.double_number);
                                }
                        else    {
                                State.Indent = Depth;
                                State.printIndent();
                                State.printf("Value=%g\n",Value.double_number);
                                }
                        }
                }
        else if (_Value->isArray())
                {
                JSON_Value_Array &Array = *(JSON_Value_Array*)_Value;
                if (State.asJSON) State.printf("[");
                for (unsigned i=0; i < Array.nValues; i++)
                        {
                        if (State.asJSON && i) State.printf(",");
                        PrintJSONValue(State,Depth,Array[i]);
                        }
                if (State.asJSON) State.printf("]");
                }
        else if (_Value->isObject())
                {
                JSON_Value_Object &Value = *(JSON_Value_Object*)_Value;
                PrintJSONObject(State,Depth+1,Value.object);
                }
        return;
        }

static void PrintJSONMember(PrintObjectStateJSON &State,
                            unsigned Depth, 
                            JSON_Member &Member)
        {
        if (State.asJSON)
                {
                State.Indent = Depth;
                State.printIndent();
                State.printf("\"%s\":",Member.Key);
                }
        else    {
                State.Indent = Depth;
                State.printIndent();
                State.printf("Key=%s\n",Member.Key);
                }
        if (!Member.Value) 
                {
                if (State.asJSON)
                        {
                        State.printf("null");
                        }
                else    {
                        State.Indent = Depth;
                        State.printIndent();
                        State.printf("Value=<null>\n");
                        }
                }
        else    {
                PrintJSONValue(State,Depth+1,Member.Value);
                }
        return;
        }

static void PrintJSONObject(PrintObjectStateJSON &State,
                            unsigned Depth, 
                            JSON_Object &Object)
        {
        if (State.asJSON) 
                {
                if (Depth) 
                        {
                        State.printNL();
                        State.Indent = Depth;
                        State.printIndent();
                        }
                State.printf("{");
                }
        for (unsigned i=0; i < Object.nMembers; i++)
                {
                if (State.asJSON) 
                        {
                        if (i) State.printf(",");
                        State.printNL();
                        }
                PrintJSONMember(State,Depth,Object[i]);
                }
        if (State.asJSON) 
                {
                State.printNL();
                State.Indent = Depth;
                State.printIndent();
                State.printf("}");
                }
        return;
        }

int PrintJSONObject(JSON_Object &Object,                // Returns number of
                    PrintJSONDocumentFn_t PrintFn,      // bytes output.
                    void *PrintUserPtr,
                    bool asJSON,
                    bool Pretty)
        {
        PrintObjectStateJSON State(PrintFn,PrintUserPtr,asJSON,Pretty);
        PrintJSONObject(State,0,Object);
        return State.nPrintBytes;
        }

int PrintJSONValue(JSON_Value &Value,                   // Returns number of
                   PrintJSONDocumentFn_t PrintFn,       // bytes output.
                   void *PrintUserPtr,
                   bool asJSON,
                   bool Pretty)
        {
        PrintObjectStateJSON State(PrintFn,PrintUserPtr,asJSON,Pretty);
        PrintJSONValue(State,0,&Value);
        return State.nPrintBytes;
        }

// -------------------
// ---- To stdout ----
// -------------------

int PrintJSONObject(JSON_Object &Object,        // Returns bytes output.
                    bool asJSON,
                    bool Pretty)           
        {
        PrintJSONDocumentFn_t PrintFn = (PrintJSONDocumentFn_t)vfprintf;
        void *PrintUserPtr = stdout;
        return PrintJSONObject(Object,PrintFn,PrintUserPtr,asJSON,Pretty);
        }

// ***************************************************************************
// ******************************* End of File *******************************
// ***************************************************************************
