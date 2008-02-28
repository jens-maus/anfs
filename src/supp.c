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

/* minor support functions */

#include <exec/types.h>
#include <exec/memory.h>

#include <dos/dos.h>

#include <proto/exec.h>

#include <stdarg.h>
#include <string.h>

struct PutCArgs
{
    UBYTE *BufP;
    LONG Len;
};

#if 0 /* AmiTCP 4.0 has them */
static void __asm
MyPutc(register __d0 ULONG Char, register __a3 struct PutCArgs *pca)
{
    *(pca->BufP)++ = Char;
    if(Char)
	pca->Len++;
}

static LONG
VSPrintf(UBYTE *buffer, UBYTE *fmt, UBYTE *args)
{
    struct PutCArgs pca;
    
    pca.BufP = buffer;
    pca.Len = 0;
    
    RawDoFmt(fmt, args, MyPutc, &pca);

    return pca.Len;
}


static LONG
SPrintf(UBYTE *buffer, UBYTE *fmt, ...)
{
    LONG i;
    va_list args;
    
    va_start(args, fmt);
    i = VSPrintf(buffer, fmt, args);
    va_end(va);

    return i;
}
#endif

BSTR
NewBSTR(UBYTE *s)
{
    UBYTE *p;
    LONG l;

    l = strlen(s);
    p = AllocVec(l+2, MEMF_PUBLIC);
    if(p)
    {
	p[0] = l;
	strcpy(&p[1], s);
    }

    return MKBADDR(p);
}

BSTR
DupBSTR(BSTR bs)
{
    LONG l;
    UBYTE *p = NULL, *s;
    
    s = BADDR(bs);
    if(s)
    {
	l = s[0];
	p = AllocVec(l+2, MEMF_PUBLIC);
	if(p)
	{
	    CopyMem(s, p, l+1);
	    p[l+1] = 0; /* null terminate */
	}
    }

    return MKBADDR(p);
}

void
DelBSTR(BSTR bs)
{
    if(bs)
	FreeVec(BADDR(bs));
}



