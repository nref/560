#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char current_dir[FS_MAXPATHLEN]	= "";	// Current shell path
//fs_public_interface fs_pub;
//fs_private_interface fs_priv;


char* sh_getAbsolutePath(char* path) {
	fs_path* p;
	char* abs_path;
	
	p = fs.pathFromString(current_dir);

	if ('/' == path[0])	/* Path is already absolute*/
		return path;

	if ('.' == path[0]) {
		if ('.' == path[1])	printf(".. not yet implemented\n");
		else			printf(". not yet implemented\n");
		return NULL;
	}

	fs.pathAppend(p, path);
	abs_path = fs.stringFromPath(p);

	free(p);
	return abs_path;
}

void sh_tree_recurse(filesystem *fs, uint depth, uint maxdepth, dentv* dv) {
	uint i;
	dentv *iterator = NULL;

	if (NULL == dv->head) {
		printf("%*s" "%s\n", depth*2, " ", "(empty)" );
		return;
	}
	
	
	if (!dv->head->v_attached)
		dv->head->datav.dir = _fs._load_dir(fs, dv->head->num);	/* Dynamically load into memory from disk */

	iterator = dv->head->datav.dir;
	if (NULL == iterator) return;			/* Reached a leaf node of the directory tree */

	for (i = 0; i < dv->ndirs; i++)			// For each subdir at this level
	{
		printf("%*s" "%s\n", depth*2, " ", iterator->name);

		if (depth < maxdepth)
			sh_tree_recurse(fs, depth+1, maxdepth, iterator);

		if (!iterator->next->v_attached)
			iterator->next->datav.dir = _fs._load_dir(fs, iterator->next->num);

		iterator = iterator->next->datav.dir;
		if (NULL == iterator) continue;
	}

	for (i = 0; i < dv->nfiles; i++) {		// For each file at this level
		//if (!dv->files[i].v_attached)
		//	dv->files[i].datav.file = fs_priv._load_file(fs, dv->files[i].num);

		printf("%s\n", dv->files[i].data.file.name);	
	}

	for (i = 0; i < dv->ino->nlinks; i++)		// For each link at this level
		printf("%s\n", dv->links[i].data.file.name);	
}

void sh_tree(filesystem *shfs, char* name) {
	dentv* root_v = NULL;
	inode* ino = NULL;

	if (NULL == shfs || NULL == &shfs->sb) {
		printf("NULL filesystem!\n");
		return;
	}

	ino = fs.stat(shfs, name);
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
	sh_tree_recurse(shfs, 1, FS_MAXPATHFIELDS, root_v);
}

void sh_stat(filesystem* shfs, char* name) {
	inode* ret;
	if (NULL == shfs) printf("No filesystem yet!");
	
	ret = fs.stat(shfs, name);
	if (NULL == ret) printf("Inode not found for \"%s\"!", name);
	else printf("%d %d", ret->mode, ret->size);
	printf("\n");
}

int sh_mkdir(filesystem* shfs, char* currentdir, char* dirname) {
	int i;
	char* abs_path;

	if (NULL == shfs) {
		printf("No filesystem yet!\n");
		return FS_ERR;
	}

	abs_path = sh_getAbsolutePath(dirname);

	if (NULL == abs_path) {
		printf("Bad or unsupported path\n");
		return FS_ERR;
	}

	i = fs.fs_mkdir(shfs, currentdir, abs_path);
	printf("%s\n", fs_responses[i]);
	return FS_OK;
}

filesystem* sh_mkfs() {	
	filesystem* shfs = NULL;
	shfs = fs.fs_mkfs();
	return shfs;
}

// Show the shell prompt
void prompt() { 
	if (strlen(current_dir) > 0) 
		printf("%s ", current_dir); 
	printf("> ");
}

// Try to open a preexisting filesystem
filesystem* sh_openfs() {
	filesystem* fs = NULL;

	current_dir[0] = '\0';
	fs = _fs._open();
	if (NULL == fs)
		return NULL;
	return fs;
}

char* sh_cd(filesystem *shfs, char* path) {
	inode* ino;
	char* abs_path = NULL;

	if (NULL == path || 0 == strlen(path)) {
		printf("Bad arguments\n");
		return NULL;
	}

	abs_path = sh_getAbsolutePath(path);

	if (NULL == abs_path) {
		printf("Bad or unsupported path\n");
		return NULL;
	}

	ino = fs.stat(shfs, abs_path);

	if (NULL == ino) {
		printf("No such directory \"%s\"\n", path);
		return NULL;
	}

	if (FS_DIR != ino->mode) {
		printf("Found \"%s\", but it's not a directory.\n", path);
		return NULL;
	}

	return abs_path;
}

void sh_ls(filesystem* shfs, char* path) {
	inode* ino;
	char* abs_path;

	if (NULL == path || 0 == strlen(path)) {
		printf("Bad arguments");
		return;
	}

	abs_path = sh_getAbsolutePath(path);

	if (NULL == abs_path) {
		printf("Bad or unsupported path\n");
		return;
	}

	ino = fs.stat(shfs, abs_path);

	if (NULL == ino) {
		printf("No such directory \"%s\"\n", path);
		return;
	}

	if (FS_DIR != ino->mode) {
		printf("Found \"%s\", but it's not a directory.\n", path);
		return;
	}

	if (!ino->v_attached)
		ino->datav.dir = _fs._load_dir(shfs, ino->num);

	if (NULL == ino->datav.dir) {
		printf("Could not load \"%s\".\n", path);
		return;
	}

	sh_tree_recurse(shfs, 0, 0, ino->datav.dir);
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
	uint i;
	char buf[SH_BUFLEN]		= "";		// Buffer for user input
	char* delimiter = "\t ";

	filesystem* shfs = NULL;
	fs_path* cmd;
	dentv* root = NULL;
	current_dir[0] = '\0';

	_fs._debug_print();
	shfs = sh_openfs();

	if (NULL != shfs)
		root = sh_getfsroot(shfs);

	if (NULL != root)
		strcpy(current_dir, root->name); 

	prompt();							
	while (NULL != fgets(buf, SH_BUFLEN-1, stdin)) {		// Get user input

		if (strlen(buf) == 0 || !strcmp(buf, "\n")) { prompt(); continue; }		// Repeat loop on empty input or return character
        buf[strlen(buf)-1] = '\0';				// Remove trailing newline

		// Break input into cmd->fields separated by whitespace. 
		cmd = _fs._tokenize(buf, delimiter);

        if (!strcmp(cmd->fields[0], "exit")) break;
		
		else if (!strcmp(cmd->fields[0], "ls")) {
			if (cmd->nfields > 1)
				sh_ls(shfs, cmd->fields[1]);
			else sh_ls(shfs, current_dir);
		}

		else if (!strcmp(cmd->fields[0], "cd")) {

			if (cmd->nfields > 1) { // If the user provided more than one field

				char* tmp = sh_cd(shfs, cmd->fields[1]); 
				if (NULL != tmp && '\0' != tmp[0])
					strcpy(current_dir, tmp);
			}
		}

		else if (!strcmp(cmd->fields[0], "pwd")) { printf("%s\n", current_dir); }

		else if (!strcmp(cmd->fields[0], "tree")) { sh_tree(shfs, "/"); }

		else if (!strcmp(cmd->fields[0], "mkdir")) { 
			if (cmd->nfields > 1) {
				sh_mkdir(shfs, current_dir, cmd->fields[1]); 
			}
		}

		else if (!strcmp(cmd->fields[0], "stat")) { 
			if (cmd->nfields > 1) {
				sh_stat(shfs, cmd->fields[1]); 
			}
		}

		else if (!strcmp(cmd->fields[0], "mkfs")) {
			printf("mkfs() ... ");

			if (NULL != shfs) {
				fs.fs_delete(shfs);
				shfs = NULL;
			}
			shfs = sh_mkfs();
			if (NULL != shfs)
				root = sh_getfsroot(shfs);
			if (NULL != root) { 
				printf("OK"); 
				strcpy(current_dir, root->name); 
			}
			else { printf("ERROR"); }
			printf("\n");

            
        } else if (!strcmp(cmd->fields[0], "seek")) {
            printf("Not Implemented\n");
        } else if (!strcmp(cmd->fields[0], "open")) {
            printf("Not Implemented\n");
        } else if (!strcmp(cmd->fields[0], "write")) {
            printf("Not Implemented\n");
        } else if (!strcmp(cmd->fields[0], "close")) {
            printf("Not Implemented\n");
		} else { printf("Bad command \"%s\"\n", buf); }

        for (i = 0; i < cmd->nfields; i++)
			free(cmd->fields[i]);
		free(cmd);

		prompt();
	}
	printf("exit()\n");
}
