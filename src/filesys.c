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
 * Special filesystem functions.
 */

#include "nfs_handler.h"
#include "protos.h"

#include "chdebug.h"

LONG act_IS_FILESYSTEM(Global_T *g, DOSPKT *pkt)
{
    return SetRes1(g, DOSTRUE);
}

LONG act_WRITE_PROTECT(Global_T *g, DOSPKT *pkt)
{
    LONG Flag = pkt->dp_Arg1;
    ULONG Key = pkt->dp_Arg2;

    LONG DoLock = (Flag == DOSTRUE);

    if(DoLock)
	if(g->g_Flags & GF_IS_WRITE_PROTECTED)
	    SetRes(g, DOSFALSE, ERROR_DISK_WRITE_PROTECTED);
	else
	{
	    g->g_Flags |= GF_IS_WRITE_PROTECTED;
	    g->g_LockKey = Key;
	    SetRes1(g, DOSTRUE);
	}
    else /* unlock */
	if(g->g_Flags & GF_IS_WRITE_PROTECTED)
	{
	    if(Key != g->g_LockKey)
		SetRes(g, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
	    else
	    {
		g->g_Flags &= ~GF_IS_WRITE_PROTECTED;
		SetRes1(g, DOSTRUE);
	    }
	}
	else
	    SetRes1(g, DOSTRUE);

    return g->g_Res1;
}

static LONG
FlushProc(Global_T *g, EFH *efh, void *arg)
{
    LONG Res2;

    if(HasBufferedWData(efh))
    {
	if(wc_FlushCache(g, efh, &Res2) == DOSFALSE)
	{
	    chassert(Res2);
	    efh->efh_Res2 = Res2;
	    SetRes(g, DOSFALSE, Res2);
	}
    }
    return 1; /* continue loop */
}

LONG
act_FLUSH(Global_T *g, DOSPKT *pkt)
{
    fh_ForEach(g, FlushProc, NULL);

    return SetRes1(g, DOSTRUE);
}

LONG
act_CURRENT_VOLUME(Global_T *g, DOSPKT *pkt)
{
    return SetRes(g, MKBADDR(g->g_Dev), 0);
}

static LONG
RemReadBuf(Global_T *g, EFH *efh, void *arg)
{
    rb_ReleaseRBufData(g, efh);
    
    return 1; /* continue loop */
}

/*
 * Remove all ReadBufs
 */

void
RemReadBufs(Global_T *g)
{
    fh_ForEach(g, FlushProc, NULL);
}

/*
 * mean size of a cache entry
 * 	NODESIZE (8) + KeySize (4) + pointer(4) + datasize
 */
 
#define ATTRENTRYSIZE	(8+4+4+112)

/* mean size of a file name: 10 */
#define DIRENTRYSIZE	(16+44+10+2*20)

#define IsSlowMedium(g) (g->g_Flags & GF_SLOW_MEDIUM)
/*
 * distribute buffers among different caches
 *
 * allocation scheme will be different according to timeout values
 */
void
DistributeBuffers(Global_T *g,
		  LONG NumBufs, LONG *PMBufs, LONG *PAttrs, LONG *PDirs)
{
    LONG BufMem, MBufs, Attrs, Dirs;
    LONG BigTimeout = 0;

    BufMem = NumBufs * 512;
    /*
     * FIXME: The memory distribution scheme may change
     *
     * current scheme:
     *
     * big timeouts:
     *	1/2 buffers
     *	1/2* 1/2 AttrCache
     *	1/2* 1/2 DirCache
     *
     * small timeouts:
     * as above but maximums:
     *		AttrCache: 200
     *		DirCache:  200
     */

    if((g->g_AttrCacheTimeout >= (4* ATTR_CACHE_TIMEOUT_DEFAULT)) ||
       (g->g_DirCacheTimeout >= (4* DIR_CACHE_TIMEOUT_DEFAULT)))
    {
	BigTimeout = 1;
    }
    
    MBufs = (BufMem / 2) / MBUFSIZE;
    Attrs = (BufMem / 4) / ATTRENTRYSIZE; 
    Dirs = (BufMem / 4) / DIRENTRYSIZE; 

    if(! BigTimeout)
    {
	if(Attrs > 200) Attrs = 200; /* FIXME: make constant */
	if(Dirs > 200) Dirs = 200;
    }
    if(IsSlowMedium(g))
    {
	/* slow medium */
	if(((MBufs * MBUFSIZE) / 8192) > 50)
	    MBufs = 50;
    }
    BufMem -= MBufs * MBUFSIZE;
    BufMem -= Attrs * ATTRENTRYSIZE;
    BufMem -= Dirs * DIRENTRYSIZE;

    if(! BigTimeout && ! IsSlowMedium(g))
    {
	LONG l;
	/* priority on buffers on "fast" lines */

	l = BufMem / MBUFSIZE;
	if(l)
	{
	    MBufs += l;
	    BufMem -= l*MBUFSIZE;
	}
    }
    /* distribute rest among caches */
    while(BufMem >= 0)
    {
	Attrs ++; Dirs++;
	BufMem -= (ATTRENTRYSIZE + DIRENTRYSIZE);
    }

    /* minimal values */

    if(Attrs < 20) Attrs = 20;	/* 20 Entries */
    if(Dirs < 20) Dirs = 20;	/* 20 Entries */

    *PMBufs = MBufs;
    *PAttrs = Attrs;
    *PDirs = Dirs;
}

LONG
act_MORE_CACHE(Global_T *g, DOSPKT *pkt)
{
    LONG Num = pkt->dp_Arg1;

    LONG NumBufs;

    if(Num == 0)
    {
	return SetRes(g, DOSTRUE, g->g_NumBuffers);
    }
    else
    {
	LONG MBufs, Attrs, Dirs, DiffMBufs, ok = TRUE;;
	
	NumBufs = g->g_NumBuffers + Num;
	if(NumBufs < 0)
	    NumBufs = 0;

	DistributeBuffers(g, NumBufs, &MBufs, &Attrs, &Dirs);

	DiffMBufs = MBufs - g->g_NumMBufs;
	
	if(DiffMBufs < 0)
	{
	    if((g->g_NumMBufs - MBufs) > g->g_NumFreeMBufs)
	    {
		act_FLUSH(g, NULL);
	    }
	    if((g->g_NumMBufs - MBufs) > g->g_NumFreeMBufs)
	    {
		RemReadBufs(g);
	    }
	    mb_FreeSomeMBufs(g, g->g_NumMBufs - MBufs);
	}
	else if(DiffMBufs > 0)
	{
	    ok = mb_AllocMoreMBufs(g, DiffMBufs);
	}
	if(ok)
	{
	    attrs_SetCacheSize(g, Attrs);
	    g->g_AttrCacheMaxEntries = Attrs;
	    dc_SetCacheSize(g, Dirs);
	    g->g_DirCacheMaxEntries = Dirs;

	    g->g_NumBuffers = NumBufs;
	}
	
	AKDEBUG((0,"\tNew Buffer/Cache values:\n"));
	AKDEBUG((0,"\t\tBuffers = %ld\n", g->g_NumBuffers));
	AKDEBUG((0,"\t\tMBufs   = %ld\n", g->g_NumMBufs));
	AKDEBUG((0,"\t\tAttrs   = %ld\n", g->g_AttrCacheMaxEntries));
	AKDEBUG((0,"\t\tDirs    = %ld\n", g->g_DirCacheMaxEntries));
	if(ok)
	{
	    return SetRes(g, DOSTRUE, g->g_NumBuffers);
	}
	else
	{
	    return SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
	}
    }
}

LONG
act_RENAME_DISK(Global_T *g, DOSPKT *pkt)
{
    BSTR newname = MKBADDR(pkt->dp_Arg1);
    BSTR bnewvolname;
    UBYTE *newvolname;
    
    if(newname)
    {
	bnewvolname = DupBSTR(newname);
	newvolname = StrNDup(((UBYTE *)newname)+1,((UBYTE *)newname)[0]);
	    
	if(bnewvolname && newvolname)
	{
	    DelBSTR(g->g_Dev->dl_Name);
	    fn_Delete(&g->g_VolumeName);
	    
	    g->g_Dev->dl_Name = bnewvolname;
	    g->g_VolumeName = newvolname;
	}
	else
	{
	    if(bnewvolname)
		DelBSTR(bnewvolname);
	    if(newvolname)
		fn_Delete(&newvolname);
	}
    }
	
    return SetRes1(g, DOSTRUE);
}
