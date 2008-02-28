#ifndef WBUF_H
#define WBUF_H

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

#define WBUFHDRINFOSIZE 59

typedef struct wbufhdrinfo
{
    WORD wbi_From;		/* inclusive start boundary */
    WORD wbi_To;		/* inclusive end boundary */
} WBUFHDRINFO;
    
typedef struct _wbufhdr
{
    struct _wbufhdr *wbh_Next;
    
    LONG wbh_FileOffs;		/* Offset of data inside file
				   (this is the offset of the data buffer, not
				   the data block !)*/
    LONG wbh_NumInfo;		/* Number of filled info cells */
    UBYTE *wbh_Data;		/* pointer to real data buffer */
    WBUFHDRINFO wbh_Info[WBUFHDRINFOSIZE];
} WBUFHDR;

typedef struct _wbufcluster
{
    LONG wbc_WBufsMax;
    LONG wbc_WBufsUsed;
    WBUFHDR *wbc_FirstData;	/* points to first buffer header */
    WBUFHDR *wbc_LastData;	/* points to last buffer header
				   (if first is valid) */
    WBUFHDR *wbc_PosData;
    LONG wbc_FilePos;
} WBUFCLUSTER;
#endif
