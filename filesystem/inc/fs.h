#include <stdint.h>

#define BLKSIZE 4096				// Block size in bytes
//#define MAXBLOCKS 25600			// Max num allocatable blocks. 4096 bytes * 25600 blocks == 100MB
#define MAXBLOCKS 256				// Temporary for rapid development: 1MB filesystem

#define NBLOCKS 8				// Number of direct blocks an inode can point to
#define NBLOCKS_IBLOCK 8			// Number of direct blocks an indirect block can point to
#define NIBLOCKS 8				// Number of indirect blocks an indirect block can point to

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

typedef unsigned int uint;
typedef unsigned long fs_ino_t;			// Inode number is just a ulong
typedef uint fs_mode_t;				// File mode is just an int (0 =='r', 1 =='w')

typedef struct block {
	uint16_t num;				// Index of this block. The block knows where it is in the fs
	uint16_t next;				// Index of next block. Enables traversing blocks in linked-list fashion
	char data[BLKSIZE-2];			// Size of this block - size of the fields = how much data we can store
} block;

typedef struct iblock1 {	
	/*block* blocks[NBLOCKS_IBLOCK];*/	// 1st-level indirect blocks
	uint blocks[NBLOCKS_IBLOCK];
} iblock;

typedef struct iblock2 {	
	struct iblock1 iblocks[NIBLOCKS];	// 2nd-level indirect blocks
} iblock2;

typedef struct iblock3 {
	struct iblock2 iblocks[NIBLOCKS];	// 3rd-level indirect blocks
} iblock3;

struct file;
struct dentry;
struct link;

typedef struct inode {
	fs_ino_t num;				// Inode number
	uint nlinks;				// Number of hard links to the inode
	uint size;				// File size in bytes
	uint nblocks;				// File size in blocks
	uint mode;				// 0 file, 1 directory, 2 link
	union {
		struct file* file_o;
		struct dentry* dir_o;
		struct link* link_o;
	} owner;				// "owner" of this inode can be file, dir, link

	/*block* blocks[NBLOCKS];*/		// Directly addressable blocks (8 of them)
	uint blocks[NBLOCKS];

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

typedef struct file {
	fs_mode_t mode;				// 0 or 'r' read, 1 or 'w' write
	uint f_pos;				// Byte offset seek'ed to
	inode* f_inode;				// pointer to the file's inode
	char name[FS_NAMEMAXLEN];		// filename
} file;

typedef struct link {
	struct inode ino;			// This link's inode
	struct inode dest;			// inode pointing to
	char name[FS_NAMEMAXLEN];		// filename
} link;

typedef struct dentry {				// Directory entry
	inode ino;				// Inode
	struct dentry* parent;			// Parent directory
	struct dentry* head;			// First dir added here
	struct dentry* tail;			// Last dir added here
	struct dentry* next;			// Next dir in parent
	struct dentry* prev;			// Previous dir in parent

	file* files;				// Files in this dir
	link* links;				// Links in this dir
	uint ndirs, nfiles;

	char name[FS_NAMEMAXLEN];		// filename
} dentry;

typedef struct superblock {
	uint root_first_block;			// First block of root dir
	uint root_last_block;			// Last block of root dir
	uint numBlocks;				// The number of allocated blocks
	uint next;				// If the superblock > 1 block, index of next block
	uint free_blocks_base;			// Index of lowest free block
	inode root;				// Root directory entry

} superblock;

typedef struct filesystem {	

	superblock* sb;				// Block 0. Contains fs configuration
	block* free_block_bitmap;		/* Block 1. A map of free blocks. 
						 * Since filesystem size == 100MB == 25600 4096-byte blocks,
						 * we can use a single block of 4096 chars == 4096*8 == 32768 bits
						 * to store the free block bitmap.
						 */
} filesystem;

extern char* fname;				/* The name our filesystem will have on disk */
extern filesystem* fs;				/* Pointer to the in-memory representation of the fs, 
						 * e.g. freelist && superblock  && root
						 */
extern block* block_cache[MAXBLOCKS];		/* In-memory copies of blocks on disk */

extern int fs_openfs();
extern int fs_mkfs();
extern int fs_open(char* filename, char* mode);
extern char* fs_read(int fd, int size);
extern void fs_write(int fd, char* string);
extern void fs_seek(int fd, int offset);
extern void fs_close(int fd);
extern int fs_mkdir(char* currentdir, char* dirname);
extern void fs_rmdir(int fd);
extern void fs_link(char* src, char* dest);
extern void fs_unlink(char* name);
extern inode* fs_stat(char* name);