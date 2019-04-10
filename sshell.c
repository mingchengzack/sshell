#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	char *cmd[3] = { "/bin/date", "-u", NULL };
	pid_t pid;
	int status;

	pid = fork();
	if(pid == 0) {
		/* Child */
		execvp(cmd[0], cmd);
		perror("execvp");
		exit(EXIT_FAILURE);
	} else if(pid > 0) {
		/* Parent */
		waitpid(-1, &status, 0);
		printf("Return status value for '%s': %d\n", cmd[0], status);
	} else {
                /* fork error */
		perror("fork");
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}
