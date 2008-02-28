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
 * Attribute cache.
 */

#include <hash/fthash.h>
#include <proto/fthash.h>

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#define CopyFAttr(a,b) CopyMem(a,b,sizeof(struct fattr))

typedef struct { u_int Id; } attrs_Key_T;

static ULONG
attrs_HashFunc(attrs_Key_T * key, ULONG s)
{
    return (((ULONG)key->Id) & 0xFF);
}

static LONG
attrs_KeyCmp(attrs_Key_T *key1, attrs_Key_T *key2, ULONG s)
{
    return (LONG) (key1->Id - key2->Id);
}

LONG
attrs_Init(Global_T *g)
{
    FTHashTable *tb;

    tb = fthash_New(256, sizeof(attrs_Key_T),
		    g->g_AttrCacheMaxEntries,
		    g->g_AttrCacheTimeout /* secs */,
		    attrs_HashFunc,
		    NULL, /* standard free function (FreeVec()) */
		    attrs_KeyCmp);
    if(tb)
    {
	g->g_AttrCache = tb;
	return 1;
    }
    else
	return 0;
}

void
attrs_Cleanup(Global_T *g)
{
    if(g && g->g_AttrCache)
    {
	FTHashTable **t = (FTHashTable **) &g->g_AttrCache;

	fthash_Delete(t);
    }
}

ACEntry *
attrs_LookupFromFileId(Global_T *g, u_int FileId)
{
    attrs_Key_T Key;
    
    Key.Id = FileId;
    return fthash_Lookup(g->g_AttrCache, &Key);
}

void
attrs_Delete(Global_T *g, LONG FileId)
{
    attrs_Key_T Key;

    Key.Id = FileId;
    fthash_MarkRemoved(g->g_AttrCache, &Key);
}

ACEntry *
attrs_Update(Global_T *g,
	     nfs_fh *FH, 
	     fattr *FAttr)
{
    ACEntry *ACEnt;
    
    ACEnt = attrs_LookupFromFileId(g, FAttr->fileid);
    if(ACEnt)
    {
	CopyFH(FH, &ACEnt->ace_NFSFh);
	CopyFAttr(FAttr, &ACEnt->ace_FAttr);

	return ACEnt;
    }
    else
	return attrs_CreateAndInsert(g, FH, FAttr);
    
}			   

/* returns pointer to new entry, NULL on error */

ACEntry *
attrs_CreateAndInsert(Global_T *g, 
		      nfs_fh *FH, 
		      fattr *FAttr)
			   
{
    attrs_Key_T Key;
    ACEntry *ent;
    LONG RetVal;

    Key.Id = FAttr->fileid;

    ent = AllocVec(sizeof(ACEntry), MEMF_CLEAR);
    if(!ent)
	return NULL;

    ent->ace_FileId = Key.Id;
    CopyFH(FH, &ent->ace_NFSFh);
    CopyFAttr(FAttr, &ent->ace_FAttr);
	
    RetVal = fthash_Insert(g->g_AttrCache, &Key, ent);
    if(RetVal)
	return ent;
    else
    {
	FreeVec(ent);
	return NULL;
    }
}

void
attrs_UnLockAll(Global_T *g)
{
    fthash_UnLockAll(g->g_AttrCache);
}

void
attrs_Flush(Global_T *g)
{
    fthash_Flush(g->g_AttrCache);
}

/* return old value on success, -1 on error */

LONG
attrs_SetCacheTimeout(Global_T *g, LONG Timeout)
{
    return fthash_SetTimeToLive(g->g_AttrCache, Timeout);
}

/*
 * remove timedout cache values
 */

void
attrs_Bury(Global_T *g)
{
    fthash_Bury(g->g_AttrCache);
}

void
attrs_SetCacheSize(Global_T *g, ULONG NewSize)
{
    fthash_SetMaxEntries(g->g_AttrCache, NewSize);
}
