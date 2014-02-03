#include "bankOk.h"

static char *msgOk[] = { "START OK", "WITHDRAW OK",
				  		  "DEPOSIT OK", "TRANSFER OK",
				  		  "INQUIRY OK", "STOP OK" };

result_t* startOk() {
	ret.success = true;
	ret.message.message_val = msgOk[0];
	return &ret;
}

result_t* withdrawOk() {
	ret.success = true;
	ret.message.message_val = msgOk[1];
	return &ret;
}

result_t* depositOk() {
	ret.success = true;
	ret.message.message_val = msgOk[2];
	return &ret;
}

result_t* transferOk() {
	ret.success = true;
	ret.message.message_val = msgOk[3];
	return &ret;
}

result_t* inquiryOk() {
	ret.success = true;
	ret.message.message_val = msgOk[4];
	return &ret;
}

result_t* stopOk() {
	ret.success = true;
	ret.message.message_val = msgOk[5];
	return &ret;
}
