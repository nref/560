#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

dentv* cur_dv = NULL;
char* current_path;

char* sh_get_dv_path(dentv* dv) {
	char* path = NULL;
	fs_path* p = fs.newPath();

	fs.getAbsolutePathDV(dv, p);
	path = fs.stringFromPath(p);

	fs.pathFree(p);
	return path;
}

void sh_update_current_path() {
	current_path = sh_get_dv_path(cur_dv);
}

char* sh_path_cat(char* path, char* suffix) {
	char* ret = NULL;
	fs_path* p = fs.newPath();

	fs.pathAppend(p, path);
	fs.pathAppend(p, suffix);
	ret = fs.stringFromPath(p);

	fs.pathFree(p);
	return ret;
}

void sh_tree_recurse(uint depth, uint maxdepth, dentv* dv) {
	uint i;
	dentv* iterator = NULL;
	char *next, *dv_path;

	if (NULL == dv) return;

	if (NULL == dv->head) {
		printf("%*s" "%s\n", depth*2, " ", "(empty)" );
		return;
	}
	
	dv_path = sh_get_dv_path(dv);
	next = sh_path_cat(dv_path, dv->head->data.dir.name);

	for (i = 0; i < dv->ndirs; i++)	// For each subdir at this level
	{
		iterator = fs.opendir(next);
		if (NULL == iterator) {
			printf("sh_tree_recurse: Could not open directory \"%s\"",next);
			return;
		}
		printf("%*s" "%s\n", depth*2, " ", iterator->name);

		if (depth < maxdepth) {

			sh_tree_recurse(depth+1, maxdepth, iterator);
			free(next);

		}

		if (	NULL == iterator->next ||
			0 == iterator->next->data.dir.ino ||			/* If the dir has a next */			
			iterator->ino->num == iterator->next->data.dir.ino)	/* If the dir doesn't point to itself */
		{
			//fs.closedir(iterator);
			//iterator = NULL;
			break;
		}
			
		next = sh_path_cat(dv_path, iterator->next->data.dir.name);
		//fs.closedir(iterator);
		//iterator = NULL;
	}

	for (i = 0; i < dv->nfiles; i++)		// For each file at this level
		printf("%s\n", dv->files[i].data.file.name);	

	for (i = 0; i < dv->ino->nlinks; i++)	// For each link at this level
		printf("%s\n", dv->links[i].data.file.name);	

	free(dv_path);
}

void sh_tree(char* name) {
	dentv* dv = NULL;

	dv = fs.opendir(name);

	if (NULL == dv) {
		printf("NULL root directory!\n");
		return;
	}

	if (NULL == dv->ino) {
		printf("NULL filesystem inode!\n");
		return;
	}
	
	if ('\0' == dv->name[0] || strlen(dv->name) == 0) {

		printf("Filesystem root name not \"/\"!\n");
		return;
	}

	printf("%s\n", dv->name);
	fs.closedir(dv);

	sh_tree_recurse(1, FS_MAXPATHFIELDS, dv);
}

void sh_stat(char* name) {
	inode* ret;

	ret = fs.stat(name);
	if (NULL == ret) printf("Inode not found for \"%s\"!", name);
	else printf("%d %d", ret->mode, ret->size);
	printf("\n");
}

int sh_mkdir(char* dir_name) {
	int i;
	char* abs_path;
	char* cur_path;
	fs_path *p;

	p = fs.newPath();
	cur_path = fs.getAbsolutePathDV(cur_dv, p);
	fs.pathFree(p);
	
	abs_path = fs.getAbsolutePath(cur_path, dir_name);

	if (NULL == abs_path) {
		printf("Bad or unsupported path\n");
		return FS_ERR;
	}

	i = fs.mkdir(cur_path, abs_path);
	printf("%s\n", fs_responses[i]);
	return i;
}

// Show the shell prompt
void prompt() { 
	if (NULL != cur_dv) 
		printf("%s ", cur_dv->name); 
	printf("> ");
}

// Try to open a preexisting filesystem
void sh_openfs() {
	fs.openfs();
	sh_getfsroot();
}

int sh_cd(char* path) {
	dentv* dv;
	char* abs_path = NULL;

	if (NULL == path || 0 == strlen(path)) {
		printf("Bad arguments\n");
		return FS_ERR;
	}

	sh_update_current_path();
	abs_path = fs.getAbsolutePath(current_path, path);

	if (NULL == abs_path) return FS_ERR;

	dv = fs.opendir(abs_path);
	if (NULL == dv) return FS_ERR;

	cur_dv = dv;
	return FS_OK;
}

int sh_ls(char* path) {
	dentv* dv;
	char* abs_path = NULL;

	if (NULL == path || 0 == strlen(path)) {
		printf("sh_ls: Bad arguments\n");
		return FS_ERR;
	}

	abs_path = fs.getAbsolutePath(current_path, path);
	if (NULL == abs_path) return FS_ERR;

	dv = fs.opendir(abs_path);
	if (NULL == dv) return FS_ERR;

	sh_tree_recurse(0, 0, dv);
	return FS_NORMAL;
}

int sh_getfsroot() {
	dentv* dv = NULL;

	dv = fs.opendir("/");

	if (NULL == dv || NULL == dv->ino)
		return FS_ERR;

	cur_dv = dv;
	sh_update_current_path();
	return FS_OK;
}

int main() {
	int retv;
	char buf[SH_BUFLEN] = "";	// Buffer for user input
	char* delimiter = "\t ";

	fs_path* cmd;

	_fs._debug_print();
	sh_openfs();

	prompt();							
	while (NULL != fgets(buf, SH_BUFLEN-1, stdin)) {		// Get user input

		if (strlen(buf) == 0) { prompt(); continue; }		// Repeat loop on empty input
		buf[strlen(buf)-1] = '\0';				// Remove trailing newline

		// Break input into cmd->fields separated by whitespace. 
		cmd = fs.tokenize(buf, delimiter);

		if (!strcmp(cmd->fields[0], "exit")) break;
		
		else if (!strcmp(cmd->fields[0], "ls")) {

			// If the user provided more than one field
			if (cmd->nfields > 1)
				retv = sh_ls(cmd->fields[1]);
			else retv = sh_ls(cur_dv->name);
		}

		else if (!strcmp(cmd->fields[0], "cd")) {

			if (cmd->nfields > 1)
				retv = sh_cd(cmd->fields[1]); 
		}

		else if (!strcmp(cmd->fields[0], "pwd")) { 
			printf("%s\n", current_path); 
			retv = FS_NORMAL;
		}

		else if (!strcmp(cmd->fields[0], "tree")) { 
			sh_tree("/"); 
			retv = FS_NORMAL;
		}

		else if (!strcmp(cmd->fields[0], "mkdir")) { 
			if (cmd->nfields > 1) {
				retv = sh_mkdir(cmd->fields[1]); 
				retv = FS_NORMAL;
			}
		}

		else if (!strcmp(cmd->fields[0], "stat")) { 
			if (cmd->nfields > 1) {
				sh_stat(cmd->fields[1]); 
				retv = FS_NORMAL;
			}
			retv = FS_ERR;
		}

		else if (!strcmp(cmd->fields[0], "mkfs")) {
			printf("mkfs() ... ");

			fs.mkfs();
			retv = sh_getfsroot();

		} else if (!strcmp(cmd->fields[0], "seek")) {
			printf("Not Implemented\n");
		} else if (!strcmp(cmd->fields[0], "open")) {
			printf("Not Implemented\n");
		} else if (!strcmp(cmd->fields[0], "write")) {
			printf("Not Implemented\n");
		} else if (!strcmp(cmd->fields[0], "close")) {
			printf("Not Implemented\n");
		} else {
			printf("Bad command \"%s\"", buf); 
			retv = FS_NORMAL; 
		}

		if (FS_OK == retv)		printf("OK");
		else if (FS_ERR == retv)	printf("ERROR");
		printf("\n");

		fs.pathFree(cmd);
		prompt();
	}
	printf("exit()\n");
}
