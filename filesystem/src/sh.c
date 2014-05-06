#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#include "Shlwapi.h"
#else 
#include <unistd.h>
#endif

dentv* cur_dv = NULL;
char* current_path;

static fs_args* newArgs() {
	uint i = 0;
	fs_args* args = (fs_args*)malloc(sizeof(fs_args));
	args->fieldSize = SH_MAXFIELDSIZE;

	for (i = 0; i < SH_MAXFIELDS; i++) {
		memset(args->fields[i], 0, SH_MAXFIELDSIZE);
	}
	memset(args->quoted_fields, 0, sizeof(int)*SH_MAXFSARGS);

	args->nfields = 0;
	args->firstField = 0;

	return args;
}

static void argsFree(fs_args* p) {
	if (NULL == p) return;

	free(p);
	p = NULL;
}

/* Split a string on delimiter(s) */
void sh_tokenize(const char* str, const char* delim, fs_args* args) {
	size_t i = 0;
	char* next_field	= NULL;
	size_t len;
	char* str_cpy;

	if (NULL == str || '\0' == str[0]) return;

	len = strlen(str);
	str_cpy = (char*)malloc(len + 1);
	
	// Copy the input because strtok replaces delimieter with '\0'
	strncpy(str_cpy, str, min(SH_MAXFIELDSIZE-1, len+1));	
	str_cpy[len] = '\0';

	// Split the path on delim
	next_field = strtok(str_cpy, delim);
	while (NULL != next_field) {

		i = args->nfields;
		len = strlen(next_field);

		/* Hack, I know */
		if (!strcmp(" ", next_field) || !strcmp("  ", next_field) ) {
			next_field = strtok(NULL, delim);
			continue;
		}

		strncpy(args->fields[i], next_field, min(SH_MAXFIELDSIZE-1, len+1));	
		args->fields[i][len] = '\0';

		if (args->nfields + 1 == SH_MAXFSARGS)
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

void sh_print_dir(char* name, int depth) {
	
#if defined(_WIN64) || defined(_WIN32)
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	WORD saved_attributes;
	GetConsoleScreenBufferInfo(console, &info);
	saved_attributes = info.wAttributes;
#endif
	
#if defined(_WIN64) || defined(_WIN32)
	SetConsoleTextAttribute(console, FOREGROUND_BLUE);
#else
	printf("%s", ANSI_COLOR_BLUE);
#endif
	if (0 == depth)	printf("%s/", name);
	else		printf("%*s" "%s/", depth*2, " ", name);
	
	// Restore color
#if defined(_WIN64) || defined(_WIN32)
	SetConsoleTextAttribute(console, saved_attributes);
	printf(" \n");
#else
	printf("%s \n", ANSI_COLOR_RESET);
#endif
	
}

void sh_tree_recurse(uint depth, uint maxdepth, dentv* dv) {
	size_t copysize;
	uint i;
	dentv* iterator = NULL;
	char next[FS_NAMEMAXLEN];
	char *dv_path;
	char* newpath;
		
	memset(next, 0, FS_NAMEMAXLEN);

	if (NULL == dv) return;
	dv_path = sh_get_dv_path(dv);

	if (NULL == dv->head && 
		dv->ndirs == 0 &&
		dv->nfiles == 0 &&
		dv->nlinks == 0) 
	{
//		free(dv_path);
		return;
	}

	if (NULL != dv->head) {
		newpath = sh_path_cat(dv_path, dv->head->data.dir.name);
		copysize = min(FS_NAMEMAXLEN - 1, strlen(newpath));
		strncat(next, newpath, copysize);
		next[copysize] = '\0';

		for (i = 0; i < dv->ndirs; i++) {	// For each subdir at this level
		
			iterator = fs.opendir(next);
			if (NULL == iterator)
				printf("sh_tree_recurse: Could not open directory \"%s\"",next);
			else {

				sh_print_dir(iterator->name, depth);
				
				if (depth < maxdepth) sh_tree_recurse(depth+1, maxdepth, iterator);

				if (	NULL == iterator->next ||
					0 == iterator->next->data.dir.ino ||			/* If the dir has a next */			
					iterator->ino->num == iterator->next->data.dir.ino)	/* If the dir doesn't point to itself */
				{
					//fs.closedir(iterator);
					//iterator = NULL;
					break;
				}
				
				newpath = sh_path_cat(dv_path, iterator->next->data.dir.name);
				copysize = min(FS_NAMEMAXLEN - 1, strlen(newpath));
				next[0] = '\0';
				strncat(next, newpath, copysize);
				next[copysize] = '\0';

				//fs.closedir(iterator);
				//iterator = NULL;
			}
		}
	}

	sh_traverse_files(dv, depth);
	sh_traverse_links(dv, depth);
	//free(dv_path);
}

void sh_traverse_files(dentv* dv, int depth) {
	size_t i;
	
#if defined(_WIN64) || defined(_WIN32)
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	WORD saved_attributes;
	GetConsoleScreenBufferInfo(console, &info);
	saved_attributes = info.wAttributes;
#endif
	
	for (i = 0; i < dv->nfiles; i++) {	// For each file at this level
		inode* f_ino = fs.statI(dv->ino->data.dir.files[i]);
		
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, FOREGROUND_GREEN);
#else
		printf("%s", ANSI_COLOR_GREEN);
#endif
		printf("%*s" "%s", depth*2, " ", f_ino->data.file.name);
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, saved_attributes);
		printf(" \n");
#else
		printf("%s \n",ANSI_COLOR_RESET);
#endif
		
		//fs.inodeUnload(f_ino);
	}

}

void sh_traverse_links(dentv* dv, int depth) {
	size_t i;

#if defined(_WIN64) || defined(_WIN32)
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	WORD saved_attributes;
	GetConsoleScreenBufferInfo(console, &info);
	saved_attributes = info.wAttributes;
#endif
	
	for (i = 0; i < dv->nlinks; i++)	{// For each link at this level
		inode* l_ino = NULL;
		inode* dest_ino;
		
		l_ino = dv->links[i];
		dest_ino = l_ino->datav.link->dest;
		
//		l_ino = fs.statI(dv->ino->data.dir.links[i]);
//		dest_ino = fs.statI(l_ino->data.link.dest);

		
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_BLUE);
#else
		printf("%s", ANSI_COLOR_MAGENTA);
#endif
		printf("%*s" "%s", depth*2, " ", l_ino->data.link.name);
		
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, saved_attributes);
#else
		printf("%s", ANSI_COLOR_RESET);
#endif
		printf(" --> ");

#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_BLUE);
#else
		printf("%s", ANSI_COLOR_MAGENTA);
#endif
		if (FS_LINK == l_ino->data.link.mode)
			printf("%s", dest_ino->data.link.name);
		
//		if (FS_LINK == l_ino->data.link.mode) {
//			char* parent_path = NULL;
//			char* full_path = NULL;
//			fs_path* p;
//			
//			p = fs.newPath();
//			parent_path = fs.getAbsolutePathDV(dest_ino->datav.link->parent->datav.dir, p);
//			fs.pathFree(p);
//			
//			full_path = sh_path_cat(parent_path, dest_ino->data.link.name);
//			printf("%s", full_path);
//			free(full_path);
//		}
		
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, FOREGROUND_BLUE);
#else
		printf("%s", ANSI_COLOR_BLUE);
#endif

		if (FS_DIR == l_ino->data.link.mode) {
			char* path = NULL;
			fs_path* p;
			
			p = fs.newPath();
			path = fs.getAbsolutePathDV(dest_ino->datav.dir, p);
			fs.pathFree(p);
			
			printf("%s", path);
			free(path);
		}
		
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, FOREGROUND_GREEN);
#else
		printf("%s", ANSI_COLOR_GREEN);
#endif
		if (FS_FILE == l_ino->data.link.mode)
			printf("%s", dest_ino->data.file.name);
		
//		if (FS_FILE == l_ino->data.link.mode) {
//			char* parent_path = NULL;
//			char* full_path = NULL;
//			fs_path* p = NULL;
//			
////			inode* dest_ino2 = NULL;
////			dest_ino2 = fs.statI(l_ino->data.link.dest);
//			
//			p = fs.newPath();
//			parent_path = fs.getAbsolutePathDV(dest_ino->datav.file->parent->datav.dir, p);
//			fs.pathFree(p);
//			
//			full_path = sh_path_cat(parent_path, dest_ino->data.file.name);
//			printf("%s", full_path);
//			free(full_path);
//			free(parent_path);
//		}
		
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, saved_attributes);
		printf(" \n");
#else
		printf("%s \n", ANSI_COLOR_RESET);
#endif
//		fs.inodeUnload(l_ino);
//		fs.inodeUnload(dest_ino);
	}
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

	if (!strcmp("/", name))	sh_print_dir("", 0);
	else			sh_print_dir(dv->name, 0);

	sh_tree_recurse(1, FS_MAXPATHFIELDS, dv);
//	fs.closedir(dv);
}

int sh_open(fs_args* cmd){
	char* parent = NULL;
	char* name = NULL;
	char* mode = NULL;
	char* abs_path = NULL;
	int fd = 0;
	fs_path* p = NULL;
	
	if (cmd->nfields < 3) {
		printf("sh_open: too few args, have %zu", cmd->nfields);
		return FS_ERR;
	}
	
	abs_path = fs.getAbsolutePath(current_path, cmd->fields[1]);
	p = fs.pathFromString(abs_path);
	mode = cmd->fields[2];
	
	parent = fs.pathSkipLast(p);
	name = fs.pathGetLast(p);
	
	fd = fs.open(parent, name, mode);

	free(abs_path);
	fs.pathFree(p);

	if (FS_ERR == fd) {
		printf("file open error\n");
		return FS_ERR;
	}
	
	printf("got file descriptor \"%d\"\n", fd);
	return fd;
}

char* sh_read(int fd, size_t size){
	char* rdbuf = NULL;
	
	rdbuf = fs.read(fd, size);
	if (NULL != rdbuf)
		printf("Error: Buffer is empty\n");
	return rdbuf;
}

void sh_close(int fd){
	fs.close(fd);
}

int sh_write(fs_args* cmd) {
	
	size_t write_byte_count;
	size_t write_expected_byte_count;
	fd_t fd;
	int retv;

	if (NULL == cmd) return FS_ERR;
	if (cmd->nfields < 2) return FS_ERR;

	if (!fs.isNumeric(cmd->fields[1]))
		return FS_ERR;

	fd = atoi(cmd->fields[1]);

	write_expected_byte_count = strlen(cmd->fields[2]);
	write_byte_count = fs.write(fd, cmd->fields[2]);

	printf("Wrote %lu of %lu bytes to fd %d ",
		write_byte_count, write_expected_byte_count, fd);

	if (write_expected_byte_count != write_byte_count) 
		retv = FS_ERR;
	else	retv = FS_OK;

	return (int)write_byte_count;
}

void sh_stat(char* name) {
	inode* ino;

	ino = fs.stat(name);
	
	if (NULL == ino) printf("inode not found for \"%s\"", name);
	else {
		
#if defined(_WIN64) || defined(_WIN32)
		HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO info;
		WORD saved_attributes;
		GetConsoleScreenBufferInfo(console, &info);
		saved_attributes = info.wAttributes;
#endif
		
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, FOREGROUND_BLUE);
#else
		printf("%s", ANSI_COLOR_BLUE);
#endif
		printf("\nstat() for \"%s\":\n\n", name);
		printf("\tNumber: %d\n", ino->num);
		printf("\tType: ");
		
		switch (ino->mode) {
			case FS_FILE:
				printf("FILE\n");
				break;
			case FS_DIR:
				printf("DIRECTORY\n");
				break;
			case FS_LINK:
				printf("LINK\n");
				break;
			default:
				break;
		}
		printf("\tSize (bytes): %zu\n", ino->size);
		printf("\tLink count: %zu\n", ino->nlinks);
		printf("\tNumber of blocks: %zu\n", ino->ndatablocks);
		printf("\tVolatile data attached: ");
		
		switch (ino->v_attached) {
			case true:
				printf("TRUE\n");
				break;
			case false:
				printf("FALSE\n");
				break;
			default:
				break;
		}
		printf("\tName: ");
		switch (ino->mode) {
			case FS_FILE:
				printf("%s\n", ino->data.file.name);
				break;
			case FS_DIR:
				printf("%s\n", ino->data.dir.name);
				break;
			case FS_LINK:
				printf("%s\n", ino->data.link.name);
				break;
			default:
				break;
		}
		
		// Restore color
#if defined(_WIN64) || defined(_WIN32)
		SetConsoleTextAttribute(console, saved_attributes);
#else
		printf("%s", ANSI_COLOR_RESET);
#endif
	}
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

int sh_rmdir(char* name) {
	int retv;
	char* abs_path;
	char* cur_path;
	fs_path *p;

	p = fs.newPath();
	cur_path = fs.getAbsolutePathDV(cur_dv, p);
	fs.pathFree(p);
	
	abs_path = fs.getAbsolutePath(cur_path, name);

	if (NULL == abs_path) {
		printf("Bad or unsupported path\n");
		return FS_ERR;
	}

	retv = fs.rmdir(cur_path, abs_path);

	return retv;
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

int sh_export(fs_args* cmd) {
	int retv = FS_OK;
	int file_exists;
	int fd = 0;
	char* out_buf = NULL;
	char* filename = NULL;
	FILE* fp;
	
	inode* src = NULL;
	fs_args* open_tmp = NULL;
	
	if (cmd->nfields < 3) {
		printf("Not enough arguments, have %zu\n", cmd->nfields);
		return FS_ERR;
	}

	src = fs.stat(cmd->fields[1]);

	open_tmp = newArgs();
	open_tmp->nfields = 3;
	strcpy(open_tmp->fields[0], "open");
	strcpy(open_tmp->fields[1], cmd->fields[1]);
	strcpy(open_tmp->fields[2], "r");
	
	// Perform Open and Read to get inner file contents
	fd = sh_open(open_tmp);
	argsFree(open_tmp);
	
	if (FS_ERR == fd)
		return FS_ERR;

	out_buf = sh_read(fd,src->size);
	filename = cmd->fields[2];

	// Test if file exists
#if defined(_WIN64) || defined(_WIN32)
	file_exists = PathFileExists(filename);
	if( file_exists ){
		printf("File already exists\n");
		return FS_ERR;
	}
#else
	file_exists = access(filename, R_OK);
	if( -1 != file_exists ){
		printf("File already exists\n");
		return FS_ERR;
	}
#endif	

	// Create file pointer for dst
	fp = fopen(filename, "w");
	if (NULL == fp) {
		printf("Could not open file \"%s\" for writing.\n", filename);
		return FS_ERR;
	}

	// Pass to export to write each block to the file
	if(0 == fwrite(out_buf, src->size, 1, fp)) {
		printf("Error occurred while writing export\n");
		retv = FS_ERR;
	} else	retv = FS_OK;

	sh_close(fd);
	free(out_buf);
	fclose(fp);
	
	return retv;
}

int sh_import(fs_args* cmd) {
	int retv = FS_OK;
	int fd;
	
	long fsize;
	char *src_buf = NULL;

	fs_args* open_tmp = NULL;
	fs_args* write_cmd_tmp = NULL;
	FILE* fp;
	
	if (cmd->nfields < 3) {
		printf("Not enough arguments, have %zu\n", cmd->nfields);
		return FS_ERR;
	}

	// Create file pointer for source
	fp = fopen(cmd->fields[1], "rb");
	if (NULL == fp) {
		printf("File does not exist.\n");
		return FS_ERR;
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);		//getting the offset
	fseek(fp, 0, SEEK_SET);		//setting it back to the start
	
	src_buf = (char*)malloc(fsize + 1);	//allocating the right size
	fread(src_buf, fsize, 1, fp);
	fclose(fp);
	
	src_buf[fsize] = 0;		//making it null terminated
	
	open_tmp = newArgs();
	open_tmp->nfields = 3;
	strcpy(open_tmp->fields[0], "open");
	strcpy(open_tmp->fields[2], "w");
	strcpy(open_tmp->fields[1], cmd->fields[2]);
	
	// Open the destination file for writing
	fd = sh_open(open_tmp);
	
	argsFree(open_tmp);
	
	write_cmd_tmp		= newArgs();
	write_cmd_tmp->nfields	= 2;
	
	strcpy(write_cmd_tmp->fields[0],"write");
	sprintf(write_cmd_tmp->fields[1],"%d",fd);
	
	strncpy(write_cmd_tmp->fields[2], src_buf, SH_MAXFIELDSIZE);
	
	sh_write(write_cmd_tmp);
	argsFree(write_cmd_tmp);
	sh_close(fd);

	free(src_buf);
		
	return retv;
}

/* Combine the quoted fields of an fs_arg into one field */
void sh_fs_args_quote_split(fs_args* cmd) {
	size_t i, j ,k;
	size_t nquotedfields = 0;
	size_t len = 0;
	
	for (i = 0; i < cmd->nfields; i++) {

		/* Find the opening quote */
		if (cmd->fields[i][0] == '\"') {
			size_t num_fields_consolidated = 0;
			size_t l;
			int found_closing_quote = false;

			/* Find the closing quote */
			for (j = i+1; j < cmd->nfields; j++) {
				size_t len = strlen(cmd->fields[j]);

				if (cmd->fields[j][len-1] == '\"') {
					cmd->quoted_fields[i] = ++nquotedfields;

					found_closing_quote = true;
					break;
				}
			}
			if (!found_closing_quote) break;
				
			/* Copy all fields within the quotes into one field*/
			for (k = i; k < j; k++) {
				
				if (k+1 >= cmd->nfields) break;

				len = strlen(cmd->fields[k+1]);
				if (k == j - 1) len--; /* Skip closing quote */

				strncat(cmd->fields[i], " ", 1);					
				strncat(cmd->fields[i], cmd->fields[k+1], len);
				num_fields_consolidated++;
			}

			/* For each remaining field after the quote, move it to the first freed spot */
			for (l = i + 1; l < cmd->nfields; l++) {
				len = strlen(cmd->fields[l + num_fields_consolidated]);
				strncpy(cmd->fields[l], cmd->fields[l + num_fields_consolidated], len);
				cmd->fields[l][len] = '\0';
			}

			/* Reset remaining fields */
			for (l = l + num_fields_consolidated; l < cmd->nfields; l++)
				cmd->fields[l][0] = '\0';

			/* Skip opening quote */
			memmove(cmd->fields[i], cmd->fields[i]+1, strlen(cmd->fields[i]));
			cmd->nfields -= num_fields_consolidated;
		}
	}	
}

fs_args* sh_parse_input(char* buf) {
	size_t i;
	fs_args* cmd;
	fs_args* cmd_quotes;
	char* delimiter = "\t ";
	
	// Split input on whitespace
	cmd = newArgs();
	sh_tokenize(buf, delimiter, cmd);
		
	//TODO issue here w/ treating every like as a newline
	
	/* Split input on quoted strings.
		* First element will be the unquoted leading input,
		* Second to n elements will be quoted fields, and the (n+1)th
		* Element will be the trailing unquoted input
		* e.g. write 0 "this is a long string   " works
		*as well as "open "long file name" w */
	cmd_quotes = newArgs();
	sh_tokenize(buf, "\"", cmd_quotes);
	sh_fs_args_quote_split(cmd);

	for (i = 0; i < cmd->nfields; i++)
	{
		if (cmd->quoted_fields[i])
			strcpy(cmd->fields[i], cmd_quotes->fields[cmd->quoted_fields[i]]);
	}

	return cmd;
}

int main() {
	int retv = 0;
	char buf[SH_BUFLEN] = "";	// Buffer for user input

	fs_args* cmd;
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

		cmd = sh_parse_input(buf);
		if('#' == cmd->fields[0][0]) continue; /* Skip commented lines */

		if (!strcmp(cmd->fields[0], "exit")) break;
		
		else if (!strcmp(cmd->fields[0], "ls")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			// If the user provided more than one field
			if (1 < cmd->nfields)	retv = sh_ls(cmd->fields[1]);
			else			retv = sh_ls(current_path);
		}

		else if (!strcmp(cmd->fields[0], "cd")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (1 < cmd->nfields)	retv = sh_cd(cmd->fields[1]);
			else			retv = sh_cd("/");
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

			if (1 < cmd->nfields)	retv = sh_mkdir(cmd->fields[1]);
			else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}
		}

		else if (!strcmp(cmd->fields[0], "rmdir")) { 
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (1 < cmd->nfields)	retv = sh_rmdir(cmd->fields[1]);
			else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}
		}

		else if (!strcmp(cmd->fields[0], "stat")) { 
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (1 < cmd->nfields) {
				sh_stat(cmd->fields[1]); 
				retv = FS_NORMAL;
			} else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}
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

			if(2 < cmd->nfields) {
				fs.seek((fd_t)atoi(cmd->fields[1]), (size_t)atoi(cmd->fields[2]));
				retv = FS_OK;
			} else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}
			
		} else if (!strcmp(cmd->fields[0], "open")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			retv = sh_open(cmd);

		} else if (!strcmp(cmd->fields[0], "write")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			retv = sh_write(cmd);

		} else if (!strcmp(cmd->fields[0], "read")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (2 < cmd->nfields) {
				char* rdbuf = NULL;

				rdbuf = fs.read(atoi(cmd->fields[1]), atoi(cmd->fields[2]));
				if (NULL == rdbuf) retv = FS_ERR;
				else {
					retv = FS_OK;
					printf("%s\n",rdbuf);
					free(rdbuf);
				}
			}  else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}

		} else if (!strcmp(cmd->fields[0], "close")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}

			if (1 < cmd->nfields) {

				fs.close(atoi(cmd->fields[1]));
				retv = FS_OK;
			}  else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}
			
		} else if (!strcmp(cmd->fields[0], "link")) {
			
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}
			
			if (2 < cmd->nfields) {
				
				char* abs_path, *abs_path2;
				inode *val, *val2;
				
				abs_path = fs.getAbsolutePath(current_path, cmd->fields[1]);
				abs_path2 = fs.getAbsolutePath(current_path, cmd->fields[2]);
				
				val = fs.stat(abs_path);
				//BUG: If the file doesn't exist, I get an error
				val2 = fs.stat(abs_path2);
				
				if (NULL != val2) {
					printf("sh_link: target exists.\n");
					prompt();
					continue;
				}
				
				if (NULL == val) {
					printf("sh_link: Source does not exist \n");
					prompt();
					continue;
				}
				
				retv = fs.link(abs_path, abs_path2);
			} else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}
			
		} else if (!strcmp(cmd->fields[0], "unlink")) {
			
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}
			if (1 < cmd->nfields) {
				char* abs_path;
				abs_path = fs.getAbsolutePath(current_path, cmd->fields[1]);
				retv = fs.ulink(abs_path);
			} else {
				printf("Not enough arguments\n");
				retv = FS_ERR;
			}
	        } else if (!strcmp(cmd->fields[0], "export")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}
			
			retv = sh_export(cmd);
			
		} else if (!strcmp(cmd->fields[0], "import")) {
			if (NULL == current_path || current_path[0] == '\0') {
				printf("No filesystem.\n");
				prompt();
				continue;
			}
			
			retv = sh_import(cmd);
			
		} else {
			printf("Bad command \"%s\"", buf); 
			retv = FS_NORMAL; 
		}

		if (FS_OK == retv)		printf("OK");
		else if (FS_ERR == retv)	printf("ERROR");
		printf("\n");

		argsFree(cmd);
		memset(buf, 0, SH_BUFLEN);
		retv = 0;
 		prompt();
	}
	printf("exit()\n");
}
