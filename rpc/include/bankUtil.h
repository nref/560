#include "bank.h"

extern bool readConfiguration(char*);	/* Read configuration from file */
extern bool readStartingBalances();	/* Read initial account balances from file */
extern bool writeAccounts();	/* Write in-memory account balances to file. */
extern bool isValidAccount(int, int);	/* Return true if an account number is valid */
extern bool twoValidAccounts(int, int); /* Return true if two account numbers are both valid */