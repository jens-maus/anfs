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

#include <string.h>
#include <math.h> /* for min */

#include "nfs_handler.h"

#include "protos.h"

#include "Debug.h"

/*
 * returns TRUE if n is between low and high (inclusive)
 */

static __inline LONG
IsInclBetween(LONG n, LONG low, LONG high)
{
    return ((low <= n) && (n <= high));
}

void
mb_FreeSomeMBufs(Global_T *g, LONG NumBufs)
{
    MBUFNODE *mbn;

    ASSERT(NumBufs > 0)
    ASSERT(NumBufs <= g->g_NumMBufs);
    ASSERT(NumBufs <= g->g_NumFreeMBufs);

    g->g_NumMBufs -= NumBufs;
    g->g_NumFreeMBufs -= NumBufs;

    while(NumBufs > 0)
    {
	mbn = g->g_FreeMBufs;
	g->g_FreeMBufs = mbn->mbn_Next;
	FreeMem(mbn, MBUFSIZE);
	NumBufs--;
    }
}

void
mb_FreeMBufs(Global_T *g)
{
    if(g->g_NumMBufs)
	mb_FreeSomeMBufs(g, g->g_NumMBufs);
}

/*
 * returns TRUE on success
 */

LONG
mb_AllocMoreMBufs(Global_T *g, LONG NumBufs)
{
    register LONG i;
    register MBUFNODE *mbn, *mbn_head;

    mbn_head = g->g_FreeMBufs;
    i = NumBufs;
    while(i--)
    {
	mbn = AllocMem(MBUFSIZE, MEMF_PUBLIC);
	if(!mbn)
	{
	    LONG NumNew = NumBufs-(i+1);
	    
	    g->g_FreeMBufs = mbn_head;
	    g->g_NumMBufs += NumNew;
	    g->g_NumFreeMBufs += NumNew;
	    mb_FreeSomeMBufs(g, NumNew);
	    return FALSE;
	}
	mbn->mbn_Next = mbn_head;
	mbn_head = mbn;
    }
    g->g_FreeMBufs = mbn_head;
    g->g_NumMBufs += NumBufs;
    g->g_NumFreeMBufs += NumBufs;

    return TRUE;
}

LONG
mb_AllocMBufs(Global_T *g, LONG NumBufs)
{
    g->g_FreeMBufs = NULL;
    g->g_NumMBufs = g->g_NumFreeMBufs = 0;

    return mb_AllocMoreMBufs(g, NumBufs);
}

MBUFNODE *
mb_ObtainMBuf(Global_T *g)
{
    MBUFNODE *mbn;

    if(g->g_NumFreeMBufs)
    {
	g->g_NumFreeMBufs--;
    
	mbn = g->g_FreeMBufs;
	
	g->g_FreeMBufs = mbn->mbn_Next;
	
    }

    return mbn;
}

void
mb_ReleaseMBuf(Global_T *g, MBUFNODE *mbn)
{
    if(mbn)
    {
	g->g_NumFreeMBufs++;

	mbn->mbn_Next = g->g_FreeMBufs;
	g->g_FreeMBufs = mbn;
    }
}

