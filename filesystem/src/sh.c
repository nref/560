#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char curDir[FS_MAXPATHLEN];		// Current shell directory
char cmd[SH_BUFLEN];			// Buffer for user input

void _sh_tree_recurse(uint depth, dentry_volatile* dv) {
	uint i;
	dentry_volatile *iterator;

	if (NULL == dv->head) {
		printf("%*s" "%s\n", depth*2, " ", "(empty)" );
		return;
	}
	
	iterator = dv->head->data_v.dir;


	for (i = 0; i < dv->ndirs; i++)	// For each subdir at this level
	{
		printf("%*s" "%s\n", depth*2, " ", iterator->name);	
		_sh_tree_recurse(depth+1, iterator);
		iterator = iterator->next->data_v.dir;
	}

	// TODO: change these to linked lists as well
	//for (i = 0; i < dir->nfiles; i++)	// For each file at this level
	//	printf("%s\n", dir->files[i].name);	
	//for (i = 0; i < dir->ino->nlinks; i++)	// For each link at this level
	//	printf("%s\n", dir->links[i].name);	
}

void sh_tree(filesystem *fs, char* name) {
	dentry_volatile* root_v = NULL;
	inode* ino = NULL;

	if (NULL == fs || NULL == &fs->sb) {
		printf("NULL filesystem!\n");
		return;
	}

	ino = fs_stat(fs, name);
	root_v = ino->data_v.dir;

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
	_sh_tree_recurse(1, root_v);
}

void sh_stat(filesystem* fs, char* name) {
	inode* ret = fs_stat(fs, name);
	if (NULL == ret) printf("No filesystem yet!");
	else printf("%d %d", ret->mode, ret->size);
	printf("\n");
}

int sh_mkdir(filesystem* fs, char* currentdir, char* dirname) {
	if (NULL == fs) printf("No filesystem yet!");
	else return fs_mkdir(fs, currentdir, dirname);
	printf("\n");
	return FS_ERR;
}

filesystem* sh_mkfs() {	
	filesystem* fs = NULL;
	fs = fs_mkfs();
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
	fs = fs_openfs();
	if (NULL == fs)
		return NULL;
	return fs;
}

dentry_volatile* sh_getfsroot(filesystem *fs) {
	dentry_volatile* root_v = NULL;

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
	char cmd_cpy[SH_BUFLEN];
	uint i, j;
	filesystem* fs = NULL;
	dentry_volatile* root = NULL;
	curDir[0] = '\0';

	fs = sh_openfs();
	if (NULL != fs)
		root = sh_getfsroot(fs);
	if (NULL != root)
		strcpy(curDir, root->name); 

	prompt();							
	while (NULL != fgets(cmd, SH_BUFLEN-1, stdin)) {		// Get user input

		if (NULL == cmd) { prompt(); continue; }		// Sanity check
		cmd[strlen(cmd)-1] = '\0';				// Remove trailing newline
		if (strlen(cmd) == 0) { prompt(); continue; }		// Repeat loop on empty input

		// Break input into fields separated by whitespace
		strcpy(cmd_cpy, cmd);					// Copy input because strtok replaces delimiter with '\0'
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
		else if (!strcmp(fields[0], "tree")) { sh_tree(fs, "/"); }

		else if (!strcmp(fields[0], "mkdir")) { 
			if (i>1) {	// If the user provided more than one field
				int retval = sh_mkdir(fs, curDir, fields[1]); 
				if (FS_ERR != retval) { printf("OK"); }
				else printf("ERROR");
				printf("\n");
			}
		}

		else if (!strcmp(fields[0], "stat")) { 
			if (i>1)	// If the user provided more than one field
				sh_stat(fs, fields[1]); 
		}

		else if (!strcmp(fields[0], "mkfs")) {
			printf("mkfs() ... ");

			if (NULL != fs) {
				fs_delete(fs);
				fs = NULL;
			}
			fs = sh_mkfs();
			if (NULL != fs)
				root = sh_getfsroot(fs);
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
