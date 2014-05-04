#include "fs.h"

#define SH_BUFLEN 1024*1024	// How many chars to accept per line from user
#define SH_MAXFIELDS 8		// How many whitespace-separated fields to accept from user
#define SH_MAXFIELDSIZE 1024*1024

typedef struct fs_args {
	char fields[8][SH_MAXFIELDSIZE]; /* A struct for storing command arguments */
	size_t nfields;
	size_t firstField;
	size_t fieldSize;

} fs_args;

extern void		sh_traverse_files(dentv* dv, int depth);
extern void		sh_traverse_links(dentv* dv, int depth);
extern int		sh_getfsroot	();
extern void		sh_openfs	();
extern void		sh_mkfs		();
extern int		sh_open		(char* filename, char* mode);
extern char*		sh_read		(int fd, int size);
extern int		sh_write	(fs_args*, fs_args*);
extern void		sh_seek		(int fd, int offset);
extern void		sh_close	(int fd);
extern int		sh_mkdir	(char* name);
extern void		sh_rmdir	(int fd);
extern int		sh_cd		(char* path);
extern void		sh_link		(char* src, char* dest);
extern void		sh_unlink	(char* path);
extern void		sh_stat		(char* path);
extern int		sh_ls		(char* path);
extern void		sh_cat		(char* path);
extern void		sh_cp		(char* src, char* dest);
extern void		sh_tree		(char* name);
extern void		sh_import	();
extern void		sh_export	();
