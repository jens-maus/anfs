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
 * Filename handling.
 */

/* convert amiga filename to unix filename */

#include <string.h>

#ifndef FN_TESTS
#include "nfs_handler.h"
#include "protos.h"
#else
#include <exec/types.h>
#include <exec/memory.h>

#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>

typedef UBYTE *CBSTR;
#endif

#include "chdebug.h"

#define MASSIVE_ASSERT 0

#if 0
#undef AKDEBUG
#define AKDEBUG(x)
#endif

/* convert amiga "/" style to unix ./.. style */
/* e.g. dir1//dir2 is mapped to dir1/../dir2 */

UBYTE *
fn_AM2UN(UBYTE *s)
{
    UBYTE *s2 = s, *s3, c;
    LONG ecnt = 0; /* extra chars */
    LONG l = 0;

    if(!s)
	return NULL;

    while(c = *s2++)
    {
	if(c == '/')
	{
	    if(*s2 == '/')
		ecnt+=2;
	}
	l++;
    }
    s3 = s2 = AllocVec(l+1+ecnt, MEMF_PUBLIC);
    if(s2)
    {
	while(c = *s++)
	{
	    *s2++ = c;
	    if(c == '/')
	    {
		if(*s == '/')
		{
		    *s2++ = '.';
		    *s2++ = '.';
		}
	    }
	}
	*s2 = '\0';
#if MASSIVE_ASSERT
	chassert(strlen(s3) < l+1+ecnt);
#endif
    }	     
    return s3;
}

static __inline UBYTE *
bstrchr(CBSTR bs, int c)
{
    LONG l = bs[0];
    UBYTE *s = &bs[1];
    
    while(l--)
    {
	if(*s == c)
	    return s;
	s++;
    }
    return NULL;
}

void
fn_AddPart(UBYTE *s1, LONG l1, UBYTE *s2, LONG l2, UBYTE **ds, LONG *dl)
{
    UBYTE *s;
    LONG l;

    if(l1 == -1) l1 = strlen(s1);
    if(l2 == -1) l2 = strlen(s2);


    AKDEBUG((0,"fn_AddPart( \"%s\" , \"%s\")\n", s1, s2));

    chassert(s1);
    chassert(s2);
    
    l = l1+l2+3;
    s = AllocVec(l, MEMF_PUBLIC);
    
    if(!s)
    {
	*ds = NULL;
	return;
    }
#if 0
    CopyMem(s1, s, l1+1);
#else
    memcpy(s, s1, l1+1);
#endif
    if(AddPart(s, s2, l))
    {
	*dl = strlen(s);
	*ds = s;

#if MASSIVE_ASSERT
	chassert(*dl < l);
#endif
	AKDEBUG((0,"\t = \"%s\"\n", s));
	return;
    }

    FreeVec(s);
    *ds = NULL;
}

__inline UBYTE *StrNDup(const UBYTE *s, LONG l)
{
    UBYTE *ns;

    ns = AllocVec(l+1, MEMF_PUBLIC);
    if(ns)
    {
	CopyMem((UBYTE *) s, ns, l);
	ns[l] = 0; /* null terminate string */
    }
    
    return ns;
}

__inline UBYTE *StrDup(const UBYTE *s)
{
    return StrNDup(s, strlen(s));
}

/* 
  FIXME? : fix this function for pathes like "a/b/c/d//"
*/

void fn_FilePart(UBYTE *s, LONG l, UBYTE **ds, LONG *dl)
{
    UBYTE *p1, *s2 = NULL;
    LONG l2;
    
    if(!l)
    {
	*ds = StrDup("");
	*dl = 0;
	return;
    }
    if(s[l-1] == ':') 
    {
	*ds = StrNDup(s, l); /* FIXME: does THIS make sense? Check usage! */
	*dl = l;
	return;
    }

    p1 = FilePart(s);

    if(!p1[0]) /* e.g. a/b/c/d/ but also /a/b/c/d// ... */
    {
	chassert(l>1);
	/* FIXME: better solution possible */
	/* we need to modify s, so make a copy */
	s2 = StrNDup(s, l);
	if(s2)
	{
	    p1 = s2 + (p1-s);
	    chassert(p1[-1] == '/'); /* real assert */

	    if(p1[-2] == '/')
	    {
		FIXME("add support for PathNames like a/b/c/d//");
		FreeVec(s2);
		*ds = NULL;
		return;
	    }
	    p1[-1] = '\0'; /* strip single "/" */
	    p1 = FilePart(s2);
	    s = s2; /* for length calc below */
	}
	else
	{
	    *ds = NULL;
	    return;
	}
    }
    chassert(p1[0]);

    l2 = s+l-p1;

    *ds = StrNDup(p1, l2);
    *dl = l2;
    if(s2)
	FreeVec(s2);
#if MASSIVE_ASSERT
    if(*ds)
    {
	chassert(strlen(*ds) == *dl);
    }
#endif
}

void fn_B2CStr(CBSTR bs, UBYTE **ds, LONG *dl)
{
    UBYTE *s;
    LONG l = bs[0];

    s = AllocVec(l+1, MEMF_CLEAR | MEMF_PUBLIC);
    if(s)
    {
	CopyMem(&bs[1], s, l);
	s[l] = '\0';
    }

    *ds = s;
    *dl = l;
}

/*
 * This function scans the given path and returns a pointer to the next 
 * component
 * 
 * Pos must not be modified between calls.
 * If the returned pointer points to "/" that means "goto parent".
 * LastElement will be true when the return value points
 * to last element of path.
 *
 * Notes:
 * 	- The given Path will be modified in-place !
 *      - LostElement and Pos must be initialized to 0 before first call
 *	- Illegal Pathes are
 *		- Empty path: ""
 *		- 1 Trailing "/": e.g. "xyz/"
 *	- But Legal: file consisting of a single "/"
 */

UBYTE *fn_ScanPath(UBYTE *APath, LONG PathLen, LONG *PosP, LONG *LastElement)
{
    register UBYTE c, *p, *e;
    LONG Pos;

    chassert(APath);
    chassert(PathLen);
    chassert(PosP);
#ifdef DEBUG
    if(!(*PosP))
    {
	chassert(strlen(APath) == PathLen);
    }
    else
    {
	chassert(*PosP <= PathLen);
    }
#endif
    chassert(LastElement);
    chassert(!(*LastElement));

    p = APath;
    Pos = *PosP;

    AKDEBUG((0,"\tScanPath(\"%s\", %ld, %ld)\n", APath, PathLen, *PosP));

    if(Pos != 0)
    {
	p += (Pos-1);
	*p++ = '/';   /* restore slash */
    }
    e = p;
#ifdef DEBUG
    if(PathLen > 1)
    {
	chassert(*p); /* will fail for illegal pathes from list above */
    }
#endif

    while((c = *e) && (c != '/'))
	e++;
    if(c)
    {
	if(e != p) /* something between slashes */
	{
	    chassert(c == '/');
	    *e = 0; /* terminate string */
	}
	*PosP = (e - APath) + 1; /* 1 means APath[0] */
#if MASSIVE_ASSERT
	chassert(*PosP < PathLen);
#endif
    }
    else
	*LastElement = 1;

    return p;
}

/* 
  Note:
  a ":" in the input string will be treated as an ordinary character

  this function will unify the given pathname and will return the
  new allocated string

  unifying means removing "//" sequences e.g. "a/b/c/d///" becomes "a/b"
  also "a/" will become "a"

  a leading "/" is considered illegal
  a leading "../" is considered illegal
  ".." is considered illegal
  a single trailing "/" will be removed (?)

  (possible problem ?)
  	"/./" sequences will be reduced to "/"
	"/." sequence at end of string will be removed 
	(this will be the same as the consequence of scanning a directory path 
	containing "." via nfs calls)
	"/../" will remove the preceeding directory
	"/../" at end of string will be handled as "/.." 
*/

UBYTE *
fn_PathUnify(UBYTE *ss, /* Input Parameter */
	     LONG *Len, LONG *StrippedDirs, LONG *Res2 /* Output Parameters */
	     )
{
#if MASSIVE_ASSERT
    LONG memlen;
#endif
    LONG l;
    UBYTE *s=ss,*d, *d0, c;
    LONG AddSep;

    AKDEBUG((0, "fn_PathUnify( \"%s\" )\n", ss));

    l = strlen(s);
#if MASSIVE_ASSERT
    memlen = l+1;
#endif
    
    *StrippedDirs = 0;

    if((*s == '/')
       || ((*s == '.') && (s[1] == '.') &&(!s[2] || (s[2] == '/'))))
    {
	*Res2 = ERROR_OBJECT_NOT_FOUND;
	return NULL;
    }
    d = AllocVec(l+1, MEMF_PUBLIC);
    if(!d)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }

    AddSep = 0; /* will be set if path separator ("/") must be added before
		   next component */
    d0 = d;
    if(l) {
	while(l--) {
	    if((c = *s++) == '/') {
		if(l) {
		    if(*s == '/') {
			*StrippedDirs = 1;

			/* handle "//" sequences */
			while(l && *s == '/') {
			    if(d == d0) {
				/* no dirname to strip */
				FreeVec(d0);
				*Res2 = ERROR_INVALID_COMPONENT_NAME;
				return NULL;
			    }
			    d--; /* set d to last character of last component */
			    /* strip last directory name */
			    while((d != d0) && (*d != '/'))
			    {
				d--;
			    }
			    l--; s++;
			}
			/* s points beyond last "/" of "//" here,
			   so re-activate last "/" */
			l++; s--;
			
			AddSep = 1;
		    }
		    else if (*s == '.') {
			/* "/." so far */
			if((*(s+1) == '/') /* "/./" sequence */
			   || (*(s+1) == 0)) /* "/." at end of string */
			{
			    s++; /* skip "/.", 
				    will continue on second "/" or end */
			    l--;
			}
			else if(*(s+1) == '.') {
			    /* "/.." so far */
			    if((*(s+2) == '/') /* "/../" */
			       || *(s+2) == 0) /* "/.." at end of string */
			    {
				if(d == d0) {
				 /* no dirname to strip */
				    FreeVec(d0);
				    *Res2 = ERROR_INVALID_COMPONENT_NAME;
				    return NULL;
				}
				d--; /* set d to last character of
					last component */
				/* strip last directory name */
				while((d != d0) && (*d != '/'))
				    d--;

				/* here: s points to first "/" of "/.." */
				l -= 2;
				s += 2; /* skip ".." */

				AddSep = 1;
			    }
			    else /* "/..<normal>" */
				AddSep = 1;
			}
			else /* "/.<normal>" */
			    AddSep = 1;
		    }
		    else /* "/<normal>" */
			AddSep = 1;
		}
#if 0 
/* remove single trailing "/" if 0 */
		else
		{
		    *d++ = c;
		}
#endif
	    }
	    else if((c == '.') && ((*s == '/') || (*s == 0)))
		/* skip initial "./" or single "." */
	    {
		;
	    }
	    else
	    {
		if(AddSep && (d != d0))
		{
		    *d++= '/';
		}
		AddSep = 0;
		*d++ = c;
	    }
	}
    }
    *d = '\0';

    if(Len)
    {
	*Len = d-d0;
    }
#if MASSIVE_ASSERT
    chassert((d-d0) < memlen);
#endif

    if((*d0 == '/')
       || ((*s == '.') && (s[1] == '.') &&(!s[2] || (s[2] == '/'))))
    {
	AKDEBUG((0, "considered illegal (leading / or ../): \t = \"%s\"\n", d0));

	FreeVec(d0);
	*Res2 = ERROR_OBJECT_NOT_FOUND;
	return NULL;
    }

    AKDEBUG((0, "\t = \"%s\"\n", d0));

    return d0;
}

/* 
 * removes all characters incl. ":" if present
 * removes single trailing "/" but leaves a filename consisting
 * of a single "/"
 */

UBYTE *fn_BPathWithoutDev(CBSTR bs, LONG *IsAbsolute, LONG *Res2)
{
    LONG l = bs[0];
    UBYTE *s,*d;
#if MASSIVE_ASSERT
    LONG memlen;
#endif
    
    s = bstrchr(bs, ':');
    if(s)
    {
	l -= (s-&bs[1]);
	l--;
	s++;
	*IsAbsolute = 1;
    }
    else
    {
	s = &bs[1];
	*IsAbsolute = 0;
    }
#if MASSIVE_ASSERT
    memlen = l+1;
#endif
    d = AllocVec(l+1, MEMF_PUBLIC);
    if(!d)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }

    if(l)
    {
	memcpy(d,s,l);
	/* remove single trailing "/" */
	if(l>1)
	{
	    if((d[l-1] == '/') && (d[l-2] != '/'))
		l--;
	}
    }

    d[l] = '\0';
#if MASSIVE_ASSERT
    chassert(strlen(d) < memlen);
#endif
    return d;
}

UBYTE *fn_PathWithoutDev(UBYTE *ss, LONG *IsAbsolute, LONG *Res2)
{
    LONG l;
    UBYTE *s,*d;

    l = strlen(ss);
    s = strchr(ss, ':');
    if(s)
    {
	l -= (s-ss);
	l--;
	s++;
	*IsAbsolute = 1;
    }
    else
    {
	s = ss;
	*IsAbsolute = 0;
    }
    d = AllocVec(l+1, MEMF_PUBLIC);
    if(!d)
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }

    if(l)
	memcpy(d,s,l);

    d[l] = '\0';
    return d;
}

void fn_Delete(UBYTE **s)
{
    if(s && *s)
    {
	FreeVec(*s);
	*s = NULL;
    }
}

/* back search for separator "/" or ":" */

static UBYTE *fn_RSep(UBYTE *s, UBYTE *e)
{
    while(s != e)
    {
	if(*e == '/')
	    break;
	if(*e == ':')
	    break;
	e--;
    }
    return e;
}

/* 
  return pointer to new allocated memory containg the path to
   the entry described by s

   some expected results:

   dev:			dev:
   dev:dir1		dev:
   dev:dir1/dir2	dev:dir1
   dir1			""
   dev:dir1/dir2/       dev:dir1 (not supported yet)
   dev:dir1/dir2//      dev: (not supported yet)

 */

void fn_PathPart(UBYTE *s, LONG l, UBYTE **ds, LONG *dl)
{
    UBYTE *p1;
    LONG l2;

    AKDEBUG((0,"\tfn_PathPart(\"%s\"\n", s));
    if(!l) /* Name: "" */
    {
	*ds = StrDup("");
	*dl = 0;
	return;
    }
    if(s[l-1] == ':') /* Name: "<dev>:" */
    {
	*ds = StrNDup(s, l);
	*dl = l;
	return;
    }
    p1 = s + l -1; /* points to last char */
    if(p1[0] == '/')
    {
	chassert(0); /* FIXME: not supported yet */
#if 0
	if(l>1)
	{
	    if(p1[-1] == '/')
	    {
	    }
	    else
	    {
		p1 = fn_RSep(s,p1-1);
	    }
	}
	else
	{
	    p1--;
	}
#endif
    }
    chassert(s[l-1] != '/');
    
    p1 = PathPart(s); /* FIXME: use DirPath? use fn_DirPath at caller ?*/
    l2 = p1-s;

    *ds = p1 = StrNDup(s, l2);
    if(*ds)
    {
	p1[l2] = 0;	/* null terminate string */
#if MASSIVE_ASSERT
    chassert(strlen(p1) <= l2);
#endif

	AKDEBUG((0,"\t\t= \"%s\"\n", p1));
    }
    *dl = l2;
}

#if 0
void fn_DirPart(UBYTE *s, LONG l, UBYTE **ds, LONG *dl)
{
    UBYTE *p1;
    LONG l2;
    
    chassert(l);
    chassert(s[l-1] != ':');
#if 0
    if(!l)
    {
	*ds = StrDup("");
	*dl = 0;
	return;
    }
    if(s[l-1] == ':')
    {
	*ds = StrNDup(s, l);
	*dl = l;
	return;
    }
#endif
    chassert(s[l-1] != '/');
    
    p1 = PathPart(s);
    l2 = p1-s;

    *ds = StrNDup(s, l2);
    *dl = l2;
}
#endif

#if 1
#define DEBUG_INSERTLINK 1
#endif

/*
 * Insert contents of Link into Path, replacing part of length Len, starting
 * at From.
 * The Destination buffer Buf has size BufLen.
 * The "/" eventually preceeding or following part will be ignored.
 
 * Returns Buffer on success NULL else.
 */

UBYTE *
fn_InsertLink(UBYTE *Buf, ULONG BufLen,
	      UBYTE *Device, /* name of this device */
	      UBYTE *Path, ULONG From, ULONG Len,
	      UBYTE *Link, LONG *Res2)
{
    ULONG BufFree = BufLen;
    UBYTE *d = Buf;

    if(Device)
    {
	ULONG l;
	l = strlen(Device);

	if((l+2) > BufLen)
	{
	    *Res2 = ERROR_NO_FREE_STORE;
	    return NULL;
	}
	else
	{
	    memcpy(Buf, Device, l+1);
	    if(Buf[l-1] != ':')
	    {
		Buf[l] = ':';
		Buf[l+1] = 0;
		l++;
	    }
	    BufFree -= l;
	    d += l;
	}
    }
    if(From > 0)
    {
	chassert(Path[0] != ':');
	chassert(Path[From-1] != ':');
	if(From < BufFree)
	{
	    memcpy(d, Path, From-1); /* don\'t copy "/" */
	    d[From-1] = 0;
	}
	else
	{
	    *Res2 = ERROR_NO_FREE_STORE;
	    return NULL;
	}
    }
    if(!AddPart(Buf, Link, BufLen))
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }
    if(Path[From+Len] == 0) /* path does end after part */
	return Buf;
    chassert(Path[From+Len] == '/');
    chassert(Path[From+Len+1]);
    
    if(!AddPart(Buf, &Path[From+Len+1], BufLen))
    {
	*Res2 = ERROR_NO_FREE_STORE;
	return NULL;
    }
    return Buf;
}
