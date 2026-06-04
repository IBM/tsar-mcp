///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: QueryTool.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012
// All rights reserved. 
//                    
// Author(s) : Eric Kass, Robert Waury
//                                                                             
///////////////////////////////////////////////////////////////////////////////
// XDA DB QueryTool
//
//                                      Copyright (c) 2008 Eric Kass
//                                      All rights reserved.
//
//
// OS/400 Compile:
//
//      CRTCPPMOD MODULE(ERIC/QUERYTOOL)
//      TERASPACE(*YES *TSIFC) DBGVIEW(*ALL)
//      SRCSTMF('/home/Eric/Projects/EDRSTest/QueryTool.cpp')
//      INCDIR('/home/Eric/Projects/CommonC/include')
//
//      [XDA/XDN/QSQPRCED: Modify COMMONLIB.CLP to select...]
//
//      CRTPGM PGM(ERIC/QUERYTOOL) 
//      BNDDIR(ERIC/COMMONLIB ERIC/PRCEDLIB) 
//      BNDSRVPGM(*LIBL/XDNXDAAPI)              <<== Only For XDN
//
// PASE Compile:
//
//      xlC QueryTool.cpp -c -q64 -qldbl128 -qlonglong -qalign=natural
//                       -I /sapmnt/ashost/home/Eric/Projects/CommonC/include
//                       -I /sapmnt/depot/apps/IBM/PASE/v6r1m0/Include 
//                       -I /sapmnt/depot/apps/IBM/DB4/v6r1m0/Include
//
//      [XDA/XDN: Fill ../../Network/XDN and run CommonLibAIX.mak]
//
//      xlC -o QueryTool -q64 -lpthread QueryTool.o 
//          /sapmnt/ashost/home/Eric/Projects/CommonC/lib/CommonLibPASE.a 
//          /sapmnt/ashost/home/Eric/Projects/CommonC/lib/PRCEDLib.a
//          -bI:/sapmnt/depot/apps/IBM/PASE/v6r1m0/Lib/as400_libc.exp
//          -bI:/sapmnt/depot/apps/sap/as400/DBSLDirectDrive/Lib/XDNXDAAPI.exp
//
//      setenv LIBPATH /bas/720_REL/gen/opt/as400_pase_64
//
// AIX Compile:
//
//      Same as PASE Compile, except:
//
//              1. Link CommonLibAIX.a
//              2. setenv LIBPATH /bas/720_REL/gen/opt/rs6000_64
//

#ifdef  __OS400_TGTVRM__
        #pragma convert(500)
#endif

#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32) && !defined(__OS400_TGTVRM__)
        #ifndef stricmp
                #define stricmp strcasecmp
        #endif
        #ifndef strnicmp
                #define strnicmp strncasecmp
        #endif
#endif

#ifdef _WIN32
        #include <io.h>
#else
        #include <unistd.h>
#endif

#include <ebcdic.h>
#include <LevelTrace.h>
#include <HexDump.h>
#include <MainArguments.h>
#include <LoadLib.h>

#ifdef __OS400_TGTVRM__
        #include <iTerminal.h>
        #define iTerminalTITLE "QueryTool"
#endif

#include <QxdaUTIL.h>
#include <PRCEDpp.h>
#include <PRCEDPrint.h>

#include <SimpleSqlParser.h>
#include <util.h>
#include <SelectBuilder.h>

#define QUERYTOOL_VERSION "2.21"

#define TOTAL_FETCH_ROWS 40
#define BLOCK_FETCH_ROWS 20

#define LOB_READ_MAX 65536
#define LOB_PRINT_MAX 512
#define LOB_READ_BUFFER_SIZE 32768

#define USE_DEFER_OPEN 1
// TODO: change to QTEMP
#define PACKAGELIB_DEFAULT "QUERYTOOL"          /* Max 10 Characters */
#ifdef __OS400_TGTVRM__
        #define PACKAGE_DEFAULT "QUERYTOOL"     /* Max 10 Characters */
#else
        #define PACKAGE_DEFAULT "WQUERYTOOL"    /* Max 10 Characters */
#endif

#define StatementNumberFINDSTMT (-1)

static char PACKAGELIB[10+1] = PACKAGELIB_DEFAULT;
static char PACKAGE[10+1] = PACKAGE_DEFAULT;
static int XDAHandle = 0;         

// ***************************************************************************
// **** Utilities ************************************************************
// ***************************************************************************

static char* rtrim(char *String)                // Modifies String!
        {
        if (!String) return NULL;
        char *pos = String + strlen(String);
        while (pos != String && isspace(*(pos-1))) pos--;
        *pos = '\0';
        return String;
        }

static void memcpyUPPER(char *Dest, const char *Source, size_t Length)
        {
        while (Length--)
                {
                *Dest = toupper(*Source);
                Source++;
                Dest++;
                }
        return;
        }

static const char* rtrimQuotes(const char *String, size_t *Length)
        {
        if (*Length && String[0] == '"')
                {
                (*Length)--;
                String++;
                }
        while (*Length)
                {
                if (String[*Length-1] != ' ' && String[*Length-1] != '"')
                        {
                        break;
                        }
                (*Length)--;
                }
        return String;
        }

static size_t wordlen(const char *String)
        {
        size_t Length = 0;
        while (*String && !isspace(*String))
                {
                Length++;
                String++;
                }
        return Length;
        }

static bool isKeyword(const char *Text, const char *Keyword)
        {
        while (isspace(*Text)) Text++;
        size_t TextLength = wordlen(Text);
        size_t KeywordLength = strlen(Keyword);
        if (TextLength < KeywordLength) return false;
        return strnicmp(Text,Keyword,KeywordLength) == 0;
        }

// ***************************************************************************
// **** LevelTrace Redirect **************************************************
// ***************************************************************************

// *****************************************
// **** LevelTraceFileTrace (QueryTool) ****
// *****************************************

static int LevelTraceFileTrace(void *UserPtr,
                               unsigned Level, 
                               const char *Format, 
                               va_list Args)
        {
        if (Level > GetMaxTraceLevel()) return 0;
        FILE *hFile = (FILE *)UserPtr;
        if (!hFile) return 0;
        if (Level == TRACELEVEL_ERROR) fprintf(hFile,"ERROR: ");
        else if (Level == TRACELEVEL_INFO) fprintf(hFile,"INFO:  ");
        else if (Level == TRACELEVEL_DEBUG) fprintf(hFile,"DEBUG: ");
        vfprintf(hFile,Format,Args);
        fprintf(hFile,"\n");
        return 1;
        }

#ifdef __OS400_TGTVRM__

// ******************************************
// **** SetXDNTraceSTDOUT (XDN on OS400) ****
// ******************************************

enum QxdnTraceLocation_t                        // From XDNXDAAPI.h
        {
        QTL_Default,
        QTL_Stream,
        QTL_File,
        QTL_Callback
        };

typedef void (*QxdnSetTraceLocation_FNt)
                (
                QxdnTraceLocation_t Location,
                void * Param0,
                void * Param1
                );

static bool SetXDNTraceFILE(FILE *hFile)
        {
        static const char *ProcName = "SetXDNTraceFILE";
        bool Status  = true;
        if (GetPRCEDvia() != PRCEDpp_VIA_XDN)
                {
                return true;
                }
        LIBHANDLE hLib = LoadLib("*LIBL/XDNXDAAPI");
        if (!hLib)
                {
                TERROR(("%s: LoadLib() Failed (%d)",ProcName,errno));
                return false;
                }
        QxdnSetTraceLocation_FNt QxdnSetTraceLocation_FN;
        QxdnSetTraceLocation_FN = (QxdnSetTraceLocation_FNt)
                                  LoadSymbol(hLib,"QxdnSetTraceLocation");
        if (!QxdnSetTraceLocation_FN)
                {
                TERROR(("%s: LoadSymbol() Failed (%d)",ProcName,errno));
                Status = false;
                }
        else    {
                if (hFile)
                        {
                        QxdnSetTraceLocation_FN(QTL_Stream,hFile,NULL);
                        }
                else    {
                        QxdnSetTraceLocation_FN(QTL_Default,NULL,NULL);
                        }
                }
        CloseLib(&hLib);
        return Status;
        }
     
#else /* !__OS400_TGTVRM__ */

static bool SetXDNTraceFILE(FILE *)
        {
        return true;
        }

#endif /* !__OS400_TGTVRM__ */

// ***************************************************************************
// **** RunSQLQuery **********************************************************
// ***************************************************************************

void FreeLOC(DAVariables &Variables, int Row, int Col)
        {
        PRCED_Locator Locator(Variables,Row,Col);
        Locator.SetAutoFree(true);
        return;
        }

size_t ProcessLOB(DAVariables &Variables, int Row, int Col, size_t MaxLen)
        {
        PRCED_Locator Locator(Variables,Row,Col);
        size_t LobLength = Locator.GetLength();
        if (!LobLength) return 0;
        size_t ReadPos = 0;
        size_t BufferSize = LOB_READ_BUFFER_SIZE * 2;
        char *LobBuffer = (char *)malloc(BufferSize);   // CHAR or GRAPHIC.
        if (!LobBuffer)
                {
                TERROR(("ProcessLOB: Alloc(%u) Failed",BufferSize));
                return 0;
                }
        size_t Remaining = LobLength > MaxLen ? MaxLen : LobLength;
        while (Remaining)
                {         
                size_t ReadLen = LOB_READ_BUFFER_SIZE;
                if (ReadLen > Remaining) ReadLen = Remaining;
                Locator.GetSubString(LobBuffer,ReadPos,ReadLen);
                ReadPos += ReadLen;
                Remaining -= ReadLen;
                }
        free(LobBuffer);
        return LobLength > MaxLen ? MaxLen : LobLength;
        }

void ProcessData(DAVariables &OutVars, unsigned nRows, bool PrintOutput)
        {
        static const char *ProcName = "ProcessData";
        size_t LobLength = 0;
        int nCols = OutVars.GetColumnCount();
        if (PrintOutput) printf("------\n");
        for (unsigned Row=0; Row < nRows; Row++)
                {
                for (int Col=0; Col < nCols; Col++)
                        {
                        if (PrintOutput)
                               {
                               PrintVariable(OutVars,Row,Col);
                               }
                        if (isLobLocator(OutVars,Col) &&
                            OutVars.GetIndicator(Row,Col) != -1)
                                {
                                if (PrintOutput)
                                        {
                                        LobLength += PrintLOB(OutVars,
                                                              Row,
                                                              Col,
                                                              LOB_PRINT_MAX);
                                        }
                                else    {
                                        LobLength += ProcessLOB(OutVars,
                                                                Row,
                                                                Col,
                                                                LOB_READ_MAX);
                                        }
                                FreeLOC(OutVars,Row,Col);
                                }
                        }
                if (PrintOutput) printf("------\n");
                }
        if (LobLength) TINFO(("%s: Lob Data Read: %u\n",ProcName,LobLength));
        return;
        }

bool RunSQLQuery(const char *StatementText, 
                 int StatementNumber,
                 unsigned nMaxRows,
                 unsigned nBlockedRows,
                 bool Reopen,
                 bool PrintOutput)
        {
        static const char *ProcName = "RunSQLQuery";
        SQLCtrlBlock CB;
        bool Status = true;
        const char *Library = PACKAGELIB;
        const char *Package = PACKAGE;
        unsigned StatementCount = 0;
        char StatementName[MAX_STMT_NAME_LENGTH+1];
        if (StatementNumber > 0)
                {
                sprintf(StatementName,"STMT%u",StatementNumber);
                }
        else if (StatementNumber == StatementNumberFINDSTMT)
                {
                StatementName[0] = '\0'; // Let XDA/XDN find the stmt.
                }
        else    {
                StatementCount = time(NULL) %  86400;
                sprintf(StatementName,"STMT%u",StatementCount);
                }
        try     {
                int rc = 0;
                DAVariables InVars;
                DAVariables OutVars;
                PRCED_Statement Statement(StatementName,
                                          Library,
                                          Package);
                /* ===================== */
                /* ====== Prepare ====== */
                /* ===================== */
                if (StatementNumber > 0)        // Known statement: Try Open
                        {
                        rc = Statement.Describe(OutVars);
                        if (rc) PrintSQLCodeText(rc);
                        }
                if (StatementNumber <= 0 || rc) // Unknown: Prepare-Open
                        {
                        if (GetPRCEDvia() == PRCEDpp_VIA_XDA)
                                {
                                rc = Statement.Prepare(StatementText);
                                if (rc == 0) rc = Statement.Describe(OutVars);
                                }
                        else // QSQPRCED or XDN Server Version > 4.29:
                                {
                                rc = Statement.PrepareDescribe(StatementText,
                                                               OutVars); 
                                }
                        if (rc) 
                                {
                                PrintSQLCodeText(rc);
                                return false;
                                }
                        }
                if (nBlockedRows > nMaxRows) 
                        {
                        nBlockedRows = nMaxRows;
                        }
                else if (nBlockedRows && nMaxRows % nBlockedRows)
                        {
                        TINFO(("%s: NOTICE: nBlockRows (%u) is not an integral "
                               "factor of",
                               ProcName,
                               nBlockedRows));
                        TINFO(("%s:         nMaxRows (%u) use: "
                               "\"FETCH FIRST %u ROWS ONLY\".",
                               ProcName,
                               nMaxRows,
                               nMaxRows));
                        }
                /* ================== */
                /* ====== Open ====== */
                /* ================== */
                char CursorName[18+1];
                if (StatementNumber > 0)
                        {
                        sprintf(CursorName,"CURSOR%u",StatementNumber);
                        }
                else if (StatementNumber == StatementNumberFINDSTMT)
                        {
                        // -------------------------------------------------
                        // ---- Use cursor name that matches statement -----
                        // ---- name : only important if Reopen is used ----
                        // -------------------------------------------------
                        size_t MinLength = 0;
                        const char* FoundName;
                        FoundName = Statement.GetStatementName(&MinLength);
                        FoundName = rtrimQuotes(FoundName,&MinLength);
                        size_t MaxLength = sizeof(CursorName) - 3;
                        if (MinLength > MaxLength) MinLength = MaxLength;
                        sprintf(CursorName,"C0%.*s",MinLength,FoundName);
                        }
                else    {
                        sprintf(CursorName,"CURSOR%u",StatementCount);
                        Reopen = false; // Caller not in control of the
                        }               // cursor so close it when done.
                PRCED_Cursor Cursor(Statement,CursorName);
                Cursor.SetDeferOpen(USE_DEFER_OPEN);
                rc = Cursor.Open(InVars,nBlockedRows,Reopen);
                if (rc) 
                        {
                        PrintSQLCodeText(rc);
                        return false;
                        }
                /* ===================================== */
                /* ====== Lob Columns to Locators ====== */
                /* ===================================== */
                OutVars.ConvertLobs2Locators();
                /* =================== */
                /* ====== Fetch ====== */
                /* =================== */
                int TotalProcessed = 0;
                int nRows = nBlockedRows ? nBlockedRows : 1;
                int nVars = OutVars.GetColumnCount();
                int nCells = nVars * nRows;
                size_t BufferSize = OutVars.GetRecordSize() * nRows;
                char *Buffer = (char *)malloc(BufferSize);
                short *Indicators = (short *)malloc(nCells * sizeof(short));
                unsigned *LobLens = (unsigned *)malloc(nCells * sizeof(unsigned));
                if (!Buffer)
                        {
                        TERROR(("%s: Can't Allocate Buffer",ProcName));
                        return false;
                        }
                if (GetPRCEDvia() != PRCEDpp_VIA_QSQPRCED)
                        {
                        OutVars.SetDirectMap(false);    // Test Server Cache.
                        }
                //OutVars.SetBuffers(Buffer,Indicators);
                OutVars.SetBuffers(Buffer,Indicators,LobLens,nRows);
                while (rc == 0 && nMaxRows && !Cursor.AreAllRowsRead())
                        {
                        rc = Cursor.Fetch(OutVars);
                        if (rc > 0) PrintSQLCodeText(rc);
                        else if (rc)
                              {
                              PrintSQLCodeText(rc);
                              TERROR(("%s: Fetch Failed",ProcName));
                              Cursor.Close(true);
                              return false;
                              }
                        int nProcessed = Cursor.GetRowsProcessed();
                        ProcessData(OutVars,nProcessed,PrintOutput);
                        TotalProcessed += nProcessed;
                        nMaxRows -= nProcessed;
                        }
                TINFO(("%s: Total Rows Read: %d",ProcName,TotalProcessed));
                if (!Reopen) Cursor.Close(); // else "soft" (no) close.
                free(LobLens);
                free(Indicators);
                free(Buffer);
                }
        catch (ASError &Error)
                {
                const char *ErrorText = Error.GetText();
                TERROR(("%s: %.7s",ErrorText,Error.GetExceptionID()));
                Status = false;
                }
        catch (ErrorClass &Error)
                {
                const char *ErrorText = Error.GetText();
                TERROR(("%s",ErrorText));
                Status = false;
                }
        return Status;
        }

// ***************************************************************************
// **** PrepareExecute *******************************************************
// ***************************************************************************

bool PrepareExecute(const char *StatementText, 
                    int StatementNumber,
                    DAVariables &DA,
                    unsigned nRows)
        {
        static const char *ProcName = "XDNClient:PrepareExecute";
        const char *Library = PACKAGELIB;
        const char *Package = PACKAGE;
        char StatementName[MAX_STMT_NAME_LENGTH+1];
        if (StatementNumber > 0)
                {
                sprintf(StatementName,"STMT%u",StatementNumber);
                }
        else if (StatementNumber == StatementNumberFINDSTMT)
                {
                StatementName[0] = '\0'; // Let XDA/XDN find the stmt.
                }
        else    {
                unsigned StatementCount = time(NULL) %  86400;
                sprintf(StatementName,"STMT%u",StatementCount);
                }
        PRCED_Statement Statement(StatementName,Library,Package);
        try     {
                /* ===================== */
                /* ====== Prepare ====== */
                /* ===================== */
                int rc = Statement.Prepare(StatementText);
                if (rc)
                      {
                      PrintSQLCodeText(rc);
                      TERROR(("%s: Prepare Failed",ProcName));
                      return false;
                      }
                }
        catch (ASError &Error)
                {
                TERROR(("%s: %s: %.7s",ProcName,
                                       Error.GetText(),
                                       Error.GetExceptionID()));
                }
        try     {
                /* ===================== */
                /* ====== Execute ====== */
                /* ===================== */
                int rc = Statement.Execute(DA,nRows);
                if (rc > 0) 
                        {
                        PrintSQLCodeText(rc);
                        TINFO(("%s: PrepareExecute(\"%.*s\") w/Warnings",
                               ProcName,
                               wordlen(StatementText),
                               StatementText));
                        }
                else if (rc < 0)
                        {
                        PrintSQLCodeText(rc);
                        TERROR(("%s: PrepareExecute(\"%.*s\") Failed",
                                ProcName,
                                wordlen(StatementText),
                                StatementText));
                        return false;
                        }
                else    {
                        TINFO(("%s: PrepareExecute(\"%.*s\") Success",
                               ProcName,
                               wordlen(StatementText),
                               StatementText));
                        }
                }
        catch (ASError &Error)
                {
                TERROR(("%s: %s: %.7s",ProcName,
                                       Error.GetText(),
                                       Error.GetExceptionID()));
                return false;
                }
        return true;
        }

// ***************************************************************************
// **** SQL Utilities ********************************************************
// ***************************************************************************

bool CreatePackage()
        {
        static const char *ProcName = "CreatePackage";
        PRCED_Statement Statement("",PACKAGELIB,PACKAGE);
        int rc = Statement.CreatePackage();
        if (rc == -601)
                {
                TINFO(("%s: %s/%s Already Exists",ProcName,
                                                  PACKAGELIB,
                                                  PACKAGE));
                rc = 0;
                }
        else if (rc == 0) 
                {
                TINFO(("%s: Package %s/%s Created",ProcName,
                                                   PACKAGELIB,
                                                   PACKAGE));
                }
        return rc == 0;
        }

bool DeletePackage()
        {
        static const char *ProcName = "DeletePackage";
        PRCED_Statement Statement("",PACKAGELIB,PACKAGE);
        int rc = Statement.DeletePackage();
        if (rc == 0) 
                {
                TINFO(("%s: Package %s/%s Deleted",ProcName,
                                                   PACKAGELIB,
                                                   PACKAGE));
                }
        return rc == 0;
        }

bool SetQAQQINI(const char *Library)
        {
        static const char *ProcName = "SetQAQQINI";
        bool Status;
        char CHGQRYA[256];
        sprintf(CHGQRYA,"CHGQRYA JOB(*) QRYOPTLIB(%s)",Library); 
        if (GetPRCEDvia() == PRCEDpp_VIA_QSQPRCED)
                {
                int rc = system(CHGQRYA);
                Status = rc ==0;
                }
        else    {
                Status = ProcessCommand(XDAHandle,CHGQRYA);
                }
        if (!Status)
                {
                TERROR(("%s: %s - Failed",ProcName,CHGQRYA));
                }
        else    {
                TINFO(("%s: %s - Success",ProcName,CHGQRYA));
                }
        return Status;
        }

int FindStatementID(const char *StatementText)  // Native QSQPRCED only.
        {
        static const char *ProcName = "FindStatementID";
        SQLCtrlBlock CB;
        bool Status = true;
        const char *Library = PACKAGELIB;
        const char *Package = PACKAGE;
        PRCED_Statement Statement("",PACKAGELIB,PACKAGE);
        try     {
                bool rc = Statement.FindStatementName(StatementText);
                if (!rc) 
                        {
                        TDEBUG(("%s: FindStatementName() - None",ProcName));
                        return 0;
                        }
                }
        catch (ASError &Error)
                {
                const char *ErrorText = Error.GetText();
                TERROR(("%s: %.7s",ErrorText,Error.GetExceptionID()));
                Status = false;
                }
        catch (ErrorClass &Error)
                {
                const char *ErrorText = Error.GetText();
                TERROR(("%s",ErrorText));
                Status = false;
                }
        int StatementNumber = 0;
        if (Status)
                {
                size_t Length = 0;
                const char* StmtName = Statement.GetStatementName(&Length);
                int rc = sscanf(StmtName,"STMT%8d",&StatementNumber);
                if (rc != 1)
                        {
                        TERROR(("%s: Unable to determine Statement Number",
                                ProcName));
                        }
                else    {
                        TINFO(("%s: Found Statement: STMT%d",
                               ProcName,
                               StatementNumber));
                        }
                }
        return StatementNumber;
        }

// ***************************************************************************
// **** Statement-Specific Functions *****************************************
// ***************************************************************************

bool FindStatementName(char* Stmt)
        {
        PRCED_Statement Statement("", PACKAGELIB, PACKAGE);
        return Statement.FindStatementName(Stmt);
        }

bool PerformInsert(const char* SQLText)
        {
        const char* ProcName = "PerformInsert";
        
        char StatementName[MAX_STMT_NAME_LENGTH+1];
        GetUniqueStatementName(StatementName);

        SimpleSqlParser parser(SQLText, INSERT);
        if(parser.Failed())
                {
                TERROR(("%s: Parsing SQL Statement failed.", ProcName));
                return false;
                }

        SelectBuilder Builder(parser.getTableString());
        if(Builder.Failed())
                {
                TERROR(("%s: Building SELECT Statement failed.", ProcName));
                return false;
                }

        short* Indicators = (short*) calloc(parser.getRowCount()*parser.getColumnCount(), sizeof(short));
        char* Buffer = NULL;

        int rc = 0;
        SQLCtrlBlock CB;

        bool stmtExists = false;
        // XDA/XDN handle caching
        if(GetPRCEDvia() != PRCEDpp_VIA_QSQPRCED)
                {
                StatementName[0] = '\0';
                }
        else
                {
                stmtExists = FindStatementName(parser.getGenericStatement());
                if(stmtExists) StatementName[0] = '\0';
                }

        try
                {
                PRCED_Statement stmt(StatementName, PACKAGELIB, PACKAGE);

                if(!stmtExists)
                        {
                        TINFO(("%s: Statement %s not found in SQL Package %s.",ProcName,parser.getGenericStatement(),PACKAGE));
                        TINFO(("%s: Creating Statement %s in SQL Package %s.",ProcName,parser.getGenericStatement(),PACKAGE));
                        }
                if(stmtExists)
                        {
                        if(stmt.FindStatementName(parser.getGenericStatement()))
                                {
                                size_t size = MAX_STMT_NAME_LENGTH;
                                TINFO(("%s: Statement %s found in SQL Package %s.",ProcName,stmt.GetStatementName(&size),PACKAGE));
                                }
                        }           
                rc = stmt.CreatePackage();
                if (rc == -601)
                        {
                        TINFO(("%s: %s/%s Already Exists",ProcName,PACKAGELIB,PACKAGE));
                        rc = 0;
                        }
                else if (rc == 0) 
                        {
                        TINFO(("%s: Package %s/%s Created",ProcName,PACKAGELIB,PACKAGE));
                        }                
                if(!stmtExists)
                        {
                        rc = stmt.Prepare(parser.getGenericStatement());
                        if(rc == 0)
                                {
                                TINFO(("%s: Successfully prepared statement %s in %s/%s.",ProcName,
                                                                                          parser.getGenericStatement(),
                                                                                          PACKAGELIB,
                                                                                          PACKAGE)); 
                                }
                        else
                                {
                                TERROR(("%s: Could not prepare statement %s in %s/%s.",ProcName,
                                                                                       parser.getGenericStatement(),
                                                                                       PACKAGELIB,
                                                                                       PACKAGE));
                                TERROR(("%s: SQL Error %d.",ProcName,rc));
                                return false;
                                }
                        }

                // block insert
                DAVariables Input;
                for(int i = 0; i < parser.getColumnCount(); i++)
                        {
                        switch(Builder.getColumnTypes()[i])
                                {
                                case DA_VARCHAR_TYPE:
				case DA_TIMESTAMP_TYPE:
				case DA_CHAR_TYPE:
				case DA_CLOB_TYPE:
				case DA_XML_TYPE:
			                Input.AddVariable(Builder.getColumnTypes()[i], NULL, parser.getMaxSizes()[i]);
                                        break;
                                case DA_INT_TYPE:
				case DA_SHORT_TYPE:
                                        Input.AddVariable(Builder.getColumnTypes()[i], NULL, sizeof(int));
                                        break;
                                case DA_FLOAT_TYPE:
                                        Input.AddVariable(Builder.getColumnTypes()[i], NULL, sizeof(double));
                                        break;
                                default:
					TERROR(("%s: Encountered Unsupported Type %d",ProcName,Builder.getColumnTypes()[i]));
					return false;
                                }
                        }
                
                Buffer = (char*) calloc(parser.getRowCount(), Input.GetRecordSize());
                if(Buffer == NULL)
                        {
                        TERROR(("%s: calloc(%d, %d) Failed",ProcName,
                                                            parser.getRowCount(),
                                                            Input.GetRecordSize()));
                        }

                // TODO: LOB allocate
                unsigned* LobLens;
                LobLens = NULL;

                Input.SetBuffers(Buffer, Indicators, LobLens, parser.getRowCount());

                for(int row = 0; row < parser.getRowCount(); row++)
                        {
                        for(int col = 0; col < parser.getColumnCount(); col++)
                                {
				switch(Builder.getColumnTypes()[col])
					{
					case DA_VARCHAR_TYPE:
					case DA_TIMESTAMP_TYPE:
					case DA_CHAR_TYPE:
					case DA_CLOB_TYPE:
					case DA_XML_TYPE:
						Input.SetVariable(row, col, parser.getValues()[row][col]);
					        break;
					case DA_INT_TYPE:
					case DA_SHORT_TYPE:
						Input.SetVariable(row, col, atoi(parser.getValues()[row][col]));
						break;
					case DA_FLOAT_TYPE:
						Input.SetVariable(row, col, atof(parser.getValues()[row][col]));
                                                break;
					default:
						TERROR(("%s: Encountered Unsupported Type %d",ProcName,Builder.getColumnTypes()[col]));
						return false;
					} 
                                }
                        }

                rc = stmt.Execute(Input, parser.getRowCount());
                if(rc == 0)
                        {
                        TINFO(("%s : Statement successfully executed.", ProcName));
                        }
                else
                        {
                        TERROR(("%s: Error executing Statement %s.", ProcName, parser.getGenericStatement()));
                        TERROR(("%s: SQL Error %d.",ProcName, rc));
                        return false;
                        }
                } 
        catch(...)
                {
                TERROR(("%s : Exception caught", ProcName));
                return false;
                }
        return true;
        }

bool PerformMerge(const char* SQLText)
        {
        const char* ProcName = "PerformMerge";
        
        char StatementName[MAX_STMT_NAME_LENGTH+1];
        GetUniqueStatementName(StatementName);

        SimpleSqlParser parser(SQLText, MERGE);
        if(parser.Failed())
                {
                TERROR(("%s: Parsing SQL Statement failed.", ProcName));
                return false;
                }

        short* Indicators = (short*) calloc(parser.getRowCount()*parser.getColumnCount(), sizeof(short));
        char* Buffer = NULL;

        int rc = 0;
        SQLCtrlBlock CB;

        bool stmtExists = false;
        // XDA/XDN handle caching
        if(GetPRCEDvia() != PRCEDpp_VIA_QSQPRCED)
                {
                StatementName[0] = '\0';
                }
        else
                {
                stmtExists = FindStatementName(parser.getGenericStatement());
                if(stmtExists) StatementName[0] = '\0';
                }
        try
                {
                PRCED_Statement stmt(StatementName, PACKAGELIB, PACKAGE);

                if(!stmtExists)
                        {
                        TINFO(("%s: Statement %s not found in SQL Package %s.",ProcName,parser.getGenericStatement(),PACKAGE));
                        TINFO(("%s: Creating Statement %s in SQL Package %s.",ProcName,parser.getGenericStatement(),PACKAGE));
                        }
                if(stmtExists)
                        {
                        if(stmt.FindStatementName(parser.getGenericStatement()))
                                {
                                size_t size = MAX_STMT_NAME_LENGTH;
                                TINFO(("%s: Statement %s found in SQL Package %s.",ProcName,stmt.GetStatementName(&size),PACKAGE));
                                }
                        }
                rc = stmt.CreatePackage();
                if (rc == -601)
                        {
                        TINFO(("%s: %s/%s Already Exists",ProcName,PACKAGELIB,PACKAGE));
                        rc = 0;
                        }
                else if (rc == 0) 
                        {
                        TINFO(("%s: Package %s/%s Created",ProcName,PACKAGELIB,PACKAGE));
                        }
                if(!stmtExists)
                        {
                        rc = stmt.Prepare(parser.getGenericStatement());
                        if(rc == 0)
                                {
                                TINFO(("%s: Successfully prepared statement %s in %s/%s.",ProcName,
                                                                                          parser.getGenericStatement(),
                                                                                          PACKAGELIB,
                                                                                          PACKAGE)); 
                                }
                        else
                                {
                                TERROR(("%s: Could not prepare statement %s in %s/%s.",ProcName,
                                                                                       parser.getGenericStatement(),
                                                                                       PACKAGELIB,
                                                                                       PACKAGE));
                                TERROR(("%s: SQL Error %d.",ProcName,rc));
                                return false;
                                }
                        }

                // block insert
                DAVariables Input;
                for(int i = 0; i < parser.getColumnCount(); i++)
                        {
                        Input.AddVariable(DA_VARCHAR_TYPE, NULL, parser.getMaxSizes()[i]);
                        }
                
                Buffer = (char*) calloc(parser.getRowCount(), Input.GetRecordSize());
                if(Buffer == NULL)
                        {
                        TERROR(("%s: calloc(%d, %d) Failed",ProcName,
                                                            parser.getRowCount(),
                                                            Input.GetRecordSize()));
                        }

                // TODO: LOB allocate
                unsigned* LobLens;
                LobLens = NULL;

                Input.SetBuffers(Buffer, Indicators, LobLens, parser.getRowCount());

                for(int row = 0; row < parser.getRowCount(); row++)
                        {
                        for(int col = 0; col < parser.getColumnCount(); col++)
                                {
                                Input.SetVariable(row, col, parser.getValues()[row][col]); 
                                }
                        }
                rc = stmt.Execute(Input, parser.getRowCount());
                if(rc == 0)
                        {
                        TINFO(("%s : Statement successfully executed.", ProcName));
                        }
                else
                        {
                        TERROR(("%s: Error executing Statement %s.", ProcName, parser.getGenericStatement()));
                        TERROR(("%s: SQL Error %d.",ProcName, rc));
                        return false;
                        }
                }
        catch(...)
                {
                TERROR(("%s : Exception caught", ProcName));
                return false;
                }
        return true;
        }

// ***************************************************************************
// **** Main *****************************************************************
// ***************************************************************************

static const char* ShortName(const char *Program)
        {
        const char *Pos = Program + strlen(Program);
        while (Pos != Program && *Pos != '\\') Pos--;
        if (*Pos == '\\') Pos++;
        return Pos;
        }

static void Help(const char *Program)
        {
        Program = ShortName(Program);
        printf("\n");
        printf("SQL QueryTool via PRCED-XDA-XDN:\n\n");
        if (GetPRCEDvia() != PRCEDpp_VIA_QSQPRCED)
                {
                printf("%s [Options] Host[:Port] User Password\n\n",Program);
                }
        else    {
                printf("%s [Options]\n\n",Program);
                }
        printf("Options:\n\n");
        printf("     -rowmax <n>    - Maximum number of rows "
                                     "(Default: %u).\n",TOTAL_FETCH_ROWS);
        printf("     -blocking <n>  - Number of blocked rows "
                                     "(Default: %u).\n\n",BLOCK_FETCH_ROWS);
        printf("     -findstmt-     - Don't search packages for SQL");
        printf(                     " and close cursors.\n");
        printf("     -reopen-       - Don't reopen but close cursors.\n");
        printf("     -pkglib <LIB>  - Library for %s package "
                                     "(Default: %s).\n",PACKAGE,PACKAGELIB);
        printf("     -qaqqini <LIB> - Library to find QAQQINI.\n\n");
        printf("     -continuous-   - Exit after one SQL statement.\n");
        printf("     -print-        - Don't print SQL data.\n\n");
        if (GetPRCEDvia() != PRCEDpp_VIA_QSQPRCED)
                {
                printf("Environment:\n\n");
                printf("     da_cache_size - SQLDA cache (Default: 0).\n");
                printf("     XDNTraceFile  - Path and Filename prefix.\n\n");
                printf("     QIBM_COMPONENT_TRACE_LEVEL: XDA,NONE\n");
                printf("                                 XDA,ERROR\n");
                printf("                       (Default) XDA,INFO\n");
                printf("                                 XDA,VERBOSE\n\n");
                }
        printf("Redirect (QSH / Windows):\n\n");
        printf("     %s < ScriptFile\n",Program);
        printf("     %s > ResultFile 2> ErrorFile\n",Program);
        printf("     %s > AllOutFile 2>&1\n\n",Program);
        printf("ScriptFile:\n\n");
        printf("    # Comment\n");
        printf("    SQLStatement;<enter>\n");
        printf("    SQLStatement<enter><enter>\n\n");
        printf("BEGIN-END Blocks:\n\n");
        printf("    'BEGIN' and 'END' must appear on separate lines.\n\n");
        printf("Version: %s\n",QUERYTOOL_VERSION);
        printf("Contact: kasseric@de.ibm.com, eric.kass@sap.com\n\n");
        return;
        }

bool ProcessSQL(const char *SQLText,
                bool FindStmt,
                unsigned RowMax,
                unsigned Blocking,
                bool Reopen,
                bool PrintOutput)
        {
        static const char *ProcName = "ProcessSQL";
        int StatementNumber = 0;
        if (FindStmt)
                {
                if (GetPRCEDvia() == PRCEDpp_VIA_QSQPRCED)
                        {
                        // +-------------------------------------+
                        // | Emulate QXDA_FIND_STMT for QSQPRCED |
                        // +-------------------------------------+
                        StatementNumber = FindStatementID(SQLText);
                        }
                else StatementNumber = StatementNumberFINDSTMT;
                }
        bool Status;
        if (strnicmp(SQLText,"SELECT",6) == 0)
                {
                Status = RunSQLQuery(SQLText,
                                     StatementNumber,
                                     RowMax,
                                     Blocking,
                                     Reopen,
                                     PrintOutput);
                if (!Status)
                        {
                        TERROR(("%s: RunSQLQuery() - Failed",ProcName));
                        }
                }
        else if (strnicmp(SQLText,"INSERT",6) == 0)
                {
                Status = PerformInsert(SQLText);
                if (!Status)
                        {
                        TERROR(("%s: PerformInsert() - Failed",ProcName));
                        }
                }
        else if (strnicmp(SQLText,"MERGE",5) == 0)
                {
                Status = PerformMerge(SQLText);
                if (!Status)
                        {
                        TERROR(("%s: PerformMerge() - Failed",ProcName));
                        }
                }
        else    {
                DAVariables NullDA;
                Status = PrepareExecute(SQLText,StatementNumber,NullDA,0);
                if (!Status)
                        {
                        TERROR(("%s: PrepareExecute() - Failed",ProcName));
                        }
                }
        return Status;
        }

int main(int argc, const char* argv[])
        {             
        static const char *ProcName = "main";
        SetMaxTraceLevel(TRACELEVEL_NONE);
        setlocale(LC_ALL,"C");
        // ******************************
        // **** Interactive Terminal ****
        // ******************************
        bool UseStdout = false;
  #ifdef __OS400_TGTVRM__
        if (argc > 1 && GetPRCEDvia() == PRCEDpp_VIA_XDN) // Multiple Threads.
                {
                const char *UseDescriptorSTDIO;
                UseDescriptorSTDIO = getenv("QIBM_USE_DESCRIPTOR_STDIO");
                if (!UseDescriptorSTDIO || *UseDescriptorSTDIO == 'N')
                        {
                        iSeriesTerminal Terminal(iTerminalTITLE);
                        Terminal.Start(argc,argv,NULL,NULL);
                        return Terminal.Run();
                        }
                }
        UseStdout = true;
  #endif
        setbuf(stdout,NULL);
        setbuf(stderr,NULL);
        // ********************************
        // **** Parse Input Parameters ****
        // ********************************
        bool Status = true;
        const char *DBHost = NULL;
        const char *DBUser = NULL;
        const char *DBPassword = NULL;
        const char *RowMaxARG = NULL;
        const char *BlockingARG = NULL;
        const char *PkgLib = NULL;
        const char *QAQQINI = NULL;
        bool SupressOutput = false;
        bool NotContinuous = false;
        bool ShowHelp = false;
        bool NoFindSTMT = false;
        bool NoReopen = false;
        bool EchoInput = false;
        bool EchoInputOff = false;
        ParameterList Parameters[] =
                {
                        {"-host",&DBHost,NULL},
                        {"-h",NULL,&ShowHelp},
                        {"/h",NULL,&ShowHelp},
                        {"/?",NULL,&ShowHelp},
                        {"-user",&DBUser,NULL},
                        {"-pass",&DBPassword,NULL},
                        {"-rowmax",&RowMaxARG,NULL},
                        {"-blocking",&BlockingARG,NULL},
                        {"-pkglib",&PkgLib,NULL},
                        {"-qaqqini",&QAQQINI,NULL},
                        {"-continuous-",NULL,&NotContinuous},
                        {"-findstmt-",NULL,&NoFindSTMT},
                        {"-reopen-",NULL,&NoReopen},
                        {"-print-",NULL,&SupressOutput},
                        {"-echo-",NULL,&EchoInputOff},
                        {"-echo",NULL,&EchoInput},
                        {NULL,NULL,NULL}
                };
        const char **argvend = NULL;
        Status = ParseArguments(Parameters,argc,argv,&argvend);
        if (!Status || ShowHelp)
                {
                Help(argv[0]);
                return 1;
                }
        int argcend = argvend ? argc - (argvend - argv) : 0;
        bool Continuous = !NotContinuous;
        bool FindSTMT = !NoFindSTMT;
        if(!GetPRCEDvia() == PRCEDpp_VIA_QSQPRCED) FindSTMT = true;
        bool Reopen = !NoReopen;
        bool PrintOutput = !SupressOutput;
        unsigned RowMax = RowMaxARG ? atoi(RowMaxARG) : TOTAL_FETCH_ROWS;
        unsigned Blocking = BlockingARG ? atoi(BlockingARG) : BLOCK_FETCH_ROWS;
        if (PkgLib)
                {
                size_t MinLength = strlen(PkgLib);
                size_t MaxLength = sizeof(PACKAGELIB)-1;
                if (MinLength > MaxLength) MinLength = MaxLength;
                memcpyUPPER(PACKAGELIB,PkgLib,MinLength);
                PACKAGELIB[MinLength] = '\0';
                }
        // ************************    // Has little effect if linking XDN in
        // **** Trace Redirect ****    // directly (sharing LevelTrace w/XDN).
        // ************************    
        if (UseStdout)                  
                {
                SetLevelTraceFunction(LevelTraceFileTrace,stderr);
                SetXDNTraceFILE(stderr);
                }
        TPRINT(("SQL QueryTool via PRCED-XDA-XDN (Use -h for Help)"));
        // ************************
        // **** Operating Mode ****
        // ************************
        if (GetPRCEDvia() == PRCEDpp_VIA_QSQPRCED)
                {
                TINFO(("%s: Using QSQPRCED Directly",ProcName));
                }
        else if (GetPRCEDvia() == PRCEDpp_VIA_XDA)
                {
                TINFO(("%s: Using QSQPRCED via XDA",ProcName));
                }
        else if (GetPRCEDvia() == PRCEDpp_VIA_XDN)
                {
                TINFO(("%s: Using QSQPRCED via XDN",ProcName));
                }
        else    {
                TERROR(("%s: Unknown Operating Mode",ProcName));
                }
        // *****************
        // **** Connect ****
        // *****************
        XDAHandle = 0;
        if (GetPRCEDvia() != PRCEDpp_VIA_QSQPRCED)
                {
                if (!DBHost && argcend >= 1) DBHost = argvend[0];
                if (!DBUser && argcend >= 2) DBUser = argvend[1];
                if (!DBPassword && argcend >= 3) DBPassword = argvend[2];
                if (!DBHost || DBHost[0] == '-' || !DBUser || !DBPassword)
                        {
                        TERROR(("%s: Missing Hostname, Username, or Password",
                                ProcName));
                        Status = false;
                        goto Exit;
                        }
                XDAHandle = Connect(DBHost,DBUser,DBPassword);
                if (XDAHandle)
                        {
                        TINFO(("%s: Connected to %s as %s!",ProcName,
                                                            DBHost,
                                                            DBUser));
                        }
                else    {
                        TERROR(("%s: Can't Connect to %s - Exiting",ProcName,
                                                                    DBHost));
                        Status = false;
                        goto Exit;
                        }
                }
        else    {
                // ********************************
                // **** Set Commitment Control ****
                // ********************************
                int rc = system("ENDCMTCTL");
                if(rc)
                        {
                        TINFO(("%s: ENDCMTCTL Failed with rc = %d. Proceeding.", ProcName, rc));
                        }
                rc = system("STRCMTCTL LCKLVL(*CS) CMTSCOPE(*JOB)");
                if (rc)
                        {
                        TERROR(("%s: STRCMTCTL LCKLVL(*CS) Failed",ProcName));
                        Status = false;
                        goto Exit;
                        }
                else    {
                        TINFO(("%s: Isolation Level = *CS",ProcName));
                        }
                }
        // **************************
        // **** QAQQINI Override ****
        // **************************
        if (QAQQINI) SetQAQQINI(QAQQINI);
        // *******************************
        // **** Package Configuration ****
        // *******************************
        if (NoFindSTMT && !PkgLib) 
                {
                TINFO(("%s: Deleting Package since '-findstmt-'",ProcName));
                DeletePackage();
                }
        TINFO(("%s: Using Package: %s/%s",ProcName,PACKAGELIB,PACKAGE));
        if (GetPRCEDvia() == PRCEDpp_VIA_QSQPRCED)
                {
                CreatePackage();        // QSQPRCED has no AutoCreate.
                }
        SetLobPackageDefault(PACKAGELIB,PACKAGE);
        // ********************
        // **** RunSQLStmt ****
        // ********************
        clock_t start = clock();
        if (!EchoInput && !EchoInputOff)
                {
                EchoInput = !isatty(fileno(stdin)) || !isatty(fileno(stdout));
                }  
        do      {
                //TPRINT(("----------------------------------------\n"));
                //TPRINT(("Enter SQL or 'quit' followed by ';' or 2 <enters>"));
                bool EOText = false;
                bool InBEGIN = false;
                char SQLText[64*1024];
                SQLText[0] = '\0';
                char *Pos = SQLText;
                size_t Length = 0;
                size_t Remaining = sizeof(SQLText);
                while (!EOText && Remaining != 0)
                        {
                        char *rc = fgets(Pos,Remaining,stdin);
                        if (!rc) EOText = true;
                        else    {
                                size_t NewLength = strlen(SQLText);
                                size_t Delta = NewLength - Length;
                                if (Delta == 1) 
                                        {
                                        EOText = true;
                                        continue;
                                        }
                                else if (*Pos == '#')
                                        {
                                        continue; // Skip comment.
                                        }
                                if (isKeyword(Pos,"BEGIN"))
                                        {
                                        InBEGIN = true;
                                        }
                                else if (InBEGIN && isKeyword(Pos,"END"))
                                        {
                                        InBEGIN = false;
                                        }
                                if (!InBEGIN)
                                        {
                                        char *SemiPos = strrchr(Pos,';');
                                        if (SemiPos)
                                                {
                                                *SemiPos = ' ';
                                                EOText = true;
                                                }
                                        }
                                Remaining -= Delta;
                                Pos += Delta;
                                Length += Delta;
                                }
                        }
                for (unsigned i=0; i < Length; i++)
                        {
                        if (SQLText[i] == '\n') SQLText[i] = ' ';                
                        }
                rtrim(SQLText);
                if (!SQLText[0] && !feof(stdin)) continue;
                if (!SQLText[0] ||
                    isKeyword(SQLText,"quit") || 
                    isKeyword(SQLText,"exit") ||
                    isKeyword(SQLText,"bye"))
                        {
                        TINFO(("%s: ... bye",ProcName));
                        break;
                        }
                if (EchoInput)
                        {
                        PrintTextCCSID("Issuing",SQLText,strlen(SQLText));
                        }
                Status = ProcessSQL(SQLText,
                                    FindSTMT,
                                    RowMax,
                                    Blocking,
                                    Reopen,
                                    PrintOutput);
                if (!Status)
                        {
                        TERROR(("%s: ProcessSQL() - Failed",ProcName));
                        }
                } while (Continuous);
        // ********************
        // **** Disconnect ****
        // ********************
  Exit: if (XDAHandle) Disconnect(XDAHandle);
        if (UseStdout)
                {
                SetXDNTraceFILE(NULL);
                SetLevelTraceFunction(NULL,NULL);
                }
        clock_t end = clock();
        printf("Milliseconds elapsed: %d\n",end-start);
        getchar();
        return Status ? 0 : 1;
        }
