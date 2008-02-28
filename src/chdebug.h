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

//#include <misclib/protos.h>
#include <stdlib.h>

extern void log_end(void);

#define chassert(val) 							      \
{									      \
    if(!(val))								      \
    {									      \
	APrintReq("%s: %ld: %s failed assertion\n", 			      \
		  __FILE__,__LINE__, #val);				      \
	AKDEBUG((3,"%s: %ld: %s failed assertion\n", 			      \
		  __FILE__,__LINE__, #val)); log_end();abort();		      \
    }									      \
}									      \

#define myassert(v) chassert(v)

#define FIXME(t)							      \
{									      \
    APrintReq("%s: %ld: need to be fixed: %s\n", 			      \
	      __FILE__,__LINE__, t);					      \
    AKDEBUG((2,"%s: %ld: need to be fixed: %s\n", 			      \
	     __FILE__,__LINE__, t));log_end();				      \
}

#define TNULL(x) myassert((x) != NULL)
#define TDEADBEEF(x) myassert((x) != (void *) 0xdeadbeef)
#define TDEADFOOD(x) myassert((x) != (void *) 0xdeadf00d)
#define TDEAD(x) TDEADBEEF(x); TDEADFOOD(x)

#define BZERO(x) bzero(x, sizeof(*x));

#ifdef DEBUG
extern int kdebug;
#define AKDEBUG(x) akprintf x;
#else
#define AKDEBUG(x)
#endif

void akprintf(int level, const char *fmt,...);

