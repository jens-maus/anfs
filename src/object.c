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
 * Delete/rename actions, create dir.
 */

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

//#include <aulib/aulib.h>

#include "Debug.h"

LONG act_DELETE_OBJECT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg2;

    UBYTE *Name;
    LONG Res2;

    DCEntry *ParentDir;
    NEntry *NEnt;
    ACEntry *ACEnt;

    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0 /* not CString */, 
			       &Name, NULL,
			       NULL,NULL, /* no FullName needed */
			       &Res2);
    if(!ParentDir)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    /* we need to know if directory or file and we need the fileid
       to see if open locks */
    /* FIXME: i think it is possible for a lookup to fail but
       a remove still being possible ?
       */
    NEnt = GetNameEntry(g, ParentDir, Name, &ACEnt, &Res2);
    if(NEnt)
    {
	/* don\'t allow remove if locks are still open */
	if(!lock_LookupFromKey(g, NEnt->ne_FileId))
	{
	    if(NEnt->ne_FType == NFDIR)
	    {
		if(nfs_RmDir(g->g_NFSClnt, 
			     &ParentDir->dce_NFSFh, Name, &Res2))
		    SetRes1(g, DOSTRUE);
		else
		    SetRes(g, DOSFALSE, Res2);
		/* delete directory from cache */
		/* FIXME: test for directory contents ? */
		dc_DCEDeleteFromFileId(g, NEnt->ne_FileId);
	    }
	    else
	    {
		if(nfs_Remove(g->g_NFSClnt,
			      &ParentDir->dce_NFSFh, Name, &Res2))
		    SetRes1(g, DOSTRUE);
		else
		    SetRes(g, DOSFALSE, Res2);
	    }
	    /* remove cache entry */
	    attrs_Delete(g, NEnt->ne_FileId);
	    dc_NERemove(g, ParentDir, NEnt);
	}
	else
	    SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
    }
    else
	SetRes(g, DOSFALSE, Res2);

 exit:
    fn_Delete(&Name);
    
    return g->g_Res1;
}

LONG
act_CREATE_DIR(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg2;

    UBYTE *FullName, *Name;
    LONG FullNameLen, NameLen;
    LONG Res2;

    ELOCK *newelock;
    diropokres *dir;
    DCEntry *ParentDir;
    
    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0 /* not CString */, 
			       &Name, &NameLen,
			       &FullName, &FullNameLen, &Res2);
    if(!ParentDir)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    /* we just try to create directory and leave it to the server
       to inform us about errors like "already exists" etc. */

    dir = nfs_MkDir(g->g_NFSClnt, 
		    &ParentDir->dce_NFSFh, Name,
		    nfsc_GetUID(g), nfsc_GetGID(g), 
		    nfsc_ApplyUMask(g, 0777), &Res2);
    if(dir)
    {
	newelock = lock_Create(g, 
			       FullName, FullNameLen, 
			       Name, NameLen, 
			       NFDIR,
			       EXCLUSIVE_LOCK);
	if(newelock)
	{
	    CopyFH(&dir->file, &newelock->efl_NFSFh);
	    newelock->efl_Lock.fl_Key = dir->attributes.fileid;
	    lock_Insert(g, newelock);
	    /* FIXME: intser cache here ! */
	    SetRes1(g, MKBADDR(&newelock->efl_Lock));
	}
	else
	    SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
    }
    else
	SetRes(g, DOSFALSE, Res2);

 exit:
    fn_Delete(&FullName);
    fn_Delete(&Name);
    
    return g->g_Res1;
}

LONG act_RENAME_OBJECT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl1 = (LOCK *) pkt->dp_Arg1;
    CBSTR cbstr1 = (CBSTR) pkt->dp_Arg2;
    LOCK *fl2 = (LOCK *) pkt->dp_Arg3;
    CBSTR cbstr2 = (CBSTR) pkt->dp_Arg4;

    UBYTE *Name1 = NULL, *Name2 = NULL;
    LONG Res2;

    DCEntry *ParentDir1, *ParentDir2;
    NEntry *NEnt;
    ACEntry *ACEnt;

    ParentDir1 = LRootLocateDir(g, 
			       fl1, cbstr1, 0 /* not CString */, 
			       &Name1, NULL,
			       NULL,NULL, /* no FullName needed */
			       &Res2);
    if(!ParentDir1)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    ParentDir2 = LRootLocateDir(g, 
			       fl2, cbstr2, 0 /* not CString */, 
			       &Name2, NULL,
			       NULL,NULL, /* no FullName needed */
			       &Res2);
    if(!ParentDir2)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    /* we need to know if directory or file and we need the fileid
       to see if open locks */
    /* FIXME: is it possible on Amige to rename locked file?
       */
    NEnt = GetNameEntry(g, ParentDir1, Name1, &ACEnt, &Res2);
    if(NEnt)
    {
	/* FIXME: currently: don\'t allow rename if locks are still open 
	 but this is wrong, FFS does allow remame on locked files,
	 solution: do rename and fix NFSFh of all locks */
	if(!lock_LookupFromKey(g, NEnt->ne_FileId))
	{
	    if(nfs_Rename(g->g_NFSClnt, 
			  &ParentDir1->dce_NFSFh, Name1, 
			  &ParentDir2->dce_NFSFh, Name2, &Res2))
	    {
		/* remove cached entry */
		dc_DCEDeleteFromFileId(g, NEnt->ne_FileId);
		attrs_Delete(g, NEnt->ne_FileId);
		dc_NERemove(g, ParentDir1, NEnt);
		SetRes1(g, DOSTRUE);
	    }
	    else
		SetRes(g, DOSFALSE, Res2);
	}
	else
	    SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
    }
    else
	SetRes(g, DOSFALSE, Res2);

 exit:
    fn_Delete(&Name1);
    fn_Delete(&Name2);
    
    return g->g_Res1;
}

LONG act_CHANGE_MODE(Global_T *g, DOSPKT *pkt)
{
    LONG ObjType = pkt->dp_Arg1;
    LONG NewMode = pkt->dp_Arg3;

    LOCK *fl;
    ELOCK *efl;
    FHANDLE *fh;
    EFH *efh;

    if(!((ObjType == CHANGE_FH) || (ObjType != CHANGE_LOCK)) ||
       !((NewMode == SHARED_LOCK) || (NewMode == EXCLUSIVE_LOCK)))
	return SetRes(g, DOSFALSE, ERROR_OBJECT_WRONG_TYPE);

    switch(ObjType)
    {
 case CHANGE_FH:
	fh = (FHANDLE *) pkt->dp_Arg2;
	efh = fh_Lookup(g, fh->fh_Arg1);
	if(efh)
	{
	    if(NewMode == SHARED_LOCK)
	    {
		if(efh->efh_ELock->efl_Lock.fl_Access == SHARED_LOCK)
		    /* already ok */
		    SetRes1(g, DOSTRUE);
		else
		{
		    if(lock_NumLocksFromKey(g, efh->efh_ELock->efl_Lock.fl_Key)
		       > 1)
			SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
		    else
		    {
			SetRes1(g, DOSTRUE);
			efh->efh_ELock->efl_Lock.fl_Access = SHARED_LOCK;
		    }
		}
	    }
	    else
	    {
		/* EXCLUSIVE_LOCK */
		if(efh->efh_ELock->efl_Lock.fl_Access == EXCLUSIVE_LOCK)
		    /* already ok */
		    SetRes1(g, DOSTRUE);
		else
		{
		    if(lock_NumLocksFromKey(g, efh->efh_ELock->efl_Lock.fl_Key)
		       > 1)
			SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
		    else
		    {
			SetRes1(g, DOSTRUE);
			efh->efh_ELock->efl_Lock.fl_Access = EXCLUSIVE_LOCK;
		    }
		}
	    }
	}
	else
	    SetRes(g, DOSFALSE, ERROR_FILE_NOT_OBJECT); /* FIXME: correct ? */
	break;
 case CHANGE_LOCK:
	fl = (LOCK *) pkt->dp_Arg2;
	efl = lock_Lookup(g, fl);
	if(efl)
	{
	    if(NewMode == SHARED_LOCK)
	    {
		if(efl->efl_Lock.fl_Access == SHARED_LOCK)
		    /* already ok */
		    SetRes1(g, DOSTRUE);
		else
		{
		    if(lock_NumLocksFromKey(g, efl->efl_Lock.fl_Key)
		       > 1)
			SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
		    else
		    {
			SetRes1(g, DOSTRUE);
			efl->efl_Lock.fl_Access = SHARED_LOCK;
		    }
		}
	    }
	    else
	    {
		/* EXCLUSIVE_LOCK */
		if(efl->efl_Lock.fl_Access == EXCLUSIVE_LOCK)
		    /* already ok */
		    SetRes1(g, DOSTRUE);
		else
		{
		    if(lock_NumLocksFromKey(g, efl->efl_Lock.fl_Key)
		       > 1)
			SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
		    else
		    {
			SetRes1(g, DOSTRUE);
			efl->efl_Lock.fl_Access = EXCLUSIVE_LOCK;
		    }
		}
	    }
	}
	else
	    SetRes(g, DOSFALSE, ERROR_FILE_NOT_OBJECT); /* FIXME: correct ? */
	break;
    }

    return g->g_Res1;
}

LONG
act_CREATE_OBJECT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg2;
    LONG Type = pkt->dp_Arg3;
    LONG subtype1 = pkt->dp_Arg4;
    
    UBYTE *Name;
    LONG NameLen;
    LONG Res2;
    ULONG UMode, ULen;

    diropokres *dir;
    DCEntry *ParentDir;

    /* e.g. create dir can just be supported without
       returning/creating a lock !
     */
       
    if((Type != ST_BDEVICE) && (Type != ST_CDEVICE))
	return SetRes(g, DOSFALSE, ERROR_BAD_NUMBER);
    
    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0 /* not CString */, 
			       &Name, &NameLen,
			       NULL, NULL, &Res2);
    if(!ParentDir)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    /* we just try to create special file and leave it to the server
       to inform us about errors like "already exists" etc. */

    UMode = nfsc_ApplyUMask(g, 0777);
    switch(Type)
    {
 case ST_BDEVICE:
  #warning "what about auS_IFBLK?"
	//UMode |= auS_IFBLK;
	ULen = subtype1;
	break;
 case ST_CDEVICE:
	//UMode |= auS_IFCHR;
	ULen = subtype1;
	break;
    }
    dir = nfs_Create(g->g_NFSClnt, 
		    &ParentDir->dce_NFSFh, Name,
		    nfsc_GetUID(g), nfsc_GetGID(g), 
		    UMode, ULen, &Res2);
    if(dir)
    {
	SetRes1(g, DOSTRUE);
    }
    else
	SetRes(g, DOSFALSE, Res2);

 exit:
    fn_Delete(&Name);
    
    return g->g_Res1;
}
