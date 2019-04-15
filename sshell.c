#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/wait.h>
#include <fcntl.h>

/*************************************************************
 *                    MACRO DEFINITIONS                      *
 *************************************************************/

#define MAX_CHAR 255
#define MAX_CMD 512
#define MAX_ARGS 16

/*************************************************************
 *                    STRUCT and ENUM DEFINITIONS             *
 *************************************************************/

/* redirection code */
enum {
    ARGUMENT,
    INPUT,
    OUTPUT
}; 

/* error code enum */
enum {
    SUCCESS,
    ERR_INVALID_CMDLINE,
    ERR_CMD_NOTFOUND,
    ERR_DIR_NOTFOUND,
    ERR_OPEN_INPUTFILE,
    ERR_OPEN_OUTPUTFILE,
    ERR_NO_INPUTFILE,
    ERR_NO_OUTPUTFILE,
    ERR_INPUT_MISLOCATED,
    ERR_OUTPUT_MISLOCATED,
    ERR_BACKGROUND_MISLOCATED,
    ERR_ACTIVE_JOBS
}; 

/* builtin command code enum */
enum { 
    EXIT,
    CD,
    PWD,
    NOT_BUILTIN
};

/* command strcut */
struct command {
    char command[MAX_CMD];		/* the whole command */
    char *args[MAX_ARGS];		/* arguments of the command */
    int num_input;			/* number of input redirection */
    int num_output;			/* number of output redirection */
    char input_file[MAX_ARGS][MAX_CMD];	/* the array of the input files */
    char output_file[MAX_ARGS][MAX_CMD];/* the array of the output files */
    int num_args;			/* number of arguments in the command line */
};

/* job struct */
struct job {
    char commandline[MAX_CMD];		/* the total command line */
    struct command processes[MAX_CMD];	/* a job contains one or more commands */
    int num_commands;			/* number of commands/processes */
};

/*************************************************************
 *                    LOCAL FUNCTION PROTOTYPES              *
 *************************************************************/

char find_closest_occurence(char *array, const char *delim);
void read_job(struct job *job);
void read_command(struct command *cmd);
void free_command(struct command *cmd);
int is_valid_command(const struct command cmd);
int is_builtin_command(const struct command cmd);
int cd(const char *dir);
int pwd();
void redirection(int mode, const char files[MAX_ARGS][MAX_CMD], int num_files);
int check_redirection_file(int mode, const char files[MAX_ARGS][MAX_CMD], int num_files);
void error_message(int error_code);

/*************************************************************
 *                    LOCAL FUNCTION DEFINITIONS             *
 *************************************************************/

/*
 * This function reads the whole command line from terminal and store each command
 * @param - {job *} - the job to store one or more commands (pipeline) 
 * @return - none
 */
void read_job(struct job *job) {
    char commands[MAX_CMD];
   // char command[MAX_CMD];
    char *token;

    fgets(commands, MAX_CMD, stdin);		/* get the entire command line */
    commands[strlen(commands) - 1] = 0;		/* get rid of newline */
    job->num_commands = 0;			/* initialize number of command */

    token = strtok(commands, "|");		/* get the first command */
    if(token == NULL) {				/* only one or zero command */
    	if(commands[0] != 0) {			/* one command */
	    read_command(&(job->processes[0]));
	} else {				/* no command */
            //job->processes[0] = NULL;	
	}
    }
    while(token != NULL) {
	read_command(&(job->processes[job->num_commands++]));
	token = strtok(NULL, "|");	
    }
    return;
}	

/*
 * This function parses the command and store it
 * @param - {struct *} - the command struct
 *        - {char *} - the command to be parsed
 * @return - none
 */
void read_command(struct command *cmd) {
    char command[MAX_CMD], arg[MAX_CMD];
    int num_white_space;

    fgets(command, MAX_CMD, stdin);		/* get the argument list for the command */
    command[strlen(command) - 1] = 0;		/* get rid of newline */
    cmd->num_args = 0;				/* initialize number of arguments */
    cmd->num_input = 0;				/* initialize number of input redirections */
    cmd->num_output = 0;			/* initialize number of output redirections */
    strcpy(cmd->command, command);		/* store command line */
    
    /* get rid of leading spaces */
    for(num_white_space = 0; command[num_white_space] == ' '; num_white_space++);
    strcpy(command, command + num_white_space);

    /* get rid of trailing spaces */
    int i;
    for(i = strlen(command) - 1; i >= 0 && command[i] == ' '; i--)
        command[i] = 0;

    /* parse the command */
    int read_code = ARGUMENT;
    i = 0;
    while(i < strlen(command)) {
        memset(arg, 0, MAX_CMD);
	int j = 0;

	/* get the argument */
        while(command[i] != ' ' && command[i] != '<' && command[i] != '>' && command[i] != 0) {
            arg[j++] = command[i++];
	}
	arg[j] = 0;				/* add null terminator */

        for(; command[i] == ' '; i++);		/* get rid of leading spaces */
      
	switch(read_code) {
            case ARGUMENT:	/* an argument for the program */
	        cmd->args[cmd->num_args] = (char *) malloc((strlen(arg) + 1));
         	strcpy(cmd->args[cmd->num_args++], arg);
		break;
	    case INPUT:		/* input file */
		if(arg[0] == 0) {
		    strcpy(cmd->input_file[cmd->num_input++], " ");
	        } else {
		    strcpy(cmd->input_file[cmd->num_input++], arg);
                }
		read_code = ARGUMENT;
	        break;
	    case OUTPUT:	/* output file */
		if(arg[0] == 0) {
                    strcpy(cmd->output_file[cmd->num_output++], " ");
                } else {
                    strcpy(cmd->output_file[cmd->num_output++], arg);
                }
		read_code = ARGUMENT;
	        break;
	}

	/* check for redirections */
	if(command[i] == '<') {			/* read input file for next argument */
            read_code = INPUT;
            i++;
            for(; command[i] == ' '; i++);	/* get rid of leading spaces */
        } else if(command[i] == '>') {		/* read output file for next argument */
            read_code = OUTPUT;
            i++;
            for(; command[i] == ' '; i++);	/* get rid of leading spaces */
        }
    }
    
    /* check given files */
    if(read_code == INPUT) {
        strcpy(cmd->input_file[cmd->num_input++], " ");	 /* use emtpy string to indicate not given file */
    } else if (read_code == OUTPUT) {
    	strcpy(cmd->output_file[cmd->num_output++], " "); /* use emtpy string to indicate not given file */	
    }
    
    cmd->args[cmd->num_args] = NULL;		/* set null terminator */
    return;
}

/*
 * This function frees the memory allocated for the arguments in the command line struct
 * @param - {struct *} - the command line struct
 * @return - none
 */
void free_command(struct command *cmd) {
    int i;
    for(i = 0; i < (cmd->num_args); i++) {
        free(cmd->args[i]);			/* free the allocated memory for each argument */
    }
    return;
}

/* TO DO !!!
 * This function check if the command line is valid
 * @param - {const struct} - the command line struct
 * @return - {int} - error code
 */
int is_valid_command(const struct command cmd) {
    return 0;
}

/*
 * This function check if the command line is a builtin command
 * @param - {const struct} - the command line struct
 * @return - {int} - builtin command enum
 */
int is_builtin_command(const struct command cmd) {
    if(strcmp(cmd.args[0], "exit") == 0) {		/* exit */
        return EXIT;
    } else if(strcmp(cmd.args[0], "cd") == 0) {		/* cd */
        return CD;
    } else if(strcmp(cmd.args[0], "pwd") == 0) {	/* pwd */
        return PWD;
    } else {
        return NOT_BUILTIN;				/* not a built in command */
    }
}

/*
 * This function changes the working directory specfied by the parameter
 * @param - {const char *} - the directory name
 * @return - {int} - return success or failure status 
 */
int cd(const char *dir) {
   int status = chdir(dir);
   if(status == -1) {
       error_message(ERR_DIR_NOTFOUND);
       return EXIT_FAILURE;
   } 
   return EXIT_SUCCESS;
}

/*
 * This function prints out the working directory
 * @param - none
 * @return - {int} - return success or failure status
 */
int pwd() {
    char cwd[MAX_CMD];
    getcwd(cwd, MAX_CMD);
    if(cwd != NULL) {		/* success */
        printf("%s\n", cwd);
        return EXIT_SUCCESS;
    } else {			/* failure */
        return EXIT_FAILURE;	
    }
}

/*
 * This function handles the input/output redirection and connects according std
 * @param - {int} - indicates if it is for input redirection or output redirection
 *        - {char *[]} - the array of files
 *        - {int} - number of files
 * @return - none
 */
void redirection(int mode, const char files[MAX_ARGS][MAX_CMD], int num_files) {
    int fd, i;
    for(i = 0; i < num_files; i++) {
        if(mode == INPUT) {			
            fd = open(files[i], O_RDONLY);
            dup2(fd, STDIN_FILENO);         	/* input redirection */
        } else {	
	    fd = open(files[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); /* create the file if not exist */
	    dup2(fd, STDOUT_FILENO);		/* output redirection */
        }
        close(fd);				/* close unused file */
    }
    return;
}

/*
 * This function checks if the input/output file can be opened and if it is given
 * @param - {int} - indicates if it is for input redirection or output redirection
 *        - {char *[]} - the array of files
 *        - {int} - number of files
 * @return - error code
 */
int check_redirection_file(int mode, const char files[MAX_ARGS][MAX_CMD], int num_files) {
    int i;
    for(i = 0; i < num_files; i++) {
        /* check if the file name is given */
        if(strcmp(files[i], " ") == 0) {
            return (mode == INPUT) ? ERR_NO_INPUTFILE : ERR_NO_OUTPUTFILE;
        }
    
        /* check if the file can be opened */
        if(mode == INPUT) {		/* input redirection */
            if(open(files[i], O_RDONLY) < 0) {		/* error opening file for reading */
                return ERR_OPEN_INPUTFILE;
	    }
        } else {					/* output redirection */
	    /* file exists but file doesn't allow access */
            if(access(files[i], F_OK) == 0 && access(files[i], W_OK) < 0) {
                return ERR_OPEN_OUTPUTFILE;		/* error opening file for writing */
	    }
        }
    }
    return SUCCESS;
}

/*
 * This function prints out according error mesage depending on error code
 * @param - {int} - error code enum
 * @return - none
 */
void error_message(int error_code) {
    switch(error_code) {
        case(ERR_INVALID_CMDLINE):
	    fprintf(stderr, "Error: invalid command line\n");
	    break;
	case(ERR_CMD_NOTFOUND):
	    fprintf(stderr, "Error: command not found\n");
            break;
	case(ERR_DIR_NOTFOUND):
            fprintf(stderr, "Error: no such directory\n");
	    break;
	case(ERR_OPEN_INPUTFILE):
            fprintf(stderr, "Error: cannot open input file\n");
	    break;
	case(ERR_OPEN_OUTPUTFILE):
            fprintf(stderr, "Error: cannot open open file\n");
	    break;
	case(ERR_NO_INPUTFILE):
            fprintf(stderr, "Error: no input file\n");
	    break;
	case(ERR_NO_OUTPUTFILE):
            fprintf(stderr, "Error: no output file\n");
	    break;
	case(ERR_INPUT_MISLOCATED):
            fprintf(stderr, "Error: mislocated input redirection\n");
	    break;
	case(ERR_OUTPUT_MISLOCATED):
            fprintf(stderr, "Error: mislocated output redirection\n");
	    break;
	case(ERR_BACKGROUND_MISLOCATED):
            fprintf(stderr, "Error: mislocated background sign\n");
	    break;
	case(ERR_ACTIVE_JOBS):
            fprintf(stderr, "Error: active jobs still running\n");
	    break;
    }
}

/*
 * main function of the sshell
 */
int main(int argc, char *argv[])
{	
    pid_t pid;
    int status;

    while(1) {
        struct command cmd;
//	struct job job;
	int builtin_command_code;

	printf("sshell$ ");		/* Display prompt */
//	read_job(&job);			/* read the job */
	read_command(&cmd);		/* read command from stdin */

	/* no command is entered */
	if(cmd.args[0] == NULL) {
	    continue;
	} 
	
	/* check if it is a built in command */
	builtin_command_code = is_builtin_command(cmd);

	/* check if it has input redirection */
        if(cmd.num_input > 0) {
	    /* check if it has errors */
	    int error_code = check_redirection_file(INPUT, cmd.input_file, cmd.num_input);
	    if(error_code != SUCCESS) {
                error_message(error_code);
		continue;
            }
	}

	/* check if it has output redirection */
        if(cmd.num_output > 0) {
            /* check if it has errors */
            int error_code = check_redirection_file(OUTPUT, cmd.output_file, cmd.num_output);
            if(error_code != SUCCESS) {
                error_message(error_code);
                continue;
            }
        }

	/* check if it is builtin command */
        if(builtin_command_code == EXIT) {          /* leave the shell */
            fprintf(stderr, "Bye...\n");
            exit(EXIT_SUCCESS);
        }
        else if(builtin_command_code == CD) {    /* run cd command */
            status = cd(cmd.args[1]);

        } else if(builtin_command_code == PWD) {    /* run pwd command */
            status = pwd();
        }

	/* run not-built-in command */
	pid = fork();			/* fork child process */
	if(pid == 0) {			/* Child */
	    /* perform input redirection */
	    if(cmd.num_input > 0) {
                redirection(INPUT, cmd.input_file, cmd.num_input);
            } 
	    
	    /* perform output redirection */
	    if(cmd.num_output > 0) {
                redirection(OUTPUT, cmd.output_file, cmd.num_output);
	    }
	    
	    if(builtin_command_code == NOT_BUILTIN) {	/* not builtin command */
	        execvp(cmd.args[0], cmd.args);		
	        /* execvp error */
	        error_message(ERR_CMD_NOTFOUND);
	        exit(EXIT_FAILURE);
	    } else {					/* builtin command */
                exit(status);
            }
	} else if(pid > 0) {		/* Parent */
	    waitpid(-1, &status, 0);		/* wait for child to complete */
	    /* Information message after execution */
	    fprintf(stderr, "+ completed '%s' [%d]\n", cmd.command, WEXITSTATUS(status));
	} else {			/* fork error */
	    perror("fork");	
	    exit(EXIT_FAILURE);
	}
	free_command(&cmd);		/* free the memory allocated for the command line */
    }
    return EXIT_SUCCESS;
}
