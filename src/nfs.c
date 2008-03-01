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
 * Link between amiga and nfs functions, returns amiga error codes.
 */

#include <math.h> /* for/min/max */

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#include "Debug.h"

#define CLIENTERROR(cl) 	*Res2 = CLIENT2IoErr(cl);

#ifdef DEBUG
static void fhdebug(const nfs_fh *fh)
{
    LONG *lp;

    lp = (LONG *) fh;
    D(DBF_ALWAYS,"\t\t(0x%08lx 0x%08lx 0x%08lx 0x%08lx",
	     lp[0], lp[1], lp[2], lp[3]);
    D(DBF_ALWAYS,"\t\t 0x%08lx 0x%08lx 0x%08lx 0x%08lx)",
	     lp[4], lp[5], lp[6], lp[7]);
}

#define FHDEBUG(fh) fhdebug(fh)
#else
#define FHDEBUG(fh)
#endif


fattr *nfs_GetAttr(CLIENT *clnt, nfs_fh *file, LONG *Res2)
{
    attrstat *res;

#ifdef DEBUG
    D(DBF_ALWAYS,"\tnfs_GetAttr()");
    FHDEBUG(file);
#endif

    res = nfsproc_getattr_2(file, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS, "getattr failed:%s", StrNFSErr(res->status));
	*Res2 = NFSErr2IoErrCont(res->status, 'r');
	return NULL;
    }
    return &res->attrstat_u.attributes;
}

fattr *nfs_SetAttr(CLIENT *clnt, nfs_fh *filefh, 
		   ULONG Mode, ULONG Uid, ULONG Gid, ULONG Size, 
		   nfstime *tv,
		   LONG *Res2)
{
    attrstat *res;
    sattrargs sargs;
    sattr *a;

    sargs.file = *filefh;

    a = &sargs.attributes;

    a->mode = Mode;
    a->uid = Uid;
    a->gid = Gid;
    a->size = Size;
    if(!tv)
    {
	a->atime.seconds = -1;
	a->atime.useconds = -1;
	a->mtime.seconds = -1;
	a->mtime.useconds = -1;
    }
    else
    {
	a->atime.seconds  = a->mtime.seconds  =	tv->seconds;
	a->atime.useconds = a->mtime.useconds = tv->useconds;
    }

    res = nfsproc_setattr_2(&sargs, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_SetAttr failed: %s", StrNFSErr(res->status));

	*Res2 = NFSErr2IoErrCont(res->status, 'w');
	return NULL;
    }
    return &res->attrstat_u.attributes;
}

/* case insensitive lookup */

/* FIXME: place for speedup: 
 * 1. avoid nfs_ReadDir alloc/free,
 * 2. use ReadDir results for cache update
 */

diropokres *nfs_LookupNC(CLIENT *clnt, 
			 const nfs_fh *dirfh, 
			 const UBYTE *name,
			 ULONG MaxRead,
			 LONG *Res2)
{
    entry *entries;
    nfscookie mynfscookie;
    LONG Eof = 0;
    UBYTE Name[255];
    LONG NameLen;

    NameLen = strlen(name);

    MaxRead = min(MaxRead, NFS_MAXDATA);
    memset(&mynfscookie, 0, sizeof(mynfscookie));

    while(1)
    {
	entries = nfs_ReadDir(clnt, dirfh, mynfscookie, MaxRead, &Eof, Res2);
	if(!entries)
	{
	    if(Eof)
		*Res2 = ERROR_OBJECT_NOT_FOUND;
	    return NULL;
	}
	while(entries)
	{
	    if(((NameLen >= 106) 
		&& (strnicmp(entries->name, (char *) name, 106) == 0)) 
	       || ((NameLen < 106) 
		   && (stricmp(entries->name, (char *) name) == 0)))
	    {
		strcpy(Name, entries->name);
		return nfs_ILookup(clnt, dirfh, Name, MaxRead, Res2, 0);
	    }

	    if(!entries->nextentry)
		CopyCookie(entries->cookie, mynfscookie);
	    entries = entries->nextentry;
	}
	if(Eof)
	{
	    *Res2 = ERROR_OBJECT_NOT_FOUND;
	    return NULL;
	}
    }
}

diropokres *nfs_ILookup(CLIENT *clnt, 
			const nfs_fh *dirfh, 
			const UBYTE *name,
			ULONG MaxReadDirSize,
			LONG *Res2,
			int DoCaseInsensitive)
{
    diropres *res;
    diropargs dop;

    dop.dir = *dirfh;
    dop.name = (UBYTE *) name; /* just trust */

#ifdef DEBUG
    D(DBF_ALWAYS,"\tnfs_Lookup(\"%s\")", name);
    FHDEBUG(dirfh);
#endif

    res = nfsproc_lookup_2(&dop, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if((res->status == NFSERR_NOENT) && DoCaseInsensitive)
    {
	/* exact name does not match, AmigaDOS is not case sensitive so
	   we need to do a case insensitive search */
	return nfs_LookupNC(clnt, dirfh, name, MaxReadDirSize, Res2);
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_lookup failed: %s", StrNFSErr(res->status));

	*Res2 = NFSErr2IoErrCont(res->status, 'r');
	return NULL;
    }
    return &res->diropres_u.diropres;
}

diropokres *nfs_Lookup(CLIENT *clnt, 
		       const nfs_fh *dirfh, 
		       const UBYTE *name,
		       ULONG MaxReadDirSize,
		       LONG *Res2)
{
    return nfs_ILookup(clnt, dirfh, name, MaxReadDirSize, Res2, 1);
}

nfspath nfs_ReadLink(CLIENT *clnt, nfs_fh *file, LONG *Res2)
{
    readlinkres *res;

    res = nfsproc_readlink_2(file, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS,"readlink failed:%s", StrNFSErr(res->status));
	*Res2 = NFSErr2IoErrCont(res->status, 'r');
	return NULL;
    }
    return res->readlinkres_u.data;
}

#if 0
LONG nfs_Read(CLIENT *clnt, nfs_fh *fh, LONG offset, UBYTE *Buf,
	      LONG count, LONG *FileLen, LONG *Res2)
{
    LONG Len;
    readargs rargs;
    readres *res;

#ifdef DEBUG
    D(DBF_ALWAYS, "\tnfs_Read(0x%08lx (offs), 0x%08lx (cnt) ... )", offset, count);
    FHDEBUG(fh);
#endif

    CopyFH(fh, &rargs.file);
    rargs.offset = offset;
    rargs.count = count;

    res = nfsproc_read_2(&rargs, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return -1;
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS, "read failed:%s", StrNFSErr(res->status));
	*Res2 = NFSErr2IoErrCont(res->status, 'r');
	return -1;
    }
    Len = res->readres_u.reply.data.data_len;
    if(Len)
	CopyMem(res->readres_u.reply.data.data_val, Buf, Len);

    *FileLen = res->readres_u.reply.attributes.size;

    D(DBF_ALWAYS,"\t\t-> 0x%08lx (size), 0x%08lx (len read)", *FileLen, Len);
    return Len;
}
#endif

readres *nfsproc_read_buf(readargs *argp, CLIENT *clnt, char *buf);

LONG
nfs_MRead(NFSGlobal_T *ng, CLIENT *clnt, nfs_fh *fh, ULONG offset, UBYTE *Buf,
	  ULONG count, LONG *FileLen, LONG *Res2)
{
    ULONG MaxRead;
    LONG Len, TotLen=0;
    readargs rargs;
    readres *res;

#ifdef DEBUG
    D(DBF_ALWAYS, "\tnfs_MRead(0x%08lx (offs), 0x%08lx (cnt) ... )", offset, count);
    FHDEBUG(fh);
#endif

    MaxRead = min(ng->ng_MaxReadSize, NFS_MAXDATA);
    CopyFH(fh, &rargs.file);

    while(count)
    {
	rargs.offset = offset;
	if(count < MaxRead)
	    rargs.count = count;
	else
	    rargs.count = MaxRead; 

	res = nfsproc_read_buf(&rargs, clnt, Buf);
	if (res == NULL) 
	{
	    CLIENTERROR(clnt);
	    return -1;
	}
	if(res->status != NFS_OK)
	{
	    W(DBF_ALWAYS, "(m)read failed:%s", StrNFSErr(res->status));
	    *Res2 = NFSErr2IoErrCont(res->status, 'r');
	    return -1;
	}
	Len = res->readres_u.reply.data.data_len;

#if 0
	if(Len)
	    CopyMem(res->readres_u.reply.data.data_val, Buf, Len);
	else
	    break; /* FIXME ? */
#else
	if(!Len)
	    break; /* FIXME ? */
#endif

	Buf += Len;
	offset += Len;
	count -= Len;
	TotLen += Len;
    }

    *FileLen = res->readres_u.reply.attributes.size;

    D(DBF_ALWAYS,"\t\t-> 0x%08lx (size), 0x%08lx (len read)", *FileLen, TotLen);
    return TotLen;
}

int
nfsproc_send_read_buf(readargs *argp, CLIENT *clnt,
		      readres *clnt_res,
		      char *buf,
		      void *userdata);

readres *nfsproc_wait_read_buf(CLIENT *clnt, void **userdata);

#define RNUMASYNC 5

struct rargsent
{
    int used;
    
    readargs rargs;
    readres res;
};

LONG
nfs_MARead(NFSGlobal_T *ng, CLIENT *clnt, nfs_fh *fh, ULONG offset, UBYTE *Buf,
	  ULONG count, LONG *FileLenP, LONG *Res2)
{
    ULONG MaxRead;
    LONG TotLen=0;
    struct rargsent rhs[RNUMASYNC];
    LONG numpending = 0;
    LONG NumRPCs;
    int i, queue=0;
    int reading;
    int eof=0; /* eof will be set when reading less bytes then requested */
    LONG UpperOffs=-1; /* will be set to offset of last byte of block which raises
		       eof. When set
		       - all blocks received after raising eof must be as
		          long as requested or of length NULL,
		       - read errors beyond UpperOffs will be ignored
		       */
    LONG FileLen = 0;

#ifdef DEBUG
    D(DBF_ALWAYS, "\tnfs_MRead(0x%08lx (offs), 0x%08lx (cnt) ... )", offset, count);
    FHDEBUG(fh);
#endif

    for(i = 0; i < RNUMASYNC; i++)
    {
	rhs[i].used = 0;
	CopyFH(fh, &rhs[i].rargs.file);
    }
    
    MaxRead = min(ng->ng_MaxReadSize, NFS_MAXDATA);
    NumRPCs = ng->ng_NumRRPCs;
    
    reading = 1;
    while(reading || numpending)
    {
	if((reading && (numpending >= NumRPCs))
	   ||(!reading && numpending))
	{
	    struct rargsent *rh;
	    readres *res;
	    
	    res = nfsproc_wait_read_buf(clnt, (void **) &rh);
	    if (res == NULL) 
	    {
		clnt_abort(clnt);
		CLIENTERROR(clnt);
		return -1;
	    }
	    /* when reading beyond file length we should get
	       NFS_OK with len=0 but we don\'t count on it */
	    if(res->status != NFS_OK)
	    {
		if(eof || (rh->rargs.offset <= UpperOffs))
		{
		    clnt_abort(clnt);
		    W(DBF_ALWAYS, "(m)read failed:%s", StrNFSErr(res->status));
		    *Res2 = NFSErr2IoErrCont(res->status, 'r');
		    return -1;
		}
	    }
	    else
	    {
		if(res->readres_u.reply.data.data_len < rh->rargs.count)
		{
		    if(eof)
		    {
			if(res->readres_u.reply.data.data_len)
			{
			    /* incomplete read and eof already raised => error */
			    clnt_abort(clnt);
			    W(DBF_ALWAYS, "(m)read failed, incomplete read");
			
			    *Res2 = ERROR_OBJECT_NOT_FOUND;
			    return -1;
			}
		    }
		    else
		    {
			eof = 1;
			reading = 0;
			UpperOffs = rh->rargs.offset + rh->rargs.count -1;
		    }
		}
	    
		TotLen += res->readres_u.reply.data.data_len;
	    
		FileLen = max(FileLen, res->readres_u.reply.attributes.size);
	    }
	    rh->used = 0;
	    numpending--;
	}
	if(reading)
	{
	    struct rargsent *rh;
	    int ok;
	    
	    /* get free entry */
	    i = -1;
	    do
	    {
		if(!rhs[queue].used)
		    i = queue;
		queue++;
		if(queue >= RNUMASYNC)
		    queue = 0;
	    } while(i<0);

	    rh = &rhs[i];
	    rh->rargs.offset = offset;
	    
	    if(count < MaxRead)
		rh->rargs.count = count;
	    else
		rh->rargs.count = MaxRead; 

	    ok = nfsproc_send_read_buf(&rh->rargs, clnt, &rh->res, Buf,
				       (void*) rh);
	    if (ok == 0)
	    {
		clnt_abort(clnt);
		CLIENTERROR(clnt);
		return -1;
	    }
	    else
	    {
		LONG Len;
		
		rh->used = 1;
		numpending++;
		Len = rh->rargs.count;
		Buf += Len;
		offset += Len;
		count -= Len;
		if(count == 0)
		    reading = 0;
	    }
	}
    }
    *FileLenP = FileLen;

    D(DBF_ALWAYS,"\t\t-> 0x%08lx (size), 0x%08lx (len read)", FileLen, TotLen);

    return TotLen;
}

#if 0
LONG nfs_Write(CLIENT *clnt, nfs_fh *fh, LONG offset, UBYTE *Buf,
	      LONG count, LONG *FileLen, LONG *Res2)
{
    LONG Len;
    writeargs wargs;
    attrstat *res;

    CopyFH(fh, &wargs.file);
    wargs.offset = offset;
    wargs.data.data_len = count;
    wargs.data.data_val = Buf;

    res = nfsproc_write_2(&wargs, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return -1;
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS, "write failed:%s", StrNFSErr(res->status));
	*Res2 = NFSErr2IoErrCont(res->status, 'w');
	return -1;
    }
    Len = count; /* FIXME ? */
    *FileLen = res->attrstat_u.attributes.size;

    return Len;
}
#endif

LONG
nfs_MWrite(NFSGlobal_T *ng, CLIENT *clnt, nfs_fh *fh, ULONG offset, UBYTE *Buf,
	   ULONG arg_count, LONG *FileLen, LONG *Res2)
{
    ULONG MaxWrite;
    LONG Len, count = arg_count;
    writeargs wargs;
    attrstat *res;

#ifdef DEBUG
    D(DBF_ALWAYS, "\tnfs_MWrite(0x%08lx (offs), 0x%08lx (cnt) ... )", offset, count);
    FHDEBUG(fh);
#endif

    MaxWrite = min(ng->ng_MaxWriteSize, NFS_MAXDATA);
    CopyFH(fh, &wargs.file);

    while(count)
    {
	wargs.offset = offset;
	if(count < MaxWrite)
	    Len = count;
	else
	    Len = MaxWrite;
	wargs.data.data_len = Len;
	wargs.data.data_val = Buf;

	res = nfsproc_write_2(&wargs, clnt);
	if (res == NULL) 
	{
	    CLIENTERROR(clnt);
	    return -1;
	}
	if(res->status != NFS_OK)
	{
	    W(DBF_ALWAYS, "(m)write failed:%s", StrNFSErr(res->status));
	    *Res2 = NFSErr2IoErrCont(res->status, 'w');
	    return -1;
	}
	Buf += Len;
	offset += Len;
	count -= Len;
    }
	
    *FileLen = res->attrstat_u.attributes.size;

    return (LONG) arg_count; /* FIXME ? */
}

/* FIXME */
extern int nfsproc_send_write_2(writeargs *argp, CLIENT *clnt); 
attrstat *nfsproc_wait_write_2(CLIENT *clnt);
attrstat *nfsproc_check_write_2(CLIENT *clnt, int *error_flag);
int nfsproc_send_writesb_2(writeargs *argp, CLIENT *clnt);

LONG
nfs_MAWrite(NFSGlobal_T *ng, CLIENT *clnt, nfs_fh *fh, ULONG offset, UBYTE *Buf,
	   ULONG arg_count, LONG *FileLenP, LONG *Res2)
{
    LONG MaxWrite;
    LONG count = arg_count;
    writeargs wargs;
    int numpending = 0;
    int NumRPCs;
    LONG FileLen = 0;
    
#ifdef DEBUG
    D(DBF_ALWAYS, "\tnfs_SMWrite(0x%08lx (offs), 0x%08lx (cnt) ... )", offset, count);
    FHDEBUG(fh);
#endif

    MaxWrite = min(ng->ng_MaxWriteSize, NFS_MAXDATA);
    CopyFH(fh, &wargs.file);
    NumRPCs = ng->ng_NumWRPCs;

    while(count || numpending)
    {
	if((numpending >= NumRPCs) || ((count == 0) && numpending))
	{
	    attrstat *res;
	    
	    D(DBF_ALWAYS, "\twait now");
	    res = nfsproc_wait_write_2(clnt);
	    if(res == NULL)
	    {
		clnt_abort(clnt);
		
		CLIENTERROR(clnt);
		return -1;
	    }
	    if(res->status != NFS_OK)
	    {
		clnt_abort(clnt);

		W(DBF_ALWAYS, "send_(m)write failed:%s", StrNFSErr(res->status));
		*Res2 = NFSErr2IoErrCont(res->status, 'w');
		return -1;
	    }
	    FileLen = max(res->attrstat_u.attributes.size, FileLen);
	    numpending--;
#if 0
	    /* check for second result */
	    if(numpending)
	    {
		res = nfsproc_check_write_2(clnt, &error_flag);
		if(res == NULL)
		{
		    if(error_flag)
		    {
			clnt_abort(clnt);

			CLIENTERROR(clnt);
			return -1;
		    }
		}
		else
		{
		    if(res->status != NFS_OK)
		    {
			clnt_abort(clnt);

			W(DBF_ALWAYS, "send_(m)write failed:%s", StrNFSErr(res->status));
			*Res2 = NFSErr2IoErrCont(res->status, 'w');
			return -1;
		    }
		    FileLen = max(res->attrstat_u.attributes.size, FileLen);
		    numpending--;
		}
	    }
#endif
	}
	if(count)
	{
	    int ok;
	    LONG Len;
	    
	    wargs.offset = offset;
	    if(count < MaxWrite)
		Len = count;
	    else
		Len = MaxWrite;
	    wargs.data.data_len = Len;
	    wargs.data.data_val = Buf;

	    D(DBF_ALWAYS, "\tsend now");
	    ok = nfsproc_send_writesb_2(&wargs, clnt);
	    if(!ok)
	    {
		CLIENTERROR(clnt);
		return -1;
	    }
	    numpending++;
	    Buf += Len;
	    offset += Len;
	    count -= Len;
	}
    }

    *FileLenP = FileLen;

    return (LONG) arg_count; /* FIXME ? */
}

diropokres
*nfs_Create(CLIENT *clnt, nfs_fh *dirfh, 
	    UBYTE *name, ULONG UID, ULONG GID,
	    ULONG Mode, ULONG Size,
	    LONG *Res2)
{
    diropres *dres;
    createargs cargs;
    sattr *a;

    cargs.where.dir = *dirfh;
    cargs.where.name = name;

    a = &cargs.attributes;

    a->mode = Mode;
    a->uid = UID;
    a->gid = GID;
    a->size = Size;
    /* FIXME */
    a->atime.seconds = -1;
    a->atime.useconds = -1;
    a->mtime.seconds = -1;
    a->mtime.useconds = -1;

    dres = nfsproc_create_2(&cargs, clnt);
    if (dres == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if(dres->status != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_create failed: %s", StrNFSErr(dres->status));

	*Res2 = NFSErr2IoErrCont(dres->status, 'c');
	return NULL;
    }
    return &dres->diropres_u.diropres;
}

/* TRUE on success */

LONG
nfs_Remove(CLIENT *clnt, nfs_fh *dirfh, UBYTE *name, LONG *Res2)
{
    nfsstat *res;
    diropargs dop;

    dop.dir = *dirfh;
    dop.name = name;

#ifdef DEBUG
    D(DBF_ALWAYS,"\tnfs_Remove(\"%s\" )", name);
    FHDEBUG(dirfh);
#endif

    res = nfsproc_remove_2(&dop, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return 0;
    }

    if(*res != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_remove failed: %s", StrNFSErr(*res));

	*Res2 = NFSErr2IoErrCont(*res, 'w');
	return 0;
    }
    return 1;
}

LONG
nfs_Rename(CLIENT *clnt, 
	   nfs_fh *dirfh1, UBYTE *name1, 
	   nfs_fh *dirfh2, UBYTE *name2,
	   LONG *Res2)
{
    nfsstat *res;
    renameargs rena;

    rena.from.dir = *dirfh1;
    rena.from.name = name1;
    rena.to.dir = *dirfh2;
    rena.to.name = name2;

#ifdef DEBUG
    D(DBF_ALWAYS,"\tnfs_Rename(\"%s\",\"%s\"", name1, name2);
    FHDEBUG(dirfh1);
    FHDEBUG(dirfh2);
#endif

    res = nfsproc_rename_2(&rena, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return 0;
    }

    if(*res != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_rename failed: %s", StrNFSErr(*res));

	*Res2 = NFSErr2IoErrCont(*res, 'w');
	return 0;
    }
    return 1;
}

LONG
nfs_Link(CLIENT *clnt, 
	 nfs_fh *fromfh,
	 nfs_fh *dirfh, 
	 UBYTE *Name,
	 LONG *Res2)
{
    nfsstat *res;
    linkargs sargs;

    sargs.from = *fromfh;
    sargs.to.dir = *dirfh;
    sargs.to.name = Name;

    res = nfsproc_link_2(&sargs, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return 0;
    }
    if(*res != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_link failed: %s", StrNFSErr(*res));

	*Res2 = NFSErr2IoErrCont(*res, 'w');
	return 0;
    }
    return 1;
}

LONG
nfs_Symlink(CLIENT *clnt,
	    nfs_fh *dirfh, 
	    UBYTE *Name, 
	    ULONG UID,
	    ULONG GID,
	    UBYTE *Path,
	    LONG *Res2)
{
    nfsstat *res;
    symlinkargs sargs;
    sattr *a;

    sargs.from.dir = *dirfh;
    sargs.from.name = Name;

    sargs.to = Path;

    a = &sargs.attributes;

    a->mode = 0777;
    a->uid = UID;
    a->gid = GID;
    a->size = -1;

    a->atime.seconds = -1;
    a->atime.useconds = -1;
    a->mtime.seconds = -1;
    a->mtime.useconds = -1;

    res = nfsproc_symlink_2(&sargs, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return 0;
    }
    if(*res != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_symlink failed: %s", StrNFSErr(*res));

	*Res2 = NFSErr2IoErrCont(*res, 'w');
	return 0;
    }
    return 1;
}

diropokres
*nfs_MkDir(CLIENT *clnt, nfs_fh *dirfh, UBYTE *name, 
	   ULONG UID, ULONG GID, u_int Mode, LONG *Res2)
{
    diropres *res;
    createargs cargs;
    sattr *a;

    cargs.where.dir = *dirfh;
    cargs.where.name = name;

    a = &cargs.attributes;

    a->mode = Mode;
    a->uid = UID;
    a->gid = GID; 
    a->size = -1;

    a->atime.seconds = -1;
    a->atime.useconds = -1;
    a->mtime.seconds = -1;
    a->mtime.useconds = -1;

#ifdef DEBUG
    D(DBF_ALWAYS,"\tnfs_MkDir(\"%s\" )", name);
    FHDEBUG(dirfh);
#endif

    res = nfsproc_mkdir_2(&cargs, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_mkdir failed: %s", StrNFSErr(res->status));

	*Res2 = NFSErr2IoErrCont(res->status, 'c');
	return NULL;
    }
    return &res->diropres_u.diropres;
}

LONG
nfs_RmDir(CLIENT *clnt, nfs_fh *dirfh, UBYTE *name, LONG *Res2)
{
    nfsstat *res;
    diropargs dop;

    dop.dir = *dirfh;
    dop.name = name;

#ifdef DEBUG
    D(DBF_ALWAYS,"\tnfs_RmDir(\"%s\")", name);
    FHDEBUG(dirfh);
#endif

    res = nfsproc_rmdir_2(&dop, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return 0;
    }

    if(*res != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_rmdir failed: %s", StrNFSErr(*res));

	*Res2 = NFSErr2IoErrCont(*res, 'w');
	return 0;
    }
    return 1;
}

entry *
nfs_ReadDir(CLIENT *clnt, 
	    const nfs_fh *dir, 
	    nfscookie cookie, 
	    ULONG BufSize,
	    LONG *Eof,
	    LONG *Res2)
{
    readdirargs args;
    readdirres *res;

#ifdef DEBUG
#if 0
    entry *ent;
#endif
    D(DBF_ALWAYS,"\tnfs_ReadDir()");
    FHDEBUG(dir);
#endif

    memset(&args, 0, sizeof(args));
    CopyFH(dir, &args.dir);

    args.count = BufSize;
    CopyCookie(cookie, args.cookie);

    res = nfsproc_readdir_2(&args, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if(res->status != NFS_OK)
    {
	W(DBF_ALWAYS, "nfs_readdir failed: %s", StrNFSErr(res->status));

	*Res2 = NFSErr2IoErrCont(res->status, 'r');
	return NULL;
    }
    /* entries may be present when Eof is true */
    *Eof = res->readdirres_u.reply.eof;
    if(res->readdirres_u.reply.eof)
    {
	D(DBF_ALWAYS,"\t\tEOF reached");
    }
#if 0
    ent = res->readdirres_u.reply.entries;
    while(ent)
    {
	D(DBF_ALWAYS,"\t\tent: %s (Id: 0x%08lx)", ent->name, ent->fileid);
	ent = ent->nextentry;
    }
#endif
    return res->readdirres_u.reply.entries;
}

statfsokres *
nfs_statfs(CLIENT *clnt, nfs_fh *dir, LONG *Res2)
{
    statfsres *res;

    res = nfsproc_statfs_2(dir, clnt);
    if (res == NULL) 
    {
	CLIENTERROR(clnt);
	return NULL;
    }
    if(res->status != NFS_OK)
    {
	E(DBF_ALWAYS, "statfs failed:%s", StrNFSErr(res->status));
	*Res2 = NFSErr2IoErrCont(res->status, 'r');
	return NULL;
    }
    return &res->statfsres_u.reply;
}

/* helper functions */

fattr *
nfs_Chmod(CLIENT *clnt,
	  nfs_fh *filefh, 
	  ULONG Mode,
		 LONG *Res2)
{
    return nfs_SetAttr(clnt, filefh, Mode, -1, -1, -1, NULL, Res2);
}

fattr *
nfs_SetSize(CLIENT *clnt, nfs_fh *filefh, LONG Size, LONG *Res2)
{
    return nfs_SetAttr(clnt, filefh, -1, -1, -1, Size, NULL, Res2);
}

fattr *
nfs_SetTimes(CLIENT *clnt, nfs_fh *filefh, nfstime *tv, LONG *Res2)
{
    return nfs_SetAttr(clnt, filefh, -1, -1, -1, -1, tv, Res2);
}

fattr *
nfs_SetUidGid(CLIENT *clnt, nfs_fh *filefh, u_int uid, u_int gid, LONG *Res2)
{
    return nfs_SetAttr(clnt, filefh, -1, uid, gid, -1, NULL, Res2);
}

