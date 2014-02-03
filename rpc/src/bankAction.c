#include "bankAction.h"
#include <unistd.h> /* for sleep() */

result_t ret;
uint init_done = 0;

bool _doTransfer(transaction_t *t) {
	accounts_table[t->from - BASE] -= t->value;
	accounts_table[t->to - BASE] += t->value;
	return writeAccounts();
}

bool _doWithdrawal(transaction_t *t) {
	accounts_table[t->from - BASE] -= t->value;
	return writeAccounts();
}

bool _doDeposit(transaction_t *t) {
	accounts_table[t->to - BASE] += t->value;
	return writeAccounts();
}

bool _doInquiry(transaction_t *t) {
	ret.value = accounts_table[t->account - BASE];
	return ATM_SUCCESS;
}

result_t* _doAction(transaction_t *t, 
					bool (*action)(transaction_t *t),
					result_t* (*actionOk)(),
					result_t* (*actionError)(),
					bool (*isValidInput)(int,int),
					int account1, int account2)
{	
	if (!init_done) 
		return notStartedError();
		
	if (isValidInput(account1, account2)) {
		if (action(t)) return actionOk();
		return actionError();
	}
	return invalidAccountError();
}

bool _doStart() {
	char* accounts_init_file = "accounts_start";
	if (!init_done) {
		if (readConfiguration(accounts_init_file)) {
			init_done = true;
		}
	}
	return init_done;
}

bool _doStop() {
	sleep(write_interval);
	return writeAccounts();
}
