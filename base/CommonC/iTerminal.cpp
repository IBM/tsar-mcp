// Generic Terminal for iSeries: iTerminal.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: iTerminal.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2002 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Allows spawned thread jobs to be interactive.
//

#include <qlirlibd.h>
#include <qp0z1170.h>
#include <qp0ztrml.h>
#include <qusec.h>

#include <errno.h>
#include <stdio.h>
#include <iTerminal.h>
#include <LevelTrace.h>

#define AS4_OBJ_LEN 10
#define ERROR_MEM_SIZE 256

extern char **environ;

#pragma pack(1)

struct Qus_Vlen_Rec_ASP : public Qus_Vlen_Rec_4
        {
        char ASPName[AS4_OBJ_LEN];
        };

#pragma pack(pop)

static void padcpy(char *Dest, 
                   const char *Source, 
                   size_t DestLen, 
                   size_t CopyLen)
        {
        for (size_t i=0; i < DestLen; i++)
                {
                if (CopyLen && *Source)
                        {
                        *Dest++ = *Source++;
                        CopyLen--;
                        }
                else *Dest++ = ' ';
                }
        return;
        }

static void padcpy(char *Dest, const char *Source, size_t DestLen)
        {
        for (size_t i=0; i < DestLen; i++)
                {
                if (*Source) *Dest++ = *Source++;
                else *Dest++ = ' ';
                }
        return;
        }

static char* rtrim(char *String)                // Modifies String!
        {
        if (!String) return NULL;
        char *pos = String + strlen(String);
        while (pos != String && *(pos-1) == ' ') pos--;
        *pos = '\0';
        return String;
        }

static bool SplitLibProg(char Lib[11], char Prog[11], const char *LibProg)
        {
        int i;
        for (i=0; i < AS4_OBJ_LEN && *LibProg && *LibProg != '/'; i++) 
                {
                Lib[i] = *LibProg++;
                }
        Lib[i] = '\0';
        if (*LibProg == '/') LibProg++;
        for (i=0; i < AS4_OBJ_LEN && *LibProg && *LibProg != '/'; i++) 
                {
                Prog[i] = *LibProg++;
                }
        Prog[i] = '\0';
        return !*LibProg;
        }

static bool GetLIBASP(const char *Library, char ASPName[AS4_OBJ_LEN+1])
        {
        static const char *ProcName = "GetLIBASP";
        if (!Library) return false;
        char ErrorMemory[ERROR_MEM_SIZE];
        Qus_EC_t* ErrorCode = (Qus_EC_t*)ErrorMemory;
        ErrorCode->Bytes_Provided = sizeof(ErrorMemory);
        ErrorCode->Bytes_Available = 0;
        ErrorCode->Reserved = 0;

        char LibraryName[AS4_OBJ_LEN];          
        padcpy(LibraryName,Library,sizeof(LibraryName));
        size_t TotalLength = sizeof(Qli_Rlibd_Rtn) + sizeof(Qus_Vlen_Rec_ASP);
        Qli_Rlibd_Rtn *LibInfo = (Qli_Rlibd_Rtn *)malloc(TotalLength);
        if (!LibInfo)
                {
                TERROR(("%s: Alloc of LibInfo Failed",ProcName));
                return false;
                }
        unsigned AttributesInfo[] =
                {
                1,                      // Number of Attributes.
                8                       // 8 = ASP device name.
                };
        QLIRLIBD(LibInfo,
                 TotalLength,
                 LibraryName,
                 AttributesInfo,
                 ErrorCode);
        if (ErrorCode->Bytes_Available)
                {
                TERROR(("%s: QLIRLIBD(%s) Failed: %.7s",
                        ProcName,
                        Library,
                        ErrorCode->Exception_Id));
                free(LibInfo);
                return false;
                }
        if (LibInfo->Vlen_Records_Returned < 1)
                {
                TERROR(("%s: ASPName not Returned",ProcName));
                free(LibInfo);
                return false;
                }
        char *VarInfo = (char *)LibInfo + sizeof(Qli_Rlibd_Rtn);
        Qus_Vlen_Rec_ASP *ASPInfo = (Qus_Vlen_Rec_ASP *)VarInfo;
        if (ASPInfo->Length_Data > AS4_OBJ_LEN)
                {
                TERROR(("%s: Invalid ASPName Length",ProcName));
                free(LibInfo);
                return false;
                }
        padcpy(ASPName,ASPInfo->ASPName,AS4_OBJ_LEN,ASPInfo->Length_Data);
        ASPName[ASPInfo->Length_Data] = '\0';
        rtrim(ASPName);
        free(LibInfo);
        return true;
        }

static char* LibExe2PathExe(char FilenameBuffer[256], const char *Program)
        {
        if (*Program == '/') 
                {
                strncpy(FilenameBuffer,Program,256);
                }
        else    {
                const char *Format = "/QSYS.LIB/%s.LIB/%s.PGM";
                const char *FormatASP = "/%s/QSYS.LIB/%s.LIB/%s.PGM";
                char Lib[11];
                char Prog[11];
                char ASPName[AS4_OBJ_LEN+1];
                SplitLibProg(Lib,Prog,Program);
                bool Status = GetLIBASP(Lib,ASPName);
                if (Status && ASPName[0] && ASPName[0] != '*')
                        {
                        sprintf(FilenameBuffer,FormatASP,ASPName,Lib,Prog);
                        }
                else sprintf(FilenameBuffer,Format,Lib,Prog);
                }
        FilenameBuffer[255] = '\0';
        return FilenameBuffer;
        }

// **************************************************************************
// **** iSeriesTerminal *****************************************************
// **************************************************************************

iSeriesTerminal::iSeriesTerminal(const char *title)
        {
        Started = false;
        EnvCount = 0;
        Environment = NULL;
        Title = title ? title : "iSeriesTerminal";
        ResetDefaultEnvironment();
        putenv("QIBM_USE_DESCRIPTOR_STDIO=Y");
        memset(&handle,0,sizeof(Qp0z_Terminal_T));
        memset(&attr,0,sizeof(Qp0z_Terminal_Attr_T));
        return;
        }

iSeriesTerminal::~iSeriesTerminal()
        {
        End();
        ClearEnvironment();
        return;
        }

void iSeriesTerminal::End()
        {
        if (Started) Qp0zEndTerminal(handle);
        memset(&handle,0,sizeof(Qp0z_Terminal_T));
        Started = false;
        return;
        }

void iSeriesTerminal::ClearEnvironment()
        {
        while (EnvCount--) free((*Environment)[EnvCount]);
        free(Environment);
        Environment = NULL;
        return;
        }

int iSeriesTerminal::putenv(const char *EnvironmentString)
        {
        if (!EnvironmentString) return EINVAL;
        const char *Equal = strchr(EnvironmentString,'=');
        if (!Equal) return EINVAL;
        size_t NameLen = Equal - EnvironmentString;
        for (unsigned i=0; i < EnvCount; i++)
                {
                const char *Current = (*Environment)[i];
                if (strncmp(Current,EnvironmentString,NameLen) == 0)
                        {
                        free((*Environment)[i]);
                        (*Environment)[i] = strdup(EnvironmentString);
                        return 0;
                        }
                }
        char* (*Temp)[] = (char* (*)[])
                          realloc(Environment,
                                  (++EnvCount + 1) * sizeof(char *));
        if (!Temp) return -1;
        Environment = Temp;
        (*Environment)[EnvCount-1] = strdup(EnvironmentString);
        (*Environment)[EnvCount] = NULL;
        return 0;
        }

bool iSeriesTerminal::ResetDefaultEnvironment()
        {
        Qp0zInitEnv();
        ClearEnvironment();
        for (EnvCount=0; environ[EnvCount]; EnvCount++);

        Environment = (char* (*)[])calloc((EnvCount + 1),sizeof(char *));
        if (!Environment) return false;

        for (int i=0; i < EnvCount; i++)
                {
                (*Environment)[i] = strdup(environ[i]);
                if (!(*Environment)[i]) return NULL;
                }
        return true;
        }

int iSeriesTerminal::Run()
        {
        static const char *ProcName = "iSeriesTerminal::Run";
        if (!Started)
                {
                TERROR(("%s: Not Started",ProcName));
                return -1;
                }
        int rc = Qp0zRunTerminal(handle);
        if (rc)
                {
                TERROR(("%s: Qp0zRunTerminal Failed (%u)",ProcName,errno));
                }
        return rc;
        }

bool iSeriesTerminal::Start(int argc, 
                            const char *argv[], 
                            const char *argv0,          // Replace the first.
                            const char *argvN)          // One additional.
        {
        static const char *ProcName = "iSeriesTerminal::Start";
        int rc;
        char Program[256];
        const char *(*ArgvZ)[];
        attr.Buffer_Size = 8196;
        attr.Inherit.flags = SPAWN_SETTHREAD_NP;
        attr.Inherit.pgroup = SPAWN_NEWPGROUP;
        attr.Return_Exit_Status = 'N';
        attr.Send_End_Msg = 'N';
        attr.Title = (char *)Title; 
        attr.Cmd_Key_Line1 = "F3=Exit F9=Retrieve";
        attr.Cmd_Key_Line2 = "F17=Top F18=Bottom";
        ArgvZ = (const char* (*)[])malloc((argc + 2) * sizeof(char *));
        if (!ArgvZ) return false;
        for (int i=0; i < argc; i++) (*ArgvZ)[i] = argv[i];
        if (argv0) (*ArgvZ)[0] = argv0;
        else    {
                LibExe2PathExe(Program,argv[0]);
                (*ArgvZ)[0] = Program;
                }
        if (argvN) (*ArgvZ)[argc++] = argvN;
        (*ArgvZ)[argc] = NULL;
        rc = Qp0zStartTerminal(&handle,(char **)*ArgvZ,*Environment,attr);
        if (rc == 0) Started = true;
        else    {
                TERROR(("%s: Qp0zStartTerminal Failed (%u)",ProcName,errno));
                }
        free(ArgvZ);
        return Started;
        }

bool iSeriesTerminal::Start(int argc, 
                            char *argv[], 
                            const char *argv0,          // Replace the first.
                            const char *argvN)          // One additional.
        {
        return Start(argc,(const char **)argv,argv0,argvN);
        }

// **************************************************************************
// **** isatty **************************************************************
// **************************************************************************

extern "c" int isatty(int handle)
        {
        const char *UseDescriptorSTDIO;
        UseDescriptorSTDIO = getenv("QIBM_USE_DESCRIPTOR_STDIO");
        if (!UseDescriptorSTDIO || *UseDescriptorSTDIO == 'N')
                {
                return 1;       // Standard 5250 Console.
                }
        return Qp0zIsATerminal(handle);
        }

// **************************************************************************
// ****************************** End of File *******************************
// **************************************************************************
