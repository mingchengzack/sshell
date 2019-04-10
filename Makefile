sshell: sshell.o
	gcc -Wall -Werror -o sshell sshell.o

sshell.o: sshell.c
	gcc -Wall -Werror -c -o sshell.o sshell.c

clean:
	rm -f sshell sshell.o
