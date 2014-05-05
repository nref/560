#ifndef _FS_H
#define _FS_H

#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <stdio.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define BLKSIZE 4096				// Block size in bytes
//#define MAXBLOCKS 25600				// Max num allocatable blocks. 4096 bytes * 25600 blocks == 100MB
#define MAXBLOCKS 256				// Temporary for rapid development: 1MB filesystem
#define MAXINODES MAXBLOCKS			// Max num allocatable inodes. Free inode numbers is always <= MAXBLOCKS

#define NBLOCKS 8				// Number of direct blocks an inode can point to
#define NBLOCKS_IBLOCK 8			// Number of direct blocks an indirect block can point to
#define NIBLOCKS 8				// Number of indirect blocks an indirect block can point to

#define SUPERBLOCK_MAXBLOCKS 64			// Number of blocks we can allocate to the superblock

// Maximum number of blocks that an inode can address
#define MAXBLOCKS_DIRECT NBLOCKS
#define MAXBLOCKS_IB1 NBLOCKS_IBLOCK
#define MAXBLOCKS_IB2 (NIBLOCKS * NBLOCKS_IBLOCK)
#define MAXBLOCKS_IB3 (NIBLOCKS * NIBLOCKS * NBLOCKS_IBLOCK)
#define MAXFILEBLOCKS (MAXBLOCKS_DIRECT + \
		MAXBLOCKS_IB1 + MAXBLOCKS_IB2 + MAXBLOCKS_IB3)

#define FS_NAMEMAXLEN 256			// Max length of a directory or file name
#define FS_MAXPATHFIELDS 32			// Max number of forward-slash "/"-separated fields in a path (i.e. max directory recursion)
#define FS_MAXPATHLEN (FS_NAMEMAXLEN*FS_MAXPATHFIELDS)	// Maximum path length

#define FS_MAXFILES 256				// Max number of files in a dir
#define FS_MAXLINKS 256				// Max number of links in a dir

#define FS_MAXOPENFILES 16

#define FS_ERR -1
#define FS_NORMAL 0
#define FS_OK 1

#define true 1
#define false 0

#ifndef min
#define min(a,b)	(((a) < (b)) ? (a) : (b))
#endif

/* The types that we want to write to or read from disk */
enum { BLOCK, MAP, SUPERBLOCK_I, SUPERBLOCK, INODE } TYPE;
enum { DIRECT, INDIRECT1, INDIRECT2, INDIRECT3 } INDIRECTION;
enum { OK, ERR, DIREXISTS, BADPATH, NOTONDISK, TOOFEWARGS } FS_MESSAGE;
enum { FS_FILE, FS_DIR, FS_LINK } FILETYPE;
enum { FS_READ, FS_WRITE, FS_RW } FILEMODES;

extern char* fname;				/* The name our filesystem will have on disk */
extern char* fs_responses[6];
extern size_t stride;

typedef unsigned int uint;
typedef uint16_t block_t;			// Block number
typedef uint16_t inode_t;			// Inode number
typedef uint8_t fs_mode_t;			// File mode (0 =='r', 1 =='w')
typedef unsigned int fd_t;			/* File descriptor */

typedef struct map {
	char data[BLKSIZE];
} map;

typedef struct block {
	block_t num;				// Index of this block. The block knows where it is in the fs
	block_t next;				// Index of next block. Enables traversing blocks in linked-list fashion
	char data[BLKSIZE-4];			// Size of this block - size of the fields = how much data we can store
} block;

typedef struct iblock1 {	
	block* blocks[NBLOCKS_IBLOCK];		// 1st-level indirect blocks
} iblock1;

typedef struct iblock2 {	
	struct iblock1* iblocks[NIBLOCKS];	// 2nd-level indirect blocks
} iblock2;

typedef struct iblock3 {
	struct iblock2* iblocks[NIBLOCKS];	// 3rd-level indirect blocks
} iblock3;

/* files, directories, and links have an in-memory "volatile" structure as well as an on-disk structure */
typedef struct file {
	inode_t ino;				// Index of the file's inode
	inode_t parent;				// Inode number of parent dir
//	size_t seek_pos;			// Byte offset seek'ed to
	char name[FS_NAMEMAXLEN];		// filename
} file;

typedef struct hlink {				// On-disk link
	inode_t ino;				// This link's inode
	inode_t dest;				// inode pointing to
	inode_t parent;				// Inode number of parent dir
	uint16_t mode;				// 0 file, 1 dir, 2 link
	char name[FS_NAMEMAXLEN];		// link name
} hlink;		/* hardlink */

typedef struct dent {				// On-disk directory entry
	inode_t ino;				// Inode number
	inode_t parent;				// Parent directory inode number
	inode_t head;				// First dir added here
	inode_t tail;				// Last dir added here
	inode_t next;				// Next dir in parent
	inode_t prev;				// Previous dir in parent

	inode_t files[FS_MAXFILES];		// Files in this dir
	inode_t links[FS_MAXLINKS];		// Links in this dir
	size_t ndirs, nfiles, nlinks;

	char name[FS_NAMEMAXLEN];		// dir name
} dent;

struct inode;					/* Forward declaration because of mutual 
						 * dependence inode <-> { file, dent, link } */
typedef struct filev {
	struct inode* ino;			// Pointer to the file's inode
	struct inode* parent;			// Pointer to the parent dir inode
	fs_mode_t mode;				// 0 or 'r' read, 1 or 'w' write
	size_t seek_pos;			// Byte offset seek'ed to
	char name[FS_NAMEMAXLEN];		// Filename
} filev;

typedef struct hlinkv {				// In-memory link
	struct inode* ino;			// This link's inode
	struct inode* dest;			// Inode pointing to
	struct inode* parent;			// Pointer to the parent dir inode
	char name[FS_NAMEMAXLEN];		// Link name
} hlinkv;

typedef struct dentv {				// In-memory directory entry
	struct inode* ino;			// Inode
	struct inode* parent;			// Parent directory (inode number)
	struct inode* head;			// First dir added here
	struct inode* tail;			// Last dir added here
	struct inode* next;			// Next dir in parent
	struct inode* prev;			// Previous dir in parent

	struct inode** files;			// Files in this dir
	struct inode** links;			// Links in this dir

	size_t ndirs, nfiles, nlinks;

	char name[FS_NAMEMAXLEN];		// Dir name
} dentv;

/* By intention the block, inode, and superblock have identical in-memory and disk structure */
typedef struct inode {
	inode_t num;				/* Inode number */

	size_t nblocks;				/* Size in blocks == ndatablocks + ninoblocks */
	size_t ndatablocks;			/* Number of data blocks */

	size_t size;				/* File size in bytes */
	size_t nlinks;				/* Number of hard links to the inode */

	uint16_t ninoblocks;			/* Of the allocated blocks, how many are for the inode itself */
	uint16_t mode;				/* 0 file, 1 directory, 2 link */
	uint16_t v_attached;			/* Did we load the volatile version already ? true : false */

	union {
		struct file file;
		struct dent dir;
		struct hlink link;
	} data;					/* On-disk data of this inode */

	union {
		struct filev* file;
		struct dentv* dir;
		struct hlinkv* link;
	} datav;				/* In-memory data of this inode  */
	
	block_t blocks[MAXFILEBLOCKS];		/* Indices to all blocks */

	block* directblocks[NBLOCKS];		/* Direct block pointers */
		
	struct iblock1* ib1;			/* Indices to singly indirected blocks. 
						 * NBLOCKS_IBLOCK addressable blocks 
						 */

	struct iblock2* ib2;			/* Indices to doubly indirected blocks. 
						 * NIBLOCKS * NBLOCKS_IBLOCK addressable blocks
						 * e.g. 8*8 = 64
						 */

	struct iblock3* ib3;			/* Indices to triply indirected blocks. 
						 * NIBLOCKS^2 * NBLOCKS_IBLOCK addressable blocks
						 * e.g. 8^2*8 == 512 
						 */
} inode;

typedef struct superblock {
	size_t free_blocks_base;			// Index of lowest unallocated block
	inode_t free_inodes_base;			// Index of lowest unallocated inode
	inode_t root;					// Inode number of root directory entry
	block_t inode_first_blocks[MAXBLOCKS];		// Index of first allocated block for each inode.
	size_t inode_block_counts[MAXBLOCKS];		// How many allocated blocks for each inode.

} superblock;

/* Need these fields outside of superblock because we can be certain they fit into one block */
typedef struct superblock_i {
	size_t nblocks;				// The number of blocks allocated to the superblock
	block_t blocks[SUPERBLOCK_MAXBLOCKS];	// Indices to superblock's blocks

} superblock_i;

typedef struct filesystem {	
	dentv* root;				/* Root directory entry */
	filev* fds[FS_MAXOPENFILES];		/* File descriptors. Pointers to open files */
	fd_t first_free_fd;			/* Index into the first free file descriptor */
	fd_t allocated_fds[FS_MAXOPENFILES];	/* Table of free file descriptors */

	map fb_map;				/* Block 0. Free block map. 
						 * Since filesystem size == 100MB == 25600 4096-byte blocks,
						 * we can use a single block of 4096 chars == 4096*8 == 32768 bits
						 * to store the free block bitmap. */
	map ino_map;				/* Block 1. Free inodes */

	superblock_i sb_i;			/* Block 2. Tells us where the superblock blocks are. */
	superblock sb;				/* Blocks 3...sizeof(superblock)/sizeof(block)+1. 
						 * Superblock. Contains filesystem topology */
} filesystem;

typedef struct fs_path {			/* A struct for storing the fields of a path */
	char* fields[FS_MAXPATHFIELDS];
	size_t nfields;
	size_t firstField;
} fs_path;

typedef struct { 
	void			(* _pathFree)		(fs_path*);
	fs_path*		(* _newPath)		();
	fs_path*		(* _tokenize)		(const char*, const char*);
	fs_path*		(* _pathFromString)	(const char*);
	char*			(* _stringFromPath)	(fs_path*);
	char*			(* _pathSkipLast)	(fs_path*);
	char*			(* _pathGetLast)	(fs_path*);
	int			(* _pathAppend)		(fs_path*, const char*);
	char*			(* _pathTrimSlashes)	(char*);
	char*			(* _getAbsolutePathDV)	(filesystem*, dentv*, fs_path *);
	char*			(* _getAbsolutePath)	(char*, char*);
	char*			(* _strSkipFirst)	(char*);
	char*			(* _strSkipLast)	(char*);
	char*			(* _trim)		(char*);

	int			(* _isNumeric)		(char* str);

	dent*			(* _newd)		(filesystem*, const int, const char*);
	dentv*			(* _newdv)		(filesystem* , const int, const char*);
	
	file*			(* _newf)		(filesystem*, const int, const char*);
	filev*			(* _newfv)		(filesystem*, const int, const char*);
	
	hlink*			(* _newh)		(filesystem*, const int, const char*);
	hlinkv*			(* _newhl)		(filesystem*, const int, const char*);

	dentv*			(* _ino_to_dv)		(filesystem* , inode*);
	filev*			(* _ino_to_fv)		(filesystem* , inode*);
	hlinkv*			(* _ino_to_lv)		(filesystem* , inode*);
	
	dentv*			(* _mkroot)		(filesystem *, int);
	
	filev*			(* _load_file)		(filesystem*, dentv* parent, inode_t);
	int			(* _unload_file)	(filesystem*, inode*);

	dentv*			(* _load_dir)		(filesystem* , inode_t);
	int			(* _unload_dir)		(filesystem* , inode*);

	hlinkv*			(* _load_link)		(filesystem*, dentv* parent, inode_t);
	int			(* _unload_link)	(filesystem* , inode*);
	
	dentv*			(* _new_dir)		(filesystem *, dentv*, const char*);
	filev*			(* _new_file)		(filesystem *, dentv*, const char*);
	hlinkv*			(* _new_link)		(filesystem *, dentv*, inode*, const char*);
	
	int			(* _v_attach)		(filesystem* , inode*);
	int			(* _v_detach)		(filesystem* , inode*);
	
	int			(* _get_fd)		(filesystem*);
	int			(* _free_fd)		(filesystem*, int);

	int			(* _prealloc)		();
	int			(* _zero)		();
	filesystem*		(* _open)		();
	filesystem*		(* _mkfs)		();
	filesystem*		(* _init)		(int);
	
	int			(* __balloc)		(filesystem* );
	int			(* _mballoc)		(filesystem*, const size_t, block_t*);
	int			(* _bfree)		(filesystem* , block*);
	block*			(* _newBlock)		();
	inode*			(* _new_inode)		();
	void			(* _free_inode)		(inode*);

	int			(* _ialloc)		(filesystem *);
	int			(* _ifree)		(filesystem* , inode_t);

	int			(* _inode_fill_blocks_from_data) (filesystem*, inode*, size_t, char*);
	int			(* _inode_fill_blocks_from_disk) (inode*);

	int			(* _inode_extend_datablocks)	(filesystem*, inode*, block**);
	size_t			(* _inode_read_direct_blocks)	(char*, block**, size_t);
	char*			(* _inode_read_data)		(inode*, size_t, size_t);
	int			(* _inode_commit_data)		(inode*);

	inode*			(* _inode_load)		(filesystem* , inode_t);
	int			(* _inode_unload)	(filesystem*, inode*);

	int			(* readblock)		(void*, block_t);
	int			(* writeblock)		(block_t, size_t, void*);

	int			(* readirectblocks)	(void*, block_t*, size_t, size_t);
	int			(* writeblocks)		(void*, block_t*, size_t, size_t);

	int			(* write_commit)	(filesystem*, inode*);
	inode*			(* _stat_recurse)	(filesystem* , dentv*, size_t, size_t, fs_path*);
	inode*			(* _files_iterate)	(filesystem*, dentv*, fs_path*, size_t);
	inode*			(* _links_iterate)	(filesystem*, dentv*, fs_path*, size_t);
	
	int			(* _sync)		(filesystem* );

	void			(* _safeopen)		(char*, char*);
	void			(* _safeclose)		();
	
	void			(* _print_mem)		(void const*, size_t);
	void			(* _debug_print)	();

} fs_private_interface;
extern fs_private_interface const _fs;

#endif /* _FS_H */
