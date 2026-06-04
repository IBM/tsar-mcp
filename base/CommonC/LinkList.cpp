// LinkList.C
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: LinkList.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1997 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <assert.h>

#include "LinkList.h"

void Link::LinkAfter(Link *After)
{
	if (Next)
		{
		After->Next = Next;
		Next->Prev = After;
		}
	After->Prev = this;
	Next = After;
	return;
}

void Link::LinkBefore(Link *Before)
{
	if (Prev)
		{
		Before->Prev = Prev;
		Prev->Next = Before;
		}
	Before->Next = this;
	Prev = Before;
	return;
}

void Link::Unlink()
{
	if (Next) Next->Prev = Prev;
	if (Prev) Prev->Next = Next;
	Next = Prev = NULL;
	return;
}

LinkList::~LinkList()
{
	if (AutoCleanup) ChainSaw();
	return;
}

void LinkList::Append(LinkList &ToAppend)
{
        if (&ToAppend == this) return;
        if (!ToAppend.Top) return;
        Link *First = ToAppend.Top;
        Link *Last = ToAppend.Bottom;
	if (Bottom)
		{
		Bottom->Next = First;
		First->Prev = Bottom;
		}
	Bottom = Last;
	if (!Top) Top = First;
        ToAppend.Top = ToAppend.Bottom = NULL;
	return;
}

void LinkList::ChainSaw()
{
	Link *Current;
	while (Top)
		{
		Current = Top;
		Remove(Current);
		delete Current;
		}
	return;
};

void LinkList::AddAfter(Link *ToAdd, Link *Before)
{
        assert(Before);
        Before->LinkAfter(ToAdd);
        if (Bottom == Before) Bottom = ToAdd;
	return;
}

void LinkList::AddBefore(Link *ToAdd, Link *After)
{
        assert(After);
	After->LinkBefore(ToAdd);
	if (Top == After) Top = ToAdd;
	return;
}

void LinkList::AddTop(Link *ToAdd)
{
	if (Top)
		{
		Top->Prev = ToAdd;
		ToAdd->Next = Top;
		}
	Top = ToAdd;
	if (!Bottom) Bottom = Top;
	return;
}

void LinkList::AddBottom(Link *ToAdd)
{
	if (Bottom)
		{
		Bottom->Next = ToAdd;
		ToAdd->Prev = Bottom;
		}
	Bottom = ToAdd;
	if (!Top) Top = Bottom;
	return;
}

void LinkList::Remove(Link *ToRemove)
{
	if (Top == ToRemove) Top = ToRemove->Next;
	if (Bottom == ToRemove) Bottom = ToRemove->Prev;
	ToRemove->Unlink();
	return;
}


/* ***********************************************************************
 ***************************** End of File *******************************
 ************************************************************************/
