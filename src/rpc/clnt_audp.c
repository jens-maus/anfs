/* $Id: clnt_audp.c,v 1.3 1994/04/10 19:01:00 alph Exp $ */

/* Asynchronous RPC calls, based on clnt_udp.c below */
   
/* @(#)clnt_udp.c	2.2 88/08/01 4.0 RPCSRC */
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
static char sccsid[] = "@(#)clnt_udp.c 1.39 87/08/11 Copyr 1984 Sun Micro";
#endif

#ifdef RCSID
const static char RCSid1[] = "@(#)clnt_audp.c, based on clnt_udp.c 1.39 87/08/11 Copyr 1984 Sun Micro";
const static char RCSid2[] = "$Id: clnt_audp.c,v 1.3 1994/04/10 19:01:00 alph Exp $ Copyr 1994 Carsten Heyl";
#endif

/*
 * alnt_audp.c, Implements an asynchronous UDP/IP based, client side RPC.
 *
 * Copyright (C) 1994, Carsten Heyl
 * based on
 * clnt_udp.c, Implements a UDP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <rpc/pmap_clnt.h>
#if 0
#include <stdio.h>
#endif
#include "rpc/clnt_audp.h"

#ifdef AMIGA
#include <proto/exec.h>
#include <proto/socket.h>
#include "chdebug.h"
#ifndef AMITCP
#define AMITCP
#endif
#endif

/*
 * UDP bases client side rpc operations
 */
static enum clnt_stat	clntaudp_call(CLIENT * h, u_long proc,
				      xdrproc_t xdr_args, caddr_t args_ptr,
				      xdrproc_t xdr_results,
				      caddr_t results_ptr,
				      struct timeval timeout);
static enum clnt_stat	clntaudp_send(CLIENT * h, u_long proc,
				      xdrproc_t xdr_args, caddr_t args_ptr,
				      xdrproc_t xdr_results,
				      caddr_t results_ptr,
				      caddr_t userdata);
static enum clnt_stat	clntaudp_sendsb(CLIENT * h, u_long proc,
					xdrproc_t xdr_args, caddr_t args_ptr,
					xdrproc_t xdr_results,
					caddr_t results_ptr,
					caddr_t userdata,
					caddr_t block, u_int blen);
static enum clnt_stat	clntaudp_wait(CLIENT * h, 
				      struct timeval timeout);
static void		clntaudp_abort(CLIENT * h);
static void		clntaudp_geterr(CLIENT * h, struct rpc_err * errp);
static bool_t		clntaudp_freeres(CLIENT * cl, 
					xdrproc_t xdr_res, caddr_t res_ptr);
static bool_t           clntaudp_control(CLIENT * cl,
					u_int request, caddr_t info);
static void		clntaudp_destroy(CLIENT * h);

static struct clnt_ops udp_ops =
{
    clntaudp_call,
    clntaudp_abort,
    clntaudp_geterr,
    clntaudp_freeres,
    clntaudp_destroy,
    clntaudp_control
};

static struct clnt_aops audp_ops =
{
    clntaudp_send,
    clntaudp_wait,
    clntaudp_sendsb
};

#define MAXASYNC 5

/*
 * Private data per async handle
 */

struct scu_data
{
    int		scu_inuse;	/* handle is in use */
    
    XDR		scu_outxdrs;
    u_int	scu_xdrpos;
    char	*scu_outbuf;
    /* remembered send params */
    caddr_t	scu_resultsp;	/* pointer to results */
    caddr_t	scu_userdata;
    
    struct timeval scu_time_waited;
    int 	scu_nrefreshes;	/* number of times to refresh cred */
};

/* 
 * Private data kept per client handle
 */
struct mcu_data
{
    u_int 	mcu_numscu;	/* number of scu handles (used slots) */
    u_int 	mcu_numfreescu; /* number of free scu handles */
    u_long	mcu_xid;	/* transaction id */
    struct scu_data mcu_scu[MAXASYNC];
    struct clnt_audp_cb mcu_timeout_cb;
    
    int		mcu_sock;
    bool_t	mcu_closeit;
    struct sockaddr_in mcu_raddr;
    int		mcu_rlen;	/* length of address above */
    struct timeval mcu_wait; /* retry timeout */
    struct timeval mcu_total; /* total timeout */
    struct rpc_err mcu_error;
    xdrproc_t	mcu_xresults; 	/* xdr routine for results */
    u_int	mcu_sendsz;
    u_int	mcu_recvsz;
    char	*mcu_outbuf;	/* pointer to start of all outbufs */
    char	mcu_inbuf[1];
};


/*
 * Create a UDP based client handle.
 * If *sockp<0, *sockp is set to a newly created UPD socket.
 * If raddr->sin_port is 0 a binder on the remote machine
 * is consulted for the correct port number.
 * NB: It is the clients responsibility to close *sockp.
 * NB: The rpch->cl_auth is initialized to null authentication.
 *     Caller may wish to set this something more useful.
 *
 * wait is the amount of time used between retransmitting a call if
 * no response has been heard;  retransmition occurs until the actual
 * rpc call times out.
 *
 * sendsz and recvsz are the maximum allowable packet sizes that can be
 * sent and received.
 *
 * num is the number of async handles to be created, must be 1 <= num <= MAXASYNC
 */

CLIENT *
clntaudp_bufcreate(struct sockaddr_in *raddr,
		  u_long program,
		  u_long version,
		  struct timeval wait,
		  register int *sockp,
		  u_int sendsz,
		  u_int recvsz,
		  u_int numasync)
{
    CLIENT_AUDP *cl;
    register struct mcu_data *cu;
    struct timeval now;
    struct rpc_msg call_msg;
    int i;
    
    if(numasync > MAXASYNC)
    {
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
	goto fooy;
    }
	    
    cl = (CLIENT_AUDP *)mem_alloc(sizeof(CLIENT_AUDP));
    if (cl == NULL)
    {
#if 0
	(void) fprintf(stderr, "clntudp_create: out of memory\n");
#endif
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
	goto fooy;
    }
    memset(cl, 0, sizeof(CLIENT_AUDP));
    sendsz = ((sendsz + 3) / 4) * 4;
    recvsz = ((recvsz + 3) / 4) * 4;
    cu = (struct mcu_data *)mem_alloc(sizeof(*cu) + recvsz);
    if (cu == NULL)
    {
#if 0
	(void) fprintf(stderr, "clntudp_create: out of memory\n");
#endif
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
	goto fooy;
    }
    memset(cu, 0, sizeof(*cu) + recvsz);
    
    cu->mcu_outbuf = mem_alloc(numasync * sendsz);
    if (cu->mcu_outbuf == NULL)
    {
#if 0
	(void) fprintf(stderr, "clntudp_create: out of memory\n");
#endif
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
	goto fooy;
    }
    memset(cu->mcu_outbuf, 0, numasync * sendsz);
    for(i = 0; i < numasync; i++)
    {
	cu->mcu_scu[i].scu_outbuf = &cu->mcu_outbuf[i*sendsz];
    }
    
    (void)gettimeofday(&now, (struct timezone *)0);
    if (raddr->sin_port == 0)
    {
	u_short port;
	if ((port = pmap_getport(raddr, program, version, IPPROTO_UDP)) == 0)
	{
	    goto fooy;
	}
	raddr->sin_port = htons(port);
    }
    cl->cla_clnt.cl_ops = &udp_ops;
    cl->cla_clnt.cl_private = (caddr_t)cu;
    cl->cla_ops = &audp_ops;
    cu->mcu_numscu = numasync;
    cu->mcu_numfreescu = numasync;
    cu->mcu_raddr = *raddr;
    cu->mcu_rlen = sizeof (cu->mcu_raddr);
    cu->mcu_wait = wait;
    cu->mcu_total.tv_sec = -1;
    cu->mcu_total.tv_usec = -1;
    cu->mcu_sendsz = sendsz;
    cu->mcu_recvsz = recvsz;
#ifdef AMIGA
    cu->mcu_xid = (u_long)FindTask(NULL) ^ now.tv_sec ^ now.tv_usec;
#else
    cu->mcu_xid = getpid() ^ now.tv_sec ^ now.tv_usec;
#endif
    call_msg.rm_xid = cu->mcu_xid;
    call_msg.rm_direction = CALL;
    call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
    call_msg.rm_call.cb_prog = program;
    call_msg.rm_call.cb_vers = version;
    for(i = 0; i < numasync; i++)
    {
	xdrmem_create(&(cu->mcu_scu[i].scu_outxdrs),
		      cu->mcu_scu[i].scu_outbuf,
		      sendsz, XDR_ENCODE);

	if (! xdr_callhdr(&(cu->mcu_scu[i].scu_outxdrs), &call_msg))
	{
	    goto fooy;
	}
	cu->mcu_scu[i].scu_xdrpos = XDR_GETPOS(&(cu->mcu_scu[i].scu_outxdrs));
    }
    if (*sockp < 0)
    {
	int dontblock = 1;

	*sockp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*sockp < 0)
	{
	    rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	    rpc_createerr.cf_error.re_errno = errno;
	    goto fooy;
	}
	/* attempt to bind to prov port */
	(void)bindresvport(*sockp, (struct sockaddr_in *)0);
	/* the sockets rpc controls are non-blocking */
#ifdef AMITCP
	(void)IoctlSocket(*sockp, FIONBIO, (char *) &dontblock);
#else
	(void)ioctl(*sockp, FIONBIO, (char *) &dontblock);
#endif
#if 0
	if(1)
	{
	    int size = 32768;
	    if(setsockopt(*sockp, SOL_SOCKET, SO_RCVBUF, (caddr_t) &size,
		       sizeof(size)) < 0)
	    {
		AKDEBUG((2,"setsockopt() rcvbuf failed\n"));
	    }
	    if(setsockopt(*sockp, SOL_SOCKET, SO_SNDBUF, (caddr_t) &size,
		       sizeof(size)) < 0)
	    {
		AKDEBUG((2,"setsockopt() sndbuf failed\n"));
	    }
	}
#endif
	cu->mcu_closeit = TRUE;
    } else {
	cu->mcu_closeit = FALSE;
    }
    cu->mcu_sock = *sockp;
    cl->cla_clnt.cl_auth = authnone_create();
    return ((CLIENT *) cl);
 fooy:
    if (cu)
    {
	if(cu->mcu_outbuf)
	    mem_free((caddr_t)cu->mcu_outbuf, numasync * sendsz);
	mem_free((caddr_t)cu, sizeof(*cu) + recvsz);
    }
    if (cl)
	mem_free((caddr_t)cl, sizeof(CLIENT));
    return ((CLIENT *)NULL);
}

CLIENT *
clntaudp_create(struct sockaddr_in *raddr, u_long program, u_long version,
		struct timeval wait, register int *sockp, u_int numasync)
{

    return(clntaudp_bufcreate(raddr, program, version, wait, sockp,
			      UDPMSGSIZE, UDPMSGSIZE, numasync));
}

#ifdef FD_SETSIZE
#define XFD_ZERO(maskp) FD_ZERO(maskp)
#define XFD_SET(s, maskp) FD_SET(s, maskp)
typedef fd_set xfd_set;
#else
#define XFD_ZERO(masp) 		(*(maskp) = 0)
#define XFD_SET(s, maskp)    	*(maskp) = 1 << (s)
typedef int xfd_set;
#endif

static enum clnt_stat 
clntaudp_call(register CLIENT	*cl,		/* client handle */
	      u_long		proc,		/* procedure number */
	      xdrproc_t		xargs,		/* xdr routine for args */
	      caddr_t		argsp,		/* pointer to args */
	      xdrproc_t		xresults,	/* xdr routine for results */
	      caddr_t		resultsp,	/* pointer to results */
	      struct timeval	utimeout	/* seconds to wait before giving up */
	      )
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;
    register XDR *xdrs;
    register int outlen;
    register int inlen;
#ifdef AMITCP
    long fromlen;
#else
    int fromlen;
#endif
    xfd_set readfds;
    xfd_set mask;
    struct sockaddr_in from;
    struct rpc_msg reply_msg;
    XDR reply_xdrs;
    struct timeval time_waited;
    bool_t ok;
    int nrefreshes = 2;		/* number of times to refresh cred */
#ifdef AMITCP
    struct timeval timeout, t_temp;
#else
    struct timeval timeout;
#endif
    if (cu->mcu_total.tv_usec == -1)
    {
	timeout = utimeout;     /* use supplied timeout */
    }
    else
    {
	timeout = cu->mcu_total; /* use default timeout */
    }

    time_waited.tv_sec = 0;
    time_waited.tv_usec = 0;
call_again:
    xdrs = &(cu->mcu_scu[0].scu_outxdrs);
    xdrs->x_op = XDR_ENCODE;
    XDR_SETPOS(xdrs, cu->mcu_scu[0].scu_xdrpos);
    /*
     * the transaction is the first thing in the out buffer
     *
     * Note: original clnt_udp.c modifies only the higher
     *       word of the transactions id, so we do.
     */
    (*(u_short *)(&cu->mcu_xid))++;
    *(u_long *)(cu->mcu_scu[0].scu_outbuf) = cu->mcu_xid;
    
    if ((! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	(! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
	(! (*xargs)(xdrs, argsp)))
	return (cu->mcu_error.re_status = RPC_CANTENCODEARGS);
    outlen = (int)XDR_GETPOS(xdrs);

send_again:
    if (sendto(cu->mcu_sock, cu->mcu_scu[0].scu_outbuf, outlen, 0,
	       (struct sockaddr *)&(cu->mcu_raddr), cu->mcu_rlen)
	!= outlen)
    {
	cu->mcu_error.re_errno = errno;
	return (cu->mcu_error.re_status = RPC_CANTSEND);
    }

    /*
     * Hack to provide rpc-based message passing
     */
    if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
    {
	return (cu->mcu_error.re_status = RPC_TIMEDOUT);
    }
    /*
     * sub-optimal code appears here because we have
     * some clock time to spare while the packets are in flight.
     * (We assume that this is actually only executed once.)
     */
    reply_msg.acpted_rply.ar_verf = _null_auth;
    reply_msg.acpted_rply.ar_results.where = resultsp;
    reply_msg.acpted_rply.ar_results.proc = xresults;

    XFD_ZERO(&mask);
    XFD_SET(cu->mcu_sock, &mask);
    for (;;)
    {
	readfds = mask;
	switch (select(_rpc_dtablesize(), &readfds, NULL, 
		       NULL, &(t_temp = cu->mcu_wait)))
	{
	    
    case 0:
	    time_waited.tv_sec += cu->mcu_wait.tv_sec;
	    time_waited.tv_usec += cu->mcu_wait.tv_usec;
	    while (time_waited.tv_usec >= 1000000)
	    {
		time_waited.tv_sec++;
		time_waited.tv_usec -= 1000000;
	    }
	    if ((time_waited.tv_sec < timeout.tv_sec) ||
		((time_waited.tv_sec == timeout.tv_sec) &&
		 (time_waited.tv_usec < timeout.tv_usec)))
		goto send_again;	
	    return (cu->mcu_error.re_status = RPC_TIMEDOUT);

	    /*
	     * buggy in other cases because time_waited is not being
	     * updated.
	     */
    case -1:
#ifndef AMITCP			/* EINTR is returned in case of a CTRL-C by default */
	    if (errno == EINTR)
		continue;	
#endif
	    cu->mcu_error.re_errno = errno;
	    return (cu->mcu_error.re_status = RPC_CANTRECV);
	}
#ifndef AMITCP			/* EINTR is returned in case of a CTRL-C by default */
	do {
#endif
	    fromlen = sizeof(struct sockaddr);
	    inlen = recvfrom(cu->mcu_sock, cu->mcu_inbuf, 
			     (int) cu->mcu_recvsz, 0,
			     (struct sockaddr *)&from, &fromlen);
#ifndef AMITCP /* EINTR is returned in case of a CTRL-C by default */
	} while (inlen < 0 && errno == EINTR);
#endif
	if (inlen < 0) {
	    if (errno == EWOULDBLOCK)
		continue;	
	    cu->mcu_error.re_errno = errno;
	    return (cu->mcu_error.re_status = RPC_CANTRECV);
	}
	if (inlen < sizeof(u_long))
	    continue;	
	/* see if reply transaction id matches sent id */
	if (*((u_long *)(cu->mcu_inbuf)) != *((u_long *)(cu->mcu_scu[0].scu_outbuf)))
	    continue;	
	/* we now assume we have the proper reply */
	break;
    }

    /*
     * now decode and validate the response
     */
    xdrmem_create(&reply_xdrs, cu->mcu_inbuf, (u_int)inlen, XDR_DECODE);
    ok = xdr_replymsg(&reply_xdrs, &reply_msg);
    /* XDR_DESTROY(&reply_xdrs);  save a few cycles on noop destroy */
    if (ok)
    {
	_seterr_reply(&reply_msg, &(cu->mcu_error));
	if (cu->mcu_error.re_status == RPC_SUCCESS)
	{
	    if (! AUTH_VALIDATE(cl->cl_auth,
				&reply_msg.acpted_rply.ar_verf))
	    {
		cu->mcu_error.re_status = RPC_AUTHERROR;
		cu->mcu_error.re_why = AUTH_INVALIDRESP;
	    }
	    if (reply_msg.acpted_rply.ar_verf.oa_base != NULL)
	    {
		xdrs->x_op = XDR_FREE;
		(void)xdr_opaque_auth(xdrs,
				      &(reply_msg.acpted_rply.ar_verf));
	    } 
	}			/* end successful completion */
	else
	{
	    /* maybe our credentials need to be refreshed ... */
	    if (nrefreshes > 0 && AUTH_REFRESH(cl->cl_auth))
	    {
		nrefreshes--;
		goto call_again;
	    }
	}			/* end of unsuccessful completion */
    }				/* end of valid reply message */
    else
    {
	cu->mcu_error.re_status = RPC_CANTDECODERES;
    }
    return (cu->mcu_error.re_status);
}

/*
 * Note: Returning RPC_TIMEDOUT means o.k.
 */

static enum clnt_stat 
clntaudp_send(register CLIENT	*cl,		/* client handle */
	      u_long		proc,		/* procedure number */
	      xdrproc_t		xargs,		/* xdr routine for args */
	      caddr_t		argsp,		/* pointer to args */
	      xdrproc_t		xresults,	/* xdr routine for results */
	      caddr_t		resultsp,	/* pointer to results */
	      caddr_t		userdata	/* user private data */
	      )
{
    return clntaudp_sendsb(cl, proc, xargs, argsp, xresults, resultsp,
			   userdata, NULL, 0);
}

static enum clnt_stat 
clntaudp_sendsb(register CLIENT	*cl,		/* client handle */
		u_long		proc,		/* procedure number */
		xdrproc_t	xargs,		/* xdr routine for args */
		caddr_t		argsp,		/* pointer to args */
		xdrproc_t	xresults,	/* xdr routine for results */
		caddr_t		resultsp,	/* pointer to results */
		caddr_t 	userdata,	/* user private data */
		caddr_t		block,		/* extra data pointer */
		u_int 		blen		/* length of block */
		)
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;
    register struct scu_data *scu;
    register XDR *xdrs;
    register int outlen;
    int i;

    if(cu->mcu_numfreescu == 0)
	return (cu->mcu_error.re_status = RPC_CANTENCODEARGS);

    for(i = 0; i < cu->mcu_numscu; i++)
    {
	scu = &cu->mcu_scu[i];
	if(scu->scu_inuse == 0)
	    break;
    }
    /* user errors */
    /* no free async handle */
    if(i == cu->mcu_numscu) 
	return (cu->mcu_error.re_status = RPC_CANTENCODEARGS);
    
    /* must be always the same xresultsp */
    if(cu->mcu_numfreescu == cu->mcu_numscu)
	cu->mcu_xresults = xresults;
    else
    {
	if(cu->mcu_xresults != xresults)
	    return (cu->mcu_error.re_status = RPC_CANTDECODEARGS);
    }
    
    scu->scu_resultsp = resultsp;
    scu->scu_userdata = userdata;
    
    scu->scu_time_waited.tv_sec = 0;
    scu->scu_time_waited.tv_usec = 0;

    scu->scu_nrefreshes = 2;
call_again:
    xdrs = &(scu->scu_outxdrs);
    xdrs->x_op = XDR_ENCODE;
    XDR_SETPOS(xdrs, scu->scu_xdrpos);
    /*
     * the transaction is the first thing in the out buffer
     *
     * Note: original clnt_udp.c modifies only the higher
     *       word of the transactions id, so we do.
     */
    (*(u_short *)(&cu->mcu_xid))++;
    *(u_long *)(scu->scu_outbuf) = cu->mcu_xid;

    if ((! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	(! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
	(! (*xargs)(xdrs, argsp)))
	return (cu->mcu_error.re_status = RPC_CANTENCODEARGS);
    outlen = (int)XDR_GETPOS(xdrs);

    AKDEBUG((0,"send: xid = %08lx, outlen = %ld\n", cu->mcu_xid, outlen));

send_again:
    if(block && blen)
    {
	/* FIXME: must remember block for send_again in wait */
	struct msghdr msg; /* FIXME: add to mcu ! */
	struct iovec vec[2];

	msg.msg_name = (caddr_t) &(cu->mcu_raddr);
	msg.msg_namelen = cu->mcu_rlen;
	msg.msg_iov = &vec[0];
	msg.msg_iovlen = 2;
	msg.msg_control = NULL;

	vec[0].iov_base = scu->scu_outbuf;
	vec[0].iov_len = outlen;
	vec[1].iov_base = block;
	vec[1].iov_len = blen;

	outlen += blen;
	if(sendmsg(cu->mcu_sock, &msg, 0) != outlen)
	{
	    cu->mcu_error.re_errno = errno;
	    return (cu->mcu_error.re_status = RPC_CANTSEND);
	}
    }
    else
    {
	if (sendto(cu->mcu_sock, scu->scu_outbuf, outlen, 0,
		   (struct sockaddr *)&(cu->mcu_raddr), cu->mcu_rlen)
	    != outlen)
	{
	    cu->mcu_error.re_errno = errno;
	    return (cu->mcu_error.re_status = RPC_CANTSEND);
	}
    }
    scu->scu_inuse = 1;
    cu->mcu_numfreescu--;
    
    return (cu->mcu_error.re_status = RPC_TIMEDOUT);
}

#ifdef AMIGA
__inline
#endif
static void
add_timeval(struct timeval *t1, struct timeval *t2)
{
    t1->tv_sec += t2->tv_sec;
    t1->tv_usec += t2->tv_usec;
		    
    while (t1->tv_usec >= 1000000)
    {
	t1->tv_sec++;
	t1->tv_usec -= 1000000;
    }
}

#define less_timeval(t1,t2) (((t1).tv_sec < (t2).tv_sec) || \
			     (((t1).tv_sec == (t2).tv_sec) && \
			      ((t1).tv_usec < (t2).tv_usec)))

/*
 * Does only wait for one result, must be called for each send call 
 */

static enum clnt_stat 
clntaudp_wait(register CLIENT	*cl,		/* client handle */
	      struct timeval	utimeout	/* seconds to wait before giving up */
	      )
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;
    register struct scu_data *scu;
    register XDR *xdrs;
#if 1
    register int outlen;
#endif
    register int inlen;
    struct CLIENT_AUDP *cla = (CLIENT_AUDP *) cl;
#ifdef AMITCP
    long fromlen;
#else
    int fromlen;
#endif
    xfd_set readfds;
    xfd_set mask;
    struct sockaddr_in from;
    struct rpc_msg reply_msg;
    XDR reply_xdrs;
    bool_t ok;
    int i;
#ifdef AMITCP
    struct timeval timeout, t_temp;
#else
    struct timeval timeout;
#endif
    if (cu->mcu_total.tv_usec == -1)
    {
	timeout = utimeout;     /* use supplied timeout */
    }
    else
    {
	timeout = cu->mcu_total; /* use default timeout */
    }
    
    if(0)
    {
call_again:
#if 0 /* FIXME, credentials may have been refreshed, so we need to create packet again */
	xdrs = &(cu->mcu_scu[0].scu_outxdrs);
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, cu->mcu_scu[0].scu_xdrpos);
	/*
	 * the transaction is the first thing in the out buffer
	 *
	 * Note: original clnt_udp.c modifies only the higher
	 *       word of the transactions id, so we do.
	 */
	(*(u_short *)(&cu->mcu_xid))++;
	*(u_long *)(cu->mcu_scu[0].scu_outbuf) = cu->mcu_xid;
    
	if ((! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	    (! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
	    (! (*xargs)(xdrs, argsp)))
#endif
	    return (cu->mcu_error.re_status = RPC_CANTENCODEARGS);
#if 0
#endif
	
send_again:
	xdrs = &(scu->scu_outxdrs);
	outlen = (int)XDR_GETPOS(xdrs); /* FIXME: remember in scu struct */
	AKDEBUG((0,"Send_again: outlen = %ld\n", outlen));
	if (sendto(cu->mcu_sock, scu->scu_outbuf, outlen, 0,
		   (struct sockaddr *)&(cu->mcu_raddr), cu->mcu_rlen)
	    != outlen)
	{
	    cu->mcu_error.re_errno = errno;
	    return (cu->mcu_error.re_status = RPC_CANTSEND);
	}

#if 0
	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
	{
	    return (cu->mcu_error.re_status = RPC_TIMEDOUT);
	}
#endif
    }
    /*
     * sub-optimal code appears here because we have
     * some clock time to spare while the packets are in flight.
     * (We assume that this is actually only executed once.)
     */
    reply_msg.acpted_rply.ar_verf = _null_auth;
    reply_msg.acpted_rply.ar_results.proc = cu->mcu_xresults;

    XFD_ZERO(&mask);
    XFD_SET(cu->mcu_sock, &mask);
    for (;;)
    {
	readfds = mask;
	switch (select(_rpc_dtablesize(), &readfds, NULL, 
		       NULL, &(t_temp = cu->mcu_wait)))
	{
	    
    case 0:
	    AKDEBUG((2,"wait: TO\n"));
	    for(i = 0; i < cu->mcu_numscu; i++)
	    {
		scu = &cu->mcu_scu[i];
		if(scu->scu_inuse)
		{
		    add_timeval(&scu->scu_time_waited, &cu->mcu_wait);

		    if(less_timeval(scu->scu_time_waited, timeout))
			goto send_again;

		    /* ask user via callback for extended timeout */
		    if(cu->mcu_timeout_cb.cla_cb &&
		       /* FIXME: make MACRO, add timeout parameter to create */
		       cu->mcu_timeout_cb.cla_cb(cla, &cu->mcu_timeout_cb)) 
		    {
#if 0 /* before re-activating this scu must be saved! */
			/* reset all timeouts */
			for(i = 0; i < cu->mcu_numscu; i++)
			{
			    scu = &cu->mcu_scu[i];
			    scu->scu_time_waited.tv_sec = 0;
			    scu->scu_time_waited.tv_usec = 0;
			}
#endif
			goto send_again;
		    }
		}
	    }
		       
	    return (cu->mcu_error.re_status = RPC_TIMEDOUT);

	    /*
	     * buggy in other cases because time_waited is not being
	     * updated.
	     */
    case -1:
	    AKDEBUG((2,"wait: -1, errno = %ld\n", errno));
#ifndef AMITCP			/* EINTR is returned in case of a CTRL-C by default */
	    if (errno == EINTR)
		continue;	
#endif
	    cu->mcu_error.re_errno = errno;
	    return (cu->mcu_error.re_status = RPC_CANTRECV);
	}
#ifndef AMITCP			/* EINTR is returned in case of a CTRL-C by default */
	do {
#endif
	    fromlen = sizeof(struct sockaddr);
	    inlen = recvfrom(cu->mcu_sock, cu->mcu_inbuf, 
			     (int) cu->mcu_recvsz, 0,
			     (struct sockaddr *)&from, &fromlen);
#ifndef AMITCP /* EINTR is returned in case of a CTRL-C by default */
	} while (inlen < 0 && errno == EINTR);
#endif
	if (inlen < 0) {
	    if (errno == EWOULDBLOCK)
		continue;	
	    cu->mcu_error.re_errno = errno;
	    return (cu->mcu_error.re_status = RPC_CANTRECV);
	}
	if (inlen < sizeof(u_long))
	    continue;
	AKDEBUG((0,"wait: xid = %08lx\n", *((u_long *)(cu->mcu_inbuf))));
	/* see if one reply transaction id matches sent id */
	for(i = 0; i < cu->mcu_numscu; i++)
	{
	    scu = &cu->mcu_scu[i];
	    if(scu->scu_inuse && 
	       (*((u_long *)(cu->mcu_inbuf))
		== *((u_long *)(scu->scu_outbuf))))
		break;
	}
	/* no id found */
	if(i == cu->mcu_numscu)
	{
	    AKDEBUG((2,"wait: xid not found\n"));
	    continue;
	}
	AKDEBUG((0,"wait: xid found: %ld\n", i));
	/* we now assume we have one proper reply */
	break;
    }

    reply_msg.acpted_rply.ar_results.where = scu->scu_resultsp;
    cla->cla_resp = scu->scu_resultsp;
    cla->cla_userdata = scu->scu_userdata;
	
    AKDEBUG((0,"wait: inlen = %ld\n", inlen));

    /*
     * now decode and validate the response
     */
    xdrmem_create(&reply_xdrs, cu->mcu_inbuf, (u_int)inlen, XDR_DECODE);
    ok = xdr_replymsg(&reply_xdrs, &reply_msg);
    /* XDR_DESTROY(&reply_xdrs);  save a few cycles on noop destroy */
    if (ok)
    {
	_seterr_reply(&reply_msg, &(cu->mcu_error));
	AKDEBUG((0,"ok-status: %ld\n", cu->mcu_error.re_status));
	if (cu->mcu_error.re_status == RPC_SUCCESS)
	{
	    if (! AUTH_VALIDATE(cl->cl_auth,
				&reply_msg.acpted_rply.ar_verf))
	    {
		AKDEBUG((2,"wait: RPC_AUTHERR\n"));
		cu->mcu_error.re_status = RPC_AUTHERROR;
		cu->mcu_error.re_why = AUTH_INVALIDRESP;
	    }
	    if (reply_msg.acpted_rply.ar_verf.oa_base != NULL)
	    {
		/* FIXME: check if below is sufficient */
		xdrs = &(scu->scu_outxdrs);
		xdrs->x_op = XDR_FREE;
		(void)xdr_opaque_auth(xdrs,
				      &(reply_msg.acpted_rply.ar_verf));
	    } 
	}			/* end successful completion */
	else
	{
	    AKDEBUG((2,"wait: try refresh creds\n"));
	    /* maybe our credentials need to be refreshed ... */
	    if (scu->scu_nrefreshes > 0 && AUTH_REFRESH(cl->cl_auth))
	    {
		scu->scu_nrefreshes--;
		goto call_again;
	    }
	}			/* end of unsuccessful completion */
    }				/* end of valid reply message */
    else
    {
	AKDEBUG((2,"wait: RPC_CANTDECODERES\n"));
	cu->mcu_error.re_status = RPC_CANTDECODERES;
    }
    scu->scu_inuse = 0;
    cu->mcu_numfreescu++;
    
    AKDEBUG((0,"wait: ret-status: %ld\n", cu->mcu_error.re_status));

    return (cu->mcu_error.re_status);
}

static void
clntaudp_geterr(CLIENT *cl, struct rpc_err *errp)
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;

    *errp = cu->mcu_error;
}


static bool_t
clntaudp_freeres(CLIENT *cl,
		xdrproc_t xdr_res,
		caddr_t res_ptr)
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;
#if 0
    register XDR *xdrs = &(cu->cu_outxdrs);

    /* FIXME for all results */
    
    xdrs->x_op = XDR_FREE;

    return ((*xdr_res)(xdrs, res_ptr));
#endif
    return FALSE;
}

static void 
clntaudp_abort(CLIENT *cl)
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;
    register struct scu_data *scu;
    register int i;
    
    for(i = 0; i < cu->mcu_numscu; i++)
    {
	scu = &cu->mcu_scu[i];
	if(scu->scu_inuse != 0)
	{
	    cu->mcu_numfreescu++;
	    scu->scu_inuse = 0;
	}
    }
}

static bool_t
clntaudp_control(CLIENT *cl,
		 u_int request,
		 char *info)
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;

    switch (request) {
case CLSET_TIMEOUT:
	cu->mcu_total = *(struct timeval *)info;
	break;
case CLGET_TIMEOUT:
	*(struct timeval *)info = cu->mcu_total;
	break;
case CLSET_RETRY_TIMEOUT:
	cu->mcu_wait = *(struct timeval *)info;
	break;
case CLGET_RETRY_TIMEOUT:
	*(struct timeval *)info = cu->mcu_wait;
	break;
case CLGET_SERVER_ADDR:
	*(struct sockaddr_in *)info = cu->mcu_raddr;
	break;
default:
	return (FALSE);
    }
    return (TRUE);
}
	
static void
clntaudp_destroy(CLIENT *cl)
{
    register struct mcu_data *cu = (struct mcu_data *)cl->cl_private;
    register int i;

    if (cu->mcu_closeit)
    {
#ifdef AMITCP
	(void)CloseSocket(cu->mcu_sock);
#else
	(void)close(cu->mcu_sock);
#endif
    }
    for(i = 0; i < cu->mcu_numscu; i++)
    {
	XDR_DESTROY(&(cu->mcu_scu[i].scu_outxdrs));
    }
    mem_free((caddr_t)cu->mcu_outbuf, (cu->mcu_sendsz * cu->mcu_numscu));
    mem_free((caddr_t)cu, (sizeof(*cu) + cu->mcu_recvsz));
    mem_free((caddr_t)cl, sizeof(CLIENT_AUDP));
}
