#ifndef CLNT_AUDP_H
#define CLNT_AUDP_H

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

/*
 * Asynchronous RPC Support.
 */

#include <rpc/clnt.h>

/*
 * audp control operations
 */

/*
 * standard rpc functions will operate on this as CLIENT, CLIENT_AUDP is
 * only used internal and for clnt_send, clnt_wait
 */

typedef struct CLIENT_AUDP
{
    CLIENT cla_clnt;		/* standard client structure */
    caddr_t cla_resp;		/* result pointer (on succesful wait) */
    caddr_t cla_userdata;		/* additional user specific data
				   as given to send (on successful wait) */
    struct clnt_aops
    {
	/* send remote procedure call */
	enum clnt_stat	(*cl_send)(struct CLIENT *rh, u_long proc, 
				   xdrproc_t xargs, void * argsp, 
				   xdrproc_t xres, void * resp,
				   caddr_t user_data);
	/* wait for remote procedure result */
	enum clnt_stat	(*cl_wait)(struct CLIENT *rh,
				   struct timeval timeout);
	enum clnt_stat	(*cl_sendsb)(struct CLIENT *rh, u_long proc, 
				     xdrproc_t xargs, void * argsp, 
				     xdrproc_t xres, void * resp,
				     caddr_t userdata,
				     void *block, u_int blen);
    } *cla_ops;
} CLIENT_AUDP;
    
typedef struct clnt_audp_cb
{
    /* callback pointer */
    int (*cla_cb)(CLIENT_AUDP *cl, struct clnt_audp_cb *cb); 
    /* user private data, will not be touched by clnt_audp */
    void *cla_userdata;		
} clnt_audp_cb;

#define CLSET_TIMEOUT_CB 6 /* set timeout callback, will be called when timeout arises, return value != 0
				for retry, 0 to fail with timeout */
#define CLSET_NUM_HANDLE 7 /* set number of async handles */
#define CLGET_NUM_HANDLE 8 /* get number of async handles */

/*
 * enum clnt_stat
 * CLNT_SEND(CLIENT_AUDP *rh, u_long proc,
 *	     xdrproc_t xargs, void * argsp, 
 *	     xdrproc_t xres, void * resp,
 *	     caddr_t *userdata);
 *
 * Note: returns RPC_TIMEOUT if send was o.k.
 * Note: userdata will not be modified by clnt_send but
 *	 will be returned on successful wait,
 *	 may be used to identify send request
 */

#define CLNT_SEND(rh, proc, xargs, argsp, xres, resp, userdata)	\
	((*((CLIENT_AUDP *) rh)->cla_ops->cl_send)\
	 (rh, proc, xargs, argsp, xres, resp, userdata))
#define clnt_send(rh, proc, xargs, argsp, xres, resp, userdata)	\
	((*((CLIENT_AUDP *) rh)->cla_ops->cl_send)\
	 (rh, proc, xargs, argsp, xres, resp, userdata))

/*
 * like CLNT_SEND but allows additional block of raw data
 */
    
#define CLNT_SENDSB(rh, proc, xargs, argsp, xres, resp, ud, bl, blen)	\
	((*((CLIENT_AUDP *) rh)->cla_ops->cl_sendsb)\
	 (rh, proc, xargs, argsp, xres, resp, ud, bl, blen))
#define clnt_sendsb(rh, proc, xargs, argsp, xres, resp, ud, bl, blen)	\
	((*((CLIENT_AUDP *) rh)->cla_ops->cl_sendsb)\
	 (rh, proc, xargs, argsp, xres, resp, ud, bl, blen))

/*
 * enum clnt_stat
 * CLNT_WAIT(CLIENT_AUDP *rh, struct timeval timeout);
 */

#define CLNT_WAIT(rh, timeout)	\
	((*((CLIENT_AUDP *) rh)->cla_ops->cl_wait)(rh, timeout))
#define clnt_wait(rh, timeout)	\
	((*((CLIENT_AUDP *) rh)->cla_ops->cl_wait)(rh, timeout))

/*
 * get result pointer if wait was successful
 */
     
#define CLNT_GETWAITRES(rh) ((CLIENT_AUDP *) rh)->cla_resp
#define clnt_getwaitres(rh) ((CLIENT_AUDP *) rh)->cla_resp
#define CLNT_GETUSERDATA(rh) ((CLIENT_AUDP *) rh)->cla_userdata
#define clnt_getuserdata(rh) ((CLIENT_AUDP *) rh)->cla_userdata
#endif

/* clnt_audp.c */
CLIENT *clntaudp_bufcreate ( struct sockaddr_in *raddr , u_long program , u_long version , struct timeval wait , register int *sockp , u_int sendsz , u_int recvsz , u_int numasync );
CLIENT *clntaudp_create ( struct sockaddr_in *raddr , u_long program , u_long version , struct timeval wait , register int *sockp , u_int numasync );
