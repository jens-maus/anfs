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

 $Id: AUTHORS 2600 2007-01-01 16:34:56Z damato $

***************************************************************************/

/* @(#)clnt_generic.c	2.2 88/08/01 4.0 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_generic.c 1.4 87/08/11 (C) 1987 SMI";
#endif
/*
 * Copyright (C) 1987, Sun Microsystems, Inc.
 */
#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netdb.h>

#include "rpc/clnt_audp.h"

/*
 * Generic client creation: takes (hostname, program-number, protocol) and
 * returns client handle. Default options are set, which the user can 
 * change using the rpc equivalent of ioctl()'s.
 */
CLIENT *
clnt_acreate(char *hostname, u_long prog, u_long vers, u_int numasync)
#if 0
	char *proto;
#endif
{
	struct hostent *h;
	struct protoent *p;
	struct sockaddr_in sin;
	int sock;
	struct timeval tv;
	CLIENT *client;

	h = gethostbyname(hostname);
	if (h == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNHOST;
		return (NULL);
	}
	if (h->h_addrtype != AF_INET) {
		/*
		 * Only support INET for now
		 */
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = EAFNOSUPPORT; 
		return (NULL);
	}
	sin.sin_family = h->h_addrtype;
	sin.sin_port = 0;
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	bcopy(h->h_addr, (char*)&sin.sin_addr, h->h_length);
#if 0
	p = getprotobyname(proto);
	if (p == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		rpc_createerr.cf_error.re_errno = EPFNOSUPPORT; 
		return (NULL);
	}
#endif
	sock = RPC_ANYSOCK;
#if 0
	switch (p->p_proto) {
	case IPPROTO_UDP:
#endif
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		client = clntaudp_create(&sin, prog, vers, tv, &sock, numasync);
		if (client == NULL) {
			return (NULL);
		}
		tv.tv_sec = 25;
		clnt_control(client, CLSET_TIMEOUT, (caddr_t)&tv);
#if 0
		break;
	case IPPROTO_TCP:
		client = clnttcp_create(&sin, prog, vers, &sock, 0, 0);
		if (client == NULL) {
			return (NULL);
		}
		tv.tv_sec = 25;
		tv.tv_usec = 0;
		clnt_control(client, CLSET_TIMEOUT, (caddr_t)&tv);
		break;
	default:
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = EPFNOSUPPORT; 
		return (NULL);
	}
#endif
	return (client);
}
