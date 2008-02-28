#ifndef _CRED_H
#define _CRED_H

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
 * Credentials database.
 */

#include <exec/tasks.h>

#define CR_NOBODY_UID	(-1)
#define CR_NOBODY_GID	(-1)

#define CR_DEFAULT_UMASK 022

#define CRF_USER  0x01 /* User is set */
#define CRF_UMASK 0x02 /* UMask is set */

typedef struct _Cred {
    ULONG cr_Flags;
    /* user id/group(s) user belongs to, set if CRF_USER is set
     hold\'s cached values otherwise */
    char * cr_UserName;
    u_int cr_UID;/* remote user id, used for nfs
				   rpc authorization and calls */
    u_int cr_GID;		/* remote group id, used for nfs
				   rpc authorization and calls */
    u_int cr_GIDS[NGROUPS];
    u_int cr_NGroups;
    /* set if CRF_UMASK is set, hold\'s cached values otherwise */
    u_int cr_UMask;		/* used for nfs rpc calls */
    /* cached auth value */
    AUTH *cr_Auth;
    /* which task cached values belong to, NULL if unset */
    struct Task *cr_ClientCache;
    struct Task *cr_ClientCacheUMask;
} CRED;

#define cr_GetAuth(cr) ((cr)->cr_Auth)
#define cr_GetUMask(cr) ((cr)->cr_UMask)
#define cr_GetUID(cr) ((cr)->cr_UID)
#define cr_GetGID(cr) ((cr)->cr_GID)
#endif
