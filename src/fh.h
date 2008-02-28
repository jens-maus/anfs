#ifndef FH_H
#define FH_H

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

#include "locks.h"
#include "wbuf.h"
#include "rbuf.h"

typedef struct _efh
{
    struct _efh *efh_Next;
    LONG efh_Id;
    LONG efh_FilePos;
    LONG efh_FileLen;		/* will be updated on each read/write */
    LONG efh_Mode;		/* as specified to Open */
    ELOCK *efh_ELock;
    WBUFCLUSTER *efh_WBuf;	/* write buffer, (currently) only used on
				   exclusive writes */
    RBUFHDR *efh_RBuf;	/* read ahead buffer */
    LONG efh_Res2;		/* error from cached write */
} EFH;

#define HasWBuf(efh) (efh->efh_WBuf != NULL)
#define HasRBuf(efh) (efh->efh_RBuf != NULL)

#define HasBufferedWData(efh) (efh->efh_WBuf && efh->efh_WBuf->wbc_WBufsUsed)
#define HasBufferedRData(efh) (efh->efh_RBuf && efh->efh_RBuf->rbh_Data)

struct Global_T;

/* return 0 to break traversal */
typedef LONG fh_ForEachProc_T(struct Global_T *g, EFH *efh, void *arg);
#endif
