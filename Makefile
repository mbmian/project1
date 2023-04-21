CC=gcc
CFLAGS=-Wall -Wextra -Werror

all: sshell

sshell: sshell.c
	$(CC) $(CFLAGS) sshell.c -o sshell

clean:
	rm -f sshell

