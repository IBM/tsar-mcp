// MCP/ACP Client: XCPClient.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: XCPClient.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __XCP_Client_Main

        #define __XCP_Client_Main

#include <stdio.h>

#include <ASThread.h>
#include <AutoString.h>

// ***************************************************************************
// **** XCP Message Loop *****************************************************
// ***************************************************************************

class XCPClientIOMain : public MessageQueueLoop
        {
        private:
                FILE *outFile;
                // -------------
                // ---- XCP ----
                // -------------
                bool Handle_XCPMessage(const char *XCPMessage);
                bool Handle_XCPOutput(const void *UserPtr);
                bool PostXCPMessage(char *XCPMessage);
                bool PostShutdown();
                bool PostStdError(char *ErrorText);
                friend class XCPClientIOReader;
                friend class XCPClientErrReader;
        protected: 
                virtual long getID() = 0;
                bool WriteXCPOutput(const char *XCPOutput);
                // ------------------
                // ---- ASThread ----
                // ------------------
                virtual bool OnMessage(void *Msg);
                virtual void OnMessageDestruct(void *Msg);
                // ------------------------------
                // ---- XCP Message Handlers ----
                // ------------------------------
                virtual bool OnReference(void *UserPtr) {return true;};
                virtual void OnReferenceDestruct(void *UserPtr) {};
                virtual void OnStdError(const char *ErrorText);
                virtual bool OnXCPRequest(const char *XCPRequest);
                virtual bool OnXCPResponse(const char *XCPResponse) = 0;
        public:
                XCPClientIOMain();
                virtual ~XCPClientIOMain() {Stop(true);}
                bool PostXCPOutput(char **XCPOutput); // XCPOutput is malloc().
                bool PostReference(void **UserPtr);
                int Run(FILE *out);
        };

// ***********************
// **** XCPClientMain ****
// ***********************

class XCPClientMain : public XCPClientIOMain
        {
        private:
                CriticalSection CancelHandleCS;
                FILE *CancelHandle;
                bool Canceled;
                long pidServer;
                Semaphore RunningSemaphore;
                bool Running;
        protected:
                bool IsRunning() {return Running;}      // No wait.
                void SignalRunning(bool Started);
                bool WaitRunning();
        public:
                XCPClientMain();
                virtual ~XCPClientMain() {Stop(true);}
                bool Cancel();
                virtual long getID() {return pidServer;}
                bool PostXCPOutput(char **XCPOutput);   // Start request.
                bool PostReference(void **UserPtr);     //   ""    ""
                void Reset();
                int Run(const char *Command);           // Blocking loop.
                virtual bool Stop(bool WaitToEnd);      // End nicely.
        };
               
// *************************
// **** XCPClientThread ****
// *************************

class XCPClientThread : public ASThread,
                        public XCPClientMain
        {
        private:
                AutoString ServerCMD;
        protected:
                virtual int ThreadMain();
                virtual void WaitToEnd() {Wait();}
        public:
                XCPClientThread(const char *Command)
                        {
                        ServerCMD = Command;
                        }
                virtual ~XCPClientThread() {Stop(true);}
                bool Start();                      // True: ACP server active.
                virtual bool Stop(bool WaitToEnd); // End nicely.
        };

// ***********************************************
// **** Free JSON Output Request *****************
// *****************************
//
// Output sent via PostXCPOutput must be allocated
// with malloc() FreeXCPRequestBuffer frees it.
//
// ***********************************************

void FreeXCPRequestBuffer(char **Buffer);

/* **************************************************************************
 ****************************************************************************

 Notes:

 ****************************************************************************
 **** Example ***************************************************************
 *************


 ****************************************************************************

 ****************************************************************************
 ******************************** End of File *******************************
 ************************************************************************* */

#endif /* __XCP_Client_Main */
