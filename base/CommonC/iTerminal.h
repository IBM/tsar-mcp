// Generic Terminal for iSeries: iTerminal.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: iTerminal.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2002 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Allows spawned thread jobs to be interactive.
//

#ifndef __iSeries_Generic_Terminal

        #define __iSeries_Generic_Terminal

#include <qp0z1170.h>
#include <qp0ztrml.h>

class iSeriesTerminal
        {
        private:
                bool Started;
                int EnvCount;
                const char *Title;
                char* (*Environment)[];
                Qp0z_Terminal_T handle;
                Qp0z_Terminal_Attr_T attr;
        protected:
                void ClearEnvironment();
                bool ResetDefaultEnvironment();

        public:
                iSeriesTerminal(const char *Title = NULL);
                ~iSeriesTerminal();
                bool Start(int argc, 
                           const char *argv[],
                           const char *argv0 = NULL,    // Replace the first.
                           const char *argvN = NULL);   // One additional.
                bool Start(int argc, 
                           char *argv[],
                           const char *argv0 = NULL,    // Replace the first.
                           const char *argvN = NULL);   // One additional.
                int putenv(const char *EnvironmentString);
                int Run();
                void End();
        };

extern "c" int isatty(int handle);

/* ***************************************************************************
 Notes:

******************************************************************************
**** Example: ****************************************************************
***************

#include <stdio.h>
#include <iTerminal.h>

main()
        {
        char *args[1];
        iSeriesTerminal Term;

        args[0] = "/QSYS.LIB/ERICBLD.LIB/ERICSTR.PGM";

        Term.putenv("QIBM_USE_DESCRIPTOR_STDIO=Y");
        if (Term.Start(1,args))
                {
                Term.Run();
                }

        return 0;
        }

*************************************************************************** */

#endif /* __iSeries_Generic_Terminal */
