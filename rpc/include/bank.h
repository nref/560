#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transaction.h"

#ifndef BANK_H_
#define BANK_H_

typedef unsigned int uint;
typedef int bool;
#define true 1
#define false 0 

#define ATM_SUCCESS 1
#define ATM_FAILURE 0

#define FIRST_FIELD 0
#define SECOND_FIELD 1

#define BASE 1000 /* Lowest-numbered account */

extern double write_interval;
extern double accounts_table[];
extern result_t ret;
extern uint init_done;
extern bool readStartingBalances();
extern bool readConfiguration(char*);
extern bool writeAccounts();
extern bool isValidAccount(int, int);
extern bool twoValidAccounts(int, int);
extern result_t* notStartedError();
extern result_t* invalidAccountError();

#endif /* BANK_H */