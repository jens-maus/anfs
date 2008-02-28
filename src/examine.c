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

#define USE_BUILTIN_MATH /* for min from string.h */

#include "nfs_handler.h"
#include "protos.h"
#include "inline.h"

#include "xexall.h"

/* amiga <-> unix conversion stuff */

//#include <aulib/aulib.h>
//#include <proto/aulib.h>
#include <proto/utility.h>

#include "chdebug.h"

ULONG
NFSMode2Protection(unsigned int mode)
{
    ULONG amode=0;
    
    mode &= 0777;
    if(!(mode & 0100)) amode |= FIBF_EXECUTE;
    if(!(mode & 0200)) amode |= (FIBF_WRITE | FIBF_DELETE);
    if(!(mode & 0400)) amode |= FIBF_READ;
    if(mode &  010)	amode |= FIBF_GRP_EXECUTE;
    if(mode &  020)     amode |= (FIBF_GRP_WRITE | FIBF_GRP_DELETE);
    if(mode &  040)   	amode |= FIBF_GRP_READ;
    if(mode & 01)   	amode |= FIBF_OTR_EXECUTE;
    if(mode & 02)   	amode |= (FIBF_OTR_WRITE | FIBF_OTR_DELETE);
    if(mode & 04)   	amode |= FIBF_OTR_READ;

    return amode;
}

LONG
NFSType2EntryType(ftype type)
{
    LONG det;

    switch(type)
    {
 case NFDIR: det = ST_USERDIR; break;
 case NFREG: det = ST_FILE; break;
 case NFLNK: det = ST_SOFTLINK; break;
 case NFCHR: det = ST_CDEVICE; break;
 case NFBLK: det = ST_BDEVICE; break;
 case NFSOCK:det = ST_SOCKET; break;
 case NFFIFO: det = ST_FIFO; break;
 case NFBAD:
 case NFNON:
default:
	det = 0; break;
    }
    
    return det;
}

void timeval2DateStamp(nfstime *tv, struct DateStamp *ds)
{
    aunet_timeval2DateStamp((struct timeval *)tv, ds);
}

void DateStamp2timeval(struct DateStamp *ds, nfstime *tv)
{
    aunet_DateStamp2timeval(ds, (struct timeval *) tv);
}

void FAttr2FIB(fattr *a, UBYTE *Name, LONG NameLen, LONG MaxFileNameLen, FIB *fib)
{
    fib->fib_DiskKey = a->fileid;

    fib->fib_Protection = NFSMode2Protection(a->mode);
    fib->fib_EntryType = NFSType2EntryType(a->type);
    fib->fib_DirEntryType = fib->fib_EntryType;

    if(NameLen == -1)
	NameLen = strlen(Name);

    if(NameLen > MaxFileNameLen)
	NameLen = MaxFileNameLen;

    CopyMem(Name, 
	    &fib->fib_FileName[1], 
	    NameLen);

    fib->fib_FileName[0] = NameLen;
    fib->fib_FileName[NameLen+1] = 0;
    fib->fib_Size = a->size;
    fib->fib_NumBlocks = a->blocks;
    timeval2DateStamp(&a->mtime, &fib->fib_Date);
    fib->fib_Comment[0] = '\0';
    fib->fib_OwnerUID = nfsuid2UID(a->uid);
    fib->fib_OwnerGID = nfsgid2GID(a->gid);
}

#ifdef DEBUG
void
PrintFIB(FIB *fib)
{
    AKDEBUG((0,"\t%08lx/%08lx/%08lx/%08lx/%08lx/%08lx\n",
	     fib->fib_DiskKey, fib->fib_DirEntryType,
	     fib->fib_Protection, fib->fib_EntryType,
	     fib->fib_Size, fib->fib_NumBlocks));
    AKDEBUG((0,"\t%04lx/%04lx/%s\n",
	     (ULONG) fib->fib_OwnerUID,
	     (ULONG) fib->fib_OwnerGID,
	     &fib->fib_FileName[1]));
}
#endif

LONG
do_act_EXAMINE_OBJECT(Global_T *g, ELOCK *efl, FIB *fib)
{
    ACEntry *ACEnt;
    LONG Res1 = DOSFALSE;
    LONG Res2 = g->g_Res2;

    if(efl)
    {
	AKDEBUG((0,"\tname = \"%s\"\n", efl->efl_FullName));

	/* get entry from cache if possible, else do nfs_Lookup */
	ACEnt = c_GetAttr(g,
			  efl->efl_Lock.fl_Key, &efl->efl_NFSFh, 
			  &Res2);
	if(ACEnt)
	{
	    if(efl->efl_NameLen == 0)
	    {
		 /* root lock */
		UBYTE *BName = (UBYTE *) BADDR(g->g_Dev->dl_Name);

		FAttr2FIB(&ACEnt->ace_FAttr, &BName[1], BName[0], g->g_MaxFileNameLen, fib);
	    }
	    else
	    {
		FAttr2FIB(&ACEnt->ace_FAttr, 
			  efl->efl_Name, efl->efl_NameLen, g->g_MaxFileNameLen, fib);
	    }
#if 0
	    AKDEBUG((0,"\tDirEntryType = %ld\n", fib->fib_DirEntryType));
#endif
	    Res1 = DOSTRUE;
	}
	else
	{
	    /* Res2 = Res2; */
	}
    }
    else
	Res2 = ERROR_INVALID_LOCK;

#ifdef DEBUG
    if(Res1 != DOSFALSE)
    {
	PrintFIB(fib);
    }
#endif
    return SetRes(g, Res1, Res2);
}

LONG
do_ExamineRoot(Global_T *g, FIB *fib)
{
    ACEntry *ACEnt;
    LONG Res2 = g->g_Res2;
    UBYTE *BName;

    BName = (UBYTE *) BADDR(g->g_Dev->dl_Name);

    /* get entry from cache if possible, else do nfs_Lookup */
    ACEnt = c_GetAttr(g, g->g_MntAttr.fileid, &g->g_MntFh, &Res2);
    if(ACEnt)
    {
	FAttr2FIB(&ACEnt->ace_FAttr, &BName[1], BName[0], g->g_MaxFileNameLen, fib);
    }
    else
    {
	/* fall back to mount values */
	FAttr2FIB(&g->g_MntAttr, &BName[1], BName[0], g->g_MaxFileNameLen, fib);
    }

#ifdef DEBUG
    PrintFIB(fib);
#endif
    return SetRes1(g, DOSTRUE);
}

LONG
act_EXAMINE_OBJECT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK*) pkt->dp_Arg1;
    FIB *fib = (FIB*) pkt->dp_Arg2;

    if(fl)
	return do_act_EXAMINE_OBJECT(g, lock_Lookup(g, fl), fib);
    else
	return do_ExamineRoot(g, fib);
}

fattr *
LookupAttr(Global_T *g, 
	   ELOCK *efl, /* Lock of directory */
	   u_int FileId, UBYTE *Name, 
	   LONG *Res2)
{
    DCEntry *DCEnt;
    ACEntry *ACEnt = NULL;
    NEntry *NEnt=NULL;

    if(!Name)
    {
	FIXME(("LookupAttr: Internal problem: Name = NULL\n"));
	
	*Res2 = ERROR_NO_FREE_STORE;

	return NULL;
    }

    /* 1. try attribute cache */
    ACEnt = attrs_LookupFromFileId(g, FileId);
    if(ACEnt)
	return &ACEnt->ace_FAttr;

    AKDEBUG((0,"\tLookupAttr: attr cache, no entry\n"));
    
    /* 2. need to lookup */

    DCEnt = dc_DCELookupFromFileId(g, efl->efl_Lock.fl_Key);
    if(!DCEnt)
    {
	AKDEBUG((0,"\tLookupAttr: dce cache, no entry\n"));
	DCEnt = GetDirEntry(g,
			    NULL,
			    efl->efl_Lock.fl_Key,
			    &efl->efl_NFSFh,
			    Name,
			    Res2);
	if(!DCEnt)
	    return NULL;
    }
    else
    {
	AKDEBUG((0,"\tLookupAttr: dce cache ok\n"));
    }
    if(!NEnt)
    {
	NEnt = GetNameEntry(g, DCEnt, Name, &ACEnt, Res2);
	if(!NEnt)
	    return NULL;
    }
    /* FIXME: crosscheck fileid here? -> invalidate cache entry if != */
    return &ACEnt->ace_FAttr;
}

LONG
act_EXAMINE_NEXT(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK*) pkt->dp_Arg1;
    FIB *fib = (FIB*) pkt->dp_Arg2;

    fattr *Attr;
    ELOCK *efl;
    EDH *edh; /* dir handle */
    entry *Entries;
    LONG Res1 = DOSFALSE;
    LONG Res2 = 0;

    efl = lock_Lookup(g, fl);
    if(efl)
    {
#if 1
	AKDEBUG((0,"Key = 0x%08lx, DiskKey = 0x%08lx, Name = %s\n",
		 efl->efl_Lock.fl_Key,
		 fib->fib_DiskKey,
		 fib->fib_FileName+1));
#endif
	edh = dir_LookupFromKeyAndCookieAndCBSTR(g, 
						 efl->efl_Lock.fl_Key,
						 fib->fib_DiskKey,
						 fib->fib_FileName);
	if(!edh) /* no dir handle, possible first call to ExNext() */
	{
	    nfscookie NullCookie;

	    /* if we don\'t find an edh the FIB should have been
	       filled by an previous Examine() so it should have the same
	       Key as the accompanied lock */

	    /*
	      2.04 FFS does return this on illegal FIBs,
	      o.k. so we do */
	       
	    if(efl->efl_Lock.fl_Key != fib->fib_DiskKey)
		return SetRes(g, DOSFALSE, ERROR_NO_MORE_ENTRIES);

	    /* special cookie 0, get first entry */
	    memset(NullCookie, 0, sizeof(NullCookie));

	    edh = dir_Create(g,
			     efl->efl_Lock.fl_Key,
			     fib->fib_DiskKey, 
			     &efl->efl_NFSFh,
			     NullCookie,
			     fib->fib_FileName,
			     g->g_MaxFileNameLen);
	    AKDEBUG((0,"\tassuming first call to ExNext(), new handle 0x%08lx\n",
		     edh));
	    if(!edh)
		return SetRes(g, DOSFALSE, ERROR_NO_FREE_STORE);
	    dir_Insert(g, edh);
	}
	else
	{
	    AKDEBUG((0,"\tfound old dir handle: 0x%08lx\n", edh));
	}

	if(IsSameFH(&efl->efl_NFSFh, &edh->edh_NFSDirFh))
	{
	    if(!edh->edh_NFSEntries)
	    {
		if(edh->edh_Eof) /* End Of List reached */
		{
		    edh = dir_Remove(g, edh);
		    dir_Delete(&edh);
		    
		    return SetRes(g, DOSFALSE, ERROR_NO_MORE_ENTRIES);
		}
	nextdir:
		Entries = nfs_ReadDir(g->g_NFSClnt, 
				      &edh->edh_NFSDirFh,
				      edh->edh_NFSCookie,
				      g->g_NFSGlobal.ng_MaxReadDirSize,
				      &edh->edh_Eof,
				      &Res2);
		/* we must skip "." and ".." entries, otherwise
		   amigados dir scanning utils would become very confused
		   and may loop endless

		   FIXME: for now we also skip file entries exceeding the
		   maximal filename length
		*/
		
		if(Entries)
		{
		    chassert(edh);
		    while(Entries && ((strcmp(Entries->name , ".") == 0) ||
				      (strcmp(Entries->name , "..") == 0) ||
				      (strlen(Entries->name) > g->g_MaxFileNameLen)))
		    {
			CopyCookie(Entries->cookie, edh->edh_NFSCookie);
			Entries = Entries->nextentry;
		    }
		    if(!Entries && !edh->edh_Eof)
			goto nextdir;
		}

		if(Entries)
		{
		    Entries = dir_DupEntries(Entries);
		    if(!Entries)
			Res2 = ERROR_NO_FREE_STORE;
		}
		if(Entries == NULL)
		{
		    /* Error or EOF reached, Res2 is set by ReadDir on Error*/
		    if(edh->edh_Eof)
		    {
			Res2 = ERROR_NO_MORE_ENTRIES;
		    }
		    edh = dir_Remove(g, edh);
		    dir_Delete(&edh);
		    
		    if(Res2 > 1000)
			Res2 = ERROR_NO_MORE_ENTRIES;

		    return SetRes(g, DOSFALSE, Res2);
		}
		edh->edh_NFSEntries = Entries;
	    }

	    Entries = dir_RemHeadEntry(edh);
	    chassert(Entries);

	    /* FIXME: could check for fileid identity ? */
	    Attr = LookupAttr(g, efl, 
			      Entries->fileid, Entries->name,
			      &Res2);
	    if(Attr)
	    {
		int l;
		
		FAttr2FIB(Attr, Entries->name, -1, g->g_MaxFileNameLen, fib);
		edh->edh_Cookie = Attr->fileid;
		CopyCookie( Entries->cookie, edh->edh_NFSCookie);
		/* \0 terminated BSTR */
		l = min(fib->fib_FileName[0]+2, EDH_MAX_FILENAME_SIZE-1);
		if(l)
		    memcpy(edh->edh_Name, fib->fib_FileName, l);
		else
		    edh->edh_Name[1] = 0;
		/* make sure it\'s \0 terminated */
		edh->edh_Name[l] = 0;

		Res1 = DOSTRUE;
	    }
	    else
	    {
		/* Res2 = Res2; */
	    }

	    dir_DelEntry(&Entries);
	} /* IsSameFH */
	else
	{
	    AKDEBUG((1,"\tdifferent file handles to examine next!\n"));
	    Res2 = ERROR_OBJECT_WRONG_TYPE;
	}
    } /* if (efl) */
    else
    {
	AKDEBUG((1,"\tillegal lock: 0x%08lx\n", fl));
	SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);
    }

#if 1
#ifdef DEBUG
    if(Res1 != DOSFALSE)
    {
	AKDEBUG((0,"Key = 0x%08lx\n", efl->efl_Lock.fl_Key));
	PrintFIB(fib);
    }
#endif
#endif

    return SetRes(g, Res1, Res2);
}

LONG
act_GET_ATTRIBUTES(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK*) pkt->dp_Arg2;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg3;
    struct TagItem *TagList = (struct TagItem *) pkt->dp_Arg4;

    UBYTE *Name;
    LONG Res1 = DOSFALSE, Res2 = g->g_Res2;

    DCEntry *ParentDir;
    NEntry *NEnt;
    ACEntry *ACEnt;

    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0, /* not C-String*/
			       &Name, NULL,
			       NULL, NULL,
			       &Res2);
    if(!ParentDir)
	goto exit;

    
    NEnt = GetNameEntry(g, ParentDir, Name, &ACEnt, &Res2);
    if(NEnt)
    {
	fattr *attr;
	struct TagItem *ActTag, *TagListState;
	struct DateStamp *ds;
	LONG NumProcessed = 0;
	
	chassert(ACEnt);
	attr = &ACEnt->ace_FAttr;
	    
	TagListState = TagList;
	while(ActTag = NextTagItem(&TagListState))
	{
	    ULONG Data = ActTag->ti_Data;
	    NumProcessed++;
	    
	    switch(ActTag->ti_Tag)
	    {
	case DSA_MODIFY_TIME:
		ds = (struct DateStamp *) Data;
		if(ds)
		    timeval2DateStamp(&attr->mtime, ds);
		break;
	case DSA_PROTECTION:
		ActTag->ti_Data = NFSMode2Protection(attr->mode);
		break;
	case DSA_OWNER_UID:
		ActTag->ti_Data = attr->uid;
		break;
	case DSA_OWNER_GID:
		ActTag->ti_Data = attr->gid;
		break;
	case DSA_CREATE_TIME:
		ds = (struct DateStamp *) Data;
		if(ds)
		    timeval2DateStamp(&attr->ctime, ds);
		break;
	case DSA_ACCESS_TIME:
		ds = (struct DateStamp *) Data;
		if(ds)
		    timeval2DateStamp(&attr->atime, ds);
		break;
		/* case DSA_COMMENT: */
	default:
		NumProcessed--;
		ActTag->ti_Tag = TAG_IGNORE;
		break;
	    }
	}
	Res1 = NumProcessed;
	Res2 = 0;
    }

    fn_Delete(&Name);

 exit:    
    return SetRes(g, Res1, Res2);
}

typedef LONG __asm (*MatchFunc)(register __a0 struct Hook *hook,
				register __a1 struct ExAllData *ead,
				register __a2 ULONG *ptype);

LONG
GetNextEntries(Global_T *g, EDH *edh, LONG *Res2)
{
    entry *Entries;
    
    if(edh->edh_Eof) /* End Of List reached */
    {
	*Res2 = ERROR_NO_MORE_ENTRIES;
	
	edh = dir_Remove(g, edh);
	dir_Delete(&edh);

	return 0;
    }
 nextdir:
    Entries = nfs_ReadDir(g->g_NFSClnt, 
			  &edh->edh_NFSDirFh,
			  edh->edh_NFSCookie,
			  g->g_NFSGlobal.ng_MaxReadDirSize,
			  &edh->edh_Eof,
			  Res2);
    /* we must skip "." and ".." entries, otherwise
       amigados dir scanning utils would become very confused
       and may loop endless

       FIXME: for now we also skip file entries exceeding the
       maximal filename length
    */
    
    if(Entries)
    {
	chassert(edh);
	while(Entries && ((strcmp(Entries->name , ".") == 0) ||
			  (strcmp(Entries->name , "..") == 0) ||
			  (strlen(Entries->name) > g->g_MaxFileNameLen)))
	{
	    CopyCookie(Entries->cookie, edh->edh_NFSCookie);
	    Entries = Entries->nextentry;
	}
	if(!Entries && !edh->edh_Eof)
	    goto nextdir;
    }
    
    if(Entries)
    {
	Entries = dir_DupEntries(Entries);
	if(!Entries)
	{
	    edh = dir_Remove(g, edh);
	    dir_Delete(&edh);
	    
	    *Res2 = ERROR_NO_FREE_STORE;
	    
	    return 0;
	}
    }
    if(Entries == NULL)
    {
	/* Error or EOF reached, Res2 is set by ReadDir on Error*/
	if(edh->edh_Eof)
	{
	    *Res2 = ERROR_NO_MORE_ENTRIES;
	}
	edh = dir_Remove(g, edh);
	dir_Delete(&edh);

	if(*Res2 > 1000)
	    *Res2 = ERROR_NO_MORE_ENTRIES;
	return 0;
    }
    edh->edh_NFSEntries = Entries;

    return 1;
}

/* return number of bytes of fixed part of ExAllData */

LONG
GetFixedSize(LONG Type)
{
    LONG l;

    switch(Type)
    {
 case ED_NAME: l = offsetof(struct ExAllData, ed_Type); break;
 case ED_TYPE: l = offsetof(struct ExAllData, ed_Size); break;
 case ED_SIZE: l = offsetof(struct ExAllData, ed_Prot); break;
 case ED_PROTECTION: l = offsetof(struct ExAllData, ed_Days); break;
 case ED_DATE: l = offsetof(struct ExAllData, ed_Comment); break;
 case ED_COMMENT:
	l = offsetof(struct ExAllData, ed_OwnerUID);
	break;
 case ED_OWNER:
default: l = offsetof(struct ExAllData, ed_OwnerUID) + sizeof(UWORD);
	break;
    }
    return l;
}

LONG
PutExAllData(UBYTE *Buf, LONG Size, LONG Type, LONG FixedSize,
	     LONG MaxFileNameLen, 
	      entry *anEntry, fattr *fAttr)
{
    LONG realLen, nameLen, sizeNeeded;
    struct ExAllData *ed = (struct ExAllData *) Buf;

    /* limit to maximum a FIB may return */
    realLen = min(strlen(anEntry->name), MaxFileNameLen);
    /* one byte \0, make longword aligned */
    nameLen = (realLen + 1 + 3) & 0xFFFFFFFC;

    sizeNeeded = FixedSize + nameLen;

    if(Size >= sizeNeeded)
    {
	ed->ed_Next = NULL;
	switch(Type)
	{
    default:
    case ED_OWNER:
	    ed -> ed_OwnerUID = nfsuid2UID(fAttr->uid);
	    ed -> ed_OwnerGID = nfsgid2GID(fAttr->gid);
    case ED_COMMENT: ed -> ed_Comment = NULL;
    case ED_DATE: timeval2DateStamp(&fAttr->mtime,
				    (struct DateStamp *) &ed->ed_Days);
    case ED_PROTECTION: ed->ed_Prot = NFSMode2Protection(fAttr->mode);
    case ED_SIZE: ed->ed_Size = fAttr->size;
    case ED_TYPE: ed->ed_Type = NFSType2EntryType(fAttr->type);
    case ED_NAME: ed->ed_Name = Buf + FixedSize;
	    CopyMem(anEntry->name, ed->ed_Name, realLen);
	    ed->ed_Name[realLen] = 0; /* make sure it\'s 0 terminated */
	    break;
	}
    }
    else
	sizeNeeded = -1;	/* not enough space */
	
    return sizeNeeded;
}

LONG
act_EXAMINE_ALL(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK*) pkt->dp_Arg1;
    UBYTE *Buf = (UBYTE *) pkt->dp_Arg2;
    ULONG Size = (ULONG) pkt->dp_Arg3;
    ULONG Type = (ULONG) pkt->dp_Arg4;
    struct ExAllControl *eac = (struct ExAllControl *) pkt->dp_Arg5;

    struct ExAllData *lastEntry = NULL;
    ULONG NumEntries = 0;
    fattr *Attr;
    ELOCK *efl;
    EDH *edh; /* dir handle */
    entry *anEntry;
    LONG Res1 = DOSFALSE;
    LONG Res2 = 0;

    if((Type < ED_NAME) || (Type > ED_OWNER) || (Size < 8))
	return SetRes(g, DOSFALSE, ERROR_BAD_NUMBER);

    eac->eac_Entries = 0;
    ((struct ExAllData *) Buf)->ed_Next = NULL;

    efl = lock_Lookup(g, fl);
    if(efl)
    {
	if(eac->eac_LastKey == NULL) /* first call */
	{
	    nfscookie NullCookie;
	    /* special cookie 0, get first entry */
	    memset(NullCookie, 0, sizeof(NullCookie));

	    edh = dir_Create(g,
			     efl->efl_Lock.fl_Key,
			     efl->efl_Lock.fl_Key,
			     &efl->efl_NFSFh,
			     NullCookie,
			     NULL,
			     g->g_MaxFileNameLen); /* NULL means empty string "" */
	    if(edh)
	    {
		dir_Insert(g, edh);
		eac->eac_LastKey = (ULONG) edh;
		AKDEBUG((0,"new LastKey == 0x%08lx\n",
			 eac->eac_LastKey));
	    }
	    else
		Res2 = ERROR_NO_FREE_STORE;
	}
	else
	{
	    edh = dir_LookupFromAddress(g, (void *) eac->eac_LastKey);
	    if(!edh) /* illegal call, just return NO_MORE_ENTRIES */
	    {
		AKDEBUG((1,"illegal call to ExAll(), LastKey == 0x%08lx\n",
			 eac->eac_LastKey));
		Res2 = ERROR_NO_MORE_ENTRIES;
	    }
	}
	if(edh)
	{
	    LONG doContinue = 1;
	    LONG FixedSize;
	    struct Hook *matchFunc = eac->eac_MatchFunc;
	    UBYTE *matchString = eac->eac_MatchString;

	    Res1 = DOSTRUE;
	    FixedSize = GetFixedSize(Type);
	    while(doContinue)
	    {
		if(!edh->edh_NFSEntries)
		{
		    if(!GetNextEntries(g, edh, &Res2))
		    {
			AKDEBUG((0,"\tGetNextEntries is FALSE\n"));
			Res1 = DOSFALSE;
			doContinue = 0;
		    }
		}
		while(doContinue && (anEntry = dir_GetHeadEntry(edh)))
		{
		    LONG entrySize = 0;
		    UBYTE *Name = anEntry->name;
		    LONG doMatch = 1;
		    LONG doRemHead = 1;

		    if(matchString && !MatchPatternNoCase(matchString, Name))
			doMatch = 0;

		    if(doMatch)
		    {
			if(Type > ED_NAME)
			{
			    Attr = LookupAttr(g, efl, 
					      anEntry->fileid, Name,
					      &Res2);
			    if(!Attr && (Res2 == ERROR_NO_FREE_STORE))
			    {
				/* NO_FREE_STORE may be a cache overflow,
				   so this may help */

				/* we don\'t have any pointers to cached
				   values, so these may be legally called */
				attrs_UnLockAll(g);
				dc_UnLockAll(g);
				Attr = LookupAttr(g, efl, 
						  anEntry->fileid, Name,
						  &Res2);
			    }
			}
			else
			    Attr = NULL;
			if((Type == ED_NAME) || Attr)
			{
			    entrySize = PutExAllData(Buf, Size, Type,
						     FixedSize, g->g_MaxFileNameLen,
						     anEntry, Attr);
			    if(entrySize < 0) /* buffer full */
			    {
				Res1 = DOSTRUE; Res2 = 0;
				doContinue = 0;
				doMatch = 0;
				doRemHead = 0;
			    }
			}
			else
			{
			    /* failure, Res2 is set, cleanup and exit */

			    AKDEBUG((0,"\tAttr == NULL, Res2 = %ld\n",
				     Res2));
			    Res1 = DOSFALSE;
			    edh = dir_Remove(g, edh);
			    dir_Delete(&edh);
			    
			    doContinue = 0;
			    doMatch = 0;
			    doRemHead = 0;
			}
		    }
		    if(doMatch && matchFunc
		       && !CallHookPkt(matchFunc, &Type, Buf))
			doMatch = 0;
		    if(doMatch)
		    {
			if(lastEntry)
			    lastEntry->ed_Next = (struct ExAllData *) Buf;
			lastEntry = (struct ExAllData *) Buf;
			Size -= entrySize;
			Buf += entrySize;
			NumEntries++;
		    }
		    if(doRemHead)
		    {
			anEntry = dir_RemHeadEntry(edh);
			CopyCookie(anEntry->cookie, edh->edh_NFSCookie);
			dir_DelEntry(&anEntry);
		    }
		}
	    }
	    eac->eac_Entries = NumEntries;
	}
    } /* if (efl) */
    else
    {
	AKDEBUG((1,"\tillegal lock: 0x%08lx\n", fl));
	SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);
    }

    return SetRes(g, Res1, Res2);
}
