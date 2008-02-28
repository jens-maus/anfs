#ifndef XDOS_H
#define XDOS_H

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
 * Contains dos extensions not available in SC 6.3 header files
 */

#define ACTION_CREATE_OBJECT	2346
#define ACTION_SET_ATTRIBUTES	2347
#define ACTION_GET_ATTRIBUTES	2348

#define ST_BDEVICE      -10 /* block special device (block only access) */
#define ST_CDEVICE	    -11 /* character special device (byte accessable) */
#define ST_SOCKET       -12 /* Unix style socket */
#define ST_FIFO         -13 /* named pipe (or ST_PIPE if you want) */
#define ST_LIFO         -14 /* named stack (or ST_STACK if you want) */

/* tags for GetAttributes/SetAttributes */

#define DSA_Dummy	(TAG_USER + 10000)
#define	DSA_MODIFY_TIME	(DSA_Dummy + 1)	/* for the object modify time
				ti_Data is a struct DateStamp * */
#define	DSA_PROTECTION	(DSA_Dummy + 2)	/*  for the object protection bits
				ti_Data is a LONG of protection bits */
#define	DSA_OWNER_UID	(DSA_Dummy + 3)	/*	for the owner's user-id
				ti_Data is a LONG of owner UID (note: AmigaDOS
				currently only supports 16 bits of UID) */
#define	DSA_OWNER_GID	(DSA_Dummy + 4)	/*	for the owner's group-id
				ti_Data is a LONG of owner GID (note: AmigaDOS
				currently only supports 16 bits of GID) */
#define	DSA_COMMENT	(DSA_Dummy + 5)	/*     for the object's comment
				ti_Data is an APTR to a null-terminated comment */

/*	Currently Berkeley FFS only, but other filesystems may support them: */
#define	DSA_CREATE_TIME	(DSA_Dummy + 6)	/* for the object creation time
				ti_Data is a struct DateStamp * */
#define	DSA_ACCESS_TIME	(DSA_Dummy + 7)	/* for the object creation time
				ti_Data is a struct DateStamp * */


#if !defined(__amigaos4__)

#include "dos_dostags.h"

#define FileInfoBlock OldFileInfoBlock
#include <dos/dos.h>
#undef FileInfoBlock

#ifndef FIBF_OTR_READ
#define FIBF_OTR_READ		(1<<15)
#define FIBF_OTR_WRITE		(1<<14)
#define FIBF_OTR_EXECUTE	(1<<13)
#define FIBF_OTR_DELETE		(1<<12)
#define FIBF_GRP_READ		(1<<11)
#define FIBF_GRP_WRITE		(1<<10)
#define FIBF_GRP_EXECUTE	(1<<9)
#define FIBF_GRP_DELETE		(1<<8)
#endif

struct FileInfoBlock 
{
    LONG   fib_DiskKey;
    LONG   fib_DirEntryType;	/* Type of Directory. 
				 * If < 0, then a plain file.
				 * If > 0 a directory */
    char   fib_FileName[108];	/* Null terminated. Max 30 chars used 
				 * for now */
    LONG   fib_Protection;
    LONG   fib_EntryType;
    LONG   fib_Size;
    LONG   fib_NumBlocks;
    struct DateStamp fib_Date;
    char   fib_Comment[80];
    UWORD  fib_OwnerUID;
    UWORD  fib_OwnerGID;
    
    char   fib_Reserved[32];
};
#include <dos/dosextens.h>
#define ACTION_SET_OWNER	1036

#endif

#endif
