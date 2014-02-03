#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "transaction.h"

#ifndef ATMCLIENT_H
#define ATMCLIENT_H

#ifndef BOOL_T
#define BOOL_T
typedef int bool;
#define true 1
#define false 0 
#endif /* BOOL_T */

typedef unsigned int uint;

#define ATM_SUCCESS 1
#define ATM_FAILURE 0

#ifndef CMD_INDEXES
#define CMD_INDEXES
#define CMD_TRANSFER 0
#define CMD_DEPOSIT 1
#define CMD_WITHDRAW 2
#define CMD_INQUIRY 3
#endif /* CMD_INDEXES */

struct cmd_t {
	uint argc;
	char** argv;
}; typedef struct cmd_t cmd_t;

struct command_t {
	transaction_t *t;
	char* host;
	char* name;
	int idx;
}; typedef struct command_t command_t;

typedef unsigned int uint;

extern result_t* startSession(char*);
extern result_t* stopSession(char*);
extern result_t* transfer(char*, int, int, double);
extern result_t* deposit(char*, int, double);
extern result_t* withdraw(char*, int, double);
extern result_t* inquiry(char*, int);

/* Set up function pointers to the commands */
typedef void* (*command_ptr_t)(char*, int, ...);
static const command_ptr_t cmdPtrs[] = 
	{ (void*)transfer, (void*)deposit, 
	  (void*)withdraw, (void*)inquiry };

static const uint cmdsLen = 4;	
static const char* cmdNames[] = 
    	{ "transfer", "deposit", 
    	  "withdraw", "inquiry" };  
static const int minFields[] = { 5, 4, 4, 3 };
 
static const char* delimiter = " \n";
static const char line_comment = '#'; 

#endif /* ATMCLIENT_H */

