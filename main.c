#include <stdio.h>
#include "constants.h"
#include "parsetools.h"
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char line[MAX_LINE_CHARS];                  // Buffer for reading one line of input
    char *first_command[MAX_LINE_WORDS + 1];    // One set of command + arguments
    char *second_command[MAX_LINE_WORDS + 1];   // Another set of command + arguments
    char *line_commands[MAX_LINE_WORDS + 1];    // Array of commands entered

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while (fgets(line, MAX_LINE_CHARS, stdin)) {

        __pid_t childID1 = -1;
        __pid_t childID2 = -1;
        int num_commands = split_cmd_line_commands(line, line_commands);

        //Loop through commands on this line.
        //Take commands two at a time if possible (and assume pipe between them)
        int command_index = 0;
        while (command_index < num_commands) {
            int num_words = split_cmd_line_words(line_commands[command_index++], first_command);
            int pfd[2];

            //if we have 2+ commands left, look at them both at once
            if ((num_commands - command_index) > 0) {

                if (pipe(pfd) == -1) {
                    syserror("Could not create a pipe");
                }

                childID1 = fork();
                childID2 = fork();

                num_words = split_cmd_line_words(line_commands[command_index++], second_command);

            }
                //otherwise, simply run the single command
            else {

                childID1 = fork();

                if (childID1 == 0) {
                    execvp(first_command[0], first_command);
                    syserror("exec failed");
                }
            }

            //Right side of pipe
            if (childID2 == 0) {

                if ( close( 1 ) == -1 ) {
                    syserror("Could not close stdout");
                }
                dup(pfd[1]);

                if ( close (pfd[0]) == -1 || close (pfd[1]) == -1 ) {
                    syserror("Could not close pfds from right child");
                }

                execvp(second_command[0], second_command);
                syserror( "exec failed");

            }
                //Left side of pipe
            else if (childID1 == 0) {

                if ( close( 0 ) == -1 ) {
                    syserror("Could not close stdin");
                }
                dup(pfd[0]);

                if ( close (pfd[0]) == -1 || close (pfd[1]) == -1 ) {
                    syserror("Could not close pfds from left child");
                }

                execvp(first_command[0], first_command);
                syserror( "exec failed");

            }
                //If there was a pipe
            else if (childID2 != -1) {

                if (close(pfd[0]) == -1)
                    syserror("Parent could not close stdin");
                if (close(pfd[1]) == -1)
                    syserror("Parent could not close stdout");
            }

            }

        while ( wait(NULL) != -1); // Wait for processes to finish before taking more

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
