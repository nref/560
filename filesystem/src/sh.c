#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

dentv* cur_dv = NULL;
char* current_path;

static fs_args* newArgs() {
	uint i = 0;
	fs_args* args = (fs_args*)malloc(sizeof(fs_args));
	args->fieldSize = SH_MAXFIELDSIZE;

	for (i = 0; i < SH_MAXFIELDS; i++) {
		args->fields[i] = (char*)malloc(SH_MAXFIELDSIZE);
		memset(args->fields[i], 0, SH_MAXFIELDSIZE);
	}

	args->nfields = 0;
	args->firstField = 0;

	return args;
}

static void argsFree(fs_args* p) {
	uint i;
	if (NULL == p) return;

	for (i = 0; i < SH_MAXFIELDS; i++)
		free(p->fields[i]);
	free(p);
}

/* Split a string on delimiter(s) */
void sh_tokenize(const char* str, const char* delim, fs_args* args) {
	size_t i = 0;
	char* next_field	= NULL;
	size_t len;
	char* str_cpy;

	if (NULL == str || '\0' == str[0]) return;

	len = strlen(str);
	str_cpy = (char*)malloc(sizeof(char) * len + 1);
	
	// Copy the input because strtok replaces delimieter with '\0'
	strncpy(str_cpy, str, min(SH_MAXFIELDSIZE-1, len+1));	
	str_cpy[len] = '\0';

	// Split the path on delim
	next_field = strtok(str_cpy, delim);
	while (NULL != next_field) {

		i = args->nfields;
		len = strlen(next_field);

		strncpy(args->fields[i], next_field, min(SH_MAXFIELDSIZE-1, len+1));	
		args->fields[i][len] = '\0';

		if (args->nfields + 1 == SH_MAXFIELDS)
			break;

		next_field = strtok(NULL, delim);
		args->nfields++;
	}

	free(str_cpy);
}

char* sh_get_dv_path(dentv* dv) {
	char* path = NULL;
	fs_path* p;
	
	if (!strcmp(dv->name, "/"))
		return dv->name;

	p = fs.newPath();
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
	char next[FS_NAMEMAXLEN];
	char *dv_path;

	memset(next, 0, FS_NAMEMAXLEN);

	if (NULL == dv) return;
	dv_path = sh_get_dv_path(dv);

	if (NULL == dv->head && 
		dv->ndirs == 0 &&
		dv->nfiles == 0 &&
		dv->nlinks == 0) 
	{
		printf("%*s" "%s\n", depth*2, " ", "(empty)" );
		free(dv_path);
		return;
	}

	if (NULL != dv->head) {
		strcat(next, sh_path_cat(dv_path, dv->head->data.dir.name));

		for (i = 0; i < dv->ndirs; i++)	// For each subdir at this level
		{
			iterator = fs.opendir(next);
			if (NULL == iterator) {
				printf("sh_tree_recurse: Could not open directory \"%s\"",next);
				return;
			}
			printf("%*s" "%s [dir]\n", depth*2, " ", iterator->name);

			if (depth < maxdepth) {

				sh_tree_recurse(depth+1, maxdepth, iterator);
			}

			if (	NULL == iterator->next ||
				0 == iterator->next->data.dir.ino ||			/* If the dir has a next */			
				iterator->ino->num == iterator->next->data.dir.ino)	/* If the dir doesn't point to itself */
			{
				//fs.closedir(iterator);
				//iterator = NULL;
				break;
			}
			
			strcat(next, sh_path_cat(dv_path, iterator->next->data.dir.name));
			//fs.closedir(iterator);
			//iterator = NULL;
		}
	}

	for (i = 0; i < dv->nfiles; i++) {	// For each file at this level
		//fs.open(dv_path, dv->files[i]->data.file.name, "r");
		printf("%*s" "%s [file]\n", depth*2, " ", dv->files[i]->data.file.name);
	}

	for (i = 0; i < dv->ino->nlinks; i++)	// For each link at this level
		printf("%*s" "%s [link]\n", depth*2, " ", dv->links[i]->data.file.name);

	//free(dv_path);
}

void sh_tree(char* name) {
	dentv* dv = NULL;

	if (NULL == name) {
		printf("No filesystem.\n");
		return;
	}

	dv = fs.opendir(name);

	if (NULL == dv) {
		printf("tree: Could not open the directory. (null dv) \"%s\"\n", name);
		return;
	}

	if (NULL == dv->ino) {
		printf("tree: Could not open the directory. (null inode)\n");
		return;
	}

	printf("%s\n", dv->name);

	sh_tree_recurse(1, FS_MAXPATHFIELDS, dv);
	//fs.closedir(dv);
}

int sh_write(fs_args* cmd, fs_args* cmd_quotes) {
	
	size_t write_byte_count;
	size_t write_expected_byte_count;
	fd_t fd;
	uint i;
	int retv;

	char* write_buf = NULL;


	if (cmd->nfields < 2) return FS_ERR;

	if (!fs.isNumeric(cmd->fields[1]))
		return FS_ERR;
	else {
		fd = atoi(cmd->fields[1]);

		if (NULL != cmd_quotes) {
			write_expected_byte_count = strlen(cmd_quotes->fields[1]);
			write_buf = (char*)malloc(write_expected_byte_count);
			memset(write_buf, 0, write_expected_byte_count);

			strcat(write_buf, cmd_quotes->fields[1]);
		}
		else {
			write_buf = (char*)malloc(SH_MAXFIELDSIZE);
			for (i = 2; i < cmd->nfields; i++)
			{
				strcat(write_buf, cmd->fields[i]);
				strcat(write_buf, " ");
			}
			write_expected_byte_count = strlen(write_buf);
		}

		write_byte_count = fs.write(fd, write_buf);

		if (write_expected_byte_count != write_byte_count) retv = FS_ERR;
		else {
			printf("Wrote %lu bytes to fd %d ", write_byte_count, fd);
			retv = FS_OK;
		}
	}
	return retv;
}

void sh_stat(char* name) {
	inode* ret;

	ret = fs.stat(name);
	if (NULL == ret) printf("Inode not found for \"%s\"!", name);
	else printf("%d %lu", ret->mode, ret->size);
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

	if (OK == i)
		return FS_OK;
	else printf("%s\n", fs_responses[i]);
	return FS_ERR;
}

// Show the shell prompt
void prompt() { 
	if (NULL != cur_dv) 
		printf("%s ", cur_dv->name); 
	printf("> ");
}

int sh_cd(char* path) {
	dentv* dv;
	char* abs_path = NULL;

	if (NULL == path || 0 == strlen(path)) {
		printf("Bad arguments\n");
		return FS_ERR;
	}

	abs_path = fs.getAbsolutePath(current_path, path);

	if (NULL == abs_path) return FS_ERR;

	dv = fs.opendir(abs_path);
	if (NULL == dv) return FS_ERR;

	cur_dv = dv;
	sh_update_current_path();
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
	int retv = 0;
	int input_is_quoted;
	char buf[SH_BUFLEN] = "";	// Buffer for user input
	char* delimiter = "\t ";

	fs_args* cmd;
	fs_args* cmd_quotes;
	current_path = NULL;

	_fs._debug_print();
	fs.openfs();
	sh_getfsroot();

	prompt();
        memset(buf, 0, SH_BUFLEN); // Clean buffer before printing
	while (NULL != fgets(buf, SH_BUFLEN-1, stdin)) {	// Get user input

		if (strlen(buf) == 0 || '\n' == buf[0]) { 
			prompt(); 
			continue;		// Repeat loop on empty input
		}		

		buf[strlen(buf)-1] = '\0';	// Remove trailing newline

		// Split input on whitespace. 
		cmd = newArgs();
		sh_tokenize(buf, delimiter, cmd);
		
		/* Split input on quoted strings.
		 * First element will be the unquoted leading input,
		 * Second to n elements will be quoted fields, and the (n+1)th
		 * Element will be the trailing unquoted input
		 * e.g. write 0 "this is a long string   " works
		 *as well as "open "long file name" w */
		cmd_quotes = newArgs();
		sh_tokenize(buf, "\"", cmd_quotes);

		input_is_quoted = false;
		if (cmd_quotes->nfields > 1)
			input_is_quoted = true;

		if (!strcmp(cmd->fields[0], "exit")) break;
		
		else if (!strcmp(cmd->fields[0], "ls")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

	
			// If the user provided more than one field
			if (cmd->nfields > 1)

				if (input_is_quoted)	retv = sh_ls(cmd_quotes->fields[1]);
				else			retv = sh_ls(cmd->fields[1]);
			else retv = sh_ls(current_path);
		}

		else if (!strcmp(cmd->fields[0], "cd")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (cmd->nfields > 1) {
				if (input_is_quoted)	retv = sh_cd(cmd_quotes->fields[1]);
				else			retv = sh_cd(cmd->fields[1]); 
			}
		}

		else if (!strcmp(cmd->fields[0], "pwd")) { 
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			printf("%s\n", current_path); 
			retv = FS_NORMAL;
		}

		else if (!strcmp(cmd->fields[0], "tree")) { 
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			sh_tree(current_path); 
			retv = FS_NORMAL;
		}

		else if (!strcmp(cmd->fields[0], "mkdir")) { 
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (cmd->nfields > 1) {
				if (input_is_quoted)	retv = sh_mkdir(cmd_quotes->fields[1]);
				else			retv = sh_mkdir(cmd->fields[1]); 
			}
		}

		else if (!strcmp(cmd->fields[0], "stat")) { 
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (cmd->nfields > 1) {

				if (input_is_quoted)	sh_stat(cmd_quotes->fields[1]); 
				else			sh_stat(cmd->fields[1]); 
				retv = FS_NORMAL;
			}
			else retv = FS_ERR;
		}

		else if (!strcmp(cmd->fields[0], "mkfs")) {
			printf("mkfs() ... ");

			fs.mkfs();
			retv = sh_getfsroot();

		} else if (!strcmp(cmd->fields[0], "seek")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}
			if(cmd->nfields == 3) {
				fs.seek((fd_t)atoi(cmd->fields[1]), (size_t)atoi(cmd->fields[2]));
			}
			
		} else if (!strcmp(cmd->fields[0], "open")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (cmd->nfields > 2) {
				char* parent;
				char* name;
				char* mode;
				int fd = 0;
				fs_path* p;

				if (input_is_quoted) {
					p = fs.pathFromString(cmd_quotes->fields[1]);
					mode = fs.trim(cmd_quotes->fields[2]);
				}
				else {
					p = fs.pathFromString(cmd->fields[1]);
					mode = cmd->fields[2];
				}
				parent = fs.pathSkipLast(p);
				name = fs.pathGetLast(p);

				fd = fs.open(parent, name, mode);

				if (FS_ERR == fd)
					printf("file open error\n");
				else printf("got file descriptor \"%d\"\n", fd);

			}

		} else if (!strcmp(cmd->fields[0], "write")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (input_is_quoted)	retv = sh_write(cmd, cmd_quotes);
			else			retv = sh_write(cmd, NULL);

		} else if (!strcmp(cmd->fields[0], "read")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (cmd->nfields > 2) {
				char* buf = NULL;

				if (input_is_quoted)	buf = fs.read(atoi(cmd_quotes->fields[1]), atoi(cmd_quotes->fields[2]));
				else			buf = fs.read(atoi(cmd->fields[1]), atoi(cmd->fields[2]));
				if (NULL == buf) retv = FS_ERR;
				else {
					retv = FS_OK;
					printf("%s\n",buf);
				}
			}

		} else if (!strcmp(cmd->fields[0], "close")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (cmd->nfields > 1) {

				fs.close(atoi(cmd->fields[1]));
				retv = FS_OK;
			}

		} else if (!strcmp(cmd->fields[0], "link")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}
			if (cmd->nfields == 3) { // if proper argument number
				if(!fs.stat(cmd->fields[3])){
					//if the 2nd file doesn't exist
					fs.link(cmd->fields[1], cmd->fields[2]);
					retv = FS_OK;
				} else {
					printf("destination file exists\n");
					prompt();
					continue;
				}
				
			}
		} else if (!strcmp(cmd->fields[0], "unlink")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}
			if (cmd->nfields == 2) {
				fs.ulink(cmd->fields[1]);
			}
		} else {
			printf("Bad command \"%s\"", buf); 
			retv = FS_NORMAL; 
		}

		if (FS_OK == retv)		printf("OK");
		else if (FS_ERR == retv)	printf("ERROR");
		printf("\n");

		argsFree(cmd);
		argsFree(cmd_quotes);
		memset(buf, 0, SH_BUFLEN);
		retv = 0;
 		prompt();
	}
	printf("exit()\n");
}
