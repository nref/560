#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "transaction.h"

#ifndef BANKCLIENT_H
#define BANKCLIENT_H

/* Define bool */
#ifndef BOOL_T
#define BOOL_T
typedef int bool;
#define true 1
#define false 0 
#endif /* BOOL_T */

/* Define uint */
typedef unsigned int uint;

#define ATM_SUCCESS 1
#define ATM_FAILURE 0

#define BAD_ARGS -1
#define BAD_CMD -2

/* Command codes */
#define CMD_TRANSFER 0
#define CMD_DEPOSIT 1
#define CMD_WITHDRAW 2
#define CMD_INQUIRY 3

#define MAX_HOSTS 4096	/* Max number of hosts we can remember */
#define TOO_MANY_HOSTS -1

#define NAME TRANSACTION
#define VERS TVERS

/* Contains user input from file or 
 * stdin for a transaction 
 */
struct usr_cmd_t {
	uint argc;
	char** argv;
}; typedef struct usr_cmd_t usr_cmd_t;

/* An unpacked version of usr_cmd_t
 * ready for a transaction.
 */
struct command_t {
	transaction_t *t;
	char* host;
	char* name;
	int idx; /* Index into cmdPtrs[] */
}; typedef struct command_t command_t;

/* Bank action function prototypes */
extern result_t* startSession(char*);
extern result_t* stopSession(char*);
extern result_t* transfer(char*, int, int, double);
extern result_t* deposit(char*, int, double);
extern result_t* withdraw(char*, int, double);
extern result_t* inquiry(char*, int);

/* Function pointer to a command */
typedef void* (*command_ptr_t)(char*, int, ...);

/* Function pointers to the commands */
extern const command_ptr_t cmdPtrs[];
extern const uint cmdsLen;

/* String names of the commands */
extern const char* cmdNames[];

/* Number of fields each command requires */
extern const int minFields[];
extern uint getNumFields(char*);

/* Command file characteristics */
extern const char* delimiter;
extern const char line_comment;

/* Host name management */
extern bool host_sessions[];
extern char* hosts[];

/* Command handling */
extern usr_cmd_t* newCommand(uint, char**);	/* Allocate memory for an command */
extern command_t* parseCommand(usr_cmd_t*); /* Convert user input to a transaction-ready structure */
extern int isValidCommand(usr_cmd_t*, command_t*);	/* Check that the input is known */
extern bool tryDoCommand(usr_cmd_t*);	/* Try to do a command */
extern bool doSingleCommand(usr_cmd_t*); /* Do a single command and close the session */
extern bool doCommand(command_t*);	/* Do a single command and keep session open */
extern bool doFileCommands(char*);	/* Read commands from file */
extern void closeSessions();

/* Help */
extern void print_help();
extern void helpAndExit(bool);

/* Argument checking */
extern bool checkNumArgs(usr_cmd_t*);
extern bool checkExpectedNumArgs(usr_cmd_t*, int);
extern bool checkArgs(int, char**);

#endif /* BANKCLIENT_H */