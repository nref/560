#include "sh.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char curDir[FS_MAXPATHLEN];		// Current shell directory
char cmd[SH_BUFLEN];			// Buffer for user input

void sh_stat(char* name) {
	inode* ret = fs_stat(name);
	if (NULL == ret) printf("No filesystem yet!");
	else printf("%d %d", ret->mode, ret->size);
	printf("\n");
}

int sh_mkdirent(char* currentdir, char* dirname) {
	return fs_mkdirent(curDir, dirname);
}

int sh_mkfs() {	return fs_mkfs(); }

void prompt() { 
	if (strlen(curDir) > 0) 
		printf("%s ", curDir); 
	printf("> ");
}

int main(int argc, char** argv) {

	char* fields[SH_MAXFIELDS];
	char* delimiter = "\t ";
	char* next_field = NULL;
	char cmd_cpy[SH_BUFLEN];
	int i;
	curDir[0] = '\0';

	prompt();
	while (NULL != fgets(cmd, SH_BUFLEN-1, stdin)) {

		if (NULL == cmd) { prompt(); continue; }		// Sanity check
		cmd[strlen(cmd)-1] = '\0';						// Remove trailing newline
		if (strlen(cmd) == 0) { prompt(); continue; }	// Repeat loop on empty input

		// Get fields. Copy them because strtok replaces delimiter with '\0'
		strcpy(cmd_cpy, cmd);
		next_field = strtok(cmd_cpy, delimiter);		// Get first field

		i = 0;
		while (NULL != next_field) {					// While there is another field
			
			fields[i] = (char*)malloc(					// Store this field
				strlen(next_field)*sizeof(char));
			strcpy(fields[i], next_field);				
			
			next_field = strtok(NULL, delimiter);		// Get the next field
			++i;										// Remember how many fields we have saved
		}

		if (!strcmp(fields[0], "exit")) break;
		else if (!strcmp(fields[0], "pwd")) { printf("%s\n", curDir); }
		else if (!strcmp(fields[0], "mkdir")) { 
			if (i>1) {	// If the user provided more than one field
				int retval = sh_mkdirent(curDir, fields[1]); 
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

			if (!sh_mkfs())	{ printf("OK"); strcpy(curDir, "/"); }
			else			{ printf("ERROR"); }
			printf("\n");
		}
		prompt();
	}
	printf("exit()\n");
}