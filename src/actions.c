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
 * This file implements the action dispatch table and prepares
 * the packet for use (e.g. changes BPTR\'s to APTR\'s) .
 */

#include "nfs_handler.h"
#include "protos.h"

#include "chdebug.h"
#include "nfsc_iocontrol.h"

#ifdef DEBUG
typedef struct 
{
    LONG ate_Action;
    ActionFunc ate_Func;
    LONG ate_BPArgs;
    LONG ate_NumArgs;
    UBYTE *ate_Name;
} ATEntry;
#define ATE(act,numargs, args) \
{ACTION_##act,act_##act,args,numargs,"ACTION_"#act}

#define ATE2(act,func,numargs,args) \
{ACTION_##act,func,args,numargs,"ACTION_"#act}
#define ATN(n) {n,act_Unknown,0,7,"dummy_"#n}
#else
typedef struct 
{
    LONG ate_Action;
    ActionFunc ate_Func;
    LONG ate_BPArgs;
} ATEntry;
#define ATE(act,numargs, args) {ACTION_##act,act_##act,args}
#define ATE2(act,func,numargs,args) {ACTION_##act,func,args}
#define ATN(n) {n,act_Unknown,0}
#endif

#define OK(act) ATE2(act,act_OK,0,0)
#define UN(act) ATE2(act,act_Unknown,7,0)
#define OB UN
#define NI UN

#define BP1 1
#define BP2 2
#define BP3 4
#define BP4 8

#define INTF 64 /* internal/pseudo packet */
#define WF 128 /* write flag (for software write protect) */

const static ATEntry ActionTable[] = 
{
#if 1
	ATE(DIE, 		0,	0		),
#else
	UN(DIE),
#endif
	OB(EVENT					),
	ATE(CURRENT_VOLUME,	1,	0		),
	ATE(LOCATE_OBJECT,	3,	BP1 | BP2	),
	ATE(RENAME_DISK,	1, 	WF | BP1	),
	ATN(10), /* place holders */
	ATN(11),
	ATN(12),
	ATN(13),
	ATN(14), /* place holders */
	ATE(FREE_LOCK, 		1,	BP1		),
	ATE(DELETE_OBJECT,	2,	WF | BP1 | BP2	),
	ATE(RENAME_OBJECT,	4,	WF | BP1 | BP2 | BP3 | BP4),
	ATE(MORE_CACHE,		1,	0		),
	ATE(COPY_DIR,		1,	BP1		),
	UN(WAIT_CHAR					), /* CONSOLE HANDLER */
	ATE(SET_PROTECT,	4,	WF | BP2 | BP3	),
	ATE(CREATE_DIR,		2,	WF | BP1 | BP2	),
	ATE(EXAMINE_OBJECT,	2,	BP1 | BP2	),
	ATE(EXAMINE_NEXT,	2,	BP1 | BP2	),
	ATE(DISK_INFO,		1,	BP1		),
	ATE(INFO,		2,	BP1 | BP2	),
	ATE(FLUSH,		0,	0		),
	NI(SET_COMMENT					),
	ATE(PARENT,		1,	BP1		),
	ATE(TIMER,		0,	INTF		),
	UN(INHIBIT					),
	OB(DISK_TYPE					),
	OB(DISK_CHANGE					),
	ATE(SET_DATE,		4,	WF | BP2 | BP3	),

	ATE(SAME_LOCK,		2,	BP1 | BP2	),

	ATE(READ,		3,	0        	),

	ATE(WRITE,		3,	WF		),


	ATE(IOCONTROL,		2,	0		),

	UN(READ_RETURN					),
	UN(WRITE_RETURN					),
	ATN(1003),
	ATE(FINDUPDATE,		3,	BP1 | BP2 | BP3	),
	ATE(FINDINPUT,		3,	BP1 | BP2 | BP3	),
	ATE(FINDOUTPUT,		3,	WF | BP1 | BP2 | BP3	),
	ATE(END,		1,	0		),
	ATE(SEEK,		3,	0		),

	NI(FORMAT					),
	ATE(MAKE_LINK,		4,	WF | BP1 | BP2	),
	ATE(SET_FILE_SIZE,	3,	WF		),
	ATE(WRITE_PROTECT,	2,	0		),

	ATE(READ_LINK,		4,	BP1		),
	ATN(1025),
	ATE(FH_FROM_LOCK,	2,	BP1 | BP2	),
	ATE(IS_FILESYSTEM,	0,	0		),
	ATE(CHANGE_MODE,	3,	BP2		),
	ATN(1029),
	ATE(COPY_DIR_FH,	1,	0		),
	ATE(PARENT_FH,		1,	0		),
	ATN(1032),
	ATE(EXAMINE_ALL,	5,	BP1		),
	ATE(EXAMINE_FH,		2,	BP2		),
	ATN(1035),
	ATE(SET_OWNER,		4,	WF | BP2 | BP3	),

	/* FIXME: lock stuff may be implemented via lock manager ? */
	UN(LOCK_RECORD					),
	UN(FREE_RECORD					),

	ATE(CREATE_OBJECT,	5,	WF | BP1 | BP2	),
	UN(SET_ATTRIBUTES				),
	ATE(GET_ATTRIBUTES,	4,	BP2 | BP3	),

	UN(ADD_NOTIFY					),
	UN(REMOVE_NOTIFY				),

        UN(SERIALIZE_DISK				),
};

/* for fast access continuous blocks will be represented by one entry in 
   the index */

typedef struct
{
    LONG ati_Low;		/* lowest action number of continous block */
    LONG ati_High;		/* highest action number of continous block */
    LONG ati_Index;		/* index into action table */
} ATIEntry;

#ifdef DEBUG
static ATIEntry ATIndex[14];
#else
static ATIEntry ATIndex[13];
#endif
static ATEntry *ATEntryRead, *ATEntryWrite;

LONG
act_Unknown(Global_T *g, DOSPKT *pkt)
{
    AKDEBUG((0, "\t*** action not known\n"));

    return SetRes(g, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
}

LONG
act_IS_WRITE_PROTECTED(Global_T *g, DOSPKT *pkt)
{
    AKDEBUG((0, "\t*** software write protected\n"));

    return SetRes(g, DOSFALSE, ERROR_DISK_WRITE_PROTECTED);
}

#if 0
LONG
act_NotImplemented(Global_T *g, DOSPKT *pkt)
{
    return SetRes(g, DOSFALSE, ERROR_NOT_IMPLEMENTED);
}
#endif

/* just return ok */
LONG
act_OK(Global_T *g, DOSPKT *pkt)
{
    return SetRes1(g, DOSTRUE);
}

LONG
act_DIE(Global_T *g, DOSPKT *pkt)
{
#ifdef DEBUG
    log_end(); /* close log file */
#endif

#if 1
    if(!g->g_Dev->dl_LockList && !g->g_FH)
    {
	act_FLUSH(g, pkt);
	g->g_State &= ~GF_STATE_RUNNING;
	return DOSTRUE;
    }
    else
    {
	return SetRes(g, DOSFALSE, ERROR_OBJECT_IN_USE);
    }

#else

    return DOSFALSE;
#endif
}

#define DBLU 0

static ATEntry *
act_Lookup(LONG action)
{
    int i=0;

#if DBLU
    AKDEBUG((0,"ATIndex[i].ati_Low = %ld\n", ATIndex[i].ati_Low));
#endif
    while(ATIndex[i].ati_Low <= action)
    {
#if DBLU
	AKDEBUG((0,"(2) ATIndex[i].ati_Low = %ld\n", ATIndex[i].ati_Low));
#endif
	if(ATIndex[i].ati_Index != -1)
	    i++;
	else
	    return NULL; /* not found */
    }
#if DBLU
    AKDEBUG((0,"(3) ATIndex[i].ati_Low = %ld\n", ATIndex[i].ati_Low));
#endif
    if(i == 0)
    {
	/* number to small */
	return NULL;
    }
    if(action > ATIndex[i-1].ati_High)
    {
	/* not in block */
	return NULL;
    }
    return &ActionTable[ATIndex[i-1].ati_Index + 
			(action-ATIndex[i-1].ati_Low)];
}


/* 
    initialize:
	- action table index 
*/

LONG
act_Init(Global_T *g)
{
    LONG atSize, iSize;
    LONG Act, New, Index, IIndex, RetVal = TRUE;
    

    atSize = sizeof(ActionTable)/sizeof(ATEntry);
    iSize = sizeof(ATIndex)/sizeof(ATIEntry); 
    Index = 0;
    IIndex = 0;
    Act = ActionTable[Index].ate_Action;
    ATIndex[IIndex].ati_Low = Act;
    ATIndex[IIndex].ati_Index = 0;
    IIndex++;
    while(++Index < atSize)
    {
	New = ActionTable[Index].ate_Action;
	if(New <= Act)
	{
	    AKDEBUG((2, "Internal error: ActionTable not monoton: %ld, %ld\n",
		     Act, New));
	    RetVal = 0;
	    break;
	}
	if(New == Act+1)
	    Act++;
	else
	{
	    ATIndex[IIndex-1].ati_High = Act;

	    Act = New;
	    ATIndex[IIndex].ati_Low = Act;
	    ATIndex[IIndex].ati_Index = Index;
	    IIndex++;
	    if(IIndex >= (iSize-1)) /* one entry for end marker */
	    {
		/* index table exhausted */
		AKDEBUG((0, "act_Init: Act = %ld\n", Act));

		IIndex = 0;
		RetVal = FALSE;
		break;
	    }
	}
    }
    ATIndex[IIndex-1].ati_High = Act;

    /* mark end */
    ATIndex[IIndex].ati_Low = ActionTable[atSize-1].ate_Action+1;
    ATIndex[IIndex].ati_Index = -1;

    if(RetVal)
    {
	ATEntryRead = act_Lookup('R');
	ATEntryWrite = act_Lookup('W');
    }

    return RetVal;
}

#define MYBADDR(a) ((a) << 2)

ActionFunc
act_Prepare(Global_T *g, DOSPKT *pkt, BOOL *Internal)
{
    const ATEntry *ent;
    LONG action, BPArgs;
#ifdef DEBUG
    LONG NumArgs, *lp;
    UBYTE Buf[160], *s;
#endif

    /* speedup things for read/write */
    action = pkt->dp_Type;
    if(action == 'R')
	ent = ATEntryRead;
    else if(action == 'W')
	ent = ATEntryWrite;
    else
	ent = act_Lookup(action);

    if(!ent)
    {
	AKDEBUG((1, "act_Prepare: action not in table: %ld\n"));

	return NULL;
    }

    /* preparse args, idea lent from net-handler */

    BPArgs = ent->ate_BPArgs;

    *Internal = (BPArgs & INTF);

    /* software write protect */
    if((g->g_Flags &  GF_IS_WRITE_PROTECTED)
       && (BPArgs & WF))
	return act_IS_WRITE_PROTECTED;

    if(BPArgs & BP1)
	pkt->dp_Arg1 = MYBADDR(pkt->dp_Arg1);
    if(BPArgs & BP2)
	pkt->dp_Arg2 = MYBADDR(pkt->dp_Arg2);
    if(BPArgs & BP3)
	pkt->dp_Arg3 = MYBADDR(pkt->dp_Arg3);
    if(BPArgs & BP4)
	pkt->dp_Arg4 = MYBADDR(pkt->dp_Arg4);

#ifdef DEBUG
    s = Buf;
    s += SPrintf(Buf,"%s(", ent->ate_Name);

    NumArgs = ent->ate_NumArgs;
    lp = &pkt->dp_Arg1;
    while(NumArgs--)
    {
	if(NumArgs)
	    s += SPrintf(s, "0x%08lx,", *lp++);
	else
	    s += SPrintf(s,"0x%08lx", *lp++);
    }
    SPrintf(s,")\n");
    AKDEBUG((0,Buf));
#endif
    
    return ent->ate_Func;
}



