// LinkList.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: LinkList.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//

#ifndef __Link_List_Classes

     #define __Link_List_Classes

#include <stddef.h>

class Link
{
    private:

	Link *Prev, *Next;
	void LinkAfter(Link *After);
	void LinkBefore(Link *Before);
	void Unlink();
	friend class LinkList;

    public:

	Link() {Prev = Next = NULL;}
	virtual ~Link() {}
};

class LinkList
{
    private:

	bool AutoCleanup;
	Link *Top, *Bottom;

    public:

	LinkList() {Top = Bottom = NULL; AutoCleanup = false;}
	~LinkList();
	void AddAfter(Link *ToAdd, Link *Before);
	void AddBefore(Link *ToAdd, Link *After);
	void AddBottom(Link *ToAdd);
	void AddTop(Link *ToAdd);
        void Append(LinkList &ToAppend);
	void ChainSaw();
	Link* GetFirst() {return Top;}
	Link* GetNext(Link *Current) {return Current->Next;}
	Link* GetLast() {return Bottom;}
	Link* GetPrev(Link *Current) {return Current->Prev;}
	void Remove(Link *ToRemove);
	void SetAutoCleanup(bool Cleanup) {AutoCleanup = Cleanup;}
};

#define DefineLinkList(ListName,Type)                                    \
    class ListName : public LinkList                                     \
    {									 \
      public:                                                            \
         void AddAfter(Type *T, Type *B) {LinkList::AddAfter(T,B);}      \
         void AddBefore(Type *T, Type *A) {LinkList::AddBefore(T,A);}    \
         void AddBottom(Type *ToAdd) {LinkList::AddBottom(ToAdd);}       \
         void AddTop(Type *ToAdd) {LinkList::AddTop(ToAdd);}             \
	 Type* GetFirst() {return (Type *)LinkList::GetFirst();}         \
         Type* GetLast() {return (Type *)LinkList::GetLast();}           \
	 Type* GetNext(Type *C) {return (Type *)LinkList::GetNext(C);}   \
         Type* GetPrev(Type *C) {return (Type *)LinkList::GetPrev(C);}   \
    }

#endif
