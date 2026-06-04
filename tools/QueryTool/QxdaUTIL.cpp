///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: QxdaUTIL.cpp
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
/* XDA Utilities to support QPRCEDviaXDA */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
        #include <unistd.h>
#endif

#include <ASThread.h>
#include <HexDump.h>
#include <LevelTrace.h>
#include <PRCEDpp.h>

#include <QxdaUTIL.h>

#define AS4_OBJ_LEN 10

#define TRACE_SQLCODE(Procedure,sqlcode)                                     \
        {                                                                    \
        if (sqlcode < 0) TERROR((#Procedure ": %d", (sqlcode)));             \
        else TINFO((#Procedure ": %d", (sqlcode)));                          \
        }

static char* rtrim(char *String)                        // Modifies String!
        {
        if (!String) return NULL;
        char *pos = String + strlen(String);
        while (pos != String && *(pos-1) == ' ') pos--;
        *pos = '\0';
        return String;
        }

// ***************************************************************************
// **** QSQPRCEDviaXDA Support ***********************************************
// ***************************************************************************

bool QSQPRCED_XDADeferOpen = false;

static TLSKey_t QSQPRCED_XDAHandle_Key = INVALID_TLSKEY;

class _Init_QSQPRCED_XDAHandle_Key
        {
        public:
                _Init_QSQPRCED_XDAHandle_Key();
                ~_Init_QSQPRCED_XDAHandle_Key();

        } static _Init_QSQPRCED_XDAHandle_Key;

_Init_QSQPRCED_XDAHandle_Key::_Init_QSQPRCED_XDAHandle_Key()
        {
        static const char *ProcName = "_Init_QSQPRCED_XDAHandle_Key";
        QSQPRCED_XDAHandle_Key = CreateTLS();
        if (QSQPRCED_XDAHandle_Key == INVALID_TLSKEY)
                {
                TERROR(("%s: CreateTLS Failed",ProcName));
                }
        return;
        }

_Init_QSQPRCED_XDAHandle_Key::~_Init_QSQPRCED_XDAHandle_Key()
        {
        if (QSQPRCED_XDAHandle_Key != INVALID_TLSKEY)
                {
                FreeTLS(QSQPRCED_XDAHandle_Key);
                QSQPRCED_XDAHandle_Key = INVALID_TLSKEY;
                }
        return;
        }

static int Get_QSQPRCED_XDAHandle_Quiet()
        {
        return (int)(size_t)GetTLS(QSQPRCED_XDAHandle_Key);
        }

int Get_QSQPRCED_XDAHandle()
        {
        static const char *ProcName = "Get_QSQPRCED_XDAHandle";
        int XDAHandle = Get_QSQPRCED_XDAHandle_Quiet();
        if (!XDAHandle)
                {
                TERROR(("%s: No handle set in thread",ProcName));
                return 0;
                }
        return XDAHandle;
        }

bool Set_QSQPRCED_XDAHandle(int Handle)
        {
        static const char *ProcName = "Set_QSQPRCED_XDAHandle";
        void *XDAHandleStorage = (void *)(size_t)Handle;
        bool Status = SetTLS(QSQPRCED_XDAHandle_Key,XDAHandleStorage);
        if (!Status)
                {
                TERROR(("%s: No handle set in thread",ProcName));
                }
        return Status;
        }

// ***************************************************************************
// **** Utility **************************************************************
// ***************************************************************************

void strcpypad(char *Dest, const char *Src, size_t Length)
        {
        size_t SrcLength = strlen(Src);
        if (SrcLength > Length) SrcLength = Length;
        memcpy(Dest,Src,SrcLength);
        memset(Dest+SrcLength,' ',Length-SrcLength);
        return;
        }

// ***************************************************************************
// **** XDA Functions ********************************************************
// ***************************************************************************

static void InitCDBI0200(Qxda_CDBI0200_t *CDBI, 
                         size_t CDBISize,
                         size_t Length_Job_Data)
        {
        size_t Length_Suspend_Data = 0;
        size_t Length_Profile = AS4_OBJ_LEN;
        size_t Length_Password = 128;

        memset(CDBI,0,CDBISize);        
        size_t Pos = sizeof(Qxda_CDBI0200_t);
        CDBI->Length_Job_Data = Length_Job_Data;
        CDBI->Offset_Job_Data = Pos;
        Pos += Length_Job_Data;
        CDBI->Length_Suspend_Data = Length_Suspend_Data;
        CDBI->Offset_Suspend_Data = Pos;
        Pos += Length_Suspend_Data;
        CDBI->Length_Profile = Length_Profile;
        CDBI->Offset_Profile = Pos;
        Pos += Length_Profile;
        CDBI->Length_Password = Length_Password;
        CDBI->Offset_Password = Pos;
        return;
        }

#ifdef __XDN_XDA_API_

static void InitCDBI0220(Qxdn_CDBI0220_t *CDBI, 
                         size_t CDBISize,
                         size_t Length_Job_Data)
        {
        size_t Length_Suspend_Data = 0;
        size_t Length_Profile = AS4_OBJ_LEN;
        size_t Length_Password = 128;

        memset(CDBI,0,CDBISize);        
        size_t Pos = sizeof(Qxdn_CDBI0220_t);
        CDBI->Length_Job_Data = Length_Job_Data;
        CDBI->Offset_Job_Data = Pos;
        Pos += Length_Job_Data;
        CDBI->Length_Suspend_Data = Length_Suspend_Data;
        CDBI->Offset_Suspend_Data = Pos;
        Pos += Length_Suspend_Data;
        CDBI->Length_Profile = Length_Profile;
        CDBI->Offset_Profile = Pos;
        Pos += Length_Profile;
        CDBI->Length_Password = Length_Password;
        CDBI->Offset_Password = Pos;
        return;
        }

#endif

// *****************
// **** Connect ****
// *****************

int Connect(const char *HostName, 
            const char *RDBName, 
            const char *Username, 
            const char *Password,
            char JobNumber[6+1],
            char JobUser[10+1],
            char JobName[10+1])
        {
        int Handle = 0;
        char *JobData = "QxdaUTIL Client";
        // --------------------
        // ---- Error Code ----
        // --------------------
        char ErrorMemory[128];
        Qus_EC_t *ErrorCode = (Qus_EC_t *)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);
        // ---------------
        // ---- Input ----
        // ---------------
        char CDBIBuffer[1024];
  #ifdef __XDN_XDA_API_
        char *InputFormat = "CDBI0220";
        Qxdn_CDBI0220 *CDBI = (Qxdn_CDBI0220 *)CDBIBuffer;
        InitCDBI0220(CDBI,sizeof(CDBIBuffer),strlen(JobData));
        const char *RLEEnvVar = getenv("use_data_direct");
        if (RLEEnvVar) CDBI->Use_Data_Direct = RLEEnvVar[0] == '1' ||
                                               RLEEnvVar[0] == 'T' ||
                                               RLEEnvVar[0] == 't' ||
                                               RLEEnvVar[0] == 'Y' ||
                                               RLEEnvVar[0] == 'y'
                                               ? true
                                               : false;
        else CDBI->Use_Data_Direct = false;
  #else
        char *InputFormat = "CDBI0200";
        Qxda_CDBI0200 *CDBI = (Qxda_CDBI0200 *)CDBIBuffer;
        InitCDBI0200(CDBI,sizeof(CDBIBuffer),strlen(JobData));
  #endif
        const char *DAEnvVar = getenv("da_cache_size");
        if (DAEnvVar) CDBI->SQLDA_Cache_Size = atoi(DAEnvVar);
        else CDBI->SQLDA_Cache_Size = 0;

        CDBI->Connection_Type = HostName && *HostName ? 'T' : 'L';
        CDBI->Commitment_Control = 'C';
        if (RDBName && *RDBName)
                {
                memset(CDBI->RDB_Name,' ',18);
                for (unsigned i=0; i < 18 && RDBName[i]; i++)
                        {
                        CDBI->RDB_Name[i] = toupper(RDBName[i]);
                        }
                CDBI->RDB_Specified = 'Y';
                }
        else CDBI->RDB_Specified = 0;
        CDBI->Allow_Suspend = 'N';
        CDBI->Convert_Endian_Data = '1';
        CDBI->Server_Job_CCSID = 500;
        memcpy(CDBI->Commit_Scope,"*JOB      ",10);
        char *Job_Data = (char *)CDBI + CDBI->Offset_Job_Data;
        memcpy(Job_Data,JobData,CDBI->Length_Job_Data);
        memset(CDBI->Server_Name,' ',sizeof(CDBI->Server_Name));
        strcpy(CDBI->Server_Name,HostName);

        char *Profile = (char *)CDBI + CDBI->Offset_Profile;
        memset(Profile,' ',CDBI->Length_Profile);
        int UsernameLength = strlen(Username);
        if (UsernameLength > CDBI->Length_Profile)
                {
                TERROR(("Username too long"));
                return 0;
                }
        for (int i=0; Username[i] && i < UsernameLength; i++) 
                {
                Profile[i] = toupper(Username[i]);
                }
        if (GetPRCEDvia() != PRCEDpp_VIA_XDA) // XDA needs AS4_OBJ_LEN.
                {
                CDBI->Length_Profile = UsernameLength;
                }
        char *Passwd = (char *)CDBI + CDBI->Offset_Password;
        memset(Passwd,' ',CDBI->Length_Password);
        int PasswordLength = strlen(Password);
        if (PasswordLength > CDBI->Length_Password)
                {
                TERROR(("Password too long"));
                return 0;
                }
        memcpy(Passwd,Password,PasswordLength);
        CDBI->Length_Password = PasswordLength;
        // ---------------
        // ---- Output ---
        // ---------------
        Qxda_CDBO0100_t CDBOBuffer;
        Qxda_CDBO0100 *CDBO = (Qxda_CDBO0100 *)&CDBOBuffer;
        memset(CDBO,0,sizeof(CDBOBuffer));
        int CDBOSize = sizeof(CDBOBuffer);
        // -----------------
        // ---- Connect ----
        // -----------------
        QxdaConnectEDRS(CDBI,InputFormat,
                        CDBO,&CDBOSize,"CDBO0100",
                        ErrorCode);
        if (ErrorCode->Bytes_Available)
                {
                TERROR(("Connect to: %s as %s failed (Error=%.7s)",
                        HostName,
                        Username,
                        ErrorCode->Exception_Id));
                THexDump(ErrorMemory,ErrorCode->Bytes_Available);
                }
        else    {
                memcpy(JobNumber,CDBO->Server_Job_Number,6);
                JobNumber[6] = '\0';
                rtrim(JobNumber);
                memcpy(JobUser,CDBO->Server_User_Name,10);
                JobUser[10] = '\0';
                rtrim(JobUser);
                memcpy(JobName,CDBO->Server_Job_Name,10);
                JobName[10] = '\0';
                rtrim(JobName);
                TINFO(("Connect to: %s (Method=%c)",
                       HostName,
                       CDBO->Connection_Type_Used));
                TINFO(("Database Server Job: %s/%s/%s",JobNumber,
                                                       JobUser,
                                                       JobName));
                Handle = CDBO->Connection_Handle;
                }
        if (Get_QSQPRCED_XDAHandle_Quiet() <= 0)
                {
                Set_QSQPRCED_XDAHandle(Handle);
                }
        return Handle;
        }

int Connect(const char *HostName, 
            const char *RDBName,
            const char *Username, 
            const char *Password)
        {
        char JobNumber[6+1];
        char JobUser[AS4_OBJ_LEN+1];
        char JobName[AS4_OBJ_LEN+1];
        int Handle = Connect(HostName,
                             RDBName,
                             Username,
                             Password,
                             JobNumber,
                             JobUser,
                             JobName);
        return Handle;
        }

// *********************
// **** CallProgram ****
// *********************

bool CallProgram(int Handle,
                 const char *Library,
                 const char *Program,
                 int argc,
                 Qxda_ParmInfo_t argv[])
        {
        bool Status;
        char ErrorMemory[128];
        char PgmLib[2*AS4_OBJ_LEN];
        Qus_EC_t *ErrorCode = (Qus_EC_t *)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);
        strcpypad(&PgmLib[0], Program, AS4_OBJ_LEN);
        strcpypad(&PgmLib[AS4_OBJ_LEN], Library, AS4_OBJ_LEN);
        QxdaCallProgramEDRS(&Handle,
                            PgmLib,
                            &argc,
                            argv,
                            ErrorCode);
        if (ErrorCode->Bytes_Available)
                {
                TERROR(("Unable to call: %s/%s (Error=%.7s)",
                       Library,
                       Program,
                       ErrorCode->Exception_Id));
                THexDump(ErrorMemory,ErrorCode->Bytes_Available);
                Status = false;
                }
        else Status = true;
        return Status;
        }

// ******************
// **** CommitDB ****
// ******************

int CommitDB(int Handle)
        {
        ASError Error("CommitDB");
        Qsq_sqlca_t sqlca;
        int Options = QXDA_COMMIT_WORK;
        memset(&sqlca,0,sizeof(sqlca));
        sqlca.sqlcabc = sizeof(sqlca);
        QxdaCommitEDRS(&Handle,
                       &Options,
                       &sqlca,
                       Error.GetErrorCode());
        TRACE_SQLCODE("CommitDB",sqlca.sqlcode);
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

// ********************
// **** Disconnect ****
// ********************

bool Disconnect(int Handle)
        {
        bool Status;
        char ErrorMemory[128];
        Qus_EC_t *ErrorCode = (Qus_EC_t *)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);
        int DisconnectOptions = QXDA_DISCONNECT_COMMIT;
        int HoldHandle = Handle;
        QxdaDisconnectEDRS(&Handle,
                           &DisconnectOptions,
                           ErrorCode);
        if (ErrorCode->Bytes_Available)
                {
                TERROR(("Disconnect Failed (Error=%.7s)",
                       ErrorCode->Exception_Id));
                THexDump(ErrorMemory,ErrorCode->Bytes_Available);
                Status = false;
                }
        else    {
                if (Get_QSQPRCED_XDAHandle_Quiet() == HoldHandle)
                        {
                        Set_QSQPRCED_XDAHandle(0);
                        }
                Status = true;
                }
        return Status;
        }

// ************************
// **** ProcessCommand ****
// ************************

bool ProcessCommand(int Handle, const char *Command)
        {
        bool Status;
        char ErrorMemory[128];
        int CommandLength = (int)strlen(Command);
        Qus_EC_t *ErrorCode = (Qus_EC_t *)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);
        QxdaProcessCommandEDRS(&Handle,
                               (char *)Command,
                               &CommandLength,
                               ErrorCode);
        if (ErrorCode->Bytes_Available)
                {
                TERROR(("%s Failed (Error=%.7s)",
                        Command,
                        ErrorCode->Exception_Id));
                THexDump(ErrorMemory,ErrorCode->Bytes_Available);                        
                Status = false;
                }
        else Status = true;
        return Status;
        }

// ********************
// **** RollbackDB ****
// ********************

int RollbackDB(int Handle)
        {
        ASError Error("RollbackDB");
        Qsq_sqlca_t sqlca;
        int Options = QXDA_ROLLBACK_WORK;
        memset(&sqlca,0,sizeof(sqlca));
        sqlca.sqlcabc = sizeof(sqlca);
        QxdaRollbackEDRS(&Handle,
                         &Options,
                         &sqlca,
                         Error.GetErrorCode());
        TRACE_SQLCODE("RollbackDB",sqlca.sqlcode);
        if (Error.IsError()) throw Error;
        return sqlca.sqlcode;
        }

// ***************************************************************************
// **************************** End of File **********************************
// ***************************************************************************
