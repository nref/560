#include <stdint.h>

#define BLKSIZE 4096				// Block size in bytes
//#define MAXBLOCKS 25600			// Max num allocatable blocks. 4096 bytes * 25600 blocks == 100MB
#define MAXBLOCKS 256				// Temporary for rapid development: 1MB filesystem

#define NBLOCKS 8				// Number of direct blocks an inode can point to
#define NBLOCKS_IBLOCK 8			// Number of direct blocks an indirect block can point to
#define NIBLOCKS 8				// Number of indirect blocks an indirect block can point to

#define SUPERBLOCK_MAXBLOCKS 32			// Number of blocks we can allocate to the superblock

#define MAXFILEBLOCKS NBLOCKS*NBLOCKS*NBLOCKS_IBLOCK	// Maximum number of blocks that an inode can address

#define FS_NAMEMAXLEN 256			// Max length of a directory or file name
#define FS_MAXPATHLEN 65535			// Maximum path length == sizeof(uint16)-1 == 2^16-1
#define FS_MAXPATHFIELDS 16			// Max number of forward-slash "/"-separated fields in a path

#define FS_MAXDIRS 256				// Max number of subdirs in a dir
#define FS_MAXFILES 256				// Max number of files in a dir
#define FS_MAXLINKS 256				// Max number of links in a dir

#define FS_ROOTMAXBLOCKS 512			// Max number of a blocks root dir can occupy

#define FS_READ 0				// File read mode
#define FS_WRITE 1				// File write mode

#define FS_ERR 0				// Filesystem type error
#define FS_OK 1					// Filesystem type ok

#define FS_FILE 0				// Filesystem type file
#define FS_DIR 1				// Filesystem type directory
#define FS_LINK 2				// Filesystem type link

#define INT_SZ 8

#define true 1
#define false 0

/* The types that we want to write to or read from disk */
enum { BLOCK, MAP, SUPERBLOCK_I, SUPERBLOCK, INODE } TYPE;

typedef unsigned int uint;
typedef uint16_t block_t;			// Block number
typedef unsigned long inode_t;			// Inode number
typedef uint fs_mode_t;				// File mode (0 =='r', 1 =='w')

typedef struct map {
	char data[BLKSIZE];
} map;

typedef struct block {
	block_t num;				// Index of this block. The block knows where it is in the fs
	block_t next;				// Index of next block. Enables traversing blocks in linked-list fashion
	char data[BLKSIZE-4];			// Size of this block - size of the fields = how much data we can store
} block;

typedef struct iblock1 {	
	block_t blocks[NBLOCKS_IBLOCK];		// 1st-level indirect blocks
} iblock1;

typedef struct iblock2 {	
	struct iblock1 iblocks[NIBLOCKS];	// 2nd-level indirect blocks
} iblock2;

typedef struct iblock3 {
	struct iblock2 iblocks[NIBLOCKS];	// 3rd-level indirect blocks
} iblock3;

struct inode;					/* Forward declaration because of mutual 
						 * dependence inode <-> { file, dentry, link } */

/* files, directories, and links have an in-memory "volatile" structure as well as an on-disk structure */
typedef struct file {
	inode_t ino;				// Index of the file's inode
	uint f_pos;				// Byte offset seek'ed to
	char name[FS_NAMEMAXLEN];		// filename
} file;

typedef struct link_perm {			// On-disk link
	inode_t ino;				// This link's inode
	inode_t dest;				// inode pointing to
	char name[FS_NAMEMAXLEN];		// link name
} link_perm;

typedef struct dentry {				// On-disk directory entry
	inode_t ino;				// Inode number
	inode_t parent;				// Parent directory (inode number)
	inode_t head;				// First dir added here
	inode_t tail;				// Last dir added here
	inode_t next;				// Next dir in parent
	inode_t prev;				// Previous dir in parent

	inode_t files[FS_MAXFILES];		// Files in this dir	TODO: Updates these to linked-list fashion like dirs
	inode_t links[FS_MAXLINKS];		// Links in this dir
	uint ndirs, nfiles, nlinks;

	char name[FS_NAMEMAXLEN];		// dir name
} dentry;

typedef struct file_volatile {
	struct inode* f_inode;			// pointer to the file's inode
	fs_mode_t mode;				// 0 or 'r' read, 1 or 'w' write
	uint seek_pos;				// Byte offset seek'ed to
	char name[FS_NAMEMAXLEN];		// Filename
} file_volatile;

typedef struct link_volatile {			// In-memory link
	struct inode* ino;			// This link's inode
	struct inode* dest;			// inode pointing to
	char name[FS_NAMEMAXLEN];		// Link name
} link_volatile;

typedef struct dentry_volatile {		// In-memory directory entry
	struct inode* ino;			// Inode
	struct inode* parent;			// Parent directory (inode number)
	struct inode* head;			// First dir added here
	struct inode* tail;			// Last dir added here
	struct inode* next;			// Next dir in parent
	struct inode* prev;			// Previous dir in parent

	struct inode* files;			// Files in this dir
	struct inode* links;			// Links in this dir

	uint ndirs, nfiles;

	char name[FS_NAMEMAXLEN];		// Dir name
} dentry_volatile;

/* By intention the block, inode, and superblock have identical in-memory and disk structure */
typedef struct inode {
	inode_t num;				/* Inode number */
	uint nblocks;				/* File size in blocks */
	uint size;				/* File size in bytes */
	block_t blocks[NBLOCKS];		/* Directly addressable blocks (8 of them) */

	uint nlinks;				/* Number of hard links to the inode */
	uint mode;				/* 0 file, 1 directory, 2 link */

	union {
		struct file file;
		struct dentry dir;
		struct link_perm link;
	} data;					/* On-disk data of this inode */

	union {
		struct file_volatile* file;
		struct dentry_volatile* dir;
		struct link_perm* link;
	} data_v;				/* In-memory data of this inode  */


	struct iblock1 ib1;			/* Singly indirected blocks. 
						 * NBLOCKS_IBLOCK addressable blocks 
						 */

	struct iblock2 ib2;			/* Doubly indirected blocks. 
						 * NIBLOCKS * NBLOCKS_IBLOCK addressable blocks
						 * e.g. 8*8 = 64
						 */

	struct iblock3 ib3;			/* Triply indirected blocks. 
						 * NIBLOCKS^2 * NBLOCKS_IBLOCK addressable blocks
						 * e.g. 8^2*8 == 512 
						 */
} inode;

typedef struct superblock {
	uint free_blocks_base;			// Index of lowest unallocated block
	inode_t ino;				// Inode number of root directory entry
	block_t inode_first_blocks[MAXBLOCKS];	// Index of first allocated block for each inode.
	uint inode_block_counts[MAXBLOCKS];	// How many allocated blocks for each inode.

} superblock;

/* Need these outside of superblock because we can be certain it fits into one block */
typedef struct superblock_i {
	uint nblocks;				// The number of blocks allocated to the superblock
	block_t blocks[SUPERBLOCK_MAXBLOCKS];	// Indices to superblock's blocks

} superblock_i;

typedef struct filesystem {	
	block* block_cache[MAXBLOCKS];		/* In-memory copies of blocks on disk */
	dentry_volatile* root;			/* Root directory entry */

	map fb_map;				/* Block 0. Free block map. 
						 * Since filesystem size == 100MB == 25600 4096-byte blocks,
						 * we can use a single block of 4096 chars == 4096*8 == 32768 bits
						 * to store the free block bitmap. */

	superblock_i sb_i;			/* Block 1. Tells us where the superblock blocks are. */
	superblock sb;				/* Blocks 2...sizeof(superblock). Superblock. Contains fs configuration */

} filesystem;

char* fname;				/* The name our filesystem will have on disk */

extern void fs_delete(filesystem*);
extern filesystem* fs_openfs();
extern filesystem* fs_mkfs();
extern int fs_open(char* filename, char* mode);
extern char* fs_read(int fd, int size);
extern void fs_write(int fd, char* string);
extern void fs_seek(int fd, int offset);
extern void fs_close(int fd);
extern int fs_mkdir(filesystem *, char* currentdir, char* dirname);
extern void fs_rmdir(int fd);
extern void fs_link(char* src, char* dest);
extern void fs_unlink(char* name);
extern inode* fs_stat(filesystem *fs, char* name);
