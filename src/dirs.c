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
 * Dir storing and retrieval functions.
 */

#define USE_BUILTIN_MATH /* for min from string.h */

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#include "Debug.h"

/* FIXME: make module containing single list functions !*/

EDH *dir_Create(const Global_T *g, 
		LONG Key,
		ULONG cookie, nfs_fh *nfsfh, nfscookie nfsc,
		UBYTE *Name, LONG MaxFileNameLen)
{
    EDH *edh;

    edh = AllocVec(sizeof(*edh), MEMF_CLEAR | MEMF_PUBLIC);
    if(edh)
    {
	edh->edh_Key = Key;
	edh->edh_Cookie = cookie;
	CopyFH(nfsfh, &edh->edh_NFSDirFh);
	CopyCookie(nfsc, edh->edh_NFSCookie);
	if(Name)
	{
	    int l = min(min(Name[0]+2, EDH_MAX_FILENAME_SIZE-1), MaxFileNameLen);
	    if(l)
		memcpy(edh->edh_Name, Name, l);
	    else
		edh->edh_Name[1] = 0;
	    edh->edh_Name[l] = 0;
	}
	else
	{
	    /* empty string "" */
	    edh->edh_Name[0] = 0;
	    edh->edh_Name[1] = 0;
	}
    }

    return edh;
}

void dir_Insert(Global_T *g, EDH *edh)
{
    ASSERT(edh);

    edh->edh_Next = g->g_DH;
    g->g_DH = edh;
}
#if 0
EDH *dir_LookupFromCookie(Global_T *g, ULONG Cookie)
{
    EDH *edh;

    edh = g->g_DH;
    while(edh)
    {
	if(edh->edh_Cookie == Cookie)
	    break;
	edh = edh->edh_Next;
    }

    return edh;
}
#endif

#if 0
EDH *dir_LookupFromKeyAndCookie(Global_T *g, LONG Key, ULONG Cookie)
{
    EDH *edh;

    edh = g->g_DH;
    while(edh)
    {
	if((edh->edh_Cookie == Cookie) && (edh->edh_Key == Key))
	    break;
	edh = edh->edh_Next;
    }

    return edh;
}
#endif

EDH *dir_LookupFromKeyAndCookieAndCBSTR(Global_T *g, LONG Key, ULONG Cookie,
					UBYTE *Name)
{
    EDH *edh;

    edh = g->g_DH;
    while(edh)
    {
	if((edh->edh_Cookie == Cookie) && (edh->edh_Key == Key)
	   && (memcmp(edh->edh_Name, Name, edh->edh_Name[0]+2) == 0))
	    break;
	edh = edh->edh_Next;
    }
    return edh;
}

EDH *
dir_LookupFromAddress(Global_T *g, void *addr)
{
    EDH *edh;

    edh = g->g_DH;
    while(edh)
    {
	if(edh == addr)
	    break;
	edh = edh->edh_Next;
    }

    return edh;
}

EDH *dir_Remove(Global_T *g, EDH *arg_edh)
{
    EDH *edh, **pedh;

    pedh = &g->g_DH;
    edh = *pedh;
    while(edh)
    {
	if(edh == arg_edh)
	{
	    *pedh = edh->edh_Next;
	    break;
	}
	pedh = &edh->edh_Next;
	edh = *pedh;
    }

    return edh;
}

__inline void dir_DelEntry(entry **arg_ent)
{
    entry *ent;

    if(arg_ent && *arg_ent)
    {
	ent = *arg_ent;

	if(ent->name)
	    fn_Delete(&ent->name);
	FreeVec(ent);

	*arg_ent = NULL;
    }
}

__inline static void dir_DelEntries(entry **arg_ent)
{
    entry *ent, *nent;;

    if(arg_ent && *arg_ent)
    {
	ent = *arg_ent;
	while(ent)
	{
	    nent = ent->nextentry;

	    dir_DelEntry(&ent);
	    ent = nent;
	}
	

	*arg_ent = NULL;
    }
}

void
dir_Delete(EDH **arg_edh)
{
    EDH *edh;

    if(arg_edh && *arg_edh)
    {
	edh = *arg_edh;

	dir_DelEntries(&edh->edh_NFSEntries);
	FreeVec(edh);

	*arg_edh = NULL;
    }
}

void
dir_DeleteAll(Global_T *g)
{
    EDH *edh;

    while(edh = g->g_DH)
    {
	g->g_DH = edh->edh_Next;
	dir_Delete(&edh);
    }
}

entry *
dir_RemHeadEntry(EDH *edh)
{
    entry *ent;

    ent = edh->edh_NFSEntries;
    if(ent)
	edh->edh_NFSEntries = ent->nextentry; 

    return ent;
}

entry *
dir_GetHeadEntry(EDH *edh)
{
    return  edh->edh_NFSEntries;
}

/* duplicate entry list */

entry *dir_DupEntries(entry *arg_ent)
{
    entry *ret_ent = NULL, *ent, *pent = NULL, *newent;
    filename name;

    ent = arg_ent;
    while(ent)
    {
	newent = AllocVec(sizeof(entry), MEMF_CLEAR | MEMF_PUBLIC);
	name = StrDup(ent->name);
	if(!newent || !name)
	{
	    dir_DelEntry(&newent);
	    dir_DelEntries(&ret_ent);
	    if(name)
		FreeVec(name);
	    break;
	}
	if(!ret_ent)
	    ret_ent = newent;

	newent->fileid = ent->fileid;
	newent->name = name;
	CopyCookie(ent->cookie, newent->cookie);
	if(pent)
	    pent->nextentry = newent;
	pent = newent;
	ent = ent->nextentry;
    }
	  
    return ret_ent;
}
