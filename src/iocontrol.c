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
 * Implementation of ACTION_IOCONTROL, misc. DEBUG stuff
 */

#include <math.h> /* min/max */
#include <rexx/errors.h>

#include "nfs_handler.h"
#include "protos.h"

#include "chdebug.h"

#include "nfsc_iocontrol.h"

#define CG_DEBUG 0
#define CS_DEBUG 1
#define CG_UMASK 2
#define CS_UMASK 3
#define CG_USER 4
#define CS_USER 5
#define CG_MR 6
#define CS_MR 7
#define CG_MD 8
#define CS_MD 9
#define CG_MW 10
#define CS_MW 11
#define CG_WL 12
#define CS_WL 13
#define CG_ACTO 14
#define CS_ACTO 15
#define CG_DCTO 16
#define CS_DCTO 17
#define CG_RPCTO 18
#define CS_RPCTO 19
#define CG_TR 20
#define CS_TR 21
#define CG_SM 22
#define CS_SM 23
/* ASync RPC */
#define CG_ARPC 24
#define CS_ARPC 25

#define CG_CMDS 100
#define CG_DEVICE 101
#define CG_VOLUME 102
#define CG_MNTHOST 103
#define CG_MNTDIR 104

struct Code
{
    UBYTE *Cmd;
    LONG Get;
    LONG Set;
} Codes[] =
{
{"COMMANDS", CG_CMDS, -1},
#if 0
{"DEVICE", CG_DEVICE, -1},
#endif
{"VOLUME", CG_VOLUME, -1},
{"MNTHOST", CG_MNTHOST, -1},
{"MNTDIR", CG_MNTDIR, -1},
{"DEBUG", 0, 1},
{"UMASK", 2, 3},
{"USER", 4, 5},
{"MAX_READSIZE", CG_MR, CS_MR},
{"MAX_READDIRSIZE", CG_MD, CS_MD},
{"MAX_WRITESIZE", CG_MW, CS_MW},
{"WRITEBUFFERLIMIT", CG_WL, CS_WL},
{"ATTRCACHE_TIMEOUT", CG_ACTO, CS_ACTO},
{"DIRCACHE_TIMEOUT", CG_DCTO, CS_DCTO},
{"RPC_TIMEOUT", CG_RPCTO, CS_RPCTO},
{"TIMEOUTREQ", CG_TR, CS_TR},
{"SLOW_MEDIUM", CG_SM, CS_SM},
{"ASYNC_RPC", CG_ARPC, CS_ARPC},
{NULL, 0, 0},
};
	
LONG
act_IOCONTROL(Global_T *g, DOSPKT *pkt)
{
    UBYTE Buf[512], *nextchar;
    LONG Get, CntrlCode, i;
    
    struct  RexxMsg *rmsg;
    ULONG ui;
    struct timeval tv;
    LONG RxErr = 0;
    const UBYTE *cs;
    
    /* we use this to filter out foreign packets (old IOCONTROL) */
    if(pkt != &g->g_IntPkt)
	return SetRes(g, DOSFALSE, ERROR_ACTION_NOT_KNOWN);

    /* don\'t reply message ! */
    g->g_DoReply = 0;
    rmsg = (struct RexxMsg *) pkt->dp_Link;

    AKDEBUG((0,"ARG(0) = %s\n", ARG0(rmsg)));
    nextchar=stptok(ARG0(rmsg),
		    Buf,sizeof(Buf)," ");
    while (*nextchar == ' ') nextchar++;

    if(stricmp(Buf, "GET") == 0)
    {
	Get = 1;
    }
    else if(stricmp(Buf, "SET") == 0)
    {
	Get = 0;
    }
    else
	Get = -1;

    if(Get != -1)
    {
	nextchar=stptok(nextchar,
		    Buf,sizeof(Buf)," ");
	while (*nextchar == ' ') nextchar++;

	i = 0;
	CntrlCode = -1;
	while(Codes[i].Cmd)
	{
	    if(stricmp(Codes[i].Cmd, Buf) == 0)
	    {
		if(Get)
		    CntrlCode =  Codes[i].Get;
		else
		    CntrlCode =  Codes[i].Set;
		break;
	    }
	    i++;
	}
    }

    if((Get != -1) && (CntrlCode != -1))
    {
	if(!Get)
	{
	    nextchar=stptok(nextchar,
			    Buf,sizeof(Buf)," ");
	    if(*Buf == 0)
	    {
		CntrlCode = -1;
		RxErr = ERR10_041; /* invalid expression */
	    }
	}
	switch(CntrlCode)
	{
#ifdef DEBUG
    case CG_DEBUG:
	    SPrintf(Buf, "0 %ld 3", kdebug);
	    log_end();		/* (temporary) close log file */
	    break;
    case CS_DEBUG:
	    kdebug = atoi(Buf);
	    if(kdebug<0)
		kdebug = 0;
	    if(kdebug > 3)
		kdebug = 3;

	    log_end();		/* (temporary) close log file */
	    break;
#else
    case CG_DEBUG:
	    strcpy(Buf, "-1 -1 -1");
	    break;
#endif
    case CG_UMASK:
	    if(cr_UMaskWasSet(&g->g_Creds))
	    {
		i = cr_GetUMask(&g->g_Creds);
		Buf[2] = '0' + i % 8;
		i /= 8;
		Buf[1] = '0' + i % 8;
		i /= 8;
		Buf[0] = '0' + i % 8;
		Buf[3] = 0;
	    }
	    else
	    {
		strcpy(Buf, "<unset>");
	    }
	    break;
    case CS_UMASK:
	    cr_SetUMask(&g->g_Creds, strtol(Buf, &nextchar, 8) & 0777);
	    break;
    case CG_USER:
	    Buf[sizeof(Buf) -1] = 0;
	    cs = cr_GetUser(&g->g_Creds);
	    if(cs)
	    {
		strncpy(Buf, cs, sizeof(Buf) -1);
	    }
	    else
	    {
		strcpy(Buf, "<unset>");
	    }
	    break;
    case CS_USER:
	    if(!cr_SetUser(&g->g_Creds, Buf))
	    {
		CntrlCode = -1;
		RxErr = ERR10_041; /* invalid expression */
	    }
	    break;
    case CG_MR:
	    SPrintf(Buf, "%ld %ld %ld",
		    0x100, g->g_NFSGlobal.ng_MaxReadSize, NFS_MAXDATA);
	    break;
    case CS_MR:
	    i = atoi(Buf);
	    g->g_NFSGlobal.ng_MaxReadSize = max(0x100, min(NFS_MAXDATA, i));
	    break;
    case CG_MD:
	    SPrintf(Buf, "%ld %ld %ld",
		    0x100, g->g_NFSGlobal.ng_MaxReadDirSize, NFS_MAXDATA);
	    break;
    case CS_MD:
	    i = atoi(Buf);
	    g->g_NFSGlobal.ng_MaxReadDirSize = max(0x100, min(NFS_MAXDATA, i));
	    break;
    case CG_MW:
	    SPrintf(Buf, "%ld %ld %ld",
		    0x100, g->g_NFSGlobal.ng_MaxWriteSize, NFS_MAXDATA);
	    break;
    case CS_MW:
	    i = atoi(Buf);
	    g->g_NFSGlobal.ng_MaxWriteSize = max(0x100, min(NFS_MAXDATA, i));
	    break;
    case CG_WL:
	    SPrintf(Buf, "%ld %ld %ld",
		    0, g->g_WriteBufferLimit, 10000);
	    break;
    case CS_WL:
	    i = atoi(Buf);
	    g->g_WriteBufferLimit = max(0, min(10000, i));
	    break;
    case CG_ACTO:
	    SPrintf(Buf,"%ld %ld %ld",
		    ATTR_CACHE_TIMEOUT_MIN,
		    g->g_AttrCacheTimeout,
		    ATTR_CACHE_TIMEOUT_MAX);
	    break;
    case CS_ACTO:
	    i = atoi(Buf);
	    ui = max(ATTR_CACHE_TIMEOUT_MIN,
		     min(ATTR_CACHE_TIMEOUT_MAX, i));
	    if(attrs_SetCacheTimeout(g, ui) == -1)
	    {
		CntrlCode = -1;
		RxErr = ERR10_041; /* invalid expression */
	    }
	    else
	    {
		g->g_AttrCacheTimeout = ui;
	    }
	    break;
    case CG_DCTO:
	    SPrintf(Buf,"%ld %ld %ld",
		    DIR_CACHE_TIMEOUT_MIN, g->g_DirCacheTimeout,
		    DIR_CACHE_TIMEOUT_MAX);
	    break;
    case CS_DCTO:
	    i = atoi(Buf);
	    ui = max(DIR_CACHE_TIMEOUT_MIN,
		     min(DIR_CACHE_TIMEOUT_MAX, i));
	    if(dc_SetCacheTimeout(g, ui) == -1)
	    {
		CntrlCode = -1;
		RxErr = ERR10_041; /* invalid expression */
	    }
	    else
	    {
		g->g_DirCacheTimeout = ui;
	    }
	    break;
    case CG_RPCTO:
	    SPrintf(Buf,"%ld %ld %ld",
		    RPC_TIMEOUT_MIN,
		    g->g_RPCTimeout,
		    RPC_TIMEOUT_MAX);
	    break;
    case CS_RPCTO:
	    i = atoi(Buf);
	    ui = max(RPC_TIMEOUT_MIN, min(RPC_TIMEOUT_MAX, i));
	    tv.tv_sec = ui;
	    tv.tv_usec = 0;
	    clnt_control(g->g_NFSClnt, CLSET_TIMEOUT, &tv);
	    clnt_control(g->g_NFSClnt, CLGET_TIMEOUT, &tv);
	    g->g_RPCTimeout = tv.tv_sec;
	
	    tv.tv_sec = g->g_RPCTimeout/5;
	    tv.tv_usec = 0;
	    clnt_control(g->g_NFSClnt, CLSET_RETRY_TIMEOUT, &tv);
	    break;
    case CG_TR:
	    SPrintf(Buf,"%ld", (g->g_Flags & GF_USE_TIMEOUT_REQ) ? 1 : 0);
	    break;
    case CS_TR:
	    if(atoi(Buf))
	       g->g_Flags |= GF_USE_TIMEOUT_REQ;
	    else
		g->g_Flags &= ~GF_USE_TIMEOUT_REQ;
	    break;
    case CG_SM:
	    SPrintf(Buf, "%ld", (g->g_Flags & GF_SLOW_MEDIUM) ? 1 : 0);
	    break;
    case CS_SM:
	   if(atoi(Buf))
	       g->g_Flags |= GF_SLOW_MEDIUM;
	   else
	       g->g_Flags &= ~GF_SLOW_MEDIUM;
	    break;
    case CG_ARPC:
	    SPrintf(Buf, "%ld", (g->g_Flags & GF_ASYNC_RPC) ? 1 : 0);
	    break;
    case CS_ARPC:
	   if(atoi(Buf))
	   {
	       g->g_Flags |= GF_ASYNC_RPC;
	       g->g_NFSGlobal.ng_NumRRPCs = 2;
	       g->g_NFSGlobal.ng_NumWRPCs = 2;
	   }
	   else
	   {
	       g->g_Flags &= ~GF_ASYNC_RPC;
	       g->g_NFSGlobal.ng_NumRRPCs = 1;
	       g->g_NFSGlobal.ng_NumWRPCs = 1;
	   }
	    break;
    case CG_CMDS:
	    i = 0;
	    *Buf = 0; /* FIXME: check for overflow/speedup */
	    while(Codes[i].Cmd)
	    {
		strcat(Buf,"(");
		strcat(Buf,Codes[i].Cmd);
		if(Codes[i].Get != -1)
		{
		    strcat(Buf,",");
		    strcat(Buf,"GET");
		}
		if(Codes[i].Set != -1)
		{
		    strcat(Buf,",");
		    strcat(Buf,"SET");
		}
		strcat(Buf,")");
		i++;
	    }
	    break;
    case CG_DEVICE:
	    Buf[sizeof(Buf) -1] = 0;
	    strncpy(Buf, g->g_DeviceName, sizeof(Buf) -1);
	    break;
    case CG_VOLUME:
	    Buf[sizeof(Buf) -1] = 0;
	    strncpy(Buf, g->g_VolumeName, sizeof(Buf) -1);
	    break;
    case CG_MNTHOST:
	    Buf[sizeof(Buf) -1] = 0;
	    strncpy(Buf, g->g_Host, sizeof(Buf) -1);
	    break;
    case CG_MNTDIR:
	    Buf[sizeof(Buf) -1] = 0;
	    strncpy(Buf, g->g_RootDir, sizeof(Buf) -1);
	    break;
    default:
	    CntrlCode = -1;
	    break;
	}
    }
/*		ERR10_037 invalid template */
	
    if((Get == -1) || (CntrlCode == -1))
    {
	if(RxErr)
	{
	    SetARexxLastError(g->g_ARexxCont,rmsg,"");
	    rmsg->rm_Result2 = RxErr;
	    ReplyARexxMsg(g->g_ARexxCont,rmsg,NULL, RC_ERROR);
	}
	else
	{
	    SetARexxLastError(g->g_ARexxCont,rmsg,"Unknown command");
	    rmsg->rm_Result2 = ERR10_015; /* function not found */
	    ReplyARexxMsg(g->g_ARexxCont,rmsg,NULL, RC_ERROR);
	}
    }
    else
	ReplyARexxMsg(g->g_ARexxCont,rmsg, Buf, 0);
    
    return SetRes1(g, DOSTRUE);
}

