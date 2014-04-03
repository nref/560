#define BLKSIZE 4096	// Block size in bytes
#define NBLOCKS 25600	// Max num allocatable blocks
                        // 4096 bytes * 25600 blocks == 100MB

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