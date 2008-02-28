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

#ifdef NFS_INLINE
static __inline void CopyFH(const nfs_fh *s, nfs_fh *d)
{
    memcpy(d, s, FHSIZE);
}
static __inline LONG IsSameFH(const nfs_fh *s, const nfs_fh *d)
{
    return !memcmp(d,s, FHSIZE);
}

static void __inline CopyCookie(nfscookie a, nfscookie b)
{
    memcpy(b, a, NFS_COOKIESIZE);
}
#else
#define CopyFH(from,to) CopyMem((UBYTE *) from, to, FHSIZE)
LONG IsSameFH(const nfs_fh *s, const nfs_fh *d);
#define CopyCookie(from,to) CopyMem((UBYTE *) from, to, NFS_COOKIESIZE)
#endif

