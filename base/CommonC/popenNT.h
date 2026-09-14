// popen for NT : popenNT.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: popenNT.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2009 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//      Note: Doesn't Require a Console like VS c-rutime popen.
//
//      IMPORTANT Notes:
//
//          1. When 'err' is not specified or NULL, both child stdout and
//             stderr are directed to stdout.
//
//          2. When 'in' is requested, the caller MUST fclose(in) before
//             calling pcloseNT(). When 'in' is not requested, pcloseNT()
//             sends EOF to the child automatically.
//
//          3. Call pcloseNT()/pabortNT() with 'out' or 'err' but not 'in'.
//
//          4. pabortNT()/pabortALL() may be called from another thread.
//

#ifndef __POPEN_for_NT

        #define __POPEN_for_NT

#include <stdio.h>

// -----------------------------------------------------------------
// ---- String API (popen-compatible -- caller handles quoting) ----
// -----------------------------------------------------------------

FILE* popenNT(const char *Command, const char *Mode);

bool popenNToe(const char *Command, const char *Mode, FILE **out, FILE **err);

bool popenNTio(const char *Command, const char *Mode, FILE **in, FILE **out);

bool popenNTioe(const char *Command, 
                const char *Mode, 
                FILE **in,
                FILE **out,
                FILE **err);

// ------------------------------------------------------------
// ---- Argv API (no quoting needed -- discrete arguments) ----
// ------------------------------------------------------------

bool popenNToe(int argc, const char *argv[], 
               const char *Mode, 
               FILE **out, 
               FILE **err);

bool popenNTio(int argc, const char *argv[], 
               const char *Mode, 
               FILE **in, 
               FILE **out);

bool popenNTioe(int argc, const char *argv[],
                const char *Mode, 
                FILE **in,
                FILE **out,
                FILE **err);

// ===================
// ==== Lifecycle ====
// ===================

// -----------------------------
// ---- Close after reading ----
// -----------------------------

int pcloseNT(FILE *stream);     // Close with 'out' or 'err' (not 'in').s

// -------------------------------------------
// ---- Abort (call in- or out-of-thread) ----
// -------------------------------------------

bool pabortNT(FILE *stream);    // Terminates the child (must still pclose()).

void pabortALL();               // Terminates ALL active child processes!!

// ****************************************************************************
// **** Helper Command to argv Utilities **************************************
// ***************************************
//
//     Both Command2argv() and ShellPrefix2argv() return an argv vector 
//     that must be freed using FreeargvC(). They return -1 on error. 
//
//     ShellPrefix2argv() prepends the ShellPrefix to the argvIn[0] if
//     present. If ShellPrefix is "just a path", verify it terminates with
//     a platform directory separator (e.g. '/' or '\\'). ShellPrefix may 
//     be NULL or empty, and argcIn may be zero.
//
//     Example: char **argvCmd  = NULL;
//              char **argvFull = NULL;
//              int argcCmd  = Command2argv(Command,&argvCmd);
//              int argcFull = ShellPrefix2argv(shellPrefix,
//                                              argcCmd,
//                                              (const char**)argvCmd,
//                                              &argvFull);
//              FreeargvC(argcCmd,&argvCmd);     // Copied, not consumed.
//              if (argcFull < 0) return;        // Error; argvFull NULL.
//              ...                              // Use argvFull, then:
//              FreeargvC(argcFull,&argvFull);   // Safe even if argc 0.
//
// ****************************************************************************

int Command2argv(const char *Command, char ***argv);    // Returns argc.

int ShellPrefix2argv(const char *ShellPrefix,           // Returns argc.
                     int argcIn, 
                     const char* argvIn[], 
                     char ***argv);

void FreeargvC(int argc, char ***argv);                 // Frees Command2argv
                                                        // and ShellPrefix2argv.

#endif /* __POPEN_for_NT */
