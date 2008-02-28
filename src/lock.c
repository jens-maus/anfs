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

#include <stdio.h>

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#include "chdebug.h"

ELOCK *DupELock(Global_T *g,
		ELOCK *OldLock, 
		UBYTE *Name, UBYTE *FullName,
		LONG Mode,
		LONG *Res2)
{
    LONG NameLen;
    ELOCK *NewELock;

    if(OldLock)
    {
	if(OldLock->efl_Lock.fl_Access != SHARED_LOCK)
	{
	    *Res2 = ERROR_OBJECT_IN_USE;
	    return NULL;
	}
	if(Mode != SHARED_LOCK)
	{
	    *Res2 = ERROR_OBJECT_IN_USE;
	    return NULL;
	}
    }
    else
    {
	/* we don\'t allow exclusive locks on ROOT: */
	if(Mode != SHARED_LOCK)
	{
	    *Res2 = ERROR_OBJECT_IN_USE;
	    return NULL;
	}
    }
    chassert(Name);
    NameLen = strlen(Name);

    /* we don\'t use lock_Dup here to avoid freeing probably wrong Name */

    NewELock = lock_Create(g, 
			   FullName, strlen(FullName), /* FIXME: len? */
			   Name, NameLen, 
			   NFNON,
			   Mode);
    if(NewELock)
    {
	if(OldLock)
	{
	    CopyFH(&OldLock->efl_NFSFh, &NewELock->efl_NFSFh);
	    NewELock->efl_Lock.fl_Key = OldLock->efl_Lock.fl_Key;
	    NewELock->efl_NFSType = OldLock->efl_NFSType;
	}
	else
	{
	    /* new root lock */
	    CopyFH(&g->g_MntFh, &NewELock->efl_NFSFh);
	    NewELock->efl_Lock.fl_Key = g->g_MntAttr.fileid;
	    NewELock->efl_NFSType = g->g_MntAttr.type;
	}
    }
    else
	*Res2 = ERROR_NO_FREE_STORE;
    
    return NewELock;
}

/* 
 * FileId may be unset (DCE_ID_UNUSED)
 * NFSFh must always be set
 */

ACEntry *c_GetAttr(Global_T *g, u_int FileId, nfs_fh *NFSFh, LONG *Res2)
{
    ACEntry *ACEnt;
    fattr *attr;

    if(FileId != DCE_ID_UNUSED)
    {
	ACEnt = attrs_LookupFromFileId(g, FileId);
	if(ACEnt)
	return ACEnt;
    }
    
    attr = nfs_GetAttr(g->g_NFSClnt, NFSFh, Res2);
    if(!attr)
	return NULL;

    FileId = attr->fileid;

    ACEnt = attrs_LookupFromFileId(g, FileId);
    if(ACEnt)
	ACEnt = attrs_Update(g, NFSFh, attr);

    if(!ACEnt)
	ACEnt = attrs_CreateAndInsert(g, NFSFh, attr);
    if(!ACEnt)
	*Res2 = ERROR_NO_FREE_STORE;

    AKDEBUG((0,"\tc_GetAttr: new: attr.fileid = 0x%08lx\n", attr->fileid));
    return ACEnt;
}

/* 
 * get name entry for given name in given directory.
 * will use nfs-functions and update cache if necessary.
 *
 * ActDir will be modified to contain NameEntry if necessary
 */

/* FIXME: introduce timeout for name entries too ? */
/* FIXME: change name to reflect output of ACEntry (make proc?) */
   
NEntry *GetNameEntry(Global_T *g,
		     DCEntry *ActDir, /* INPUT */
		     const UBYTE *Name, /* INPUT */
		     ACEntry **ACEP, /* OUTPUT */
		     LONG *Res2) /* OUTPUT */
{
    diropokres *dirent;
    NEntry *NEnt;
    ACEntry *ACE;

    AKDEBUG((0,"\tGetNameEntry(0x%08lx,\"%s\")\n", ActDir->dce_FileId, Name));

    NEnt = dc_NELookup(g, ActDir, Name);
    if(NEnt)
    {
	AKDEBUG((0,"\t\tNameEntry: cache ok!\n"));

	if(ACEP) /* attribute cache entry wanted */
	{
	    ACE = attrs_LookupFromFileId(g, NEnt->ne_FileId);
	    if(ACE)
	    {
		*ACEP = ACE;
		return NEnt;
	    }
	}
	else
	    return NEnt;
    }

    dirent = nfs_Lookup(g->g_NFSClnt, &ActDir->dce_NFSFh, Name,
			g->g_NFSGlobal.ng_MaxReadDirSize, Res2);
    if(!dirent)
	return NULL;

    /* insert attributes into cache, key: nfs fileid */

    ACE = attrs_LookupFromFileId(g, dirent->attributes.fileid);
    if(ACE)
	ACE = attrs_Update(g,
			   &dirent->file,
			   &dirent->attributes);
    if(!ACE)
	ACE = attrs_CreateAndInsert(g,
				    &dirent->file,
				    &dirent->attributes);
    if(ACE == NULL)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }
    if(ACEP)
	*ACEP = ACE;

    /* 
     * insert cache entry for name into directory ActDir,
     * key: (ActDir,name)
     */
    if(!NEnt)
    {
	NEnt = dc_NECreateAndInsert(g,
				ActDir,
				Name,
				dirent->attributes.type,
				ACE->ace_FileId);
    }
    else
    {
	/* Name entry already present, just update file id in case it changed */
	/* FIXME: makes update sense ? */
	NEnt->ne_FType = dirent->attributes.type;
	NEnt->ne_FileId = ACE->ace_FileId;
    }
    if(!NEnt)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }

    return NEnt;
}

/* 
 * get directory described by NFSFh,
 * directory name is Name
 * use nfs calls if necessary 
 */

DCEntry *GetDirEntry(Global_T *g,
		     NEntry *ParName, /* name entry of this dir or NULL */
		     u_int ActDirId, /* if ParName==NULL: Id of this dir or 
				       unset */
		     nfs_fh *ActDirFh,
		     UBYTE *Name, /* currently unused */
		     LONG *Res2)
{
    ACEntry *ACEnt = NULL;
    DCEntry *ActDir;
#if 0
    NEntry *NEnt;
#endif

    if(ParName != NULL)
	ActDirId = ParName->ne_FileId;

    AKDEBUG((0, "\tGetDirEntry(0x%08lx)\n", ActDirId));

    if(ActDirId != DCE_ID_UNUSED)
    {
	/* try to get info from cache */
	ActDir = dc_DCELookupFromFileId(g, ActDirId);
	if(ActDir)
	    return ActDir; /* success */
    }
    /* do it the hard way */

    AKDEBUG((0,"\t\tDCE Cache lookup, no entry\n"));
#if 0 /* FIXME */
    if(ParName)
    {
	if(ACEnt = attrs_LookupFromFileId(g, ParName->ne_FileId))
	    AttrId = ParName->ne_FileId;
    }
#endif

    ACEnt = c_GetAttr(g, ActDirId, ActDirFh, Res2);
    if(!ACEnt)
    {
	AKDEBUG((0,"\t\tc_GetAttr failed\n"));
	return NULL;
    }

    /* assumption: ACEnt points to ACEntry of this dir */

    ActDir = dc_DCECreateAndInsert(g, 
				   ACEnt->ace_FileId,
				   ActDirFh);
    if(ActDir == NULL)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }

    return ActDir;
}

/* 
  this functions gets a filelock and a path and creates the resulting
  path
  Input:
  	fl	pointer to filelock, legal to be NULL

  Output:
  	**DirLock	Pointer to lock to which name is relative to or
			NULL if absolute
  	*FullNameLen	Length of new full filename
  	

  Return:
	  returns pointer to fullname if o.k., NULL otherwise
*/

UBYTE *
CBuildFullName(Global_T *g,
	      LOCK *fl, UBYTE *cstr, /* INPUT */
	      ELOCK **DirLock, /* OUTPUT */
	      LONG *ArgFullNameLen, /* OUTPUT */
	      UBYTE **ArgName,
	      LONG *ArgNameLen,
	      UBYTE **UPath,
	      LONG *Res2) /* OUTPUT if ERR */
{
    UBYTE *pathWithoutDev = NULL, *u_path = NULL;
    UBYTE *OldFullName, *FullName = NULL, *Name = NULL;
    LONG OldFullNameLen, NameLen, FullNameLen;
    LONG IsAbsolute, StrippedDirs = 0;
    ELOCK *oldelock = NULL;
    LONG LocalRes2 = 0;

    chassert(Res2);

    if(fl == NULL)		/* relative to root */
    {
	OldFullName = "";
	OldFullNameLen = 0;
    }
    else
    {
	/* relative to other lock */

	oldelock = lock_Lookup(g, fl);
	if(!oldelock)
	{
	    *Res2 = ERROR_INVALID_LOCK;
	    return NULL;
	}

	OldFullName = oldelock->efl_FullName;
	OldFullNameLen = oldelock->efl_FullNameLen;
    }

    /* FIXME: this must be simpler without so much copying */

    /* pathWitouthdev need not be copied anymore but may be a 
       pointer into APath */

    pathWithoutDev = fn_PathWithoutDev(cstr, &IsAbsolute, Res2);
    if(!pathWithoutDev)
	return NULL;

    /*
     * following (strange?) behaviour:
     *
     *	assign x: nfs:dir1/dir2
     *		-> lock1
     *	list x:
     *		-> Lock(lock1,"x:", ....)
     *
     *	so we ignore this case for now
     *
     */
    if((IsAbsolute && pathWithoutDev[0]) 	/* x:<something> */
       && (fl && (OldFullNameLen != 0)))	/* non-root filelock */
    {
	UBYTE *s;
	
	/* need to compare absolute path with our Volumename */

	s = strchr(cstr, ':');
	if(strnicmp(cstr, g->g_VolumeName, s-cstr) == 0)
	{
	    /* handle like root lock, pathwithoutdev contains absolute
	       path */
		
	    oldelock = NULL;
	    OldFullName = "";
	    OldFullNameLen = 0;
	}
	else
	{
	    /* ignore "x:" and handle rest as relative to lock */
#if 0
	    AKDEBUG((0,"\tcstr = %s\n", cstr));
	    AKDEBUG((0,"\tpathWithoutDev = %s\n", pathWithoutDev));
	    AKDEBUG((0,"\tOldFullName = %s\n", OldFullName));
		
	    FIXME("IsAbsolute && fl has happened!\n");
#endif
	}
    }

    /*
      Current philosophy:
      for simplicity I use always the full pathname when accessing 
      a file. This may change in the future.
      */

    u_path = fn_AM2UN(pathWithoutDev);
 
    if(u_path)
    {
	UBYTE *s=NULL;

	fn_AddPart(OldFullName, OldFullNameLen,
		   pathWithoutDev, -1, 
		   &s, &FullNameLen);
	if(s)
	{
	    /* FIXME: Unify could work "in-place" */

	    FullName = fn_PathUnify(s, &FullNameLen, &StrippedDirs,
				    &LocalRes2);

	    if(FullName && StrippedDirs)
	    {
		fn_Delete(&u_path);
		u_path = fn_AM2UN(FullName);
	    }
	    fn_Delete(&s);
	}
	if(FullName)
	    fn_FilePart(FullName, FullNameLen, &Name, &NameLen);
	if(Name && (NameLen > MAX_LONGAMIGAFILENAMELEN))
	{
	    fn_Delete(&u_path);
	    fn_Delete(&FullName);
	    fn_Delete(&Name);

	    *Res2 = ERROR_INVALID_COMPONENT_NAME;

	    return NULL;
	}
    }

    fn_Delete(&pathWithoutDev);

    if(!u_path || !FullName || !Name)
    {
	fn_Delete(&u_path);
	fn_Delete(&FullName);
	fn_Delete(&Name);

	if(LocalRes2)
	    *Res2 = LocalRes2;
	else
	    *Res2 = ERROR_NO_FREE_STORE;

	return NULL;
    }

    /* FIXME: possibly make this default! */
    /* need to use full path because some dirs were stripped */
/*
  make this default for now
  if(StrippedDirs)
  */
    {
	oldelock = NULL;
    }

    if(UPath)
	*UPath = u_path;
    else
	fn_Delete(&u_path);
    
    if(ArgFullNameLen)
	*ArgFullNameLen = FullNameLen;
    if(ArgName)
	*ArgName = Name;
    else
	fn_Delete(&Name);
    if(ArgNameLen)
	*ArgNameLen = NameLen;

    if(DirLock)
	*DirLock = oldelock;

    return FullName;
}

UBYTE *
BuildFullName(Global_T *g,
	      LOCK *fl, CBSTR cbstr, /* INPUT */
	      ELOCK **DirLock,
	      LONG *ArgFullNameLen, /* OUTPUT */
	      UBYTE **ArgName,
	      LONG *ArgNameLen,
	      UBYTE **UPath,
	      LONG *Res2) /* OUTPUT if ERR */
{
    UBYTE *cstr;
    UBYTE *Res;
    LONG Len;

    chassert(Res2);
    chassert(cbstr);

    fn_B2CStr(cbstr, &cstr, &Len);
    if(!cstr)
    {
	*Res2 = ERROR_NO_FREE_STORE;

	return NULL;
    }

    Res = CBuildFullName(g, fl, cstr, DirLock,
			  ArgFullNameLen,
			  ArgName, ArgNameLen,
			  UPath, Res2);
    fn_Delete(&cstr);

    return Res;
}


/*
 * RootLocateDir()
 *
 * This functions implements the FilePath walk to directory containing
 * file specified via FullFileName.
 * It returns a pointer to a cache entry describing the containing
 * directory or NULL on error.
 *
 */

DCEntry *
RootLocateDir(Global_T *g, 
	      UBYTE *FullName,
	      LONG FullNameLen,
	      LONG *Res2) /* OUTPUT */
{
    UBYTE *PComp; /* points to current path component */
    UBYTE *WorkPath = NULL;
    DCEntry *ParDir, *ActDir;
    NEntry *ActNEnt;
    ACEntry *ACE;
    LONG LastElement = 0; /* flag: if set, last element of directory is found */
    /* (init important !) */
    LONG Pos = 0; /* fn_ScanPath needs this as buffer */
    /* (init important !) */

    AKDEBUG((0, "\tRootLocateDir(\"%s\")\n", FullName));

    WorkPath = StrNDup(FullName, FullNameLen); /* we will modify WorkPath */
    if(!WorkPath)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	goto errorexit;
    }
    /* get (cached) root handle */
    ParDir = GetDirEntry(g, 
			 NULL, /* no name entry */
			 g->g_MntAttr.fileid,
			 &g->g_MntFh,
			 NULL, /* currently not used */
			 Res2);
    if(!ParDir)
	goto errorexit;

    if(FullNameLen) /* Filename != "" */
    {
	do
	{
	    PComp = fn_ScanPath(WorkPath, FullNameLen, &Pos, &LastElement);
	    chassert(PComp);
	    if(LastElement)
	    {
		AKDEBUG((0,"\tLast PComp = \"%s\"\n", PComp));
		break;
	    }
	    AKDEBUG((0,"\tPComp = \"%s\"\n", PComp));
	    if(*PComp == '/')	/* goto parent dir, not allowed in this 
				   version, must be unified path ! */
	    {
		FIXME(("*** parent dir reference not allowed !!\n"));
		break;
	    }
	    ActNEnt = GetNameEntry(g, ParDir, PComp, &ACE, Res2);
	    if(!ActNEnt)
		goto errorexit;

	    if((ActNEnt->ne_FType != NFDIR)
	       && (ActNEnt->ne_FType != NFLNK))
	    {
		*Res2 = ERROR_INVALID_COMPONENT_NAME;
		goto errorexit;
	    }
	    if(ActNEnt->ne_FType == NFLNK)
	    {
		*Res2 = ERROR_IS_SOFT_LINK; /* FIXME: check other filesystems */
		goto errorexit;
	    }
	    /* note: GetDirEntry will modify ActNEnt if necessary */
	    ActDir = GetDirEntry(g, 
				 ActNEnt,
				 DCE_ID_UNUSED,
				 &ACE->ace_NFSFh,
				 PComp,
				 Res2);
	    ParDir = ActDir;
	    if(!ParDir)
	    {
		goto errorexit;
	    }
	} while(1);
	/* assumption: "ParDir" points to directory containing entry "PComp" */
    }
    else
    { /* filename: "" */
    }

    fn_Delete(&WorkPath);    
    return ParDir;

 errorexit:
    if(WorkPath)
	fn_Delete(&WorkPath);    
    return NULL;
}

/*
 * CLRootLocateDir()
 *
 * Like RootLocate, but starts with (Lock,Path)-pair
 *
 * It returns a pointer to a cache entry describing the containing
 * directory or NULL on error;
 */

DCEntry *
LRootLocateDir(Global_T *g, 
	       LOCK *fl, 
	       UBYTE *RelPath,  /* B- or C-String */
	       LONG IsCString,
	       UBYTE **PName, /* OUTPUT, if non NULL */
	       LONG *PNameLen, /* OUTPUT, if non NULL */
	       UBYTE **PFullName, /* OUTPUT, if non NULL */
	       LONG *PFullNameLen, /* OUTPUT, if non NULL */
	       LONG *Res2) /* OUTPUT */
{
    UBYTE *FullName = NULL, *Name = NULL;
    LONG FullNameLen, NameLen;
    ELOCK *DirLock;
    DCEntry *ParDir;
    

    if(IsCString)
    {
	AKDEBUG((0, "\tLRootLocateDir(\"%s\")\n", RelPath));
	FullName = CBuildFullName(g,
				  fl, RelPath,
				  &DirLock, /* needed ? */
				  &FullNameLen,
				  &Name, &NameLen,
				  NULL,
				  Res2);
    }
    else
    {
#ifdef DEBUG
	UBYTE *cstr = NULL;
	LONG Len;

	fn_B2CStr(RelPath, &cstr, &Len);
	if(cstr)
	{
	    AKDEBUG((0, "\tLRootLocateDir(\"%s\")\n", cstr));
	    fn_Delete(&cstr);
	}
#endif
	FullName = BuildFullName(g,
				  fl, RelPath,
				  &DirLock, /* needed ? */
				  &FullNameLen,
				  &Name, &NameLen,
				  NULL,
				  Res2);
    }
    if(!FullName)
	return NULL;

    AKDEBUG((0,"\tFullName = \"%s\"\n", FullName));

    ParDir = RootLocateDir(g, FullName, FullNameLen, Res2);
    if(!ParDir)
	goto errorexit;
    
    if(PName)
	*PName = Name;
    else
	fn_Delete(&Name);
    if(PNameLen) *PNameLen = NameLen;
    if(PFullName)
	*PFullName = FullName;
    else
	fn_Delete(&FullName);
    if(PFullNameLen) *PFullNameLen = FullNameLen;

    return ParDir;
 errorexit:
    if(FullName)
	fn_Delete(&FullName);
    if(Name)
	fn_Delete(&Name);
    return NULL;
}
/*
 * RootLocateAndLock()
 *
 * This functions implements the FilePath walk to file starting
 * with root.
 * It returns a lock to this file if possible, else NULL.
 */

ELOCK *
RootLocateAndLock(Global_T *g, 
	   LOCK *fl, 
	   UBYTE *RelPath,  /* CString */
	   LONG Mode, /* INPUT: SHARED/EXCLUSIVE */
	   LONG *Res2) /* OUTPUT */
{
    UBYTE *FullName = NULL, *Name = NULL;
    LONG FullNameLen, NameLen;
    ELOCK *DirLock;
    DCEntry *ParDir;
    NEntry *ActNEnt;
    ACEntry *ACE;
    ELOCK *newelock;
    ELOCK *oldelock = NULL; /* (init important !) */

    AKDEBUG((0, "\tRootLocateAndLock(name = \"%s\")\n", RelPath));

    /* test for illegal modes */
    if(!((Mode == SHARED_LOCK) || (Mode == EXCLUSIVE_LOCK)))
    {
	*Res2 = ERROR_REQUIRED_ARG_MISSING;
	return DOSFALSE;
    }

    FullName = CBuildFullName(g,
			      fl, RelPath,
			      &DirLock, /* needed ? */
			      &FullNameLen,
			      &Name,
			      &NameLen,
			      NULL,
			      Res2);
    if(!FullName)
    {
	return DOSFALSE;
    }
    AKDEBUG((0,"\tFullName = \"%s\"\n", FullName));

    ParDir = RootLocateDir(g, FullName, FullNameLen, Res2);
    if(!ParDir)
	goto errorexit;

    if(FullNameLen)
    {
	ActNEnt = GetNameEntry(g, ParDir, Name, &ACE, Res2);
	if(!ActNEnt)
	{
	    goto errorexit;
	}
	oldelock = lock_LookupFromKey(g, ACE->ace_FAttr.fileid);
	if(oldelock)
	{
	    AKDEBUG((0,"\told Lock found\n"));
	    if(!IsSameFH(&ACE->ace_NFSFh, &oldelock->efl_NFSFh))
	    {
		AKDEBUG((1, "possible inconsistency, LocateObject, different FHs\n"));
	    }
	    if(ACE->ace_FAttr.type != oldelock->efl_NFSType)
	    {
		/* filetype has changed in the mean time ! */
		/* we just ignore this for now and take actual values,
		   if someone uses the changed lock later this will
		   fail hopefully itself */
	    	AKDEBUG((1, "possible inconsistency, file type changed\n"));
	    }
	}
	else
	{
	    AKDEBUG((0,"\tno old Lock found\n"));
	}
    }    
    /* Note: oldelock may be NULL here,
       this will be true for root locks and completely new locks */
    
    /* FIXME: attribute setting and cases below may be cleaned up */
    if(oldelock)
    {
	newelock = DupELock(g, oldelock, Name, FullName, Mode, Res2);
    }
    else
    {
	if(FullNameLen)
	{
	    /* completely new lock */
	    if(ACE->ace_FAttr.type == NFLNK)
	    {
		*Res2 = ERROR_IS_SOFT_LINK;
		newelock = NULL;
	    }
	    else
	    {
		newelock = lock_Create(g, FullName, FullNameLen,
				       Name, NameLen,
				       ACE->ace_FAttr.type,
				       Mode);
		if(!newelock)
		    *Res2 = ERROR_NO_FREE_STORE;
	    }
	} 
	else
	{
	    /* duplicate root lock */
	    newelock = DupELock(g, oldelock, Name, FullName, Mode, Res2);
	}
    }
    if(!newelock)
    {
	goto errorexit;
    }

    if(FullNameLen)
    {
	chassert(ACE);
	CopyFH(&ACE->ace_NFSFh, &newelock->efl_NFSFh);
	newelock->efl_NFSType = ACE->ace_FAttr.type;
	newelock->efl_Lock.fl_Key = ACE->ace_FAttr.fileid;
    }
    else
    {
	if(!oldelock)
	{
	    fattr *attr = NULL;
	    ACEntry *ACEnt;
	    /* FIXME: use cache (insert) */

	    ACEnt = attrs_LookupFromFileId(g, g->g_MntAttr.fileid);
	    if(ACEnt)
		attr = &ACEnt->ace_FAttr;
	    if(!attr)
		attr = nfs_GetAttr(g->g_NFSClnt, &g->g_MntFh, Res2);
	    if(!attr)
	    {
		lock_Delete(g, &newelock);
		goto errorexit;
	    }
	    CopyFH(&g->g_MntFh, &newelock->efl_NFSFh);
	    
	    newelock->efl_Lock.fl_Key  = attr->fileid;
	    newelock->efl_NFSType = attr->type;
	}
    }
    
    lock_Insert(g, newelock);
    fn_Delete(&FullName);
    fn_Delete(&Name);
    return newelock;

 errorexit:
    if(FullName)
	fn_Delete(&FullName);
    if(Name)
	fn_Delete(&Name);
    return NULL;
}

/* 
   FFS:
   an exclusive lock on a directory is allowed and
   and has the same semantics as a file lock 
   a directory lock does not put restrictions on its contents 
   (creating/deleting files/dirs)
*/
   
LONG
act_LOCATE_OBJECT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg2;
    LONG Mode = pkt->dp_Arg3;

    LONG Res2;
    ELOCK *elock;
    UBYTE *Path;
    LONG l; /* FIXME */

    fn_B2CStr(cbstr, &Path, &l);
    if(!Path)
	return SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
    
    if(elock = RootLocateAndLock(g, fl, Path, Mode, &Res2))
	SetRes(g, MKBADDR(elock), 0);
    else
	SetRes(g, DOSFALSE, Res2);

    fn_Delete(&Path);

    return g->g_Res1;
}

LONG act_FREE_LOCK(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    ELOCK *elock;

    if(!fl)
	return SetRes1(g, DOSTRUE);

    elock = lock_Lookup(g, fl);
    if(!elock)
	return SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);

    elock = lock_Remove(g, elock);
    lock_Delete(g, &elock);

    return SetRes1(g, DOSTRUE);
}

LONG do_act_COPY_DIR(Global_T *g, ELOCK *elock)
{
    ELOCK *nel;

    if(!elock)
	return SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);

    if(elock->efl_Lock.fl_Access != SHARED_LOCK)
	return SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
    
    nel = lock_Dup(g, elock);

    if(!nel)
	return SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);

    lock_Insert(g, nel);

    return SetRes1(g, MKBADDR(nel));
}

LONG
act_COPY_DIR(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;

    /* FIXME: return real lock? */
    if(!fl)
	return SetRes(g, NULL, NULL);

    return do_act_COPY_DIR(g, lock_Lookup(g, fl));
}

LONG
do_act_PARENT(Global_T *g, ELOCK *oldelock)
{
    LOCK *fl;
    ELOCK *elock;

#if 1
    UBYTE *DirName;
    LONG DirNameLen;
#endif
    LONG Res2;

    if(!oldelock)
	return SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);

    AKDEBUG((0, "\tFullName = \"%s\"\n", oldelock->efl_FullName));

    if((oldelock->efl_FullNameLen == 0) ||
       (oldelock->efl_FullName[oldelock->efl_FullNameLen-1] == ':'))
	return SetRes(g, 0, 0); /* has no parent */

    fn_PathPart(oldelock->efl_FullName,
		oldelock->efl_FullNameLen,
		&DirName,
		&DirNameLen);

    if(!DirName)
	return SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);

    fl = NULL; /* start at root */
    if(elock = RootLocateAndLock(g, fl, DirName, SHARED_LOCK, &Res2))
    {
	AKDEBUG((0,"\tout = \"%s\"\n", elock->efl_FullName));
	SetRes(g, MKBADDR(elock), 0);
    }
    else
	SetRes(g, DOSFALSE, Res2);

    fn_Delete(&DirName);

    return g->g_Res1;
}

LONG act_PARENT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;

    return do_act_PARENT(g, lock_Lookup(g, fl));
}

LONG act_SAME_LOCK(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl1 = (LOCK*) pkt->dp_Arg1;
    LOCK *fl2 = (LOCK*) pkt->dp_Arg2;

    ELOCK *efl1, *efl2;

    efl1 = lock_Lookup(g, fl1);
    efl2 = lock_Lookup(g, fl2);

    if(!efl1 || !efl2)
	return SetRes(g, DOSFALSE, 0);

    /* FIXME: compare PATH ? but path compare may fail when file was renamed */
    if(efl1->efl_Lock.fl_Key == efl2->efl_Lock.fl_Key)
	return SetRes(g, DOSTRUE, 0);
    else
	return SetRes(g, DOSFALSE, 0);
}

