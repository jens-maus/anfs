#ifndef NFS_HANDLER_H
#define NFS_HANDLER_H

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
 * Global declarations and includes.
 */

#define NO_DEVICE 1 /* FIXME */

#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>

#include "xdos.h" /* extended dos/dos.h, dos/dosextend.h */
#include "nfsc_iocontrol.h"

#include <unistd.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <proto/usergroup.h>

#include "mount.h"
#include "nfs_prot.h"

#include "simplerexx.h"

typedef struct DosPacket DOSPKT;
typedef struct List LIST;
typedef struct FileInfoBlock FIB;
typedef struct FileLock LOCK;
typedef struct InfoData IDATA;
typedef struct FileHandle FHANDLE;

#include "locks.h"
#include "dirs.h"
#include "attrs.h"
#include "dcache.h"
#include "fh.h"
#include "timer.h"
#include "wbuf.h"
#include "rbuf.h"
#include "mbuf.h"
#include "nfs.h"
#include "cred.h"

#define nfsc_GetUID(g) (cr_GetUID(&g->g_Creds))
#define nfsc_GetGID(g) (cr_GetGID(&g->g_Creds))
#define nfsc_ApplyUMask(g, umode) (((umode | cr_GetUMask(&g->g_Creds))) ^ cr_GetUMask(&g->g_Creds))

#define GF_STATE_RUNNING 1

#define GF_IS_MOUNTED 		1
#define GF_IS_WRITE_PROTECTED 	2
#define GF_USE_TIMEOUT_REQ	4
#define GF_PARDEBUG		8
#define GF_SERDEBUG		0x10
#define GF_SLOW_MEDIUM		0x20 /* hint that medium is slow */
#define GF_ASYNC_RPC		0x40 /* use asynchronous RPC */
	
typedef struct Global_T
{
    /* these are main private (read only) */
    /* exceptions: Lock handling will use dl_LockList part of Dev */
#ifndef NO_DEVICE
    struct DevInfo *g_Device;
#endif
    struct DeviceList *g_Dev; /* FIXME: name should be Volume */
    struct MsgPort *g_Port;
    char *g_UserName;
    const char *g_Host;
    const char *g_RootDir;
    char *g_VolumeName;
    char *g_DeviceName;
    LONG g_Flags;
    ULONG g_LockKey;		/* key for software write protect */
    ULONG g_NumBuffers;		/* number of "buffers" (AddBuffers) */
    ULONG g_MaxFileNameLen;     /* Maximal amiga filename len */
    struct DosPacket g_IntPkt;  /* internal "packet" (e.g. ARexx Port)*/
    AREXXCONTEXT g_ARexxCont;	/* ARexx Port (SimpleRexx) */
    ULONG g_ARexxSig;
	
    /* sync timer (time defined in timer.h) */
    SyncTimer g_SyncTimer;
    
    /* config stuff */
    NFSGlobal_T g_NFSGlobal;

    ULONG g_WriteBufferLimit;	/* maximum length of single write
				   request which is buffered */
    /* remote stuff */
    nfs_fh g_MntFh;		/* mountd file handle but nfsd structure */
    CLIENT *g_MntClnt;		/* mountd client handle */
    CLIENT *g_NFSClnt;		/* nfsd client handle */
    fattr g_MntAttr;		/* contains file attributes of mount point
				   at startup, currently ony fileid will
				   be used */
    UBYTE g_HostName[MAXHOSTNAMELEN]; /* This host\'s name */
    u_int g_MntUID;		      /* remote user id, used for mount
					 rpc authorization */
    u_int g_MntGID;		/* remote group id, used for mount
				   rpc authorization */
#if 0
    u_int g_UID;		/* remote user id, used for nfs
				   rpc authorization and calls */
    u_int g_GID;		/* remote group id, used for nfs
				   rpc authorization and calls */
    u_int g_UMask;		/* used for nfs rpc calls */
/*    ULONG g_GIDS[1]; FIXME: groups, user belongs to */
#else
    CRED g_Creds;
#endif
    LONG g_RPCTimeout;		/* main timeout for rpc calls */
    
    /* these may be modified by find procedures */
    EFH *g_FH;			/* points to queued filehandle id\'s */

    /* these may be modified by ExNext procedure */
    EDH *g_DH;			/* points to queued directory handles */

    /* caches */
    void *g_AttrCache;		/* attribute cache */
    LONG g_AttrCacheTimeout;	/* in secs */
    LONG g_AttrCacheMaxEntries;
    void *g_DirCache;		/* directory cache */ 
    LONG g_DirCacheTimeout;	/* in secs */
    LONG g_DirCacheMaxEntries;
#if 0
    DCEntry g_MntDirEntry;
#endif

    /* common memory buffers */
    LONG g_NumMBufs;
    LONG g_NumFreeMBufs;
    MBUFNODE *g_FreeMBufs;
    
    /* write buf stuff */
    LONG g_BufSizePerFile;
    LONG g_NumWBufHdrs;
    LONG g_NumFreeWBufHdrs;
    /* important here: number of WBufs must match WBufHdrs */
    WBUFHDR *g_FreeWBufHdr;

    /* read buf stuff */
#if 0
    LONG g_NumRBufs;
    LONG g_NumFreeRBufs;
    RBUFNODE *g_FreeRBufs;
#endif
    
    /* these may be modified by action procedures */
#if 0 /* needed ? */
    struct Task *g_Client; /* address of task using this handler */
#endif
    LONG g_State;
    LONG g_DoReply;
    LONG g_Res1;
    LONG g_Res2;
} Global_T;

typedef LONG (*ActionFunc) (Global_T*, DOSPKT*);

#define SetRes(g, res1, res2) (g->g_Res2 = res2, g->g_Res1 = res1)
#define SetRes1(g, res1) (g->g_Res1 = res1)

typedef UBYTE *CBSTR; /* APTR to BCPL string */

#define fputbstr(f,bs) fwrite(&bs[1],1,bs[0],f)

/* lock.c/DoLock */

/* don\'t do lookup of file */
#define DOLF_NO_LOOKUP 1 

#define DOLF_IS_CSTR 2
#define DOLF_IGN_NOT_EXIST_ERR 4

/* some default values (in secs) */
#define ATTR_CACHE_TIMEOUT_MIN 		0 
#define ATTR_CACHE_TIMEOUT_DEFAULT 	30
#define ATTR_CACHE_TIMEOUT_MAX 		(60*60) /* for slip users */
#define DIR_CACHE_TIMEOUT_MIN 		0
#define DIR_CACHE_TIMEOUT_DEFAULT 	60
#define DIR_CACHE_TIMEOUT_MAX		(60*60) /* for slip users */

/* MAXENTRIES of caches must be at this minimum */
#define ATTR_MINMAXENTRIES	20
#define DIR_MINMAXENTRIES	20

/* default buffer size on startup */
#define NUMBUFFERS_DEFAULT	32

#define RPC_TIMEOUT_MIN 		25
#define RPC_TIMEOUT_DEFAULT 		25
#define RPC_TIMEOUT_MAX			(10*60)

/* FIB/ExAll: 108 bytes for filename, first byte for len, one byte for \0 */
#define MAX_AMIGAFILENAMELEN 30

#define MAX_LONGAMIGAFILENAMELEN 106

#endif
