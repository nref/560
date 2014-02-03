#include "atmClient.h"

#ifndef ATMCLIENTUTILITY_H
#define ATMCLIENTUTILITY_H

extern cmd_t* newCommand(uint, char**);
extern command_t* parseCommand(cmd_t*);
extern bool tryDoCommand(cmd_t*);
extern bool doCommand(command_t*);
extern bool doFileCommands(char*);

extern void print_help();
extern void helpAndExit(bool);

extern bool checkNumArgs(cmd_t*);
extern bool checkExpectedNumArgs(cmd_t*, int);
extern bool checkArgs(int, char**);

extern int isValidCommand(cmd_t*, command_t*);
extern uint getNumFields(char*);

#endif /* ATMCLIENTUTILITY_H */