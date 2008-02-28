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
#include "chdebug.h"

#include "protos.h"

#define RBUF_DEBUG 1

#ifndef RBUF_DEBUG
#ifdef DEBUG
#undef DEBUG
#endif
#endif

/*----------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

/*
 * nothing will be done if no memory (just no buf is added)
 */

void
rb_CreateRBuf(EFH *efh)
{
    RBUFHDR *rbh;

    rbh = AllocVec(sizeof(RBUFHDR), MEMF_CLEAR | MEMF_PUBLIC);
    if(rbh)
    {
	AKDEBUG((0,"\trb_CreateBuf(%08lx) -- ok (%08lx)\n",
		 efh, rbh));
    }
    else
    {
	AKDEBUG((0,"\trb_CreateBuf(%08lx) -- failed\n",
		 rbh));
    }

    efh->efh_RBuf = rbh;
}

/*
 * Data buf must be removed on entry
 */

void
rb_DeleteRBuf(EFH *efh)
{
    RBUFHDR *rbh;

    AKDEBUG((0, "\trb_DeleteRBuf(%08lx)\n", efh));
    
    if(rbh = efh->efh_RBuf)
    {
	chassert(rbh->rbh_Data == NULL);
	FreeVec(efh->efh_RBuf);
	efh->efh_RBuf = NULL;
    }
}


void
rb_ObtainRBufData(Global_T *g, EFH *efh)
{
    MBUFNODE *rbn;
    RBUFHDR *rbh;

    AKDEBUG((0, "\trb_ObtainRBufData(%08lx)\n", efh));
    
    chassert(g->g_NumFreeMBufs);
    
    rbh = efh->efh_RBuf;
    chassert(rbh);
    
    rbn = mb_ObtainMBuf(g);
    chassert(rbn);

    rbh->rbh_Data = (UBYTE *) rbn;
}

void
rb_ReleaseRBufData(Global_T *g, EFH *efh)
{
    MBUFNODE *rbn;
    RBUFHDR *rbh;

    AKDEBUG((0, "\trb_ReleaseRBufData(%08lx)\n", efh));

    rbh = efh->efh_RBuf;
    chassert(rbh);
    rbn = (MBUFNODE*) rbh->rbh_Data;
    chassert(rbn);

    mb_ReleaseMBuf(g, rbn);
    
    rbh->rbh_Data = NULL;
}

/*
 * returns -1 on error
 * length read else
 */

/*
 * FIXME: Flag for "used before" in efh instead of FilePos test
 */

LONG
rb_ReadBuf(Global_T *g, EFH *efh, UBYTE *Buf, LONG Num, LONG *Res2)
{
    if((Num <= RBUFFERLIMIT)
       && efh->efh_FilePos
       && (efh->efh_RBuf->rbh_Data || (g->g_NumFreeMBufs > 0)))
    {
	RBUFHDR *rbh;
	LONG NumRead, NumLeft;
	LONG FilePos;
	
	NumLeft = Num;
	FilePos = efh->efh_FilePos;
	rbh = efh->efh_RBuf;
	if(rbh->rbh_Data == NULL)
	{
	    rb_ObtainRBufData(g, efh);
	}
	else
	{
	    LONG End;

	    End = rbh->rbh_FileOffs + rbh->rbh_DataLen;
	    if((rbh->rbh_FileOffs <= FilePos)
	       && (FilePos < End))
	    {
		LONG l;

		l = min(NumLeft, End-FilePos);
		CopyMem(rbh->rbh_Data + (FilePos - rbh->rbh_FileOffs),
			Buf,
			l);
		NumLeft -= l;
		Buf += l;
		FilePos += l;
	    }
	    
	}
	if(NumLeft)
	{
	    NumRead = nfs_MRead(&g->g_NFSGlobal,
				g->g_NFSClnt, 
				&efh->efh_ELock->efl_NFSFh,
				FilePos,
				rbh->rbh_Data,
				MBUFSIZE,
				&efh->efh_FileLen,
				Res2);
	    if(NumRead == 0)
	    {
		efh->efh_FilePos = FilePos;
		rb_ReleaseRBufData(g, efh);
		
		return Num-NumLeft;
	    }
	    else if(NumRead > 0)
	    {
		LONG l;
		
		rbh->rbh_FileOffs = FilePos;
		rbh->rbh_DataLen = NumRead;
		
		l = min(NumRead, NumLeft);
		CopyMem(rbh->rbh_Data,
			Buf,
			l);
		efh->efh_FilePos = FilePos + l;
		NumLeft -= l;
		
		return Num-NumLeft;
	    }
	    else
	    {
		rb_ReleaseRBufData(g, efh);
		
		return -1;
	    }
	}
	else
	{
	    efh->efh_FilePos = FilePos;
		
	    return Num;
	}
    }
    else
    {
	if(HasBufferedRData(efh))
	    rb_ReleaseRBufData(g, efh);
	
	Num = nfs_MRead(&g->g_NFSGlobal,
			g->g_NFSClnt, 
			&efh->efh_ELock->efl_NFSFh,
			efh->efh_FilePos,
			Buf,
			Num,
			&efh->efh_FileLen,
			Res2);
	if(Num > 0)
	    efh->efh_FilePos += Num;
	
	return Num;
    }
}

