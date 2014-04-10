#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char curDir[FS_MAXPATHLEN];		// Current shell directory
char cmd[SH_BUFLEN];			// Buffer for user input

void _sh_tree_recurse(uint depth, dentry* dir) {
	uint i;

	dentry *iterator = dir->head;

	for (i = 0; i < dir->ndirs; i++)	// For each subdir at this level
	{
		printf("%*s" "%s\n", depth*2, " ", iterator->name);	
		_sh_tree_recurse(depth+1, iterator);
		iterator = iterator->next;
	}

	// TODO: change these to linked lists as well
	for (i = 0; i < dir->nfiles; i++)	// For each file at this level
		printf("%s\n", dir->files[i].name);	
	for (i = 0; i < dir->ino.nlinks; i++)	// For each link at this level
		printf("%s\n", dir->links[i].name);	
}

void sh_tree() {
	dentry* root;
	
	if (NULL != &fs->sb->root)
		root = fs->sb->root.owner.dir_o;
	else {
		printf("No filesystem yet!\n");
		return;
	}

	if (	NULL == fs || 
		NULL == fs->sb || 
		NULL == root || 
		strlen(root->name) == 0) {

		printf("No filesystem yet!\n");
		return;
	}
	printf("%s\n", root->name);
	_sh_tree_recurse(1, root);
}

void sh_stat(char* name) {
	inode* ret = fs_stat(name);
	if (NULL == ret) printf("No filesystem yet!");
	else printf("%d %d", ret->mode, ret->size);
	printf("\n");
}

int sh_mkdir(char* currentdir, char* dirname) {
	if (NULL == fs) printf("No filesystem yet!");
	else return fs_mkdir(curDir, dirname);
	return FS_ERR;
}

int sh_mkfs() {	
	return fs_mkfs(); 
}

// Show the shell prompt
void prompt() { 
	if (strlen(curDir) > 0) 
		printf("%s ", curDir); 
	printf("> ");
}

// Try to open a preexisting filesystem
dentry* sh_openfs() {
	int i;
	dentry* root;

	i = fs_openfs();
	
	if (NULL != &fs->sb->root)
		root = fs->sb->root.owner.dir_o;
	else {
		printf("openfs() problem: root inode is NULL!\n");
		return NULL;
	}

	if (i > 0)
		strcpy(curDir, root->name); 
	return root;
}

int main(int argc, char** argv) {

	char* fields[SH_MAXFIELDS];
	char* delimiter = "\t ";
	char* next_field = NULL;
	char cmd_cpy[SH_BUFLEN];
	int i;
	dentry* root;
	curDir[0] = '\0';

	root = sh_openfs();	

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
			
			fields[i] = (char*)malloc(			// Store this field
				strlen(next_field)*sizeof(char));
			strcpy(fields[i], next_field);				
			
			next_field = strtok(NULL, delimiter);		// Get the next field
			++i;						// Remember how many fields we have saved
		}

		if (!strcmp(fields[0], "exit")) break;
		else if (!strcmp(fields[0], "pwd")) { printf("%s\n", curDir); }
		else if (!strcmp(fields[0], "tree")) { sh_tree(); }

		else if (!strcmp(fields[0], "mkdir")) { 
			if (i>1) {	// If the user provided more than one field
				int retval = sh_mkdir(curDir, fields[1]); 
				if (retval >= 0) { printf("OK"); }
				else printf("ERROR");
				printf("\n");
			}
		}

		else if (!strcmp(fields[0], "stat")) { 
			if (i>1)	// If the user provided more than one field
				sh_stat(fields[1]); 
		}

		else if (!strcmp(fields[0], "mkfs")) {
			printf("mkfs() ... ");

			if (FS_ERR != sh_mkfs()) { 
				printf("OK"); 
				strcpy(curDir, root->name); 
			}
			else { printf("ERROR"); }
			printf("\n");
		}
		prompt();
	}
	printf("exit()\n");
}