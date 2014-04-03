#include "sh.h"
#include <string.h>
#include <stdio.h>

int sh_mkfs() {
	return fs_mkfs();
}

int main(int argc, char** argv) {
	char cmd[SHBUFLEN];

	printf("> ");
	while (EOF != scanf("%s", cmd)) {

		if (!strcmp(cmd, "exit")) break;

		if (!strcmp(cmd, "mkfs")) {
			printf("mkfs() ... ");

			if (!sh_mkfs())	{ printf("OK"); }
			else			{ printf("ERROR"); }
			printf("\n> ");
		}
	}
	printf("exit()\n");
}