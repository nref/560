#ifndef FS_H
#define FS_H

#include "_fs.h"

typedef struct { 

	/* Path utilities */
	void		(* pathFree)		(fs_path*);
	fs_path*	(* newPath)		();
	fs_path*	(* tokenize)		(const char*, const char*);
	fs_path*	(* pathFromString)	(const char*);
	char*		(* stringFromPath)	(fs_path*);
	char*		(* pathSkipLast)	(fs_path*);
	char*		(* pathGetLast)		(fs_path*);
	int		(* pathAppend)		(fs_path*, const char*);
	char*		(* getAbsolutePathDV)	(dentv*, fs_path *);
	char*		(* getAbsolutePath)	(char* current_dir, char* next_dir);
	char*		(* pathTrimSlashes)	(char*);
	char*		(* strSkipFirst)	(char*);
	char*		(* strSkipLast)		(char*);
	char*		(* trim)		(char*);
	int		(* isNumeric)		(char*);

	void		(* destruct)		();
	void		(* openfs)		();
	void		(* mkfs)		();
	int		(* mkdir)		(char*, char*);
	inode*		(* stat)		(char*);
	int		(* open)		(char*, char*, char*);
	void		(* close)		(fd_t);
	dentv*		(* opendir)		(char*);
	void		(* closedir)		(dentv*);
	void		(* rmdir)		(fd_t);
	char*		(* read)		(fd_t, size_t);
	size_t		(* write)		(fd_t, char*);
	void		(* seek)		(fd_t, size_t);
	void		(* link)		(char*, char*);
	void		(* ulink)		(char*);

} fs_public_interface;
extern fs_public_interface const fs;

#endif /* FS_H */
