#define BLKSIZE 4096					// Block size in bytes
//#define MAXBLOCKS 25600				// Max num allocatable blocks. 4096 bytes * 25600 blocks == 100MB
#define MAXBLOCKS 256					// Temporary for testing: 1MB filesystem
#define NBLOCKS 10						// Number of direct blocks a block can point to
#define NIBLOCKS 10						// Number of indirect blocks a block can point to

#define FS_NAMEMAXLEN 256				// Max length of a directory or file name
#define FS_MAXPATHLEN 65535				// Maximum path length
#define FS_MAXPATHFIELDS 16				// Max number of forward-slash "/"-separated fields in a path

#define FS_MAXDIRS 256					// Max number of subdirs in a dir
#define FS_MAXFILES 4096				// Max number of files in a dir
#define FS_MAXLINKS 4096				// Max number of links in a dir

#define FS_READ 0
#define FS_WRITE 1

#define FS_ERR -1
#define FS_FILE 0
#define FS_DIR 1
#define FS_LINK 2

typedef unsigned long fs_ino_t;			// Inode number is just a ulong
typedef unsigned int fs_mode_t;			// File mode is just an int (0 =='r', 1 =='w')

typedef struct block {
	char data[BLKSIZE];
} block;

typedef struct iblock {	

	block* blocks[NBLOCKS];
	struct iblock* iblocks;			// nth-level indirect blocks
} iblock;

typedef struct inode {
	fs_ino_t num;				// Inode number
	int nlinks;				// Number of hard links to the inode
	int size;				// File size
	int mode;				// 0 file, 1 directory, 2 link

	block* blocks[NBLOCKS];			// Direct blocks
	iblock* iblocks[NIBLOCKS];		// First-level indirect blocks
} inode;

typedef struct file {
	fs_mode_t mode;				// 0 or 'r' read, 1 or 'w' write
	int f_pos;				// Byte offset seek'ed to
	inode* f_inode;				// pointer to the file's inode
} file;

typedef struct link {
	inode ino;				// This link's inode
	inode dest;				// inode pointing to
} link;

typedef struct dirent {				// Directory entry
	inode ino;				// Inode
	struct dirent* dirents[FS_MAXDIRS];	// Subdirs
	file* files[FS_MAXFILES];		// Files in this dir
	link* links[FS_MAXLINKS];		// Links in this dir

	int numDirs, numFiles, numLinks;

	char name[FS_NAMEMAXLEN];		// filename
} dirent;

typedef struct filesystem {	
	block* blocks[NBLOCKS];

	block* superblock;			// Block 0
	block* free_block_bitmap;		// Block 1. A map of free blocks
} filesystem;

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