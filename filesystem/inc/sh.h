#include "fs.h"

#define SH_BUFLEN 1024*1024	// How many chars to accept per line from user
#define SH_MAXFIELDS 8		// How many whitespace-separated fields to accept from user
#define SH_MAXFIELDSIZE 512*512

#define SH_MAXFSARGS 16

typedef struct fs_args {
	char fields[SH_MAXFSARGS][SH_MAXFIELDSIZE]; /* A struct for storing command arguments */
	size_t quoted_fields[SH_MAXFSARGS];
	size_t nfields;
	size_t firstField;
	size_t fieldSize;

} fs_args;

extern void		sh_traverse_files(dentv* dv, int depth);
extern void		sh_traverse_links(dentv* dv, int depth);
extern int		sh_getfsroot	();
extern void		sh_openfs	();
extern void		sh_mkfs		();
extern int		sh_open		(fs_args*);
extern int		sh_close	(int fd);
extern char*		sh_read		(int fd, size_t size);
extern int		sh_write	(fs_args*);
extern void		sh_seek		(int fd, int offset);
extern int		sh_mkdir	(char* name);
extern int		sh_rmdir	(char* name);
extern int		sh_cd		(char* path);
extern void		sh_link		(char* src, char* dest);
extern void		sh_unlink	(char* path);
extern int		sh_stat		(char* path);
extern int		sh_ls		(char* path);
extern int		sh_cat		(fs_args*);
extern int		sh_cp		(char* src, char* dest);
extern void		sh_tree		(char* name);
extern int		sh_import	(fs_args*);
extern int		sh_export	(fs_args*);
