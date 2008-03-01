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
#include <math.h> /* for min/max */
#include <stdlib.h>
#include <signal.h>

#if 0
#include <dos.h>
#else
extern unsigned long stacksize(void);
#endif

#include <pwd.h>

#include <misclib/protos.h>

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#include "rpc/clnt_audp.h"
#include "rpc/clnt_ageneric.h"

#include "Debug.h"

static const char Version[] = "$VER: ch_nfsc Version 1.02 Beta (30 Jun 94)";

struct Library *UtilityBase = NULL;

/* FIXME: make RPCTimeout() and SGlobals non global variables */

Global_T *SGlobals;

/* called if an RPC timeout occurs, returns TRUE on retry, FALSE else */
int
RPCTimeoutReq(void)
{
    if(SGlobals->g_Flags & GF_USE_TIMEOUT_REQ)
	if(APrintReq2("Retry", "Cancel", "RPC call timed out\n"))
	    return 0;
	else
	    return 1;
    else
	return 0;
}

void
G_CleanUp(Global_T *g)
{
#ifdef DEBUG
    if(g->g_Dev)
    {
	ASSERT(g->g_Dev->dl_LockList  == NULL);
    }
#endif
    ASSERT(g->g_FH == NULL);

    dir_DeleteAll(g);
    ASSERT(g->g_DH == NULL);
    
    mb_FreeMBufs(g);
    wc_FreeWBufs(g);

    FreeARexx(g->g_ARexxCont);
    
    if(g->g_AttrCache)
	attrs_Cleanup(g);
    if(g->g_DirCache)
	dc_Cleanup(g);
    if(g->g_Port)
	DeleteMsgPort(g->g_Port);
    if(g->g_Dev)
    {
	FreeDosEntry((struct DosList *) g->g_Dev);
    }
#if 0 /* device node must stay resident */
    if(g->g_Device)
    {
	FreeDosEntry(g->g_Device);
    }
#endif
    if(g->g_UserName)
	FreeVec((UBYTE *) g->g_UserName);
    if(g->g_Host)
	FreeVec((UBYTE *) g->g_Host);
    if(g->g_RootDir)
	FreeVec((UBYTE *) g->g_RootDir);
    if(g->g_VolumeName)
	FreeVec(g->g_VolumeName);
    if(g->g_DeviceName)
	FreeVec(g->g_DeviceName);
    if(UtilityBase)
    {
	CloseLibrary(UtilityBase);
	UtilityBase = NULL;
    }
}

LONG
G_Init(Global_T *g)
{
    struct DeviceList *MyDev;
    LONG MBufs, Attrs, Dirs;

    DistributeBuffers(g, g->g_NumBuffers, &MBufs, &Attrs, &Dirs);

    UtilityBase = OpenLibrary("utility.library", 0);
    if(!UtilityBase)
    {
	APrintReq("could not open utility.library\n");
	return 0;
    }
	
    g->g_Dev = (struct DeviceList *) MakeDosEntry(g->g_VolumeName, DLT_VOLUME);
    if(!g->g_Dev)
    {
	return 0;
    }
#ifndef NO_DEVICE
    g->g_Device = (struct DevInfo *) MakeDosEntry(g->g_DeviceName, DLT_DEVICE);
    if(!g->g_Device)
    {
	return 0;
    }
#endif
    g->g_Port = CreateMsgPort();
    if(!g->g_Port)
    {
	APrintReq("no port\n");
	return 0;
    }
    g->g_AttrCacheMaxEntries = Attrs;
    if(!attrs_Init(g))
    {
	APrintReq("(internal) attrs_Init failed\n");
	return 0;
    }
    g->g_DirCacheMaxEntries = Dirs;
    if(!dc_Init(g))
    {
	APrintReq("(internal) dc_Init failed\n");
	return 0;
    }
    /* note: g_Port must be initialized prior to calling ti_Init() */
    if(!ti_Init(g))
    {
	APrintReq("(internal) ti_Init failed\n");
	return 0;
    }
    g->g_ARexxCont = InitARexx("ch_nfsc", NULL);
    if(!g->g_ARexxCont)
    {
	APrintReq("creating ARexx port failed\n");
	return 0;
    }
    g->g_ARexxSig = ARexxSignal(g->g_ARexxCont);
    
    MyDev = g->g_Dev;

    MyDev->dl_Task = g->g_Port;
    MyDev->dl_Lock = NULL; /* not for volumes */
    DateStamp(&MyDev->dl_VolumeDate);
    MyDev->dl_LockList = 0;
    MyDev->dl_DiskType = ID_DOS_DISK;

#ifndef NO_DEVICE
    g->g_Device->dvi_Task = g->g_Port;
    g->g_Device->dvi_Handler = NULL; /* FIXME: add handler path */
    /* FIXME: when ch_nfsc is restarted the DeviceNode must not be
       re-created and the whole startup code must recognize this case */
    g->g_Device->dvi_StackSize = stacksize(); /* FIXME */
#endif
    
    if(! mb_AllocMBufs(g, MBufs))
    {
	APrintReq("dc_Init: no mem (1)\n");
    }
    g->g_BufSizePerFile = 4*8192; /* FIXME: make parameter */
    
    /* this call does only allocate WBUFHDR structures */
    if(! wc_AllocWBufs(g, 16 /* FIXME */))
    {
	APrintReq("dc_Init: no mem (2)\n");
    }

    return 1;
}

DOSPKT *
G_WaitPkt(Global_T *g)
{
    struct  RexxMsg     *rmsg = NULL;
    DOSPKT *MyPkt = NULL;
    struct Message *MyMsg;
    ULONG Signals;

    while(MyPkt == NULL)
    {
	MyMsg = GetMsg(g->g_Port);
	if(!MyMsg)
	{
	    rmsg = GetARexxMsg(g->g_ARexxCont);
	}
	if(!MyMsg && !rmsg)
	{
	    Signals = Wait((1 << g->g_Port->mp_SigBit)
			   | g->g_ARexxSig
			   | SIGBREAKF_CTRL_C);
	    if(Signals & SIGBREAKF_CTRL_C)
		break;
	    if(Signals & g->g_ARexxSig)
		rmsg = GetARexxMsg(g->g_ARexxCont);
	    else
		if(Signals & (1 << g->g_Port->mp_SigBit))
		    MyMsg = GetMsg(g->g_Port);
	}
	if(MyMsg)
	{
	    MyPkt = (struct DosPacket *) MyMsg->mn_Node.ln_Name;

	    g->g_Res1 = MyPkt->dp_Res1;
	    g->g_Res2 = MyPkt->dp_Res2;
	}
	if(rmsg)
	{
	    /* mask as ACTION_IOCONTROL */
	    MyPkt = &g->g_IntPkt;
	    g->g_IntPkt.dp_Link = (struct Message *) rmsg;
	    /* so IOCONTROL can decide if it\'s internal */
	    g->g_IntPkt.dp_Port = (struct MsgPort *) MyPkt;
	    g->g_IntPkt.dp_Type = ACTION_IOCONTROL;
	}
    }
    return MyPkt;
}

DOSPKT *
G_GetPkt(Global_T *g)
{
    DOSPKT *MyPkt = NULL;
    struct Message *MyMsg;

    MyMsg = GetMsg(g->g_Port);
    if(MyMsg)
    {
	MyPkt = (struct DosPacket *) MyMsg->mn_Node.ln_Name;
	
	g->g_Res1 = MyPkt->dp_Res1;
	g->g_Res2 = MyPkt->dp_Res2;
    }

    return MyPkt;
}

void
G_ReplyPkt(Global_T *g, DOSPKT *pkt)
{
    struct MsgPort *port;

    port = pkt->dp_Port;
    pkt->dp_Port = g->g_Port;
    pkt->dp_Res1 = g->g_Res1;
    pkt->dp_Res2 = g->g_Res2;

    PutMsg(port, pkt->dp_Link);
}

void
G_UnMount(Global_T *g)
{
    dirpath dir = (char *) g->g_RootDir;

    if(g->g_MntClnt)
    {
	if(g->g_Flags & GF_IS_MOUNTED)
	{
	    mountproc_umnt_1(&dir, g->g_MntClnt);
	}
	if(g->g_MntClnt->cl_auth)
	    auth_destroy(g->g_MntClnt->cl_auth);

	clnt_destroy(g->g_MntClnt);
	g->g_MntClnt = NULL;
    }
    if(g->g_NFSClnt)
    {
#if 0 /* now inside cred */
	if(g->g_NFSClnt->cl_auth)
	    auth_destroy(g->g_NFSClnt->cl_auth);
#endif
	clnt_destroy(g->g_NFSClnt);
	g->g_NFSClnt = NULL;
    }
}

LONG
G_Mount(Global_T *g, char **reason)
{
    fhstatus *myfhstatus;
    char *host = (char *) g->g_Host;
    dirpath dir = (char *) g->g_RootDir;
    fattr *MntAttr;
    LONG Res2;

    *reason = NULL;

    g->g_MntClnt = clnt_create(host, MOUNTPROG, MOUNTVERS, "udp");
    if (g->g_MntClnt == NULL) 
    {
	*reason = clnt_spcreateerror(host);
	return 0;
    }
    auth_destroy(g->g_MntClnt->cl_auth);
    g->g_MntClnt->cl_auth = authunix_create(g->g_HostName,
					    g->g_MntUID,
					    g->g_MntGID,
					    1,
					    (gid_t *) &g->g_MntGID); /* FIXME */
    if(!g->g_MntClnt->cl_auth)
    {
	*reason = "authunix_create failed\n";
	return 0;
    }
    if(myfhstatus = mountproc_mnt_1(&dir, g->g_MntClnt))
    {
	if(myfhstatus->fhs_status)
	{
	    APrintReq("mount returned %s\n", 
		      StrNFSErr(myfhstatus->fhs_status));
	    return 0;
	}
	g->g_Flags |=  GF_IS_MOUNTED;

	CopyFH((nfs_fh *) &myfhstatus->fhstatus_u.fhs_fhandle, &g->g_MntFh);

	g->g_NFSClnt = clnt_acreate(host, NFS_PROGRAM, NFS_VERSION, 3);
	if (g->g_NFSClnt == NULL) 
	{
	    *reason = clnt_spcreateerror(host);
	    return 0;
	}
	auth_destroy(g->g_NFSClnt->cl_auth);
	if(cr_UpdateAuth(&g->g_Creds, FindTask(0L), g->g_HostName))
	{
	    g->g_NFSClnt->cl_auth = cr_GetAuth(&g->g_Creds);
	}
	else
	    g->g_NFSClnt->cl_auth = NULL;
	
	if(!g->g_NFSClnt->cl_auth)
	{
	    APrintReq("authunix_create failed (2)\n");
	    return 0;
	}

        {
	    struct timeval to;

	    clnt_control(g->g_NFSClnt, CLGET_TIMEOUT, &to);
	    D(DBF_ALWAYS,"G_Mount: old timeout = ( %ld sec, %ld usec)", to.tv_sec, to.tv_usec);

	    to.tv_sec = g->g_RPCTimeout;
	    to.tv_usec = 0;
	    D(DBF_ALWAYS,"G_Mount: new timeout = ( %ld sec, %ld usec)", to.tv_sec, to.tv_usec);
	    clnt_control(g->g_NFSClnt, CLSET_TIMEOUT, &to);

	    clnt_control(g->g_NFSClnt, CLGET_RETRY_TIMEOUT, &to);
	    D(DBF_ALWAYS,"G_Mount: old retry timeout = ( %ld sec, %ld usec)", to.tv_sec, to.tv_usec);

	    to.tv_sec = g->g_RPCTimeout/5;
	    to.tv_usec = 0;
	    D(DBF_ALWAYS,"G_Mount: new retry timeout = ( %ld sec, %ld usec)", to.tv_sec, to.tv_usec);
	    clnt_control(g->g_NFSClnt, CLSET_RETRY_TIMEOUT, &to);
	}

	MntAttr = nfs_GetAttr(g->g_NFSClnt, &g->g_MntFh, &Res2);
	if(!MntAttr)
	{
	    *reason = "getattr failed";
	    E(DBF_ALWAYS,"nfs_GetAttr returned Res2 = %ld", Res2);
	    return 0;
	}
	CopyMem(MntAttr, &g->g_MntAttr, sizeof(*MntAttr));

	return 1;
    }
    else
    {
	*reason = clnt_sperror(g->g_MntClnt, "mount");
    }
    return 0;
}

#ifndef NO_DEVICE
#define TEMPLATE_AS ",AS/K"
#else
#define TEMPLATE_AS ""
#endif

#ifdef DEBUG
#define TEMPLATE_DEBUG ",DEBUG/K/N,PARDEBUG/K/N,SERDEBUG/K/N"
#else
#define TEMPLATE_DEBUG ""
#endif

char *argtemplate = "REMOTE_DEVICE/A,LOCAL_DEVICE/A"TEMPLATE_AS",\
MNT_USER/K,USER/K,UMASK/K/N,\
MR=MAX_READSIZE/K/N,MD=MAX_READDIRSIZE/K/N,MW=MAX_WRITESIZE/K/N,B=BUFFERS/K/N,\
ACTO=ATTRCACHE_TIMEOUT/K/N,DCTO=DIRCACHE_TIMEOUT/K/N,RPCTO=RPC_TIMEOUT/K/N,\
NO_TIMEOUTREQ/S,\
SM=SLOW_MEDIUM/S,ASYNC_RPC/S,LONG_FILENAMES/S"TEMPLATE_DEBUG;

struct Args
{
    UBYTE *RemoteDevice;
    UBYTE *LocalDevice;
#ifndef NO_DEVICE
    UBYTE *As;
#endif
    UBYTE *MntUser;		/* Mountuser, default root */
    UBYTE *User;		/* NFS User, default nobody */
    ULONG *UMask;		/* mask of file mode bits to clear for
				   nfs calls */
    ULONG *MaxReadSize;
    ULONG *MaxReadDirSize;
    ULONG *MaxWriteSize;
    ULONG *Buffers;
    ULONG *AttrCacheTimeout;
    ULONG *DirCacheTimeout;
    ULONG *RPCTimeout;
    ULONG NoTimeoutReq;
    ULONG SlowMedium;
    ULONG ASyncRPC;
    ULONG LongFileNames;
#ifdef DEBUG
    ULONG *Debug;
    ULONG *ParDebug;
    ULONG *SerDebug;
#endif
};

void
usage(char *progname)
{
    APrintReq("Illegal arguments");
}

LONG
ParseArgs(Global_T *g, char *progname)
{
    struct Args ParsedArgs;
    struct RDArgs *DosArgs;
    struct passwd *pw;
    UBYTE *s;
#if 0
    UBYTE *User;
#endif
    LONG l;
    LONG Success = TRUE;
    
    memset(&ParsedArgs, 0, sizeof(ParsedArgs));
    DosArgs = ReadArgs((UBYTE *) argtemplate, (LONG *) &ParsedArgs, NULL);
    if(!DosArgs)
    {
	usage(progname);
	return 0;
    }

    if(ParsedArgs.SlowMedium)
	g->g_Flags |= GF_SLOW_MEDIUM;

    if(!ParsedArgs.NoTimeoutReq)
	g->g_Flags |= GF_USE_TIMEOUT_REQ; /* default is to use timeout req */
    
    if(ParsedArgs.ASyncRPC)
    {
	g->g_NFSGlobal.ng_NumRRPCs = 2;
	g->g_NFSGlobal.ng_NumWRPCs = 2;
	
	g->g_Flags |= GF_ASYNC_RPC;
    }
    else
    {
	g->g_NFSGlobal.ng_NumRRPCs = 1;
	g->g_NFSGlobal.ng_NumWRPCs = 1;
    }
    if(ParsedArgs.LongFileNames)
    {
	g->g_MaxFileNameLen = MAX_LONGAMIGAFILENAMELEN;
    }
    else
    {
	g->g_MaxFileNameLen = MAX_AMIGAFILENAMELEN;
    }
    
#ifdef DEBUG
    l = 0;
    if(ParsedArgs.Debug)
    {
	l++;
	kdebug = *ParsedArgs.Debug;
    }
    else
	kdebug = 2; /* default: log only errors */
    if(ParsedArgs.SerDebug)
    {
	l++;
	kdebug = *ParsedArgs.SerDebug;
	g->g_Flags = GF_SERDEBUG;
	log_SetSer();
    }
    if(ParsedArgs.ParDebug)
    {
	l++;
	kdebug = *ParsedArgs.ParDebug;
	g->g_Flags = GF_PARDEBUG;
	log_SetPar();
    }
    if(l > 1)
    {
	APrintReq("only one debug option can be provided\n");
	return 0;
    }
#endif
    s = strchr(ParsedArgs.RemoteDevice, ':');
    if(!s || (ParsedArgs.RemoteDevice == s) || (*(s+1) == 0))
    {
	FreeArgs(DosArgs);
	APrintReq("device does not match template <host>:<mntdir>\n");
	SetIoErr(ERROR_REQUIRED_ARG_MISSING);
	return 0;
    }
    l = s - ParsedArgs.RemoteDevice;
    g->g_Host = StrNDup(ParsedArgs.RemoteDevice, l);
    g->g_RootDir = StrDup(s+1);
#ifndef NO_DEVICE
    if(ParsedArgs.As)
	g->g_VolumeName = StrDup(ParsedArgs.As);
    else
#endif
	g->g_VolumeName = StrDup(ParsedArgs.LocalDevice);

    /* remove ":" if present */
    s = strchr(ParsedArgs.LocalDevice, ':');
    if(s)
	l = s - ParsedArgs.LocalDevice;
    else
	l = strlen(ParsedArgs.LocalDevice);
    
    if(l)
    {
	g->g_DeviceName = StrNDup(ParsedArgs.LocalDevice, l);
	if(g->g_DeviceName)
	{
	    /* device names containing "/" are illegal */
	    if(strchr(g->g_DeviceName, '/'))
		l = 0;
	}
    }
    if(Success)
    {
	if(ParsedArgs.User)
	{
	    Success = cr_SetUser(&g->g_Creds, ParsedArgs.User);
	    if(!Success)
	    {
		FreeArgs(DosArgs);
		APrintReq("Could not get passwd entry for %s\n",
			  ParsedArgs.User);
		return 0;
	    }
	}
    }
	
    if(!l || !g->g_Host || !g->g_RootDir || !g->g_VolumeName
       || !g->g_DeviceName || !Success)
    {
	FreeArgs(DosArgs);
	APrintReq("no mem\n");
	return 0;
    }
    s = g->g_VolumeName;
    while(s = strchr(s, ':'))
    {
	/* small hack, so that "al:" becomes "al" not "al_" */
	if(s[1])
	    *s = '_';
	else
	    *s = 0;
    }
    s = g->g_VolumeName;
    while(s = strchr(s, '/'))
	*s = '_';

    if(gethostname(g->g_HostName, sizeof(g->g_HostName)) < 0)
    {
	FreeArgs(DosArgs);
	APrintReq("Unable to get hostname\n");
	
	return 0;
    }
    /* limit volumename a bit */
    if(strlen(g->g_VolumeName) >250)
    {
	g->g_VolumeName[250] = 0;
    }
	
    /* if not set, UID/GID entries default to 0=root/wheel */
    if(ParsedArgs.MntUser)
    {
	pw = getpwnam(ParsedArgs.MntUser);
	if(!pw)
	{
	    APrintReq("Could not get passwd entry for %s\n",
		    ParsedArgs.MntUser);
	    FreeArgs(DosArgs);
	    return 0;
	}
	g->g_MntUID = UID2nfsuid(pw->pw_uid);
	g->g_MntGID = GID2nfsgid(pw->pw_gid);
    }

#if 0
    /* if not set, UID/GID entries default to nobody */
    
    User = ParsedArgs.User ? ParsedArgs.User : "nobody";

    pw = getpwnam(User);
    if(!pw)
    {
	APrintReq("Could not get passwd entry for %s\n",
		  User);
	FreeArgs(DosArgs);
	return 0;
    }
#if 1
    g->g_UID = pw->pw_uid;
    g->g_GID = pw->pw_gid;
#else
    g->g_UID = UID2nfsuid(pw->pw_uid);
    g->g_GID = GID2nfsgid(pw->pw_gid);
#endif
#endif
    
    if(ParsedArgs.UMask)
    {
	u_int umask;
	umask = *ParsedArgs.UMask;

	/* umask was interpreted as an integer but is indeed an octal */
	cr_SetUMask(&g->g_Creds,
		    (((umask / 100) % 10) & 7) * 0100
		    + (((umask / 10) % 10) & 7) * 010
		    + ((umask % 10) & 7));
    }
    
    if(ParsedArgs.MaxReadSize)
	g->g_NFSGlobal.ng_MaxReadSize = max(0x100,
					    min(NFS_MAXDATA,
						*ParsedArgs.MaxReadSize));
    else
	g->g_NFSGlobal.ng_MaxReadSize = NFS_MAXDATA;

    if(ParsedArgs.MaxReadDirSize)
	g->g_NFSGlobal.ng_MaxReadDirSize = max(0x100,
					       min(NFS_MAXDATA,
						   *ParsedArgs.MaxReadDirSize));
    else
	g->g_NFSGlobal.ng_MaxReadDirSize = NFS_MAXDATA;

    if(ParsedArgs.MaxWriteSize)
	g->g_NFSGlobal.ng_MaxWriteSize = max(0x100,
					     min(NFS_MAXDATA,
						 *ParsedArgs.MaxWriteSize));
    else
	g->g_NFSGlobal.ng_MaxWriteSize = NFS_MAXDATA;

    g->g_WriteBufferLimit = 2*8192; /* FIXME */

    if(ParsedArgs.Buffers)
	g->g_NumBuffers = *ParsedArgs.Buffers;
    else
	g->g_NumBuffers = NUMBUFFERS_DEFAULT;

    if(ParsedArgs.AttrCacheTimeout)
	g->g_AttrCacheTimeout = max(ATTR_CACHE_TIMEOUT_MIN,
				    min(ATTR_CACHE_TIMEOUT_MAX,
					*ParsedArgs.AttrCacheTimeout));
    else
	g->g_AttrCacheTimeout = ATTR_CACHE_TIMEOUT_DEFAULT;

    if(ParsedArgs.DirCacheTimeout)
	g->g_DirCacheTimeout = max(DIR_CACHE_TIMEOUT_MIN,
				    min(DIR_CACHE_TIMEOUT_MAX,
					*ParsedArgs.DirCacheTimeout));
    else
	g->g_DirCacheTimeout = DIR_CACHE_TIMEOUT_DEFAULT;

    if(ParsedArgs.RPCTimeout)
	g->g_RPCTimeout = max(RPC_TIMEOUT_MIN,
			      min(RPC_TIMEOUT_MAX,
				  *ParsedArgs.RPCTimeout));
    else
	g->g_RPCTimeout = RPC_TIMEOUT_DEFAULT;
    
    FreeArgs(DosArgs);
    
    return 1;
}

int
main(int argc, char *argv[])
{
    Global_T Globals, *g;
    UBYTE *s;
    LONG Result = 10;
    LONG TimerUp;
    struct DosList *dlist;

#if 0
    extern void __stdargs XSTIopenTimer(void);
    extern void __stdargs XSTIopenSockets(void);
    extern __stdargs void XSTDcloseSockets(void);
    extern __stdargs void XSTDcloseTimer(void);
    
    XSTIopenTimer();
    atexit(XSTDcloseTimer);

    XSTIopenSockets();
    atexit(XSTDcloseSockets);
#endif
    
    memset(&Globals, 0, sizeof(Globals));
    g = &Globals;
    signal(SIGINT, SIG_IGN);

    /* FIXME */
    SGlobals = g;

    if(stacksize() < 29000)
    {
	APrintReq("stacksize too low\nmust be >= 30000\n");
	goto earlyexit;
    }
    cr_Init(&g->g_Creds); /* Initialize credentials, they may be set by ParseArgs() */
    
    if(!ParseArgs(g, argv[0]))
    {
	goto earlyexit;
    }

    if(!G_Init(g))
	goto exit;

    TimerUp = 1;
    if(!act_Init(g))
    {
	APrintReq("init actions failed\n");
	goto exit;
    }
    s = NULL;
    if(!G_Mount(g, &s))
    {
	if(s)
	    APrintReq("mount failed:\n%s\n", s);
	goto exit;
    }

    dlist = LockDosList(LDF_READ | LDF_DEVICES);
    if(!dlist)
    {
	APrintReq("Unable to lock dos list\n");
	goto exit;
    }
    if(FindDosEntry(dlist, g->g_DeviceName, LDF_DEVICES))
    {
	UnLockDosList(LDF_READ | LDF_DEVICES);
	APrintReq("%s already mounted\n", g->g_DeviceName);
	
	goto exit;
    }
    UnLockDosList(LDF_READ | LDF_DEVICES);
    
#ifdef NO_DEVICE
    dlist = LockDosList(LDF_READ | LDF_VOLUMES);
    if(!dlist)
    {
	APrintReq("Unable to lock dos list\n");
	goto exit;
    }
    if(FindDosEntry(dlist, g->g_DeviceName, LDF_VOLUMES))
    {
	UnLockDosList(LDF_READ | LDF_VOLUMES);
	APrintReq("%s already mounted\n", g->g_DeviceName);
	
	goto exit;
    }
    UnLockDosList(LDF_READ | LDF_VOLUMES);
#endif
    
#ifndef NO_DEVICE
    if(!AddDosEntry((struct DosList *) g->g_Device))
    {
	APrintReq("AddDevice failed\n");
	goto exit;
    }
#endif
    if(!AddDosEntry((struct DosList *) g->g_Dev))
    {
#ifndef NO_DEVICE
	LockDosList(LDF_WRITE | LDF_DEVICES);
	if(!RemDosEntry((struct DosList *) g->g_Device))
	{
	    /* what do do ??? */
	}
	UnLockDosList(LDF_WRITE | LDF_DEVICES);
#endif
	
	APrintReq("AddVolume failed\n");
	goto exit;
    }
    
    g->g_State |= GF_STATE_RUNNING;
    Result = 0;
    while(g->g_State & GF_STATE_RUNNING)
    {
	struct DosPacket *MyPkt;
    
	MyPkt = G_WaitPkt(g);
	if(MyPkt)
	{
	    BOOL Internal;
	    LONG Arg1, Arg2, Arg3, Arg4;
	    LONG Res1;
	    ActionFunc Func;
	    
	    Globals.g_DoReply = 1;

#ifdef DEBUG
	    /* if we get this, some case was missed somewhere */
	    SetRes(g, DOSFALSE, ERROR_BAD_HUNK);
#endif
	    /* save arguments */
	    Arg1 = MyPkt->dp_Arg1;
	    Arg2 = MyPkt->dp_Arg2;
	    Arg3 = MyPkt->dp_Arg3;
	    Arg4 = MyPkt->dp_Arg4;
	    
	    Func = act_Prepare(g, MyPkt, &Internal);
	    if(Func)
	    {
		Res1 = DOSTRUE;

		/* update simple creds */
		if(!Internal && (Func != act_IOCONTROL))
		    cr_UpdateCreds(&g->g_Creds, MyPkt->dp_Port->mp_SigTask);
	    
		/* don\'t change Auth for Internal packets or control packets */
		if(!Internal && (Func != act_IOCONTROL))
		{
		    if(cr_AuthNeedsUpdate(&g->g_Creds, MyPkt->dp_Port->mp_SigTask))
		    {
			act_FLUSH(g, NULL); /* flush data using old auth */

			/* Note: g->g_NFSClnt->cl_auth will be invalid after successful call to
			   g_UpDateAuth() below !*/
			   
			if(cr_UpdateAuth(&g->g_Creds, MyPkt->dp_Port->mp_SigTask, g->g_HostName))
			{
			    g->g_NFSClnt->cl_auth = cr_GetAuth(&g->g_Creds);
			}
			else
			{
			    /* update failed */
			    SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE); /* FIXME: Auth error ? */
			    Res1 = DOSFALSE;
			}
		    }
		}

		if(Res1 == DOSTRUE)
		{
		    Res1 = Func(g, MyPkt);
		
		    /* restore arguments */
		    MyPkt->dp_Arg1 = Arg1;
		    MyPkt->dp_Arg2 = Arg2;
		    MyPkt->dp_Arg3 = Arg3;
		    MyPkt->dp_Arg4 = Arg4;

		    /* no references on cached entries anymore, so unlock
		       all possibly locked */
		    attrs_UnLockAll(g);
		    dc_UnLockAll(g);

		    if(Res1 == DOSTRUE)
			SetRes1(g, Res1);
		}
	    }
	    else
	    {
		SetRes(g, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
	    }
	    
	    if(Globals.g_DoReply)
	    {
#ifdef DEBUG
		if(Globals.g_Res1 != DOSFALSE)
		{
		    D(DBF_ALWAYS, "\t--- succeeded (0x%08lx (0x%08lx))",
			     Globals.g_Res1,
			     BADDR(Globals.g_Res1));
		}
		else
		{
		    D(DBF_ALWAYS, "\t*** failed with %ld",
			     Globals.g_Res2);
		}
#endif
		G_ReplyPkt(g, MyPkt);
	    }
	}
	else
	{
	    if(act_DIE(g, NULL) == DOSTRUE)
		g->g_State &= ~GF_STATE_RUNNING;
	}
    }

    ti_Cleanup(g); /* clean up timer (timer packet must not be pending
		      when entering loop below) */
    TimerUp = 0;
    
    LockDosList(LDF_WRITE |LDF_VOLUMES);
    if(!RemDosEntry((struct DosList *) g->g_Dev))
    {
	/* what do do ??? */
	UnLockDosList(LDF_WRITE | LDF_VOLUMES);
	ASSERT("Remove Volume failed" == NULL);
    }
    else
	UnLockDosList(LDF_WRITE | LDF_VOLUMES);

#ifndef NO_DEVICE
    g->g_Device->dvi_Task = NULL;
    g->g_Device->dvi_SegList = NULL;
#endif

    Delay(2*50); /* let the dust settle ... */
    if(1)
    {
    	struct DosPacket *MyPkt;

	Disable();
	while(MyPkt = G_GetPkt(g))
	{
	    SetRes(g, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
	    G_ReplyPkt(g, MyPkt);
	}
	Enable();
    }

 exit:
    if(TimerUp)
	ti_Cleanup(g); /* remove timer */

    G_UnMount(g);
    G_CleanUp(g);

#ifdef DEBUG
    log_end();
#endif
    
 earlyexit:
#if 0
    {
	extern __stdargs void XSTDcloseSockets(void);
	extern __stdargs void XSTDcloseTimer(void);

	XSTDcloseSockets();
	XSTDcloseTimer();
    }
#endif
    exit(Result);
}

/*
 * Local variables:
 * compile-command: "gmake"
 * end:
 */


