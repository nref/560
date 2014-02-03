#include "bank.h"

extern bool _doTransfer(transaction_t*);
extern bool _doWithdrawal(transaction_t*);
extern bool _doDeposit(transaction_t*);
extern bool _doInquiry(transaction_t*);
extern bool _doStart();
extern bool _doStop();
extern result_t* _doAction(transaction_t*, 
		bool (*)(transaction_t*),
		result_t* (*)(), result_t* (*)(),
		bool (*)(int,int), int, int);