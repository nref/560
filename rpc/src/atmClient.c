#include "atmClient.h"

#define NAME TRANSACTION
#define VERS TVERS

result_t* startSession(char* host) {
	result_t* r;
	result_t* ret = malloc(sizeof(result_t));
	CLIENT* client = clnt_create(host, NAME, VERS, "udp");
		
	if (NULL == client) {
		clnt_pcreateerror(host);
		ret->success = ATM_FAILURE;
		return ret;
    }
    
	/* Change timeout */
	struct timeval tv;
    tv.tv_sec = 1;
    clnt_control(client, CLSET_TIMEOUT, (char*)&tv);
    
	r = startsession_1((void*)0, client);  
	
	if (NULL == r) { 
    	clnt_perror(client, "call failed:"); 
    	ret->success = ATM_FAILURE;
    } else {
		ret->success = r->success;
		ret->value = r->value;
		ret->message = r->message;
    }
	clnt_destroy(client);
    return ret;
}

result_t* stopSession(char* host) {
	result_t* r;
	result_t* ret = malloc(sizeof(result_t));
	CLIENT* client = clnt_create(host, NAME, VERS, "udp");
		
	if (NULL == client) {
		clnt_pcreateerror(host);
		ret->success = ATM_FAILURE;
		return ret;
    }
    
	/* Change timeout */
	struct timeval tv;
    tv.tv_sec = 1;
    clnt_control(client, CLSET_TIMEOUT, (char*)&tv);
    
	r = stopsession_1((void*)0, client);  
	
	if (NULL == r) { 
    	clnt_perror(client, "call failed:"); 
    	ret->success = ATM_FAILURE;
    } else {
		ret->success = r->success;
		ret->value = r->value;
		ret->message = r->message;
    }
	clnt_destroy(client);
    return ret;
}

result_t* transfer(char* host, int from_account, int to_account, double amt) {	
	 
	transaction_t t;
	t.to = to_account;
	t.from = from_account;
	t.value = amt;
	
	result_t* r;
	result_t* ret = malloc(sizeof(result_t));
	
	CLIENT* client = clnt_create(host, NAME, VERS, "udp"); 
	if (client == NULL) {
		clnt_pcreateerror(host);
		ret->success = ATM_FAILURE;
		return ret;
    }
    
    r = transfer_1(&t, client);
       
    if (r == NULL) { 
    	clnt_perror(client, "call failed:");
    	ret->success = ATM_FAILURE;
    } else {
		ret->success = r->success;
		ret->value = r->value;
		ret->message = r->message;
    }
	clnt_destroy(client);
    return ret;
}

result_t* deposit(char* host, int account, double amt) {

	transaction_t t;
	t.to = account;
	t.value = amt;

	result_t* r;
	result_t* ret = malloc(sizeof(result_t));
	
	CLIENT* client = clnt_create(host, NAME, VERS, "udp"); 
	if (client == NULL) {
		clnt_pcreateerror(host);
		ret->success = ATM_FAILURE;
		return ret;
    }
    
    r = deposit_1(&t, client); 
      
    if (r == NULL) { 
    	clnt_perror(client, "call failed:"); 
    	ret->success = ATM_FAILURE;
    } else {
		ret->success = r->success;
		ret->value = r->value;
		ret->message = r->message;
    }
    
	clnt_destroy(client);
    return ret;
}

result_t* withdraw(char* host, int account, double amt) {
	
	transaction_t t;
	t.from = account;
	t.value = amt;

	result_t* r;
	result_t* ret = malloc(sizeof(result_t));
	
	CLIENT* client = clnt_create(host, NAME, VERS, "udp"); 
	if (client == NULL) {
		clnt_pcreateerror(host);
		ret->success = ATM_FAILURE;
		return ret;
    }
    
    r = withdraw_1(&t, client);  
     
    if (r == NULL) { 
    	clnt_perror(client, "call failed:"); 
    	ret->success = ATM_FAILURE;
    } else {
		ret->success = r->success;
		ret->value = r->value;
		ret->message = r->message;
    }
    
	clnt_destroy(client);
    return r;
}

result_t* inquiry(char* host, int account) {
    			
    transaction_t t;
    t.account = account;
    
    result_t* r;
	result_t* ret = malloc(sizeof(result_t));
	
    CLIENT* client = clnt_create(host, NAME, VERS, "udp");
    
	if (NULL == client) {
		clnt_pcreateerror(host);
		ret->success = ATM_FAILURE;
		return ret;
    }
    
	struct timeval tv;
    tv.tv_sec = 1;
    clnt_control(client, CLSET_TIMEOUT, (char*)&tv);
    
    r = inquiry_1(&t, client);  
     
    if (NULL == r) { 
    	clnt_perror(client, "call failed:"); 
    	ret->success = ATM_FAILURE;
    } else {
		ret->success = r->success;
		ret->value = r->value;
		ret->message = r->message;
    }
	clnt_destroy(client);
    return ret;
}