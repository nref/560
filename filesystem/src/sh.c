#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char curDir[FS_MAXPATHLEN]	= "";	// Current shell path
fs_public_interface fs_pub;
_fs_private_interface fs_priv;

void _sh_tree_recurse(filesystem *fs, uint depth, dentv* dv) {
	uint i;
	dentv *iterator = NULL;

	if (NULL == dv->head) {
		printf("%*s" "%s\n", depth*2, " ", "(empty)" );
		return;
	}
	
	/* Dynamically load the dir into memory from disk */
	dv->head->datav.dir = fs_priv._load_dir(fs, dv->head->num);
	iterator = dv->head->datav.dir;
	if (NULL == iterator) return;	/* Reached a leaf node of the directory tree */

	for (i = 0; i < dv->ndirs; i++)	// For each subdir at this level
	{
		printf("%*s" "%s\n", depth*2, " ", iterator->name);	
		_sh_tree_recurse(fs, depth+1, iterator);

		/* Dynamically load the dir into memory from disk */
		iterator->next->datav.dir = fs_priv._load_dir(fs, iterator->next->num);
		iterator = iterator->next->datav.dir;
	}

	// TODO: change these to linked lists as well
	//for (i = 0; i < dir->nfiles; i++)	// For each file at this level
	//	printf("%s\n", dir->files[i].name);	
	//for (i = 0; i < dir->ino->nlinks; i++)	// For each link at this level
	//	printf("%s\n", dir->links[i].name);	
}

void sh_tree(filesystem *fs, char* name) {
	dentv* root_v = NULL;
	inode* ino = NULL;

	if (NULL == fs || NULL == &fs->sb) {
		printf("NULL filesystem!\n");
		return;
	}

	ino = fs_pub.stat(fs, name);
	root_v = ino->datav.dir;

	if (NULL == root_v) {
		printf("NULL root directory!\n");
		return;
	}

	if (NULL == root_v->ino) {
		printf("NULL filesystem inode!\n");
		return;
	}
	
	if ('\0' == root_v->name[0] || strlen(root_v->name) == 0) {

		printf("Filesystem root name not \"/\"!\n");
		return;
	}

	printf("%s\n", root_v->name);
	_sh_tree_recurse(fs, 1, root_v);
}

void sh_stat(filesystem* fs, char* name) {
	inode* ret = fs_pub.stat(fs, name);
	if (NULL == ret) printf("No filesystem yet!");
	else printf("%d %d", ret->mode, ret->size);
	printf("\n");
}

int sh_mkdir(filesystem* fs, char* currentdir, char* dirname) {
	if (NULL == fs) printf("No filesystem yet!");
	else return fs_pub.fs_mkdir(fs, currentdir, dirname);
	printf("\n");
	return FS_ERR;
}

filesystem* sh_mkfs() {	
	filesystem* fs = NULL;
	fs = fs_pub.fs_mkfs();
	return fs;
}

// Show the shell prompt
void prompt() { 
	if (strlen(curDir) > 0) 
		printf("%s ", curDir); 
	printf("> ");
}

// Try to open a preexisting filesystem
filesystem* sh_openfs() {
	filesystem* fs = NULL;

	curDir[0] = '\0';
	fs = _fs._open();
	if (NULL == fs)
		return NULL;
	return fs;
}

dentv* sh_getfsroot(filesystem *fs) {
	dentv* root_v = NULL;

	if (NULL == fs)
		return NULL;
	root_v = fs->root;

	if (NULL == root_v) {
		printf("openfs() problem: root directory is NULL!\n");
		return NULL;
	}

	if (NULL == root_v->ino) {
		printf("openfs() problem: root inode is NULL!\n");
		return NULL;
	}

	return root_v;
}


int main() {

	char* fields[SH_MAXFIELDS];
	char* delimiter = "\t ";
	char* next_field = NULL;

	char cmd[SH_BUFLEN]		= "";		// Buffer for user input
	char cmd_cpy[SH_BUFLEN] = "";			// Copy input because strtok replaces delimiter with '\0'

	uint i, j;
	filesystem* shfs = NULL;
	dentv* root = NULL;
	curDir[0] = '\0';
	
	fs_pub = fs;
	fs_priv = _fs;

	shfs = sh_openfs();
	if (NULL != shfs)
		root = sh_getfsroot(shfs);
	if (NULL != root)
		strcpy(curDir, root->name); 

	prompt();							
	while (NULL != fgets(cmd, SH_BUFLEN-1, stdin)) {		// Get user input

		if (strlen(cmd) == 0) { prompt(); continue; }		// Repeat loop on empty input
		cmd[strlen(cmd)-1] = '\0';				// Remove trailing newline

		// Break input into fields separated by whitespace. 
		// TODO: Move this and other tokenizing loops into dedicated function
		strcpy(cmd_cpy, cmd);					
		next_field = strtok(cmd_cpy, delimiter);		// Get first field

		i = 0;
		while (NULL != next_field) {				// While there is another field
			fields[i] = (char*)malloc(strlen(next_field)+1);// Store this field
			strcpy(fields[i], next_field);				
			next_field = strtok(NULL, delimiter);		// Get the next field
			++i;						// Remember how many fields we have saved
		}
		free(next_field);

		if (!strcmp(fields[0], "exit")) break;
		else if (!strcmp(fields[0], "pwd")) { printf("%s\n", curDir); }
		else if (!strcmp(fields[0], "tree")) { sh_tree(shfs, "/"); }

		else if (!strcmp(fields[0], "mkdir")) { 
			if (i>1) {	// If the user provided more than one field
				int i = sh_mkdir(shfs, curDir, fields[1]); 
				printf("%s\n", errormsgs[i]);
			}
		}

		else if (!strcmp(fields[0], "stat")) { 
			if (i>1)	// If the user provided more than one field
				sh_stat(shfs, fields[1]); 
		}

		else if (!strcmp(fields[0], "mkfs")) {
			printf("mkfs() ... ");

			if (NULL != shfs) {
				fs_pub.fs_delete(shfs);
				shfs = NULL;
			}
			shfs = sh_mkfs();
			if (NULL != shfs)
				root = sh_getfsroot(shfs);
			if (NULL != root) { 
				printf("OK"); 
				strcpy(curDir, root->name); 
			}
			else { printf("ERROR"); }
			printf("\n");

		} else { printf("Bad command\n"); }

		for (j = 0; j < i; j++)
			free(fields[j]);

		prompt();
	}
	printf("exit()\n");
}
