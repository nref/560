struct transaction_t {
	int to;		/* For transfer */
	int from;	/* For transfer */
	int account;	
	double value;
};

struct result_t {
	double value;
	int success;
	char msg<>;
};

program TRANSACTION {
    version TVERS {
    	result_t TRANSFER(transaction_t)= 1;
        result_t DEPOSIT(transaction_t)	= 2;
        result_t WITHDRAW(transaction_t)= 3;
        result_t INQUIRY(transaction_t)	= 4;
	result_t STARTSESSION()		= 5;
    	result_t STOPSESSION()		= 6;
	} = 1;
} = 10101;