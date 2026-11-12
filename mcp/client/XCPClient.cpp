// MCP/ACP Client: XCPClient.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: XCPClient.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <popenNT.h>

#include <XCPClient.h>

#ifndef XCPClient_ShutdownGraceMS
        #define XCPClient_ShutdownGraceMS 4000 // Seconds for server shutdown.
#endif

// ***********************************************
// **** Free JSON Output Request *****************
// *****************************
//
// Output sent via PostXCPOutput must be allocated
// with malloc() FreeXCPRequestBuffer frees it.
//
// ***********************************************

void FreeXCPRequestBuffer(char **Buffer)
        {
        if (Buffer && *Buffer)
                {
                free(*Buffer);
                *Buffer = NULL;
                }
        return;
        }

// ***************************************************************************
// **** XCP Message Loop Messages ********************************************
// ***************************************************************************

enum XCPClientIOMessage_Reason_t
        {
        XIM_Ping=0, 
        XIM_Reference,                  // Pipe a UserPtr.
        XIM_Shutdown, 
        XIM_StdError,
        XIM_XCPRequest,                 // Initiate XCPOutput to server.
        XIM_XCPResponse                 // Message from XCPClientIOReader.
        };

struct XCPClientIOMessage               // Responses handled in MessageQueueLoop.
        {
        char EyeCatcher[8];
        XCPClientIOMessage_Reason_t Reason;
        void *UserPtr;
        XCPClientIOMessage(XCPClientIOMessage_Reason_t reason, void *userPtr) 
                {
                memcpy(EyeCatcher,"ERKIOMSG",8);
                Reason = reason;
                UserPtr = userPtr;
                return;
                }
        };

// ***************************************************************************
// **** Read STDIN for JSON-RPC 2.0 Input (Synchronous) **********************
// ***************************************************************************

#ifndef ReadSTDIN_RPCLine_MAXSIZE
        #define ReadSTDIN_RPCLine_MAXSIZE (20*1024*1024)
#endif

#ifdef _DEBUG
        #define ReadSTDIN_RPCLine_BufferSIZE 128
#else
        #define ReadSTDIN_RPCLine_BufferSIZE 2048
#endif /* _DEBUG */            

static char ReadSTDIN_OVERFLOW_SENTINEL[ReadSTDIN_RPCLine_BufferSIZE];

void FreeRPCLine(char **Buffer)
        {
        if (Buffer && *Buffer)
                {
                if (*Buffer != ReadSTDIN_OVERFLOW_SENTINEL)
                        {
                        free(*Buffer);
                        }
                *Buffer = NULL;
                }
        return;
        }

char* ReadSTDIN_RPCLine(FILE *in)   // "getline()" - reads until EOL.
        {
        static const char *ProcName = "ReadSTDIN_JSONRPCLine";
        size_t BufferSize = ReadSTDIN_RPCLine_BufferSIZE;
        char *Buffer = (char *)malloc(BufferSize);
        if (!Buffer)
                {
                TERROR(("%s: Unable to malloc(%u)", ProcName,BufferSize));
                return NULL;
                }
        size_t Used = 0;
        do      {
                // ---------------------------
                // --- Read until Newline ----
                // ---------------------------
                char *Pos = Buffer + Used;
                char *readBuffer = fgets(Pos,BufferSize-Used,in);
                if (!readBuffer)
                        {
                        if (feof(in)) TDEBUG(("%s: fgets() - EOF",ProcName));
                        else TERROR(("%s: fgets() Error",ProcName));
                        FreeRPCLine(&Buffer);
                        break;
                        }
                size_t readBufferLength = strlen(readBuffer);
                if (!readBufferLength)
                        {
                        TERROR(("%s: Unexpected fgets() no read",ProcName));
                        FreeRPCLine(&Buffer);
                        break;
                        }
                if (readBuffer[readBufferLength-1] == '\n')
                        {
                        readBuffer[readBufferLength-1] = '\0';
                        break;
                        }
                // --------------------------
                // --- Overflow handling ----
                // --------------------------
                if (Buffer == ReadSTDIN_OVERFLOW_SENTINEL)
                        {
                        continue; // ...reading until the end of message.
                        }
                if (BufferSize > ReadSTDIN_RPCLine_MAXSIZE)
                        {
                        TERROR(("%s: Size governor triggered: "
                                "ReadSTDIN_RPCLine_MAXSIZE (0x%X bytes)",
                                ProcName,
                                ReadSTDIN_RPCLine_MAXSIZE));
                        FreeRPCLine(&Buffer);
                        Buffer = ReadSTDIN_OVERFLOW_SENTINEL;
                        BufferSize = ReadSTDIN_RPCLine_BufferSIZE;
                        Used = 0;
                        continue;
                        }
                // --------------------------
                // --- Reallocate Buffer ----
                // --------------------------
                Used += readBufferLength;
                size_t NewBufferSize = BufferSize * 2;
                char *NewBuffer = (char *)realloc(Buffer,NewBufferSize);
                if (!NewBuffer)
                        {
                        TERROR(("%s: Unable to realloc(%u)",ProcName,
                                                            NewBufferSize));
                        FreeRPCLine(&Buffer);
                        break;
                        }
                Buffer = NewBuffer;
                BufferSize = NewBufferSize;
                } while (1);
        return Buffer;
        }

// ***************************************************************************
// **** XCPClientIOMain Implementation ***************************************
// ***************************************************************************

XCPClientIOMain::XCPClientIOMain()
        {
        outFile = NULL;
        return;
        }

bool XCPClientIOMain::OnMessage(void *Msg)
        {
        static const char *ProcName = "XCPClientIOMain::OnMessage";
        XCPClientIOMessage *XCPMsg = (XCPClientIOMessage *)Msg;
        if (XCPMsg->Reason == XIM_XCPResponse)
                {
                char *XCPMessage = (char *)XCPMsg->UserPtr;
                bool Status = Handle_XCPMessage(XCPMessage);
                if (!Status)
                        {
                        TERROR(("%s: Handle_XCPMessage() Failed",ProcName));
                        }
                FreeRPCLine(&XCPMessage);
                }
        else if (XCPMsg->Reason == XIM_XCPRequest)
                {
                char *XCPOutput = (char *)XCPMsg->UserPtr;
                bool Status = Handle_XCPOutput(XCPOutput);
                if (!Status)
                        {
                        TERROR(("%s: Handle_XCPOutput() Failed",ProcName));
                        }
                FreeXCPRequestBuffer(&XCPOutput);
                }
        else if (XCPMsg->Reason == XIM_Reference)
                {
                bool Status = OnReference(XCPMsg->UserPtr);
                if (!Status)
                        {
                        TERROR(("%s: OnReference() Failed",ProcName));
                        }
                }
        else if (XCPMsg->Reason == XIM_Shutdown)
                {
                bool Status = Stop();
                if (!Status)
                        {
                        TERROR(("%s: Failed to Stop()",ProcName));
                        }
                else    {
                        TINFO(("%s: Stopping Message Loop",ProcName));
                        }
                }
        else if (XCPMsg->Reason == XIM_StdError)
                {
                char *ErrorText = (char *)XCPMsg->UserPtr;
                if (ErrorText == ReadSTDIN_OVERFLOW_SENTINEL)
                        {
                        TERROR(("%s: StdError Overflow",ProcName));
                        }
                else if (ErrorText) OnStdError(ErrorText);
                FreeRPCLine(&ErrorText);
                }
        else if (XCPMsg->UserPtr)
                {
                TERROR(("%s: Can't handle UserPtr for XIM_Reason: %d",
                        ProcName,
                        XCPMsg->Reason));
                }
        delete XCPMsg;
        return true;
        }

void XCPClientIOMain::OnMessageDestruct(void *Msg)
        {
        static const char *ProcName = "XCPClientIOMain::OnMessageDestruct";
        XCPClientIOMessage *XCPMsg = (XCPClientIOMessage *)Msg;
        if (XCPMsg->Reason == XIM_XCPResponse)
                {
                char *XCPMessage = (char *)XCPMsg->UserPtr;
                FreeRPCLine(&XCPMessage);
                }
        else if (XCPMsg->Reason == XIM_XCPRequest)
                {
                char *XCPOutput = (char *)XCPMsg->UserPtr;
                FreeXCPRequestBuffer(&XCPOutput);
                }
        else if (XCPMsg->Reason == XIM_Reference)
                {
                OnReferenceDestruct(XCPMsg->UserPtr);
                }
        else if (XCPMsg->Reason == XIM_StdError)
                {
                char *ErrorText = (char *)XCPMsg->UserPtr;
                FreeRPCLine(&ErrorText);
                }
        else if (XCPMsg->UserPtr)
                {
                TERROR(("%s: Can't handle UserPtr for XIM_Reason: %d",
                        ProcName,
                        XCPMsg->Reason));
                }
        delete XCPMsg;
        return;
        }

void XCPClientIOMain::OnStdError(const char *ErrorText)
        {
        TINFO(("StdErr[%ld]: %s",getID(),ErrorText));
        return;
        }

bool XCPClientIOMain::Handle_XCPMessage(const char *XCPMessage)
        {
        static const char *ProcName = "XCPClientIOMain::Handle_XCPMessage";
        if (!XCPMessage)
                {
                TERROR(("%s: Missing XCPMessage",ProcName));
                }
        else if (XCPMessage == ReadSTDIN_OVERFLOW_SENTINEL)
                {
                TERROR(("%s: XCP Client Message Overflow",ProcName));
                }
        else    {
              //TDEBUG(("%s[%ld]: Dispatching Response",ProcName,getID()));
                bool Status = OnXCPResponse(XCPMessage);
                if (!Status)
                        {
                        TERROR(("%s: OnXCPResponse() Failed",ProcName));
                        }
                }
        return true;
        }

bool XCPClientIOMain::Handle_XCPOutput(const void *UserPtr)
        {
        static const char *ProcName = "XCPClientIOMain::Handle_XCPOutput";
        const char *XCPOutput = (const char *)UserPtr;
        if (!XCPOutput)
                {
                TERROR(("%s: Missing XCPOutput",ProcName));
                return false;
                }
        bool Status = OnXCPRequest(XCPOutput);
        if (!Status)
                {
                TERROR(("%s: OnXCPRequest() Did Not Send",ProcName));
                }
        return Status;
        }

bool XCPClientIOMain::OnXCPRequest(const char *XCPRequest)
        {
        return WriteXCPOutput(XCPRequest);      // Forward request to server.
        }        

bool XCPClientIOMain::PostReference(void **UserPtr)
        {
        static const char *ProcName = "XCPClientIOMain::PostReference";
        if (!UserPtr) 
                {
                TERROR(("%s: No UserPtr to Post",ProcName));
                return false;
                }
        bool Status = false;
        XCPClientIOMessage *XCPMsg;
        XCPMsg = new XCPClientIOMessage(XIM_Reference,*UserPtr);
        if (!XCPMsg)
                {
                TERROR(("%s: Can't allocate XCPClientIOMessage",ProcName));
                }
        else    {
                Status = Send(XCPMsg);
                if (!Status) delete XCPMsg;
                else *UserPtr = NULL;           // UserPtr is owned. 
                }
        return Status;
        }

bool XCPClientIOMain::PostShutdown()
        {
        static const char *ProcName = "XCPClientIOMain::PostShutdown";
        bool Status = false;
        XCPClientIOMessage *XCPMsg;
        XCPMsg = new XCPClientIOMessage(XIM_Shutdown,NULL);
        if (!XCPMsg)
                {
                TERROR(("%s: Can't allocate XCPClientIOMessage",ProcName));
                }
        else    {
                Status = SendPriority(XCPMsg);
                if (!Status) delete XCPMsg;
                }
        return Status;
        }
          
bool XCPClientIOMain::PostStdError(char *ErrorText)
        {
        static const char *ProcName = "XCPClientIOMain::PostStdError";
        bool Status = false;
        XCPClientIOMessage *XCPMsg;
        XCPMsg = new XCPClientIOMessage(XIM_StdError,ErrorText);
        if (!XCPMsg)
                {
                TERROR(("%s: Can't allocate XCPClientIOMessage",ProcName));
                }
        else    {
                Status = Send(XCPMsg);
                if (!Status) delete XCPMsg;
                }
        return Status;
        }

bool XCPClientIOMain::PostXCPMessage(char *XCPMessage)
        {
        static const char *ProcName = "XCPClientIOMain::PostXCPMessage";
        bool Status = false;
        XCPClientIOMessage *XCPMsg;
        XCPMsg = new XCPClientIOMessage(XIM_XCPResponse,XCPMessage);
        if (!XCPMsg)
                {
                TERROR(("%s: Can't allocate XCPClientIOMessage",ProcName));
                }
        else    {
                Status = Send(XCPMsg);
                if (!Status) delete XCPMsg;
                }
        return Status;
        }

bool XCPClientIOMain::PostXCPOutput(char **XCPOutput)
        {
        static const char *ProcName = "XCPClientIOMain::PostXCPOutput";
        if (!XCPOutput || !*XCPOutput) 
                {
                TERROR(("%s: No XCPOutput to Post",ProcName));
                return false;
                }
        bool Status = false;
        XCPClientIOMessage *XCPMsg;
        XCPMsg = new XCPClientIOMessage(XIM_XCPRequest,*XCPOutput);
        if (!XCPMsg)
                {
                TERROR(("%s: Can't allocate XCPClientIOMessage",ProcName));
                }
        else    {
                Status = Send(XCPMsg);
                if (!Status) delete XCPMsg;
                else *XCPOutput = NULL;         // XCPOutput is owned. 
                }
        return Status;
        }

int XCPClientIOMain::Run(FILE *out)
        {
        outFile = out;
        return MessageQueueLoop::Run();
        }

bool XCPClientIOMain::WriteXCPOutput(const char *XCPOutput)
        {
        static const char *ProcName = "XCPClientIOMain::WriteXCPOutput";
        if (!XCPOutput)
                {
                TERROR(("%s: Missing XCPOutput",ProcName));
                return false;
                }
        bool Status = true;
        unsigned nBytes = strlen(XCPOutput);
        TDEBUG(("%s[%ld]: Sending %u bytes",ProcName,getID(),nBytes));
        // ---------------------------------------------------------------
        // The newline is the message delimiter: a write that lands every
        // payload byte but drops the '\n' still desyncs the server, so the
        // expected count is nBytes+1 -- not nBytes.
        // ---------------------------------------------------------------
        int rc = fprintf(outFile,"%s\n",XCPOutput);
        if (rc < (int)nBytes+1)
                {
                TERROR(("%s[%ld]: Incomplete %d < %u",ProcName,
                                                      getID(),
                                                      rc,
                                                      nBytes+1));
                Status = false;
                }
        fflush(outFile);
        return Status;
        }

// ***************************************************************************
// **** XCP Reader Threads ***************************************************
// ***************************************************************************

class XCPClientIOReader : public ASThread
        {
        private:
                XCPClientIOMain &ClientMain;
                FILE *inFile;
        protected:
                virtual int ThreadMain();
        public:
                XCPClientIOReader(XCPClientIOMain &IOMain)
                        :
                        ClientMain(IOMain)
                        {
                        inFile = NULL;
                        return;
                        }
                bool Start(FILE *in) 
                        {
                        inFile = in;
                        return ASThread::Start();
                        }
        };

class XCPClientErrReader : public ASThread
        {
        private:
                XCPClientIOMain &ClientMain;
                FILE *inFile;
        protected:
                virtual int ThreadMain();
        public:
                XCPClientErrReader(XCPClientIOMain &IOMain)
                        :
                        ClientMain(IOMain)
                        {
                        inFile = NULL;
                        return;
                        }
                bool Start(FILE *in) 
                        {
                        inFile = in;
                        return ASThread::Start();
                        }
        };

// ***************************
// **** XCPClientIOReader ****
// ***************************

int XCPClientIOReader::ThreadMain()
        {
        static const char *ProcName = "XCPClientIOReader::ThreadMain";
        bool Continue = true;
        while (Continue)
                {
                char *MCPMessage = ReadSTDIN_RPCLine(inFile);
                if (!MCPMessage)
                        {
                        // -----------------------------------------------
                        // Expected on an orderly shutdown: the child saw
                        // EOF on its stdin and exited. ReadSTDIN_RPCLine()
                        // has already logged EOF vs. a genuine read error.
                        // -----------------------------------------------
                        TINFO(("%s: inFile broken-pipe - Exit",ProcName));
                        ClientMain.PostShutdown();
                        Continue = false;
                        }
                else    {
                        Continue = ClientMain.PostXCPMessage(MCPMessage);
                        if (!Continue)
                                {
                                TERROR(("%s: PostXCPMessage Failed",ProcName));
                                FreeRPCLine(&MCPMessage);
                                }
                        }
                }
        TINFO(("%s: Exiting ThreadMain()",ProcName));
        return 0;
        }

// ****************************
// **** XCPClientErrReader ****
// ****************************

int XCPClientErrReader::ThreadMain()
        {
        static const char *ProcName = "XCPClientErrReader::ThreadMain";
        bool Continue = true;
        while (Continue)
                {
                char *ErrorText = ReadSTDIN_RPCLine(inFile);
                if (!ErrorText)
                        {
                        TINFO(("%s: errFile broken-pipe - Exit",ProcName));
                        Continue = false;
                        }
                else    {
                        Continue = ClientMain.PostStdError(ErrorText);
                        if (!Continue)
                                {
                                TERROR(("%s: PostStdError Failed",ProcName));
                                FreeRPCLine(&ErrorText);
                                }
                        }
                }
        TINFO(("%s: Exiting ThreadMain()",ProcName));
        return 0;
        }

// ***************************************************************************
// **** XCPClientMain ********************************************************
// ***************************************************************************

XCPClientMain::XCPClientMain()
        :
        RunningSemaphore(0)
        {
        pidServer = -1;
        Canceled = false;
        CancelHandle = NULL;
        Running = false;
        return;
        }

void XCPClientMain::SignalRunning(bool Started)
        {
        // ---------------------------------------------------------------
        // Released exactly once per Run(), on every exit path, so a
        // waiting Start() is never left holding an unanswered question.
        // ---------------------------------------------------------------
        Running = Started;
        RunningSemaphore.Release();
        return;
        }

bool XCPClientMain::WaitRunning()
        {
        RunningSemaphore.Take();
        return Running;
        }

bool XCPClientMain::PostXCPOutput(char **XCPOutput)
        {
        static const char *ProcName = "XCPClientMain::PostXCPOutput";
        CancelHandleCS.Take();
        if (Canceled)
                {
                CancelHandleCS.Release();
                TERROR(("%s: Client was Canceled",ProcName));
                return false;
                }
        bool Status = XCPClientIOMain::PostXCPOutput(XCPOutput);
        CancelHandleCS.Release();
        return Status;
        }

bool XCPClientMain::PostReference(void **UserPtr)
        {
        static const char *ProcName = "XCPClientMain::PostReference";
        CancelHandleCS.Take();
        if (Canceled)
                {
                CancelHandleCS.Release();
                TERROR(("%s: Client was Canceled",ProcName));
                return false;
                }
        bool Status = XCPClientIOMain::PostReference(UserPtr);
        CancelHandleCS.Release();
        return Status;
        }

int XCPClientMain::Run(const char *Command)
        {
        static const char *ProcName = "XCPClientMain::Run";
        // ---------------
        // ---- Spawn ----
        // ---------------
        CancelHandleCS.Take();
        if (Canceled)
                {
                CancelHandleCS.Release();
                TDEBUG(("%s: CANCELED via Cancel()",ProcName));
                SignalRunning(false);
                return -1;
                }
        FILE *inFile = NULL;
        FILE *outFile = NULL;
        FILE *errFile = NULL;
        TDEBUG(("%s: %s",ProcName,Command));
        bool Status = popenNTioe(Command,"r",&inFile,&outFile,&errFile);
        if (!Status || !inFile || !outFile || !errFile)
                {
                TERROR(("%s: popenNT(%s) Failed errno=%d",ProcName,
                                                          Command,
                                                          errno));
                CancelHandleCS.Release();
                SignalRunning(false);
                return -1;
                }
        pidServer = popenNT_getpid(outFile);
        CancelHandle = outFile;
        CancelHandleCS.Release();
        // --------------------------------------
        // ---- Begin Output / Error Reading ----
        // --------------------------------------
        int rc = -1;
        XCPClientIOReader OutReader(*this);
        XCPClientErrReader ErrReader(*this);
        bool Reading = OutReader.Start(outFile);
        if (!Reading)
                {
                TERROR(("%s: Unable to Start XCPClientIOReader",ProcName));
                }
        else    {
                Reading = ErrReader.Start(errFile);
                if (!Reading)
                        {
                        TERROR(("%s: Unable to Start XCPClientErrReader",
                                ProcName));
                        }
                }
        // ---------------------------
        // ---- Main Message Loop ----
        // ---------------------------
        // -----------------------------------------------------------
        // Release blocking Start() -- We're ready to accept messages.
        // -----------------------------------------------------------
        SignalRunning(Reading);
        if (Reading) rc = XCPClientIOMain::Run(inFile);
        // -------------------------------------------------
        // Closing the child's stdin is the shutdown signal.
        // -------------------------------------------------
        fclose(inFile);
        unsigned long GraceMS = Reading ? XCPClient_ShutdownGraceMS : 0;
        int Ended = pwaitNT(outFile,GraceMS);   // -1=error 0=running 1=ended
        if (Ended != 1)
                {
                if (Ended == 0 && Reading)
                        {
                        TERROR(("%s: Child ignored EOF after %lu ms",
                                ProcName,
                                GraceMS));
                        }
                pabortNT(outFile);      // pwaitNT() logs its own errors.
                }
        OutReader.Wait();
        ErrReader.Wait();
        // -----------------
        // ---- End I/O ----
        // -----------------
        TINFO(("%s: Server [%ld] is Shutdown",ProcName,pidServer));
        CancelHandleCS.Take();
        CancelHandle = NULL;
        pidServer = -1;
        CancelHandleCS.Release();
        pcloseNT(outFile);
        if (Canceled)
                {
                TERROR(("%s: CANCELED via Cancel()",ProcName));
                }
        return rc;
        }

bool XCPClientMain::Cancel()
        {
        static const char *ProcName = "XCPClientIOMain::Cancel";
        bool Status = true;
        CancelHandleCS.Take();
        Canceled = true;
        if (CancelHandle)
                {
                TDEBUG(("%s: Canceling [%ld] via pabortNT(0x%p)",
                        ProcName,
                        getID(),
                        CancelHandle));
                Status = pabortNT(CancelHandle);
                }
        CancelHandleCS.Release();
        return Status;
        }

void XCPClientMain::Reset()
        {
        Stop(true);
        CancelHandleCS.Take();
        pidServer = -1;
        Canceled = false;
        CancelHandle = NULL;
        Running = false;
        CancelHandleCS.Release();
        return;
        }

bool XCPClientMain::Stop(bool WaitToEnd)
        {
        // ---------------------------------------------------------------
        // Orderly: ending the loop lets Run() fclose the child's stdin,
        // so it sees EOF and exits on its own. Cancel() is for a genuine
        // abort -- which is what makes its pabortNT() an ERROR.
        // ---------------------------------------------------------------
        return XCPClientIOMain::Stop(WaitToEnd);
        }

// ***************************************************************************
// **** XCPClientThread ******************************************************
// ***************************************************************************

int XCPClientThread::ThreadMain() 
        {
        return Run(ServerCMD.GetString());
        }

bool XCPClientThread::Start()
        {
        static const char *ProcName = "XCPClientThread::Start";
        if (IsStarted()) return IsRunning();
        if (!ASThread::Start())
                {
                TERROR(("%s: Unable to Start Client Thread",ProcName));
                return false;
                }
        bool Started = WaitRunning();
        if (Started)
                {
                TINFO(("%s: Server [%ld] is Operational",ProcName,getID()));
                }
        else    {
                TERROR(("%s: Client Thread started without a Server",
                        ProcName));
                }
        return Started;
        }

bool XCPClientThread::Stop(bool WaitToEnd)
        {
        bool Status;
        if (IsCurrentThread()) Status = MessageQueueLoop::Stop();
        else Status = XCPClientMain::Stop(WaitToEnd);
        return Status;
        }

// ***************************************************************************
// ******************************* End of File *******************************
// ***************************************************************************
