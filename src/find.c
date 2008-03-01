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
 * Implementation of file open actions.
 */


#define FINDF_DO_CREATE 	1
#define FINDF_DO_TRUNCATE 	2
#define FINDF_EXCLUSIVE		4
#define FINDF_MUST_EXIST	8

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#include "Debug.h"

#if 0
static LONG act_FIND(Global_T *g, DOSPKT *pkt, LONG AMode,
		     LONG u_mode, LONG Flags)
{
    FHANDLE *fh = (FHANDLE *) pkt->dp_Arg1;
    LOCK *fl = (LOCK *) pkt->dp_Arg2;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg3;

    diropokres *dres = NULL;
    LONG Res2;
    EFH *efh;
    ELOCK *elock = NULL; /* important ! */
    DCEntry *ParentDir;

    UBYTE *Name, *FullName, *cstr;
    LONG NameLen, FullNameLen, Len /* dummy */;

    Res2 = g->g_Res2;

    fn_B2CStr(cbstr, &cstr, &Len);
    if(!cstr)
    {
	return SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
    }

    /* FIXME: LRootLocateDir and RootLocateAndLock are too much ! */
    ParentDir = LRootLocateDir(g,
			       fl, cbstr, 0 /* not C-String */,
			       &Name, &NameLen,
			       &FullName, &FullNameLen,
			       &Res2);
    if(!ParentDir)
	return SetRes(g, DOSFALSE, Res2);

    /* FIXME: Name, FullName needs to be freed on function return ! */
    /* try to get a lock */

    /* FIXME: tests need to be cleaned up, not all cases are possible */

    if(Flags & FINDF_MUST_EXIST)
    {
	if(Flags & FINDF_EXCLUSIVE)
	    elock = RootLocateAndLock(g, fl, cstr, EXCLUSIVE_LOCK, &Res2);
	else
	    elock = RootLocateAndLock(g, fl, cstr, SHARED_LOCK, &Res2);
    }
    else
    {
	#warning "This FIND case needs to be fixed!"
	return SetRes(g, DOSFALSE, ERROR_NOT_IMPLEMENTED);

	dres = nfs_Lookup(g->g_NFSClnt, 
			  &ParentDir->dce_NFSFh, Name,
			  &Res2);
	/* FIXME: not clean here, we assume
	 *  - dres failed: no entry (may be other reasons!)
	 *  - dres o.k.:   has entry 
	 */
	if(dres)
	{
	    elock = lock_LookupFromKey(g, dres->attributes.fileid);
	    if(elock)
	    {
		if((Flags & FINDF_EXCLUSIVE) || 
		   (elock->efl_Lock.fl_Access == EXCLUSIVE_LOCK))
		{
		    elock = NULL;
		    SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
		    goto errorexit;
		}
		elock = NULL;
	    }
	}
#if 0 /* FIXME */
	if(Flags & FINDF_EXCLUSIVE)
	    elock = DoLock(g, fl, cbstr, EXCLUSIVE_LOCK, DOLF_IGN_NOT_EXIST_ERR,
			   &u_path, &Res2);
	else
	    elock = DoLock(g, fl, cbstr, SHARED_LOCK, DOLF_IGN_NOT_EXIST_ERR,
			   &u_path, &Res2);
#endif
    }
    if(!elock)
	return SetRes(g, DOSFALSE, Res2);

    if(Flags & FINDF_DO_TRUNCATE)
    {
	dres = nfs_Create(g->g_NFSClnt, 
			  &ParentDir->dce_NFSFh, Name,
			  nfsc_GetUID(g),
			  nfsc_GetGID(g),
			  nfsc_ApplyUMask(g, 0777),
			  0, /* size */
			  &Res2);
    }
    else
    {
	dres = nfs_Lookup(g->g_NFSClnt, 
			  &ParentDir->dce_NFSFh, Name,
			  &Res2);
	if(!dres && (Flags & FINDF_DO_CREATE))
	    dres = nfs_Create(g->g_NFSClnt, 
			      &ParentDir->dce_NFSFh, Name,
			      nfsc_GetUID(g),
			      nfsc_GetGID(g),
			      nfsc_ApplyUMask(g, 0777),
			      -1, /* size */
			      &Res2);
    }

    if(dres)
    {
	if(dres->attributes.mode & u_mode)
	{
	    CopyFH(&dres->file, &elock->efl_NFSFh);
	    elock->efl_NFSType = NFREG;
	    elock->efl_Lock.fl_Key = dres->attributes.fileid;

	    efh = fh_Create(g, elock, AMode);
	    if(efh)
	    {
		fh_Insert(g, efh);
		fh->fh_Arg1 = efh->efh_Id;
		efh->efh_FileLen = dres->attributes.size;
		CopyFH(&dres->file, &efh->efh_NFSFh);

		SetRes1(g, DOSTRUE);
	    }
	    else
		SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
	}
	else
	    SetRes(g, DOSFALSE, ERROR_READ_PROTECTED);
    }
    else
    {
	SetRes(g, DOSFALSE, Res2);
    }

 errorexit:
    if(g->g_Res1 == DOSFALSE)
    {
	if(elock)
	{
	    elock = lock_Remove(g, elock);
	    lock_Delete(g, &elock);
	}
    }
 exit:
    fn_Delete(&Name);
    fn_Delete(&FullName);

    if(g->g_Res1 != DOSFALSE)
    {
	D(DBF_ALWAYS,"\tfile arg1 = %08lx\n", fh->fh_Args);
    }
    return g->g_Res1;
}
#endif

LONG act_FINDREADWRITE(Global_T *g, DOSPKT *pkt, LONG FileMode, LONG Mode)
{
    FHANDLE *fh = (FHANDLE *) pkt->dp_Arg1;
    LOCK *fl = (LOCK *) pkt->dp_Arg2;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg3;

    diropokres *dres = NULL;
    LONG Res1 = DOSFALSE;
    LONG Res2 = g->g_Res2;
    EFH *efh;
    ELOCK *elock;
    DCEntry *ParentDir;

    UBYTE *Name, *FullName;
    LONG NameLen, FullNameLen;

    /* FIXME: LRootLocateDir and RootLocateAndLock are too much ! */
    ParentDir = LRootLocateDir(g,
			       fl, cbstr, 0 /* not C-String */,
			       &Name, &NameLen,
			       &FullName, &FullNameLen,
			       &Res2);
    if(!ParentDir)
	return SetRes(g, DOSFALSE, Res2);

    /* FIXME: Name, FullName needs to be freed on function return ! */
    /* try to get a lock */

    /* FIXME: tests need to be cleaned up, not all cases are possible */

    /* 1. Check for old file and possible old locks */
    dres = nfs_Lookup(g->g_NFSClnt, 
		      &ParentDir->dce_NFSFh, Name,
		      g->g_NFSGlobal.ng_MaxReadDirSize,
		      &Res2);
    if(!dres && (Res2 == ERROR_OBJECT_NOT_FOUND) &&
       (FileMode == MODE_READWRITE))
    {
	dres = nfs_Create(g->g_NFSClnt, 
			  &ParentDir->dce_NFSFh, Name,
			  nfsc_GetUID(g),
			  nfsc_GetGID(g),
			  nfsc_ApplyUMask(g, 0777),
			  -1, /* size */
			  &Res2);
    }

    if(dres)
    {
	if(dres->attributes.type != NFREG)
	{
	    /* FFS (2.04) like */
	    if(FileMode == MODE_READWRITE)
		Res2 = ERROR_OBJECT_WRONG_TYPE;
	    else
		Res2 = ERROR_OBJECT_EXISTS;
	    goto exit;
	}
	elock = lock_LookupFromKey(g, dres->attributes.fileid);
	if(elock)
	{
	    if((Mode == EXCLUSIVE_LOCK) ||
	       (elock->efl_Lock.fl_Access == EXCLUSIVE_LOCK))
	    {
		Res2 = ERROR_OBJECT_IN_USE;
		goto errorexit;
	    }
	    elock = NULL;
	}
	elock = lock_Create(g,
			    FullName, FullNameLen,
			    Name, NameLen,
			    dres->attributes.type,
			    Mode);

	/* FIXME: remove file on error */
	if(!elock)
	{
	    Res2 = ERROR_NO_FREE_STORE;
	}
	else
	{
	    CopyFH(&dres->file, &elock->efl_NFSFh);
	    elock->efl_NFSType = NFREG;
	    elock->efl_Lock.fl_Key = dres->attributes.fileid;

	    efh = fh_Create(g, elock, FileMode);
	    if(!efh)
	    {
		lock_Delete(g, &elock);
		Res2 = ERROR_NO_FREE_STORE;
	    }
	    else
	    {
		fh_Insert(g, efh);
		lock_Insert(g, elock);
		fh->fh_Arg1 = efh->efh_Id;
		efh->efh_FileLen = dres->attributes.size;
	
		Res1 = DOSTRUE; Res2 = 0;
	    }
	}
    }
    else
    {
	/* Res2 = Res2 */
    }
 errorexit:
 exit:
    fn_Delete(&Name);
    fn_Delete(&FullName);

    if(Res1 != DOSFALSE)
    {
	D(DBF_ALWAYS,"\tFile Arg1 = %08lx\n", fh->fh_Args);
	rb_CreateRBuf(efh);
    }
    
    return SetRes(g, Res1, Res2);
}

/* 
 * Mode			lock		delete	create
 *
 * MODE_READWRITE	EXCLUSIVE_LOCK	no	yes
 *
 */

LONG act_FINDUPDATE(Global_T *g, DOSPKT *pkt) 
{ 
    return act_FINDREADWRITE(g, pkt, MODE_READWRITE, EXCLUSIVE_LOCK); 
}

/* 
 * Mode			lock		delete	create
 *
 * MODE_OLDFILE		SHARED_LOCK	no	no
 *
 */

LONG act_FINDINPUT(Global_T *g, DOSPKT *pkt)
{
    return act_FINDREADWRITE(g, pkt, MODE_OLDFILE, SHARED_LOCK);
}

LONG act_FH_FROM_LOCK(Global_T *g, DOSPKT *pkt)
{
    FHANDLE *fh = (FHANDLE *) pkt->dp_Arg1;
    LOCK *fl = (LOCK *) pkt->dp_Arg2;

    fattr *attr = NULL;
    LONG Res2;
    EFH *efh;
    ELOCK *elock;
    LONG FileMode;

    Res2 = g->g_Res2;

    elock = lock_Lookup(g, fl);
    if(!elock)
    {
	return SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);
    }

    if(elock->efl_Lock.fl_Access == EXCLUSIVE_LOCK)
	FileMode = MODE_READWRITE;
    else
	FileMode = MODE_OLDFILE;

    /* we need file size */
    attr = nfs_GetAttr(g->g_NFSClnt, 
		       &elock->efl_NFSFh,
		       &Res2);

    if(attr)
    {
	if(attr->type != NFREG)
	{
	    SetRes(g, DOSFALSE, ERROR_OBJECT_WRONG_TYPE);
	}
	else
	{
	    efh = fh_Create(g, elock, FileMode);
	    if(!efh)
	    {
		SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
	    }
	    else
	    {
		fh_Insert(g, efh);
		fh->fh_Arg1 = efh->efh_Id;
		efh->efh_FileLen = attr->size;
	
		SetRes1(g, DOSTRUE);
	    }
	}
    }
    else
    {
	SetRes(g, DOSFALSE, Res2);
    }

    if(g->g_Res1 != DOSFALSE)
    {
	D(DBF_ALWAYS,"\tFile Arg1 = %08lx\n", fh->fh_Args);
	rb_CreateRBuf(efh);
    }

    return g->g_Res1;
}

/* 
 * Mode			lock		delete	create
 *
 * MODE_NEWFILE		EXCLUSIVE_LOCK	yes	yes
 *
 */

/* FIXME: error handling could be better (remove created files etc... */

LONG
act_FINDOUTPUT(Global_T *g, DOSPKT *pkt)
{
    FHANDLE *fh = (FHANDLE *) pkt->dp_Arg1;
    LOCK *fl = (LOCK *) pkt->dp_Arg2;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg3;

    LONG AMode = MODE_NEWFILE;
    diropokres *dres = NULL;
    LONG Res2;
    EFH *efh;
    ELOCK *elock = NULL; /* important ! */
    DCEntry *ParentDir;

    UBYTE *Name, *FullName;
    LONG NameLen, FullNameLen;

    Res2 = g->g_Res2;

    /* FIXME: LRootLocateDir and RootLocateAndLock are too much ! */
    ParentDir = LRootLocateDir(g,
			       fl, cbstr, 0 /* not C-String */,
			       &Name, &NameLen,
			       &FullName, &FullNameLen,
			       &Res2);
    if(!ParentDir)
	return SetRes(g, DOSFALSE, Res2);

    /* FIXME: Name, FullName needs to be freed on function return ! */
    /* try to get a lock */

    /* 1. Check for old file and possible old locks */
    dres = nfs_Lookup(g->g_NFSClnt, 
		      &ParentDir->dce_NFSFh, Name,
		      g->g_NFSGlobal.ng_MaxReadDirSize,
		      &Res2);
    /* 
     * FIXME: not clean here, we assume
     *  - dres failed: no entry (may be other reasons!)
     *  - dres o.k.:   has entry 
     */
    if(dres)
    {
	if(dres->attributes.type != NFREG)
	{
	    SetRes(g, DOSFALSE, ERROR_OBJECT_EXISTS);
	    goto exit;
	}
	elock = lock_LookupFromKey(g, dres->attributes.fileid);
	if(elock)
	{
	    elock = NULL; /* else this (used !) elock will be removed ! */
	    
	    SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
	    goto errorexit;
	}
	/* I don\'t check for remove result, real failures will be later
	   true on create, too */
	nfs_Remove(g->g_NFSClnt, 
		   &ParentDir->dce_NFSFh, Name,
		   &Res2);
    }
    dres = nfs_Create(g->g_NFSClnt, 
		      &ParentDir->dce_NFSFh, Name,
		      nfsc_GetUID(g),
		      nfsc_GetGID(g),
		      nfsc_ApplyUMask(g, 0777),
		      0, /* size */
		      &Res2);
    if(!dres)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }
    elock = lock_Create(g,
			FullName, FullNameLen,
			Name, NameLen,
			dres->attributes.type,
			AMode);
    /* FIXME: remove file on error */
    if(!elock)
    {
	SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
	goto exit;
    }
    D(DBF_ALWAYS, "\tfile lock = 0x%08lx\n", elock);
    CopyFH(&dres->file, &elock->efl_NFSFh);
    elock->efl_NFSType = NFREG;
    elock->efl_Lock.fl_Key = dres->attributes.fileid;

    efh = fh_Create(g, elock, AMode);
    if(!efh)
    {
	SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
	goto exit;
    }
    D(DBF_ALWAYS, "\tfile handle = 0x%08lx\n", efh);
    fh_Insert(g, efh);
    lock_Insert(g, elock);
    fh->fh_Arg1 = efh->efh_Id;
    efh->efh_FileLen = dres->attributes.size;
	
    SetRes1(g, DOSTRUE);

 errorexit:
 exit:
    if(g->g_Res1 == DOSFALSE)
    {
	if(elock)
	{
	    elock = lock_Remove(g, elock);
	    lock_Delete(g, &elock);
	}
    }
    fn_Delete(&Name);
    fn_Delete(&FullName);

    if(g->g_Res1 != DOSFALSE)
    {
	if(g->g_BufSizePerFile)
	    wc_CreateCache(efh, g->g_BufSizePerFile/MBUFSIZE);
	rb_CreateRBuf(efh);
	D(DBF_ALWAYS,"\tfile arg1 = %08lx\n", fh->fh_Args);
    }

    return g->g_Res1;
}

LONG act_END(Global_T *g, DOSPKT *pkt)
{
    LONG Arg1 = pkt->dp_Arg1;
    
    EFH *efh;
    ELOCK *elock;
    LONG Res2;
    LONG BufferError = 0;

    efh = fh_Lookup(g, Arg1);
    if(efh)
    {
	BufferError = efh->efh_Res2;
	efh->efh_Res2 = 0; /* FIXME: this may be wrong ??? */

	if(HasWBuf(efh))
	{
	    if(HasBufferedWData(efh))
	    {
		if(wc_FlushCache(g, efh, &Res2) == DOSFALSE)
		    SetRes(g, DOSFALSE, Res2);
		else
		    SetRes1(g, DOSTRUE);
	    }
	    else
		SetRes1(g, DOSTRUE);
	    wc_DeleteCache(efh);
	}
	else
	    SetRes1(g, DOSTRUE);

	if(HasRBuf(efh))
	{
	    if(HasBufferedRData(efh))
	    {
		rb_ReleaseRBufData(g, efh);
	    }
	    rb_DeleteRBuf(efh);
	}
	
	elock = efh->efh_ELock;
	if(!elock)
	{
	    E(DBF_ALWAYS, "elock == NULL");
	}
	else
	{
	    ELOCK *xelock;
	    
	    xelock = lock_Remove(g, elock);
	    if(!xelock)
	    {
		    E(DBF_ALWAYS, "elock not in list !");
		    E(DBF_ALWAYS, "elock = 0x%08lx", elock);
	    }
	    else
		lock_Delete(g, &xelock);
	}

	efh = fh_Remove(g, efh);
	if(!efh)
	{
	    E(DBF_ALWAYS, "efh == NULL");
	}
	else
	    fh_Delete(g, &efh);
    }
    else
	SetRes(g, DOSFALSE, ERROR_FILE_NOT_OBJECT); /* FIXME: correct ? */

    if(BufferError)
    {
	SetRes(g, DOSFALSE, BufferError);
    }

    return g->g_Res1;
}

LONG act_SET_FILE_SIZE(Global_T *g, DOSPKT *pkt)
{
    LONG Arg1 = pkt->dp_Arg1;
    LONG Offset = pkt->dp_Arg2;
    LONG Mode = pkt->dp_Arg3;

    LONG Res2;
    LONG NewSize;
    EFH *efh;
    fattr *NewAttr;
    LONG Abort = 0;
    
    efh = fh_Lookup(g, Arg1);
    if(efh)
    {
	if(HasBufferedWData(efh))
	{
	    if(wc_FlushCache(g, efh, &Res2) == DOSFALSE)
	    {
		SetRes(g, -1, Res2);
		Abort = 1;
	    }
	}
	/* FIXME: we currently don\'t allow this function if more
	   than 1 filehandle is open */
	
	if(!Abort && (
	    lock_NumLocksFromKey(g, efh->efh_ELock->efl_Lock.fl_Key) > 1))
	{
	    SetRes(g, -1, ERROR_OBJECT_IN_USE);
	}
	else
	{
	    switch(Mode)
	    {
	case OFFSET_BEGINNING:
		NewSize = Offset;
		break;
	case OFFSET_CURRENT:
		NewSize = efh->efh_FilePos + Offset;
		break;
	case OFFSET_END:
		NewSize = efh->efh_FileLen + Offset;
		break;
	default:
		NewSize = -1;
	    }
	    if(NewSize == -1)
		SetRes(g, -1, ERROR_REQUIRED_ARG_MISSING); /* FIXME */
	    else if( NewSize < 0)
		SetRes(g, -1, ERROR_SEEK_ERROR);
	    else if((NewAttr = nfs_SetSize(g->g_NFSClnt,
					   &efh->efh_ELock->efl_NFSFh,
					   NewSize, &Res2)) != NULL)
	    {
		attrs_Update(g, &efh->efh_ELock->efl_NFSFh, NewAttr);
		if(NewAttr->size < efh->efh_FilePos)
		    efh->efh_FilePos= NewAttr->size;
		efh->efh_FileLen = NewAttr->size;

		SetRes1(g, efh->efh_FileLen); 
	    }
	    else
		SetRes(g, -1, Res2);
	}    
    }
    else
	SetRes(g, -1, ERROR_FILE_NOT_OBJECT); /* FIXME: correct ? */

    return g->g_Res1;    
}

LONG act_COPY_DIR_FH(Global_T *g, DOSPKT *pkt)
{
    LONG Arg1 = pkt->dp_Arg1;

    EFH *efh;
    
    efh = fh_Lookup(g, Arg1);
    if(efh)
	return do_act_COPY_DIR(g, efh->efh_ELock);
    else
	return SetRes(g, -1, ERROR_FILE_NOT_OBJECT); /* FIXME: correct ? */
}

LONG act_PARENT_FH(Global_T *g, DOSPKT *pkt)
{
    LONG Arg1 = pkt->dp_Arg1;

    EFH *efh;
    
    efh = fh_Lookup(g, Arg1);
    if(efh)
	return do_act_PARENT(g, efh->efh_ELock);
    else
	return SetRes(g, -1, ERROR_FILE_NOT_OBJECT); /* FIXME: correct ? */
}

LONG act_EXAMINE_FH(Global_T *g, DOSPKT *pkt)
{
    LONG Arg1 = pkt->dp_Arg1;
    FIB *fib = (FIB *) pkt->dp_Arg2;

    EFH *efh;
    
    efh = fh_Lookup(g, Arg1);
    if(efh)
	return do_act_EXAMINE_OBJECT(g, efh->efh_ELock, fib);
    else
	return SetRes(g, -1, ERROR_FILE_NOT_OBJECT); /* FIXME: correct ? */
}




