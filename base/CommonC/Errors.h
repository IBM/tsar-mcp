// Error Classes
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: Errors.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Error_Classes

     #define __Error_Classes

class ErrorBase
	{
	private:
		const char *ErrorText;
	public: 
                ErrorBase() {ErrorText = "";}
		ErrorBase(const char *errorText) 
                        {
                        ErrorText = errorText;
                        }
                const char* GetErrorText() {return ErrorText;}
                const char* GetText() {return ErrorText;}
	};

class Error_Memory : public ErrorBase
        {
        public:
                Error_Memory() : ErrorBase() {};
                Error_Memory(const char *errorText) : ErrorBase(errorText) {}
        };

class Error_InOp : public ErrorBase 		// Invalid Operation.
        {
        public:
                Error_InOp() : ErrorBase() {};
                Error_InOp(const char *errorText) : ErrorBase(errorText) {}
        };

class Error_Construction : public ErrorBase 
	{
        protected:
                Error_Construction() : ErrorBase() {}
	public: 
                Error_Construction(const char *className) : ErrorBase(className)
                        {
                        };
                const char* GetClassName() {return GetErrorText();}
	};

class Error_Syntax : public ErrorBase
        {
        public:
                Error_Syntax() : ErrorBase() {};
                Error_Syntax(const char *errorText) : ErrorBase(errorText) {}
        };

class Error_Grammar : public ErrorBase
        {
        public:
                Error_Grammar(const char *errorText) : ErrorBase(errorText) {}
        };

#endif

