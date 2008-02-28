/***************************************************************************

 aNFS (ch_nfs) - Amiga NFS client
 Copyright (C) 1993-1994 Carsten Heyl
 Copyright (C) 2008      aNFS Open Source Team

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 aNFS OpenSource project:  http://sourceforge.net/projects/anfs/

 $Id: AUTHORS 2600 2007-01-01 16:34:56Z damato $

***************************************************************************/

/*
 * This module implements a generic cache using hashing.
 * This hashing functions use a fixed key size.
 * Each entry has a time attached when it should be flushed. 
 */

#include <exec/types.h>
#include <exec/memory.h>

#include <dos/dos.h> /* DateStamp */
#include <proto/dos.h> /* dto. */
#include <proto/exec.h>

#include <string.h>

#if 0
#define ASSERTIONS
#endif

#ifdef ASSERTIONS
#include <stdio.h>
#include <stdlib.h>
#include <misclib/aassert.h>
#endif

#include <hash/fthash.h>

#if 0
static __inline void
InitMinList(struct MinList *minList) { NewList((struct List *) minList); }

static __inline FTHashEntry *
FTRemHead(struct MinList *minList)
{ return (FTHashEntry *) RemHead((struct List *) minList); }

static __inline FTHashEntry *
FTRemTail(struct MinList *minList)
{ return (FTHashEntry *) RemTail((struct List *) minList); }

static __inline void
FTRemove(FTHashEntry *the)
{ Remove((struct Node *) &the->the_Node); }

static __inline void
FTAddTail(struct MinList *minList, FTHashEntry *the)
{ AddTail((struct List *) minList, (struct Node *) &the->the_Node); }

static __inline void
FTAddHead(struct MinList *minList, FTHashEntry *the)
{ AddHead((struct List *) minList, (struct Node *) &the->the_Node); }

static __inline LONG IsMinListEmpty(struct MinList *minList)
{ return IsListEmpty((struct List *) minList); }

static __inline LONG 
FT_EOL(FTHashEntry *the) { return (the->the_Node.mln_Succ == NULL); }

static __inline FTHashEntry *
FTNext(FTHashEntry *the)
{
    return FT_EOL((FTHashEntry *) the->the_Node.mln_Succ)
	? NULL : (FTHashEntry *) the->the_Node.mln_Succ;
}

static __inline FTHashEntry *
FTHead(struct MinList *ml)
{
    return FT_EOL((FTHashEntry *) ml->mlh_Head)
	? NULL : (FTHashEntry *) ml->mlh_Head;
}
#else
#define InitMinList(minList) NewList((struct List *) minList)
#define FTRemHead(minList) (FTHashEntry *) RemHead((struct List *) minList)
#define FTRemTail(minList) (FTHashEntry *) RemTail((struct List *) minList)
#define FTRemove(the) Remove((struct Node *) &(the)->the_Node)
#define FTAddTail(minList, the) \
	AddTail((struct List *) minList, (struct Node *) &(the)->the_Node)
#define FTAddHead(minList, the) \
	AddHead((struct List *) minList, (struct Node *) &(the)->the_Node)
#define IsMinListEmpty(minList) IsListEmpty((struct List *) minList)
#define FT_EOL(the) ((the)->the_Node.mln_Succ == NULL)
#define FTNext(the) \
    (FT_EOL((FTHashEntry *) (the)->the_Node.mln_Succ) \
     ? NULL : (FTHashEntry *) (the)->the_Node.mln_Succ)
#define FTHead(ml) \
    (FT_EOL((FTHashEntry *) (ml)->mlh_Head) \
     ? NULL : (FTHashEntry *) (ml)->mlh_Head)
#endif

/* default key compare function */

static LONG
fthash_DefaultKeyCmp(const void *te1, const void *te2, ULONG size)
{
    return memcmp(te1, te2, size);
}

static void
fthash_DefaultFreeVal(FTHashTable *t, FTHashEntry *te)
{
    FreeVec(te->the_Val);
}

static void __inline
fthash_FreeVal(FTHashTable *t, FTHashEntry *te)
{
    t->tht_FreeVal(t,te);
    te->the_Val = NULL;
}

FTHashTable *
fthash_New(ULONG Size, ULONG KeySize, ULONG MaxEntries, ULONG TTL,
	   ULONG (*hashfunc)(void  *, ULONG size),
	   void (*freefunc)(FTHashTable *t, FTHashEntry *te),
	   LONG (*KeyCmp)(void *, void *, ULONG))
{
    FTHashTable *t;
    LONG i;

    if(!Size || !KeySize || !MaxEntries || !TTL || !hashfunc)
	return NULL;

    t = AllocVec(sizeof(*t), MEMF_CLEAR | MEMF_PUBLIC);
    if(!t)
	return t;
    t->tht_Entries = AllocVec(sizeof(struct MinList) * Size, 
			      MEMF_CLEAR | MEMF_PUBLIC);
    if(!t->tht_Entries)
    {
	FreeVec(t);
	return NULL;
    }
    for(i = 0; i < Size; i++)
	InitMinList(&t->tht_Entries[i]);

    InitMinList(&t->tht_FreeEntries);
    InitMinList(&t->tht_LockedEntries);
    
    t->tht_Size = Size;
    t->tht_KeySize = KeySize;
    t->tht_MaxEntries = MaxEntries;
    t->tht_TTL = TTL;
    t->tht_HashFunc = hashfunc;
    if(freefunc)
	t->tht_FreeVal = freefunc;
    else
	t->tht_FreeVal = fthash_DefaultFreeVal;
    
    if(KeyCmp)
	t->tht_KeyCmp = KeyCmp;
    else
	t->tht_KeyCmp = fthash_DefaultKeyCmp;

    return t;
}

#define fthash_Free(t,e,f) fthash_XFreeFunc(t,e,f)
#define fthash_Hash(t,k) (t->tht_HashFunc(k,t->tht_KeySize) % t->tht_Size)
#define fthash_IsLocked(t) (t->tht_LockCnt > 0)
#define fthash_IsSameKey(t,a,k) \
(((t->tht_KeyCmp(the_Key(a),k,t->tht_KeySize) == 0)))

static void
fthash_FreeEntry(FTHashTable *t, FTHashEntry **pte)
{
    FTHashEntry *te = *pte;

    if(te->the_Val)
	fthash_FreeVal(t, te);
    FreeVec(te);
    
    *pte = NULL;
}

/* removes all entries of a single list */

static void
fthash_RemEntries(FTHashTable *t, struct MinList *ml)
{
    FTHashEntry *te;
    
    while(te = FTRemHead(ml))
    {
	fthash_FreeEntry(t, &te);
    }
}

/* delete complete hash table (opposite to New) */

void
fthash_Delete(FTHashTable **t)
{
    LONG i;

    if(t && *t)
    {
	if((*t)->tht_Entries)
	{
	    fthash_RemEntries(*t, &(*t)->tht_FreeEntries);
	    fthash_RemEntries(*t, &(*t)->tht_LockedEntries);
	    for(i = 0; i < (*t)->tht_Size; i++)
		fthash_RemEntries(*t, &(*t)->tht_Entries[i]);
	    
	    FreeVec((*t)->tht_Entries);
	    (*t)->tht_Entries = NULL;
	}
	FreeVec(*t);
	*t = NULL;
    }
}

/* FIXME: use timer functions ? */

static LONG
fthash_Now(void)
{
    struct DateStamp Now;
    DateStamp(&Now);

    return Now.ds_Days*(24*60*60)+Now.ds_Minute*60+Now.ds_Tick/50;
}

/* this Lookup version will return the FTHashEntry itself */

FTHashEntry *
fthash_Lookup2(FTHashTable *t, void *Key)
{
    ULONG hash;
    ULONG dead;
    FTHashEntry *ent;
    struct MinList *thl;

    hash = fthash_Hash(t, Key);
    dead = fthash_Now() - t->tht_TTL; /* back calculate time */

    thl = &t->tht_Entries[hash]; 
    ent = FTHead(thl);
    while(ent)
    {
	if(fthash_IsSameKey(t, ent, Key))
	{
	    if(dead > ent->the_Birth)
	    {
		/* entry is dead but possibly locked, 
		   just move it to Locked/FreeList */
				
		FTRemove(ent);
		if(fthash_IsLocked(t))
		{
		    t->tht_LockCnt++;
		    FTAddHead(&t->tht_LockedEntries, ent);
		}
		else
		{
		    fthash_FreeVal(t, ent);
		    FTAddHead(&t->tht_FreeEntries, ent);
		}
		ent = NULL;
		break;
	    }
	    t->tht_LockCnt++;
	    /* valid entry found, move to pole position */
	    if(ent != FTHead(thl))
	    {
		FTRemove(ent);
		FTAddHead(thl, ent);
	    }
	    break;
	}
	ent = FTNext(ent);
    }
    return ent;
}

/* 
 * application promises:
 *	no more references, so all dead entries can
 * 	be marked free 
 */

/* FIXME: could be speedup with direct list adding (?) */

void
fthash_UnLockAll(FTHashTable *t)
{
    FTHashEntry *the;
    struct MinList *fthl;

    fthl = &t->tht_FreeEntries;
    if(! IsMinListEmpty(&t->tht_LockedEntries))
    {
	struct MinList *lthl;
	
	lthl = &t->tht_LockedEntries;
	
	while(the = FTRemHead(lthl))
	{
	    fthash_FreeVal(t, the);
	    FTAddTail(fthl, the);
	}
    }
    t->tht_LockCnt = 0;
    
    /* more entries than allowed */

    /* first try to remove free entries */
    while((t->tht_NumEntries > t->tht_MaxEntries) && (the = FTRemHead(fthl)))
    {
	t->tht_NumEntries--;
	fthash_FreeEntry(t, &the);
    }
    /* second, need to remove hash entries */
    if(t->tht_NumEntries > t->tht_MaxEntries)
    {
	struct MinList *thl;
	LONG RemStart;
	LONG i = t->tht_NumEntries - t->tht_MaxEntries;

	/* use a pseudo random number to decide where to start */
	RemStart = fthash_Now() % t->tht_Size;
	while(i > 0)
	{
	    thl = &t->tht_Entries[RemStart];
	    /* search for non empty list */
	    while(IsMinListEmpty(thl))
	    {
		RemStart++;
		if(RemStart == t->tht_Size)
		{
		    thl = t->tht_Entries;
		    RemStart = 0;
		}
		else
		    thl++;
	    }
	    /* remove from end */
	    while((i > 0) && (the = FTRemTail(thl)))
	    {
		fthash_FreeEntry(t, &the);
		i--;
	    }
	}
	t->tht_NumEntries = t->tht_MaxEntries;
    }
}

/*
 * free all FthashEntries listed as free
 */

void
fthash_Flush(FTHashTable *t)
{
    FTHashEntry *ent;

    while(ent = FTRemHead(&t->tht_FreeEntries))
    {
	fthash_FreeEntry(t, &ent);
	t->tht_NumEntries--;
#ifdef ASSERTIONS
	if(t->tht_NumEntries < 0)
	{
	    fprintf(stderr,"fthash_Flush: NumEntries became negative\n");
	    exit(1);
	}
#endif
    }
}

/* same as Lookup2 above but returns value contained in FTHashEntry */

void *
fthash_Lookup(FTHashTable *t, void *Key)
{
    FTHashEntry *te;

    te = fthash_Lookup2(t, Key);
    if(te)
	return te->the_Val;
    else
	return te;
}

/* remove dead entries */

void
fthash_Bury(FTHashTable *t)
{
    FTHashEntry *ent, *hent;
    ULONG dead;
    LONG i;

    if(!fthash_IsLocked(t))
    {
	dead = fthash_Now() - t->tht_TTL;
	for(i = 0; i < t->tht_Size; i++)
	{
	    ent = FTHead(&t->tht_Entries[i]);
	    while(ent)
	    {
		if(dead > ent->the_Birth)
		{
		    /* entry too old, move to free list */

		    hent = FTNext(ent);
		    
		    FTRemove(ent);
		    fthash_FreeVal(t, ent);
		    FTAddTail(&t->tht_FreeEntries, ent);

		    ent = hent;
		}
		else
		    ent = FTNext(ent);
	    }
	}
    }
#ifdef ASSERTIONS
    else
    {
	/* FIXME */
	fprintf(stderr,"fthash_Bury called while table locked !\n");
    }
#endif
}

#define FTREMF_FREE_VAL 	0x01
/* don\'t delete entry but move it to locked list */
#define FTREMF_MOVE 		0x02

static void *
fthash_Remove3(FTHashTable *t, void *Key, ULONG Flags)
{
    ULONG hash;
    FTHashEntry *ent;
    void *val = NULL;

    hash = fthash_Hash(t, Key);

#ifdef ASSERTIONS
    if(fthash_IsLocked(t) && (!(Flags & FTREMF_MOVE)))
    {
	/* FIXME */
	fprintf(stderr,"fthash_Remove3 called while table locked !\n");
	exit(1);
    }
    else
#endif
    {
	ent = FTHead(&t->tht_Entries[hash]); 
	while(ent)
	{
	    if(fthash_IsSameKey(t, ent, Key))
	    {
		FTRemove(ent);

		if(Flags & FTREMF_MOVE)
		    FTAddTail(&t->tht_LockedEntries, ent);
		else
		{
		    if(Flags & FTREMF_FREE_VAL)
			fthash_FreeVal(t, ent);
		    else
		    {
			val = ent->the_Val;
			ent->the_Val = NULL;
		    }
		    FTAddTail(&t->tht_FreeEntries, ent);
		}
		break;
	    }
	    ent = FTNext(ent);
	}
    }
    return val;
}

/* does not free val memory but returns pointer to it */

void *
fthash_Remove2(FTHashTable *t, void *Key)
{
    return fthash_Remove3(t, Key, 0);
}

void
fthash_Remove(FTHashTable *t, void *Key)
{
    fthash_Remove3(t, Key, FTREMF_FREE_VAL);
}

void
fthash_MarkRemoved(FTHashTable *t, void *Key)
{
    fthash_Remove3(t, Key, FTREMF_MOVE);
}

static __inline FTHashEntry *
GetFreeEnt(FTHashTable *t)
{
    FTHashEntry *ent;
    
    if(! IsMinListEmpty(&t->tht_FreeEntries))
    {
	return FTRemHead(&t->tht_FreeEntries);
    }

    /* MaxEntries may be be more than MaxEntries until next call to
       UnLockAll(), UnLockAll() will remove some entries and enter them
       to FreeList() and/or will free them if MaxEntries is reached */
      
    if(t->tht_NumEntries >= (t->tht_MaxEntries + 10))
    {
	return NULL;
    }
    else
    {
	ent = AllocVec(sizeof(*ent)+t->tht_KeySize ,MEMF_CLEAR | MEMF_PUBLIC);
	if(ent)
	    t->tht_NumEntries++;
	
	return ent;
    }
	
}
 
/* returns true on success */

/* Note: Insert does not test for duplicate keys! */
/* Note: Insert will allocate memory for Key */

LONG
fthash_Insert(FTHashTable *t, void *Key, void *Val)
{
    ULONG hash;
    FTHashEntry *ent;

    hash = fthash_Hash(t, Key); 

    ent = GetFreeEnt(t);
    if(!ent)
	return 0;
    
    ent->the_Birth = fthash_Now();

    CopyMem(Key, the_Key(ent), t->tht_KeySize);
    ent->the_Val = Val;
    FTAddHead(&t->tht_Entries[hash], ent); 
    
    return 1;
}

/* returns true on success */

void
fthash_SetMaxEntries(FTHashTable *t, ULONG NewMaxEntries)
{
    /* require: not locked, NewMaxEntries > 0 */

#if 0
    AAssert(NewMaxEntries > 0);
#endif
    if(t->tht_MaxEntries <= NewMaxEntries)
    {
	t->tht_MaxEntries = NewMaxEntries;
    }
    else
    {
	t->tht_MaxEntries = NewMaxEntries;
	/* remove illegal entries */
	fthash_UnLockAll(t);
    }
}

void fthash_Lock(FTHashTable *t)
{
    t->tht_LockCnt++;
}
#if 0
void fthash_UnLock(FTHashTable *t)
{
    t->tht_LockCnt--;
#ifdef ASSERTIONS
    if(t->tht_LockCnt < 0)
    {
	fprintf(stderr,"fthash_UnLock: negative lock count \n");
	exit(1);
    }
#endif
}
#endif

LONG
fthash_SetTimeToLive(FTHashTable *t, LONG TTL)
{
    LONG Old;

    if(TTL < 0)
	return -1;
    
    Old = t->tht_TTL;
    t->tht_TTL = TTL;
    
    return Old;
}

