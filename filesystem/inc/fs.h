#define BLKSIZE 4096					// Block size in bytes
#define MAXBLOCKS 25600					// Max num allocatable blocks. 4096 bytes * 25600 blocks == 100MB
#define NBLOCKS 10						// Number of direct blocks a block can point to
#define NIBLOCKS 10						// Number of indirect blocks a block can point to

#define FS_READ 0
#define FS_WRITE 1

#define FS_FILE 0
#define FS_DIR 1
#define FS_LINK 2

typedef unsigned long fs_ino_t;			// Inode number is just a ulong
typedef unsigned int fs_mode_t;			// File mode is just an int ('r', 'w', etc)

typedef struct block {
	char data[BLKSIZE];
} block;

typedef struct iblock {	

	block* blocks[NBLOCKS];
	struct iblock* iblocks;				// nth-level indirect blocks
} iblock;

typedef struct dirent {					// Directory entry
	fs_ino_t inode;						// inode number
	char name[];						// filename
} dirent;

typedef struct inode {
	fs_ino_t num;						// Inode number
	int nlinks;							// Number of hard links to the inode
	int size;							// File size
	int mode;							// 0 file, 1 directory, 2 link

	block* blocks[NBLOCKS];		// Direct blocks
	iblock* iblocks[NIBLOCKS];	// First-level indirect blocks
} inode;

typedef struct file {
	fs_mode_t mode;						// 0 or 'r' read, 1 or 'w' write
	int f_pos;							// Byte offset seek'ed to
	inode* f_inode;				// pointer to the file's inode
} file;

typedef struct filesystem {	
	struct block blocks[MAXBLOCKS];

	struct block* superblock;			// Block 0
	struct block* free_block_bitmap;	// Block 1. A map of free blocks
} filesystem;

extern int fs_mkfs();
extern int fs_open(char* filename, char* mode);
extern char* fs_read(int fd, int size);
extern void fs_write(int fd, char* string);
extern void fs_seek(int fd, int offset);
extern void fs_close(int fd);
extern void fs_mkdirent(int currentdir, char* dirname);
extern void fs_rmdirent(int fd);
extern void fs_link(char* src, char* dest);
extern void fs_unlink(char* name);
extern void fs_stat(char* name);