#ifndef NFS_H
#define NFS_H

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
 * "Global" nfs parameters, read-only by nfs.c functions,
 * execpt statistics.
 */

struct NFSStat
{
    ULONG Calls;
    ULONG Failures;
};
   
typedef struct NFSStat NFSStat;

struct NFSStats
{
    NFSStat ns_Stats[18]; /* all defined NFS calls */
};

typedef struct NFSStats NFSStats;

struct NFSGlobal_T
{
    LONG ng_NumRRPCs;		/* number of async read rpcs */
    LONG ng_NumWRPCs;		/* number of async write rpcs */
    ULONG ng_MaxReadSize;	/* Maximal length of data portion of a read request */
    ULONG ng_MaxReadDirSize;	/* Maximal length of data portion of a readdir request */
    ULONG ng_MaxWriteSize;	/* Maximal length of data portion of a write request */
    
    NFSStats ng_Stats;		/* statistics for all NFS calls */
};
typedef struct NFSGlobal_T NFSGlobal_T;

#endif
