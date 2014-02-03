#include "bankError.h"

static char *msgErr[] = { "SERVICE NOT RUNNING", "ACCOUNT INVALID",
						   "START FAIL", "WITHDRAW FAIL",
				   		   "DEPOSIT FAIL", "TRANSFER FAIL",
				  		   "INQUIRY FAIL", "STOP FAIL" };
				   
result_t* notStartedError() {
	ret.success = false;
	ret.message.message_val = msgErr[0];
	return &ret;
}

result_t* invalidAccountError() {
	ret.success = false;
	ret.message.message_val = msgErr[1];
	return &ret;
}

result_t* startError() {
	ret.success = false;
	ret.message.message_val = msgErr[2];
	return &ret;
}

result_t* transferError() {
	ret.success = false;
	ret.message.message_val = msgErr[5];
	return &ret;
}

result_t* withdrawError() {
	ret.success = false;
	ret.message.message_val = msgErr[3];
	return &ret;
}

result_t* depositError() {
	ret.success = false;
	ret.message.message_val = msgErr[4];
	return &ret;
}

result_t* inquiryError() {
	ret.success = false;
	ret.message.message_val = msgErr[6];
	return &ret;
}

result_t* stopError() {
	ret.success = false;
	ret.message.message_val = msgErr[7];
	return &ret;
}