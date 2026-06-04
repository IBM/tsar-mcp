// printf to Memory Stream:  MEMprintf.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: MEMprintf.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2006 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdarg.h>

#ifndef __Memory_Printf

        #define __Memory_Printf

class MemoryPrintf
        {
        private:
                void *PrintfIMPL;
                friend class MemoryPrintfIMPL;
        protected:
                virtual void FreeBuffer(void *Buffer);
                virtual void* ReallocBuffer(void *Buffer, size_t Size);
        public:
                MemoryPrintf();
                ~MemoryPrintf();
                char* AquireBuffer();
                const wchar_t* AquireBufferW();
                void _AttachBuffer(char *Buffer, size_t Size);
                void Clear();
                const char* GetBuffer();
                const wchar_t* GetBufferW() {return (wchar_t*)GetBuffer();}
                void SetUTF8(bool CharUTF8, bool WideUTF8);
                // ***************
                // **** stdio ****
                // ***************
                int fputs(const char *String);
                int fputws(const wchar_t *String);
                int printf(const char *Format, ...);
                int wprintf(const wchar_t *Format, ...);
                int puts(const char *String);
                int putws(const wchar_t *String);
                int vprintf(const char *Format, va_list Args);
                int vwprintf(const wchar_t *Format, va_list Args);
        };

inline const wchar_t* MemoryPrintf::AquireBufferW() 
        {
        return (wchar_t *)AquireBuffer();
        }

// ***********************************
// **** Dimensioning MemoryPrintf ****
// ***********************************

class DimMemoryPrintf : public MemoryPrintf
        {
        private:
                size_t MinSize;
                bool WriteMode;
        protected:
                virtual void* ReallocBuffer(void *Buffer, size_t Size);
        public:
                DimMemoryPrintf();
                void SetWriteClear(bool Mode);
        };

/* ***************************************************************************
 Notes:

        MemoryPrintf is a dynamic memory canvas for printf().

     A. Methods:

                - char* AquireBuffer()
                  wchar_t* AquireBufferW()

                        Returns the address of the current canvas buffer.

                        The caller is responsible for free()'ing the
                        storage as would the virtual method FreeBuffer()
                        would.

                        The MemoryPrintf instance is in a state as though
                        Clear() had been called.


                -  _AttachBuffer(char *Buffer, size_t Size)

                        Clears the current canvas, and sets NULL TERMINATED
                        string as the Buffer. 'Buffer' must be compatable
                        with the virtual FreeBuffer() and ReallocBuffer()
                        methods.

                - Clear()

                        Clears the canvas.

                - GetBuffer()
                  GetBufferW()

                        Returns a pointer to the current buffer. This
                        pointer MAY BE INVALID after subsequenct calls
                        to anyother MemoryPrintf method.

                - SetUTF8(bool CharUTF8, bool WideUTF8)

                        When CharUTF8 is set true, charcters ("%c") are 
                        converted from Latin-1 (CodePage 1252) to UTF-8.

                        When WideUTF8 is set true, wide characters ("%ws") 
                        are converted from UTF-16 to UTF-8 (Currently
                        UTF-16 surrogate-pairs are not supported and
                        are not printed).

                        When set false (default), characters ("%c") are 
                        writen unmodified and wide character strings ("%ws") 
                        are truncated; meaning wide characters other than 
                        Latin-1 will appear incorrectly.

                - printf(const char *Format, ...)
                  wprintf(const wchar_t *Format, ...)

                        Same semantics as c-runtime printf() and wprintf()
                        respectively, only that the output goes to the canvas.

                        Returns the number of characters written.

                - fputs(const char *String)
                  fputws(const wchar_t *String)

                        Same semantics as c-runtime fputs() and fputws() 
                        respectively, only that the output goes to the canvas.

                        Returns the number of characters written.

                - puts(const char *String)
                  putws(const wchar_t *String)

                        Same semantics as c-runtime puts() (String + NewLine)
                        and putws() respectively, only that the output goes 
                        to the canvas.

                        Returns the number of characters written.

     B. Virtual Methods:


                - FreeBuffer(void *Buffer)

                        Must free Buffer' as would the c-runtime free().
                        The default is realloc().

                - ReallocBuffer(void *Buffer, size_t Size)

                        Must allocate and reallocate 'Buffer' as would
                        the c-runtime realloc(). The default is realloc().

                        If the function returns NULL, printf continues
                        to process and count but not write into memory. 

                        This feature can be used by a derived class to
                        first count (track the maximum value of 'Size'
                        in  counting-mode) then allocate at least the
                        maximum 'Count' at the first call to 'Realloc' 
                        during write-mode.

     C. Special Formats:

             1. 64 Bit Integers Size Modifiers: "ll", "I64", "L"

                        e.g. "%llu", "%I64X", "Ld"

                Note: Both modifiers work on all platforms.     

     D. DimMemoryPrintf:

             When first constructed, a DimMemoryPrintf is in counting
             mode and no data is written to memory. The idea is to print
             the same thing twice; once in 'count' mode, once in 'write'
             mode.
             
             After SetWriteClear(true) is called to enable 'Write' mode, 
             the first allocation will allocate a minimum number of 'count'
             bytes. For further writes beyond 'count', DimMemoryPrintf
             behaves like MemoryPrintf.

             In general MemoryPrintf has a runtime cost of O(n*log2(n))
             and DimMemoryPrintf a cost of O(2*n).

             When SetWriteClear(false) is called to enable 'count' mode,
             the 'count' is reset to zero (0) as if the class instance
             had just been constructed.

     E. Unicode Variants:

             UTF8: The non-wide functions can write utf8 characters
                   to memory using SetUTF8() - See above.


                   Note: To set a DOS concole to UTF-8: chcp 65001
    
      
             UTF16/UTF32: Windows=UTF16. AIX-64=UTF32. AIX-32=UTF16.

                   The wide functions (e.g. wprintf) write wchar_t for
                   both wide (%c, %wc) and non-wide (%hc) arguments.

                   When using wide functions, it is convienent to
                   use the associated buffer get and aquire functions:

                      - GetBufferW()
                      - AquireBufferW()

******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

#include <stdio.h>
#include <MEMprintf.h>

main()
        {
        MemoryPrintf Canvas;

        Canvas.printf("The %u Quick Brown ",34);
        Canvas.fputs("foxes ");
        Canvas.printf("jumped over the %g lazy ",123.456);
        Canvas.puts("dogs");

        puts(Canvas.GetBuffer());
        return 0;
        }

*****************************************************************************/

#endif
