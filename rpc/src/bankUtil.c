#include "bankUtil.h"
#include <time.h>

double write_interval = 2;	/* Write every 2s */
double accounts_table[] = { [0 ... 10] = 0.0};
static double starting_balances[] = { [0 ... 10] = 0.0};
static uint num_accounts = 11;
static time_t last_write;			/* Time since last write to file */

/* Read configuration from file */
bool readConfiguration(char* file) {
    
	uint field;
	uint account_number;
	double balance;
	
	char* line;
	char* next_field;
	char* pEnd;
	
	char* log_file = "bankserver_log";
	char* delimiter = ", "; 	// Comma-separated
	char line_comment = '#';	// Lines to ignore
	
	size_t len;
	ssize_t read;
	
	FILE* accounts;
	FILE* log;
	
	accounts = fopen(file, "r");
	log = fopen(log_file, "w+");
	
	if (accounts == NULL || log == NULL)
		return ATM_FAILURE;
    
	// For each line in the file
	while ((read = getline(&line, &len, accounts)) != -1) {
		if (line[0] == line_comment)
			continue;
		
		field = 0;
		account_number = 0;
		balance = 0.0;
		
		// For each field in the line
		next_field = strtok(line, delimiter);
		while (next_field != NULL) {
            
			if (field == FIRST_FIELD)
				account_number = atoi(next_field);
			else if (field == SECOND_FIELD)
				balance = strtod(next_field, &pEnd);
			
			next_field = strtok(NULL, delimiter);
			field++;
		}
		
		starting_balances[account_number - BASE] = balance;
		fprintf(log,"%d%s%0.2lf\n", account_number, delimiter, balance);
	}
    
	fclose(accounts);
	fclose(log);
	
	if (line)
		free(line);
    
	return (ATM_SUCCESS && readStartingBalances());
}

/* Read initial account balances from file */
bool readStartingBalances() {
	uint i;
	for (i = 0; i < num_accounts; i++)
		accounts_table[i] = starting_balances[i];
    
	return ATM_SUCCESS;
}

/* Write in-memory account balances to file. */
bool writeAccounts() {
	time_t now = time(NULL);	/* Get system time */
	double seconds = difftime(now, last_write);
	
	if (seconds < write_interval)
		return ATM_SUCCESS;
		
	uint i;
	char* accounts_file = "accounts";
	FILE* accounts = fopen(accounts_file, "w+");
	
	if (accounts == NULL)
		return ATM_FAILURE;
    
	for (i = 0; i < num_accounts; i++)
		fprintf(accounts,"%d,%0.2lf\n", i+BASE, accounts_table[i]) ;
	fclose(accounts);
	
	return ATM_SUCCESS;
}

/* Return true if an account number is valid */
bool isValidAccount(int account, int dummy) {
    
	if ((int)account < BASE || (int)account > BASE+num_accounts)
		return ATM_FAILURE;
	return ATM_SUCCESS;
}

/* Return true if two account numbers are both valid */
bool twoValidAccounts(int account1, int account2) {
	return (isValidAccount(account1, BASE) &&
			isValidAccount(account2, BASE));
}