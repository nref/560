#include "serverUtil.h"
#include <time.h>
#include <unistd.h> /* for sleep() */

char *msgOk[] 		   = { "START OK", "WITHDRAW OK",
				  		   "DEPOSIT OK", "TRANSFER OK",
				  		   "INQUIRY OK", "STOP OK" };

char *msgErr[] 		   = { "SERVICE NOT RUNNING", "ACCOUNT INVALID",
						   "START FAIL", "WITHDRAW FAIL",
				   		   "DEPOSIT FAIL", "TRANSFER FAIL",
				  		   "INQUIRY FAIL", "STOP FAIL", 
				  		   "POSITIVE AMOUNT REQUIRED",
				  		   "NEGATIVE BALANCE DISALLOWED" };
				   
double write_interval = 2;	/* Write every 2s */
double accounts_table[] = { [0 ... 10] = 0.0};
double starting_balances[] = { [0 ... 10] = 0.0};

uint num_accounts = 11;
time_t last_write;			/* Time since last write to file */

const char* log_file = "bankserver_log";
const char* accounts_file = "accounts";
const char* config_file = "accounts_start";
const char* delimiter = ", "; 	// Comma-separated
const char line_comment = '#';	// Lines to ignore

FILE* accounts;
FILE* logfile;

result_t ret;
bool init_done = false;

result_t* _doTransfer(transaction_t *t) {
	if (0. >= t->value)
		return valueError();
    
	if (causesNegativeBalance(t->from, t->value))
		return balanceError();
    
	accounts_table[t->from - BASE] -= t->value;
	accounts_table[t->to - BASE] += t->value;
	return transferOk();
}

result_t* _doWithdrawal(transaction_t *t) {
	if (0. >= t->value)
		return valueError();
    
	if (causesNegativeBalance(t->from, t->value)) 
		return balanceError();
		
	accounts_table[t->from - BASE] -= t->value;
	return withdrawOk();
}

result_t* _doDeposit(transaction_t *t) {
	if (0. >= t->value)
		return valueError();
    
	accounts_table[t->to - BASE] += t->value;
	return depositOk();
}

result_t* _doInquiry(transaction_t *t) {
	ret.value = accounts_table[t->account - BASE];
	return inquiryOk();
}

result_t* _doAction(transaction_t *t, 
					result_t* (*action)(transaction_t *t),
					bool (*isValidInput)(int,int),
					int account1, int account2)
{	
	if (!init_done) 
		return notStartedError();
				
	if (!isValidInput(account1, account2))		
		return invalidAccountError();
	
	result_t* ret = action(t);
	writeAccounts();
	return ret;
}

result_t* _doStart() {
	if (!init_done) {
		accounts = fopen(accounts_file, "w");
		logfile = fopen(log_file, "a");
		
		if (NULL == accounts || NULL == logfile)
			return ATM_FAILURE;
			
		if (readConfiguration())
			init_done = true;
	}
	return startOk();
}

result_t* _doStop() {
	fflush(accounts);
	fflush(logfile);
	
	init_done = false;
	return stopOk();
}

/* Read configuration from file */
bool readConfiguration() {
    
	uint field, account_number;
	double balance;
	
	char* line = NULL;
	char* next_field = NULL;
	char* pEnd = NULL;
	
	size_t len;
	ssize_t read;
	
	FILE* accounts_start = fopen(config_file, "r");
	
	if (NULL == accounts_start)
		return ATM_FAILURE;
    
	// For each line in the file
	while ((read = getline(&line, &len, accounts_start)) != -1) {

		/* Skip commented lines and blank lines */
		if (line[0] == line_comment || line[0] == '\n') continue;
		
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
	}
	if (NULL != line)
		free(line);
		
	fclose(accounts_start);
    readStartingBalances();
    
	return ATM_SUCCESS;
}

/* Copy starting account balances into accounts table */
bool readStartingBalances() {
	for (uint i = 0; i < num_accounts; i++)
		accounts_table[i] = starting_balances[i];
    
	return ATM_SUCCESS;
}

bool causesNegativeBalance(int account, double value) {
	if (0. > accounts_table[account - BASE] - value)
		return true;
	return false;
}

/* Append a message to a log file */
bool writeLog(char* msg) {	
 	if (NULL == logfile) 
 		return ATM_FAILURE;	
 		
	char buffer[25];
	struct tm* tm_info;

	time_t now = time(NULL);	/* Get system time */
	tm_info = localtime(&now);	/* Convert to local time */
	strftime(buffer, 25, "%Y.%m.%d %H:%M:%S", tm_info);
 		
    fprintf(logfile,"%s %s\n", buffer, msg);
    
	return ATM_SUCCESS;
}

/* Write in-memory account balances to file. */
bool writeAccounts() {
	freopen(accounts_file, "w", accounts);
	if (NULL == accounts)
		return ATM_FAILURE;
    
	for (uint i = 0; i < num_accounts; i++)
		fprintf(accounts,"%d,%0.2lf\n", i+BASE, accounts_table[i]);
	
	time_t now = time(NULL);	/* Get system time */
	double seconds = difftime(now, last_write);
	
	if (seconds > write_interval)
		fflush(accounts);
		
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
			isValidAccount(account2, BASE) &&
            account1 != account2);
}

void msgCpy(char *msg) {
	if (NULL != msg) {
		uint n = strlen(msg);
		ret.msg.msg_val = malloc(n*sizeof(char));
		strcpy(ret.msg.msg_val, msg);
		ret.msg.msg_len = strlen(msg);
	}
}

result_t* startOk()		{ return defaultOk(msgOk[0]); }
result_t* withdrawOk()	{ return defaultOk(msgOk[1]); }
result_t* depositOk()	{ return defaultOk(msgOk[2]); }
result_t* transferOk()	{ return defaultOk(msgOk[3]); }
result_t* inquiryOk()	{ return defaultOk(msgOk[4]); }
result_t* stopOk()		{ return defaultOk(msgOk[5]); }

result_t* defaultOk(char* msg) {
	msgCpy(msg);
	ret.success = true;
	return &ret;
}

result_t* notStartedError()		{ return defaultError(msgErr[0]); }
result_t* invalidAccountError()	{ return defaultError(msgErr[1]); }
result_t* startError()			{ return defaultError(msgErr[2]); }
result_t* transferError()		{ return defaultError(msgErr[3]); }
result_t* withdrawError()		{ return defaultError(msgErr[4]); }
result_t* depositError()		{ return defaultError(msgErr[5]); }
result_t* inquiryError()		{ return defaultError(msgErr[6]); }
result_t* stopError()			{ return defaultError(msgErr[7]); }
result_t* valueError()			{ return defaultError(msgErr[8]); }
result_t* balanceError()		{ return defaultError(msgErr[9]); }

result_t* defaultError(char* msg) {
	msgCpy(msg);
    ret.value = -99999.99;
	ret.success = false;
	return &ret;
}
