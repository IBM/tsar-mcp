// AutoString.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: AutoString.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Auto_String_Class

     #define __Auto_String_Class

#include "Errors.h"

class AutoString_Error_Memory : public Error_Memory {};

// ***************
// **** ASCII ****
// ***************

class AutoString
{
    private:

	char *Buffer;
	size_t Size;
	size_t AllocSize;
	bool Realloc(size_t NewSize);

    public:

	AutoString() {Size = AllocSize = 0; Buffer = NULL;}
	AutoString(const char *string);
	AutoString(const AutoString &string);
	~AutoString();
	char* GetString() {return Buffer ? Buffer : (char*)"";}
	size_t GetLength() {return Size;}
	void Reset(size_t InitSize);
	// ********************
	// **** Assignment ****
	// ********************
	void operator =(const char *Source);
	void operator =(const AutoString &Source);
	void operator <<=(const char *Source);
	// *****************
	// **** Compare ****
	// *****************
	bool operator ==(const char *ToCompare);
	bool operator ==(const AutoString &ToCompare);
	bool operator >(const char *ToCompare);
	bool operator >(const AutoString &ToCompare);
	bool operator <(const char *ToCompare);
	bool operator <(const AutoString &ToCompare);
};

// *****************
// **** UNICODE ****
// *****************

class AutoStringW
{
    private:

	wchar_t *BufferW;
	size_t SizeW;
	size_t AllocSizeW;
	bool ReallocW(size_t NewSize);

    public:

	AutoStringW() {SizeW = AllocSizeW = 0; BufferW = NULL;}
	AutoStringW(const char *string);
        AutoStringW(const wchar_t *string);
	AutoStringW(const AutoStringW &string);
	~AutoStringW();
	const wchar_t* GetString() {return BufferW ? BufferW : L"";}
	size_t GetLength() {return SizeW;}
	void Reset(size_t InitSizeW);
	// ********************
	// **** Assignment ****
	// ********************
        void operator =(const char *Source);
	void operator =(const wchar_t *Source);
	void operator =(const AutoStringW &Source);
	void operator <<=(const char *Source);
        void operator <<=(const wchar_t *Source);
	// *****************
	// **** Compare ****
	// *****************
	bool operator ==(const wchar_t *ToCompare);
	bool operator ==(const AutoStringW &ToCompare);
	bool operator >(const wchar_t *ToCompare);
	bool operator >(const AutoStringW &ToCompare);
	bool operator <(const wchar_t *ToCompare);
	bool operator <(const AutoStringW &ToCompare);
};

/* *****************************************************************
 *******************************************************************

 Notes:

     I.	AutoString class

 *******************************************************************
 **** Example ******************************************************
 **************
 *******************************************************************

 *******************************************************************
 ************************* End of File *****************************
 **************************************************************** */

#endif
