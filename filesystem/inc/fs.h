#ifndef FS_H
#define FS_H

#include "_fs.h"

typedef struct { 
	void		(* fs_delete)(filesystem*);
	filesystem*	(* fs_mkfs)();
	int		(* fs_mkdir)(filesystem*, char*, char*);
	inode*		(* stat)(filesystem*, char*);
	int		(* attach_datav)(filesystem*, inode* ino);
	int		(* fs_open)(char*, char*);
	void		(* fs_close)(int);
	void		(* fs_rmdir)(int);
	char*		(* fs_read)(int, int);
	void		(* fs_write)(int, char*);
	void		(* fs_seek)(int, int);
	void		(* fs_link)(inode_t, inode_t);
	void		(* fs_unlink)(inode_t);

} fs_public_interface;
extern fs_public_interface const fs;

#endif /* FS_H */
