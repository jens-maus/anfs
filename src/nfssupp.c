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

#include <stdio.h>
#include <string.h>

#include "nfs_handler.h"

#include "Debug.h"

#define CASE(a,b) case a: s = b; break;
#define CASE1(a) case a: s = #a; break;

const char *
StrNFSErr(nfsstat i)
{
    const char *s;

    switch(i)
    {
	CASE(NFS_OK,"no error");
	CASE(NFSERR_PERM, "not owner");
	CASE(NFSERR_NOENT, "no such file or directory");
	CASE(NFSERR_IO,"I/O error");
	CASE(NFSERR_NXIO, "no such device or address");
	CASE(NFSERR_ACCES, "permission denied");
	CASE(NFSERR_EXIST, "file exists");
	CASE(NFSERR_NODEV, "no such device");
	CASE(NFSERR_NOTDIR, "not a directory");
	CASE(NFSERR_ISDIR, "is a directory");
	CASE(NFSERR_FBIG, "file too large");
	CASE(NFSERR_NOSPC, "no space left on device");
	CASE(NFSERR_ROFS, "read-only file system");
	CASE(NFSERR_NAMETOOLONG, "file name too long");
	CASE(NFSERR_NOTEMPTY, "directory not empty");
	CASE(NFSERR_DQUOT, "disc quota exceeded");
	CASE(NFSERR_STALE, "stale NFS file handle");
	CASE(NFSERR_WFLUSH, "write cache flushed");
default:
	s = "unknown nfs error";
    }
    return s;
}

#define LCASE(a,b) case a: l = b; break

/* use context sensitive version when possible */
   
LONG
NFSErr2IoErr(nfsstat i)
{
    LONG l;

    switch(i)
    {
	LCASE(NFS_OK, 0);
	LCASE(NFSERR_PERM, 	ERROR_WRITE_PROTECTED); 
	LCASE(NFSERR_NOENT, 	ERROR_OBJECT_NOT_FOUND);
	LCASE(NFSERR_IO, 	ERROR_OBJECT_NOT_FOUND);
	LCASE(NFSERR_NXIO, 	ERROR_OBJECT_NOT_FOUND);
	LCASE(NFSERR_ACCES, 	ERROR_WRITE_PROTECTED); 
	LCASE(NFSERR_EXIST, 	ERROR_OBJECT_EXISTS);
	LCASE(NFSERR_NODEV,	ERROR_OBJECT_NOT_FOUND);
	LCASE(NFSERR_NOTDIR,	ERROR_OBJECT_WRONG_TYPE);
	LCASE(NFSERR_ISDIR,    	ERROR_OBJECT_WRONG_TYPE);
	LCASE(NFSERR_FBIG,	ERROR_OBJECT_TOO_LARGE);
	LCASE(NFSERR_NOSPC,	ERROR_DISK_FULL);
	LCASE(NFSERR_ROFS,	ERROR_DISK_WRITE_PROTECTED);
	LCASE(NFSERR_NAMETOOLONG, ERROR_BAD_STREAM_NAME);
	LCASE(NFSERR_NOTEMPTY,	ERROR_DIRECTORY_NOT_EMPTY);
	LCASE(NFSERR_DQUOT,	ERROR_DISK_FULL);
	LCASE(NFSERR_STALE,	ERROR_INVALID_LOCK);
	LCASE(NFSERR_WFLUSH,	100); /* FIXME */
default:
	l = ERROR_BAD_NUMBER;
    }
    return l;
}

/*
 * context sensitive error conversion routine
 *
 * type of action (acttype) helps to choose
 * appropiate error code
 *
 * supported types:
 *	'r'	read
 *	'w'	write
 *	'c'	create
 */

LONG
NFSErr2IoErrCont(nfsstat i, char acttype)
{
    LONG l=0;

    if(i)
    {
	if(acttype)
	{
	    switch(acttype)
	    {
	case 'c':
	case 'w':
		switch(i)
		{
		    LCASE(NFSERR_PERM, 	ERROR_WRITE_PROTECTED);
		    LCASE(NFSERR_ACCES,	ERROR_WRITE_PROTECTED);
		}
		break;
	case 'r':
		switch(i)
		{
		    LCASE(NFSERR_PERM, 	ERROR_READ_PROTECTED);
		    LCASE(NFSERR_ACCES,	ERROR_READ_PROTECTED);
		}
		break;
	    }
	}
	if(!l)
	{
	    l = NFSErr2IoErr(i);
	}
    }
    return l;
}

const char *
StrNFSType(int t)
{
    const char *s;

    switch(t)
    {
	CASE1(NFNON);
	CASE1(NFREG);
	CASE1(NFDIR);
	CASE1(NFBLK);
	CASE1(NFCHR);
	CASE1(NFLNK);
	CASE1(NFSOCK);
	CASE1(NFBAD);
	CASE1(NFFIFO);
default:
	s = "unknown nfs types";
    }
    return s;
}

static const struct CSCEnt {
    enum clnt_stat clerr;
    LONG IoErr;
} ClntStatConv[] = /* FIXME: does 3.0 has codes fitting better ? */
{
{RPC_SUCCESS, 0},			/* call succeeded */
	/*
	 * local errors
	 */
#if 0
{RPC_CANTENCODEARGS, ERROR_REQUIRED_ARG_MISSING},/* can't encode arguments */
{RPC_CANTDECODERES, ERROR_REQUIRED_ARG_MISSING}, /* can't decode results */
{RPC_CANTSEND, ERROR_NO_FREE_STORE}, /* failure in sending call */
{RPC_CANTRECV, ERROR_NO_FREE_STORE}, /* failure in receiving result */
{RPC_TIMEDOUT, ERROR_NO_FREE_STORE}, /* call timed out */
	/*
	 * remote errors
	 */
{RPC_VERSMISMATCH, ERROR_NO_FREE_STORE}, 	/* rpc versions not compatible */
{RPC_AUTHERROR, ERROR_READ_PROTECTED},		/* authentication error */
{RPC_PROGUNAVAIL, ERROR_NO_FREE_STORE},		/* program not available */
{RPC_PROGVERSMISMATCH, ERROR_NO_FREE_STORE},	/* program version mismatched */
{RPC_PROCUNAVAIL, ERROR_NO_FREE_STORE},		/* procedure unavailable */
{RPC_CANTDECODEARGS, ERROR_NO_FREE_STORE},	/* decode arguments error */
{RPC_SYSTEMERROR, ERROR_NO_FREE_STORE}	,	/* generic "other problem" */
	/*
	 * callrpc & clnt_create errors
	 */
{RPC_UNKNOWNHOST, ERROR_NO_FREE_STORE},		/* unknown host name */
{RPC_UNKNOWNPROTO, ERROR_NO_FREE_STORE},	/* unkown protocol */
	/*
	 * _ create errors
	 */
{RPC_PMAPFAILURE, ERROR_NO_FREE_STORE},		/* the pmapper failed in its call */
{RPC_PROGNOTREGISTERED, ERROR_NO_FREE_STORE},	/* remote program is not registered */
	/*
	 * unspecified error
	 */
{RPC_FAILED, ERROR_NO_FREE_STORE}
#endif
};

LONG
CLIENT2IoErr(CLIENT *clnt)
{
    struct rpc_err clerr;
    enum clnt_stat clstat;
    int i, max;

    clnt_geterr(clnt, &clerr);

    clstat = clerr.re_status;
    
    E(DBF_ALWAYS,"\tCLIENT2IoErr: %ld, %s", clstat, clnt_sperrno(clstat));

    max = sizeof(ClntStatConv)/sizeof(struct CSCEnt);
    for(i=0; i< max; i++)
    {
	if(ClntStatConv[i].clerr == clstat)
	    return ClntStatConv[i].IoErr;
    }
    return 1000 + clstat; /* FIXME */
}

#if 0
const char *
StrNFSMode(unsigned int i)
{
    static char modes[10];
    unsigned int fmt;
    char c;

    modes[9] = 0;
    fmt = i & NFSMODE_FMT;
    switch(fmt)
    {
 case NFSMODE_DIR: c = 'd'; break;
 case NFSMODE_CHR: c = 'c'; break;
 case NFSMODE_BLK: c = 'b'; break;
 case NFSMODE_REG: c = '-'; break;
 case NFSMODE_LNK: c = 'l'; break;
 case NFSMODE_SOCK: c = 's'; break;
 case NFSMODE_FIFO: c = 'f'; break;
default: c = '?';
    }
    modes[0] = c;
    fmt = i & 0777; 
    for(i = 0; i <= 6; i+=3) 
    {
	modes[i+1] = (fmt & 0400) ? 'r' : '-';
	modes[i+2] = (fmt & 0200) ? 'w' : '-';
	modes[i+3] = (fmt & 0100) ? 'x' : '-';
	fmt <<= 3;
    }
    return modes;
}
#endif

UWORD
nfsuid2UID(u_int uid)
{
   /* different root/nobody mapping (FIXME correct ?) */
    if(uid >= 0xFFFF)
	return 0;
    else if(uid == 0) 
	return 0xFFFF;
    else
	return (UWORD) uid;
}

UWORD
nfsgid2GID(u_int gid)
{
    if(gid >= 0xFFFF)
	return 0;
    else if(gid == 0)
	return 0xFFFF;
    else
	return (UWORD) gid;
}

u_int
UID2nfsuid(UWORD UID)
{
    if(UID == 0xFFFF)
	return 0;
    else if(UID == 0) /* nobody */
	return (u_int) -1;
    else
	return UID;
}

u_int
GID2nfsgid(UWORD GID)
{
    if(GID == 0xFFFF)
	return 0;
    else if(GID == 0) /* nogroup */
	return (u_int) -1;
    else
	return GID;
}

#ifndef NFS_INLINE
LONG
IsSameFH(const nfs_fh *s, const nfs_fh *d)
{
    return !memcmp(d,s, FHSIZE);
}
#endif
