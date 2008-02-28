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

/* FIXME: these functions are referenced by RPC lib and should be removed there */

#include <exec/types.h>
#include <dos/dos.h>

#include <proto/exec.h>

#include <stdarg.h>

#include "supp.h"

/* fake mkproto */

#define Myfprintf fprintf
#define Myperror perror
#define Mysprintf sprintf
#define Mymalloc malloc
#define Mycalloc calloc
#define Myrealloc realloc
#define Myfree free

int Myfprintf(int i, const char *fmt, ...) { return 1; }
void Myperror(void) { }

LONG
Mysprintf(UBYTE *buffer, UBYTE *fmt, ...)
{
    register LONG i;
    register va_list args;
    
    va_start(args, fmt);
    i = VSPrintf(buffer, fmt, args);
    va_end(va);

    return i;
}

#if 0 /* stress memory */
void *
Mymalloc(size_t size)
{
    return AllocVec(size, MEMF_PUBLIC);
}
void *
Mycalloc(size_t n, size_t size)
{
    return AllocVec(n * size, MEMF_CLEAR | MEMF_PUBLIC);
}
void *
Myrealloc(void *b, size_t n)
{
    ULONG *lp, l;
    void *nb = NULL;

    if(n)
    {
	lp = (ULONG *) b;
	l = lp[-1];
	nb = Mymalloc(n);
	if(nb)
	{
	    if(l > n)
		l = n;
	    CopyMem(b, nb, l);
	    Myfree(b);
	}
    }
    
    return nb;
}
void
Myfree(void *ptr)
{
    FreeVec(ptr);
}
#endif
