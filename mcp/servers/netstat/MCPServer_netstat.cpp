// MCP Server showing Asynchronous Netstat: MCPServer_netstat.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MCPServer_netstat.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Demonstrates MCPServerAsync (MCPAsyncCore.h): a work item whose
//      BeginHandler() runs a command in a child process on a worker thread
//      ('netstat', or PowerShell on Windows) and AbortWork() terminates
//      that child when the client cancels.
//
//      Tool: netstat { "resolve": <bool, optional> }
//
//              Lists the remote hosts with ESTABLISHED TCP connections.
//
//              resolve=false (default) reports numeric addresses.
//              resolve=true resolves host names; this can take a long
//              time and is the case where cancellation matters.
//
//      Link: $(ASYNC_OBJS) + $(COMMONC_OBJS) (popenNT) in the Makefiles.
//
//      Platforms: Linux ("netstat -tn") and AIX ("netstat -n"): the
//                 foreign address is the field before the state
//                 ESTABLISHED.
//
//                 Windows (PowerShell Get-NetTCPConnection): netstat prints
//                 the state in the local language (e.g. German
//                 HERGESTELLT); the cmdlet's -State filter does not. It
//                 yields one remote address per line. Windows 8 or later.
//
//      Test prompt: "Which hosts am I currently connected to?"
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <ASThread.h>
#include <LevelTrace.h>
#include <MEMprintf.h>
#include <popenNT.h>

#include <MCPAsyncCore.h>

const char *MCPServer_Name = "MCPServer_netstat";
const char *MCPServer_Version = "0.1.3";
const char *MCPServer_Capabilities = "\"tools\":{}";
bool MCPServer_Asynchronous = true;

// ***************************************************************************
// **** MCP Test *************************************************************
// ***************************************************************************

static const char* MCPInputTestRequest_NetstatCall =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"netstat\","
         "   \"arguments\": { \"resolve\": false }"
         " }"
         "}";

const char* MCPInputTestRequests[] = 
        {
        MCPInputTestRequest_NetstatCall,
        NULL
        };

// ***************************************************************************
// **** Init / Shutdown ******************************************************
// ***************************************************************************

bool MCPServer_OnStartup()
        {
        static const char *ProcName = "MCPServer_OnStartup";
        TINFO(("%s: %s Startup",ProcName,MCPServer_Name));
        return true;
        }

bool MCPServer_OnInitialize(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "MCPServer_OnInitialize";
        TINFO(("%s: %s Initialized",ProcName,MCPServer_Name));
        return true;
        }

bool MCPServer_OnShutdown()
        {
        static const char *ProcName = "MCPServer_OnShutdown";
        TINFO(("%s: %s Shutting Down...",ProcName,MCPServer_Name));
        return true;
        }

// ***************************************************************************
// **** MCP Directoy Handling ************************************************
// ***************************************************************************

const unsigned MCPToolInfoCount = 1;

MCPToolInfo_t MCPToolInfo[] = 
        {
                // ------------------
                // ---- netstat ----
                // ------------------
                {
                // Name
                "netstat",
                // Description
                "Lists the remote hosts with established TCP connections "
                "(netstat; PowerShell on Windows).",
                1,
                // InputSchema {Properties}
                        {
                        "\"resolve\":{\"type\":\"boolean\","
                                     "\"description\":\"Resolve host names "
                                     "(slow). Default false.\"}"
                        },
                // [Required]
                NULL
                }
                // ---------------
                // ---- <end> ----
                // ---------------
        };

// ***************************************************************************
// **** netstat Output Parsing ***********************************************
// ***************************************************************************

#ifndef _WIN32
        #ifndef NetstatEstablished
                #define NetstatEstablished "ESTABLISHED"
        #endif
        #ifdef _AIX
                #define PortSeparator '.'       // AIX: 10.1.1.2.5555 / ::1.1023
        #else
                #define PortSeparator ':'       // Linux: 1.2.3.4:443
        #endif
#endif

#define MaxHosts 256

struct HostCount_t
        {
        char Host[128];
        unsigned Count;
        };

// ---------------------------------------------------------------------------
// ---- SplitFields: Whitespace-separate Line in place -----------------------
// ---------------------------------------------------------------------------

#ifndef _WIN32

static int SplitFields(char *Line, char *Field[], int Max)
        {
        int n = 0;
        char *p = Line;
        while (*p && n < Max)
                {
                while (*p && isspace((unsigned char)*p)) p++;
                if (!*p) break;
                Field[n++] = p;
                while (*p && !isspace((unsigned char)*p)) p++;
                if (*p) *p++ = '\0';
                }
        return n;
        }

// ---------------------------------------------------------------------------
// ---- ForeignHost: Strip the port from a foreign address (in place) --------
// ---------------------------------------------------------------------------
//
//      Linux:   1.2.3.4:443   ::ffff:1.2.3.4:443 (IPv4-mapped)
//      AIX:     1.2.3.4.443   ::1.1023
//
// ---------------------------------------------------------------------------

static const char* ForeignHost(char *Foreign)
        {
        char *Sep = strrchr(Foreign,PortSeparator);
        if (!Sep) return NULL;
        *Sep = '\0';
        char *Host = Foreign;
        if (*Host == '[')
                {
                Host++;
                size_t n = strlen(Host);
                if (n && Host[n-1] == ']') Host[n-1] = '\0';
                }
        if (strncmp(Host,"::ffff:",7) == 0 && strchr(Host + 7,'.'))
                {
                Host += 7;                      // IPv4-mapped IPv6.
                }
        return *Host ? Host : NULL;
        }

#else /* _WIN32 */

// ---------------------------------------------------------------------------
// ---- WindowsHost: One remote address (or name) per output line ------------
// ---------------------------------------------------------------------------
//
//      Trims the line in place. A host never contains whitespace, so
//      multi-word PowerShell diagnostics are rejected (see FirstLine).
//
// ---------------------------------------------------------------------------

static const char* WindowsHost(char *Line)
        {
        while (*Line && isspace((unsigned char)*Line)) Line++;
        size_t n = strlen(Line);
        while (n && isspace((unsigned char)Line[n-1])) Line[--n] = '\0';
        if (!n) return NULL;
        for (size_t i=0; i < n; i++)
                {
                if (isspace((unsigned char)Line[i])) return NULL;
                }
        return Line;
        }

// Get-NetTCPConnection -State is an enum: same on every Windows locale.
// ($a: inside catch, $_ is the error record.)

static const char *PSNumeric = 
        "Get-NetTCPConnection -State Established "
        "-ErrorAction SilentlyContinue "
        "| ForEach-Object { $_.RemoteAddress }";

static const char *PSResolve = 
        "Get-NetTCPConnection -State Established "
        "-ErrorAction SilentlyContinue "
        "| ForEach-Object { $a = $_.RemoteAddress; "
        "try { [System.Net.Dns]::GetHostEntry($a).HostName } "
        "catch { $a } }";

#endif /* _WIN32 */

// ***************************************************************************
// **** MCPAsyncWork_netstat *************************************************
// ***************************************************************************

struct MCPAsyncWork_netstat : MCPAsyncWork_MCPOutput
        {
        // -----------------
        // ---- Input ----
        // -----------------
        bool Resolve;
        // ----------------
        // ---- Output ----
        // ----------------
        HostCount_t Hosts[MaxHosts];
        unsigned nHosts;
        unsigned nConnections;
        unsigned nLines;
        bool Truncated;
        char FirstLine[160];
        // -----------------
        // ---- Methods ----
        // -----------------
        MCPAsyncWork_netstat(JSON_Value &inId, 
                             JSON_Object &inArgs,
                             bool inResolve)
                :
                MCPAsyncWork_MCPOutput(inId,inArgs)
                {
                Resolve = inResolve;
                nHosts = 0;
                nConnections = 0;
                nLines = 0;
                Truncated = false;
                FirstLine[0] = '\0';
                Out = NULL;
                return;
                }
        protected:
                virtual bool BeginHandler();            // Called in thread.
                virtual bool AbortWork();               // Called in main.
        private:
                CriticalSection OutLock;        // Guards Out vs. AbortWork().
                FILE *Out;                      // Child stdout (while alive).
                void AddHost(const char *Host);
                void AddLine(char *Line);
        };

// ---------------------------------------------------------------------------
// ---- AddHost: Tally one connection to Host --------------------------------
// ---------------------------------------------------------------------------

void MCPAsyncWork_netstat::AddHost(const char *Host)
        {
        nConnections++;
        unsigned j = 0;
        while (j < nHosts && strcmp(Hosts[j].Host,Host) != 0) j++;
        if (j < nHosts) Hosts[j].Count++;
        else if (nHosts < MaxHosts)
                {
                strncpy(Hosts[j].Host,Host,sizeof(Hosts[j].Host) - 1);
                Hosts[j].Host[sizeof(Hosts[j].Host) - 1] = '\0';
                Hosts[j].Count = 1;
                nHosts++;
                }
        else Truncated = true;
        return;
        }

// ---------------------------------------------------------------------------
// ---- AddLine: Tally one line of command output ----------------------------
// ---------------------------------------------------------------------------

void MCPAsyncWork_netstat::AddLine(char *Line)
        {
  #ifdef _WIN32
        const char *Host = WindowsHost(Line);
        if (Host) AddHost(Host);
  #else
        char *Field[16];
        int n = SplitFields(Line,Field,16);
        for (int i=1; i < n; i++)               // Foreign precedes State.
                {
                if (strcmp(Field[i],NetstatEstablished) != 0) continue;
                const char *Host = ForeignHost(Field[i-1]);
                if (Host) AddHost(Host);
                break;
                }
  #endif
        return;
        }

// ---------------------------------------------------------------------------
// ---- BeginHandler (worker thread) -----------------------------------------
// ---------------------------------------------------------------------------

bool MCPAsyncWork_netstat::BeginHandler()
        {
        static const char *ProcName = "MCPAsyncWork_netstat::BeginHandler";
        // ----------------------------------------------
        // ---- Cancelled before start: Don't spawn. ----
        // ----------------------------------------------
        if (isCancelled())
                {
                TINFO(("%s: Cancelled before start (id=%s)",
                       ProcName,
                       id2string(get_Id())));
                return false;
                }
        // ---------------------------------------------------------
        // ---- Fixed argv (nothing from the client is executed) ----
        // ---------------------------------------------------------
        const char *argv[6];
        int argc = 0;
  #ifdef _WIN32
        argv[argc++] = "powershell.exe";
        argv[argc++] = "-NoProfile";
        argv[argc++] = "-NonInteractive";
        argv[argc++] = "-Command";
        argv[argc++] = Resolve ? PSResolve : PSNumeric;
  #else
        argv[argc++] = "netstat";
    #ifdef _AIX
        if (!Resolve) argv[argc++] = "-n";
    #else
        argv[argc++] = Resolve ? "-t" : "-tn";
    #endif
  #endif
        argv[argc] = NULL;
        // -----------------------
        // ---- Run netstat. ----
        // -----------------------
        FILE *out = NULL;
        if (!popenNToe(argc,argv,"r",&out,NULL))
                {
                TERROR(("%s: Can't run netstat (id=%s)",
                        ProcName,
                        id2string(get_Id())));
                MCPOutput = FormMCPResponse_ERROR(&get_Id(),
                                                  MCPErrorCode_Exception,
                                                  "Exec Failed",
                                                  "Unable to run netstat");
                return MCPOutput != NULL;
                }
        // ------------------------------------------------------------
        // Publish the stream for AbortWork(). Cancelled between the
        // pre-spawn check and this point (AbortWork() saw no stream)
        // is honoured here.
        // ------------------------------------------------------------
        OutLock.Take();
        Out = out;
        if (isCancelled()) pabortNT(out);
        OutLock.Release();
        // -------------------------------------------------------
        // Read to EOF (child exit or pabortNT() ends the stream).
        // -------------------------------------------------------
        char Line[512];
        while (fgets(Line,sizeof(Line),out))
                {
                if (!nLines++)
                        {
                        strncpy(FirstLine,Line,sizeof(FirstLine) - 1);
                        FirstLine[sizeof(FirstLine) - 1] = '\0';
                        size_t n = strlen(FirstLine);
                        while (n && (FirstLine[n-1] == '\n' ||
                                     FirstLine[n-1] == '\r')) 
                                {
                                FirstLine[--n] = '\0';
                                }
                        }
                AddLine(Line);
                }
        // --------------------------------------------------------
        // Unpublish before closing so AbortWork() never sees a
        // closed stream.
        // --------------------------------------------------------
        OutLock.Take();
        Out = NULL;
        OutLock.Release();
        pcloseNT(out);
        if (isCancelled())
                {
                TINFO(("%s: Cancelled (id=%s)",ProcName,id2string(get_Id())));
                return false;
                }
        // ------------------
        // ---- Results ----
        // ------------------
  #ifndef _WIN32                // PowerShell prints nothing when none.
        if (nLines == 0)
                {
                MCPOutput = FormMCPResponse_ERROR(&get_Id(),
                                                  MCPErrorCode_Exception,
                                                  "No Output",
                                                  "netstat produced no output");
                return MCPOutput != NULL;
                }
  #endif
        MemoryPrintf Canvas;
        if (nConnections == 0)
                {
                if (FirstLine[0])
                        {
                        Canvas.printf("No established connections found. "
                                      "Command output began: %s",FirstLine);
                        }
                else Canvas.printf("No established connections found.");
                }
        else    {
                Canvas.printf("%u established connection(s) to %u host(s):",
                              nConnections,nHosts);
                for (unsigned i=0; i < nHosts; i++)
                        {
                        Canvas.printf("\n%s (%u)",Hosts[i].Host,Hosts[i].Count);
                        }
                if (Truncated)
                        {
                        Canvas.printf("\n... list truncated at %u hosts",
                                      (unsigned)MaxHosts);
                        }
                }
        MCPOutput = FormMCPResponse_Text(&get_Id(),Canvas.GetBuffer());
        return MCPOutput != NULL;
        }

// ---------------------------------------------------------------------------
// ---- AbortWork (main thread) ----------------------------------------------
// ---------------------------------------------------------------------------
//
//      Terminates the netstat child; BeginHandler()'s read then hits EOF
//      and unwinds. CompleteHandler() is skipped by the framework.
//
//      Returns true when there is no child to abort (not yet spawned or
//      already finished): BeginHandler() honours Cancelled itself.
//
// ---------------------------------------------------------------------------

bool MCPAsyncWork_netstat::AbortWork()
        {
        bool Status = true;
        OutLock.Take();
        if (Out) Status = pabortNT(Out);
        OutLock.Release();
        return Status;
        }

// ***************************************************************************
// **** Tool Handlers ********************************************************
// ***************************************************************************

// ---------------------------------------------------------------------------
// ---- MCPtool_netstat ------------------------------------------------------
// ---------------------------------------------------------------------------

char* MCPtool_netstat(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_netstat";
        /* ----- INPUT -----------------------------------------------
        {
          "jsonrpc": "2.0",
          "id": 3,
          "method": "tools/call",
          "params": {
            "name": "netstat",
            "arguments": { "resolve": false }
          }
        }
        ----------------------------------------------------------- */
        char *MCPOutput = NULL;
        bool Resolve = false;
        JSON_Value_Boolean *Flag = Arguments.Find_member_bool("resolve");
        if (Flag) Resolve = Flag->boolean;
        MCPAsyncWork_netstat *WorkItem;
        WorkItem = new MCPAsyncWork_netstat(idRequest,Arguments,Resolve);
        if (WorkItem)
                {
                bool Status = Begin_AsyncWork(&WorkItem);
                if (Status)
                        {
                        MCPOutput = Return_MCPOutput_Pending();
                        }
                else    {
                        if (WorkItem) delete WorkItem;
                        TERROR(("%s: Begin_AsyncWork Failed (id=%s)",
                                ProcName,
                                id2string(idRequest)));
                        MCPOutput = FormMCPResponse_ERROR(&idRequest,
                                          MCPErrorCode_Exception,
                                          "Can't Schedule Work",
                                          "Begin_AsyncWork() Failed");
                        }
                }
        return MCPOutput;
        }

// ---------------------------------------------------------------------------
// ---- Handle_tools_call ----------------------------------------------------
// ---------------------------------------------------------------------------

char* Handle_tools_call(JSON_Value &idRequest,
                        const char *ToolName, 
                        JSON_Object &Arguments)
        {
        char *MCPOutput = NULL;
        if (strcmp(ToolName,"netstat") == 0)  
                {
                MCPOutput = MCPtool_netstat(idRequest,Arguments);
                }
        return MCPOutput;
        }

// ****************************************************************************
// **** Handle_notification (Dormant) *****************************************
// ****************************************************************************

void Handle_notification(MCPInputRequest &MCPRequest)
        {
        static const char *ProcName = "Handle_notification";
        const char *Method = MCPRequest.Get_method();
        TDEBUG(("%s: Ignoring: %s",ProcName,Method));
        return;
        }

// ****************************************************************************
// **** Handle_sampling_response (Dormant) ***********************************
// ****************************************************************************

char* Handle_sampling_response(MCPInputRequest &MCPRequest)
        {
        return NULL;
        }

// ****************************************************************************
// **** Handle_tools_call_resume (Dormant) ************************************
// ****************************************************************************

char* Handle_tools_call_resume(MCPInputRequest &MCPRequest)
        {
        return NULL;
        }

// ****************************************************************************
// **** Handle_notify_action (Dormant) ****************************************
// ****************************************************************************

char* Handle_notify_action(void *UserPtr)
        {
        return NULL;
        }

void Discard_notify_action(void *UserPtr)
        {
        return;
        }

// ****************************************************************************
// **** Handle_completion_complete (Dormant) *********************************
// ****************************************************************************

char* Handle_completion_complete(JSON_Value &idRequest,
                                 const char *RefType,
                                 const char *RefName,
                                 const char *ArgName,
                                 const char *ArgValue)
        {
        return NULL;
        }

// ****************************************************************************
// **** Handle_prompts_get (Dormant) ******************************************
// ****************************************************************************

const unsigned MCPPromptInfoCount = 0;

MCPPromptInfo_t MCPPromptInfo[] = { {0} };

char* Handle_prompts_get(JSON_Value &idRequest,
                         const char *PromptName, 
                         JSON_Object &Arguments)
        {
        return NULL;
        }

// ****************************************************************************
// **** Resources (Dormant) ***************************************************
// ****************************************************************************

const unsigned MCPResourceInfoCount = 0;

MCPResourceInfo_t MCPResourceInfo[] = { {0} };

const unsigned MCPResourceTemplateInfoCount = 0;

MCPResourceTemplateInfo_t MCPResourceTemplateInfo[] = { {0} };

char* Handle_resources_read(JSON_Value &idRequest,
                            const char *URI)
        {
        return NULL;
        }

char* Handle_resources_subscribe(JSON_Value &idRequest,
                                 const char *URI)
        {
        return NULL;
        }

char* Handle_resources_unsubscribe(JSON_Value &idRequest,
                                   const char *URI)
        {
        return NULL;
        }

// ***************************************************************************
// **** Handle_method (Dormant) *********************************************
// ***************************************************************************

char* Handle_method(MCPInputRequest &MCPRequest)
        {
        return NULL;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
