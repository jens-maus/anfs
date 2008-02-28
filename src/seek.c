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

#include "chdebug.h"

#if 0
#define DEEPDEBUG 1
#endif

LONG act_SEEK(Global_T *g, DOSPKT *pkt)
{
    LONG Arg1 = pkt->dp_Arg1;
    LONG NewPos = pkt->dp_Arg2;
    LONG Mode = pkt->dp_Arg3;

    EFH *efh;
    LONG OldPos;

    efh = fh_Lookup(g, Arg1);
    if(efh)
    {
	OldPos = efh->efh_FilePos;
	switch(Mode)
	{
    case OFFSET_CURRENT:
	    NewPos = OldPos + NewPos;
	    break;
    case OFFSET_BEGINNING:
	    break;
    case OFFSET_END:
	    NewPos = efh->efh_FileLen + NewPos;
	    break;
    default:
	    NewPos = -1;
	    break;
	}
#ifdef DEEPDEBUG
	AKDEBUG((0,"\tOldPos = 0x%08lx, NewPos = 0x%08lx\n",
		 OldPos, NewPos));
	AKDEBUG((0,"\tFilePos = 0x%08lx, FileLen = 0x%08lx\n",
		 efh->efh_FilePos, efh->efh_FileLen));
#endif
	if((0 <= NewPos) && (NewPos <= efh->efh_FileLen))
	{
	    efh->efh_FilePos = NewPos;
	    SetRes(g, OldPos, 0);
	}
	else
	    SetRes(g, DOSFALSE, ERROR_SEEK_ERROR);
    }
    else
	SetRes(g, DOSFALSE, ERROR_FILE_NOT_OBJECT); /* FIXME: ok? */

    return g->g_Res1;    
}

