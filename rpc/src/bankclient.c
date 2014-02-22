#include "clientUtil.h"

int main(int argc, char* argv[]) {
	if (!checkArgs(argc, argv)) 
		return doSingleCommand(newCommand(argc-1, &argv[1])); /* Skip 1st arg */
	else return doFileCommands(argv[1]);
}