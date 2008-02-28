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
 * Functions that deal with the timer.
 */

#include "nfs_handler.h"
#include "protos.h"

#include "chdebug.h"

#if 0
#define DISABLED 1
#endif

static void
ti_StartTimer(SyncTimer *st)
{
    st->st_Pkt->dp_Type = ACTION_TIMER;
    
    st->st_Req->tr_node.io_Command = TR_ADDREQUEST;
    st->st_Req->tr_time.tv_secs = SYNC_TIME;
    st->st_Req->tr_time.tv_micro = 0;

    SendIO(&st->st_Req->tr_node);

    st->st_Flags |= STF_TIMER_USED;
}

void
ti_Cleanup(Global_T *g)
{
#ifndef DISABLED
    SyncTimer *st = &g->g_SyncTimer;

    if(st->st_Pkt)
    {
	if(st->st_Req)
	{
	    if(st->st_Flags & STF_TIMER_USED)
	    {
		if(!CheckIO(&st->st_Req->tr_node))
		    AbortIO(&st->st_Req->tr_node);
		WaitIO(&st->st_Req->tr_node);
	    }
	    if(st->st_Flags & STF_DEVICE_OPEN)
	    {
		CloseDevice(&st->st_Req->tr_node);
	    }
	}
    }
    if(st->st_Pkt)
	FreeVec(st->st_Pkt);
    if(st->st_Req)
	DeleteIORequest(st->st_Req);
    
    memset(st, 0, sizeof(*st));
#endif
}

LONG
ti_Init(Global_T *g)
{
#ifdef DISABLED
    return 1;
#else
    LONG RetVal = 0;
    SyncTimer *st = &g->g_SyncTimer;
    
    st->st_Pkt = AllocVec(sizeof (DOSPKT), MEMF_CLEAR | MEMF_PUBLIC);
    st->st_Req = CreateIORequest(g->g_Port, sizeof (struct timerequest));

    if(st->st_Pkt && st->st_Req)
    {
	/* make StandardPacket linkage */
	
	st->st_Pkt->dp_Link = &st->st_Req->tr_node.io_Message;
	st->st_Req->tr_node.io_Message.mn_Node.ln_Name =
	    (UBYTE *) st->st_Pkt;

	if(OpenDevice(TIMERNAME, UNIT_VBLANK, &st->st_Req->tr_node, 0) == 0)
	{
	    st->st_Flags = STF_DEVICE_OPEN;
	    RetVal = 1;
	}
    }

    if(RetVal == 0)
	ti_Cleanup(g);
    else
	ti_StartTimer(st);
    
    return RetVal;
#endif
}


LONG
act_TIMER(Global_T *g, DOSPKT *pkt)
{
    SyncTimer *st = &g->g_SyncTimer;

    if(pkt != st->st_Pkt)
    {
	AKDEBUG((2,"illegal packet received\n"));
	
	SetRes(g, DOSFALSE, ERROR_OBJECT_WRONG_TYPE);
    }
    else
    {
	g->g_DoReply = 0;

	dc_Bury(g); /* remove old stuff from dir cache */
	attrs_Bury(g); /* remove old stuff from attribute cache */
	    
	SetRes1(g, act_FLUSH(g, pkt));

	ti_StartTimer(st);
    }
    
    return g->g_Res1;
}

