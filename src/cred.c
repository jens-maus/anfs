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

#include "nfs_handler.h"
#include "protos.h"

#include <grp.h>

#include <libraries/usergroup.h>
#include <proto/usergroup.h>

#include "chdebug.h"

void
cr_Init(CRED *cr)
{
    cr->cr_Flags = 0L;
    cr->cr_ClientCache = NULL;
    cr->cr_Auth = NULL;
}

static void
cr_InvalidateCache(CRED *cr)
{
    cr->cr_ClientCache = NULL;
    cr->cr_ClientCacheUMask = NULL;
}

/* flush cache */

void
cr_Flush(CRED *cr)
{
    cr_InvalidateCache(cr);
}

void
cr_CleanUp(CRED *cr)
{
    if(cr->cr_Auth)
    {
	auth_destroy(cr->cr_Auth);
	cr->cr_Auth = NULL;
	cr_InvalidateCache(cr);
    }
}

/* TRUE = success */
BOOL
cr_SetUser(CRED *cr, const char *user)
{
    BOOL RetVal = FALSE;
    struct passwd *pw;
    char *name;

    name = StrDup(user);
    if(name)
    {
	pw = getpwnam(name);
	if(pw)
	{
	    if(cr->cr_UserName)
		fn_Delete(&cr->cr_UserName);
	    cr->cr_UserName = name;
#if 1
	    cr->cr_UID = pw->pw_uid;
	    cr->cr_GID = pw->pw_gid;
#else
	    cr->cr_UID = UID2nfsuid(pw->pw_uid);
	    cr->cr_UID = GID2nfsgid(pw->pw_gid);
#endif
	    RetVal = 1;
	    cr->cr_Flags |= CRF_USER;

	    cr->cr_ClientCache = NULL; /* invalidate cache */
	}
    }
    
    return RetVal;
}

BOOL
cr_UMaskWasSet(CRED *cr)
{
    return (BOOL) ((cr->cr_Flags & CRF_UMASK) != 0);
}

/* will only update umask */
void
cr_UpdateCreds(CRED *cr, struct Task *client)
{
    struct UserGroupCredentials *creds;
    
    if(! (cr->cr_Flags & CRF_UMASK))
    {
	if(client != cr->cr_ClientCacheUMask)
	{
	    creds = getcredentials(client);
	    if(creds)
	    {
		cr->cr_UMask = creds->cr_umask;
		cr->cr_ClientCacheUMask = client;
	    }
	    else
	    {
		cr->cr_UMask = CR_DEFAULT_UMASK;;
		cr->cr_ClientCacheUMask = NULL;
	    }
	}
    }
}
    
void
cr_SetUMask(CRED *cr, u_int umask)
{
    cr->cr_Flags |= CRF_UMASK;
    cr->cr_UMask = umask;
    cr->cr_ClientCacheUMask = NULL; /* invalidate cache */
}

/*
 *  get name of actual user (if stored), NULL otherwise
 */

const char*
cr_GetUser(CRED *cr)
{
    if(cr->cr_Flags & CRF_USER)
	return cr->cr_UserName;
    else
	return NULL;
}

#if 0
u_int
Xcr_GetUMask(CRED *cr)
{
    return cr->cr_UMask;
}

u_int
Xcr_GetUID(CRED *cr)
{
    return cr->cr_UID;
}

u_int
Xcr_GetGID(CRED *cr, struct Task *client)
{
    return cr->cr_GID;
}
#endif

static int
cr_getgroups(const char *name, int gidsetlen, gid_t *gidset)
{
    int i = 0, error = 0;
    struct group *grp;

    if(gidsetlen > NGROUPS)
	gidsetlen = NGROUPS;
    
    setgrent();
    while((grp = getgrent()) && !error)
    {
	char **s;

	s = grp->gr_mem;

	while(*s)
	{
	    if(strcmp(name, *s) == 0)
	    {
		if(i == gidsetlen)
		{
		    error = 1;
		    break;
		}
		gidset[i++] = grp->gr_gid;
		break;
	    }
	    s++;
	}
    }
    endgrent();
    
    if(error)
	return -1;
    else
	return i;
}
    
static AUTH *
cr_SetAuth(CRED *cr, struct Task *client, const char *HostName)
{
    AUTH *an_auth, *ret_auth = NULL;
    struct UserGroupCredentials *creds;
    gid_t groups[NGROUPS];
    int numgrp=0;
    
    if(cr->cr_Flags & CRF_USER)
    {
	struct passwd *pw;
	    
	pw = getpwnam(cr->cr_UserName);
	if(pw)
	    numgrp = cr_getgroups(cr->cr_UserName, NGROUPS, groups);
	
	AKDEBUG((0,"pw = %08lx, numgrp = %ld\n", pw, numgrp));
	    
	if(pw && (numgrp >= 0))
	{
	    int i;
	
	    cr->cr_UID = pw->pw_uid;
	    cr->cr_GID = pw->pw_gid;
		
	    cr->cr_NGroups = numgrp;
		
	    for(i = 0; i < numgrp; i++)
		cr->cr_GIDS[i] = groups[i];

	    an_auth = authunix_create((char *) HostName,
				      cr->cr_UID,
				      cr->cr_GID,
				      cr->cr_NGroups,
				      groups);
	    if(an_auth)
	    {
		AKDEBUG((0,"auth = %08lx\n", an_auth));
		
		if(cr->cr_Auth)
		    auth_destroy(cr->cr_Auth);
		cr->cr_Auth = an_auth;
		
		ret_auth = cr->cr_Auth;
	    }
	    cr->cr_ClientCache = NULL;
	}
    }
    else
    {
	creds = getcredentials(client);
	if(creds)
	{
	    an_auth = authunix_create((char *) HostName,
				      creds->cr_euid,
				      creds->cr_rgid,
				      creds->cr_ngroups,
				      creds->cr_groups);
	    if(an_auth)
	    {
		int i;
		    
		if(cr->cr_Auth)
		    auth_destroy(cr->cr_Auth);
		cr->cr_Auth = an_auth;
		
		cr->cr_UID = creds->cr_euid;
		cr->cr_GID = creds->cr_rgid;
		cr->cr_NGroups = creds->cr_ngroups;
		for(i = 0; i < creds->cr_ngroups; i++)
		    cr->cr_GIDS[i] = creds->cr_groups[i];

		cr->cr_ClientCache = client;
		ret_auth = cr->cr_Auth;
	    }
	}
    }    
    return ret_auth;
}

/* returns TRUE on success (no change is SUCCESS) */

BOOL
cr_UpdateAuth(CRED *cr, struct Task *client, const char *HostName)
{
    BOOL OK = TRUE;
    AUTH *ret_auth;
    
    if(cr->cr_Auth == NULL)
    {
	ret_auth = cr_SetAuth(cr, client, HostName);
	OK = (ret_auth != NULL);
    }
    else
    {
	if(cr->cr_Flags & CRF_USER)
	{
	    /* auth is set and o.k. */
	}
	else
	{
	    if(client == cr->cr_ClientCache)
	    {
		/* auth is set and o.k. */
	    }
	    else
	    {
		ret_auth = cr_SetAuth(cr, client, HostName);
		OK = (ret_auth != NULL);
	    }
	}
    }

    return OK;
	
}

BOOL
cr_AuthNeedsUpdate(CRED *cr, struct Task *client)
{
    BOOL Needed = FALSE;
    
    if(cr->cr_Auth == NULL)
    {
	Needed = TRUE;
    }
    else
    {
	if(cr->cr_Flags & CRF_USER)
	{
	    /* auth is set and o.k. */
	}
	else
	{
	    if(client != cr->cr_ClientCache)
	    {
		Needed = TRUE;
	    }
	    else
	    {
		struct UserGroupCredentials *creds;
		
		creds = getcredentials(client);
		if(creds)
		{
		    if(creds->cr_euid != cr->cr_UID)
		    {
			Needed = TRUE;
			cr_InvalidateCache(cr); /* invalidate cache */
		    }
		}
	    }
	}
    }

    return Needed;
}

#if 0
AUTH *
Xcr_GetAuth(CRED *cr)
{
    return cr->cr_Auth;

}
#endif

