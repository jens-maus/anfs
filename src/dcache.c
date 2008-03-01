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

 $Id$

***************************************************************************/

/*
 * Directory cache. Does not cache complete directories.
 */

//#include <hash/fthash.h>
//#include <proto/fthash.h>

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#include "Debug.h"

typedef struct { u_int Id; } dc_Key_T;

static ULONG
dc_HashFunc(dc_Key_T * key, ULONG s)
{
    return (((ULONG)key->Id) & 0xFF);
}

static LONG
dc_KeyCmp(dc_Key_T *key1, dc_Key_T *key2, ULONG s)
{
    return (LONG) (key1->Id - key2->Id);
}

static void
DeleteNEnt(NEntry **PNEnt)
{
    NEntry *NEnt;

    ASSERT(PNEnt);
    ASSERT(*PNEnt);

    NEnt = *PNEnt;
    fn_Delete(&NEnt->ne_Name);
    FreeVec(NEnt);
    *PNEnt = NULL;
}

static void
dce_Delete(DCEntry **dcent)
{
    NEntry *nent, *hnent;

    ASSERT(dcent);
    ASSERT(*dcent);

    nent = (*dcent)->dce_Entries;
    while(nent)
    {
	hnent = nent->ne_Next;
	fn_Delete(&nent->ne_Name);
	FreeVec(nent);
	nent = hnent;
    }
    FreeVec(*dcent);
    *dcent = NULL;
}

static void
dc_FreeVal(FTHashTable *tb, FTHashEntry *ent)
{
    DCEntry **dcentp;

    ASSERT(ent->the_Val);

    dcentp = (DCEntry **) &ent->the_Val;
    dce_Delete(dcentp);
}

LONG
dc_Init(Global_T *g)
{
    FTHashTable *tb;
#if 0
    DCEntry *dce;

    dce = &g->g_MntDirEntry;

    memset(dce, 0, sizeof(DCEntry));
    dce->dce_FileId = DCE_ID_UNUSED;
    dce->dce_ParentId = DCE_ID_UNUSED;
#endif

    tb = fthash_New(256, sizeof(dc_Key_T),
		    g->g_DirCacheMaxEntries,
		    g->g_DirCacheTimeout /* secs */,
		    dc_HashFunc,
		    dc_FreeVal,
		    dc_KeyCmp);
    if(tb)
    {
	g->g_DirCache = tb;
	return 1;
    }
    else
	return 0;
}

void dc_Cleanup(Global_T *g)
{
    if(g && g->g_DirCache)
    {
	FTHashTable **t = (FTHashTable **) &g->g_DirCache;

	fthash_Delete(t);
    }
}

/* returns pointer to new directory entry, NULL on error */

DCEntry *
dc_DCECreateAndInsert(Global_T *g, 
		      u_int fileid,
		      nfs_fh *FH)
{
    dc_Key_T Key;
    DCEntry *ent;
    LONG RetVal;

    Key.Id = fileid;

    ASSERT(FH);

    ent = AllocVec(sizeof(DCEntry), MEMF_CLEAR);
    if(!ent)
	return NULL;

    ent->dce_FileId = fileid;
    ent->dce_ParentId = DCE_ID_UNUSED;

    CopyFH(FH, &ent->dce_NFSFh);
	
    RetVal = fthash_Insert(g->g_DirCache, &Key, ent);

    if(RetVal)
    {
	return ent;
    }
    else
    {
	FreeVec(ent);
	return NULL;
    }
}

static ULONG
dc_NEStr2Hash(const UBYTE *as)
{
    ULONG l=0;
    UBYTE c;
    const UBYTE *s=as;
    
    while(c = *s++)
    {
	l <<= 1;
	l += c;
    }
    l += (as-s);
    
    return l;
}

NEntry *
dc_NECreateAndInsert(Global_T *g, 
		     DCEntry *dcent,
		     const UBYTE *AName,
		     ftype FType,
		     u_int FileId)
{
    NEntry *ent;
    UBYTE *Name;

    ASSERT(AName);

    ent = AllocVec(sizeof(NEntry), MEMF_CLEAR);
    if(!ent)
	return 0;
    Name = StrDup(AName);
    if(!Name)
    {
	FreeVec(ent);
	return 0;
    }

    ent->ne_FType = FType;
    ent->ne_FileId = FileId;
    ent->ne_Name = Name;
    ent->ne_Hash = dc_NEStr2Hash(Name);
	
    /* AddHead() */

    ent->ne_Next = dcent->dce_Entries;
    dcent->dce_Entries = ent;

    /* make sure no one removes dir entry while we use NEnt */
    fthash_Lock(g->g_DirCache); 

    return ent;
}

/* lookup directory entry from name */
/* note: entries may not be complete ! */

NEntry *
dc_NELookup(Global_T *g,
	    const DCEntry *DCEnt,
	    const UBYTE *Name)
{
    ULONG Hash;
    NEntry *NEnt;

    ASSERT(Name);
    Hash = dc_NEStr2Hash(Name);
    NEnt = DCEnt->dce_Entries;
    while(NEnt)
    {
	ASSERT(NEnt->ne_Name);

	if((Hash == NEnt->ne_Hash) && (strcmp(Name, NEnt->ne_Name) == 0))
	    break;
	NEnt = NEnt->ne_Next;
    }
    /* make sure no one removes dir entry while we use NEnt */
    if(NEnt)
	fthash_Lock(g->g_DirCache); 
    return NEnt;
}

void
dc_NERemove(Global_T *g,
	    DCEntry *DCEnt,
	    NEntry *ParNEnt)
{
    NEntry *NEnt, **PNEnt;

    PNEnt = &DCEnt->dce_Entries;
    NEnt = *PNEnt;
    while(NEnt)
    {
	if(NEnt == ParNEnt)
	{
	    /* remove entry from list */
	    *PNEnt = NEnt->ne_Next;

	    DeleteNEnt(&NEnt);
	    break;
	}
	PNEnt =&NEnt->ne_Next;
	NEnt = *PNEnt;
    }
}
		  
DCEntry *
dc_DCELookupFromFileId(Global_T *g,
		       u_int Id)
{
    dc_Key_T Key;
    DCEntry *ent;

    Key.Id = Id;

    ent = fthash_Lookup(g->g_DirCache, &Key);

    return ent;
}

void
dc_DCEDeleteFromFileId(Global_T *g, u_int Id)
{
    dc_Key_T Key;

    Key.Id = Id;

    fthash_MarkRemoved(g->g_DirCache, &Key);
}

void dc_UnLockAll(Global_T *g)
{
    fthash_UnLockAll(g->g_DirCache);
}

void
dc_Flush(Global_T *g)
{
    fthash_Flush(g->g_DirCache);
}

/* return old value on success, -1 on error */

LONG
dc_SetCacheTimeout(Global_T *g, LONG Timeout)
{
    return fthash_SetTimeToLive(g->g_DirCache, Timeout);
}

/*
 * remove timedout cache values
 */

void
dc_Bury(Global_T *g)
{
    fthash_Bury(g->g_DirCache);
}

void
dc_SetCacheSize(Global_T *g, ULONG NewSize)
{
    fthash_SetMaxEntries(g->g_DirCache, NewSize);
}
