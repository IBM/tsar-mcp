// JSONParserExp.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: JSONParserExp.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2005 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <assert.h>
#include <JSONParserExp.h>
#include <JSONParserDetect.h>

#if defined(_MT) || defined(__MT__)
        #define MultithreadSafeALLOC 1
#else
        #define MultithreadSafeALLOC 0
#endif

#define MemPoolSIZE (sizeof(StackItem) |        /* >Maximum of all items */  \
                     sizeof(TerminalNodeBase) |                              \
                     sizeof(NonTerminalNodeBase) |                           \
                     sizeof(JSONLexicalItem))

// ****************************************************************************
// **** JSONExpAlloc / JSONExpDealloc / JSONPrealloc **************************
// ****************************************************************************

static bool UseSafeALLOC = MultithreadSafeALLOC;

void SetMultithreadSafeAlloc(bool beSafe)
        {
        UseSafeALLOC = beSafe;
        return;
        }

#if MultithreadSafeALLOC
        #include <ASThread.h>
        #define LOCKPOOL {if (UseSafeALLOC) PoolMutex.Take();}
        #define UNLOCKPOOL {if (UseSafeALLOC) PoolMutex.Release();}
#else
        #define LOCKPOOL ((void)0)
        #define UNLOCKPOOL ((void)0)
#endif

class JTQ_MemPool;

struct JTQ_MemItem
        {
        size_t Size;
        bool QuickHeap;
        JTQ_MemPool *Owner;
        JTQ_MemItem *Next;
        unsigned char Data[1];
        };

#define DataOffset offsetof(JTQ_MemItem,Data)
#define DataToItem(Data) (JTQ_MemItem *)(((char *)Data)-DataOffset)

class JTQ_MemPool
        {
        private:
                size_t MaxSize;
                JTQ_MemItem *Top;
                unsigned Depth;
                void *QuickHeap;
  #if MultithreadSafeALLOC
                CriticalSection PoolMutex;
  #endif
        public:
                JTQ_MemPool(size_t Size);
                ~JTQ_MemPool();
                void ReleaseItem(JTQ_MemItem *Item);
                JTQ_MemItem* GetItem();
                size_t GetMaxSize() {return MaxSize;}
                unsigned GetDepth() {return Depth;}
                bool PreallocHeap(unsigned nItems);
        };

JTQ_MemPool::JTQ_MemPool(size_t Size)
        {
        QuickHeap = NULL;
        MaxSize = Size;
        Top = NULL;
        Depth = 0;
        return;
        }

JTQ_MemPool::~JTQ_MemPool()
        {
        while (Top)
                {
                JTQ_MemItem *Next = Top->Next;
                if (!Top->QuickHeap) free(Top);
                Top = Next;
                }
        if (QuickHeap) free(QuickHeap);
        return;
        }

inline JTQ_MemItem* JTQ_MemPool::GetItem()
        {
        JTQ_MemItem *Item = NULL;
        LOCKPOOL;
        if (Top)
                {
                Depth--;
                Item = Top;
                Top = Item->Next;
                }
        UNLOCKPOOL;
        if (Item) return Item;
        size_t MemSize = sizeof(JTQ_MemItem) + MaxSize - 1;
        Item = (JTQ_MemItem *)malloc(MemSize);
        if (!Item) return NULL;
        Item->QuickHeap = false;
        Item->Owner = this;
        return Item;
        }

bool JTQ_MemPool::PreallocHeap(unsigned nItems)
        {
        if (QuickHeap) return false;
        size_t MemSize = sizeof(JTQ_MemItem) + MaxSize - 1;
        QuickHeap = malloc(nItems * MemSize);
        if (!QuickHeap) return false;
        for (unsigned i=0; i < nItems; i++)
                {
                size_t Index = i * MemSize;
                JTQ_MemItem *Item = (JTQ_MemItem *)((char *)QuickHeap + Index);
                Item->QuickHeap = true;
                Item->Owner = this;
                ReleaseItem(Item);
                }
        return true;
        }

inline void JTQ_MemPool::ReleaseItem(JTQ_MemItem *Item)
        {
        LOCKPOOL;
        Item->Next = Top;
        Top = Item;
        Depth++;
        UNLOCKPOOL;
        return;
        }

static JTQ_MemPool MemPool(MemPoolSIZE);

void* JSONExpAlloc(size_t Size)
        {
        assert(MemPool.GetMaxSize() >= Size);
        JTQ_MemItem *Item = MemPool.GetItem();
        if (!Item) return NULL;
        Item->Next = NULL;
        Item->Size = Size;
        return Item->Data;
        }

void JSONExpDealloc(void *Memory)
        {
        JTQ_MemItem *Item = DataToItem(Memory);
        if (Item->Owner) Item->Owner->ReleaseItem(Item);
        else free(Item);
        return;
        }

bool JSONExpPrealloc(unsigned nItems)
        {
        return MemPool.PreallocHeap(nItems);
        }

unsigned JSONExpGetDepth()
        {
        return MemPool.GetDepth();
        }

// ****************************************************************************
// **** JSONParser Nodes ********************************************************
// ****************************************************************************

JSONTerminalNode::~JSONTerminalNode()
        {
        JSONLexicalItem *Item = (JSONLexicalItem *)GetValue();
        delete Item;
        return;
        }

// ****************************************************************************
// ******************************* End of File ********************************
// ****************************************************************************
