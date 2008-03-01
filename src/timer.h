#ifndef TIMER_H
#define TIMER_H

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

#include <devices/timer.h>

struct SyncTimer
{
    struct TimeRequest *st_Req;
    struct DosPacket *st_Pkt;
    ULONG st_Flags;
};

typedef struct SyncTimer SyncTimer;

#define STF_TIMER_USED 1
#define STF_DEVICE_OPEN 2

#define SYNC_TIME 30 /* seconds */

#endif
