#ifndef NFSC_IOCONTROL_H
#define NFSC_IOCONTROL_H

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

/* handler private action for debugging purposes */
#define ACTION_IOCONTROL 142

/* 
  ACTION_IOCONTROL takes 2 Arguments:
  Arg1: IO Control code as defined below
  Arg2: Pointer to structure or ULONG Argument special to IO Control Code 
*/

#define NFSC_IOCNTRL_GET_DEBUG 1
#define NFSC_IOCNTRL_SET_DEBUG 2

typedef struct {
    LONG DebugLevel;
} nfsc_iocntrl_debug;

#define NFSC_IOCNTRL_GET_UMASK 3
#define NFSC_IOCNTRL_SET_UMASK 4

typedef struct {
    LONG UMask;
} nfsc_iocntrl_umask;

#define NFSC_IOCNTRL_GET_UIDGID 5
#define NFSC_IOCNTRL_SET_UIDGID 6

typedef struct {
    ULONG UID;
    ULONG GID;
} nfsc_iocntrl_uidgid;

#define NFSC_IOCNTRL_GET_MAXREADSIZE 7
#define NFSC_IOCNTRL_SET_MAXREADSIZE 8

#define NFSC_IOCNTRL_GET_MAXREADDIRSIZE  9
#define NFSC_IOCNTRL_SET_MAXREADDIRSIZE 10

#define NFSC_IOCNTRL_GET_MAXWRITESIZE 11
#define NFSC_IOCNTRL_SET_MAXWRITESIZE 12

#define NFSC_IOCNTRL_GET_WRITEBUFFERLIMIT 13
#define NFSC_IOCNTRL_SET_WRITEBUFFERLIMIT 14

typedef struct {
    ULONG Size;
} nfsc_iocntrl_size;

#define NFSC_IOCNTRL_GET_ATTRCACHETIMEOUT 15
#define NFSC_IOCNTRL_SET_ATTRCACHETIMEOUT 16

#define NFSC_IOCNTRL_GET_DIRCACHETIMEOUT 17
#define NFSC_IOCNTRL_SET_DIRCACHETIMEOUT 18

#define NFSC_IOCNTRL_GET_RPCTIMEOUT 	19
#define NFSC_IOCNTRL_SET_RPCTIMEOUT 	20

typedef struct {
    ULONG Time;
} nfsc_iocntrl_time;

#define NFSC_IOCNTRL_GET_TIMEOUTREQ 	21
#define NFSC_IOCNTRL_SET_TIMEOUTREQ	22

#define NFSC_IOCNTRL_GET_SLOW_MEDIUM 	23
#define NFSC_IOCNTRL_SET_SLOW_MEDIUM 	24

typedef struct {
    LONG Flag;
} nfsc_iocntrl_flag;
#endif
