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
 * Lock storing and retrieval.
 */

#include <string.h>

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

/*
 * Create and init lock
 */

ELOCK *lock_Create(Global_T *g, 
		   UBYTE *FullName, LONG FullNameLen,
		   UBYTE *Name, LONG NameLen, 
		   ftype nfstype,
		   LONG Mode)
{
    ELOCK *elock;
    UBYTE *s1, *s2;

    elock = AllocVec(sizeof(*elock), MEMF_CLEAR | MEMF_PUBLIC);
    if(elock)
    {
	elock->efl_Lock.fl_Access = Mode;
	elock->efl_Lock.fl_Task = g->g_Port;
	elock->efl_Lock.fl_Volume = MKBADDR(g->g_Dev);
	elock->efl_NFSType = nfstype;
	elock->efl_FullNameLen = FullNameLen;
	elock->efl_FullName = s1 = StrNDup(FullName, FullNameLen);
 	elock->efl_NameLen = NameLen;
 	elock->efl_Name = s2 = StrNDup(Name, NameLen);
	if(!s1 || !s2)
	{
	    if(s1) FreeVec(s1);
	    if(s2) FreeVec(s2);
	    FreeVec(elock);
	    elock = NULL;
	}
    }

    return elock;
}

ELOCK *lock_Dup(Global_T *g, ELOCK *oel)
{
    ELOCK *elock;
    UBYTE *s1=NULL, *s2=NULL;

    elock = AllocVec(sizeof(*elock), MEMF_CLEAR | MEMF_PUBLIC);
    if(elock)
    {
	elock->efl_FullName = s1 = StrNDup(oel->efl_FullName, 
					   oel->efl_FullNameLen);
	elock->efl_Name = s2 = StrNDup(oel->efl_Name, 
				       oel->efl_NameLen);
    }

    if(!elock || !s1 || !s2)
    {
	if(elock) FreeVec(elock);
	if(s1) FreeVec(s1);
	if(s2) FreeVec(s2);
	elock = NULL;
    }
    else
    {
	elock->efl_Lock.fl_Key = oel->efl_Lock.fl_Key;
	elock->efl_Lock.fl_Access = oel->efl_Lock.fl_Access;
	elock->efl_Lock.fl_Task = oel->efl_Lock.fl_Task;
	elock->efl_Lock.fl_Volume = oel->efl_Lock.fl_Volume;

	CopyFH(&oel->efl_NFSFh, &elock->efl_NFSFh);

	elock->efl_NFSType = oel->efl_NFSType;
	elock->efl_FullNameLen = oel->efl_FullNameLen;
	elock->efl_NameLen = oel->efl_NameLen;
    }

    return elock;
}

/* FIXME: evt. better to use own list structure, too */

/* Note: does not check if already in list */

void lock_Insert(Global_T *g, ELOCK *elock)
{
    LOCK *l;

    l = &elock->efl_Lock;

    l->fl_Link = g->g_Dev->dl_LockList;
    g->g_Dev->dl_LockList = MKBADDR(l);
}

ELOCK *lock_Lookup(Global_T *g, LOCK *l)
{
    LOCK *al;
    ELOCK *el=NULL;

    al = BADDR(g->g_Dev->dl_LockList);
    while(al)
    {
	if(al == l)
	{
	    el = (ELOCK *) al;
	    break;
	}
	al = BADDR(al->fl_Link);
    }

    return el;
}

ELOCK *lock_LookupFromKey(Global_T *g, LONG Key)
{
    LOCK *al;
    ELOCK *el=NULL;

    al = BADDR(g->g_Dev->dl_LockList);
    while(al)
    {
	if(Key == al->fl_Key)
	{
	    el = (ELOCK *) al;
	    break;
	}
	al = BADDR(al->fl_Link);
    }

    return el;
}

LONG lock_NumLocksFromKey(Global_T *g, LONG Key)
{
    LONG Num = 0;
    LOCK *al;

    al = BADDR(g->g_Dev->dl_LockList);
    while(al)
    {
	if(Key == al->fl_Key)
	{
	    Num++;
	}
	al = BADDR(al->fl_Link);
    }

    return Num;
}

#if 0 /* not used */
ELOCK *lock_LookupFromNFSFh(Global_T *g, nfs_fh *fh)
{
    ELOCK *el;

    el = BADDR(g->g_Dev->dl_LockList);
    while(el)
    {
	if(IsSameFH(&el->efl_NFSFh, fh))
	{
	    break;
	}
	el = BADDR(el->efl_Lock.fl_Link);
    }
    return el;
}
#endif

ELOCK *lock_Remove(Global_T *g, ELOCK *l)
{
    LOCK *al;
    BPTR *pal;
    ELOCK *el=NULL;

    pal = &g->g_Dev->dl_LockList;
    al = BADDR(*pal);
    while(al)
    {
	if(al == (LOCK *) l)
	{
	    *pal = al->fl_Link;
	    el = (ELOCK *) al;
	    break;
	}
	pal = &al->fl_Link;
	al = BADDR(*pal);
    }
    return el;
}

void lock_Delete(Global_T *g, ELOCK **el)
{
    ELOCK *l;

    if(el && *el)
    {
	l = *el;
	/* Make reusing difficult */
	
	l->efl_Lock.fl_Link = 0;
	l->efl_Lock.fl_Task = NULL;
	l->efl_Lock.fl_Volume = 0;
	
	if(l->efl_FullName)
	{
	    FreeVec(l->efl_FullName);
	    l->efl_FullName = NULL;
	}
	if(l->efl_Name)
	{
	    FreeVec(l->efl_Name);
	    l->efl_Name = NULL;
	}
	FreeVec(l);
	*el = NULL;
    }
}

