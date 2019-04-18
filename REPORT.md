# Program 1 Report: sshell
  Authors: Ming Cheng, Jiayi Xu  
  This report has the following sections
  * Overview 
  * Implementation
    * Data Structures
    * Parsing
    * Error Checking  
    * Built-in Commands
    * Input and Output Redirections
    * Pipeline
    * Background
  * Testing
  * Resources
  
# Overview
  The basic flow of my program is that when the user enters a command, it will parse the command and store it in a job list
  data structure we create. Then we will determine if it is either an empty command or invalid command. If not we go to the 
  next round of entering commands. If it is valid, we will check if it is built-in command. If it is a built-in command, we
  will run the command in the parent proces. Then we will fork a process. In that child process, if it is a built-in command,
  we just normally exit the child and if not we run the command. In the parent process after forking, we check if it is 
  a pipeline command, and run pipeline if it is. Then we will check if we need to wait for the job to complete (a command or a 
  pipeline of commands), and store the exit status to corresponding commands. Then we will check if there are any background
  processes completed, if so we get the exit status for them and mark it as finished. Then at last, we print out the completed
  message for the process if it is marked finished. Then we repeat the above process until the user enters exit.
# Implementation
  ## Data Structures
  We used two linked lists for storing the jobs and commands. We have a struct called job_list which is a linked list of jobs.
  A job is a linked list of commands. The command contains the process id, the command line, the command arguments, input 
  files if any, output files if any, finish flag for completion indication, exit status, number of input files, number of
  output files, number of arguments, job also contains a flag called finish to indicate if the job is finished, and number of
  background flags. The job contains the whole command line and finish flag to indicate if all commands are finished.
  ## Parsing
  We have two functions to take care of parsing: read_job() and read_command(). After fgets, we store the whole command line 
  to the job, and parse through the command to find the number of processes by finding how many '|' are there (for error 
  checking). Then we use strtok() to seperate the command line into each command as a token where we use read_command() to
  manually parse that token.   
  
  In read_command(), we first get rid of any leading and trailing spaces and tabs. Then we manually
  parse through it. If we see a space, '<', '>' or '&', we stop and handle the string before that terminator. If it's a space,
  we read that string as an argument for the command. If it is a '<' or '>', we set the flag that reminds us to read the input
  file or output file for the next terminator. If it is a '&', we simply increment the background flag for that command. We
  continue this process until reaches the end of the command. If the flag for reading input or output is still set, that means
  the input file or output file is not given. We basically set input file or output_file as NULL for later error checking. We
  repeat read_command() until strtok cannot find any more commands to read.
  ## Error Checking
  For all the pre-run errors, we check the errors of a job that is already stored from parsing by checking one command at time
  for that job by parsing that command. If we encounter '<', we check for input mislocation and no input and cannot open input 
  errors. If we encounter '>', we check for no output file and cannot open file errors. If we encounter a '&', we check for
  background mislocation error. At the end, we check for output mislocation error. If we have a error, we simply return before
  running it and report it. For the run error (command not found, directory not command, active jobs still running), we find
  out them after running the command and report it.  
  
  **Is Empty Command**: This is not an error but we still check it before the actual error checking. We simply check if it is   a null terminator, '\0' after getting rid of the leading spaces.  
  
  **Invalid Command Line**: For thie error, we check if the command we store is NULL (it happends when there are less commands
  than we expect them to be), or is starting with '<', '>', '\0', '&', or '|'.
  
  **Command Not Found**: This errors happends when execvp fails, we simply prints out the message after that.
  
  **Directory Not Found**: This errors happends when cd fails (returns -1 from chdir), we prints out message afterthat.
  
  **No Output/Input File**: We check these two errors by checking if the stored file array in command struct is NULL(mentioned
  in parsing).
  
  **Cannot Open Input/Output File**: We check theses two errors by using open() and check if open() returns -1.
  
  **Input/Output Mislocated**: When we go through the commands in the job, we have the index, we simply checks if the index is
  the first one for input and if it is the last one for output.
  
  **Background Mislocated**: When we go through the commands in the job, we have the index, we simply checks if the index is 
  the last one and if that background sign is the last character of that command string.
  
  **Active Jobs still Running**: We check this error when the user tries to exit. We checks if the job list is empty except 
  for that exit job.
  ## Built-in Commands
  **CD**: We use chdir() for this function.  
  **PWD**: We use getcwd for this function.  
  **EXIT**: We simply exit the program if there is no active jobs.  
  ## Input and Output Redirections
  **Input Redirection**: For input redirection, we replaces STDIN_FILENO with that file using dup2. Since there can be more
  than one input files, we loop the saved input files and replaces STDIN_FILENO.
  
  **Output Redirection**: Same as above, but we replaces STDOUT_FILENO instead.  
  ## Pipeline
  For checking if we have pipeline commands, I simply check if that job has more commands.  
  
  For doing pipeline, we first creat a pipe in the main function. At the child process, we use dup2 to replace STDOUT_FILENO
  with the writing portion of the pipe and run that process, then in the parent process we pass the reading portion of the
  pipe and the next process to the recursive function pipeline(). In pipeline() function, we create a new pipe if there are
  commands to pipe after this process, and creat a new child process for this process (which is the next process of the
  previous one before calling pipeline()) and connect the old reading portion of the pipe to STDIN_FILENO (which should have 
  the input from last process). Then we connect the writing portion of the new pipe to STDOUT_FILENO in the child process if
  there is more command to pipe. Then in the parent process, we continue to call pipeline() and pass the new pipe if there are
  more commands to pipe. We continue this process until we reach the end of the pipe line. Then the output will just print out
  to STDOUT or files we redirect.
  
  For getting the status of the exited child process. We wait for any process after the pipeline until the job is finished.
  (as mentioned in data structure, the job is finished when all the sub processes are finished). When we get a completed
  child process, and get his id and status, we use that id to find that command in the job and set the status and the finish
  flag. 
  ## Background
  For checking if we have background, we check if the last command of the pipeline (or only one command) has the background
  flag set.  
   
  For doing background, we don't wait for the job to finish after doing pipeline. We then go through the job list for 
  background process (not including the one that just added) to see if any of the process is finished using waitpid with an 
  option of WNOHANG. If it returns 0, it means it is not finished. At the end, we will print out the completed job in the 
  order they are entered by the user. When we have a new job, we insert it to the end of the job list, we go through the job
  list and check if they are finished. If so print out their completion message and delete them in the job list.
# Testing
  For testing, I simply come up different test cases for each phase and manually test it and compare the results with sample
  program. 
# Resources
  https://www.gnu.org/software/libc/manual/html_mono/libc.html
