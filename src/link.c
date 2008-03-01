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
 * File/Directory soft/hard-Link functions
 */

#include "nfs_handler.h"
#include "protos.h"

#include "Debug.h"


/*
 * follow given Path "FullName" from the root to the node
 * stopping on softlinks and returning "ReadLink" result
 *
 * returns
 *	2 when ok and softlink
 *	1 when ok, no soft link
 *	0 on Res2 error
 *	-1 if buffer is too small
 *
 * Note: very similiar to RootLocateDir
 *	 differences: LastElement will be parsed too,
 *		      softlink is no error
 */

static LONG
PathWalk(Global_T *g, 
	 UBYTE *FullName,
	 LONG FullNameLen,
	 UBYTE *LinkPath,	/* Buf to get result into, IN/OUTPUT name of LINK */
	 ULONG BufLen,		/* length of buf above */
	 LONG *Res2)		/* OUTPUT */
{
    UBYTE *PComp; /* points to current path component */
    UBYTE *WorkPath = NULL;
    DCEntry *ParDir, *ActDir;
    NEntry *ActNEnt;
    ACEntry *ACE;
    LONG LastElement = 0; /* flag: if set, last element of directory is found */
    /* (init important !) */
    LONG Pos = 0; /* fn_ScanPath needs this as buffer */
    /* (init important !) */
    LONG Res = 1;

    D(DBF_ALWAYS, "\tPathWalk(\"%s\")", FullName);

    WorkPath = StrNDup(FullName, FullNameLen); /* we will modify WorkPath */
    if(!WorkPath)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	Res = 0;
	
    }
    else
    {
	/* get (cached) root handle */
	ParDir = GetDirEntry(g, 
			     NULL, /* no name entry */
			     g->g_MntAttr.fileid,
			     &g->g_MntFh,
			     NULL, /* currently not used */
			     Res2);
	if(ParDir)
	{
	    if(FullNameLen)	/* Filename != "" */
	    {
		do
		{
		    PComp = fn_ScanPath(WorkPath, FullNameLen,
					&Pos, &LastElement);
		    ASSERT(PComp);

		    D(DBF_ALWAYS,"\tPComp = \"%s\"", PComp);
		    if(*PComp == '/') /* goto parent dir, not allowed in this 
					 version, must be unified path ! */
		    {
			    E(DBF_ALWAYS, "FIXME: *** parent dir reference not allowed !!");
			    Res = 0;
			    break;
		    }
		    ActNEnt = GetNameEntry(g, ParDir, PComp, &ACE, Res2);
		    if(!ActNEnt)
		    {
			Res = 0;
			break;
		    }

		    if((ActNEnt->ne_FType != NFDIR)
		       && (ActNEnt->ne_FType != NFLNK))
		    {
			*Res2 = ERROR_INVALID_COMPONENT_NAME;
			Res = 0;
			break;
		    }
		    if(ActNEnt->ne_FType == NFLNK)
		    {
			nfspath u_lpath;
		    
			ASSERT(ACE);
			ASSERT(LinkPath);
			u_lpath = nfs_ReadLink(g->g_NFSClnt,
					       &ACE->ace_NFSFh, Res2);
			if(u_lpath)
			{
			    D(DBF_ALWAYS,"\treadlink returned: %s", u_lpath);
#if 0
			    if(UseUnixLinks(g))
				;
#endif
			    if(fn_InsertLink(LinkPath,
					     BufLen,
					     g->g_VolumeName,
					     FullName,
					     PComp-WorkPath,
					     strlen(PComp),
					     u_lpath,
					     Res2) == NULL)
			    {
				if(*Res2 == ERROR_NO_FREE_STORE)
				    Res = -1;
				else
				    Res = 0;
			    }
			    else
				Res = 2;
			}
			else
			{
			    Res = 0;
			}
			break;
		    }
		    /* note: GetDirEntry will modify ActNEnt if necessary */
		    ActDir = GetDirEntry(g, 
					 ActNEnt,
					 DCE_ID_UNUSED,
					 &ACE->ace_NFSFh,
					 PComp,
					 Res2);
		    ParDir = ActDir;
		} while(!LastElement && ParDir);
	    }
	    else
	    {			/* filename: "" */
		LinkPath[0] = 0;
		Res = 1;
	    }
	}
	else
	    Res = 0;
	fn_Delete(&WorkPath);
    }
    return Res;
}

LONG
act_READ_LINK(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    UBYTE *Path = (UBYTE *) pkt->dp_Arg2;
    UBYTE *Buf = (UBYTE *) pkt->dp_Arg3;
    LONG BufSize = pkt->dp_Arg4;

    LONG Res2;

    UBYTE *FullName;
    LONG FullNameLen;

    ELOCK *oldelock = NULL;
    LONG Res;

    FullName = CBuildFullName(g,
			     fl, Path,
			     &oldelock,
			     &FullNameLen,
			     NULL, NULL,
			     NULL,
			     &Res2);
    if(!FullName)
	return SetRes(g, DOSFALSE, Res2);

    Res = PathWalk(g,
		   FullName, FullNameLen,
		   Buf,
		   BufSize,
		   &Res2);
    switch(Res)
    {
 case 2:
	SetRes(g, strlen(Buf), 0);
	D(DBF_ALWAYS,"Res: %s", Buf);
	break;
 case 1:
	SetRes(g, DOSFALSE, ERROR_OBJECT_NOT_FOUND);
	break;
 case 0:
	SetRes(g, DOSFALSE, Res2);
	break;
 case -1:
	W(DBF_ALWAYS, "\tReadLink: buf to small");
	SetRes(g, -2, 0);
	break;
    }

    fn_Delete(&FullName);

    return g->g_Res1;
}

LONG do_act_MAKE_HARD_LINK(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl1 = (LOCK *) pkt->dp_Arg1;
    CBSTR cbstr1 = (CBSTR) pkt->dp_Arg2;
    /* special treating of Arg3, must be converted here,
       Arg3 may char pointer in case of softlink */
    LOCK *fl2 = (LOCK *) BADDR(pkt->dp_Arg3);

    UBYTE *Name1 = NULL;
    LONG Res2;
    ELOCK *destelock;

    DCEntry *ParentDir1;

    destelock = lock_Lookup(g, fl2);
    if(!destelock)
    {
	SetRes(g, DOSFALSE, ERROR_INVALID_LOCK);
	goto exit;
    }

    ParentDir1 = LRootLocateDir(g, 
			       fl1, cbstr1, 0 /* not CString */, 
			       &Name1, NULL,
			       NULL,NULL, /* no FullName needed */
			       &Res2);
    if(!ParentDir1)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    if(nfs_Link(g->g_NFSClnt, 
		&destelock->efl_NFSFh,
		&ParentDir1->dce_NFSFh, Name1, &Res2))
	SetRes1(g, DOSTRUE);
    else
	SetRes(g, DOSFALSE, Res2);

 exit:
    fn_Delete(&Name1);
    
    return g->g_Res1;
}

LONG do_act_MAKE_SOFT_LINK(Global_T *g, DOSPKT *pkt)
{
    LOCK *fl = (LOCK *) pkt->dp_Arg1;
    CBSTR cbstr = (CBSTR) pkt->dp_Arg2;
    UBYTE *Path = (UBYTE *) pkt->dp_Arg3;
/*     LONG Mode = pkt->dp_Arg4; */
    LONG Res2;
    UBYTE *Name;
    DCEntry *ParentDir;

    ParentDir = LRootLocateDir(g, 
			       fl, cbstr, 0, /* not C-String*/
			       &Name, NULL,
			       NULL, NULL,
			       &Res2);
    if(!ParentDir)
    {
	SetRes(g, DOSFALSE, Res2);
	goto exit;
    }

    if(nfs_Symlink(g->g_NFSClnt, &ParentDir->dce_NFSFh, Name,
		   nfsc_GetUID(g), nfsc_GetGID(g), Path,  &Res2))
	SetRes1(g, DOSTRUE);
    else
	SetRes(g, DOSFALSE, Res2);

 exit:
    fn_Delete(&Name);

    return g->g_Res1;
}

LONG act_MAKE_LINK(Global_T *g, DOSPKT *pkt)
{
    LONG Mode = pkt->dp_Arg4;

    switch(Mode)
    {
 case LINK_SOFT:
	return do_act_MAKE_SOFT_LINK(g, pkt);
 case LINK_HARD:
	return do_act_MAKE_HARD_LINK(g, pkt);
default:
	return SetRes(g, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
    }
}
