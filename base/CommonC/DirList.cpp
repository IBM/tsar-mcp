// DirList.cpp - List of Files or Paths and File Finding
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: DirList.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2002 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
        #include <io.h>
        #include <direct.h>
        #include <windows.h>
#else
        #include <dirent.h>
        #include <limits.h>
        #include <unistd.h>
        #include <sys/types.h>
#endif

#ifdef _AIX
        #include <procinfo.h>
        extern "C" int getargs (procentry64*, int, char*, int);
#endif

#ifdef __BORLANDC__
        struct _finddata_t : public ffblk
                {
                char (&name)[256];
                _finddata_t() : name(ff_name) {}
                };
        #define _findfirst(a,b) findfirst(a,b,0)
        #define _findnext(a,b) findnext(b)
        #define _findclose(a) ((void)0)
#endif

#include <LevelTrace.h>
#include <MiniGREP.h>
#include <DirList.h>

#ifdef _WIN32
        #define PATHDELIM '\\'
        #define PATHSEP ';'
        #ifndef __BORLANDC__
                #define access _access
        #endif
#else
        #define PATHDELIM '/'
        #define PATHSEP ':'
#endif

static bool EditString(char **Source, const char *Find, const char *Replace)
        {
        if (!Source || !*Source || !Find || !Replace) return true;
        bool Found;
        const char *Start, *End;
        size_t ReplaceLen = strlen(Replace);
        Start = *Source;
        Found = FindWildCardMatch(Start,Find,&Start,&End);
        while (Found)
                {
                size_t SrcLength = Start - *Source;
                size_t EndLength = strlen(End);
                size_t TotalLength = SrcLength + ReplaceLen + EndLength + 1;
                char *Dest = (char *)malloc(TotalLength);
                if (!Dest) return false;
                memcpy(Dest,*Source,SrcLength);
                memcpy(Dest+SrcLength,Replace,ReplaceLen);
                char *EndPos = Dest + SrcLength + ReplaceLen;
                strcpy(EndPos,End);
                free(*Source);
                *Source = Dest;
                Start = EndPos;
                Found = FindWildCardMatch(Start,Find,&Start,&End);
                }
        return true;
        }

DirectoryItem::DirectoryItem(DirectoryItem &Item) 
        {
        Directory = NULL; 
        *this = Item;
        return;
        }

DirectoryItem::~DirectoryItem() 
        {
        if (Directory) free(Directory);
        return;
        }

char* DirectoryItem::_AquireDirectory()
        {
        char *Hold = Directory;
        Directory = NULL;
        return Hold;
        }

void DirectoryItem::_AssignDirectory(char *Dir)
        {
        if (Directory) free(Directory);
        Directory = Dir;
        return;
        }

bool DirectoryItem::CheckExists()
        {
        if (!Directory) return false;
        return access(Directory,0) == 0;
        }

bool DirectoryItem::ConstructPath(const char *Path, const char *Filename)
        {
        size_t PathLength = strlen(Path);
        size_t FileLength = strlen(Filename);
        Directory = (char *)realloc(Directory,PathLength+FileLength+2);
        if (!Directory) return false;
        strcpy(Directory,Path);
        bool PEndDelim = PathLength && Directory[PathLength-1] == PATHDELIM;
        bool FBeginDelim = Filename[0] == PATHDELIM;
        if (PEndDelim && FBeginDelim) PathLength--;
        else if (PathLength && !PEndDelim && !FBeginDelim)  
                {
                Directory[PathLength] = PATHDELIM;
                PathLength++;
                }
        strcpy(Directory + PathLength, Filename);
        return Directory != NULL;
        }

bool DirectoryItem::ConstructPath(const DirectoryItem &Item, const char *File)
        {
        return ConstructPath(Item.Directory,File);
        }

bool DirectoryItem::Edit(const char *Find, const char *Replace)
        {
        return EditString(&Directory,Find,Replace);
        }

#ifdef _WIN32

char* DirectoryItem::FindFirst(const char *ToFind,      // Caller is
                               DICheckFn_t CheckFn,     // responsible for
                               void *UserPtr)           // freeing returned.
        {
        static const char *ProcName = "FindFirst";
        DirectoryItem Query;
        bool Status = Query.ConstructPath(Directory,ToFind);
        if (!Status)
                {
                TERROR(("%s: ConstructPath() Failed",ProcName));
                return NULL;
                }
        char *Found = NULL;
        struct _finddata_t DirEntry;
        long DStream = _findfirst(Query.GetDirectory(),&DirEntry);
        if (DStream != -1)
                {
                int rc = 0;
                do      {
                        if (!CheckFn || 
                            (*CheckFn)(DirEntry.name,UserPtr))
                                {
                                Found = strdup(DirEntry.name);
                                break;
                                }
                        rc = _findnext(DStream,&DirEntry);
                        } while (rc == 0);
                _findclose(DStream);
                }
        return Found;
        }

#else

char* DirectoryItem::FindFirst(const char *ToFind,      // Caller is
                               DICheckFn_t CheckFn,     // responsible for
                               void *UserPtr)           // freeing returned.
        {
        static const char *ProcName = "FindFirst";
        DIR *hDir = opendir(Directory);
        if (!hDir)
                {
                TERROR(("%s: Can't Open Directory: %s",ProcName,Directory));
                return NULL;
                }
        long NameMax = pathconf(Directory,_PC_NAME_MAX);
        if (NameMax == -1)
                {
                TERROR(("%s: pathconf(%s) Failed",ProcName,Directory));
                closedir(hDir);
                return NULL;
                }
        struct dirent *Entry = (dirent *)malloc(sizeof(dirent) + NameMax);
        if (!Entry)
                {
                TERROR(("%s: Unable to Alloc",ProcName));
                closedir(hDir);
                return NULL;
                }
        char *Found = NULL;
        struct dirent *Result = NULL; 
        int rc = 0;
        do      {
                rc = readdir_r(hDir,Entry,&Result);
                if (Result == NULL) rc = EBADF;
                if (rc == 0)
                        {
                        if (IsWildCardMatch(Result->d_name,ToFind) &&
                            (!CheckFn || 
                             (*CheckFn)(Result->d_name,UserPtr)))
                                {
                                Found = strdup(Result->d_name);
                                break;
                                }
                        }
                else if (rc != EBADF)
                        {
                        TERROR(("%s: readdir() Failed: %d",ProcName,rc));
                        }
                } while (rc == 0);
        closedir(hDir);
        free(Entry);
        return Found;
        }

#endif 

char* DirectoryItem::FindFirst(const char *ToFind)      // Caller is 
        {                                               // responsible for
        return FindFirst(ToFind,NULL,NULL);             // freeing returned.
        }

bool DirectoryItem::SetDirectory(const char *Dir)
        {
        Directory = (char *)realloc(Directory,strlen(Dir) + 1);
        if (Directory) strcpy(Directory,Dir);
        return Directory != NULL;
        }

DirectoryItem& DirectoryItem::operator = (DirectoryItem &Item)
        {
        if (&Item == this) return *this;
        SetDirectory(Item.Directory);
        return *this;
        }

bool DirectoryList::AddDirectory(const char *Directory)
        {
        // ----------------------------------
        // ---- Strip leading quote --------
        // ----------------------------------
        if (*Directory == '"' || *Directory == '\'') Directory++;
        // ----------------------------------
        // ---- Find end of this entry -----
        // ----------------------------------
        const char *Stop = Directory;
        while (*Stop && *Stop != PATHSEP) Stop++;
        while (Stop != Directory && isspace(*(Stop-1))) Stop--;
        if (Stop != Directory && (*(Stop-1) == '"' || *(Stop-1) == '\'')) Stop--;
        size_t Length = Stop - Directory ;
        if (!Length) return false;
        // ---------------------------
        // ---- Add to the Bottom ----
        // ---------------------------
        DirectoryItem *Item = new DirectoryItem;
        if (!Item) return false;
        Item->Directory = (char *)malloc(Length + 1);
        if (!Item->Directory) return false;
        memcpy(Item->Directory,Directory,Length);
        Item->Directory[Length] = '\0';
        AddBottom(Item);
        return true;
        }

bool DirectoryList::AddDirectory(DirectoryList &List, DL_Position_t Pos)
        {
        DirectoryItem *Current;
        if (Pos == DL_After) Current = List.GetFirst();
        else Current = List.GetLast();
        while (Current)
                {
                List.Remove(Current);
                if (Pos == DL_After) 
                        {
                        AddBottom(Current);
                        Current = List.GetFirst();
                        }
                else    {
                        AddTop(Current);
                        Current = List.GetLast();
                        }
                }
        return true;
        }

bool DirectoryList::AddPath(const char *Path, DL_Position_t Pos)
        {
        if (!Path || !*Path) return true;
        DirectoryList AppendList;
        const char *Directory = GetFirstDir(Path);
        while (Directory)
                {
                AppendList.AddDirectory(Directory);
                Directory = GetNextDir(Directory);
                }
        AddDirectory(AppendList,Pos);
        return true;
        }

bool DirectoryList::AddPATHEnv(DL_Position_t Pos)
        {
        bool rc = true;
        const char *PathEnv = getenv("PATH");
        if (PathEnv) rc = AddPath(PathEnv,Pos);
        return rc;
        }

bool DirectoryList::AddLIBPATHEnv(DL_Position_t Pos)
        {
        bool rc = true;
        const char *PathEnv = getenv("LIBPATH");
        if (PathEnv) rc = AddPath(PathEnv,Pos);
        return rc;
        }

bool DirectoryList::AddCWD(DL_Position_t Pos)
        {
        int rc = ERANGE;
        char *CWD = NULL;
        size_t PathLength = 128;
        while (rc == ERANGE)
                {
                PathLength *= 2;
                CWD = (char *)realloc(CWD,PathLength);
                if (getcwd(CWD,PathLength)) 
                        {
                        rc = AddPath(CWD,Pos) ? 0 : -1;
                        }
                else rc = errno;
                }
        if (CWD) free(CWD);
        return rc == 0;
        }

#if defined(_WIN32)

bool DirectoryList::AddApplicationDir(DL_Position_t Pos)
        {
        char ModulePath[MAX_PATH+1];
        size_t ModulePathLen = GetModuleFileName(NULL,ModulePath,MAX_PATH);
        if (!ModulePathLen) return false;
        char *Program = ModulePath + ModulePathLen;
        while (Program != ModulePath && *Program != '\\') Program--;
        if (*Program == '\\') *Program++ = '\0';
        return AddPath(ModulePath,Pos);
        }

#elif defined(__OS400_TGTVRM__)

bool DirectoryList::AddApplicationDir(DL_Position_t)
        {
        return true;
        }

#elif defined(_AIX)

bool DirectoryList::AddApplicationDir(DL_Position_t Pos)
        {
        int rc;
        pid_t Pid = getpid();
        procentry64 ProcessInfo;
        char AppDir[PATH_MAX+1];
        char ArgBuffer[PATH_MAX+1];

        ProcessInfo.pi_pid = Pid;
        rc = getargs(&ProcessInfo,sizeof(ProcessInfo),ArgBuffer,PATH_MAX);
        if (rc) return false;
        
        if (strchr(ArgBuffer,PATHDELIM))        // Path specified:
                {
                if (!realpath(ArgBuffer,AppDir)) 
                        {
                        return false;
                        }
                AppDir[PATH_MAX] = '\0';
                }
        else    {                               // Must search PATH:
                DirectoryList Path;
                DirectoryItem DirItem;
                Path.AddPATHEnv(DL_After);
                if (Path.FindFile(ArgBuffer,&DirItem))
                        {
                        if (!realpath(DirItem.GetFile(),AppDir)) 
                                {
                                return false;
                                }
                        AppDir[PATH_MAX] = '\0';
                        }
                else return false;
                }
        char *End = AppDir + strlen(AppDir);
        while (End != AppDir && *End != PATHDELIM) End--;
        if (*End == PATHDELIM) *End = '\0';
        return AddPath(AppDir,Pos);
        }

#else /* Linux */

bool DirectoryList::AddApplicationDir(DL_Position_t Pos)
        {
        int rc;
        char AppDir[PATH_MAX+1];
        if (!realpath("/proc/self/exe",AppDir))
                {
                return false;
                }
        AppDir[PATH_MAX] = '\0';
        char *End = AppDir + strlen(AppDir);
        while (End != AppDir && *End != PATHDELIM) End--;
        if (*End == PATHDELIM) *End = '\0';
        return AddPath(AppDir,Pos);
        }

#endif /* Linux */

bool DirectoryList::Edit(const char *Find, const char *Replace)
        {
        bool Status = true;
        DirectoryItem *Current = GetFirst();
        while (Current && Status)
                {
                Status = Current->Edit(Find,Replace);
                Current = GetNext(Current);
                }
        return Status;
        }

bool DirectoryList::FindFile(const char *Filename, DirectoryItem *FoundPath)
        {
        DirectoryItem SourceFile;
        if (!Filename) return false;
        DirectoryItem *Current = GetFirst();
        if (Filename[0] == PATHDELIM)                   // Absolute path.
                {
                SourceFile.SetDirectory(Filename);
                if (SourceFile.CheckExists()) goto DL_FF_Found;
                return false;
                }
        while (Current)                                 // Relitive path.
                {
                SourceFile.ConstructPath(*Current,Filename);
                if (SourceFile.CheckExists())
                        {
 DL_FF_Found:           if (FoundPath)
                                {
                                char *Path = SourceFile._AquireDirectory();
                                FoundPath->_AssignDirectory(Path);
                                }
                        return true;
                        }
                Current = GetNext(Current);
                }
        return false;
        }

const char* DirectoryList::GetFirstDir(const char *Paths)
        {
        while (*Paths && isspace(*Paths)) Paths++;
        return *Paths ? Paths : NULL;
        }

const char* DirectoryList::GetNextDir(const char *Paths)
        {
        while (*Paths && *Paths != PATHSEP) Paths++;
        if (!*Paths) return NULL;
        Paths++;
        while (*Paths && isspace(*Paths)) Paths++;
        return Paths;
        }

bool DirectoryList::SetPath(const char *Path)
        {
        ChainSaw();
        return AddPath(Path,DL_After);
        }

// **************************************************************************
// **************************** End of File *********************************
// **************************************************************************
