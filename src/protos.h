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

/* nfs_handler.c */
int RPCTimeoutReq ( void );
void G_CleanUp ( Global_T *g );
LONG G_Init ( Global_T *g );
DOSPKT *G_WaitPkt ( Global_T *g );
DOSPKT *G_GetPkt ( Global_T *g );
void G_ReplyPkt ( Global_T *g , DOSPKT *pkt );
void G_UnMount ( Global_T *g );
LONG G_Mount ( Global_T *g , char **reason );
void usage ( char *progname );
LONG ParseArgs ( Global_T *g , char *progname );
int main ( int argc , char *argv []);

/* log.c */
void log_SetPar ( void );
void log_SetSer ( void );
void log_end ( void );
void akprintf ( int level , const char *fmt , ...);
void akprintf ( int level , const char *fmt , ...);
void log_end ( void );

/* actions.c */
LONG act_Unknown ( Global_T *g , DOSPKT *pkt );
LONG act_IS_WRITE_PROTECTED ( Global_T *g , DOSPKT *pkt );
LONG act_NotImplemented ( Global_T *g , DOSPKT *pkt );
LONG act_OK ( Global_T *g , DOSPKT *pkt );
LONG act_DIE ( Global_T *g , DOSPKT *pkt );
LONG act_Init ( Global_T *g );
ActionFunc act_Prepare ( Global_T *g , DOSPKT *pkt , BOOL *Internal );

/* nfssupp.c */
const char *StrNFSErr ( nfsstat i );
LONG NFSErr2IoErr ( nfsstat i );
LONG NFSErr2IoErrCont ( nfsstat i , char acttype );
const char *StrNFSType ( int t );
LONG CLIENT2IoErr ( CLIENT *clnt );
const char *StrNFSMode ( unsigned int i );
UWORD nfsuid2UID ( u_int uid );
UWORD nfsgid2GID ( u_int gid );
u_int UID2nfsuid ( UWORD UID );
u_int GID2nfsgid ( UWORD GID );
LONG IsSameFH ( const nfs_fh *s , const nfs_fh *d );

/* fn.c */
UBYTE *fn_AM2UN ( UBYTE *s );
void fn_AddPart ( UBYTE *s1 , LONG l1 , UBYTE *s2 , LONG l2 , UBYTE **ds , LONG *dl );
__inline UBYTE *StrNDup ( const UBYTE *s , LONG l );
__inline UBYTE *StrDup ( const UBYTE *s );
void fn_FilePart ( UBYTE *s , LONG l , UBYTE **ds , LONG *dl );
void fn_B2CStr ( CBSTR bs , UBYTE **ds , LONG *dl );
UBYTE *fn_ScanPath ( UBYTE *APath , LONG PathLen , LONG *PosP , LONG *LastElement );
UBYTE *fn_PathUnify ( UBYTE *ss , LONG *Len , LONG *StrippedDirs , LONG *Res2 );
UBYTE *fn_BPathWithoutDev ( CBSTR bs , LONG *IsAbsolute , LONG *Res2 );
UBYTE *fn_PathWithoutDev ( UBYTE *ss , LONG *IsAbsolute , LONG *Res2 );
void fn_Delete ( UBYTE **s );
void fn_PathPart ( UBYTE *s , LONG l , UBYTE **ds , LONG *dl );
void fn_DirPart ( UBYTE *s , LONG l , UBYTE **ds , LONG *dl );
UBYTE *fn_InsertLink ( UBYTE *Buf , ULONG BufLen , UBYTE *Device , UBYTE *Path , ULONG From , ULONG Len , UBYTE *Link , LONG *Res2 );

/* nfs.c */
fattr *nfs_GetAttr ( CLIENT *clnt , nfs_fh *file , LONG *Res2 );
fattr *nfs_SetAttr ( CLIENT *clnt , nfs_fh *filefh , ULONG Mode , ULONG Uid , ULONG Gid , ULONG Size , nfstime *tv , LONG *Res2 );
diropokres *nfs_LookupNC ( CLIENT *clnt , const nfs_fh *dirfh , const UBYTE *name , ULONG MaxRead , LONG *Res2 );
diropokres *nfs_ILookup ( CLIENT *clnt , const nfs_fh *dirfh , const UBYTE *name , ULONG MaxReadDirSize , LONG *Res2 , int DoCaseInsensitive );
diropokres *nfs_Lookup ( CLIENT *clnt , const nfs_fh *dirfh , const UBYTE *name , ULONG MaxReadDirSize , LONG *Res2 );
nfspath nfs_ReadLink ( CLIENT *clnt , nfs_fh *file , LONG *Res2 );
LONG nfs_Read ( CLIENT *clnt , nfs_fh *fh , LONG offset , UBYTE *Buf , LONG count , LONG *FileLen , LONG *Res2 );
LONG nfs_MRead ( NFSGlobal_T *ng , CLIENT *clnt , nfs_fh *fh , ULONG offset , UBYTE *Buf , ULONG count , LONG *FileLen , LONG *Res2 );
LONG nfs_MARead ( NFSGlobal_T *ng , CLIENT *clnt , nfs_fh *fh , ULONG offset , UBYTE *Buf , ULONG count , LONG *FileLenP , LONG *Res2 );
LONG nfs_Write ( CLIENT *clnt , nfs_fh *fh , LONG offset , UBYTE *Buf , LONG count , LONG *FileLen , LONG *Res2 );
LONG nfs_MWrite ( NFSGlobal_T *ng , CLIENT *clnt , nfs_fh *fh , ULONG offset , UBYTE *Buf , ULONG arg_count , LONG *FileLen , LONG *Res2 );
LONG nfs_MAWrite ( NFSGlobal_T *ng , CLIENT *clnt , nfs_fh *fh , ULONG offset , UBYTE *Buf , ULONG arg_count , LONG *FileLenP , LONG *Res2 );
diropokres *nfs_Create ( CLIENT *clnt , nfs_fh *dirfh , UBYTE *name , ULONG UID , ULONG GID , ULONG Mode , ULONG Size , LONG *Res2 );
LONG nfs_Remove ( CLIENT *clnt , nfs_fh *dirfh , UBYTE *name , LONG *Res2 );
LONG nfs_Rename ( CLIENT *clnt , nfs_fh *dirfh1 , UBYTE *name1 , nfs_fh *dirfh2 , UBYTE *name2 , LONG *Res2 );
LONG nfs_Link ( CLIENT *clnt , nfs_fh *fromfh , nfs_fh *dirfh , UBYTE *Name , LONG *Res2 );
LONG nfs_Symlink ( CLIENT *clnt , nfs_fh *dirfh , UBYTE *Name , ULONG UID , ULONG GID , UBYTE *Path , LONG *Res2 );
diropokres *nfs_MkDir ( CLIENT *clnt , nfs_fh *dirfh , UBYTE *name , ULONG UID , ULONG GID , u_int Mode , LONG *Res2 );
LONG nfs_RmDir ( CLIENT *clnt , nfs_fh *dirfh , UBYTE *name , LONG *Res2 );
entry *nfs_ReadDir ( CLIENT *clnt , const nfs_fh *dir , nfscookie cookie , ULONG BufSize , LONG *Eof , LONG *Res2 );
statfsokres *nfs_statfs ( CLIENT *clnt , nfs_fh *dir , LONG *Res2 );
fattr *nfs_Chmod ( CLIENT *clnt , nfs_fh *filefh , ULONG Mode , LONG *Res2 );
fattr *nfs_SetSize ( CLIENT *clnt , nfs_fh *filefh , LONG Size , LONG *Res2 );
fattr *nfs_SetTimes ( CLIENT *clnt , nfs_fh *filefh , nfstime *tv , LONG *Res2 );
fattr *nfs_SetUidGid ( CLIENT *clnt , nfs_fh *filefh , u_int uid , u_int gid , LONG *Res2 );

/* supp.c */
BSTR NewBSTR ( UBYTE *s );
BSTR DupBSTR ( BSTR bs );
void DelBSTR ( BSTR bs );

/* locks.c */
ELOCK *lock_Create ( Global_T *g , UBYTE *FullName , LONG FullNameLen , UBYTE *Name , LONG NameLen , ftype nfstype , LONG Mode );
ELOCK *lock_Dup ( Global_T *g , ELOCK *oel );
void lock_Insert ( Global_T *g , ELOCK *elock );
ELOCK *lock_Lookup ( Global_T *g , LOCK *l );
ELOCK *lock_LookupFromKey ( Global_T *g , LONG Key );
LONG lock_NumLocksFromKey ( Global_T *g , LONG Key );
ELOCK *lock_LookupFromNFSFh ( Global_T *g , nfs_fh *fh );
ELOCK *lock_Remove ( Global_T *g , ELOCK *l );
void lock_Delete ( Global_T *g , ELOCK **el );

/* fh.c */
EFH *fh_Create ( Global_T *g , ELOCK *elock , LONG Mode );
void fh_Insert ( Global_T *g , EFH *efh );
EFH *fh_Lookup ( Global_T *g , LONG Id );
EFH *fh_Remove ( Global_T *g , EFH *arg_efh );
void fh_Delete ( Global_T *g , EFH **efh );
void fh_ForEach ( Global_T *g , fh_ForEachProc_T foreachproc , void *arg );

/* dirs.c */
EDH *dir_Create ( const Global_T *g , LONG Key , ULONG cookie , nfs_fh *nfsfh , nfscookie nfsc , UBYTE *Name , LONG MaxFileNameLen );
void dir_Insert ( Global_T *g , EDH *edh );
EDH *dir_LookupFromCookie ( Global_T *g , ULONG Cookie );
EDH *dir_LookupFromKeyAndCookie ( Global_T *g , LONG Key , ULONG Cookie );
EDH *dir_LookupFromKeyAndCookieAndCBSTR ( Global_T *g , LONG Key , ULONG Cookie , UBYTE *Name );
EDH *dir_LookupFromAddress ( Global_T *g , void *addr );
EDH *dir_Remove ( Global_T *g , EDH *arg_edh );
__inline void dir_DelEntry ( entry **arg_ent );
void dir_Delete ( EDH **arg_edh );
void dir_DeleteAll ( Global_T *g );
entry *dir_RemHeadEntry ( EDH *edh );
entry *dir_GetHeadEntry ( EDH *edh );
entry *dir_DupEntries ( entry *arg_ent );

/* attrs.c */
LONG attrs_Init ( Global_T *g );
void attrs_Cleanup ( Global_T *g );
ACEntry *attrs_LookupFromFileId ( Global_T *g , u_int FileId );
void attrs_Delete ( Global_T *g , LONG FileId );
ACEntry *attrs_Update ( Global_T *g , nfs_fh *FH , fattr *FAttr );
ACEntry *attrs_CreateAndInsert ( Global_T *g , nfs_fh *FH , fattr *FAttr );
void attrs_UnLockAll ( Global_T *g );
void attrs_Flush ( Global_T *g );
LONG attrs_SetCacheTimeout ( Global_T *g , LONG Timeout );
void attrs_Bury ( Global_T *g );
void attrs_SetCacheSize ( Global_T *g , ULONG NewSize );

/* dcache.c */
LONG dc_Init ( Global_T *g );
void dc_Cleanup ( Global_T *g );
DCEntry *dc_DCECreateAndInsert ( Global_T *g , u_int fileid , nfs_fh *FH );
NEntry *dc_NECreateAndInsert ( Global_T *g , DCEntry *dcent , const UBYTE *AName , ftype FType , u_int FileId );
NEntry *dc_NELookup ( Global_T *g , const DCEntry *DCEnt , const UBYTE *Name );
void dc_NERemove ( Global_T *g , DCEntry *DCEnt , NEntry *ParNEnt );
DCEntry *dc_DCELookupFromFileId ( Global_T *g , u_int Id );
void dc_DCEDeleteFromFileId ( Global_T *g , u_int Id );
void dc_UnLockAll ( Global_T *g );
void dc_Flush ( Global_T *g );
LONG dc_SetCacheTimeout ( Global_T *g , LONG Timeout );
void dc_Bury ( Global_T *g );
void dc_SetCacheSize ( Global_T *g , ULONG NewSize );

/* wbuf.c */
void wc_CreateCache ( EFH *efh , LONG MaxBufs );
void wc_DeleteCache ( EFH *efh );
void wc_FreeWBufs ( Global_T *g );
LONG wc_AllocWBufs ( Global_T *g , LONG NumBufs );
void WBufAddTail ( WBUFCLUSTER *wbc , WBUFHDR *wbh );
void WBufAddHead ( WBUFCLUSTER *wbc , WBUFHDR *wbh );
void WBufInsert ( WBUFCLUSTER *wbc , WBUFHDR *oldwbh , WBUFHDR *wbh );
WBUFHDR *WBufRemHead ( WBUFCLUSTER *wbc );
LONG wc_WriteCache ( Global_T *g , EFH *efh , UBYTE *Buf , LONG Num , LONG *Res2 );
LONG wc_FlushCache ( Global_T *g , EFH *efh , LONG *Res2 );

/* rbuf.c */
void rb_CreateRBuf ( EFH *efh );
void rb_DeleteRBuf ( EFH *efh );
void rb_ObtainRBufData ( Global_T *g , EFH *efh );
void rb_ReleaseRBufData ( Global_T *g , EFH *efh );
LONG rb_ReadBuf ( Global_T *g , EFH *efh , UBYTE *Buf , LONG Num , LONG *Res2 );

/* mbuf.c */
void mb_FreeSomeMBufs ( Global_T *g , LONG NumBufs );
void mb_FreeMBufs ( Global_T *g );
LONG mb_AllocMoreMBufs ( Global_T *g , LONG NumBufs );
LONG mb_AllocMBufs ( Global_T *g , LONG NumBufs );
MBUFNODE *mb_ObtainMBuf ( Global_T *g );
void mb_ReleaseMBuf ( Global_T *g , MBUFNODE *mbn );

/* cred.c */
void cr_Init ( CRED *cr );
void cr_Flush ( CRED *cr );
void cr_CleanUp ( CRED *cr );
BOOL cr_SetUser ( CRED *cr , const char *user );
BOOL cr_UMaskWasSet ( CRED *cr );
void cr_UpdateCreds ( CRED *cr , struct Task *client );
void cr_SetUMask ( CRED *cr , u_int umask );
const char *cr_GetUser ( CRED *cr );
u_int Xcr_GetUMask ( CRED *cr );
u_int Xcr_GetUID ( CRED *cr );
u_int Xcr_GetGID ( CRED *cr , struct Task *client );
BOOL cr_UpdateAuth ( CRED *cr , struct Task *client , const char *HostName );
BOOL cr_AuthNeedsUpdate ( CRED *cr , struct Task *client );
AUTH *Xcr_GetAuth ( CRED *cr );

/* lock.c */
ELOCK *DupELock ( Global_T *g , ELOCK *OldLock , UBYTE *Name , UBYTE *FullName , LONG Mode , LONG *Res2 );
ACEntry *c_GetAttr ( Global_T *g , u_int FileId , nfs_fh *NFSFh , LONG *Res2 );
NEntry *GetNameEntry ( Global_T *g , DCEntry *ActDir , const UBYTE *Name , ACEntry **ACEP , LONG *Res2 );
DCEntry *GetDirEntry ( Global_T *g , NEntry *ParName , u_int ActDirId , nfs_fh *ActDirFh , UBYTE *Name , LONG *Res2 );
UBYTE *CBuildFullName ( Global_T *g , LOCK *fl , UBYTE *cstr , ELOCK **DirLock , LONG *ArgFullNameLen , UBYTE **ArgName , LONG *ArgNameLen , UBYTE **UPath , LONG *Res2 );
UBYTE *BuildFullName ( Global_T *g , LOCK *fl , CBSTR cbstr , ELOCK **DirLock , LONG *ArgFullNameLen , UBYTE **ArgName , LONG *ArgNameLen , UBYTE **UPath , LONG *Res2 );
DCEntry *RootLocateDir ( Global_T *g , UBYTE *FullName , LONG FullNameLen , LONG *Res2 );
DCEntry *LRootLocateDir ( Global_T *g , LOCK *fl , UBYTE *RelPath , LONG IsCString , UBYTE **PName , LONG *PNameLen , UBYTE **PFullName , LONG *PFullNameLen , LONG *Res2 );
ELOCK *RootLocateAndLock ( Global_T *g , LOCK *fl , UBYTE *RelPath , LONG Mode , LONG *Res2 );
LONG act_LOCATE_OBJECT ( Global_T *g , DOSPKT *pkt );
LONG act_FREE_LOCK ( Global_T *g , DOSPKT *pkt );
LONG do_act_COPY_DIR ( Global_T *g , ELOCK *elock );
LONG act_COPY_DIR ( Global_T *g , DOSPKT *pkt );
LONG do_act_PARENT ( Global_T *g , ELOCK *oldelock );
LONG act_PARENT ( Global_T *g , DOSPKT *pkt );
LONG act_SAME_LOCK ( Global_T *g , DOSPKT *pkt );

/* info.c */
LONG act_INFO ( Global_T *g , DOSPKT *pkt );
LONG act_DISK_INFO ( Global_T *g , DOSPKT *pkt );

/* find.c */
LONG act_FINDREADWRITE ( Global_T *g , DOSPKT *pkt , LONG FileMode , LONG Mode );
LONG act_FINDUPDATE ( Global_T *g , DOSPKT *pkt );
LONG act_FINDINPUT ( Global_T *g , DOSPKT *pkt );
LONG act_FH_FROM_LOCK ( Global_T *g , DOSPKT *pkt );
LONG act_FINDOUTPUT ( Global_T *g , DOSPKT *pkt );
LONG act_END ( Global_T *g , DOSPKT *pkt );
LONG act_SET_FILE_SIZE ( Global_T *g , DOSPKT *pkt );
LONG act_COPY_DIR_FH ( Global_T *g , DOSPKT *pkt );
LONG act_PARENT_FH ( Global_T *g , DOSPKT *pkt );
LONG act_EXAMINE_FH ( Global_T *g , DOSPKT *pkt );

/* read.c */
LONG act_READ ( Global_T *g , DOSPKT *pkt );

/* examine.c */
ULONG NFSMode2Protection ( unsigned int mode );
LONG NFSType2EntryType ( ftype type );
void timeval2DateStamp ( nfstime *tv , struct DateStamp *ds );
void DateStamp2timeval ( struct DateStamp *ds , nfstime *tv );
void FAttr2FIB ( fattr *a , UBYTE *Name , LONG NameLen , LONG MaxFileNameLen , FIB *fib );
void PrintFIB ( FIB *fib );
LONG do_act_EXAMINE_OBJECT ( Global_T *g , ELOCK *efl , FIB *fib );
LONG do_ExamineRoot ( Global_T *g , FIB *fib );
LONG act_EXAMINE_OBJECT ( Global_T *g , DOSPKT *pkt );
fattr *LookupAttr ( Global_T *g , ELOCK *efl , u_int FileId , UBYTE *Name , LONG *Res2 );
LONG act_EXAMINE_NEXT ( Global_T *g , DOSPKT *pkt );
LONG act_GET_ATTRIBUTES ( Global_T *g , DOSPKT *pkt );
LONG GetNextEntries ( Global_T *g , EDH *edh , LONG *Res2 );
LONG GetFixedSize ( LONG Type );
LONG PutExAllData ( UBYTE *Buf , LONG Size , LONG Type , LONG FixedSize , LONG MaxFileNameLen , entry *anEntry , fattr *fAttr );
LONG act_EXAMINE_ALL ( Global_T *g , DOSPKT *pkt );

/* filesys.c */
LONG act_IS_FILESYSTEM ( Global_T *g , DOSPKT *pkt );
LONG act_WRITE_PROTECT ( Global_T *g , DOSPKT *pkt );
LONG act_FLUSH ( Global_T *g , DOSPKT *pkt );
LONG act_CURRENT_VOLUME ( Global_T *g , DOSPKT *pkt );
void RemReadBufs ( Global_T *g );
void DistributeBuffers ( Global_T *g , LONG NumBufs , LONG *PMBufs , LONG *PAttrs , LONG *PDirs );
LONG act_MORE_CACHE ( Global_T *g , DOSPKT *pkt );
LONG act_RENAME_DISK ( Global_T *g , DOSPKT *pkt );

/* seek.c */
LONG act_SEEK ( Global_T *g , DOSPKT *pkt );

/* write.c */
LONG act_WRITE ( Global_T *g , DOSPKT *pkt );

/* link.c */
LONG act_READ_LINK ( Global_T *g , DOSPKT *pkt );
LONG do_act_MAKE_HARD_LINK ( Global_T *g , DOSPKT *pkt );
LONG do_act_MAKE_SOFT_LINK ( Global_T *g , DOSPKT *pkt );
LONG act_MAKE_LINK ( Global_T *g , DOSPKT *pkt );

/* object.c */
LONG act_DELETE_OBJECT ( Global_T *g , DOSPKT *pkt );
LONG act_CREATE_DIR ( Global_T *g , DOSPKT *pkt );
LONG act_RENAME_OBJECT ( Global_T *g , DOSPKT *pkt );
LONG act_CHANGE_MODE ( Global_T *g , DOSPKT *pkt );
LONG act_CREATE_OBJECT ( Global_T *g , DOSPKT *pkt );

/* attr.c */
LONG act_SET_PROTECT ( Global_T *g , DOSPKT *pkt );
LONG act_SET_DATE ( Global_T *g , DOSPKT *pkt );
LONG act_SET_OWNER ( Global_T *g , DOSPKT *pkt );

/* iocontrol.c */
LONG act_IOCONTROL ( Global_T *g , DOSPKT *pkt );

/* timer.c */
void ti_Cleanup ( Global_T *g );
LONG ti_Init ( Global_T *g );
LONG act_TIMER ( Global_T *g , DOSPKT *pkt );

/* dummy.c */
int Myfprintf ( int i , const char *fmt , ...);
void Myperror ( void );
LONG Mysprintf ( UBYTE *buffer , UBYTE *fmt , ...);
void *Mymalloc ( size_t size );
void *Mycalloc ( size_t n , size_t size );
void *Myrealloc ( void *b , size_t n );
void Myfree ( void *ptr );

/* ch_nfs_prot_clnt.c */
void *nfsproc_null_2 ( void *argp , CLIENT *clnt );
attrstat *nfsproc_getattr_2 ( nfs_fh *argp , CLIENT *clnt );
attrstat *nfsproc_setattr_2 ( sattrargs *argp , CLIENT *clnt );
void *nfsproc_root_2 ( void *argp , CLIENT *clnt );
diropres *nfsproc_lookup_2 ( diropargs *argp , CLIENT *clnt );
readlinkres *nfsproc_readlink_2 ( nfs_fh *argp , CLIENT *clnt );
readres *nfsproc_read_2 ( readargs *argp , CLIENT *clnt );
readres *nfsproc_read_buf ( readargs *argp , CLIENT *clnt , char *buf );
int nfsproc_send_read_buf ( readargs *argp , CLIENT *clnt , readres *clnt_res , char *buf , void *userdata );
readres *nfsproc_wait_read_buf ( CLIENT *clnt , void **userdata );
readres *nfsproc_check_read_buf ( CLIENT *clnt );
void *nfsproc_writecache_2 ( void *argp , CLIENT *clnt );
attrstat *nfsproc_write_2 ( writeargs *argp , CLIENT *clnt );
attrstat *nfsproc_wait_write_2 ( CLIENT *clnt );
attrstat *nfsproc_check_write_2 ( CLIENT *clnt , int *error_flag );
int nfsproc_send_writesb_2 ( writeargs *argp , CLIENT *clnt );
int nfsproc_send_write_2 ( writeargs *argp , CLIENT *clnt );
diropres *nfsproc_create_2 ( createargs *argp , CLIENT *clnt );
nfsstat *nfsproc_remove_2 ( diropargs *argp , CLIENT *clnt );
nfsstat *nfsproc_rename_2 ( renameargs *argp , CLIENT *clnt );
nfsstat *nfsproc_link_2 ( linkargs *argp , CLIENT *clnt );
nfsstat *nfsproc_symlink_2 ( symlinkargs *argp , CLIENT *clnt );
diropres *nfsproc_mkdir_2 ( createargs *argp , CLIENT *clnt );
nfsstat *nfsproc_rmdir_2 ( diropargs *argp , CLIENT *clnt );
readdirres *nfsproc_readdir_2 ( readdirargs *argp , CLIENT *clnt );
statfsres *nfsproc_statfs_2 ( nfs_fh *argp , CLIENT *clnt );

/* simplerexx.c */
char *ARexxName ( AREXXCONTEXT RexxContext );
ULONG ARexxSignal ( AREXXCONTEXT RexxContext );
struct RexxMsg *GetARexxMsg ( AREXXCONTEXT RexxContext );
void ReplyARexxMsg ( AREXXCONTEXT RexxContext , struct RexxMsg *rmsg , char *RString , LONG Error );
short SetARexxLastError ( AREXXCONTEXT RexxContext , struct RexxMsg *rmsg , char *ErrorString );
short SendARexxMsg ( AREXXCONTEXT RexxContext , char *RString , short StringFile );
void FreeARexx ( AREXXCONTEXT RexxContext );
AREXXCONTEXT InitARexx ( char *AppName , char *Extension );
