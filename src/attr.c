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
 * Misc functions changinf file attributes.
 */

#include "nfs_handler.h"
#include "protos.h"

/* amiga <-> unix conversion stuff */

//#include <aulib/aulib.h>
//#include <proto/aulib.h>

#include "Debug.h"

LONG act_SET_PROTECT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg2;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg3;
    LONG Mask = pkt->dp_Arg4;

    UBYTE *Name;
    LONG Res2;

    fattr *newattr;
    diropokres *file;
    unsigned int u_mode;
    DCEntry *ParentDir;

    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0, /* not C-String*/
			       &Name, NULL,
			       NULL, NULL,
			       &Res2);
    if(!ParentDir)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    /* can use this because unix mode == nfs mode */
    u_mode = au_Protection2umode(Mask);

    file = nfs_Lookup(g->g_NFSClnt, &ParentDir->dce_NFSFh, Name,
		      g->g_NFSGlobal.ng_MaxReadDirSize, &Res2);
    if(file)
    {
	if(newattr = nfs_Chmod(g->g_NFSClnt, &file->file, u_mode, &Res2))
	{
	    attrs_Update(g, &file->file, newattr);
	    SetRes1(g, DOSTRUE);
	}
	else
	{
	    attrs_Delete(g, file->attributes.fileid);
	    SetRes(g, DOSFALSE, Res2);
	}
    }
    else
    {
	attrs_Delete(g, ParentDir->dce_FileId);
	SetRes(g, DOSFALSE, Res2);
    }

    fn_Delete(&Name);

 exit:    
    return g->g_Res1;
}

LONG act_SET_DATE(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg2;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg3;
    struct DateStamp *ds = (struct DateStamp *) pkt->dp_Arg4;

    UBYTE *Name;
    LONG Res2;

    fattr *newattr;
    diropokres *file;
    nfstime tv;
    DCEntry *ParentDir;

    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0, /* not C-String*/
			       &Name, NULL,
			       NULL, NULL,
			       &Res2);
    if(!ParentDir)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }
    
    DateStamp2timeval(ds, &tv);

    file = nfs_Lookup(g->g_NFSClnt, &ParentDir->dce_NFSFh, Name,
		      g->g_NFSGlobal.ng_MaxReadDirSize, &Res2);
    if(file)
    {
	if(newattr = nfs_SetTimes(g->g_NFSClnt, &file->file, &tv, &Res2))
	{
	    attrs_Update(g, &file->file, newattr);
	    SetRes1(g, DOSTRUE);
	}
	else
	{
	    attrs_Delete(g, file->attributes.fileid);
	    SetRes(g, DOSFALSE, Res2);
	}
    }
    else
    {
	attrs_Delete(g, ParentDir->dce_FileId);
	SetRes(g, DOSFALSE, Res2);
    }

    fn_Delete(&Name);

 exit:    
    return g->g_Res1;
}

LONG act_SET_OWNER(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg2;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg3;
    ULONG OwnerGroup = pkt->dp_Arg4;

    UWORD Owner, Group;

    UBYTE *Name;
    LONG Res2;

    fattr *newattr;
    diropokres *file;
    DCEntry *ParentDir;

    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0, /* not C-String*/
			       &Name, NULL,
			       NULL, NULL,
			       &Res2);
    if(!ParentDir)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }
    
    Owner = (OwnerGroup>>16) & 0xFFFFU;
    Group = OwnerGroup & 0xFFFFU;

    file = nfs_Lookup(g->g_NFSClnt, &ParentDir->dce_NFSFh, Name,
		      g->g_NFSGlobal.ng_MaxReadDirSize, &Res2);
    if(file)
    {
	if(newattr = nfs_SetUidGid(g->g_NFSClnt, &file->file, 
				   Owner, Group, &Res2))
	{
	    attrs_Update(g, &file->file, newattr);
	    SetRes1(g, DOSTRUE);
	}
	else
	{
	    attrs_Delete(g, file->attributes.fileid);
	    SetRes(g, DOSFALSE, Res2);
	}
    }
    else
    {
	attrs_Delete(g, ParentDir->dce_FileId);
	SetRes(g, DOSFALSE, Res2);
    }

    fn_Delete(&Name);

 exit:    
    return g->g_Res1;
    
}

