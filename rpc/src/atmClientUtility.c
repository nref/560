#include "atmClientUtility.h"

static bool host_started = false;
static char* last_host;

cmd_t* newCommand(uint fieldCount, char** argv) {
	cmd_t* cmd = malloc(sizeof(cmd_t));
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
	printf("\nExample: ./program 127.0.0.1 transfer 1000 1001 99.95\n");
}

void helpAndExit(bool status) {
	print_help();
	exit(status);
}

uint getNumFields(char* line) {
	uint j;
	char* field;
	
	if (NULL == line)
		return 0;
		
	char* line_copy = malloc(strlen(line)*sizeof(char));
	strcpy(line_copy, line);
	
	field = strtok(line_copy, delimiter);
	
	while (NULL != field) {
		field = strtok(NULL, delimiter);
		j++;
	} 
	return j;
}

bool checkNumArgs(cmd_t* cmd) {
	bool numArgsOK = false;
	for (int i = 0; i < cmdsLen; i++) {
		if (cmd->argc == minFields[i]) 
			numArgsOK = true;
	}
	return numArgsOK;
}

bool checkExpectedNumArgs(cmd_t* cmd, int expected) {

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
		helpAndExit(ATM_SUCCESS); 
	}
	return fileMode;
}

int isValidCommand(cmd_t* cmd, command_t* command) {
	int cmdIdx = -1;
	
	/* If incorrect number of fields provided */
	if (!checkNumArgs(cmd))
		return cmdIdx;
		
	for (int i = 0; i < cmdsLen; i++) {
		/* If not valid name */
		if (!strcmp(command->name, cmdNames[i])) {
			cmdIdx = i;
			break;
		}
	}
	return cmdIdx;
}

command_t* parseCommand(cmd_t* cmd) {
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

	double *d = NULL;
	bool cmdOK = false;
	int cmdIdx = cmd->idx;
	transaction_t* t = cmd->t;
	result_t* r;

	if (!host_started) {
		host_started = true;
		
		/* Init a session on the server. Also checks hostname. */ 
		r = startSession(cmd->host);  
		if (NULL == r || !r->success) { 
			printf("BAD HOSTNAME, SERVER NOT STARTED, OR SERVER FAILED TO START.\n");
			return ATM_FAILURE;
		}
		/* Remember the host name so we can close it */
		last_host = malloc(strlen(cmd->host)*sizeof(char));
		strcpy(last_host, cmd->host);
	}
	switch (cmdIdx) {
		case CMD_TRANSFER: 	
			printf("Transferring %0.2lf from account %d to account %d: ",
    			t->value, t->account, t->to); 
    		break;
		case CMD_DEPOSIT: 
			printf("Depositing %0.2lf to account %d: ", t->value, t->account);
			break;
		case CMD_WITHDRAW: 
			printf("Withdrawing %0.2lf from account %d: ", t->value, t->account);
			break; 
		case CMD_INQUIRY: 
		    printf("Balance of account %d: ", t->account);
		    break;
		default: break;
	}
	/* Match the command name to the respective function */
	switch (cmdIdx) {
		case CMD_TRANSFER: r = (result_t*)cmdPtrs[CMD_TRANSFER](cmd->host, t->account, t->to, t->value); break;
		case CMD_DEPOSIT: r = (result_t*)cmdPtrs[CMD_DEPOSIT](cmd->host, t->account, t->value); break;
		case CMD_WITHDRAW: r = (result_t*)cmdPtrs[CMD_WITHDRAW](cmd->host, t->account, t->value); break;
		case CMD_INQUIRY: r = (result_t*)cmdPtrs[CMD_INQUIRY](cmd->host, t->account); break;
		default: break;
	}

	switch (cmdIdx) {
		case CMD_INQUIRY: if (r->value) printf("%0.2lf ",r->value);
		default: 
			if (r->success) printf("%s SUCCESS\n",cmdNames[cmdIdx]);
			else printf("%s FAILURE\n", cmdNames[cmdIdx]); 
			break;
	}
	
	return (r->success);	
}

bool tryDoCommand(cmd_t* cmd) {
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

bool doFileCommands(char* file) {
	printf("Executing commands from file...\n\n");
	
	bool allCommandsSucceeded = ATM_SUCCESS;
	
	FILE* f = fopen(file, "r");
	size_t len;
	ssize_t read;
	char* line = NULL;
	char* field;
	uint i = 0;

	/* Execute each command in the file */
	while ((read = getline(&line, &len, f)) != -1) {
		if (line[0] == line_comment || line[0] == '\n') continue;
		
		uint fieldCount = getNumFields(line); /* field count for each cmd */
		if (fieldCount < 3) continue;
		
		/* Allocate memory */
		cmd_t* cmd = newCommand(fieldCount, NULL);
		
		/* Copy each field in the line */
		i = 0;
		field = strtok(line, delimiter);
		while (NULL != field) {			
			cmd->argv[i] = malloc(strlen(field)+1*sizeof(char));
			strcpy(cmd->argv[i],field);
		
			field = strtok(NULL, delimiter);
			i++;
		}
		
		if (!tryDoCommand(cmd))
			allCommandsSucceeded = ATM_FAILURE;
	}
	if (host_started) stopSession(last_host);
	fclose(f);
	return ATM_SUCCESS;
}