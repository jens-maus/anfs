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
 * some (?) cases the write cache must be flushed: (FIXME)
 *
 * - Read()s on file
 * - Close() on file
 * - SetFileSize() on file
 *
 * we (currently ?) don\'t flush on
 * Seek()s in file but test in WriteCache instead
 * => currently only succesive writes are buffered
 *
 */

static LONG
iwc_FlushCache(Global_T *g, EFH *efh, LONG IgnoreEmptyBlocks, LONG *Res2);

#define IGNORE_EMPTY_BUFS 1

/*
 * returns TRUE if n is between low and high (inclusive)
 */

static __inline LONG IsInclBetween(LONG n, LONG low, LONG high)
{
    return ((low <= n) && (n <= high));
}

/*
 * nothing will be done if no memory (just no cache is added)
 */

void
wc_CreateCache(EFH *efh, LONG MaxBufs)
{
    WBUFCLUSTER *wbc;

    wbc = AllocVec(sizeof(WBUFCLUSTER), MEMF_CLEAR | MEMF_PUBLIC);
    if(wbc)
    {
	    wbc->wbc_WBufsMax = MaxBufs;
	    D(DBF_ALWAYS,"\twc_CreateCache(%08lx, %08lx) -- ok (%08lx)", efh, MaxBufs, wbc);
    }
    else
    {
    	D(DBF_ALWAYS,"\twc_CreateCache(%08lx, %08lx) -- failed", efh, MaxBufs);
    }

    efh->efh_WBuf = wbc;
}

/*
 * cache must be emptied on call
 */

void
wc_DeleteCache(EFH *efh)
{
    WBUFCLUSTER *wbc;

    D(DBF_ALWAYS, "\twc_DeleteCache(%08lx)", efh);
    
    if(wbc = efh->efh_WBuf)
    {
	if(wbc->wbc_WBufsUsed)
	{
    E(DBF_ALWAYS, "FIXME: Internal error: wc_DeleteCache(), cache not empty");
	}
	FreeVec(efh->efh_WBuf);
	efh->efh_WBuf = NULL;
    }
}

void
wc_FreeWBufs(Global_T *g)
{
    WBUFHDR *wbh;

    ASSERT(g->g_NumFreeWBufHdrs == g->g_NumWBufHdrs);

    while(wbh = g->g_FreeWBufHdr)
    {
	g->g_FreeWBufHdr = wbh->wbh_Next;
	FreeVec(wbh);
    }
    g->g_FreeWBufHdr = NULL;
    g->g_NumWBufHdrs = 0;
    g->g_NumFreeWBufHdrs = 0;
}

/*
 * returns TRUE on success
 */

LONG
wc_AllocWBufs(Global_T *g, LONG NumBufs)
{
    register LONG i;
    register WBUFHDR *wbh, *wbh_head;

    wbh_head = NULL;
    i = NumBufs;
    while(i--)
    {
	wbh = AllocVec(sizeof(WBUFHDR), MEMF_CLEAR | MEMF_PUBLIC);
	if(!wbh)
	{
	    g->g_FreeWBufHdr = wbh_head;
	    g->g_NumWBufHdrs = g->g_NumFreeWBufHdrs = NumBufs-(i+1);
	    wc_FreeWBufs(g);
	    return FALSE;
	}
	wbh->wbh_Next = wbh_head;
	wbh_head = wbh;
    }
    g->g_FreeWBufHdr = wbh_head;
    
    g->g_NumWBufHdrs = g->g_NumFreeWBufHdrs = NumBufs;

    return TRUE;
}

static WBUFHDR *
ObtainWBuf(Global_T *g)
{
    WBUFHDR *wbh = NULL;
    MBUFNODE *mbn;

    if(g->g_NumFreeWBufHdrs && g->g_NumFreeMBufs)
    {
	mbn = mb_ObtainMBuf(g);
    
	g->g_NumFreeWBufHdrs--;
    
	wbh = g->g_FreeWBufHdr;
	g->g_FreeWBufHdr = wbh->wbh_Next;

	wbh->wbh_FileOffs = 0;
	wbh->wbh_NumInfo = 0;
	wbh->wbh_Data = (UBYTE *) mbn;
    }

    return wbh;
}

/* FIXME: wbufhdrs should be ready for use! (already 1-to-1) */

static void
ReleaseWBuf(Global_T *g, WBUFHDR *wbh)
{
    MBUFNODE *wbn;

    if(wbh)
    {
	g->g_NumFreeWBufHdrs++;

	wbn = (MBUFNODE *) wbh->wbh_Data;
	mb_ReleaseMBuf(g, wbn);
	
	wbh->wbh_Next = g->g_FreeWBufHdr;
	g->g_FreeWBufHdr = wbh;
    }
}

/*
 * returns -1 on error
 * length written else
 */

static LONG
FlushCacheAndWrite(Global_T *g,
		   EFH *efh,
		   UBYTE *Buf,
		   LONG Num,
		   LONG *Res2)
{
    LONG Res1;

    D(DBF_ALWAYS, "\tFlushCacheAndWrite()");
    
    /*
      possible problem:
      error on flushing can only be reported as
      write error on actual data
      */
    
    if(HasBufferedWData(efh) && (iwc_FlushCache(g, efh, IGNORE_EMPTY_BUFS, Res2) == DOSFALSE))
	return -1;

    if(g->g_NFSGlobal.ng_NumWRPCs > 1)
	/* ASync */
	Num = nfs_MAWrite(&g->g_NFSGlobal,
			  g->g_NFSClnt, 
			  &efh->efh_ELock->efl_NFSFh,
			  efh->efh_FilePos,
			  Buf,
			  Num,
			  &efh->efh_FileLen,
			  Res2);
    else
	/* Sync */
	Num = nfs_MWrite(&g->g_NFSGlobal,
			  g->g_NFSClnt, 
			  &efh->efh_ELock->efl_NFSFh,
			  efh->efh_FilePos,
			  Buf,
			  Num,
			  &efh->efh_FileLen,
			  Res2);
    if(Num >= 0)
    {
	Res1 = Num;
	*Res2 = 0;
    }
    else
    {
	Res1 = -1;
    }

    
    return Res1;
}

void WBufAddTail(WBUFCLUSTER *wbc, WBUFHDR *wbh)
{
    ASSERT(wbc);
    ASSERT(wbh);

    wbc->wbc_WBufsUsed++;
    wbh->wbh_Next = NULL;
    if(wbc->wbc_FirstData)
    {
	wbc->wbc_LastData->wbh_Next = wbh;
	wbc->wbc_LastData = wbh;
    }
    else
    {
	wbc->wbc_FirstData = wbc->wbc_LastData = wbh;
    }
}

void WBufAddHead(WBUFCLUSTER *wbc, WBUFHDR *wbh)
{
    ASSERT(wbc);
    ASSERT(wbh);

    wbc->wbc_WBufsUsed++;
	
    if(wbc->wbc_FirstData)
    {
	wbh->wbh_Next = wbc->wbc_FirstData;
	wbc->wbc_FirstData = wbh;
    }
    else
    {
	wbh->wbh_Next = NULL;
	wbc->wbc_FirstData = wbc->wbc_LastData = wbh;
    }
}

void WBufInsert(WBUFCLUSTER *wbc, WBUFHDR *oldwbh, WBUFHDR *wbh)
{
    ASSERT(wbc);
    ASSERT(wbh);
    
    if(oldwbh)
    {
	wbc->wbc_WBufsUsed++;
	wbh->wbh_Next = oldwbh->wbh_Next;
	oldwbh->wbh_Next = wbh;
	if(wbh->wbh_Next == NULL) /* was last element */
	    wbc->wbc_LastData = wbh;
    }
    else
	WBufAddHead(wbc, wbh);
}

WBUFHDR *WBufRemHead(WBUFCLUSTER *wbc)
{
    WBUFHDR *wbh = NULL;
    
    if(wbc)
    {
	if(wbc->wbc_WBufsUsed)
	{
	    wbc->wbc_WBufsUsed--;
	    wbh = wbc->wbc_FirstData;
	    wbc->wbc_FirstData = wbh->wbh_Next;
	}
    }
    return wbh;
}

#define LAST(wbc)  (wbc->wbc_LastData)
#define FIRST(wbc) (wbc->wbc_FirstData)
#define POS(wbc)   (wbc->wbc_PosData)
#define NEXT(wbh)  (wbh->wbh_Next)
#define TO(wbc)    (LAST(wbc)->wbh_Data + LAST(wbc)->wbh_Len)

static __inline LONG
WBufsAvail(Global_T *g)
{
    return min(g->g_NumFreeMBufs, g->g_NumFreeWBufHdrs);
}

#define WBufHdrLastPos(wbh) (wbh->wbh_FileOffs + MBUFSIZE)

#define WBufAlignFileOffs(pos) ((ULONG) pos & (0xFFFFFFFFU - (MBUFSIZE -1)))

static __inline LONG
IsInInfo(LONG offs,WBUFHDRINFO *wbi)
{
    ASSERT((wbi->wbi_To - wbi->wbi_From) > 0);
    
    return IsInclBetween(offs, wbi->wbi_From, wbi->wbi_To);
}

/* FIXME: replace Offs/Len with Start/End ? */

static void
WBufDoMelt(WBUFHDR *wbh, WBUFHDRINFO *wbi, LONG Offs, LONG To)
{
    LONG Num, i;
    WBUFHDRINFO *nwbi;

    i = (wbi - wbh->wbh_Info);
    ASSERT(i >= 0);
    Num = wbh->wbh_NumInfo;
    i++;
    nwbi = wbi+1;
    ASSERT(To+1 >= nwbi->wbi_From); /* one melt must be possible */

    ASSERT(i < Num); /* one info entry must be following */
    /* find end of melt area */
    /* Note: compare with (Num-1) because i++/nwbi++ must point to valid entry
       (Remember that i is the Index (0..(Num-1))) */
    while((i < (Num-1)) && (To > nwbi->wbi_To))
    {
	nwbi++; i++;
    }
    if(To+1 < nwbi->wbi_From) /* got too far */
    {
	nwbi--; i--;
    }
       
    wbh->wbh_NumInfo -= nwbi - wbi; /* num to melt */
    wbi->wbi_From = min(wbi->wbi_From, Offs);
    wbi->wbi_To = max(To, nwbi->wbi_To);
    if((i+1) < Num)
    {
	/* >1 entries are following, need to copy */
	memmove(wbi+1, nwbi+1, sizeof(*wbi) * (Num-i-1));
    }
    ASSERT(wbh->wbh_NumInfo);
}

/*
 *
 * basic cases:
 *
 * A.		[-----old------]
 *				[-----new------]
 *
 * B.				[-----old------]
 *		[-----new------]
 *
 * C.		[-----old------]
 *			[-----new------]
 *
 * D.			[-----old------]
 *		[-----new------]
 *
 * E.		[-----old------]
 *		[-----new------]
 *
 * F.		[-----old-------------------]
 *			[-----new------]
 *
 * G.			[-----old------]
 *		[-----new-------------------]
 *
 *
 * H.					[-----old------]
 *		[-----new------]
 *
 * most start cases (I.)
 *		[-----old------]
 *					[-----new------]
 */

static void
WBufUpdateInfo(WBUFHDR *wbh, LONG Offs, LONG Len)
{
    LONG i, Num;
    
    /* if the assertion below fails the CacheFlush() stuff in Write()
       needs to be checked */
    ASSERT(wbh->wbh_NumInfo < (MBUFSIZE-1));
	     
    if(wbh->wbh_NumInfo == 0)
    {
	wbh->wbh_NumInfo++;
	wbh->wbh_Info[0].wbi_From = Offs;
	wbh->wbh_Info[0].wbi_To = Offs + Len - 1;
    }
    else
    {
	WBUFHDRINFO *wbi;
	LONG To = Offs+Len-1;
	    
	/* locate insertion place */
	i = 0;
	Num = wbh->wbh_NumInfo;
	wbi = wbh->wbh_Info;

	/* +1 means "touch" (A,B) */
	while((i < Num) &&
	      !(Offs <= wbi->wbi_To+1))	/* A, B, C, D, E, F, G, H */
	{
	    /* I */
	    wbi++;
	    i++;
	}
	if(i < Num)
	{
	    /* A-H */
		
	    /* our "start" is less or equal wbi "end + 1" */
		
	    /* do we overlap at all ? */

	    if(To+1 < wbi->wbi_From)
	    {
		/* H, no overlap at all, just insert new info */

		memmove(wbi+1, wbi, sizeof(*wbi) * (Num-i));
		wbi->wbi_From = Offs;
		wbi->wbi_To = Offs + Len - 1;
		wbh->wbh_NumInfo++;
	    }
	    else
	    {
		/* A-G */
		/* Note: wbi[1] is successor of wbi */
		if((Num > (i+1)) && (To+1 >= wbi[1].wbi_From))
		{
		    /* minimum of one is following and melt is possible */
		    WBufDoMelt(wbh, wbi, Offs, To);
		}
		else
		{
		    /* no melt possible with follwing or at end */
			
		    wbi->wbi_From = min(wbi->wbi_From, Offs);
		    wbi->wbi_To = max(wbi->wbi_To, To);
		}
	    }
	}
	else
	{
	    /* I, add at tail */
	    
	    wbi->wbi_From = Offs;
	    wbi->wbi_To = Offs+Len-1;
	    wbh->wbh_NumInfo++;
	}
    }
    ASSERT(wbh->wbh_NumInfo);
}

#define WBufCacheFull(wbc) (wbc->wbc_WBufsMax <= wbc->wbc_WBufsUsed)

/*
 * returns -1: failure
 * returns  0: no mem
 * returns  1: ok
 */

/* FIXME: fix parameters */

static __inline LONG
WBufSeekAndPrepare(Global_T *g, EFH *efh, WBUFCLUSTER *wbc, LONG NewPos,
		   LONG *Res2)
{
    WBUFHDR *newwbh;
    WBUFHDR *wbh;
    LONG relpos;

#define ADDHEAD 0x1
#define ADDTAIL 0x2
#define INSERT_NEWBUFF  0x4
#define INSERT 0x8

    wbc->wbc_FilePos = NewPos;
    if(FIRST(wbc))
    {
	if(NewPos < FIRST(wbc)->wbh_FileOffs)
	{
	    /* new buffer must be allocated in this function
	       (see requirements of Write() below */
	    relpos = ADDHEAD;
	}
	else if(NewPos >= WBufHdrLastPos(LAST(wbc)))
	{
	    /* will be handled by Write() */
	    wbc->wbc_PosData = LAST(wbc);
	    
	    relpos = ADDTAIL;
	}
	else
	{
	    LONG Pos = NewPos - MBUFSIZE;
	    WBUFHDR *prev_wbh = NULL;

	    wbh = FIRST(wbc);
	    /* read: while(wbh->wbh_FileOffs + WBUFSIZE < NewPos) */
	    while(wbh->wbh_FileOffs < Pos)
	    {
		prev_wbh = wbh;
		wbh = NEXT(wbh);
		ASSERT(wbh);
	    }
	    if(wbh->wbh_FileOffs > NewPos)
	    {
		/* new buffer needed */
		relpos = INSERT_NEWBUFF;
		wbh = prev_wbh;
	    }
	    else
	    {
		relpos = INSERT;
	    }
	    ASSERT(wbh);
	    wbc->wbc_PosData = wbh;
	}
    }
    else
    {
	/* adding first WBuf */
	relpos = ADDHEAD;
    }

    if(relpos & (ADDHEAD))
    {
	if(WBufCacheFull(wbc))
	{
	    if(wc_FlushCache(g, efh, Res2) == DOSFALSE)
	    {
		return -1;
	    }
	}
	newwbh = ObtainWBuf(g);
	if(!newwbh && HasBufferedWData(efh))
	{
	    if(wc_FlushCache(g, efh, Res2) == DOSFALSE)
	    {
		return -1;
	    }
	    newwbh = ObtainWBuf(g);
	}
	if(!newwbh)
	    return 0;

	wbc->wbc_PosData = newwbh;
	    
	newwbh->wbh_FileOffs = WBufAlignFileOffs(NewPos);
	/* prepare info structure */
	newwbh->wbh_NumInfo = 0;
    }
	
    if(relpos & ADDHEAD)
    {
	WBufAddHead(wbc, newwbh);
    }

    return 1;
}



/*
 * returns number of bytes actually written
 * -1 means: error
 *  0 means: no mem
 *
 * require:
 *	Actual buffer position (PosData) must be exactly
 *		- on buffer to fill
 *		or
 *		- one before buffer to fill 
 *		or
 *		- one before place to insert new buffer
 */

/* FIXME: check/reset bufferinfo after flush! */
   
static __inline LONG
WBufWrite(Global_T *g, EFH *efh, WBUFCLUSTER *wbc, UBYTE *Buf, LONG Num,
	  LONG *Res2)
{
    UBYTE *To;
    LONG Cnt;
    WBUFHDR *wbh;
    LONG Pos, Offs, BufSize;

    ASSERT(POS(wbc));
    
    wbh = POS(wbc);
    Pos = wbc->wbc_FilePos;
    
    ASSERT(Pos >= wbh->wbh_FileOffs);

    if(Pos >= (wbh->wbh_FileOffs + MBUFSIZE))
    {
	if(wbh->wbh_Next  &&
	   IsInclBetween(Pos,
			 wbh->wbh_Next->wbh_FileOffs,
			 wbh->wbh_Next->wbh_FileOffs + MBUFSIZE - 1))
	{
	    /* next buf will be o.k. */
	    wbh = wbh->wbh_Next;
	}
	else
	{
	    WBUFHDR *newwbh;
	    /* need to add new WBuf */
	
	    if(WBufCacheFull(wbc))
	    {
		if(iwc_FlushCache(g, efh, IGNORE_EMPTY_BUFS, Res2) == DOSFALSE)
		    return -1;
		wbh = NULL; /* no longer valid */
	    }
	    newwbh = ObtainWBuf(g);
	    if(!newwbh && HasBufferedWData(efh))
	    {
		if(iwc_FlushCache(g, efh, IGNORE_EMPTY_BUFS, Res2) == DOSFALSE)
		    return -1;
		wbh = NULL; /* no longer valid */
		newwbh = ObtainWBuf(g);
	    }
	    if(!newwbh)
		return 0;
	
	    wbc->wbc_PosData = newwbh;
	    
	    newwbh->wbh_FileOffs = WBufAlignFileOffs(Pos);
	    /* prepare info structure */
	    newwbh->wbh_NumInfo = 0;

	    WBufInsert(wbc, wbh, newwbh);
	    wbh = newwbh;
	}
    }
    Offs = Pos - wbh->wbh_FileOffs;
    ASSERT(Offs >= 0);
    ASSERT(Offs < MBUFSIZE);

    BufSize = MBUFSIZE - Offs;
    To = wbh->wbh_Data + Offs;
    Cnt = min(BufSize, Num);
    CopyMem(Buf, To, Cnt);
    
    wbc->wbc_FilePos += Cnt;
    WBufUpdateInfo(wbh, Offs, Cnt);

    /* make sure one info structure is always available */

    ASSERT(wbc->wbc_PosData->wbh_NumInfo > 0);
    
    if((wbc->wbc_PosData->wbh_NumInfo+1) >= WBUFHDRINFOSIZE)
    {
	if(wc_FlushCache(g, efh, Res2) == DOSFALSE)
	    return -1;
    }

    return Cnt;
}

    
/*
 * Tries to write data to data cache.
 * If no data cache or data cache is exceeded the data is written to file
 *
 * returns -1 on error
 * length written else
 */
 
LONG 
wc_WriteCache(Global_T *g, EFH *efh, UBYTE *Buf, LONG Num, LONG *Res2)
{
    WBUFCLUSTER *wbc;
    LONG BufsNeeded;
    LONG Res1 = -1;

    ASSERT(Num > 0);
    D(DBF_ALWAYS,"\twc_WriteCache(0x%08lx, %08lx, %08lx)", efh, Buf, Num);
    do
    {
	if(!(wbc = efh->efh_WBuf))
	{
	    /* no data cache */
	    D(DBF_ALWAYS,"\t\tno cache attached");

	    /* does not harm if caled without cache */
	    Res1 = FlushCacheAndWrite(g, efh, Buf, Num, Res2);
	}
	else
	{
	    BufsNeeded = (Num + MBUFSIZE - 1)/MBUFSIZE;

	    /* some space for optimize(?) FlushCache() may be better called
	       when Buffer really overflows */
	       
	    if((Num > g->g_WriteBufferLimit)
	       || (BufsNeeded > wbc->wbc_WBufsMax)
	       || (BufsNeeded > (wbc->wbc_WBufsUsed + WBufsAvail(g))))
	    {
		/*
		 * cases:
		 *  - will never be able to satisfy write with bufs,
		 *    so flush old data and do normal write
		 *  - not enough free bufs to satisfy write
		 */
		
		Res1 = FlushCacheAndWrite(g, efh, Buf, Num, Res2);
	    }
	    else
	    {
		/* "normal" buffer write, buffer will be flushed if
		   necessary */

		LONG RetVal;
		LONG Cnt;
		LONG OrigNum = Num;

		RetVal = WBufSeekAndPrepare(g, efh, wbc, efh->efh_FilePos,
					    Res2);
		if(RetVal == -1)
		    return -1;
		if(RetVal == 0)
		    return FlushCacheAndWrite(g, efh, Buf, Num, Res2);
		
		while(Num)
		{
		    Cnt = WBufWrite(g, efh, efh->efh_WBuf, Buf, Num, Res2);
		    if(Cnt == -1)
			return -1;
		    if(Cnt == 0) /* no mem */
			return FlushCacheAndWrite(g, efh, Buf, Num, Res2);
		    
		    Num -= Cnt;
		    Buf += Cnt;
		}
		Res1 = OrigNum;
		*Res2 = 0;
		if(wbc->wbc_FilePos > efh->efh_FileLen)
		    efh->efh_FileLen = wbc->wbc_FilePos;
		/* efh->efh_filepos will be set by caller */
	    }
	}
    } while(0);
    
    return Res1;
}

LONG
wc_FlushCache(Global_T *g, EFH *efh, LONG *Res2)
{
    return iwc_FlushCache(g,efh, 0, Res2);
}

/*
 * returns DOSFALSE/DOSTRUE
 */

static LONG
iwc_FlushCache(Global_T *g, EFH *efh, LONG IgnoreEmptyBlocks, LONG *Res2)
{
    WBUFHDR *wbh;
    WBUFHDRINFO *wbi;
    LONG i, Res1 = DOSTRUE;

    D(DBF_ALWAYS, "\twc_FlushCache(0x%08lx)", efh);
    
    while(wbh = WBufRemHead(efh->efh_WBuf))
    {
	LONG Len, DummyLen;

	D(DBF_ALWAYS, "\t\twbh = 0x%08lx", wbh);

	i = wbh->wbh_NumInfo;

	if((i <= 0) && !IgnoreEmptyBlocks)
	{
	    extern void log_end(void);
	    log_end();
	    ASSERT(i > 0);
	}
	else
	{
	    wbi = wbh->wbh_Info;
	    while(i--)
	    {
		ASSERT((wbi->wbi_To-wbi->wbi_From+1)>0);
#ifdef DEBUG
		if(i)
		{
		    /* [0] mut be before [1], else a melting error has happend */
		    ASSERT((wbi->wbi_To + 1) < wbi[1].wbi_From);
		}
#endif
		if(g->g_NFSGlobal.ng_NumWRPCs > 1)
		    /* ASync */
		    Len = nfs_MAWrite(&g->g_NFSGlobal,
				      g->g_NFSClnt, 
				      &efh->efh_ELock->efl_NFSFh,
				      wbh->wbh_FileOffs + wbi->wbi_From,
				      wbh->wbh_Data + wbi->wbi_From,
				      wbi->wbi_To-wbi->wbi_From+1,
				      &DummyLen,
				      Res2);
		else
		    /* Sync */
		    Len = nfs_MWrite(&g->g_NFSGlobal,
				     g->g_NFSClnt, 
				     &efh->efh_ELock->efl_NFSFh,
				     wbh->wbh_FileOffs + wbi->wbi_From,
				     wbh->wbh_Data + wbi->wbi_From,
				     wbi->wbi_To-wbi->wbi_From+1,
				     &DummyLen,
				     Res2);
	
		if(Len < 0)
		{
		    ReleaseWBuf(g, wbh);
		    while(wbh = WBufRemHead(efh->efh_WBuf))
			ReleaseWBuf(g, wbh);
	    
		    Res1 = DOSFALSE;
		    goto end;
		}
		wbi++;
	    }
	}
	ReleaseWBuf(g, wbh);
    }
 end:
#ifdef DEBUG
    if(Res1 == DOSTRUE)
    {
    	D(DBF_ALWAYS, "\t\t-- OK");
    }
    else
    {
    	D(DBF_ALWAYS, "\t\t-- FAILURE");
    }
#endif
    return Res1;
}
