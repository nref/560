#define BLKSIZE 4096				// Block size in bytes
//#define MAXBLOCKS 25600			// Max num allocatable blocks. 4096 bytes * 25600 blocks == 100MB
#define MAXBLOCKS 256				// Temporary for rapid development: 1MB filesystem

#define NBLOCKS 8				// Number of direct blocks an inode can point to
#define NBLOCKS_IBLOCK 8			// Number of direct blocks an indirect block can point to
#define NIBLOCKS 8				// Number of indirect blocks an indirect block can point to

#define MAXFILEBLOCKS NBLOCKS^2*NBLOCKS_IBLOCK	// Maximum number of blocks that an inode can address

#define FS_NAMEMAXLEN 256			// Max length of a directory or file name
#define FS_MAXPATHLEN 65535			// Maximum path length == sizeof(uint16)-1 == 2^16-1
#define FS_MAXPATHFIELDS 16			// Max number of forward-slash "/"-separated fields in a path

#define FS_MAXDIRS 256				// Max number of subdirs in a dir
#define FS_MAXFILES 256				// Max number of files in a dir
#define FS_MAXLINKS 256				// Max number of links in a dir

#define FS_READ 0				// File read mode
#define FS_WRITE 1				// File write mode

#define FS_ERR -1				// Filesystem type error
#define FS_OK 0					// Filesystem type ok
#define FS_FILE 0				// Filesystem type file
#define FS_DIR 1				// Filesystem type directory
#define FS_LINK 2				// Filesystem type link

typedef unsigned long fs_ino_t;			// Inode number is just a ulong
typedef unsigned int fs_mode_t;			// File mode is just an int (0 =='r', 1 =='w')

typedef struct block {
	char data[BLKSIZE];
} block;

typedef struct iblock1 {	
	block* blocks[NBLOCKS_IBLOCK];		// 1st-level indirect blocks
} iblock;

typedef struct iblock2 {	
	struct iblock1* iblocks[NIBLOCKS];	// 2nd-level indirect blocks
} iblock2;

typedef struct iblock3 {
	struct iblock2* iblocks[NIBLOCKS];	// 3rd-level indirect blocks
} iblock3;

typedef struct inode {
	fs_ino_t num;				// Inode number
	int nlinks;				// Number of hard links to the inode
	int size;				// File size in bytes
	int nblocks;				// File size in blocks
	int mode;				// 0 file, 1 directory, 2 link

	block* blocks[NBLOCKS];			// Directly addressible blocks (8 of them)

	struct iblock1* ib1;			/* Singly indirected blocks. 
						 * NBLOCKS_IBLOCK addressable blocks 
						 */

	struct iblock2* ib2;			/* Doubly indirected blocks. 
						 * NIBLOCKS * NBLOCKS_IBLOCK addressable blocks
						 * e.g. 8*8 = 64
						 */

	struct iblock3* ib3;			/* Triply indirected blocks. 
						 * NIBLOCKS^2 * NBLOCKS_IBLOCK addressable blocks
						 * e.g. 8^2*8 == 512 
						 */

} inode;

typedef struct file {
	fs_mode_t mode;				// 0 or 'r' read, 1 or 'w' write
	int f_pos;				// Byte offset seek'ed to
	inode* f_inode;				// pointer to the file's inode
	char name[FS_NAMEMAXLEN];		// filename
} file;

typedef struct link {
	struct inode ino;			// This link's inode
	struct inode dest;			// inode pointing to
	char name[FS_NAMEMAXLEN];		// filename
} link;

typedef struct dir_data {			// The data in a dir to write into blocks
	struct dirent* dirents[FS_MAXDIRS];	// Subdirs
	file* files[FS_MAXFILES];		// Files in this dir
	link* links[FS_MAXLINKS];		// Links in this dir
	int numDirs, numFiles, numLinks;
} dir_data;

typedef struct dirent {				// Directory entry
	struct inode ino;			// Inode
	char name[FS_NAMEMAXLEN];		// filename
} dirent;

typedef struct filesystem {	
	block* blocks[NBLOCKS];

	block* superblock;			// Block 0
	block* free_block_bitmap;		// Block 1. A map of free blocks
} filesystem;

extern char* fname;				/* The name our filesystem will have on disk */
extern filesystem* fs;				/* Pointer to the in-memory representation of the fs, 
						 * e.g. freelist && superblock 
						 */
extern dirent* root;				/* Pointer to the root directory entry */
extern dir_data* root_data;			/* Pointer to root directory data */

extern int fs_openfs();
extern int fs_mkfs();
extern int fs_open(char* filename, char* mode);
extern char* fs_read(int fd, int size);
extern void fs_write(int fd, char* string);
extern void fs_seek(int fd, int offset);
extern void fs_close(int fd);
extern int fs_mkdirent(char* currentdir, char* dirname);
extern void fs_rmdirent(int fd);
extern void fs_link(char* src, char* dest);
extern void fs_unlink(char* name);
extern inode* fs_stat(char* name);