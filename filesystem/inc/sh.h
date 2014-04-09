#include "fs.h"

#define SH_BUFLEN 1024		// How many chars to accept per line from user
#define SH_MAXFIELDS 16		// How many whitespace-separated fields to accept from user

extern int sh_openfs();
extern int sh_mkfs();
extern int sh_open(char* filename, char* mode);
extern char* sh_read(int fd, int size);
extern void sh_write(int fd, char* string);
extern void sh_seek(int fd, int offset);
extern void sh_close(int fd);
extern int sh_mkdentry(char* currentdir, char* dirname);
extern void sh_rmdentry(int fd);
extern void sh_cd();
extern void sh_link(char* src, char* dest);
extern void sh_unlink(char* name);
extern void sh_stat(char* name);
extern void sh_ls();
extern void sh_cat(char *filename);
extern void sh_cp(char* src, char* dest);
extern void sh_tree();
extern void sh_import();
extern void sh_export();