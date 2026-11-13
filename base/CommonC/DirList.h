// DirList.h - List of Files or Paths and File Finding
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: DirList.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2001 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Directory_List

        #define __Directory_List

#include <stddef.h>
#include <LinkList.h>

enum DL_Position_t {DL_Before, DL_After};

typedef bool (*DICheckFn_t)(const char *DirName, void *UserPtr);

class DirectoryItem : public Link
        {
        private:
                char *Directory;
                friend class DirectoryList;
        public:
                DirectoryItem() {Directory = NULL;}
                DirectoryItem(DirectoryItem &Item);
                ~DirectoryItem();
                char* _AquireDirectory();
                void _AssignDirectory(char *Directory);
                bool CheckExists();
                bool ConstructPath(const char *Path, const char *Filename);
                bool ConstructPath(const DirectoryItem &Item, 
                                   const char *Filename);
                const char* GetFile() {return Directory;}
                const char* GetDirectory() {return Directory;}
                bool Edit(const char *Find, const char *Replace);
                char* FindFirst(const char *ToFind);
                char* FindFirst(const char *ToFind, 
                                DICheckFn_t CheckFn, 
                                void *UserPtr);
                bool SetDirectory(const char *Directory);
                DirectoryItem& operator = (DirectoryItem &Item);
        };

DefineLinkList(_DirectoryList,DirectoryItem);

class DirectoryList : public _DirectoryList
        {
        protected:
                bool AddDirectory(const char *Directory);
                bool AddDirectory(const char *Directory, size_t Length);
                bool AddDirectory(DirectoryList &List, DL_Position_t Pos);
        public:
                ~DirectoryList() {SetAutoCleanup(true);}
                bool AddApplicationDir(DL_Position_t Pos);
                bool AddCWD(DL_Position_t Pos);
                bool AddPath(const char *Path, DL_Position_t Pos);
                bool AddPATHEnv(DL_Position_t Pos);
                bool AddLIBPATHEnv(DL_Position_t Pos);
                bool FindFile(const char *Filename, DirectoryItem *Found);
                void ClearPath() {ChainSaw();}
                bool Edit(const char *Find, const char *Replace);
                bool SetPath(const char *Path);
        };

// ***************************************************************************
// **** ExpandDirectory (Caller must free result) ****************************
// ***************************************************************************

char* ExpandDirectory(const char *Directory, size_t DirectoryLength);

/* ****************************************************************************
 Notes:

     I. DirectoryItem

                Container for one directory path, or file in a path.

    II. DirectoryList:

             A. Methods:

                      - AddPath(const char *Path, DL_Position_t Pos)

                                Adds a single 'Path' or list of paths
                                separated by the OS platform's standard
                                path delimiter. 

                                'Pos' specifies whether 'Path' should
                                be added before (DL_Before) or after 
                                (DL_After) in the current search list.

                                Environment variables and '~' in 'Path'
                                are resolved with ExpandDirectory().

                                Example: 
                                
                                        AddPath(getenv("PATH"), DL_After);

                      - AddApplicationDir(DL_Position_t Pos)

                                Adds the directory where the current
                                running program resides (supported on
                                Windows, AIX, and Linux).

                      - AddPATHEnv(DL_Position_t Pos)

                                Adds the current PATH environment. Same
                                as the example above.

                      - AddLIBPATHEnv(DL_Position_t Pos)

                                Adds the current LIBPATH environment. 
                                Same as the example above.
                                
                      - AddCWD(DL_Position_t Pos)

                                Adds the current working directory to
                                the search list.

                      - FindFile(const char *Filename, DirectoryItem *Found)

                                Looks in each directory in the list for
                                'Filename'. 

                                'Found', if not NULL, will be set to the
                                fullpath of 'Filename.'
                                
                                Returns true if the file is found. 

                      - ClearPath() 

                                Erases all entries in the DirectoryList.

                      - Edit(const char *Find, const char *Replace)

                                Edits each directory item in the list
                                by replacing all occurrences of 'Find'
                                with 'Replace'.

                      - SetPath(const char *Path)

                                Shortcut for ClearPath() and AddPath(Path).
                                SetPath(NULL) is the same as ClearPath().

   III. ExpandDirectory(const char *Directory, size_t DirectoryLength):

                Expands environment variables (%VAR%, $VAR, ${VAR}) and
                tilde ('~' -> USERPROFILE on Windows, HOME on Unix) within
                'Directory'.

                - Variables inside single quotes ('...') are preserved
                  literally without expansion.

                - '~' expands at index 0 or immediately after a space (' ')
                  or tab ('\t'), when followed by '/', '\', or end-of-slice.

                - Does not validate filesystem existence, so it can also
                  expand variables and '~' in command strings:

                        const char *Cmd = "notepad ~\\myfile.txt";
                        char *Expanded = ExpandDirectory(Cmd, strlen(Cmd));

             >> Caller is responsible for freeing the returned string.

                Example:
                        char *Expanded = ExpandDirectory(Directory, Length);
                        if (!Expanded) return false;
                        ...
                        free(Expanded);

                To also strip surrounding quotes and split PATH delimiters
                into multiple entries, use DirectoryList::AddPath() instead:

                        const char *ToResolve = "\"%windir%\\notepad.exe\"";
                        DirectoryList Resolver;
                        Resolver.AddPath(ToResolve, DL_Before);
                        DirectoryItem *Resolved = Resolver.GetFirst();
                        if (!Resolved)
                                {
                                TERROR(("Can't resolve Path: %s", ToResolve));
                                }
                        else TINFO(("Resolved: %s", Resolved->GetDirectory()));

*******************************************************************************
**** Example: which ***********************************************************
********************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <DirList.h>

bool FindFile(const char *Filename, char **FoundFile)
        {
        DirectoryItem Item;
        DirectoryList DirList;
        DirList.AddCWD(DL_After);
        DirList.AddPATHEnv(DL_After);
        bool rc = DirList.FindFile(Filename,&Item);
        if (rc && FoundFile)
                {
                *FoundFile = Item._AquireDirectory();
                }
        return rc;
        }

char* ShortName(char *Program)
        {
        char *Pos = Program + strlen(Program);
        while (Pos != Program && *Pos != '\\') Pos--;
        if (*Pos == '\\') Pos++;
        return Pos;
        }

void Help(char *Program)
        {
        Program = ShortName(Program);
        printf("\n");
        printf("Searches the PATH and Current Directory for a file.\n\n");
        printf("%s Filename\n\n",Program);
        printf("     Filename - Name of the file to search.\n\n"); 
        printf("Returns 0 if the file was found; 1 otherwise.\n\n");
        printf("To set a variable to the result of %s:\n\n",Program);
        printf("     for /f \"usebackq\" %%a in (`which notepad.exe`) do ");
        printf("set Notepad=%%a\n\n");
        return;
        }

int main(int argc, char *argv[])
        {
        char *Fullpath = NULL;
        if (argc < 2) 
                {
                Help(argv[0]);
                return 0;
                }
        const char *Filename = argv[1];
        bool rc = FindFile(Filename,&Fullpath);
        if (rc) printf("%s\n",Fullpath);
        if (Fullpath) free(Fullpath);
        return rc ? 0 : 1;
        }

*******************************************************************************

**************************************************************************** */

#endif /* __Directory_List */
