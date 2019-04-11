#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/wait.h>

/*************************************************************
 *                    MACRO DEFINITIONS                      *
 *************************************************************/

#define MAX_CMD 512
#define MAX_ARGS 16

/*************************************************************
 *                    STRUCT DEFINITIONS                      *
 *************************************************************/

/* commandline strcut */
struct commandline {
	char command[MAX_CMD];
	char *args[MAX_ARGS];
	int num_args;
};

/*************************************************************
 *                    LOCAL FUNCTION PROTOTYPES              *
 *************************************************************/

void read_command(struct commandline *cmd);

/*************************************************************
 *                    LOCAL FUNCTION DEFINITIONS             *
 *************************************************************/

/*
 * This function reads the command from terminal
 * @param - {int *} - stores the number of argument in the command
 * @return - none
 */
void read_command(struct commandline *cmd) {
	char *arg;
	
	fgets(cmd->command, MAX_CMD, stdin);		/* get the argument list for the command */
	cmd->command[strlen(cmd->command) - 1] = 0;	/* get rid of newline */
	cmd->num_args = 0;

	/* separte commandline into arguments) */
	arg = strtok(cmd->command, " ");	/* get the first token */
	
	/* find the number of arguments and copy arugments to command*/
	while(arg != NULL) {			/* parse the command */
		cmd->args[cmd->num_args] = (char *) malloc((strlen(arg) + 1) * sizeof(char));
		strcpy(cmd->args[(cmd->num_args)++], arg); 	/* copy the argument */
		arg = strtok(NULL, " ");		/* get next argument */
	}
	cmd->args[cmd->num_args] = NULL;		/* set null terminator */

	return;
}


/*
 * main function of the sshell
 */
int main(int argc, char *argv[])
{
	pid_t pid;
	int status;

	while(1) {			/* Repeat forever */
		struct commandline cmd;

		printf("sshell$ ");     /* Display prompt */
		read_command(&cmd);     /* read command from stdin */

		if(cmd.args[0] == NULL) {			/* no command is entered */
			continue;
		} else if(strcmp(cmd.args[0], "exit") == 0) {	/* leave the shell */
			printf("Bye...\n");
			exit(EXIT_FAILURE);
		}

		pid = fork();				/* fork child process */
		if(pid == 0) {
			/* Child */
			execvp(cmd.args[0], cmd.args);
			perror("execvp");		/* execvp error */
			exit(EXIT_FAILURE);
		} else if(pid > 0) {
			/* Parent */
			waitpid(-1, &status, 0);		/* wait for child to complete */
			
			/* Information message after execution */
			int i;
			fprintf(stderr, "+ completed '%s", cmd.args[0]);
			for(i = 1; i < cmd.num_args; i++) {
				fprintf(stderr, " %s", cmd.args[i]);
			}
			fprintf(stderr, "' [%d]\n", status); 
		} else {
			perror("fork");			/* fork error */
			exit(EXIT_FAILURE);
		}
	}
	return EXIT_SUCCESS;
}
