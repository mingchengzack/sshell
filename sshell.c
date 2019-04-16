#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/wait.h>
#include <fcntl.h>

/*************************************************************
 *                    MACRO DEFINITIONS                      *
 ************************************************************/

#define MAX_FILE 50
#define MAX_CMD 512
#define MAX_PROCESS 128
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
    pid_t pid;					/* the process id */
    int status;					/* exit status */
    char command[MAX_CMD];			/* the whole command */
    char *args[MAX_ARGS];			/* arguments of the command */
    int num_input;				/* number of input redirection */
    int num_output;				/* number of output redirection */
    char input_file[MAX_ARGS][MAX_FILE];	/* the array of the input files */
    char output_file[MAX_ARGS][MAX_FILE];	/* the array of the output files */
    int num_args;				/* number of arguments in the command line */
    struct command *next_command;		/* the comamnd for pipeling */
};

/* job struct */
struct job {
    char commandline[MAX_CMD];			/* the total command line */
    struct command *first_command;		/* the first command of a job */
    int num_processes;				/* the number of commands/processes */
};

/*************************************************************
 *                    LOCAL FUNCTION PROTOTYPES              *
 *************************************************************/

void insert_status(struct command **root, pid_t pid, int status);
void insert_command(struct command **root, struct command *cmd);
void read_job(struct job *job);
struct command* read_command(char *command);
void pipeline(struct command *cmd, int fd[2]);
void free_job(struct job *job);
void free_command(struct command *cmd);
int is_empty_command(char *cmd);
int is_valid_command(struct command *cmd);
int check_job(struct job *job);
int is_builtin_command(const struct command *cmd);
int cd(const char *dir);
int pwd();
void redirection(const struct command *cmd);
int check_redirection_file(struct command *cmd);
void error_message(int error_code);

/*************************************************************
 *                    LOCAL FUNCTION DEFINITIONS             *
 *************************************************************/

/*
 * This function inserts the command at the end of the job list
 * @param - {command **} - the root of the job 
 * 	  - {command *} - the command to be inserted
 * @return - none
 */
void insert_command(struct command **root, struct command *cmd) {
    /* the job list is empty */
    if(*root == NULL) {
        *root = cmd;
    } else {
        struct command *node = *root;
        /* this will find last node of of the job list */
        while(node->next_command) {
            node = node->next_command;
        }
        /* insert the command to the end */
        node->next_command = cmd;
    }
    return;
}

/*
 * This function checks if the command has the same id, if so add status to it
 * @param - {pid_t} - the id to find 
 *        - {int} - the exit status of that pid
 * @return - none
 */
void insert_status(struct command **root, pid_t pid, int status) {
    /* the job list is empty */
    if(*root == NULL) {
        return;
    } else {
	struct command *node = *root;
        /* this will find node tha has the pid */
        while(node->next_command && node->pid != pid) {
            node = node->next_command;
        }
        /* insert the status to the node */
        node->status = status;
    } 
    return;
}

/*
 * This function reads the whole command line from terminal and store each command as a linked list
 * @param - {job *} - the job to store one or more commands (pipeline) 
 * @return - none
 */
void read_job(struct job *job) {
    char commands[MAX_CMD];
    char *token;
    struct command *cmd;
  
    job->num_processes = 1;			/* initialize number of processes */
    fgets(commands, MAX_CMD, stdin);		/* get the entire command line */
    commands[strlen(commands) - 1] = 0;		/* get rid of newline */
    strcpy(job->commandline, commands);		/* store the whole command line */
    
    /* find number of processes */
    int i;
    for(i = 0; i < strlen(commands); i++) {
        if(commands[i] == '|')
	    job->num_processes++;
    }
    
    token = strtok(commands, "|");		/* get the first command */
    if(token == NULL) {		    		/* no command */
	job->first_command = NULL;
	return;
    }
   
    while(token != NULL) {			/* one command or more */
	cmd = read_command(token);
	insert_command(&job->first_command, cmd);
	token = strtok(NULL, "|");
	cmd = cmd->next_command;	
    }
    return;
}	

/*
 * This function parses the command and store it as a command struct
 * @param - {char *} - the command to be parsed
 * @return - {command *} - the command struct
 */
struct command* read_command(char *command) {
    char arg[MAX_CMD];
    int num_white_space;
    
    struct command* cmd = (struct command*) malloc(sizeof(struct command));     /* allocate space for comamnd */
    cmd->num_args = 0;				/* initialize number of arguments */
    cmd->num_input = 0;				/* initialize number of input redirections */
    cmd->num_output = 0;			/* initialize number of output redirections */
    cmd->next_command = NULL;			/* next command initializes to null */
  
    /* get rid of leading spaces */
    for(num_white_space = 0; command[num_white_space] == ' '; num_white_space++);
    command = command + num_white_space;

    /* get rid of trailing spaces */
    int i;
    for(i = strlen(command) - 1; i >= 0 && command[i] == ' '; i--)
        command[i] = 0;
    
    strcpy(cmd->command, command);              /* store command line */

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
    return cmd;
}

/*
 * This function pipelines the commands: connects the old's reading stream and creates new pipe for next process
 * @param - {command *} - the pipeline commands
 * 	  - {int [2]} - old pipe
 *
 * @return - none
 */
void pipeline(struct command *cmd, int fd[2]) {
    int builtin_command_code, status;
    int new_fd[2];
    pid_t pid;
	
    /* creates new pipe */
    pipe(new_fd);

    /* check if it is a built in command */
    builtin_command_code = is_builtin_command(cmd);

    /* check if it is builtin command */
    if(builtin_command_code == EXIT) {          /* leave the shell */
	fprintf(stderr, "Bye...\n");
	exit(EXIT_SUCCESS);
    }
    else if(builtin_command_code == CD) {       /* run cd command */
	status = cd(cmd->args[1]);

    } else if(builtin_command_code == PWD) {    /* run pwd command */
	status = pwd();
    }

    pid = fork();
    cmd->pid = pid;
    /* last command of the pipeline */
    if(pid == 0) {
	/* child */
        dup2(fd[0], STDIN_FILENO);	/* replace new read stream with old read stream */ 
	close(fd[0]);			/* close unnecessary files */

        if(builtin_command_code == NOT_BUILTIN) {       /* not builtin command */
    	    close(new_fd[0]);				/* closing unnecessary files */
	    if(cmd->next_command != NULL) {
                dup2(new_fd[1], STDOUT_FILENO);
	    } 
	    close(new_fd[1]);				/* closing unnecessary files */
	    
	    /* perform redirections */
            redirection(cmd);

	    execvp(cmd->args[0], cmd->args);
            /* execvp error */
            error_message(ERR_CMD_NOTFOUND);
            exit(EXIT_FAILURE);
        } else {                                        /* builtin command */
            close(new_fd[0]);
	    close(new_fd[1]);
            exit(status);
        }
    } else if(pid > 0) {
	/* parent */
	close(fd[0]);				     /* closing unnecessary files */
	close(new_fd[1]);			     /* closing unnecessary files */
	if(cmd->next_command) {
	    pipeline(cmd->next_command, new_fd);
	} else {
            close(new_fd[0]);
	}
    }
}

/*
 * This function frees the memory allocated for the job struct
 * @param - {job *} - the job struct
 * @return - none
 */
void free_job(struct job *job) {
    struct command *node;
    struct command *head = job->first_command;	/* initializes the head of the job list */

    while(head != NULL) {
    	node = head;				/* get the current node at the top of the job list */
	head = head->next_command;		/* iterate to next node of the job list */
	free_command(node);			/* free the command */
	free(node);				/* free the node */
    }
    free(job);
    return;
}

/*
 * This function frees the memory allocated for the arguments in the command line struct
 * @param - {command *} - the command line struct
 * @return - none
 */
void free_command(struct command *cmd) {
    int i;
    for(i = 0; i < (cmd->num_args); i++) {
        free(cmd->args[i]);			/* free the allocated memory for each argument */
    }
    return;
}

/*
 * This function checks if the command line is empty store
 * @param - {char *} - the command line
 * @return - {int} - one for empty, zero for not empty
 */
int is_empty_command(char *cmd) {
    int num_white_space;
    for(num_white_space = 0; cmd[num_white_space] == ' '; num_white_space++);
    return cmd[num_white_space] == 0 ? 1 : 0;
}

/*
 * This function check if the command line is valid
 * @param - {command *} - the command struct
 * @return - {int} - error code
 */
int is_valid_command(struct command *cmd) {
    if(cmd == NULL || cmd->command[0] == '<' || cmd->command[0] == 0 ||
        cmd->command[0] == '>' || cmd->command[0] == '|') {
        return ERR_INVALID_CMDLINE;
    }
    return SUCCESS;
}

/*
 * This function check if the job is valid
 * @param - {job *} - the job struct
 * @return - {int} - error code
 */
int check_job(struct job *job) {
    struct command *cmd = job->first_command;
    int i, error_code;

    if(job->commandline[0] == '|')
        return ERR_INVALID_CMDLINE;

    for(i = 0; i < job->num_processes; i++) {
	/* check valid command error */
	error_code = is_valid_command(cmd);
	if(error_code != SUCCESS)
	    return error_code;

	/* check redirections files error */
        error_code = check_redirection_file(cmd);
	if(error_code != SUCCESS)
            return error_code;

	/* check mislocated redirection errors */
	if(i != 0 && cmd->num_input > 0)
	    return ERR_INPUT_MISLOCATED;
	if(i != job->num_processes - 1 && cmd->num_output > 0)
	    return ERR_OUTPUT_MISLOCATED;

	/* get next command */
	cmd = cmd->next_command;
    }
    return SUCCESS;
}

/*
 * This function check if the command line is a builtin command
 * @param - {command *} - the command line struct
 * @return - {int} - builtin command enum
 */
int is_builtin_command(const struct command *cmd) {
    if(strcmp(cmd->args[0], "exit") == 0) {		/* exit */
        return EXIT;
    } else if(strcmp(cmd->args[0], "cd") == 0) {	/* cd */
        return CD;
    } else if(strcmp(cmd->args[0], "pwd") == 0) {	/* pwd */
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
 * @param - {command *} - the command struct that contains the files info
 * @return - none
 */
void redirection(const struct command *cmd) {
    int fd, i;

    /* input redirection */
    for(i = 0; i < cmd->num_input; i++) {			
        fd = open(cmd->input_file[i], O_RDONLY);
        dup2(fd, STDIN_FILENO);
	close(fd);				 /* close unused file */
    }

    /* output redirection */
    for(i = 0; i < cmd->num_output; i++) {	
	fd = open(cmd->output_file[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); /* create the file if not exist */
	dup2(fd, STDOUT_FILENO);
        close(fd);				/* close unused file */
    }
    return;
}

/*
 * This function checks if the input/output file can be opened and if it is given
 * @param - {command *} - the command struct that contains the files info
 * @return - error code
 */
int check_redirection_file(struct command *cmd) {
    int i;
    /* check input files */
    for(i = 0; i < cmd->num_input; i++) {
        /* check if the file name is given */
	if(strcmp(cmd->input_file[i], " ") == 0) {
            return ERR_NO_INPUTFILE;
	}
	
	/* check if the file can be opened */
	if(open(cmd->input_file[i], O_RDONLY) < 0) {  	/* error opening file for reading */
            return ERR_OPEN_INPUTFILE;
	} 
     }    
	    
    /* check output files */
    for(i = 0; i < cmd->num_output; i++) {
        /* check if the file name is given */
	if(strcmp(cmd->output_file[i], " ") == 0) {
            return ERR_NO_OUTPUTFILE;
        }

        /* file exists but file doesn't allow access */
	if(access(cmd->output_file[i], F_OK) == 0 && access(cmd->output_file[i], W_OK) < 0) {
	    return ERR_OPEN_OUTPUTFILE;		/* error opening file for writing */
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
    int status_array[MAX_PROCESS];
    int status;
    struct command *cmd;
    struct job *job;

    while(1) {
	memset(status_array, 0, MAX_PROCESS * sizeof(int));			/* clear previous status arrays */
        job = (struct job*) malloc(sizeof(struct job)); /* allocate space for job struct */
	int builtin_command_code, error_code;

	printf("sshell$ ");		/* Display prompt */
	read_job(job);			/* read the job */
	cmd = job->first_command;	/* initializes first command */
	
	/* no command is entered */
	if(is_empty_command(job->commandline)) {
    	    continue;
	} 

	/* check if input/output redirection has errors */
        error_code = check_job(job);
        if(error_code != SUCCESS) {
            error_message(error_code);
            continue;
    	}

	builtin_command_code = is_builtin_command(cmd);
        if(builtin_command_code == EXIT) {
            fprintf(stderr, "Bye...\n");
            exit(EXIT_SUCCESS);
        }
        else if(builtin_command_code == CD) {
            status = cd(cmd->args[1]);

        } else if(builtin_command_code == PWD) {    /* run pwd command */
            status = pwd();
        }
	
        if(cmd->next_command == NULL) {		/* only one command */	
	    /* run not-built-in command */
	    pid = fork();			/* fork child process */
	    if(pid == 0) {			/* child */
		/* perform redirections */
		redirection(cmd);
		
		if(builtin_command_code == NOT_BUILTIN) {	/* not builtin command */
		    execvp(cmd->args[0], cmd->args);		
		    /* execvp error */
		    error_message(ERR_CMD_NOTFOUND);
		    exit(EXIT_FAILURE);
		} else {					/* builtin command */
		    exit(status);
		}
	    } else if(pid > 0) {		/* parent */
		wait(&status);		/* wait for child to complete */
		/* Information message after execution */
		fprintf(stderr, "+ completed '%s' [%d]\n", job->commandline, WEXITSTATUS(status));	
	    } else {				/* fork error */ 
		perror("fork");	
		exit(EXIT_FAILURE);
	    }
	} else {				/* pipeline of commands */
	    int fd[2];
            pipe(fd);                           /* create pipe */
            
	    pid = fork();
	    cmd->pid = pid;
            if(pid == 0) {					/* child */    
                close(fd[0]);
		if(builtin_command_code == NOT_BUILTIN) {       /* not builtin command */
                    dup2(fd[1], STDOUT_FILENO);
		    close(fd[1]);
		    
		    /* perform redirections */
                    redirection(cmd);
		    
		    execvp(cmd->args[0], cmd->args);
        
		    /* execvp error */
                    error_message(ERR_CMD_NOTFOUND);
                    exit(EXIT_FAILURE);
                } else {                                        /* builtin command */
		    close(fd[1]);
                    exit(status);
                }		
	    } else if(pid > 0) {				/* parent */
		close(fd[1]);					/* close out unnessary files */
		pipeline(cmd->next_command, fd);		/* pipeline the commands */
		//wait(status_array);

		int i;
		pid_t id;
		/* wait for any child processes */
		for(i = 0; i < job->num_processes; i++) {
		    id = wait(&status);
		    insert_status(&(job->first_command), id, status);  
		}

		/* Information message after execution */
		fprintf(stderr, "+ completed '%s' ", job->commandline);
		for(i = 0; i < job->num_processes; i++) {
		    fprintf(stderr, "[%d]", WEXITSTATUS(cmd->status));
		    cmd = cmd->next_command;
		}	
		fprintf(stderr, "\n");
	    } else {						/* fork error */
                perror("fork");
                exit(EXIT_FAILURE);
	    }
	}
    }
    return EXIT_SUCCESS;
}
