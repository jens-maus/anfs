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

/*
 * Debugging log stuff.
 */

#ifdef DEBUG
#include <dos/stdio.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <clib/netlib_protos.h> /* for SPrintf */

#include <stdarg.h>
#include <time.h>

#include "supp.h"
#include "chdebug.h"


int kdebug = 0;

BPTR log_fp = 0;

#define DEST_FILE 0
#define DEST_PAR 1
#define DEST_SER 2

static LONG Dest;

char *log_name = "nfsh";
int log_init_done = 0;

extern __stdargs void DPutFmt(char *format, va_list values);
__asm void MyAKPrintF (register __a0 const char *format, 
		       register __a1 void *data);

void
log_SetPar(void)
{
    Dest = DEST_PAR;
}

void
log_SetSer(void)
{
    Dest = DEST_SER;
}

static void
log_init(void)
{
    char name[40];

    SPrintf(name, "t:%s.log_%08lx", log_name, FindTask(0L));
    if(!log_fp)
    {
	log_fp = Open(name, MODE_OLDFILE);
	if(!log_fp && (IoErr() == ERROR_OBJECT_NOT_FOUND))
	    log_fp = Open(name, MODE_NEWFILE);
	if(log_fp)
	{
	    Seek(log_fp, 0, OFFSET_END);
	    SetVBuf(log_fp, NULL, BUF_FULL, 32768);
	}
    }
}

void log_end(void)
{
    if(log_fp)
	Close(log_fp);
    log_fp = 0;
    log_init_done = 0;
}
__inline static void
xlocaltime(time_t t, struct tm *mtm)
{
    t /= 60;
    mtm->tm_min = t % 60;
    t /= 60;
    mtm->tm_hour = t % 24;
    t /= 24;
    mtm->tm_mday = t;
}

#if 0
void akprintf(int level, const char *fmt,...)
{
#ifdef SYSLOG
    char buff[1024];
#endif
    va_list args;
#ifndef SYSLOG
    time_t now;
    struct tm mytm;
    static time_t lasttime;
    static char timestr[30]; /* speedup */
#endif

    
    if (level >= kdebug)
    {
	if(!log_init_done)
	{
	    log_init();
	    log_init_done = 1;
	}
	va_start(args, fmt);
#ifdef SYSLOG
	vsprintf(buff, fmt, args);

	(void) syslog(LOG_ERR, buff);
#else
	if (log_fp == (FILE *) NULL)
	    return;
#if 0
	(void) time(&now);
#else
	if(1)
        {
	    struct DateStamp ds;
	    DateStamp(&ds);
	}
	    
#endif
	if(now != lasttime)
	{
	    /* speedup */
	    xlocaltime(now, &mytm);
	    lasttime = now;
#if 0
	    sprintf(timestr, "%s %02d/%02d/%02d %02d:%02d ",
		    log_name, mytm.tm_mon + 1, mytm.tm_mday, mytm.tm_year,
		    mytm.tm_hour, mytm.tm_min);
#else
	    sprintf(timestr, "%s %04d %02d:%02d ",
		    log_name, mytm.tm_mday, mytm.tm_hour, mytm.tm_min);
#endif
	}
	fputs(timestr, log_fp);
	vfprintf(log_fp, fmt, args);
#endif
	va_end(va);
    }
}
#else
void akprintf(int level, const char *fmt,...)
{
    va_list args;
    struct DateStamp Now;
    LONG now;
    static lasttime;
    static char timestr[30]; /* speedup */
    
    if (level >= kdebug)
    {
	if(Dest == DEST_FILE)
	{
	    if(!log_init_done)
	    {
		log_init();
		log_init_done = 1;
	    }
	    if (log_fp == 0)
		return;
	}
	va_start(args, fmt);
	DateStamp(&Now);
	now = Now.ds_Minute * 60 + Now.ds_Tick / TICKS_PER_SECOND;
	if(now != lasttime)
	{
	    LONG hour, min;
	    
	    /* speedup */
	    lasttime = now;
	    min = Now.ds_Minute;
	    hour = min/60;
	    hour %= 24;
	    min %= 60;
	    SPrintf(timestr, "%s %04ld %02ld:%02ld ",
		    log_name, Now.ds_Days, hour, min);
	}
	if(Dest == DEST_FILE)
	{
	    FPuts(log_fp, timestr);
	    VFPrintf(log_fp, (char *) fmt, (LONG *)args);
	}
	else if(Dest == DEST_PAR)
	{
	    DPutFmt(timestr, NULL);
	    DPutFmt((UBYTE *) fmt, args);
	}
	else
	{
	    MyAKPrintF(timestr, NULL);
	    MyAKPrintF((UBYTE *) fmt, args);
	}
	va_end(va);
    }
}
#endif
#else
void log_end(void) {}
#endif /* DEBUG */
