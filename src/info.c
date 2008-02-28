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

#include "nfs_handler.h"
#include "protos.h"

/* FIXME: IsInUse() must be updated if cache is introduced */
#define IsInUse(g) ((g->g_Dev->dl_LockList) || (g->g_FH))

static LONG do_act_INFO(Global_T *g, IDATA *id, nfs_fh *fh)
{
    statfsokres *st;
    LONG Res2;
    
    st = nfs_statfs(g->g_NFSClnt, fh, &Res2);
    if(!st)
	return SetRes(g, DOSFALSE, Res2);

    id->id_NumSoftErrors = 0;	/* number of soft errors on disk */
    id->id_UnitNumber = 0;	/* Which unit disk is (was) mounted on */
    /* FIXME for read only mounts */
    if(g->g_Flags & GF_IS_WRITE_PROTECTED)
	id->id_DiskState = ID_WRITE_PROTECTED;
    else
	id->id_DiskState = ID_VALIDATED;
    id->id_NumBlocks = st->blocks; /* Number of blocks on disk */
    /* Number of block in use */
    id->id_NumBlocksUsed = st->blocks-st->bavail;
    id->id_BytesPerBlock = st->bsize;
    /* FIXME */
    id->id_DiskType = g->g_Dev->dl_DiskType; /* Disk Type code */
    id->id_VolumeNode = MKBADDR(g->g_Dev);   /* BCPL pointer to volume node */

    /* Flag, zero if not in use */
    id->id_InUse = IsInUse(g);

    return SetRes1(g, DOSTRUE);
}

LONG act_INFO(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    IDATA *id = (IDATA *) pkt->dp_Arg2;
    
    if(!fl)			/* assuming root lock meant */
    {
	return do_act_INFO(g, id, &g->g_MntFh);
    }
    else
    {
	ELOCK *elock;

	elock = lock_Lookup(g, fl);
	if(!elock)
	    return SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);
	return do_act_INFO(g, id, &elock->efl_NFSFh);
    }
}

LONG act_DISK_INFO(Global_T *g, DOSPKT *pkt)
{
    IDATA *id = (IDATA *) pkt->dp_Arg1;

    return do_act_INFO(g, id, &g->g_MntFh);
}
