// Main Parameters : MainArguments.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MainArguments.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2004 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <string.h>

#include <LevelTrace.h>
#include <MainArguments.h>

bool ParseArguments(ParameterList Parameters[], 
                    int argc, 
                    const char *argv[],
                    const char ***argvEnd)
        {
        static const char *ProcName = "ParseArguments";
        if (argvEnd) *argvEnd = NULL;
        const char **NextVariable = NULL;
        for (int i=1; i < argc; i++)
                {
                if (NextVariable) 
                        {
                        if (argv[i][0] == '-')
                                {
                                TERROR(("%s: Expected parameter missing",
                                        ProcName));
                                return false;
                                }
                        *NextVariable = argv[i];
                        NextVariable = NULL;
                        }
                else    {
                        int j;
                        NextVariable = NULL;
                        for (j = 0; Parameters[j].Switch; j++)
                                {
                                const char **Value = Parameters[j].Value;
                                const char *Switch = Parameters[j].Switch;
                                bool *Specified = Parameters[j].Specified;
                                size_t SwitchLen = strlen(Switch);
                                if (strstr(argv[i],Switch) == argv[i])
                                        {
                                        const char *Pos = argv[i] + SwitchLen;
                                        while (*Pos == ' ') Pos++;
                                        if (Value)
                                                {
                                                if (*Pos) *Value = Pos; 
                                                else NextVariable = Value;
                                                }
                                        if (Specified) *Specified = true;
                                        break;
                                        }
                                }
                        if (!Parameters[j].Switch) 
                                {
                                if (argvEnd)    // First un-parsed argument.
                                        {
                                        *argvEnd = &argv[i];
                                        return true;
                                        }
                                TERROR(("%s: Unknown Argument: %s",ProcName,
                                                                   argv[i]));
                                return false;
                                }
                        }
                }
        return true;
        }

// ****************************************************************************
// ***************************** End of File **********************************
// ****************************************************************************
