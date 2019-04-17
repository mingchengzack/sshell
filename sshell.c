#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/wait.h>
#include <fcntl.h>

/*************************************************************
 *                    MACRO DEFINITIONS                      *
 ************************************************************/

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

/* finish code */
enum {
    NOT_FINISHED,
    FINISHED
};

/* error code enum */
enum {
    SUCCESS,
    FAILURE,
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
    char *input_file[MAX_ARGS];			/* the array of the input files */
    char *output_file[MAX_ARGS];		/* the array of the output files */
    int num_args;				/* number of arguments in the command line */
    struct command *next_command;		/* the comamnd for pipeling */
    int finish;					/* finish flag */
    int background;				/* number of background signs */
};

/* job struct */
struct job {
    char commandline[MAX_CMD];			/* the total command line */
    struct command *first_command;		/* the first command of a job */
    int num_processes;				/* the number of commands/processes */
    struct job *next_job;			/* the previous job that was running in background */
    int finish;					/* finish flag */
};

/* job list struct */
struct job_list {
     struct job *first_job;			/* the first job of the job list */
};

/*************************************************************
 *                    LOCAL FUNCTION PROTOTYPES              *
 *************************************************************/
void insert_job(struct job **root, struct job *job);
void free_job_list(struct job_list *job_list);
void delete_job(struct job **root, struct job *job);
struct command* find_last_command(struct command *cmd);
int check_finish_job(struct job* job);
void insert_command(struct command **root, struct command *cmd);
void insert_status(struct job *job, pid_t pid, int status);
struct job *read_job();
struct command* read_command(char *command);
void pipeline(struct command *cmd, int fd[2], struct job* first_job);
void free_job(struct job *job);
void free_command(struct command *cmd);
int is_empty_command(char *cmd);
int is_valid_command(struct command *cmd);
int check_redirection_file(char *file, int mode);
int check_command(struct command *cmd, int num_processes, int index);
int check_input_output(struct command *cmd, int num_processes, int index);
int check_background(struct command *cmd, int num_processes, int index);
int check_job(struct job *job);
int is_builtin_command(const struct command *cmd);
int cd(const char *dir);
int pwd();
void redirection(const struct command *cmd);
void check_background_process(struct job *job_start, struct job *job_end);
void error_message(int error_code);
void process_complete_message(struct job **first_job);

/*************************************************************
 *                    LOCAL FUNCTION DEFINITIONS             *
 *************************************************************/

/*
 * This function inserts the job at the end of the job list
 * @param - {job **} - the root of the job list 
 *        - {job *} - the job to be inserted
 * @return - none
 */
void insert_job(struct job **root, struct job *job) {
    /* the job is empty */
    if(*root == NULL) {
        *root = job;
    } else {
        struct job *node = *root;
        /* this will find last node of of the job list */
        while(node->next_job) {
            node = node->next_job;
        }
        /* insert the command to the end */
        node->next_job = job;
    }
    return;
}

/*
 * This function deletes the job list
 * @param - {job_list *} - the job list
 * @return - none
 */
void free_job_list(struct job_list* job_list) {
    struct job *node;
    struct job *head = job_list->first_job;  	/* initializes the head of the job list */

    while(head != NULL) {
        node = head;                            /* get the current node at the top of the job list */
        head = head->next_job;                  /* iterate to next node of the job list */
        free_job(node); 		        /* free the job */
    }
    free(job_list);
}

/*
 * This function deletes node in the job list
 * @param - {job **} - the root of the job list
 *        - {job *} - the job to be deleted
 * @return - none
 */
void delete_job(struct job **root, struct job *job) {
    /* store the root node */
    struct job *node = *root;
    struct job *prev;

    /* find the targeted pid */
    struct command *last_command = find_last_command(job->first_command);
    pid_t key_id = last_command->pid;

    last_command = find_last_command(node->first_command);
    /* remove the head */
    if(node != NULL && last_command->pid == key_id) {
        *root = node->next_job;		/* change head */
	free_job(node);
	return;
    }

    /* search for the pid */
    while(node != NULL && last_command->pid != key_id) {
        prev = node;
	node = node->next_job;
	last_command = find_last_command(node->first_command);
    }

    /* cant find it */
    if(node == NULL)  {
        return;
    }

    /* delete the node from the job list */
    prev->next_job = node->next_job;
    free_job(node);
}

/*
 * This function find the last command in the job
 * @param - {command *} - the command
 * @return - {command *}
 */
struct command* find_last_command(struct command *cmd) {
    /* empty job */
    struct command *node = cmd;
    if(node == NULL) {
        return NULL;
    }
    
    while(node->next_command) {
        node = node->next_command;
    }
    return node;
}

/*
 * This function checks if the job is finished
 * @param - {job *} - the job
 * @return - one for finish, zero not finish
 */
int check_finish_job(struct job* job) {
    struct command *cmd = job->first_command;
    while(cmd) {
        if(cmd->finish == 0) {	/* one command is not finished */
	    return NOT_FINISHED;
	}
	cmd = cmd->next_command;
    }
    return FINISHED;
}

/*
 * This function inserts the command at the end of the job list
 * @param - {command **} - the head of the job list
 * 	  - {command *} - the command to be inserted
 * @return - none
 */
void insert_command(struct command **root, struct command *cmd) {
    /* the job is empty */
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
 * @param - {job *} - the job list
 *        - {pid_t} - the id to find 
 *        - {int} - the exit status of that pid
 * @return - none
 */
void insert_status(struct job *job, pid_t pid, int status) {
    /* the job list is empty */
    if(job == NULL) {
        return;
    } else {
	struct job *job_node = job;
	while(job_node) {
	    struct command *cmd_node = job_node->first_command;
            /* this will find node tha has the pid */
            while(cmd_node && cmd_node->pid != pid) {
                cmd_node = cmd_node->next_command;
            }
            /* found it and insert the status to the node */
	    if(cmd_node) {
                cmd_node->status = status;
	        cmd_node->finish = FINISHED;
	        return;
	    } 
	    job_node = job_node->next_job;
	}
    } 
    return;
}

/*
 * This function reads the whole command line from terminal and store each command as a linked list
 * @param - none
 * @return - {job *} - the stored job
 */
struct job* read_job() {
    char commands[MAX_CMD];
    char *token, *nl;
    struct command *cmd;
  
    struct job *job = (struct job*) malloc(sizeof(struct job)); /* allocate space for job struct */
    job->num_processes = 1;			/* initialize number of processes */
    job->next_job = NULL;			/* initialize next job to NULL */
    job->finish = NOT_FINISHED;			/* initializes not finish */
    job->first_command = NULL;			/* initialize first job to NULL */

    /* get the entire command line */
    if(fgets(commands, MAX_CMD, stdin) == NULL) {	/* in case we reach EOF */
        strcpy(commands, "exit\n");
    }

    /*
     * Echoes command line to stdout if fgets read from a file and not
     * the terminal (which is the case with the test script)
     */
    if(!isatty(STDIN_FILENO)) {
        printf("%s", commands);
        fflush(stdout);
    }
    
    /* Remove trailing newline from command line */
    nl = strchr(commands, '\n');
    if (nl) {
       *nl = '\0';
    }
    
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
	return job;
    }
   
    while(token != NULL) {			/* one command or more */
	cmd = read_command(token);
	insert_command(&(job->first_command), cmd);
	token = strtok(NULL, "|");
	cmd = cmd->next_command;	
    }
    return job;
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
    cmd->background = 0;			/* initialize number of background signs */
    cmd->next_command = NULL;			/* next command initializes to null */
    cmd->finish = NOT_FINISHED;			/* initialize not finish command */

    /* get rid of leading spaces */
    for(num_white_space = 0; command[num_white_space] == ' '; num_white_space++);
    command = command + num_white_space;

    /* get rid of trailing spaces */
    int i;
    for(i = strlen(command) - 1; i >= 0 && command[i] == ' '; i--)
        command[i] = 0;
    
    strcpy(cmd->command, command);              /* store command line */

    /* parse the command manually */
    int read_code = ARGUMENT;
    i = 0;
    while(i < strlen(command)) {
        memset(arg, 0, MAX_CMD);
	int j = 0;

	/* get the argument */
        while(command[i] != ' ' && command[i] != '<' && command[i] != '>' 
		&& command[i] != 0 && command[i] != '&') {
	    arg[j++] = command[i++];
	}
	arg[j] = 0;				/* add null terminator */

        for(; command[i] == ' '; i++);		/* get rid of leading spaces */
      
	switch(read_code) {
            case ARGUMENT:	/* an argument for the program */
	        cmd->args[cmd->num_args] = (char *) malloc(strlen(arg) + 1);
         	strcpy(cmd->args[cmd->num_args++], arg);
		break;
	    case INPUT:		/* input file */
		if(arg[0] == 0) {
		    cmd->input_file[cmd->num_input++] = NULL;
	        } else {
		    cmd->input_file[cmd->num_input] = (char *) malloc(strlen(arg) + 1);
		    strcpy(cmd->input_file[cmd->num_input++], arg);
                }
		read_code = ARGUMENT;
	        break;
	    case OUTPUT:	/* output file */
		if(arg[0] == 0) {
	            cmd->output_file[cmd->num_output++] = NULL;
                } else {
		    cmd->output_file[cmd->num_output] = (char *) malloc(strlen(arg) + 1);
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
        } else if(command[i] == '&') {
            cmd->background++;
   	    i++;
   	    for(; command[i] == ' '; i++);      /* get rid of leading spaces */
	}
    }
    
    /* check given files */
    if(read_code == INPUT) {
        cmd->input_file[cmd->num_input++] = NULL;   /* use null to indicate not given file */
    } else if (read_code == OUTPUT) {
    	cmd->output_file[cmd->num_output++] = NULL; /* use null to indicate not given file */	
    }
    
    cmd->args[cmd->num_args] = NULL;		    /* set null terminator */
    return cmd;
}

/*
 * This function pipelines the commands: connects the old's reading stream and creates new pipe for next process
 * @param - {command *} - the pipeline commands
 * 	  - {int [2]} - old pipe
 *	  - {job *} - the first job: to check if there is any active job
 * @return - none
 */
void pipeline(struct command *cmd, int fd[2], struct job* first_job) {
    int builtin_command_code, status;
    int new_fd[2];
    pid_t pid;
	
    /* creates new pipe */
    pipe(new_fd);

    /* check if it is a built in command */
    builtin_command_code = is_builtin_command(cmd);

    /* check if it is builtin command */
    if(builtin_command_code == EXIT) {          /* leave the shell */
	if(first_job->next_job != NULL) {           /* try to exit while there are active jobs */
            status = EXIT_FAILURE;
            error_message(ERR_ACTIVE_JOBS);
        } else {                                    /* can exit */
            fprintf(stderr, "Bye...\n");
            exit(EXIT_SUCCESS);
        }
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
	    pipeline(cmd->next_command, new_fd, first_job);
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
    /* free args space */
    for(i = 0; i < (cmd->num_args); i++) {
       if(cmd->args[i]) { 
      	   free(cmd->args[i]);			/* free the allocated memory for each argument */
       }
    }

    /* free input file space */
    for(i = 0; i < (cmd->num_input); i++) {
       if(cmd->input_file[i]) {
	   free(cmd->input_file[i]);            /* free the allocated memory for each file */
       }
    }

    /* free output file space */
    for(i = 0; i < (cmd->num_output); i++) {
       if(cmd->output_file[i]) {
           free(cmd->output_file[i]);		/* free the allocated memory for each file */
       }
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
        cmd->command[0] == '>' || cmd->command[0] == '|' || cmd->command[0] == '&') {
        return ERR_INVALID_CMDLINE;
    }
    return SUCCESS;
}

/*
 * This function checks if the input/output file can be opened and if it is given
 * @param - {char *} - the name of the file
 *        - {int} - check for input or output
 * @return - error code
 */
int check_redirection_file(char *file, int mode) {
    int fd;
    /* check input files */
    if(mode == INPUT) {
        /* check if the file name is given */
        if(file == NULL) {
            return ERR_NO_INPUTFILE;
        }

        /* check if the file can be opened */
        fd = open(file, O_RDONLY);
	if(fd < 0) {    /* error opening file for reading */
            return ERR_OPEN_INPUTFILE;
        }
	close(fd);
     } else {
        /* check if the file name is given */
        if(strcmp(file, " ") == 0) {
            return ERR_NO_OUTPUTFILE;
        }

        /* file exists but file doesn't allow access */
        fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd < 0) {
            return ERR_OPEN_OUTPUTFILE;         /* error opening file for writing */
        }
	close(fd);
    }
    return SUCCESS;
}

/*
 * This function check if the command has redirection file errors or 
 * 	mislocated input or output or background sign
 * @param - {command *} - the command struct
 *        - {int} - number of total process
 *        - {int} - the index of the command in the job list
 * @return - {int} - error code
 */
int check_command(struct command *cmd, int num_processes, int index) {
    int i;
    int input_index = 0, output_index = 0;
    int error_code;

    for(i = 0; i < strlen(cmd->command); i++) {
	if(cmd->command[i] == '<') {		/* check for input file errors */
	    if(index != 0) {			/* check for input mislocation */
	        return ERR_INPUT_MISLOCATED;
	    }
	    error_code = check_redirection_file(cmd->input_file[input_index++], INPUT);
	    if(error_code != SUCCESS) {							/* check input redirection */
               return error_code;
	   }
	} else if(cmd->command[i] == '>') {	/* check for output file errors */
	    error_code = check_redirection_file(cmd->output_file[output_index++], OUTPUT);/* check output redirection */
            if(error_code != SUCCESS) {
                return error_code;
	    }
	} else if(cmd->command[i] == '&') {	/* check for background error */
            if(index != num_processes - 1 || i != strlen(cmd->command) - 1) {	
	        /* the background can only be the end of the last command */
		return ERR_BACKGROUND_MISLOCATED;
	    }
	}
    }
    if(output_index > 0 && index != num_processes - 1) {    /* check for output mislocation */
        return ERR_OUTPUT_MISLOCATED;
    }
    return SUCCESS;
}

/*
 * This function check if the job has any error before executing
 * @param - {job *} - the job struct
 * @return - {int} - error code
 */
int check_job(struct job *job) {
    struct command *cmd = job->first_command;
    int i, error_code;

    for(i = 0; i < job->num_processes; i++) {
	/* check valid command error */
	error_code = is_valid_command(cmd);
	if(error_code != SUCCESS)
	    return error_code;
	
	/* check mislocated and redirection errors */
        error_code = check_command(cmd, job->num_processes, i);
	if(error_code != SUCCESS)
            return error_code;
	
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
 * This function checks the background processes and adds completed status if completed 
 * @param - {job *} - the start of the job list
 * 	  - {job *} - the end of the background processes
 * @return - none
 */
void check_background_process(struct job *job_start, struct job *job_end) {
    pid_t pid;
    int i, status;
    struct command *cmd;
    struct job *job = job_start;

    /* check background processes */
    while(job != job_end) {
	cmd = job->first_command;
	/* check each sub processes */
	for(i = 0; i < job->num_processes; i++) {
	    pid = waitpid(cmd->pid, &status, WNOHANG);	/* check if that subprocess has completed */
	    if(pid != NOT_FINISHED) {                    	/* a process has finished */
		insert_status(job_start, pid, status);
	    }
	    cmd = cmd->next_command;
	}
        job->finish = check_finish_job(job);
        job = job->next_job;
    }
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
 * This function prints out any completed process info
 * @param - {job *} - the job list
 * @return - none
 */
void process_complete_message(struct job **first_job) {
    /* Information message after execution */
    int i;
    struct job *job_node = *first_job;
    while(job_node) {
        if(job_node->finish) {
            fprintf(stderr, "+ completed '%s' ", job_node->commandline);
	    struct command *cmd_node = job_node->first_command;
	    for(i = 0; i < job_node->num_processes; i++) {
		fprintf(stderr, "[%d]", WEXITSTATUS(cmd_node->status));
		cmd_node = cmd_node->next_command;
	    }
	    fprintf(stderr, "\n");
	    struct job *copy = job_node;	/* copy it for deletion */
	    job_node = job_node->next_job;	/* go to the next job */
	    delete_job(first_job, copy);	/* delete the job if it is finished */
	} else {
	    job_node = job_node->next_job;	/* go to the next job */
	}
    }
}

/*
 * main function of the sshell
 */
int main(int argc, char *argv[]) {	
    pid_t pid;
    int status;
    struct command *cmd;
    struct job_list *job_list = (struct job_list*) malloc(sizeof(struct job_list));
    job_list->first_job = NULL;		/* initialize first job to NULL */

    while(1) {
    	int fd[2];
	int builtin_command_code, error_code;
	struct job *job;
	printf("sshell$ ");		/* Display prompt */
	
	job = read_job();		/* read the job */
	cmd = job->first_command;	/* initializes first command */
	
	/* no command is entered */
	if(is_empty_command(job->commandline)) {
	    /* check background processes to see if they are completed */
	    check_background_process(job_list->first_job, NULL);

	    /* print out completed process message */
            process_complete_message(&(job_list->first_job));
	    
	    /* clear out allocated space for current job(which is empty) */
	    free_job(job);
	    continue;
	} 

	/* check if input/output redirection has errors */
	error_code = check_job(job);
        if(error_code != SUCCESS) {
  	    /* prints out error message */
            error_message(error_code);

	    /* clear out allocated space for current job */
	    free_job(job);
	    continue;
	}
	
	insert_job(&(job_list->first_job), job);			/* insert the job to the job list */
	
	builtin_command_code = is_builtin_command(cmd);
        if(builtin_command_code == EXIT) {		/* run exit */
           if(job_list->first_job->next_job != NULL) {		/* try to exit while there are active jobs */
		status = EXIT_FAILURE;
		error_message(ERR_ACTIVE_JOBS);
	   } else {					/* can exit */
		fprintf(stderr, "Bye...\n");
		free_job_list(job_list);
	     	break;
	    }
        }
        else if(builtin_command_code == CD) {		/* run cd comamnd */
            status = cd(cmd->args[1]);
	    if(job->num_processes == 1) {
                cmd->background = 0;
	    }
        } else if(builtin_command_code == PWD) {    	/* run pwd command */
            status = pwd(); 
	    if(job->num_processes == 1) {
                cmd->background = 0;
            }
        }
	     
	if(cmd->next_command != NULL) {   		/* pipelineing */
    	    pipe(fd);
	}  
	
	/* run not-built-in command */
	pid = fork();					/* fork child process */
	cmd->pid = pid;					/* save pid */
	if(pid == 0) {					/* child */
	    if(cmd->next_command != NULL) {     	/* pipelineing */
		close(fd[0]);				/* close out unnessary files */
	    }

	    if(builtin_command_code == NOT_BUILTIN) {	/* not builtin command */
		if(cmd->next_command != NULL) {		/* pipelineing */
		    dup2(fd[1], STDOUT_FILENO);		/* close out unnessary files */
		    close(fd[1]);
		}	

		/* perform redirections */
		redirection(cmd);
	        
		execvp(cmd->args[0], cmd->args);		
		/* execvp error */
		error_message(ERR_CMD_NOTFOUND);
		exit(EXIT_FAILURE);
	    } else {					/* builtin command */
	        /* perform redirections */
                redirection(cmd);

	        if(cmd->next_command != NULL) {		/* pipelineing */
		    close(fd[1]);			/* close out unnessary files */
		}
		exit(status);
	    }
	} else if(pid > 0) {		/* parent */
	    struct command *last_command = find_last_command(cmd);
	
	    if(cmd->next_command != NULL) {			/* pipelineing */
		close(fd[1]);					/* close out unnessary files */
		pipeline(cmd->next_command, fd, job_list->first_job);	/* pipeline the commands */
	    } 

	    /* waiting */
            if(last_command->background == 0) {
   		/* wait for any child processes */
		while(job->finish != FINISHED) {
		    pid = waitpid(WAIT_ANY, &status, WUNTRACED);	/* wait for any child process */
		    insert_status(job_list->first_job, pid, status);
		    job->finish = check_finish_job(job);
		}
            }

	    /* check background processes to see if they are completed */
	    check_background_process(job_list->first_job, job);

	    /* print out completed process message */
	    process_complete_message(&(job_list->first_job));
	} else {				/* fork error */ 
	    perror("fork");	
	    exit(EXIT_FAILURE);
	}
    }
    return EXIT_SUCCESS;
}
