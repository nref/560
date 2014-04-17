#ifndef FS_H
#define FS_H

#include "_fs.h"

//extern void			fs_delete	(filesystem*);
//extern filesystem*		fs_mkfs		();
//extern int			fs_mkdir	(filesystem *, char* currentdir, char* dirname);
//extern inode*			stat		(filesystem *fs, char* name);
//extern int			fs_open		(char* filename, char* mode);
//extern void			fs_close	(int fd);
//extern void			fs_rmdir	(int fd);
//extern char*			fs_read		(int fd, int size);
//extern void			fs_write	(int fd, char* string);
//extern void			fs_seek		(int fd, int offset);
//extern void			fs_link		(inode_t, inode_t);
//extern void			fs_unlink	(inode_t);

typedef struct { 
	void (* fs_delete)(filesystem*);
	filesystem* (* fs_mkfs)();
	int (* fs_mkdir)(filesystem*, char*, char*);
	inode* (* stat)(filesystem*, char*);
	int (* fs_open)(char*, char*);
	void (*fs_close)(int);
	void (*fs_rmdir)(int);
	char* (* fs_read)(int, int);
	void (*fs_write)(int, char*);
	void (*fs_seek)(int, int);
	void (*fs_link)(inode_t, inode_t);
	void (*fs_unlink)(inode_t);

} fs_public_interface;
extern fs_public_interface const fs;

#endif /* FS_H */
