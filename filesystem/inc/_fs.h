#ifndef _FS_H
#define _FS_H

#include <stdint.h>
#include <stddef.h>

#define BLKSIZE 4096				// Block size in bytes
//#define MAXBLOCKS 25600				// Max num allocatable blocks. 4096 bytes * 25600 blocks == 100MB
#define MAXBLOCKS 256				// Temporary for rapid development: 1MB filesystem
#define MAXINODES MAXBLOCKS			// Max num allocatable inodes. Free inode numbers is always <= MAXBLOCKS

#define NBLOCKS 8				// Number of direct blocks an inode can point to
#define NBLOCKS_IBLOCK 8			// Number of direct blocks an indirect block can point to
#define NIBLOCKS 8				// Number of indirect blocks an indirect block can point to

#define SUPERBLOCK_MAXBLOCKS 64			// Number of blocks we can allocate to the superblock

// Maximum number of blocks that an inode can address
#define MAXFILEBLOCKS								\
	  NBLOCKS + NIBLOCKS	/* Direct blocks + first-level indirect */	\
	+ NIBLOCKS * NBLOCKS_IBLOCK		/* Second-level indirect */	\
	+ NIBLOCKS * NIBLOCKS * NBLOCKS_IBLOCK	/* Third-level indirect */

#define FS_NAMEMAXLEN 256			// Max length of a directory or file name
#define FS_MAXPATHFIELDS 16			// Max number of forward-slash "/"-separated fields in a path (i.e. max directory recursion)
#define FS_MAXPATHLEN FS_NAMEMAXLEN*FS_MAXPATHFIELDS+1	// Maximum path length. +1 for terminating null

#define FS_MAXFILES 256				// Max number of files in a dir
#define FS_MAXLINKS 256				// Max number of links in a dir

#define FS_ERR -1
#define FS_OK 1

#define true 1
#define false 0

/* The types that we want to write to or read from disk */
enum { BLOCK, MAP, SUPERBLOCK_I, SUPERBLOCK, INODE } TYPE;
enum { DIRECT, INDIRECT1, INDIRECT2, INDIRECT3 } INDIRECTION;
enum { OK, FS_ERROR, DIREXISTS, BADPATH, NOTONDISK } FS_MESSAGE;
enum { FS_FILE, FS_DIR, FS_LINK } FILETYPE;

typedef unsigned int uint;
typedef uint16_t block_t;			// Block number
typedef uint16_t inode_t;			// Inode number
typedef uint8_t fs_mode_t;			// File mode (0 =='r', 1 =='w')

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
	uint f_pos;				// Byte offset seek'ed to
	char name[FS_NAMEMAXLEN];		// filename
} file;

typedef struct hlink {				// On-disk link
	inode_t ino;				// This link's inode
	inode_t dest;				// inode pointing to
	char name[FS_NAMEMAXLEN];		// link name
} hlink;		/* hardlink */

typedef struct dent {				// On-disk directory entry
	inode_t ino;				// Inode number
	inode_t parent;				// Parent directory (inode number)
	inode_t head;				// First dir added here
	inode_t tail;				// Last dir added here
	inode_t next;				// Next dir in parent
	inode_t prev;				// Previous dir in parent

	inode_t files[FS_MAXFILES];		// Files in this dir
	inode_t links[FS_MAXLINKS];		// Links in this dir
	uint ndirs, nfiles, nlinks;

	char name[FS_NAMEMAXLEN];		// dir name
} dent;

struct inode;					/* Forward declaration because of mutual 
						 * dependence inode <-> { file, dent, link } */
typedef struct filev {
	struct inode* f_inode;			// pointer to the file's inode
	fs_mode_t mode;				// 0 or 'r' read, 1 or 'w' write
	uint seek_pos;				// Byte offset seek'ed to
	char name[FS_NAMEMAXLEN];		// Filename
} filev;

typedef struct hlinkv {				// In-memory link
	struct inode* ino;			// This link's inode
	struct inode* dest;			// inode pointing to
	char name[FS_NAMEMAXLEN];		// Link name
} hlinkv;

typedef struct dentv {				// In-memory directory entry
	struct inode* ino;			// Inode
	struct inode* parent;			// Parent directory (inode number)
	struct inode* head;			// First dir added here
	struct inode* tail;			// Last dir added here
	struct inode* next;			// Next dir in parent
	struct inode* prev;			// Previous dir in parent

	struct inode* files;			// Files in this dir
	struct inode* links;			// Links in this dir

	uint ndirs, nfiles, nlinks;

	char name[FS_NAMEMAXLEN];		// Dir name
} dentv;

/* By intention the block, inode, and superblock have identical in-memory and disk structure */
typedef struct inode {
	inode_t num;				/* Inode number */
	uint nblocks;				/* File size in blocks */
	uint size;				/* File size in bytes */
	uint nlinks;				/* Number of hard links to the inode */

	uint16_t inode_blocks;			/* Of the allocated blocks, how many are for the inode itself */
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
		struct hlink* link;
	} datav;				/* In-memory data of this inode  */
	
	block_t blocks[MAXFILEBLOCKS];		/* Indices to all blocks */

	block* dblocks[NBLOCKS];		/* Direct block pointers */
		
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
	uint free_blocks_base;			// Index of lowest unallocated block
	inode_t free_inodes_base;		// Index of lowest unallocated inode
	inode_t root;				// Inode number of root directory entry
	block_t inode_first_blocks[MAXBLOCKS];	// Index of first allocated block for each inode.
	uint inode_block_counts[MAXBLOCKS];	// How many allocated blocks for each inode.

} superblock;

/* Need these fields outside of superblock because we can be certain they fit into one block */
typedef struct superblock_i {
	uint nblocks;				// The number of blocks allocated to the superblock
	block_t blocks[SUPERBLOCK_MAXBLOCKS];	// Indices to superblock's blocks

} superblock_i;

typedef struct filesystem {	
	dentv* root;				/* Root directory entry */

	map fb_map;				/* Block 0. Free block map. 
						 * Since filesystem size == 100MB == 25600 4096-byte blocks,
						 * we can use a single block of 4096 chars == 4096*8 == 32768 bits
						 * to store the free block bitmap. */
	map ino_map;				/* Block 1. Free inodes */

	superblock_i sb_i;			/* Block 2. Tells us where the superblock blocks are. */
	superblock sb;				/* Blocks 3...sizeof(superblock)/sizeof(block)+1. 
						 * Superblock. Contains filesystem topology */
} filesystem;

typedef struct fs_path {
	char* fields[FS_MAXPATHFIELDS];
	uint nfields;
	uint firstField;
} fs_path;

extern char* fname;				/* The name our filesystem will have on disk */
extern char* fs_responses[5];

typedef struct { 
	fs_path*		(* _newPath)		();
	fs_path*		(* _tokenize)		(const char*, const char*);
	fs_path*		(* _pathFromString)	(const char*);
	char*			(* _stringFromPath)	(fs_path*);
	char*			(* _pathSkipLast)	(fs_path* p);
	char*			(* _pathGetLast)	(fs_path* p);
	int			(* _path_append)	(fs_path*, const char*);
	char*			(* _pathTrimSlashes)	(char* path);

	char*			(* _strSkipFirst)	(char* cpy);
	char*			(* _strSkipLast)	(char* cpy);

	dent*			(* _newd)		(filesystem*, const int, const char*);
	dentv*			(* _newdv)		(filesystem* , const int, const char*);
	dentv*			(* _ino_to_dv)		(filesystem* , inode*);
	dentv*			(* _mkroot)		(filesystem *, int);
	dentv*			(* _load_dir)		(filesystem* , inode_t);
	dentv*			(* _new_dir)		(filesystem *, dentv*, const char*);
	int			(* _attach_datav)	(filesystem* , inode*); 

	int			(* _prealloc)		();
	int			(* _zero)		();
	filesystem*		(* _open)		();
	filesystem*		(* _init)		(int);

	int			(* __balloc)		(filesystem* );
	int			(* _balloc)		(filesystem* , const int, block_t*);
	int			(* _bfree)		(filesystem* , block*);
	block*			(* _newBlock)		();

	inode_t			(* _ialloc)		(filesystem *);
	int			(* _ifree)		(filesystem* , inode_t);

	int			(* _fill_block_indices)	(inode*, block_t*, uint);
	//int			(* _fill_iblocks)	(inode*, uint, block_t*);

	//int			(* _inode_write_dblocks)(block_t* blocks, uint nblocks);
	//int			(* _inode_write_all_blocks)(inode* ino);

	inode*			(* _inode_load)		(filesystem* , inode_t);

	int			(* readblock)		(void*, block_t);
	int			(* writeblock)		(block_t, size_t, void*);

	int			(* readblocks)		(void*, block_t*, uint, size_t);
	int			(* writeblocks)		(void*, block_t*, uint, size_t);

	inode*			(* _recurse)		(filesystem* , dentv*, uint, uint, char*[]);
	int			(* _sync)		(filesystem* );

	void			(* _safeopen)		(char*, char*);
	void			(* _safeclose)		();

	void			(* _print_mem)		(void const*, size_t);
	void			(* _debug_print)	();

} fs_private_interface;
extern fs_private_interface const _fs;

#endif /* _FS_H */
