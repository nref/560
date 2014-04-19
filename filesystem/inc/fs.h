#ifndef FS_H
#define FS_H

#include "_fs.h"

typedef struct { 

	/* Path utilities */
	void		(* pathFree)		(fs_path* p);
	fs_path*	(* newPath)		();
	fs_path*	(* tokenize)		(const char*, const char*);
	fs_path*	(* pathFromString)	(const char*);
	char*		(* stringFromPath)	(fs_path*);
	char*		(* pathSkipLast)	(fs_path* p);
	char*		(* pathGetLast)		(fs_path* p);
	int		(* pathAppend)		(fs_path*, const char*);
	char*		(* getAbsolutePathDV)	(dentv* dv, fs_path *p);
	char*		(* getAbsolutePath)	(char* current_dir, char* next_dir);
	char*		(* pathTrimSlashes)	(char* path);
	char*		(* strSkipFirst)	(char* cpy);
	char*		(* strSkipLast)		(char* cpy);

	void		(* destruct)		();
	void		(* openfs)		();
	void		(* mkfs)		();
	int		(* mkdir)		(char*, char*);
	inode*		(* stat)		(char*);
	filev*		(* open)		(char*, char*);
	void		(* close)		(filev*);
	dentv*		(* opendir)		(char*);
	void		(* closedir)		(dentv*);
	void		(* rmdir)		(int);
	char*		(* read)		(int, int);
	void		(* write)		(filev*, char*);
	void		(* seek)		(int, int);
	void		(* link)		(inode_t, inode_t);
	void		(* ulink)		(inode_t);

} fs_public_interface;
extern fs_public_interface const fs;

#endif /* FS_H */
