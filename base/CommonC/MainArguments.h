// Main Arguments : MainArguments.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MainArguments.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2004 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Main_Arguments

        #define __Main_Arguments

struct ParameterList
        {
        const char *Switch;
        const char **Value;
        bool *Specified;
        };

bool ParseArguments(ParameterList Parameters[], 
                    int argc, 
                    const char *argv[],
                    const char ***argvEnd=NULL);

/* ***************************************************************************
 Notes:


        - ParseArguments(ParameterList Parameters[], 
                         int argc, 
                         const char *argv[],
                         const char ***argvEnd=NULL)

                Parameters are filled with associated arguments. 
                
                The function assumes argv[0] is the program name, so it is
                skipped. If parsing should start from a center argument,
                (i.e. i >= 1) set &argv[i-1] and argc-i+1.

                Parsing ends when all the arguments are parsed sucessfully
                or at the first argument that has no associated parameter.

                        If argvEnd is not NULL, the argvEnd is set to the
                        argument that didn't parse and the function returns
                        true.

                        If argvEnd is NULL, the function returns false.

******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

main(int argc, const char* argv[])

        ...

        const char *Filename = NULL;
        const char *TraceFilename = NULL;
        const char *ExcludeFilename = NULL;
        bool WriteExcludeList = false;
        bool NoExcludeList = false;
        bool Debug = false;
        ParameterList Parameters[] =
                {
                        {"-f",&Filename,NULL},
                        {"-tf",&TraceFilename,NULL},
                        {"-exclude+",&ExcludeFilename,&WriteExcludeList},
                        {"-exclude-",NULL,&NoExcludeList},
                        {"-exclude",&ExcludeFilename,NULL},
                        {"-debug",NULL,&Debug},
                        {NULL,NULL,NULL}
                };
        bool Status = ParseArguments(Parameters,argc,argv);
        if (!Status)
                {
                Help(argv[0]);
                return 1;
                }

        ...

        return 0;
        }

*****************************************************************************/

#endif /* __Main_Arguments */
