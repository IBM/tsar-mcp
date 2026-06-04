///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: QxdaUTIL.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 International Business Machines Corporation and others
// All rights reserved.                                                
//                                                                             
///////////////////////////////////////////////////////////////////////////////
/**
 * @file QxdaUTIL.h
 * XDA Utilities to support QPRCEDviaXDA
 */
#ifndef __Qxda_Utilities

        #define __Qxda_Utilities

#include <qusec.h>
#include <qsqprced.h>

#ifdef _WIN32
        #include <qxdaedrsnt.h>
#else
        #include <qxdaedrs.h>
#endif

/**
 * Perform database commit
 * @param Handle Connection handle
 * @return SQL code, 0 if success, positive if warning, 
 *         negative if error
 */
int CommitDB(int Handle);

/**
 * Connect to DB
 * @param HostName Hostname
 * @param RDBName Database name
 * @param Username User name with sufficient priviliges
 * @param Password User password
 * @return Connection handle, 0 if failed
 */
int Connect(const char *HostName, 
            const char *RDBName, 
            const char *Username, 
            const char *Password);

/**
 * Connect to DB
 * @param HostName Hostname
 * @param RDBName Database name
 * @param Username User name with sufficient priviliges
 * @param Password User password
 * @param JobNumber Empty array
 * @param JobUser Empty array
 * @param JobName Empty array
 * @return Connection handle, 0 if failed
 */
int Connect(const char *HostName, 
            const char *RDBName, 
            const char *Username, 
            const char *Password,
            char JobNumber[6+1],
            char JobUser[10+1],
            char JobName[10+1]);

/**
 * Connect to DB
 * @param HostName Hostname
 * @param Username User name with sufficient priviliges
 * @param Password User password
 * @return Connection handle, 0 if failed
 */
inline int Connect(const char *HostName, 
                   const char *Username, 
                   const char *Password)
        {
        return Connect(HostName,NULL,Username,Password);
        }

/**
 * Call program on the host
 * @param Handle Connection Handle
 * @param Library Library containing the program
 * @param Program Program name
 * @param argc Parameter count
 * @param argv Parameter array
 * @return true if successful, otherwise false
 */
bool CallProgram(int Handle,
                 const char *Library,
                 const char *Program,
                 int argc,
                 Qxda_ParmInfo_t argv[]);

/** 
 * Disconnect from database
 * @param Handle Connection handle received from \ref Connect()
 * @return true if successful, otherwise false
 */
bool Disconnect(int Handle);

/**
 * Execute CL command on the host
 * @param Handle Connection handle
 * @param Command String with CL command
 * @return true if successful, otherwise false
 */
bool ProcessCommand(int Handle, const char *Command);

/** 
 * Perform database rollback
 * @param Handle Connection handle
 * @return SQL code, 0 if success, positive if warning, 
 *         negative if error
 */
int RollbackDB(int Handle);

/** 
 * Copies string padded with spaces
 * @param Dest Where to copy to
 * @param Src Where to copy from
 * @param Length Number of characters to copy/pad
 */
void strcpypad(char *Dest, const char *Src, size_t Length);

#endif /* __Qxda_Utilities */
