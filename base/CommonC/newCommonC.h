// newCommonC.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: newCommonC.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2026 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
// Non-throwing, NULL returning new() operators for the
// CommonC framework.
//
// newNOTHROW instructs the complier that new() returns NULL and
// not to optimize out NULL checks in the code following new().
//

#ifndef __newCommonC_h

     #define __newCommonC_h

#include <stddef.h>
#include <stdlib.h>

#ifndef newNOTHROW
        #if (defined(__cplusplus) && __cplusplus >= 201103L) || \
            (defined(_MSC_VER) && _MSC_VER >= 1900)
                #define newNOTHROW noexcept
        #else
                #define newNOTHROW throw()
        #endif
#endif

#if (defined(__cplusplus) && __cplusplus >= 201402L) || \
    (defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L)
        #define CommonC_sizedDeleteBLOCK                                          \
                void operator delete(void *Item, size_t) newNOTHROW {free(Item);} \
                void operator delete[](void *Item, size_t) newNOTHROW {free(Item);}
#else
        #define CommonC_sizedDeleteBLOCK
#endif

#define CommonC_newOperatorBLOCK                                                \
        void* operator new(size_t Size) newNOTHROW {return malloc(Size);}       \
        void  operator delete(void *Item) newNOTHROW {free(Item);}              \
        void* operator new[](size_t Size) newNOTHROW {return malloc(Size);}     \
        void  operator delete[](void *Item) newNOTHROW {free(Item);}            \
        CommonC_sizedDeleteBLOCK

#endif /* __newCommonC_h*/
