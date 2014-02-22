#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transaction.h"

#ifndef SERVERUTIL_H
#define SERVERUTIL_H

/* Define uint */
typedef unsigned int uint;

/* Define bool */
#ifndef BOOL_H
#define BOOL_H
typedef int bool;
#endif /* BOOL_H */

#define true 1
#define false 0 

#define ATM_FAILURE 0
#define ATM_SUCCESS 1

#define FIRST_FIELD 0
#define SECOND_FIELD 1

#define BASE 1000 /* Lowest-numbered account */

extern double write_interval;
extern double accounts_table[];
extern double starting_balances[];
extern uint num_accounts;
extern time_t last_write;

extern FILE* accounts;
extern FILE* logfile;

extern result_t ret;
extern bool init_done;

extern result_t* _doTransfer(transaction_t*);
extern result_t* _doWithdrawal(transaction_t*);
extern result_t* _doDeposit(transaction_t*);
extern result_t* _doInquiry(transaction_t*);
extern result_t* _doStart();
extern result_t* _doStop();
extern result_t* _doAction(transaction_t*, 
		result_t* (*)(transaction_t*),
		bool (*)(int,int), int, int);
		
extern char* msgOk[];
extern char* msgErr[];

extern const char* log_file;
extern const char* accounts_file;
extern const char* config_file;
extern const char* delimiter;
extern const char line_comment;

extern bool readConfiguration();		/* Read configuration from file */
extern bool readStartingBalances();		/* Read initial account balances from file */
extern bool causesNegativeBalance(int, double);
extern bool writeLog(char* msg); 		/* Append a message to the log file */
extern bool writeAccounts();			/* Write in-memory account balances to file. */
extern bool isValidAccount(int, int);	/* Return true if an account number is valid */
extern bool twoValidAccounts(int, int); /* Return true if two account numbers are both valid */
extern void msgCpy(char*);

extern result_t* withdrawOk();
extern result_t* depositOk();
extern result_t* transferOk();
extern result_t* inquiryOk();
extern result_t* startOk();
extern result_t* stopOk();
extern result_t* defaultOk(char*);

extern result_t* notStartedError();
extern result_t* invalidAccountError();
extern result_t* startError();
extern result_t* withdrawError();
extern result_t* depositError();
extern result_t* transferError();
extern result_t* inquiryError();
extern result_t* stopError();
extern result_t* notStartedError();
extern result_t* invalidAccountError();
extern result_t* valueError();
extern result_t* balanceError();
extern result_t* defaultError(char*);

#endif /* SERVERUTIL_H */