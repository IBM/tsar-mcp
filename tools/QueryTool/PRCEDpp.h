///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: PRCEDpp.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
/** 
 * @file PRCEDpp.h
 * Function: C++ Classes into the AS/400 QSQPRCED Database API
 *
 * Note: 'Lengths' are number of units: Bytes/Character/DoubleByteCharacter
 *       'Size' are number of bytes.
 */
#ifndef __PRCED_CPP_CLASSES

        #define __PRCED_CPP_CLASSES

#include <qusec.h>
#include <qsqprced.h>

#include <ASThread.h>
#include <LinkList.h>

#if defined(_WIN32) || defined(__OS400_TGTVRM__)
        #include <stddef.h>
        /** wchar_t as u2char_t */
        typedef wchar_t u2char_t;
        /** Whether \ref u2char_t is a wchar_t or a short */
        #define U2CHAR_IS_WCHAR 1
#else
        /** short as u2char_t */
        typedef short u2char_t;
        /** Whether \ref u2char_t is a wchar_t or a short */
        #define U2CHAR_IS_WCHAR 0
#endif

#ifndef PRCEDpp_USE410
        /** Either use SQLP0400 or SQLP0410 function template.
          * Specify before including PRCEDpp.h
          */
        #define PRCEDpp_USE410 0
#endif

#if !PRCEDpp_USE410
        /** Use SQLP0400 functin template */
        typedef Qsq_SQLP0400_t Qsq_SQLP04NN_t;
        /** Function template name */
        #define SQLP04NN_Format "SQLP0400"
#else
        /** Use SQLP0410 functin template */
        typedef Qsq_SQLP0410_t Qsq_SQLP04NN_t;
        /** Function template name */
        #define SQLP04NN_Format "SQLP0410"
#endif

/**
 * @defgroup typedefs Typedefs
 * @{
 */
/** Byte data type */
typedef unsigned char byte_t;
/** 4-byte reference to a LOB  */
typedef unsigned int LobLocator_t;
/** @} */

/** Maximum length of OS/400 object names */
#define OBJNAMELEN 10
/** Default size of \ref ASError buffer */
#define ERROR_MEM_SIZE 120

/**
 * @defgroup datatypes Supported Data Types
 * @{
 */
#define DA_BIG_TYPE 493 /**< 64-bit integer type */
#define DA_INT_TYPE 497 /**< 32-bit integer type */
#define DA_SHORT_TYPE 501 /**< 16-bit integer type */
#define DA_CHAR_TYPE 453 /**< fixed-length character type */
#define DA_VARCHAR_TYPE 449 /**< variable-length character string */
#define DA_WCHAR_TYPE 469 /**< fixed-length wide character type Type: u2char_t */
#define DA_VARWCHAR_TYPE 465 /**< variable-length wide character string */
#define DA_FLOAT_TYPE 481 /**< floating-point type */
#define DA_PACKED_TYPE 485 /**< decimal type */
#define DA_TIMESTAMP_TYPE 393 /**< timestamp type */

/**
 * @defgroup lobs LOB
 * @{
 */
#define DA_BLOB_TYPE 405 /**< binary large object */
#define DA_CLOB_TYPE 409 /**< character large object */
#define DA_DBCLOB_TYPE 413 /**< double-byte character large object Type: u2char_t */
/** @} */

/**
 * @defgroup loblocators LOB Locator
 * @{
 */
#define DA_BLOBLOC_TYPE 961 /**< BLOB Locator */
#define DA_CLOBLOC_TYPE 965 /**< CLOB Locator */
#define DA_DBCLOBLOC_TYPE 969 /**< DBCLOB Locator Type: u2char_t */
/** @} */

#define DA_XML_TYPE 989 /**< XML type Default CCSID 1208 (UTF-8) */
/** @} */

/**
 * @defgroup ccsids Supported CCSIDs
 * @{
 */
/** UCS-2 */
#define UCS2_CCSID 13488
/** ASCII */
#define ASCII_CCSID 819  
/** EBCDIC */
#define EBCDIC_CCSID 500
/** UTF-8 */
#define UTF8_CCSID 1208
/** @} */

#ifdef __OS400_TGTVRM__
        /** Local CCSID, EBCDIC on IBM i, otherwise ASCII */
        #define LOCAL_CCSID EBCDIC_CCSID
#else
        /** Local CCSID, EBCDIC on IBM i, otherwise ASCII */        
        #define LOCAL_CCSID ASCII_CCSID
#endif

/**
 * @defgroup errtype Error typedefs
 * @{
 */
/** Throwable generic error class */
typedef Error ErrorClass;
/** Throwable error class for failed allocations */
typedef Error MemoryError;
/** @} */

/** 
 * Throwable Error Class for Qus_EC_t struct 
 */
class ASError : public ErrorClass
        {
        private:
                char ErrorMemory[ERROR_MEM_SIZE];
        public:
                /** @param Text error message */
                ASError(const char *Text) : ErrorClass(Text) 
                        {
                        Qus_EC_t *ErrorCode = (Qus_EC_t *)ErrorMemory;
                        ErrorCode->Bytes_Provided = ERROR_MEM_SIZE;
                        ErrorCode->Bytes_Available = 0;
                        ErrorCode->Reserved = 0;
                        }
                /** @return Name of class ("ASError") */
                virtual char* GetErrorClassName() {return "ASError";}
                /** @return Length of the error information */
                int GetBytesAvailable() 
                        {
                        return ((Qus_EC_t *)ErrorMemory)->Bytes_Available;
                        }
                /** @return Error struct */
                Qus_EC_t* GetErrorCode() 
                        {  
                        return (Qus_EC_t *)ErrorMemory;
                        }
                /** @return Variable-length character field that contains 
                 *         insert data associated with the exception ID 
                 */
                const char* GetExceptionData() 
                        {
                        return ErrorMemory + sizeof(Qus_EC_t);
                        }
                /** @return Message identifier */
                const char* GetExceptionID() 
                        {
                        return ((Qus_EC_t *)ErrorMemory)->Exception_Id;
                        }
                /** @return 0 if error information is empty otherwise 1 */
                int IsError() {return GetBytesAvailable() != 0;}
                /** @return Length of error information */
                int GetExceptionDataLength() 
                        {
                        return GetBytesAvailable() - sizeof(Qus_EC_t);
                        }
        };

/** 
 * QSQPRCED Function Template class
 *
 * Uses either the SQLP0400 or the SQLP0410 function template
 */
class SQLCtrlBlock : public Link
        {
        private:
                Qsq_SQLP04NN_t *Qsq;
        protected:
                const char* GetFormat() {return SQLP04NN_Format;}
                Qsq_SQLP04NN_t* GetQsq() {return Qsq;}
                void SetBlockingFactor(int Rows);
                void SetCursor(const char *Cursor);
                void SetDirectMap(bool DirectMap);
                void SetFunction(char Function);
                void SetPackage(const char *Library, const char *Package);
                void SetReopen(bool Reopen);
                void SetRowsToInsert(int Rows);
                void SetStatementName(const char *StatementName);
                void SetStatementName(const char *Name, size_t Length);
                void SetSortSeq(const char *Library, const char *SortSeq);
                void SetStatementText(const char *Text);
                void SetStatementText(const char *Text, size_t TextLength);
                friend class PRCED_Statement;
                friend class PRCED_Cursor;
                friend class SQLCtrlBlockCache;
        public:
                /**  @throws MemoryError */
                SQLCtrlBlock();
                /** @param CB create SQLCtrlBlock from parameter */
                SQLCtrlBlock(SQLCtrlBlock &CB);
                /** Destructor */
                ~SQLCtrlBlock();
                /** @return Cursor name */
                const char* GetCursor() {return Qsq->Cursor_Name;}
                /** @return CCSID of statement (\ref ccsids) */
                unsigned GetStatementCCSID();
                /** @return Length of statement */
                size_t GetStatementLength();
                /**
                 * @param NameLength Where to store the length of the statement
                 * @return Statement name
                 */
                const char* GetStatementName(size_t *NameLength);
                /** @return Statement text */
                const char* GetStatementText();
                /**
                 * @return false if statement name is NULL or starts with space
                 *         otherwise true
                 */
                bool HasStatementName();
                /**
                 * Sets top program for the application
                 * When the program ends all cursors are closed and the SQL environment 
                 * goes away. Program must be on stack or the SQL0901 error will occur.
                 *
                 * @param Library Library of the main program
                 * @param Program Top program in the SQL application
                 */
                void SetProgram(const char *Library, const char *Program);
                /** 
                 * assigment operator 
                 * @throws MemoryError
                 */
                SQLCtrlBlock& operator = (SQLCtrlBlock &CB);
        };

/** Defines the SQLCtrlBlockList class */
DefineLinkList(SQLCtrlBlockList,SQLCtrlBlock);

/** 
 * Function Template Cache
 *
 * Stores already prepared function templates for reuse
 */
class SQLCtrlBlockCache : protected SQLCtrlBlockList
        {
        protected:
                Mutex ListMutex;
        public:
                /** Constructor */
                SQLCtrlBlockCache() {SetAutoCleanup(true);}
                /** @return function template struct */
                SQLCtrlBlock* GetCB();
                /** @param CB stores first SqlCtrlBlock here */
                void ReturnCB(SQLCtrlBlock* CB);
        };

/** 
 * SQLDA Variable 
 */
struct DAVariable
        {
        /** Data type from \ref datatypes */
        int Type;
        /** Variable name */
        char *Name;
        /** Length in bytes */
        size_t Length;
        /** Number of digits (for packed variables only) */
        int Digits;
        /** Scale of variable (for packed variables only) */
        int Scale;
        /** CCSID from \ref ccsids */
        unsigned CCSID;
        /** Pointer to variable data */
        char *DataPtr;
        /** Pointer to indicator variable */
        short *IndicatorPtr;
        /** Pointer to LOB length */
        unsigned *LobLenPtr;
        /**
         * Constructor
         *
         * @param type data type from \ref datatypes
         * @param name name string may be NULL
         * @param length length of the data (can be 0 for 
         *        \ref datatypes of fixed length)
         * @param ccsid CCSID from \ref ccsids (only required for character data)
         */
        DAVariable(int type, 
                   const char *name, 
                   size_t length, 
                   unsigned ccsid=0);
        /** Creates new instance from parameter Src */ 
        DAVariable(DAVariable &Src);
        /** Destructor */
        ~DAVariable() {free(Name);}
        /** Sets DataPtr, IndicatorPtr and LobLenPtr */
        void SetBuffer(char *Data, short *Indicator, unsigned *LobLen);
        /** assignment operator */
        DAVariable& operator =(DAVariable &Src);
        };

/** 
 * Variables of an SQLDA 
 */
class DAVariables
        {
        private:
                bool DirectMap;
                Qsq_sqlda_t *DA;
                size_t RecordSize;
                unsigned VarCount;
                unsigned VarListSize;
                DAVariable **VarList;
        protected:
                bool AddVariable(DAVariable &Src);
                void Clear();
                unsigned CountVars() {return VarCount;}
                void ConstructDA();
                void ConstructNullDA(unsigned nVars);
                size_t HostVarByteSize(int i);
                short* GetIndicatorPtr(int Row, int Col);
                void SetColumn(int i, 
                               int DataType, 
                               size_t Length, 
                               unsigned CCSID);
                void SetHostVar(int i,
                                char **Buffer, 
                                short **Indicators, 
                                unsigned **LobLens,
                                unsigned nRows);
                friend class PRCED_Cursor;
                friend class PRCED_Statement;
        public:
                /** Constructor */
                DAVariables();
                /** @param Src source SQLDA */
                DAVariables(DAVariables &Src);
                /** Merges two SQLDAs */
                DAVariables(DAVariables &Src1, DAVariables &Src2);
                /** Destructor */
                ~DAVariables();
                /** @return Number of columns in the SQLDA */
                unsigned GetColumnCount();
                /** @return SQLDA */
                Qsq_sqlda_t* GetDA();
                /** @param DM if true use direct map */
                void SetDirectMap(bool DM) {DirectMap = DM;}
                /** 
                 * assignment operator 
                 * @throws ErrorClass
                 */
                DAVariables& operator =(DAVariables &Src);
                // *****************************
                // **** Variable Definition ****
                // *****************************
                /** 
                 * Add variable of type decimal (\ref DA_PACKED_TYPE )
                 * @param Name Variable name
                 * @param Digits Number of digits
                 * @param Scale Number of digits right to the decimal point
                 */
                void AddPackedVariable(const char *Name, 
                                       unsigned Digits, 
                                       unsigned Scale);
                /** 
                 * Add variable
                 * @param Var DAVariable object
                 * @return false if allocation fails, otherwise true
                 */
                bool AddVariable(DAVariable *Var);
                /** 
                 * Add variable
                 * @param Col Column index
                 * @param Var DAVariable object
                 * @return false if allocation fails, otherwise true
                 */
                bool AddVariable(int Col, DAVariable *Var);
                /** 
                 * Add variable
                 * @param DataType Data type from \ref datatypes
                 * @param Name Variable name
                 * @param Length Size of variable (in byte)
                 * @param CCSID CCSID from \ref ccsids (if character type)
                 * @return false if allocation fails, otherwise true
                 */
                bool AddVariable(int DataType, 
                                 const char *Name, 
                                 size_t Length,
                                 unsigned CCSID=0);
                /** 
                 * Add variable
                 * @param Col Column index
                 * @param DataType Data type from \ref datatypes
                 * @param Name Variable Name
                 * @param Length Size of variable (in bytes)
                 * @param CCSID CCSID from \ref ccsids (if character type)
                 * @return false if allocation fails, otherwise true
                 */
                bool AddVariable(int Col,
                                 int DataType, 
                                 const char *Name, 
                                 size_t Length,
                                 unsigned CCSID=0);
                /** 
                 * Converts variables of type \ref lobs and 
                 * \ref DA_XML_TYPE to \ref loblocators 
                 */
                void ConvertLobs2Locators();
                /** @return Size of DA */
                size_t GetRecordSize();
                /**
                 * @param i Variable index
                 * @return Digits of variable
                 *         of type \ref DA_PACKED_TYPE
                 */
                unsigned GetVariableDigits(int i);
                /**
                 * @param i Variable index
                 * @return Scale of variable
                 *         of type \ref DA_PACKED_TYPE
                 */
                unsigned GetVariableScale(int i);
                /**
                 * @param i Variable index
                 * @return Name of variable
                 */
                const char* GetVariableName(int i);
                /**
                 * @param i Variable index
                 * @return \ref datatypes
                 */
                int GetVariableType(int i);
                /**
                 * @param i Variable index
                 * @return Maximum length of variable
                 */
                size_t GetVariableMaxLength(int i);
                // ***********************
                // **** Variable Data ****
                // ***********************
                /**
                 * @param Col Column index
                 * @return CCSID from \ref ccsids of Column Col, 0 if no character data
                 */
                unsigned GetCCSID(int Col);
                /**
                 * @param Row Row index
                 * @param Col Column index
                 * @return Value of indicator variable
                 */
                short GetIndicator(int Row, int Col);
                /**
                 * @param Row Row index
                 * @param Col Column index
                 * @return Pointer to variable, NULL if empty
                 */
                void* GetVariable(int Row, int Col);
                /**
                 * @param Row Row index
                 * @param Col Column index
                 * @return Pointer to variable data
                 */
                char* GetVariableData(int Row, int Col);
                /**
                 * @param Row Row index
                 * @param Col Column index
                 * @return Length of variable in characters or bytes respectively
                 */
                size_t GetVariableLength(int Row, int Col);
                /**
                 * @param Col Column index
                 * @param CCSID CCSID from \ref ccsids
                 */
                void SetCCSID(int Col, unsigned CCSID);
                /**
                 * @param Row Row index
                 * @param Col Column index
                 * @param Indicator Value of Indicator variable
                 */
                void SetIndicator(int Row, int Col, short Indicator);
                /**
                 * @param Row Row index
                 * @param Col Column index
                 * @param Value Value (is converted)
                 */
                void SetPackedVariable(int Row, int Col, int Value);
                /**
                 * @param Row Row index
                 * @param Col Column index
                 * @param Data Pointer to value
                 * @param Length of data
                 * @param SourceCCSID CCSID from \ref ccsids
                 */
                void SetVariable(int Row, 
                                 int Col, 
                                 void *Data, 
                                 size_t Length,
                                 unsigned SourceCCSID=0);
                /**
                 * Set variable to integer (wraps 
                 * \ref SetVariable(int, int, void*, size_t, unsigned))
                 * @param Row Row index
                 * @param Col Column index
                 * @param Integer Value
                 */
                void SetVariable(int Row, int Col, int Integer);
                /**
                 * Set variable to integer (wraps 
                 * \ref SetVariable(int, int, void*, size_t, unsigned))
                 * @param Row Row index
                 * @param Col Column index
                 * @param Integer Value
                 */
                void SetVariable(int Row, int Col, unsigned Integer);
                /**
                 * Set variable to double (wraps 
                 * \ref SetVariable(int, int, void*, size_t, unsigned))
                 * @param Row Row index
                 * @param Col Column index
                 * @param Double Value
                 */
                void SetVariable(int Row, int Col, double Double);
                /**
                 * Set variable to string value (wraps 
                 * \ref SetVariable(int, int, void*, size_t, unsigned))
                 * @param Row Row index
                 * @param Col Column index
                 * @param String Value
                 */
                void SetVariable(int Row, int Col, const char *String);
                /**
                 * Set variable to wide character string (wraps 
                 * \ref SetVariable(int, int, void*, size_t, unsigned))
                 * @param Row Row index
                 * @param Col Column index
                 * @param String Value
                 */
                void SetVariable(int Row, int Col, const u2char_t *String);
  #if !U2CHAR_IS_WCHAR
                /**
                 * Set variable to wide character string (wraps 
                 * \ref SetVariable(int, int, void*, size_t, unsigned))
                 * @param Row Row index
                 * @param Col Column index
                 * @param String Value
                 */
                void SetVariable(int Row, int Col, const wchar_t *String);
  #endif
                /**
                 * Set variable length
                 * @param Row Row index
                 * @param Col Column index
                 * @param Length Length of variable
                 */
                void SetVariableLength(int Row, int Col, size_t Length);
                // **********************
                // **** Bind Buffers ****
                // **********************
                /**
                 * @param Col Column index
                 * @param Buffer Char buffer (already allocated)
                 * @param Indicator Short buffer (already allocated) 
                 * @param LobLen Unsigned buffer (already allocated)
                 *               can be ignored if no LOBs are used
                 */
                void SetBuffer(int Col,
                               char *Buffer, 
                               short *Indicator,
                               unsigned *LobLen = NULL);
                /**
                 * @param Buffer Char buffer (already allocated)
                 * @param Indicator Short buffer (already allocated)
                 */
                void SetBuffers(char *Buffer, short *Indicator);
                /**
                 * @param Buffer Char buffer (already allocated)
                 * @param Indicator Short buffer (already allocated)
                 * @param LobLens Unsigned buffer (already allocated)
                 * @param nRows Number of rows in SQLDA (e.g. for multirow inserts)
                 */
                void SetBuffers(char *Buffer, 
                                short *Indicator, 
                                unsigned *LobLens,
                                unsigned nRows);
        };
        
/**
 * SQL Statement class
 */
class PRCED_Statement
        {
        private:
                static SQLCtrlBlockCache CBCache;
                SQLCtrlBlock *CB;
                Qsq_sqlca_t sqlca;
        protected:
                Qsq_sqlca_t* GetClearCA();
                SQLCtrlBlock& GetSQLCtrlBlock() {return *CB;}
                bool LoadVars(DAVariables &DestVars, const Qsq_sqlda_t *DA);
                friend class PRCED_Cursor; 
        public:
		/**
		 * Constructor
		 * @param StatementName Name of already prepared statement for 
                 *                      reuse, empty string for search 
                 *                      or new name, max. 18 characters
                 *                      when using SQLP0400 template, 
                 *                      max. 128 when using SQLP0410
		 * @param PackageLib Name of the library with the SQL package, 
                 *                   max. 10 characters
		 * @param Package Name of the SQL package, max. 10 characters
                 * @param SortSeqLib The sort sequence table name to be used for string 
                 *                   comparisons in SQL statements. The possible values follow:
                 *                   *JOB 	The sort sequence value for the job is retrieved 
                 *                              when the package is created.
                 *                   *JOBRUN 	The sort sequence value for the job is retrieved 
                 *                              when the program is run.
                 *                   *LANGIDUNQ The unique-weight sort table for the language that
                 *                              is specified on the language identifier field is used.
                 *                   *LANGIDSHR The shared-weight sort table for the language that is 
                 *                              specified on the language identifier field is used.
                 *                   *HEX 	A sort sequence table is not used. The hexadecimal values 
                 *                              of the characters are used to determine the sort sequence.
                 *                   table-name The name of the sort sequence table to be used. 
                 *                   only required when creating new SQL packages
                 * @param SortSeq The name of the sort sequence table can be qualified 
                 *                by one of the following library values:
                 *                *LIBL 	  All libraries in the job's library list 
                 *                                are searched until the first match is found.
                 *                *CURLIB         The current library for the job is searched. 
                 *                                If no library is specified as the current library 
                 *                                for the job, the QGPL library is used.
                 *                library-name 	  The name of the library to be searched.
                 *                only required when creating new SQL packages
                 * @throws ErrorClass
		 */
                PRCED_Statement(const char *StatementName,
                                const char *PackageLib, 
                                const char *Package,
                                const char *SortSeqLib=0, 
                                const char *SortSeq=0);
                /** Destructor */
                ~PRCED_Statement();
                /**
                 * Checks whether statement has already been prepared
                 * @return true if prepared, else false
                 * @see FindStatementName
                 */
                bool CheckStatementID();
		/**
		 * Creates SQL package
		 * Returns -601 if SQL package already exists
		 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
		 */
                int CreatePackage();
		/**
		 * Deletes SQL package
		 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
		 */
                int DeletePackage();
		/**
		 * Describes already prepared statement
		 * @param Columns empty DAVariables for the result
		 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
		 */
                int Describe(DAVariables &Columns);
		/**
		 * Executes statement that has already been prepared
		 * @param Input contains result
                 * @param nInsertRows number of rows in the SQLDA, default 0
		 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
		 */
                int Execute(DAVariables &Input, int nInsertRows=0);
                /**
		 * Checks whether statement has already been prepared
		 * @param Text null-terminated string with SQL statement
		 * @return true if found, otherwise false
		 */
                bool FindStatementName(const char *Text);
                /** @see FindStatementName(const char*) */ 
                bool FindStatementName(const char *Text, size_t TextLength);
                /** @return Function template struct or NULL if none */
                Qsq_SQLP04NN_t* GetQsq() {return CB ? CB->GetQsq() : NULL;}
                /** @return Statement CCSID (\ref ccsids) */
                unsigned GetStatementCCSID();
                /** @return Length of statement */
                size_t GetStatementLength();
                /**
                 * @param NameLength Where to store the length of the statement
                 * @return Statement name
                 */
                const char* GetStatementName(size_t *NameLength);
                /** @return Statement text */
                const char* GetStatementText();
                /**
		 * Prepares statement
		 * @param StatementText null-terminated SQL statement to prepare
		 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
		 */
                int Prepare(const char *StatementText);
                /** @see Prepare(const char *) */
                int Prepare(const char *StatementText, size_t Length);
		/**
		 * Prepares and describes statement in one step
		 * @param StatementText null-terminated SQL statement to 
                 *        prepare and describe
                 * @param DescribeVars contains result
		 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
		 */
                int PrepareDescribe(const char *StatementText, 
                                    DAVariables &DescribeVars);
                /** @see PrepareDescribe(const char*, DAVariables&) */
                int PrepareDescribe(const char *StatementText,
                                    size_t StatementLength,
                                    DAVariables &DescribeVars);
        };

/** 
 * Database Cursor class
 */
class PRCED_Cursor 
        {
        private:
                bool DeferOpen;
                bool AllRowsRead;
                Qsq_sqlca_t sqlca;
                int BlockingFactor;
                PRCED_Statement &Statement;
        protected:
                Qsq_sqlca_t* GetClearCA();
                void SetCursorName(char *CursorName);
        public:
                /**
                 * Constructor
                 * @param Statement SQL statement that has been prepared on the host
                 * @param CursorName cursor name, max. 18 characters when using 
                 *                   SQLP0400 template max. 128 when using SQLP0410
                 */
                PRCED_Cursor(PRCED_Statement &Statement, char *CursorName);
                /**
                 * @return true if last row has been fetched
                 * @see Fetch()
                 */
                bool AreAllRowsRead() {return AllRowsRead;}
                /**
                 * Close open cursor
                 * @param HardClose if true also delete open path data
                 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
                 */
                int Close(bool HardClose=false);
                /**
                 * Fetch data from open cursor
                 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
                 */
                int Fetch(DAVariables &OutputVars);
                /**
                 * @return Number of rows fetched  
                 */
                int GetRowsProcessed();
                /**
                 * @return Cursor name
                 */
                const char* GetCursorName();
                /**
                 * Open cursor
                 * @param nBlockedRows number of records passed from a Fetch()
                 * @param Reopen allow cursor to be reopened
                 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
                 */
                int Open(int nBlockedRows, bool Reopen=false);
                /**
                 * Open cursor
                 * @return SQL code, 0 if success, positive if warning, 
                 *         negative if error
                 * @see Open(int, bool)
                 */
                int Open(DAVariables &InputVars, 
                         int nBlockedRows, 
                         bool Reopen=false);
                /**
                 * @param Defer if true let XDA/XDN perform a Fetch 
                 *              right after an Open and cache result  
                 */
                void SetDeferOpen(bool Defer) {DeferOpen = Defer;}
        };

// ***********************
// **** PRCED_Locator ****
// ***********************

/**
 * Set default SQL package for LOB operations
 *
 * @param Library Library name
 * @param Package SQL package name
 */
void SetLobPackageDefault(const char *Library, const char *Package);

/** 
 * LOB Locator class
 */
class PRCED_Locator
        {
        private:
                bool AutoFree;
                int LobLocType;
                size_t LobSize;
                unsigned LobCCSID;
                const char *LobPackageLib;
                const char *LobPackage;
                LobLocator_t Locator;
        protected:
                bool FreeLocator();
                bool _Append(const byte_t *Buffer, size_t nUnits);
                bool _Assign(const byte_t *Buffer, size_t nUnits);
                void* BuildInputBuffer(const char *Buffer, size_t nChars);
                void* BuildInputBuffer(const byte_t *Buffer, size_t nChars);
                void* BuildInputBuffer(const u2char_t *Buffer, size_t nChars);
  #if !U2CHAR_IS_WCHAR
                void* BuildInputBuffer(const wchar_t *Buffer, size_t nChars);
  #endif
                size_t _GetSubString(byte_t *Buffer, 
                                     size_t nStart, 
                                     size_t nUnits);
                void InitFromDAVars(DAVariables &Variables, int Row, int Col);
                size_t Length2Size(size_t Length);
                size_t Size2Length(size_t Size);
        public:
                /**
                 * @param Library Library with the SQL package
                 * @param Package SQL package
                 * @param LocType Locator type (\ref loblocators)
                 * @param LobCCSID CCSID from \ref ccsids
                 */
                PRCED_Locator(const char *Library,
                              const char *Package,
                              int LocType,
                              unsigned LobCCSID=0);
                /**
                 * @param LocType Locator type (\ref loblocators)
                 * @param LobCCSID CCSID from \ref ccsids
                 */
                PRCED_Locator(int LocType, unsigned LobCCSID=0);
                /**
                 * @param Library Library with the SQL package
                 * @param Package SQL package
                 * @param Loc LOB Locator (see \ref GetLocator())
                 * @param LocType Locator type (\ref loblocators)
                 * @param LobCCSID CCSID from \ref ccsids
                 */
                PRCED_Locator(const char *Library,
                              const char *Package,
                              LobLocator_t Loc, 
                              int LocType,
                              unsigned LobCCSID=0);
                /**
                 * @param Loc LOB Locator (see \ref GetLocator())
                 * @param LocType Locator type (\ref loblocators)
                 * @param LobCCSID CCSID from \ref ccsids
                 */
                PRCED_Locator(LobLocator_t Loc, 
                              int LocType, 
                              unsigned LobCCSID=0);
                /**
                 * @param Library Library with the SQL package
                 * @param Package SQL package
                 * @param Variables
                 * @param Row Row index
                 * @param Col Column index
                 */
                PRCED_Locator(const char *Library,
                              const char *Package,
                              DAVariables &Variables, 
                              int Row, 
                              int Col);
                /**
                 * @param Variables Create LOB locator 
                 *        from \ref DAVariable in Variables[Row][Col]
                 * @param Row Row index
                 * @param Col Column index
                 */
                PRCED_Locator(DAVariables &Variables, int Row, int Col);
                /** Destructor */
                ~PRCED_Locator();
                /** see \ref Append(const char*, size_t) */
                bool Append(const u2char_t *Buffer, size_t nChars);
                /**
                 * Appends Buffer to LOB
                 * @param Buffer Buffer with data to append
                 * @param nChars Number of characters to append
                 * @return true if successful, otherwise false
                 * @throws ASError
                 * @throws ErrorClass
                 */
                bool Append(const char *Buffer, size_t nChars);
                /**
                 * Appends Buffer to LOB
                 * @param Buffer Buffer with data to append 
                 * @param nBytes Number of bytes to append
                 * @return true if successful, otherwise false
                 * @throws ASError
                 * @throws ErrorClass
                 */
                bool Append(const byte_t *Buffer, size_t nBytes);
                /** see \ref Assign(const char*, size_t) */
                bool Assign(const u2char_t *Buffer, size_t nChars);
                /**
                 * Assigns Buffer to LOB
                 * @param Buffer Buffer with new LOB data
                 * @param nChars Number of character to copy from Buffer
                 * @return true if successful, otherwise false
                 * @throws ASError
                 * @throws ErrorClass
                 */
                bool Assign(const char *Buffer, size_t nChars);
                /**
                 * Assigns Buffer to LOB
                 * @param Buffer Buffer with new LOB data
                 * @param nBytes Number of bytes to copy from Buffer
                 * @return true if successful, otherwise false
                 * @throws ASError
                 * @throws ErrorClass
                 */
                bool Assign(const byte_t *Buffer, size_t nBytes);
                /** @return CCSID of LOB from \ref ccsids */
                unsigned GetLobCCSID() {return LobCCSID;}
                /** @return \ref loblocators */
                int GetLobLocType();
                /** @return \ref lobs */
                int GetLobType();
                /** 
                 * @return Number of chars
                 * @throws ASError
                 * @throws ErrorClass
                 */
                size_t GetLength();
                /** @return Locator */
                LobLocator_t GetLocator() {return Locator;}
                /** 
                 * @return Number of bytes
                 * @throws ASError
                 * @throws ErrorClass
                 */
                size_t GetSize();
                /** @see GetSubstring(char*, size_t, size_t) */
                size_t GetSubString(u2char_t *Buffer, 
                                    size_t nStart, 
                                    size_t nChars);
                /**
                 * @param Buffer Buffer to write to
                 * @param nStart Where to start
                 * @param nChars Number of characters to fetch 
                 * @return Number of characters written to Buffer, 0 if failed
                 * @throws ASError
                 * @throws ErrorClass
                 */
                size_t GetSubString(char *Buffer, 
                                    size_t nStart, 
                                    size_t nChars);
                /**
                 * @param Buffer Buffer to write to
                 * @param nStart Where to start in LOB
                 * @param nBytes Number of bytes to fetch from LOB 
                 * @return Number of bytes written to Buffer, 0 if failed
                 * @throws ASError
                 * @throws ErrorClass
                 */
                size_t GetSubString(byte_t *Buffer, 
                                    size_t nStart, 
                                    size_t nBytes);
  #if !U2CHAR_IS_WCHAR
                /** see \ref Append(const char*, size_t) */
                bool Append(const wchar_t *Buffer, size_t nChars);
                /** see \ref Assign(const char*, size_t) */
                bool Assign(const wchar_t *Buffer, size_t nChars);
                /** see \ref GetSubString(char*, size_t, size_t) */
                size_t GetSubString(wchar_t *Buffer, 
                                    size_t nStart, 
                                    size_t nChars);
  #endif
                /** @param Auto if true, free locator automatically in destructor */
                void SetAutoFree(bool Auto) {AutoFree = Auto;}
        };

/** 
 * Checks if column contains a LOB locator
 *
 * @param Variables SQLDA to check
 * @param Col Column number
 * @return true if LOB locator
 */
bool isLobLocator(DAVariables &Variables, int Col);

// ***************************************************************************
// **** Utility: CleanupODPs *************************************************
// ***************************************************************************

/** Determines the meaning of the ODPCount parameter in \ref CleanupODPs() */
enum CloseMethod_t 
        {
        CM_NUMBER,/**< Number of ODPs to close */ 
        CM_THRESHOLD /**< Number of ODPs to leave open */
        };

/**
 * Close Open Data Paths (ODPs)
 *
 * @param CloseMethod see  \ref CloseMethod_t
 * @param ODPCount Number of ODPs to close or leave open
 * @param nRemaining Stores number of ODPs remaining
 * @param nClosed Stores number of ODPs closed
 * @return SQL code, 0 if success, positive if warning, 
 *         negative if error
 */
int CleanupODPs(CloseMethod_t CloseMethod,
                unsigned ODPCount,              
                unsigned *nRemaining,                   // Input / Output
                unsigned short *nClosed);               // Output

// ***************************************************************************
// **** Utility: UpdateCurrentLob ********************************************
// ***************************************************************************
/**
 * Updates the value of a LOB on the database
 *
 * @param PackageLib Library with SQL package
 * @param Package SQL package to prepare statement in
 * @param Table Table to update
 * @param ColumnName Column to update
 * @param Cursor Cursor on the row to update
 * @param Locator Locator for LOB that is updated
 * @return true if successful, otherwise false
 * @throws ASError
 * @throws ErrorClass
 */
bool UpdateCurrentLob(const char *PackageLib,
                      const char *Package,
                      const char *Table,
                      const char *ColumnName,
                      PRCED_Cursor &Cursor,
                      PRCED_Locator &Locator); 

/** Oprating Mode */
enum PRCEDpp_VIA_t   
        {
        PRCEDpp_VIA_QSQPRCED, /**< QSQPRCED natively */
        PRCEDpp_VIA_XDA,      /**< IBM XDA driver */
        PRCEDpp_VIA_XDN       /**< SAP XDN driver */
        };

/** @return current operating mode as defined in \ref PRCEDpp_VIA_t */
PRCEDpp_VIA_t GetPRCEDvia();

/* ****************************************************************************
 Notes:

             I. DAVariables

                        Sequence of operation:

                                Define DA: AddVariable()
                                        
                                        Define ALL variables first. 

                                        The lengths are in units of 
                                        the type.

                                Bind Columns: SetBuffer()
                                              SetBuffers()

                                Assign Data: SetVariable()

                                        Copies values into the buffers
                                        set with SetBuffer() / SetBuffers().

                                        This step is only necessary if the 
                                        buffers are initally empty.

                                **Execute/Fetch**

                                Read Data: GetVariableData()

                                        Returns the data from the buffers
                                        set with SetBuffer() / SetBuffers().

           III. Statements

            IV. Cursors

             V. CleanupODPs


*******************************************************************************
**** Examples: ****************************************************************
***************

*******************************************************************************

        Function: Test PRCEDpp C++ Classes for QSQPRCED Database Access.

*******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ebcdic.h>

#include <PRCEDpp.h>
#include <PRCEDPrint.h>

#define FILE "TWOLOB"                           // Max 10 Characters 

#define CURSOR "cursor000000000001"             // Max 18 Characters 

#define STATEMENT "\"IAPJICLAOEONAAAA\""        // Max 18 Characters 

#define PACKAGE "AS4EXTRA"                      // Max 10 Characters 
//#define PACKAGE FILE                            // Max 10 Characters 
//#define LIBRARY "R3MOEDATA "                    // Max 10 Characters 
//#define LIBRARY "R3MOEX0000"                    // Max 10 Characters 
#define LIBRARY "ERIC"                          // Max 10 Characters 

// ************************************************************************ 
// **** Utilities ********************************************************* 
// ************************************************************************ 

#if defined(_WIN32) || defined(__OS400_TGTVRM__)
        typedef wchar_t u2char_t;
#else
        typedef short u2char_t;
#endif

// ************************************************************************ 
// **** Main ************************************************************** 
// ************************************************************************ 

main(int argc, char *argv[])
        {
        SQLCtrlBlock CB;
        char Buffer[4096] = {0};
        short Indicators[1024] = {0};

        char *StatementText = "SELECT NAMELEN, NAME, ADDRESS, TELLEN, "
                              "TELEPHONE, AGE FROM R3MOEDATA/TWOLOB "
                              "WHERE AGE = ?";
        try     {
                // ============================== 
                // ====== Create Package ======== 
                // ============================== 

                PRCED_Statement Statement(CB,
                                          STATEMENT,
                                          LIBRARY, PACKAGE);

                int rc = Statement.CreatePackage();

                // ===================== 
                // ====== Prepare ====== 
                // ===================== 

                rc = Statement.Prepare(StatementText);

                // ======================
                // ====== Describe ======
                // ======================

                DAVariables DescVars;
                rc = rc || Statement.Describe(DescVars);
                if (rc == 0)
                        {
                        int nCols = DescVars.GetColumnCount();
                        for (int Col=0; Col < nCols; Col++)
                                {
                                PrintColumn(DescVars,Col);
                                }
                        }

                // ================== 
                // ====== Open ====== 
                // ================== 
        
                int BlockedRows = 2;
                PRCED_Cursor Cursor(Statement,CURSOR);

                int Age = 42;
                char AgeBuffer[64];
                short AgeIndBuffer[1] = {0};
                DAVariables InVars;
                InVars.AddVariable(DA_INT_TYPE,"AGE",0);
                InVars.SetBuffers(AgeBuffer,AgeIndBuffer,NULL);
                InVars.SetVariable(0,0,&Age,sizeof(Age));

                rc = rc || Cursor.Open(InVars,BlockedRows);

                // =================== 
                // ====== Fetch ====== 
                // =================== 
  
                DAVariables OutVars;
                OutVars.AddVariable(DA_INT_TYPE,"NAMELEN",0);
                OutVars.AddVariable(DA_DBCLOB_TYPE,"NAME",300);
                OutVars.AddVariable(DA_WCHAR_TYPE,"ADDRESS",30);
                OutVars.AddVariable(DA_INT_TYPE,"TELLEN",0);
                OutVars.AddVariable(DA_DBCLOB_TYPE,"TELEPHONE",300);
                OutVars.AddVariable(DA_INT_TYPE,"AGE",0);
                OutVars.SetBuffers(Buffer,Indicators,NULL);

                while (rc == 0)
                        {
                        rc = rc || Cursor.Fetch(OutVars);
                        int nCols = OutVars.GetColumnCount();
                        int nProcessed = Cursor.GetRowsProcessed();
                        for (int Row=0; Row < nProcessed; Row++)
                                {
                                for (int Col=0; Col < nCols; Col++)
                                        {
                                        PrintVariable(OutVars,Row,Col);
                                        }
                                }
                        }

                Cursor.Close();
                }
        catch (ASError &Error)
                {
                char *ErrorText = Error.GetText();
                printf("ERROR: %s: %s\n",ErrorText,Error.GetExceptionID());
                }
        catch (ErrorClass &Error)
                {
                char *ErrorText = Error.GetText();
                printf("ERROR: %s\n",ErrorText);
                }

        return 0;
        }

*******************************************************************************
*********************************** End of File *******************************
**************************************************************************** */

#endif
