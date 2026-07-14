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

#ifndef MAX_BuildJSONObject_DEPTH
        #define MAX_BuildJSONObject_DEPTH 3000  // Call-Stack recursion guard.
#endif

#define USE_RecursiveDescent 0  // Control CloneJSONValue and PrintJSONObject.

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
// **** JSON_Value ***********************************************************
// ***************************************************************************

void JSON_Value::Clear()
        {
        JSON_Value *Current = this;
        while (Current)
                {
                if (Current->isObject())
                        {
                        if (((JSON_Value_Object *)Current)->nMembers == 0)
                                {
  Delete_Parents_Child:         if (Current == this) break;
                                JSON_Value *Parent = Current->Parent;
                                unsigned ParentIndex = Current->IndexInParent;
                                // ------------------------------------
                                // ---- Delete Current from Parent ----
                                // ------------------------------------
                                if (Parent->isObject())
                                        {
                                        JSON_Value_Object *pObject = (JSON_Value_Object *)Parent;
                                        assert(ParentIndex == pObject->nMembers-1);
                                        JSON_Member *Member = pObject->Members[ParentIndex]; 
                                        pObject->Members[ParentIndex] = NULL;
                                        pObject->nMembers--;
                                        delete Member;
                                        }
                                else if (Parent->isArray())
                                        {
                                        JSON_Value_Array *pArray = (JSON_Value_Array *)Parent;
                                        assert(ParentIndex == pArray->nValues-1);
                                        JSON_Value *Value = pArray->Values[ParentIndex]; 
                                        pArray->Values[ParentIndex] = NULL;
                                        pArray->nValues--;
                                        delete Value;
                                        }
                                Current = Parent;
                                }
                        else    {
                                JSON_Value_Object *Object = (JSON_Value_Object *)Current;
                                JSON_Member *Member = Object->Members[Object->nMembers-1];
                                if (Member->Value && (Member->Value->isObject() ||
                                                      Member->Value->isArray()))
                                        {
                                        assert(Member->Value->Parent);
                                        Current = Member->Value; // Walk right child...
                                        }
                                else    {
                                        Object->Members[Object->nMembers-1] = NULL;
                                        Object->nMembers--;
                                        delete Member;           // Delete simple members.
                                        }
                                }
                        }
                else if (Current->isArray())
                        {
                        JSON_Value_Array *Array = (JSON_Value_Array *)Current;
                        if (Array->nValues == 0) goto Delete_Parents_Child;
                        else    {
                                JSON_Value *Value = Array->Values[Array->nValues-1];
                                if (Value && (Value->isObject() || Value->isArray()))
                                        {
                                        assert(Value->Parent);
                                        Current = Value;         // Walk right child...
                                        }
                                else    {
                                        Array->Values[Array->nValues-1] = NULL;
                                        Array->nValues--;
                                        if (Value) delete Value; // Delete simple values.
                                        }
                                }
                        }
                else break; // Simple value; nothing to do.
                }
        return;
        }

// ************************************
// **** Sequential Iterate Utility ****
// ************************************

JSON_Value* SequentialGetNext(JSON_Value *Current,  // Containers are returned
                              unsigned &Level,      // twice; once before all
                              bool &After)          // children, once 'After'.
        {
        if (!Current) return NULL;
        JSON_Value *Child = NULL;
        if (After) goto Resume;
        if (Current->isObject())
                {
                JSON_Value_Object *cObject = (JSON_Value_Object *)Current;
                if (!cObject->nMembers) 
                        {
                        After = true;
                        return Current; // Second time seeing the container.
                        }
                Child = cObject->Members[0]->Value;
                }
        else if (Current->isArray())
                {
                JSON_Value_Array *cArray = (JSON_Value_Array *)Current;
                if (!cArray->nValues) 
                        {
                        After = true;
                        return Current; // Second time seeing the container.
                        }
                Child = cArray->Values[0];
                }
        if (Child)                                      // Send first child.
                {
                After = false;
                Level++;
                return Child;           // First time seeing the container.
                }
 Resume: while (Level)                                   // Send siblings.
                {
                JSON_Value *Parent = Current->GetParent();
                if (!Parent) return NULL;                       // All done.
                unsigned i = Current->GetIndexInParent();
                if (Parent->isObject())
                        {
                        JSON_Value_Object *pObject = (JSON_Value_Object *)Parent;
                        if (++i < pObject->nMembers)
                                {
                                After = false;
                                return pObject->Members[i]->Value;
                                }
                        Level--;
                        After = true;
                        return Parent;  // Second time seeing the container.
                        }
                else if (Parent->isArray())
                        {
                        JSON_Value_Array *pArray = (JSON_Value_Array *)Parent;
                        if (++i < pArray->nValues)
                                {
                                After = false;
                                return pArray->Values[i];
                                }
                        Level--;
                        After = true;
                        return Parent;  // Second time seeing the container.
                        }
                Level--;
                Current = Parent;
                }
        return NULL;
        }

JSON_Member* GetValueMember(JSON_Value *Value)
        {
        static const char *ProcName = "GetValueMember";
        if (!Value) return NULL;
        JSON_Value *Parent = Value->GetParent();
        if (!Parent) return NULL;
        if (Parent->isObject())
                {
                unsigned IndexInParent = Value->GetIndexInParent();
                JSON_Value_Object *pObject = (JSON_Value_Object *)Parent;
                if (IndexInParent >= pObject->maxMembers)
                        {
                        TERROR(("%s: Invalid IndexInParent %u >= %u",
                                ProcName,
                                IndexInParent,
                                pObject->maxMembers));
                        return NULL;
                        }
                return pObject->Members[IndexInParent];
                }
        return NULL;
        }

// ***************************************************************************
// **** JSON_Object **********************************************************
// ***************************************************************************

static JSON_Value* CloneJSONValue(JSON_Value *Source);

JSON_Object::JSON_Object(JSON_Object &Source)
        : JSON_Value(Source)
        {
        Members = NULL; 
        maxMembers = nMembers = 0;
        *this = Source;
        return;
        }

JSON_Object::~JSON_Object()
        {
        if (nMembers) Clear();          // Short-circut when childless.
        free(Members); 
        Members=NULL; 
        maxMembers=0;
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
        unsigned Index = nMembers++;
        Members[Index] = *MemberToAdd;
        *MemberToAdd = NULL;
        // -------------------------
        // ---- Set Backpointer ----
        // -------------------------
        if (Members[Index]->Value)
                {
                Members[Index]->Value->Parent = this;
                Members[Index]->Value->IndexInParent = Index;
                }
        return true;
        }

bool JSON_Object::AddValue(const char *Key, JSON_Value **ValueToAdd)
        {
        static const char *ProcName = "JSON_Object::AddValue";
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
        member->Value = *ValueToAdd;
        *ValueToAdd = NULL;
        if (!AddMember(&member))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete member;
                return false;
                }
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
                Object = (JSON_Value_Object*)Value;
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
        JSON_Value *clone = CloneJSONValue(&Value);
        if (!clone)
                {
                TERROR(("%s: Unable to clone Value",ProcName));
                return false;
                }
        if (!AddValue(Key,&clone))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete clone;
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
        JSON_Value_Array *valueArray = new JSON_Value_Array();
        if (!valueArray)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Array",ProcName));
                return NULL;
                }
        JSON_Value_Array *rcArray = valueArray;
        if (!AddValue(Key,(JSON_Value **)&valueArray))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete rcArray;
                return NULL;
                }
        return rcArray;
        }

JSON_Object* JSON_Object::Add_member_object(const char *Key)
        {
        static const char *ProcName = "JSON_Object::Add_member_object";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return NULL;
                }
        JSON_Value_Object *valueObj = new JSON_Value_Object();
        if (!valueObj)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Object",ProcName));
                return NULL;
                }
        JSON_Object *rcObject = valueObj;
        if (!AddValue(Key,(JSON_Value **)&valueObj))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete rcObject;
                return NULL;
                }
        return rcObject;
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
        valueStr->string = escapedString;       // Transfer ownership.
        if (!AddValue(Key,(JSON_Value **)&valueStr))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueStr;
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
        valueStr->string = escapedString;       // Transfer ownership.
        if (!AddValue(Key,(JSON_Value **)&valueStr))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueStr;
                return false;
                }
        return true;
        }

bool JSON_Object::Add_member_escString(const char *Key, const char *escString)
        {
        static const char *ProcName = "JSON_Object::Add_member_escString";
        if (!Key)
                {
                TERROR(("%s: Missing Key",ProcName));
                return false;
                }
        if (!escString)
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
        if (!valueStr->SetString(escString,strlen(escString)))
                {
                TERROR(("%s: Unable to set String",ProcName));
                delete valueStr;
                return false;
                }
        if (!AddValue(Key,(JSON_Value **)&valueStr))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueStr;
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
        if (!AddValue(Key,(JSON_Value **)&valueNum))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueNum;
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
        if (!AddValue(Key,(JSON_Value **)&valueNum))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueNum;
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
        if (!AddValue(Key,(JSON_Value **)&valueNum))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueNum;
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
        JSON_Value_Boolean *valueBool = new JSON_Value_Boolean();
        if (!valueBool)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Boolean",ProcName));
                return false;
                }
        valueBool->boolean = Boolean;
        if (!AddValue(Key,(JSON_Value **)&valueBool))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueBool;
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
        JSON_Value_Null *valueNull = new JSON_Value_Null();
        if (!valueNull)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Null",ProcName));
                return false;
                }
        if (!AddValue(Key,(JSON_Value **)&valueNull))
                {
                TERROR(("%s: Unable to add member",ProcName));
                delete valueNull;
                return false;
                }
        return true;
        }

// *********************
// **** JSON_Member ****
// *********************

JSON_Member::JSON_Member(JSON_Member &Source)
        {
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
        : JSON_Value(Source)
        {
        Values = NULL; 
        maxValues = nValues = 0;
        *this = Source;
        return;
        }

JSON_Value_Array::~JSON_Value_Array() 
        {
        if (nValues) Clear();           // Short-circut when childless.
        free(Values);
        Values=NULL;
        maxValues=0;
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
        unsigned Index = nValues++;
        Values[Index] = *ValueToAdd;
        *ValueToAdd = NULL;
        // -------------------------
        // ---- Set Backpointer ----
        // -------------------------
        if (Values[Index])
                {
                Values[Index]->Parent = this;
                Values[Index]->IndexInParent = Index;
                }
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
        JSON_Object *rcObject = valueObj;
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
        valueStr->string = escapedString;       // Transfer ownership.
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
        valueStr->string = escapedString;       // Transfer ownership.
        if (!AddValue((JSON_Value**)&valueStr))
                {
                TERROR(("%s: Unable to add value",ProcName));
                delete valueStr;
                return false;
                }
        return true;
        }

bool JSON_Value_Array::Add_value_escString(const char *escString)
        {
        static const char *ProcName = "JSON_Value_Array::Add_value_escString";
        if (!escString)
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
        if (!valueStr->SetString(escString,strlen(escString)))
                {
                TERROR(("%s: Unable to set String",ProcName));
                delete valueStr;
                return false;
                }
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
        JSON_Value_Null *valueNull = new JSON_Value_Null();
        if (!valueNull)
                {
                TERROR(("%s: Unable to alloc JSON_Value_Null",ProcName));
                return false;
                }
        if (!AddValue((JSON_Value**)&valueNull))
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
        : JSON_Value(Source)
        {
        string = NULL;
        *this = Source;
        return;
        }

JSON_Value_String& JSON_Value_String::operator =(JSON_Value_String &Source)
        {
        if (this == &Source) return *this;
        if (string) free(string);
        if (Source.string) string = strdup(Source.string);
        else string = NULL;
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
// Polymorphic deep-copy of any JSON_Value subtype.
//
// USE_RecursiveDescent == 1
//
//      Recursive reference implementation (IBM Bob - 2026-03-10). 
//      A 1MB stack supports about 2500 nesting levels.
//                         
// USE_RecursiveDescent == 0
//
//      Iterative implementation (Eric - 2026-07-11).
//      O(1) stack usage regardless of tree depth.
//
// ---------------------------------------------------------------------------

#if USE_RecursiveDescent

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
        else if (Source->isNull())
                {
                rcValue = new JSON_Value_Null();
                }
        else    {
                TERROR(("%s: Unknown JSON_Value subtype",ProcName));
                }
        return rcValue;
        }

#else /* Iterative */

static JSON_Value* CloneJSONValue(JSON_Value *Source)
        {
        static const char *ProcName = "CloneJSONValue";
        unsigned Level = 0;
        bool After = false; 
        JSON_Value *CopyContainer = NULL;
        JSON_Value *CopyRoot = NULL;
        JSON_Value *Current = Source;
        while (Current)
                {
                if (After)
                        {
                        CopyContainer = CopyContainer->GetParent();
                        Current = SequentialGetNext(Current,Level,After);
                        continue;
                        }
                // ------------------------------------------------
                // ---- Add Member / Item to Current Container ----
                // ------------------------------------------------
                JSON_Value *rcCopy = NULL;
                if (Current->isString()) 
                        {
                        JSON_Value_String *vString = (JSON_Value_String *)Current;
                        rcCopy = new JSON_Value_String(*vString);
                        }
                else if (Current->isNumber())
                        {
                        JSON_Value_Number *vNumber = (JSON_Value_Number *)Current;
                        rcCopy = new JSON_Value_Number(*vNumber);
                        }
                else if (Current->isBoolean())
                        {
                        JSON_Value_Boolean *vBoolean = (JSON_Value_Boolean *)Current;
                        rcCopy = new JSON_Value_Boolean(*vBoolean);
                        }
                else if (Current->isArray())
                        {
                        rcCopy = new JSON_Value_Array();
                        }
                else if (Current->isObject())
                        {
                        rcCopy = new JSON_Value_Object();
                        }
                else if (Current->isNull())
                        {
                        rcCopy = new JSON_Value_Null();
                        }
                else    {
                        TERROR(("%s: Unknown JSON_Value subtype",ProcName));
                        }
                if (!rcCopy)
                        {
                        TERROR(("%s: Copy Failed",ProcName));
                        if (CopyRoot) delete CopyRoot;
                        return NULL;
                        }
                JSON_Value *rcHold = rcCopy;
                if (!CopyRoot) 
                        {
                        CopyRoot = rcCopy;
                        rcCopy = NULL;
                        }
                else if (CopyContainer->isObject())
                        {
                        JSON_Value_Object *CopyObject;
                        CopyObject = (JSON_Value_Object *)CopyContainer;
                        JSON_Member *Member = GetValueMember(Current);
                        const char *Key = Member ? Member->Key : NULL;
                        CopyObject->AddValue(Key,&rcCopy);
                        }
                else if (CopyContainer->isArray())
                        {
                        JSON_Value_Array *CopyArray;
                        CopyArray = (JSON_Value_Array *)CopyContainer;
                        CopyArray->AddValue(&rcCopy);
                        }
                if (rcCopy)
                        {
                        TERROR(("%s: AddValue Failed",ProcName));
                        if (CopyRoot) delete CopyRoot;
                        return NULL;
                        }
                if (rcHold->isArray() || rcHold->isObject())
                        {
                        CopyContainer = rcHold;
                        }
                // ---------------------
                // ---- Traverse... ----
                // ---------------------
                Current = SequentialGetNext(Current,Level,After);
                }
        return CopyRoot;
        }

#endif /* !USE_RecursiveDescent */

// ---------------------------------------------------------------------------
// ---- BuildJSONObject(Node* JSONExpression) --------------------------------
// -------------------------------------------
//
// Note: BuildJSONObject enforces MAX_BuildJSONObject_DEPTH call
//       stack recursive descent limit (~2 * JSON nesting-level)
//       during construction from a parse-tree, which fires well
//       before the ~1700 nesting-level limit on a 1MB stack. 
//
// ---------------------------------------------------------------------------

class Error_BuildJSON_Depth : public JSON_Object_Error
        {
        private:
                const char *ProcName;
        public:
                Error_BuildJSON_Depth(const char *Proc)
                        {
                        ProcName = Proc;
                        }
                const char* GetProcName() {return ProcName;}
        };

static void BuildJSONArray(unsigned Depth, 
                           JSON_Value_Array &Array, 
                           Node *Expression);

static void BuildJSONObject(unsigned Depth,
                            JSON_Object &Object, 
                            Node *Expression);

static JSON_Value* BuildJSONValue(unsigned Depth, Node *ValueNode)
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
                        if (Depth >= MAX_BuildJSONObject_DEPTH) 
                                {
                                throw Error_BuildJSON_Depth(ProcName);
                                }
                        JSON_Value_Object *newObject = new JSON_Value_Object;
                        if (!newObject)
                                {
                                TERROR(("%s: Unable to alloc newObject",ProcName));
                                throw JSON_Object_Error("Unable to alloc newObject");
                                }
                        BuildJSONObject(Depth+1,*newObject,Left);
                        ReturnValue = newObject;
                        break;
                        }
                case Ew_JSONArrayID: 
                        {
                        if (Depth >= MAX_BuildJSONObject_DEPTH) 
                                {
                                throw Error_BuildJSON_Depth(ProcName);
                                }
                        JSON_Value_Array *newArray = new JSON_Value_Array;
                        if (!newArray)
                                {
                                TERROR(("%s: Unable to alloc newArray",ProcName));
                                throw JSON_Object_Error("Unable to alloc newArray");
                                }
                        BuildJSONArray(Depth+1,*newArray,Left);
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
                case Ew_nullID: 
                        {
                        JSON_Value_Null *newNull = new JSON_Value_Null;
                        if (!newNull)
                                {
                                TERROR(("%s: Unable to alloc newNull",ProcName));
                                throw JSON_Object_Error("Unable to alloc newNull");
                                }
                        ReturnValue = newNull;
                        break;
                        }
                }
        return ReturnValue;
        }

static void BuildJSONArray(unsigned Depth, 
                           JSON_Value_Array &Array, 
                           Node *Expression)
        {
        static const char *ProcName = "BuildJSONArray";
        unsigned Level = 0;
        do      {
                unsigned TypeID = Expression->GetTypeID();
                if (TypeID == Ew_JSONValueID)
                        {
                        // ******************************
                        // **** Add a Value to Array ****
                        // ******************************
                        if (Depth >= MAX_BuildJSONObject_DEPTH) 
                                {
                                throw Error_BuildJSON_Depth(ProcName);
                                }
                        JSON_Value* newValue = BuildJSONValue(Depth+1,Expression);
                        Array.AddValue(&newValue);
                        if (newValue) 
                                {
                                TERROR(("%s: Unable to add Value",ProcName));
                                delete newValue;
                                }
                        // ***************************************
                        // **** Skip already handled children ****
                        // ***************************************
                        unsigned TopLevel = Level;
                        do      {
                                Expression = SequentialGetNext(Expression,Level);
                                } while (Level > TopLevel);
                        }
                Expression = SequentialGetNext(Expression,Level);
                } while (Expression && Level);
        return;
        }

static void BuildJSONObject(unsigned Depth, 
                            JSON_Object &Object, 
                            Node *Expression)
	{
        static const char *ProcName = "BuildJSONObject";
        unsigned Level = 0;
        do      {
                unsigned TypeID = Expression->GetTypeID();
                if (TypeID == Ew_JSONMemberID)
                        {
                        // ********************************
                        // **** Add a Member to Object ****
                        // ********************************
                        if (Depth >= MAX_BuildJSONObject_DEPTH) 
                                {
                                throw Error_BuildJSON_Depth(ProcName);
                                }
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
                        newMember->Value = BuildJSONValue(Depth+1,ValueNode);
                        Object.AddMember(&newMember);
                        if (newMember) 
                                {
                                TERROR(("%s: Unable to add Member",ProcName));
                                delete newMember;
                                }
                        // ***************************************
                        // **** Skip already handled children ****
                        // ***************************************
                        unsigned TopLevel = Level;
                        do      {
                                Expression = SequentialGetNext(Expression,Level);
                                } while (Level > TopLevel);
                        }
                Expression = SequentialGetNext(Expression,Level);
                } while (Expression && Level);
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
        try     {
                BuildJSONObject(0,*Object,JSONExpression);
                }
        catch (Error_BuildJSON_Depth &Error)
                {
                TERROR(("%s: Maximum Object Depth Reached: %s",
                        ProcName,
                        Error.GetProcName()));
                delete Object;
                Object = NULL;
                }
        catch (JSON_Object_Error &Error)
                {
                TERROR(("%s: Object Build Failed: %s",
                        ProcName,
                        Error.GetText()));
                delete Object;
                Object = NULL;
                }
        return Object;
        }

// ***************************************************************************
// **** PrintJSONObject(JSON_Object *Object) *********************************
// *******************************************
//
// USE_RecursiveDescent == 1
//
//      Recursive reference implementation. 
//      A 1MB stack supports over 2500 nesting levels.
//                         
// USE_RecursiveDescent == 0
//
//      Iterative implementation. 
//      O(1) stack usage regardless of tree depth.
//
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

static void PrintJSONMember(PrintObjectStateJSON &State,
                            unsigned Depth, 
                            JSON_Member &Member)
        {
        static const char *ProcName = "PrintJSONMember";
        if (State.asJSON)
                {
                State.Indent = Depth * 2;
                State.printIndent();
                State.printf("\"%s\":",Member.Key);
                }
        else    {
                State.Indent = Depth * 2;
                State.printIndent();
                State.printf("Key=%s\n",Member.Key);
                }
        return;
        }

static void PrintJSONValue(PrintObjectStateJSON &State,
                           unsigned Depth, 
                           JSON_Value *_Value)
        {
        static const char *ProcName = "PrintJSONValue";
        if (!_Value || _Value->isNull())
                {
                if (State.asJSON)
                        {
                        State.printf("null");
                        }
                else    {
                        State.Indent = Depth * 2;
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
                        State.Indent = Depth * 2;
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
                        State.Indent = Depth * 2;
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
                                State.Indent = Depth * 2;
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
                                State.Indent = Depth * 2;
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
                                State.Indent = Depth * 2;
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
                                State.Indent = Depth * 2;
                                State.printIndent();
                                State.printf("Value=%g\n",Value.double_number);
                                }
                        }
                }
        // -----
        // Note: Won't reach here if via PrintJSONObject_Sequential()
        // -----
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
                JSON_Value_Object &Object = *(JSON_Value_Object*)_Value;
                if (State.asJSON) 
                        {
                        if (Depth) 
                                {
                                State.printNL();
                                State.Indent = Depth * 2;
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
                        PrintJSONValue(State,Depth+1,Object[i].Value);
                        }
                if (State.asJSON) 
                        {
                        State.printNL();
                        State.Indent = Depth * 2;
                        State.printIndent();
                        State.printf("}");
                        }
                }
        return;
        }

static void PrintJSONValue_Sequential(PrintObjectStateJSON &State,
                                      JSON_Value &Value)
        {
        static const char *ProcName = "PrintJSONValue_Sequential";
        unsigned Depth = 0;
        unsigned Level = 0;
        bool After = false; 
        JSON_Value *Current = &Value;
        while (Current)
                {
                if (After)
                        {
                        // -------------------------------------
                        // ---- Closing Braces and Brackets ----
                        // -------------------------------------
                        if (Current->isObject())
                                {
                                Depth--;
                                if (State.asJSON) 
                                        {
                                        State.printNL();
                                        State.Indent = Depth*2;
                                        State.printIndent();
                                        State.printf("}");
                                        }
                                }
                        else if (Current->isArray())
                                {
                                if (State.asJSON) State.printf("]");
                                }
                        }
                else    {
                        unsigned IndexInParent = Current->GetIndexInParent();
                        if (State.asJSON && IndexInParent) State.printf(",");
                        if (Depth)
                                {
                                JSON_Member *Member = GetValueMember(Current);
                                if (Member) 
                                        {                                
                                        if (State.asJSON) State.printNL();
                                        PrintJSONMember(State,Depth-1,*Member);
                                        }
                                }
                        // ----------------------------------
                        // ---- Open Braces and Brackets ----
                        // ----------------------------------
                        if (Current->isObject())
                                {
                                if (State.asJSON) 
                                        {
                                        if (Depth) 
                                                {
                                                State.printNL();
                                                State.Indent = Depth*2;
                                                State.printIndent();
                                                }
                                        State.printf("{");
                                        }
                                Depth++;
                                }
                        else if (Current->isArray())
                                {
                                if (State.asJSON) State.printf("[");
                                }
                        // ----------------------------------
                        // ---- Print Members and Values ----
                        // ----------------------------------
                        else PrintJSONValue(State,Depth,Current);
                        }
                // ---------------------
                // ---- Traverse... ----
                // ---------------------
                Current = SequentialGetNext(Current,Level,After);
                }
        return;
        }

// ----------------------------------------------------------
// ----------------------------------------------------------

int PrintJSONValue(JSON_Value &Value,                   // Returns number of
                   PrintJSONDocumentFn_t PrintFn,       // bytes output.
                   void *PrintUserPtr,
                   bool asJSON,
                   bool Pretty)
        {
        PrintObjectStateJSON State(PrintFn,PrintUserPtr,asJSON,Pretty);
  #if USE_RecursiveDescent
        PrintJSONValue(State,0,&Value);
  #else
        PrintJSONValue_Sequential(State,Value);
  #endif
        return State.nPrintBytes;
        }

// -------------------
// ---- To stdout ----
// -------------------

int PrintJSONValue(JSON_Object &Object,        // Returns bytes output.
                   bool asJSON,
                   bool Pretty)           
        {
        PrintJSONDocumentFn_t PrintFn = (PrintJSONDocumentFn_t)vfprintf;
        void *PrintUserPtr = stdout;
        return PrintJSONValue(Object,PrintFn,PrintUserPtr,asJSON,Pretty);
        }

// ***************************************************************************
// ******************************* End of File *******************************
// ***************************************************************************
