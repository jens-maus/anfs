#/***************************************************************************
#
# aNFS (ch_nfs) - Amiga NFS client
# Copyright (C) 1993-1994 Carsten Heyl
# Copyright (C) 2008      aNFS Open Source Team
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# aNFS OpenSource project:  http://sourceforge.net/projects/anfs/
#
# $Id$
#
#***************************************************************************/

# rcs revisions: rcs -nV0_003: rcs/*

# comment the following line for final version
#BETA=1

# uncomment the following for beta testers distribution
#BETADIST=1

CC=sc
LN=sc LINK
CP=copy
RM=delete
RM_FORCE=delete force
CO=co -q

#MKPROTO=cproto -E 0 -Isc:include/ >
MKPROTO=mkproto TO
RPCGEN=amitcp:bin/rpcgen

DISTBASEDIR=:dist
DISTNAME=chnfsc
VERSION=1.04
RCSVERSION=V1_04

ifdef BETA
	VERSION:=$(VERSION)BETA
endif

DISTBASE=$(DISTNAME)_$(VERSION)
DISTDIR=$(DISTBASEDIR)/$(DISTNAME)_$(VERSION)

CDEBFLAGS=OPTIMIZE OPTTIME
CDEBFLAGS+=DEF USE_RPCLIB=1 DEBUG=L #DEF DEBUG=1# DEBUG=FF
CFLAGS= $(CDEBFLAGS) \
	IDIR netinclude: \
	IDIR include: \
	IDIR com-include:hash \
	IDIR com-include:aulib \
	IDIR com-include: \
	IGN 183 

# ADDED IDIR include for speed reasons

# 183: inline function "" declared but not defined

LIBS=LIB netlib:rpcr.lib netlib:netd.lib com-lib:au.lib \
	com-lib:misclib.lib sc:lib/debug.lib sc:lib/ddebug.lib

MNTSRCS=ch_mount_clnt.c ch_mount_xdr.c
NFSSRCS= ch_nfs_prot_xdr.c

# actions
ACTSRCS=lock.c info.c find.c read.c examine.c filesys.c seek.c write.c \
	link.c object.c attr.c iocontrol.c timer.c #auth_unix.c
LSTSRCS=locks.c fh.c dirs.c attrs.c dcache.c wbuf.c rbuf.c mbuf.c cred.c
# Note: all files listed in CHSRCS will go to protos.h
CHSRCS=nfs_handler.c log.c actions.c nfssupp.c fn.c nfs.c supp.c \
	$(LSTSRCS) $(ACTSRCS) dummy.c \
	ch_nfs_prot_clnt.c \
	simplerexx.c
SRCS= common/fthash.c rpc/clnt_audp.c rpc/clnt_ageneric.c \
	$(CHSRCS) $(MNTSRCS) $(NFSSRCS)
OBJS=${SRCS:%.c=%.o} loga.o
LOBJS=#qc:c-src/common/hash/hash/thash.o

GST=handler.gst

all: .comm ch_nfsc

world:
	$(MAKE) all
	$(MAKE) all -C docs
	$(MAKE) all -C ../ch_nfsutil

.comm: SCOPTIONS
	touch .comm

protos: protos.h

# $(OBJS): $(GST)

ch_nfsc.dbg: .comm $(OBJS)
	echo >ch_nfsc.mylnk STARTUP=CRES TO $@ $(OBJS) $(LOBJS) $(LIBS) ADDSYM 
	$(LN) WITH ch_nfsc.mylnk
#MAP MHUNK MSYM MLIB MXREF

ch_nfsc: ch_nfsc.dbg
	slink from $< to $@ STRIPDEBUG
	protect $@ +p

ch_nfsctl: ch_nfsctl.o nfssupp.o simplerexx.o
	$(LN) TO $@ ch_nfsctl.o nfssupp.o simplerexx.o $(LIBS) #MHUNK MSYM MLIB MXREF

doc:
	cd docs; $(MAKE)

SCOPY=slink STRIPDEBUG from 

bindist: ch_nfsc ch_nfsc.dbg ch_nfsctl.rexx
	-delete $(DISTDIR) $(DISTDIR)i all force quiet
	makedir $(DISTDIR) $(DISTDIR)/bin $(DISTDIR)/doc $(DISTDIR)/help
	makedir $(DISTDIR)/db $(DISTDIR)/dvi
	makedir $(DISTDIR)i
	copy ch_nfsctl.rexx to $(DISTDIR)/bin/ch_nfsctl
	protect $(DISTDIR)/bin/ch_nfsctl +s
ifdef BETADIST
	copy docs/short.doc docs/beta-testers.doc docs/what-a-bug-is.doc $(DISTDIR)
else
	copy docs/short.doc $(DISTDIR)
endif
	copy docs/short.doc $(DISTDIR)/db/ch_nfstab
	copy docs/guide_dir_info $(DISTDIR)/help.info
	$(MAKE) bindist -C docs DISTDIR=$(DISTDIR)
	$(MAKE) bindist -C ../ch_nfsutil DISTDIR=$(DISTDIR) 
	makedir $(DISTDIR)/showmount
	copy /showmount/RCS $(DISTDIR)/showmount/RCS
	copy /showmount/showmount.8 $(DISTDIR)/showmount
	copy /showmount/showmount.c $(DISTDIR)/showmount
	copy /showmount/Smakefile $(DISTDIR)/showmount
	copy /showmount/Smake.def $(DISTDIR)/showmount
	copy /showmount/readme.amiga $(DISTDIR)/showmount
	copy /showmount/showmount.man $(DISTDIR)/doc/showmount.doc
	copy /showmount/showmount $(DISTDIR)/bin
	copy ch_nfsc $(DISTDIR)/bin
	copy ch_nfsc.dbg $(DISTDIR)i/ch_nfsc.dbg
	-delete $(DISTDIR).lha
	lha -Qa -N -r a $(DISTDIR).lha $(DISTBASEDIR)/ $(DISTBASE)

ch_nfsctl.o: ch_nfsctl.c ch_nfsctl_rev.h nfsc_iocontrol.h
	$(CC) TO $@ $(CFLAGS) $<

mount_clnt.o: mount.h

mount.h: mount.x
	-$(RM_FORCE) $@
	$(RPCGEN) -h -o $@ $<
mount_clnt.c: mount.x
	-$(RM) $@
	$(RPCGEN) -l -o $@ $<
mount_xdr.c: mount.x
	-$(RM) $@
	$(RPCGEN) -c -o $@ $<
nfs_prot_clnt.o: nfs_prot.h

nfs_prot.h: nfs_prot.x
	-$(RM_FORCE) $@
	$(RPCGEN) -h -o $@ $<
nfs_prot_clnt.c: nfs_prot.x
	-$(RM) $@
	$(RPCGEN) -l -o $@ $<
nfs_prot_xdr.c: nfs_prot.x
	-$(RM) $@
	$(RPCGEN) -c -o $@ $<

protos.h: $(CHSRCS)
	-$(RM_FORCE) $@
	$(MKPROTO) t:proto.tmp $(CHSRCS)
	$(CP) t:proto.tmp $@

supp.h: supp.c
	$(MKPROTO) supp.h supp.c

$(GST): nfs_handler.h attrs.h dcache.h dirs.h fh.h locks.h mount.h \
	nfs_prot.h wbuf.h timer.h rbuf.h mbuf.h xdos.h dos_dostags.h

$(GST): gst.c
	$(CC) NOOBJNAME $(CFLAGS) MAKEGST=$(GST) gst.c
	gst unload $(GST)
	
examine.o: examine.c
	$(CC) $(CFLAGS) $< 

%_rev.h: %.c
	rcsrev $* $<

.c.o: SCOPTIONS
	$(CC) $(CFLAGS) $<

# DISASM=$*.disasm

#GST=$(GST) $<

clean:
	-delete '*.o' '*/*.o' '*!' '*/*!'
	-delete *.disasm *.lnk *.info ch_nfsc.mylnk .comm

rcsclean: clean
	-delete force protos.h supp.h nfs_prot_xdr.c
	-delete TAGS ch_nfsc ch_nfsc.dbg 
	-delete force mount.h nfs_prot.h 
	-rcsclean *.c 
	-rcsclean *.h *.rexx *.x *.asm 
	-rcsclean Changelog GNUmakefile SCOPTIONS TODO
	$(MAKE) rcsclean -C rpc 
	$(MAKE) rcsclean -C common

rcsrev:
	rcs -n$(RCSVERSION): RCS/* rpc/RCS/*

co:
	$(MAKE) co -C rpc 
	
loga.o: loga.asm
	asm loga.asm

ch_nfs_prot_clnt.o: ch_nfs_prot_clnt.c ch_nfs_prot_xdr.c 
	$(CC) $(CFLAGS) IGN 181 IGN 306 $<

%.c: RCS/%.c,v
	co $<
%.h: RCS/%.h,v
	co $<

common/fthash.c:
	co $@

rpc/clnt_ageneric.c rpc/clnt_ageneric.h rpc/clnt_audp.h rpc/clnt_audp.c:
	$(MAKE) co -C rpc

nfs_handler.o: nfs_handler.h protos.h inline.h chdebug.h xdos.h\
	nfsc_iocontrol.h mount.h nfs_prot.h simplerexx.h locks.h \
	dirs.h attrs.h dcache.h fh.h timer.h dos_dostags.h wbuf.h \
	rbuf.h mbuf.h nfs.h cred.h
actions.o: nfsc_iocontrol.h
ch_nfsctl.o: nfssupp.h
rpc/clnt_audp.o: chdebug.h
log.o: supp.h
examine.o: xexall.h

# Note 306: __inline function inlined: ""
# 181: "" was declared both static and external
