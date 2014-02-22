#include <rpc/rpc.h>
#include "serverUtil.h"

result_t* transfer_1(transaction_t *t, CLIENT *client) {
	return _doAction(t, _doDeposit, twoValidAccounts, t->to, t->from);
}

result_t* withdraw_1(transaction_t *t, CLIENT *client) {
	return _doAction(t, _doWithdrawal, isValidAccount, t->from, BASE);
}

result_t* deposit_1(transaction_t *t, CLIENT *client) {
	return _doAction(t, _doDeposit, isValidAccount, t->to, BASE);
}

result_t* inquiry_1(transaction_t *t, CLIENT *client) {
	return _doAction(t, _doInquiry, isValidAccount, t->account, BASE);
}

result_t* startsession_1(void* t, CLIENT *client){
	if (_doStart())	return startOk();
	else			return startError();
}

result_t* stopsession_1(void* t, CLIENT *client){
	if (_doStop())	return stopOk();
	else			return stopError();
}

result_t* transfer_1_svc(transaction_t *t, struct svc_req *svc) {
    CLIENT *client;
    return transfer_1(t, client);
}

result_t* withdraw_1_svc(transaction_t *t, struct svc_req *svc) {
    CLIENT *client;
    return withdraw_1(t, client);
}

result_t* deposit_1_svc(transaction_t *t, struct svc_req *svc) {
    CLIENT *client;
    return deposit_1(t, client);
}

result_t* inquiry_1_svc(transaction_t *t, struct svc_req *svc) {
    CLIENT *client;
	return inquiry_1(t, client);
}

result_t* startsession_1_svc(void *t, struct svc_req *svc) {
    CLIENT *client;
    return startsession_1(t, client);
}

result_t* stopsession_1_svc(void *t, struct svc_req *svc) {
    CLIENT *client;
    return stopsession_1(t, client);
}