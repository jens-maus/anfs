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
 * Filehandle storing and retrieval 
 */

#include "nfs_handler.h"
#include "protos.h"

EFH *fh_Create(Global_T *g, ELOCK *elock, LONG Mode)
{
    static int Id;
    EFH *efh;

    efh = AllocVec(sizeof(*efh), MEMF_CLEAR | MEMF_PUBLIC);
    if(efh)
    {
	efh->efh_Id = ++Id;
	efh->efh_Mode = Mode;
	efh->efh_ELock = elock;
	efh->efh_Res2 = 0;
    }

    return efh;
}

void fh_Insert(Global_T *g, EFH *efh)
{
    efh->efh_Next = g->g_FH;
    g->g_FH = efh;
}

EFH *fh_Lookup(Global_T *g, LONG Id)
{
    EFH *efh;

    efh = g->g_FH;
    while(efh)
    {
	if(efh->efh_Id == Id)
	    break;
	efh = efh->efh_Next;
    }

    return efh;
}

EFH *fh_Remove(Global_T *g, EFH *arg_efh)
{
    EFH *efh, **pefh;

    pefh = &g->g_FH;
    efh = *pefh;
    while(efh)
    {
	if(efh == arg_efh)
	{
	    *pefh = efh->efh_Next;
	    break;
	}
	pefh = &efh->efh_Next;
	efh = *pefh;
    }

    return efh;
}

void fh_Delete(Global_T *g, EFH **efh)
{
    if(efh && *efh)
    {
	FreeVec(*efh);
	*efh = NULL;
    }
    
}

/* call given function with given argument for each filehandle
 *
 * - the given function may return 0 to break loop
 */

void
fh_ForEach(Global_T *g, fh_ForEachProc_T foreachproc, void *arg)
{
    EFH *efh, *nefh;

    efh = g->g_FH;
    while(efh)
    {
	nefh = efh->efh_Next;
	if(foreachproc(g, efh, arg) == 0)
	    break;
	efh = nefh;
    }
}
