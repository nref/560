#include "clientUtil.h"

bool host_sessions[] = { [0 ... MAX_HOSTS] = false };
char* hosts[] = { [0 ... MAX_HOSTS] = NULL };

const command_ptr_t cmdPtrs[] =
    { (void*)transfer, (void*)deposit,
      (void*)withdraw, (void*)inquiry };

const uint cmdsLen = 4;
const char* cmdNames[] =
    { "transfer", "deposit",
	  "withdraw", "inquiry" };

const int minFields[] = { 5, 4, 4, 3 };
const char* delimiter = " \n";
const char line_comment = '#';

result_t* defaultTransaction(char* host, 
							 transaction_t *t, 
							 result_t* (*cmd)(transaction_t*, CLIENT*)) {
	result_t* resp = NULL;
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
    
    if (NULL == t)	resp = cmd(NULL, client);		/* For session actions */
    else 			resp = cmd(t, client);			/* For transactions */
       
    if (NULL == resp) { 
    	clnt_perror(client, "call failed:");
    	ret->success = ATM_FAILURE;
    } else {
		ret->success = resp->success;
		ret->value = resp->value;
		ret->msg = resp->msg;
    }
	clnt_destroy(client);
    return ret;	
}

result_t* startSession(char* host) {
	return defaultTransaction(host, NULL, (void*)startsession_1);
}

result_t* stopSession(char* host) {
	return defaultTransaction(host, NULL, (void*)stopsession_1);
}

result_t* transfer(char* host, int from_account, int to_account, double amt) {	

	transaction_t *t = malloc(sizeof(transaction_t));
	t->to = to_account;
	t->from = from_account;
	t->value = amt;
	
	return defaultTransaction(host, t, transfer_1);
}

result_t* deposit(char* host, int account, double amt) {

	transaction_t *t = malloc(sizeof(transaction_t));
	t->to = account;
	t->value = amt;
	return defaultTransaction(host, t, deposit_1);
}

result_t* withdraw(char* host, int account, double amt) {
	
	transaction_t *t = malloc(sizeof(transaction_t));
	t->from = account;
	t->value = amt;
	return defaultTransaction(host, t, withdraw_1);
}

result_t* inquiry(char* host, int account) {
    			
    transaction_t *t = malloc(sizeof(transaction_t));
    t->account = account;
    return defaultTransaction(host, t, inquiry_1);
}

usr_cmd_t* newCommand(uint fieldCount, char** argv) {
	usr_cmd_t* cmd = malloc(sizeof(usr_cmd_t));
	cmd->argc = fieldCount;
	if (NULL == argv)
		cmd->argv = malloc(fieldCount*sizeof(char*));
	else cmd->argv = argv;
	return cmd;
}

void print_help() {
	printf("Help: ./program [command] | [filename]\n");
	printf("\nValid input for [command]: \n");
	printf("\t[remote hostname] inquiry  [account]\n");
	printf("\t[remote hostname] withdraw [account] [amount]\n");
	printf("\t[remote hostname] deposit  [account] [amount]\n");
	printf("\t[remote hostname] transfer [from account] [to account] [amount]\n");
	printf("\nEach line in [filename] must follow the same format.\n");
	printf("\nExample: ./program localhost transfer 1000 1001 99.95\n");
}

void helpAndExit(bool status) {
	print_help();
	exit(status);
}

uint getNumFields(char* line) {
	uint i = 0;
	char* field;
	
	if (NULL == line)
		return 0;
		
	char* line_copy = malloc(strlen(line)*sizeof(char));
	strcpy(line_copy, line);
	
	field = strtok(line_copy, delimiter);
	
	while (NULL != field) {
		field = strtok(NULL, delimiter);
		i++;
	} 
	return i;
}

bool checkNumArgs(usr_cmd_t* cmd) {
	bool numArgsOK = false;
	for (int i = 0; i < cmdsLen; i++) {
		if (cmd->argc == minFields[i]) {
			numArgsOK = true;
			break;
		}
	}
	return numArgsOK;
}

bool checkExpectedNumArgs(usr_cmd_t* cmd, int expected) {
	if (cmd->argc != expected) 
		return false;
	return true;
}

/* Return true if we are supposed to read commands from file */
bool checkArgs(int argc, char* argv[]) {
	bool fileMode = false;
	FILE* f = NULL;
	
    /* Check arguments. Require either the help argument, 
     * a commands file, or [4, 5, 6] arguments. 
     */
    if (argc > 1) 
	{
		/* If the first arg is the help argument */
		if (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h")) {
			helpAndExit(ATM_SUCCESS);
		}
		
		/* Try file mode. */
		if (argc == 2) {
			f = fopen(argv[1],"r");
			if (NULL == f) {
				printf("\'%s\' is not a file\n",argv[1]);
				exit(ATM_FAILURE);
			}
			else if (NULL != f) {
				printf("Reading file \'%s\'...\n\n", argv[1]);
				fileMode = true;				
			}
			fclose(f);
		}

		/* Default to parameter mode. */		
		
		/*	If we have the wrong number of arguments */
		else if ((argc < 4) || (argc > 6)) {
			helpAndExit(ATM_FAILURE);
		}
		
	} else { 
		printf("Not enough arguments.\n"); 
		helpAndExit(ATM_FAILURE); 
	}
	return fileMode;
}

int isValidCommand(usr_cmd_t* cmd, command_t* command) {
	int cmdIdx = -1;
	
	/* If incorrect number of fields provided */
	if (!checkNumArgs(cmd))
		return cmdIdx;
		
	for (uint i = 0; i < cmdsLen; i++) {
		/* If not valid name */
		if (!strcmp(command->name, cmdNames[i])) {
			cmdIdx = i;
			break;
		}
	}
	return cmdIdx;
}

command_t* parseCommand(usr_cmd_t* cmd) {
	int argc = cmd->argc;
	char** argv = cmd->argv;
	
	transaction_t *t = malloc(sizeof(transaction_t));
	command_t *command = malloc(sizeof(command_t));;
	char* pEnd = NULL;
		
	command->host = argv[0];
	command->name = argv[1];
	command->t = t;
	command->idx = isValidCommand(cmd, command);
	
	if (command->idx >= 0) {
		checkExpectedNumArgs(cmd, minFields[command->idx]);
		
		if (!strcmp(cmdNames[CMD_TRANSFER],command->name)) {
			t->account = atoi(argv[2]);
			t->to = atoi(argv[3]);
			t->value = strtod(argv[4], &pEnd);

		} else if (!strcmp(cmdNames[CMD_DEPOSIT],command->name)) {
			t->account = atoi(argv[2]);
			t->value = strtod(argv[3], &pEnd);
		
		} else if (!strcmp(cmdNames[CMD_WITHDRAW],command->name)) {
			t->account = atoi(argv[2]);
			t->value = strtod(argv[3], &pEnd);
			
		} else if (!strcmp(cmdNames[CMD_INQUIRY],command->name)) {
			t->account = atoi(argv[2]);
		}
	}
	return command;
}

bool doCommand(command_t *cmd) {

	bool cmdOK = false;
	int cmdIdx = cmd->idx;
	int i = 0;
	int session_id = 0;
	double *d = NULL;
	transaction_t* t = cmd->t;
	result_t* r;

	/* Find the previously known host or assign a new one */
	for (i = 0; i < MAX_HOSTS; i++) {
		
		/* First time seeing this host */
		if (NULL == hosts[i]) {
					
			/* Remember the host name so we can close it */
			hosts[i] = malloc(sizeof(char)*strlen(cmd->host));
			strcpy(hosts[i], cmd->host);
			
			session_id = i;
			break;
		}
		
		/* Seen this host before */
		if (!strcmp(cmd->host, hosts[i])) {
			session_id = i;
			break;
		}

		session_id = TOO_MANY_HOSTS;
	}
	
	if (TOO_MANY_HOSTS == session_id) {
		printf("Too many hosts.\n");
		return ATM_FAILURE;
	}
	
	/* If a session for this host is not started */
	if (!host_sessions[session_id]) {	
		
		/* Init a session on the server. Also checks hostname. */ 
		r = startSession(cmd->host);  
		if (NULL == r || !r->success) { 
			printf("BAD HOSTNAME, SERVER NOT STARTED, OR SESSION NOT OPEN.\n");
			return ATM_FAILURE;
		} 
		
		/* Mark session started */	
		else host_sessions[session_id] = true;
	}
	
	/* Print a message for the respective function */
	switch (cmdIdx) {
		case CMD_TRANSFER:	printf("Transferring %0.2lf from account %d to account %d: ", t->value, t->account, t->to);	break;
		case CMD_DEPOSIT:	printf("Depositing %0.2lf to account %d: ",					  t->value, t->account); 		break;
		case CMD_WITHDRAW:	printf("Withdrawing %0.2lf from account %d: ",				  t->value, t->account); 		break; 
		case CMD_INQUIRY:	printf("Balance of account %d: ",							 			t->account); 		break;
		default: break;
	}
	
	/* Match the command name to the respective function */
	switch (cmdIdx) {
		case CMD_TRANSFER:	r = (result_t*)cmdPtrs[CMD_TRANSFER]	(cmd->host, t->account, t->to, t->value);	break;
		case CMD_DEPOSIT:	r = (result_t*)cmdPtrs[CMD_DEPOSIT]		(cmd->host, t->account, 	   t->value);	break;
		case CMD_WITHDRAW:	r = (result_t*)cmdPtrs[CMD_WITHDRAW]	(cmd->host, t->account, 	   t->value);	break;
		case CMD_INQUIRY:	r = (result_t*)cmdPtrs[CMD_INQUIRY]		(cmd->host, t->account);					break;
		default: break;
	}

	switch (cmdIdx) {
		case CMD_INQUIRY: 	if (r->value) printf("%0.2lf ",r->value);
		default: 
			if (r->success)	printf("%s: SUCCESS ",cmdNames[cmdIdx]);
			else			printf("%s: FAILURE ", cmdNames[cmdIdx]); 
			if (0 < r->msg.msg_len) printf("[%s]\n", r->msg.msg_val);
			else printf("\n");
			break;
	}
	
	return (r->success);	
}

bool tryDoCommand(usr_cmd_t* cmd) {
	bool success = ATM_SUCCESS;
	
	command_t* command = parseCommand(cmd);
	if (command->idx < 0) {
		printf("Invalid command \'%s\'\n",command->name);
		success = ATM_FAILURE;
	}
	else if (!doCommand(command))
		success = ATM_FAILURE;
		
	return success;
}

void closeSessions() {
	for (uint i = 0; i < MAX_HOSTS; i++)
		if (host_sessions[i]) stopSession(hosts[i]);
}

bool doSingleCommand(usr_cmd_t* cmd) {
	bool ret = tryDoCommand(cmd);
	closeSessions();
	return ret;
}

/* Execute each command in the file */
bool doFileCommands(char* file) {
	printf("Executing commands from file...\n\n");
	
	bool allCommandsSucceeded = ATM_SUCCESS;
	
	FILE* f = fopen(file, "r");
	size_t len;
	ssize_t read;
	char* line = NULL;
	char* field;
	uint i = 0;

	/* For each line (== command) in the file */
	while ((read = getline(&line, &len, f)) != -1) {
	
		/* Skip commented lines and blank lines */
		if (line[0] == line_comment || line[0] == '\n' 
									|| line[0] == ' ' 
									|| line[0] == '\t') continue;
		
		uint fieldCount = getNumFields(line); /* Field count */
		if (fieldCount < 3) continue;
		
		/* Allocate memory */
		usr_cmd_t* cmd = newCommand(fieldCount, NULL);
		
		/* Copy each field in the line */
		i = 0;
		field = strtok(line, delimiter);
		while (NULL != field) {			
			cmd->argv[i] = malloc(strlen(field)+1*sizeof(char));
			strcpy(cmd->argv[i],field);
		
			field = strtok(NULL, delimiter);
			i++;
		}
		
		/* Execute the command */
		if (!tryDoCommand(cmd))
			allCommandsSucceeded = ATM_FAILURE;
	}
	fclose(f);
	closeSessions();

	return allCommandsSucceeded;
}