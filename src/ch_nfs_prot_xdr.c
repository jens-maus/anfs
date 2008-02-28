

#include <limits.h>

#include "nfs_prot.h"
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
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
/*
 * Copyright (c) 1987, 1990 by Sun Microsystems, Inc.
 */

/*
 * constants specific to the xdr "protocol"
 */
#define XDR_FALSE	((long) 0)
#define XDR_TRUE	((long) 1)

#ifndef USE_RPCLIB
#define LOCAL static __inline
#else
#define LOCAL
#endif

#ifndef USE_RPCLIB
#undef XDR_PUTLONG
#define XDR_PUTLONG xdrmem_putlong
#undef XDR_GETLONG
#define XDR_GETLONG xdrmem_getlong

#undef XDR_PUTBYTES
#define XDR_PUTBYTES xdrmem_putbytes
#undef XDR_GETBYTES
#define XDR_GETBYTES xdrmem_getbytes

#undef XDR_INLINE
#define XDR_INLINE xdrmem_inline
#endif

#define XDR_ROUND(l) (((l+BYTES_PER_XDR_UNIT-1)/BYTES_PER_XDR_UNIT)*BYTES_PER_XDR_UNIT)

#define IXDR_PUT_BYTES(buf, from, num) memcpy(buf, from, XDR_ROUND(num)); buf+=XDR_ROUND(num)
#define IXDR_GET_BYTES(buf, to, num) memcpy(to, buf, XDR_ROUND(num)); buf+=XDR_ROUND(num)
    
#ifndef USE_RPCLIB
LOCAL long *
xdrmem_inline(register XDR *xdrs, u_int len)
{
    long *buf = 0;

    if (xdrs->x_handy >= len) {
	xdrs->x_handy -= len;
	buf = (long *) xdrs->x_private;
	xdrs->x_private += len;
    }
    return (buf);
}

LOCAL bool_t
xdrmem_getlong(register XDR *xdrs, long *lp)
{
    if ((xdrs->x_handy -= sizeof(long)) < 0)
	return (FALSE);
    *lp = (long)ntohl((u_long)(*((long *)(xdrs->x_private))));
    xdrs->x_private += sizeof(long);
    return (TRUE);
}

LOCAL bool_t
xdrmem_putlong(register XDR *xdrs, long *lp)
{
    if ((xdrs->x_handy -= sizeof(long)) < 0)
	return (FALSE);
    *(long *)xdrs->x_private = (long)htonl((u_long)(*lp));
    xdrs->x_private += sizeof(long);
    return (TRUE);
}

LOCAL bool_t
xdrmem_getbytes(register XDR *xdrs, caddr_t addr, register u_int len)
{
    if ((xdrs->x_handy -= len) < 0)
	return (FALSE);
    bcopy(xdrs->x_private, addr, len);
    xdrs->x_private += len;
    return (TRUE);
}

LOCAL bool_t
xdrmem_putbytes(register XDR *xdrs, caddr_t addr, register u_int len)
{

	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	bcopy(addr, xdrs->x_private, len);
	xdrs->x_private += len;
	return (TRUE);
}
#endif

/*
 * for unit alignment
 */
static char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

#ifndef USE_RPCLIB
/*
 * XDR unsigned long integers
 * same as xdr_long - open coded to save a proc call!
 */
LOCAL
bool_t XDRFUN
xdr_u_long(register XDR *xdrs, u_long *ulp)
{
    if (xdrs->x_op == XDR_DECODE)
	return (XDR_GETLONG(xdrs, (long *)ulp));
    if (xdrs->x_op == XDR_ENCODE)
	return (XDR_PUTLONG(xdrs, (long *)ulp));
    if (xdrs->x_op == XDR_FREE)
	return (TRUE);
    return (FALSE);
}
#endif

/*
 * XDR unsigned integers
 */

#define xdr_u_int(xdrs,up) xdr_u_long(xdrs, (u_long *)up)

#ifndef USE_RPCLIB
/*
 * XDR long integers
 * same as xdr_u_long - open coded to save a proc call!
 */
LOCAL
bool_t XDRFUN
xdr_long(register XDR *xdrs, long *lp)
{
    if (xdrs->x_op == XDR_ENCODE)
	return (XDR_PUTLONG(xdrs, lp));

    if (xdrs->x_op == XDR_DECODE)
	return (XDR_GETLONG(xdrs, lp));

    if (xdrs->x_op == XDR_FREE)
	return (TRUE);

    return (FALSE);
}
#endif

/*
 * XDR long integers
 * same as xdr_u_long - open coded to save a proc call!
 */
#define xdr_long_encode XDR_PUTLONG
#define xdr_long_decode XDR_GETLONG

#ifndef USE_RPCLIB
/*
 * XDR nothing
 */
LOCAL
bool_t XDRFUN
xdr_void(XDR * xdrs, void *dummy)
{
	return (TRUE);
}
#endif

#ifndef USE_RPCLIB
/*
 * XDR booleans
 */
LOCAL
bool_t XDRFUN
xdr_bool(register XDR *xdrs, bool_t *bp)
{
    long lb;

    switch (xdrs->x_op) {

case XDR_ENCODE:
	lb = *bp ? XDR_TRUE : XDR_FALSE;
	return (XDR_PUTLONG(xdrs, &lb));

case XDR_DECODE:
	if (!XDR_GETLONG(xdrs, &lb)) {
	    return (FALSE);
	}
	*bp = (lb == XDR_FALSE) ? FALSE : TRUE;
	return (TRUE);

case XDR_FREE:
	return (TRUE);
    }
    return (FALSE);
}
#endif
#ifndef USE_RPCLIB
/*
 * XDR enumerations
 */
LOCAL 
bool_t XDRFUN
xdr_enum(XDR *xdrs, enum_t *ep)
{
    enum sizecheck { SIZEVAL };	/* used to find the size of an enum */

    /*
     * enums are treated as ints
     */
    if (sizeof (enum sizecheck) == sizeof (long)) {
	return (xdr_long(xdrs, (long *)ep));
    } else if (sizeof (enum sizecheck) == sizeof (short)) {
	return (xdr_short(xdrs, (short *)ep));
    } else {
	return (FALSE);
    }
}
#endif

/*
 * XDR enumerations
 */

#ifndef USE_RPCLIB
/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
LOCAL
bool_t XDRFUN
xdr_opaque(register XDR *xdrs, caddr_t cp, register u_int cnt)
{
    register u_int rndup;
    static char crud[BYTES_PER_XDR_UNIT];

    /*
     * if no data we are done
     */
    if (cnt == 0)
	return (TRUE);

    /*
     * round byte count to full xdr units
     */
    rndup = cnt % BYTES_PER_XDR_UNIT;
    if (rndup > 0)
	rndup = BYTES_PER_XDR_UNIT - rndup;

    if (xdrs->x_op == XDR_DECODE) {
	if (!XDR_GETBYTES(xdrs, cp, cnt)) {
	    return (FALSE);
	}
	if (rndup == 0)
	    return (TRUE);
	return (XDR_GETBYTES(xdrs, crud, rndup));
    }

    if (xdrs->x_op == XDR_ENCODE) {
	if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
	    return (FALSE);
	}
	if (rndup == 0)
	    return (TRUE);
	return (XDR_PUTBYTES(xdrs, xdr_zero, rndup));
    }

    if (xdrs->x_op == XDR_FREE) {
	return (TRUE);
    }

    return (FALSE);
}
#endif

#ifndef USE_RPCLIB
/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
LOCAL
bool_t XDRFUN
xdr_bytes(register XDR *xdrs,
	char **cpp,
	register u_int *sizep,
	u_int maxsize)
{
    register char *sp = *cpp;	/* sp is the actual string pointer */
    register u_int nodesize;

    /*
     * first deal with the length since xdr bytes are counted
     */
    if (! xdr_u_int(xdrs, sizep)) {
	return (FALSE);
    }
    nodesize = *sizep;
    if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE)) {
	return (FALSE);
    }

    /*
     * now deal with the actual bytes
     */
    	switch (xdrs->x_op) {

    case XDR_DECODE:
	    if (nodesize == 0) {
		return (TRUE);
	    }
	    if (sp == NULL) {
		*cpp = sp = (char *)mem_alloc(nodesize);
	    }
	    if (sp == NULL) {
		(void) fprintf(stderr, "xdr_bytes: out of memory\n");
		return (FALSE);
	    }
	    /* fall into ... */

    case XDR_ENCODE:
	    return (xdr_opaque(xdrs, sp, nodesize));

    case XDR_FREE:
	    if (sp != NULL) {
		mem_free(sp, nodesize);
		*cpp = NULL;
	    }
	    return (TRUE);
	}
    return (FALSE);
}
#endif

#ifndef USE_RPCLIB
/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
LOCAL 
bool_t XDRFUN
xdr_string(register XDR *xdrs, char **cpp, u_int maxsize)
{
    register char *sp = *cpp;	/* sp is the actual string pointer */
    u_int size;
    u_int nodesize;

    /*
     * first deal with the length since xdr strings are counted-strings
     */
    switch (xdrs->x_op) {
case XDR_FREE:
	if (sp == NULL) {
	    return(TRUE);	/* already free */
	}
	/* fall through... */
case XDR_ENCODE:
	size = strlen(sp);
	break;
    }
    if (! xdr_u_int(xdrs, &size)) {
	return (FALSE);
    }
    if (size > maxsize) {
	return (FALSE);
    }
    nodesize = size + 1;

    /*
     * now deal with the actual bytes
     */
    switch (xdrs->x_op) {

case XDR_DECODE:
	if (nodesize == 0) {
	    return (TRUE);
	}
	if (sp == NULL)
	    *cpp = sp = (char *)mem_alloc(nodesize);
	if (sp == NULL) {
	    (void) fprintf(stderr, "xdr_string: out of memory\n");
	    return (FALSE);
	}
	sp[size] = 0;
	/* fall into ... */

case XDR_ENCODE:
	return (xdr_opaque(xdrs, sp, size));

case XDR_FREE:
	mem_free(sp, nodesize);
	*cpp = NULL;
	return (TRUE);
    }
    return (FALSE);
}
#endif

#ifndef USE_RPCLIB
/*
 * XDR an indirect pointer
 * xdr_reference is for recursively translating a structure that is
 * referenced by a pointer inside the structure that is currently being
 * translated.  pp references a pointer to storage. If *pp is null
 * the  necessary storage is allocated.
 * size is the sizeof the referneced structure.
 * proc is the routine to handle the referenced structure.
 */
LOCAL
bool_t XDRFUN
xdr_reference(xdrs, pp, size, proc)
	register XDR *xdrs;
	caddr_t *pp;		/* the pointer to work on */
	u_int size;		/* size of the object pointed to */
	xdrproc_t proc;		/* xdr routine to handle the object */
{
    register caddr_t loc = *pp;
    register bool_t stat;

    if (loc == NULL)
	switch (xdrs->x_op) {
    case XDR_FREE:
	    return (TRUE);

    case XDR_DECODE:
	    *pp = loc = (caddr_t) mem_alloc(size);
	    if (loc == NULL) {
		(void) fprintf(stderr,
			       "xdr_reference: out of memory\n");
		return (FALSE);
	    }
	    bzero(loc, (int)size);
	    break;
	}

    stat = (*(xdr_string_t)proc)(xdrs, loc, UINT_MAX);

    if (xdrs->x_op == XDR_FREE) {
	mem_free(loc, size);
	*pp = NULL;
    }
    return (stat);
}


/*
 * xdr_pointer():
 *
 * XDR a pointer to a possibly recursive data structure. This
 * differs with xdr_reference in that it can serialize/deserialiaze
 * trees correctly.
 *
 *  What's sent is actually a union:
 *
 *  union object_pointer switch (boolean b) {
 *  case TRUE: object_data data;
 *  case FALSE: void nothing;
 *  }
 *
 * > objpp: Pointer to the pointer to the object.
 * > obj_size: size of the object.
 * > xdr_obj: routine to XDR an object.
 *
 */
LOCAL
bool_t XDRFUN
xdr_pointer(xdrs,objpp,obj_size,xdr_obj)
	register XDR *xdrs;
	char **objpp;
	u_int obj_size;
	xdrproc_t xdr_obj;
{
    bool_t more_data;

    more_data = (*objpp != NULL);
    if (! xdr_bool(xdrs,&more_data)) {
	return (FALSE);
    }
    if (! more_data) {
	*objpp = NULL;
	return (TRUE);
    }
    return (xdr_reference(xdrs,objpp,obj_size,xdr_obj));
}
#endif

/* from @(#)nfs_prot.x	1.3 91/03/11 TIRPC 1.0 */

LOCAL 
bool_t
XDRFUN
xdr_nfsstat(XDR *xdrs, nfsstat *objp)
{
    return xdr_enum(xdrs, (enum_t *)objp);
}

LOCAL 
bool_t
XDRFUN
xdr_ftype(XDR *xdrs, ftype *objp)
{
    return xdr_enum(xdrs, (enum_t *)objp);
}

LOCAL
bool_t
XDRFUN
xdr_nfs_fh(XDR *xdrs, nfs_fh *objp)
{
    return xdr_opaque(xdrs, objp->data, NFS_FHSIZE);
}

LOCAL 
bool_t
XDRFUN
xdr_nfstime(XDR *xdrs, nfstime *objp)
{
    if (!xdr_u_int(xdrs, &objp->seconds)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->useconds)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_fattr(XDR *xdrs, fattr *objp)
{
    register long *buf;

    if (xdrs->x_op == XDR_ENCODE)
    {
	buf = XDR_INLINE(xdrs,17 * BYTES_PER_XDR_UNIT);
	if (buf == NULL) 
	    return FALSE;

	/* xdr_ftype(xdrs, &objp->type) */

	IXDR_PUT_ENUM(buf, objp->type);
	IXDR_PUT_U_LONG(buf,objp->mode);
	IXDR_PUT_U_LONG(buf,objp->nlink);
	IXDR_PUT_U_LONG(buf,objp->uid);
	IXDR_PUT_U_LONG(buf,objp->gid);
	IXDR_PUT_U_LONG(buf,objp->size);
	IXDR_PUT_U_LONG(buf,objp->blocksize);
	IXDR_PUT_U_LONG(buf,objp->rdev);
	IXDR_PUT_U_LONG(buf,objp->blocks);
	IXDR_PUT_U_LONG(buf,objp->fsid);
	IXDR_PUT_U_LONG(buf,objp->fileid);

	/* xdr_nfstime(xdrs, &objp->atime) */
	/* xdr_nfstime(xdrs, &objp->mtime) */
	/* xdr_nfstime(xdrs, &objp->ctime) */

	IXDR_PUT_U_LONG(buf, objp->atime.seconds);
	IXDR_PUT_U_LONG(buf, objp->atime.useconds);
	IXDR_PUT_U_LONG(buf, objp->mtime.seconds);
	IXDR_PUT_U_LONG(buf, objp->mtime.useconds);
	IXDR_PUT_U_LONG(buf, objp->ctime.seconds);
	IXDR_PUT_U_LONG(buf, objp->ctime.useconds);
		    
	return (TRUE);
    }
    else if (xdrs->x_op == XDR_DECODE)
    {
	buf = XDR_INLINE(xdrs,17 * BYTES_PER_XDR_UNIT);
	if (buf == NULL) 
	    return FALSE;

	objp->type = IXDR_GET_ENUM(buf, ftype);
	objp->mode = IXDR_GET_U_LONG(buf);
	objp->nlink = IXDR_GET_U_LONG(buf);
	objp->uid = IXDR_GET_U_LONG(buf);
	objp->gid = IXDR_GET_U_LONG(buf);
	objp->size = IXDR_GET_U_LONG(buf);
	objp->blocksize = IXDR_GET_U_LONG(buf);
	objp->rdev = IXDR_GET_U_LONG(buf);
	objp->blocks = IXDR_GET_U_LONG(buf);
	objp->fsid = IXDR_GET_U_LONG(buf);
	objp->fileid = IXDR_GET_U_LONG(buf);

	objp->atime.seconds = IXDR_GET_U_LONG(buf);
	objp->atime.useconds = IXDR_GET_U_LONG(buf);
	objp->mtime.seconds = IXDR_GET_U_LONG(buf);
	objp->mtime.useconds = IXDR_GET_U_LONG(buf);
	objp->ctime.seconds = IXDR_GET_U_LONG(buf);
	objp->ctime.useconds = IXDR_GET_U_LONG(buf);

	return(TRUE);
    }

    if (!xdr_ftype(xdrs, &objp->type)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->mode)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->nlink)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->uid)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->gid)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->size)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->blocksize)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->rdev)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->blocks)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->fsid)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->fileid)) {
	return (FALSE);
    }
    if (!xdr_nfstime(xdrs, &objp->atime)) {
	return (FALSE);
    }
    if (!xdr_nfstime(xdrs, &objp->mtime)) {
	return (FALSE);
    }
    if (!xdr_nfstime(xdrs, &objp->ctime)) {
	return (FALSE);
    }

    return (TRUE);
}

LOCAL bool_t
XDRFUN 
xdr_sattr(XDR *xdrs, sattr *objp)
{
    register long *buf;

    if (xdrs->x_op == XDR_ENCODE)
    {
	buf = XDR_INLINE(xdrs,8 * BYTES_PER_XDR_UNIT);
	if (buf == NULL)
	    return FALSE;

	IXDR_PUT_U_LONG(buf,objp->mode);
	IXDR_PUT_U_LONG(buf,objp->uid);
	IXDR_PUT_U_LONG(buf,objp->gid);
	IXDR_PUT_U_LONG(buf,objp->size);

	IXDR_PUT_U_LONG(buf, objp->atime.seconds);
	IXDR_PUT_U_LONG(buf, objp->atime.useconds);
	IXDR_PUT_U_LONG(buf, objp->mtime.seconds);
	IXDR_PUT_U_LONG(buf, objp->mtime.useconds);

	return (TRUE);
    }
    else if (xdrs->x_op == XDR_DECODE)
    {
	buf = XDR_INLINE(xdrs,8 * BYTES_PER_XDR_UNIT);
	if (buf == NULL)
	    return FALSE;
	
	objp->mode = IXDR_GET_U_LONG(buf);
	objp->uid = IXDR_GET_U_LONG(buf);
	objp->gid = IXDR_GET_U_LONG(buf);
	objp->size = IXDR_GET_U_LONG(buf);

	objp->atime.seconds = IXDR_GET_U_LONG(buf);
	objp->atime.useconds = IXDR_GET_U_LONG(buf);
	objp->mtime.seconds = IXDR_GET_U_LONG(buf);
	objp->mtime.useconds = IXDR_GET_U_LONG(buf);

	return(TRUE);
    }

    if (!xdr_u_int(xdrs, &objp->mode)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->uid)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->gid)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->size)) {
	return (FALSE);
    }
    if (!xdr_nfstime(xdrs, &objp->atime)) {
	return (FALSE);
    }
    if (!xdr_nfstime(xdrs, &objp->mtime)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_filename(XDR *xdrs, filename *objp)
{
    if (!xdr_string(xdrs, objp, NFS_MAXNAMLEN)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_nfspath(XDR *xdrs, nfspath *objp)
{
    if (!xdr_string(xdrs, objp, NFS_MAXPATHLEN)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_attrstat(XDR *xdrs, attrstat *objp)
{
    if (!xdr_nfsstat(xdrs, &objp->status)) {
	return (FALSE);
    }
    switch (objp->status) {
case NFS_OK:
	if (!xdr_fattr(xdrs, &objp->attrstat_u.attributes)) {
	    return (FALSE);
	}
	break;
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_sattrargs(XDR *xdrs, sattrargs *objp)
{
    if (!xdr_nfs_fh(xdrs, &objp->file)) {
	return (FALSE);
    }
    if (!xdr_sattr(xdrs, &objp->attributes)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_diropargs(XDR *xdrs, diropargs *objp)
{
    if (!xdr_nfs_fh(xdrs, &objp->dir)) {
	return (FALSE);
    }
    if (!xdr_filename(xdrs, &objp->name)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_diropokres(XDR *xdrs, diropokres *objp)
{
    if (!xdr_nfs_fh(xdrs, &objp->file)) {
	return (FALSE);
    }
    if (!xdr_fattr(xdrs, &objp->attributes)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_diropres(XDR *xdrs, diropres *objp)
{
    if (!xdr_nfsstat(xdrs, &objp->status)) {
	return (FALSE);
    }
    switch (objp->status) {
case NFS_OK:
	if (!xdr_diropokres(xdrs, &objp->diropres_u.diropres)) {
	    return (FALSE);
	}
	break;
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_readlinkres(XDR *xdrs, readlinkres *objp)
{
    if (!xdr_nfsstat(xdrs, &objp->status)) {
	return (FALSE);
    }
    switch (objp->status) {
case NFS_OK:
	if (!xdr_nfspath(xdrs, &objp->readlinkres_u.data)) {
	    return (FALSE);
	}
	break;
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_readargs(XDR *xdrs, readargs *objp)
{
#if 0
    /*
      xdr_nfs_fh(xdrs, &objp->file)
      xdr_u_int(xdrs, &objp->offset)
      xdr_u_int(xdrs, &objp->count)
      xdr_u_int(xdrs, &objp->totalcount)
    */
    register long *buf;

    if (xdrs->x_op == XDR_ENCODE)
    {
	buf = XDR_INLINE(xdrs,
			 3*BYTES_PER_XDR_UNIT
			 +XDR_ROUND(NFS_FHSIZE));
	if (buf == NULL) 
	    return FALSE;

	IXDR_PUT_BYTES(buf, &objp->file, NFS_FHSIZE);
	IXDR_PUT_U_LONG(buf, objp->offset);
	IXDR_PUT_U_LONG(buf, objp->count);
	IXDR_PUT_U_LONG(buf, objp->totalcount);

	return (TRUE);
    }
    else if (xdrs->x_op == XDR_DECODE)
    {
	buf = XDR_INLINE(xdrs,
			 3*BYTES_PER_XDR_UNIT
			 +XDR_ROUND(NFS_FHSIZE));
	if (buf == NULL) 
	    return FALSE;

	IXDR_GET_BYTES(buf, &objp->file, NFS_FHSIZE);
	objp->offset =	IXDR_GET_U_LONG(buf);
	objp->count = 	IXDR_GET_U_LONG(buf);
	objp->totalcount = IXDR_GET_U_LONG(buf);
	
	return (TRUE);
    }
#endif
    if (!xdr_nfs_fh(xdrs, &objp->file)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->offset)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->count)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->totalcount)) {
	return (FALSE);
    }

    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_readokres(XDR *xdrs, readokres *objp)
{
    if (!xdr_fattr(xdrs, &objp->attributes)) {
	return (FALSE);
    }
    if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
		   (u_int *)&objp->data.data_len, NFS_MAXDATA)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL
bool_t
XDRFUN
xdr_readres(XDR *xdrs, readres *objp)
{
    if (!xdr_nfsstat(xdrs, &objp->status)) {
	return (FALSE);
    }
    switch (objp->status) {
case NFS_OK:
	if (!xdr_fattr(xdrs, &objp->readres_u.reply.attributes)) {
	    return (FALSE);
	}
	if (!xdr_bytes(xdrs, 
		       (char **)&objp->readres_u.reply.data.data_val, 
		       (u_int *)&objp->readres_u.reply.data.data_len, 
		       NFS_MAXDATA)) {
	    return (FALSE);
	}
	break;
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_writeargs(XDR *xdrs, writeargs *objp)
{
    /*
     * xdr_nfs_fh(xdrs, &objp->file)
     * xdr_u_int(xdrs, &objp->beginoffset)
     * xdr_u_int(xdrs, &objp->offset)
     * xdr_u_int(xdrs, &objp->totalcount)
     * xdr_bytes(xdrs, (char **)&objp->data.data_val,
     *		(u_int *)&objp->data.data_len, NFS_MAXDATA)
     */
#if 0
    register long *buf;

    if (xdrs->x_op == XDR_ENCODE)
    {
	buf = XDR_INLINE(xdrs,
			 XDR_ROUND(NFS_FHSIZE)
			 + 3 * BYTES_PER_XDR_UNIT);
	if (buf == NULL)
	    return FALSE;
	
	IXDR_PUT_BYTES(buf, &objp->file, NFS_FHSIZE);
	IXDR_PUT_U_LONG(buf,objp->beginoffset);
	IXDR_PUT_U_LONG(buf,objp->offset);
	IXDR_PUT_U_LONG(buf,objp->totalcount);
	
	if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
		       (u_int *)&objp->data.data_len, NFS_MAXDATA))
	{
	    return (FALSE);
	}

	return (TRUE);
    }
    else if (xdrs->x_op == XDR_DECODE)
    {
	buf = XDR_INLINE(xdrs,
			 XDR_ROUND(NFS_FHSIZE)
			 + 3 * BYTES_PER_XDR_UNIT);
	if (buf == NULL)
	    return (FALSE);

	IXDR_GET_BYTES(buf, &objp->file, NFS_FHSIZE);
	objp->beginoffset = IXDR_GET_U_LONG(buf);
	objp->offset = IXDR_GET_U_LONG(buf);
	objp->totalcount = IXDR_GET_U_LONG(buf);

	if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
		       (u_int *)&objp->data.data_len, NFS_MAXDATA))
	{
	    return (FALSE);
	}
	return(TRUE);
    }
#endif
    if (!xdr_nfs_fh(xdrs, &objp->file)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->beginoffset)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->offset)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->totalcount)) {
	return (FALSE);
    }
    if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
		   (u_int *)&objp->data.data_len, NFS_MAXDATA)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
xdr_writeargssb(XDR *xdrs, writeargs *objp)
{
    if (!xdr_nfs_fh(xdrs, &objp->file)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->beginoffset)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->offset)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->totalcount)) {
	return (FALSE);
    }
#if 1
    if (!xdr_u_int(xdrs, (u_int *)&objp->data.data_len))
    {
	return (FALSE);
    }
#else
    if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
		   (u_int *)&objp->data.data_len, NFS_MAXDATA)) {
	return (FALSE);
    }
#endif
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_createargs(XDR *xdrs, createargs *objp)
{
    if (!xdr_diropargs(xdrs, &objp->where)) {
	return (FALSE);
    }
    if (!xdr_sattr(xdrs, &objp->attributes)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_renameargs(XDR *xdrs, renameargs *objp)
{
    if (!xdr_diropargs(xdrs, &objp->from)) {
	return (FALSE);
    }
    if (!xdr_diropargs(xdrs, &objp->to)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_linkargs(XDR *xdrs, linkargs *objp)
{
    if (!xdr_nfs_fh(xdrs, &objp->from)) {
	return (FALSE);
    }
    if (!xdr_diropargs(xdrs, &objp->to)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_symlinkargs(XDR *xdrs, symlinkargs *objp)
{
    if (!xdr_diropargs(xdrs, &objp->from)) {
	return (FALSE);
    }
    if (!xdr_nfspath(xdrs, &objp->to)) {
	return (FALSE);
    }
    if (!xdr_sattr(xdrs, &objp->attributes)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN 
xdr_nfscookie(XDR *xdrs, nfscookie objp)
{
    if (!xdr_opaque(xdrs, objp, NFS_COOKIESIZE)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_readdirargs(XDR *xdrs, readdirargs *objp)
{
    if (!xdr_nfs_fh(xdrs, &objp->dir)) {
	return (FALSE);
    }
    if (!xdr_nfscookie(xdrs, objp->cookie)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->count)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_entry(XDR *xdrs, entry *objp)
{
    if (!xdr_u_int(xdrs, &objp->fileid)) {
	return (FALSE);
    }
    if (!xdr_filename(xdrs, &objp->name)) {
	return (FALSE);
    }
    if (!xdr_nfscookie(xdrs, objp->cookie)) {
	return (FALSE);
    }
    if (!xdr_pointer(xdrs, (char **)&objp->nextentry, sizeof(entry), (xdrproc_t)xdr_entry)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_dirlist(XDR *xdrs, dirlist *objp)
{
    if (!xdr_pointer(xdrs, (char **)&objp->entries, sizeof(entry), (xdrproc_t)xdr_entry)) {
	return (FALSE);
    }
    if (!xdr_bool(xdrs, &objp->eof)) {
	return (FALSE);
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_readdirres(XDR *xdrs, readdirres *objp)
{
    if (!xdr_nfsstat(xdrs, &objp->status)) {
	return (FALSE);
    }
    switch (objp->status) {
case NFS_OK:
	if (!xdr_dirlist(xdrs, &objp->readdirres_u.reply)) {
	    return (FALSE);
	}
	break;
    }
    return (TRUE);
}

LOCAL 
bool_t
XDRFUN
xdr_statfsokres(XDR *xdrs, statfsokres *objp)
{
    register long *buf;

    if (xdrs->x_op == XDR_ENCODE)
    {
	buf = XDR_INLINE(xdrs,5 * BYTES_PER_XDR_UNIT);
	if (buf == NULL)
	    return FALSE;
	
	IXDR_PUT_U_LONG(buf,objp->tsize);
	IXDR_PUT_U_LONG(buf,objp->bsize);
	IXDR_PUT_U_LONG(buf,objp->blocks);
	IXDR_PUT_U_LONG(buf,objp->bfree);
	IXDR_PUT_U_LONG(buf,objp->bavail);

	return (TRUE);
    }
    else if (xdrs->x_op == XDR_DECODE)
    {
	buf = XDR_INLINE(xdrs,5 * BYTES_PER_XDR_UNIT);
	if (buf == NULL)
	    return FALSE;
	
	objp->tsize = IXDR_GET_U_LONG(buf);
	objp->bsize = IXDR_GET_U_LONG(buf);
	objp->blocks = IXDR_GET_U_LONG(buf);
	objp->bfree = IXDR_GET_U_LONG(buf);
	objp->bavail = IXDR_GET_U_LONG(buf);

	return (TRUE);
    }

    if (!xdr_u_int(xdrs, &objp->tsize)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->bsize)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->blocks)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->bfree)) {
	return (FALSE);
    }
    if (!xdr_u_int(xdrs, &objp->bavail)) {
	return (FALSE);
    }
    
    return (TRUE); 
}

LOCAL 
bool_t
XDRFUN
xdr_statfsres(XDR *xdrs, statfsres *objp)
{

    if (!xdr_nfsstat(xdrs, &objp->status)) {
	return (FALSE);
    }
    switch (objp->status) {
case NFS_OK:
	if (!xdr_statfsokres(xdrs, &objp->statfsres_u.reply)) {
	    return (FALSE);
	}
	break;
    }
    return (TRUE);
}
