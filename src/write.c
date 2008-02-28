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
 * Implement the writing stuff
 */

#include "nfs_handler.h"
#include "protos.h"

LONG act_WRITE(Global_T *g, DOSPKT *pkt)
{
    LONG Arg1 = pkt->dp_Arg1;
    UBYTE *Buf = (UBYTE *) pkt->dp_Arg2;
    LONG Num = pkt->dp_Arg3;

    LONG Res1 = DOSFALSE;
    LONG Res2 = g->g_Res2;
    EFH *efh;
    LONG Mode;

    if(Num<0)
	Num = 0;
    
    efh = fh_Lookup(g, Arg1);
    if(efh)
    {
	Mode = efh->efh_Mode;

	if(efh->efh_Res2) /* error from cached write */
	{
	    Res2 = efh->efh_Res2;
	    efh->efh_Res2 = 0; /* FIXME: this may be wrong ??? */
	}
	else
	{
	    if(!((Mode == MODE_OLDFILE) || (Mode == MODE_READWRITE) 
		 || (Mode == MODE_NEWFILE)))
	    {
		Res2 = ERROR_FILE_NOT_OBJECT;
	    }
	    else
	    {
		/* FIXME: release only if regions overlap ?*/
		
		if(HasBufferedRData(efh))
		    rb_ReleaseRBufData(g, efh);
		if(Num > 0)
		{
#if 0
		    Num = nfs_MWrite(g->g_NFSClnt, 
				     &efh->efh_ELock->efl_NFSFh,
				     efh->efh_FilePos,
				     Buf,
				     Num,
				     &efh->efh_FileLen,
				     g->g_MaxWriteSize,
				     &Res2);
#else
		    Num = wc_WriteCache(g, efh, Buf, Num, &Res2);
#endif
		    if(Num >= 0)
		    {
			efh->efh_FilePos += Num;
			Res1 = Num; Res2 = 0;
		    }
		    else
		    {
			Res1 = -1;
		    }
		}
		else
		{
		    if(HasBufferedWData(efh))
		    {
			if(wc_FlushCache(g, efh, &Res2) == DOSFALSE)
			{
			    Res1 = -1;
			}
			else
			{
			    Res1 = Num; Res2 = 0;
			}
		    }
		    else
		    {
			Res1 = Num; Res2 = 0;
		    }
		}
	    }
	}
    }
    else
	Res2 = ERROR_FILE_NOT_OBJECT;

    return SetRes(g, Res1, Res2);
}







