/*This is a backup of main that works when there is exactly one pipe.
 * It is essentially just a generalized pipe_demo.c */

#include <stdio.h>
#include "constants.h"
#include "parsetools.h"
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int main() {
    // Buffer for reading one line of input
    char line[MAX_LINE_CHARS];
    char *first_command[MAX_LINE_WORDS + 1];
    char *second_command[MAX_LINE_WORDS + 1];
    char *line_commands[MAX_LINE_WORDS + 1];

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while (fgets(line, MAX_LINE_CHARS, stdin)) {
        __pid_t childID1 = -1;
        __pid_t childID2 = -1;
        int pfd[2];
        int num_commands = split_cmd_line_commands(line, line_commands);
        int num_words = split_cmd_line_words(line_commands[1], second_command);
        num_words = split_cmd_line_words(line_commands[0], first_command);

        if (pipe(pfd) == -1)
            syserror("Could not create a pipe");
        switch (childID1 = fork()) {
            case -1:
                syserror("First fork failed");
            case 0:
                if (close(0) == -1)
                    syserror("Could not close stdin");
                dup(pfd[0]);
                if (close(pfd[0]) == -1 || close(pfd[1]) == -1)
                    syserror("Could not close pfds from first child");
                execvp(second_command[0], second_command);
                syserror("Could not exec wc");
        }
        fprintf(stderr, "The first child's pid is: %d\n", childID1);
        switch (childID2 = fork()) {
            case -1:
                syserror("Second fork failed");
            case 0:
                if (close(1) == -1)
                    syserror("Could not close stdout");
                dup(pfd[1]);
                if (close(pfd[0]) == -1 || close(pfd[1]) == -1)
                    syserror("Could not close pfds from second child");
                execvp(first_command[0], first_command);
                syserror("Could not exec who");
        }

        fprintf(stderr, "The second child's pid is: %d\n", childID2);
        if (close(pfd[0]) == -1)
            syserror("Parent could not close stdin");
        if (close(pfd[1]) == -1)
            syserror("Parent could not close stdout");
        while (wait(NULL) != -1);

    }

    return 0;
}


void syserror(const char *s)
{
    extern int errno;

    fprintf( stderr, "%s\n", s );
    fprintf( stderr, " (%s)\n", strerror(errno) );
    exit( 1 );
}