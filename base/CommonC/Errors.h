///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: Errors.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.                                                
//                    
// Author(s) : Eric Kass 
//
///////////////////////////////////////////////////////////////////////////////
#ifndef __Error_Classes

     #define __Error_Classes

class Error
	{
	private:
		const char *ErrorText;
	public: 
                Error() {ErrorText = "";}
		Error(const char *errorText) 
                        {
                        ErrorText = errorText;
                        }
                const char* GetErrorText() {return ErrorText;}
                const char* GetText() {return ErrorText;}
	};

class Error_Memory : public Error
        {
        public:
                Error_Memory() : Error() {};
                Error_Memory(const char *errorText) : Error(errorText) {}
        };

class Error_InOp : public Error 		// Invalid Operation.
        {
        public:
                Error_InOp() : Error() {};
                Error_InOp(const char *errorText) : Error(errorText) {}
        };

class Error_Construction : public Error 
	{
        protected:
                Error_Construction() : Error() {}
	public: 
                Error_Construction(const char *className) : Error(className)
                        {
                        };
                const char* GetClassName() {return GetErrorText();}
	};

class Error_Syntax : public Error
        {
        public:
                Error_Syntax() : Error() {};
                Error_Syntax(const char *errorText) : Error(errorText) {}
        };

class Error_Grammar : public Error
        {
        public:
                Error_Grammar(const char *errorText) : Error(errorText) {}
        };

#endif

