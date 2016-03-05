#include <stdio.h>
#include "constants.h"
#include "parsetools.h"
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

void single_command(char **command);

//void multi_pipe_setup(char** current_command, char **line_commands, char *line_symbols, int *command_index, int *symbol_index);

void pipe_setup(char **second_command, char **first_command);

void output_redirect(char **second_command, char **first_command);

void input_redirect(char **second_command, char **first_command);


int main() {
    char line[MAX_LINE_CHARS];                  // Buffer for reading one line of input
    char *first_command[MAX_LINE_WORDS + 1];    // One set of command + arguments
    char *second_command[MAX_LINE_WORDS + 1];   // Another set of command + arguments
    char *line_commands[MAX_LINE_WORDS + 1];    // Array of commands entered
    char line_symbols[MAX_LINE_CHARS];          // Array of |, <, and >. Should be null terminated

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while (fgets(line, MAX_LINE_CHARS, stdin)) {

        int num_symbols = get_redirect_symbols(line, line_symbols);
        int num_commands = split_cmd_line_commands(line, line_commands);
        int symbol_index = 0;

        //Loop through commands on this line.
        //Take commands two at a time if possible
        int command_index = 0;
        while (command_index < num_commands) {
            int num_words = split_cmd_line_words(line_commands[command_index++], first_command);

            switch (line_symbols[symbol_index++]) {
                case '\0':
                    single_command(first_command);
                    break;
                case '|':
                    //multi_pipe_setup(first_command, line_commands, line_symbols, &command_index, &symbol_index);
                    num_words = split_cmd_line_words(line_commands[command_index++], second_command);
                    pipe_setup(second_command, first_command);
                    break;
                case '>':
                    num_words = split_cmd_line_words(line_commands[command_index++], second_command);
                    output_redirect(second_command, first_command);
                    break;
                case '<':
                    num_words = split_cmd_line_words(line_commands[command_index++], second_command);
                    input_redirect(second_command, first_command);
                    break;
                default:
                    break; //TODO: Add error handler if it makes sense
            }

            while (wait(NULL) != -1); // Wait for processes to finish before taking more

        }
    }

    return 0;
}

void syserror(const char *s) {
    extern int errno;

    fprintf(stderr, "%s\n", s);
    fprintf(stderr, " (%s)\n", strerror(errno));
    exit(1);
}


void single_command(char **command) {
    pid_t child = fork();

    if (child == 0) {
        execvp(command[0], command);
        syserror("exec failed");
    }
}


/*void multi_pipe_setup(char** current_command, char **line_commands, char *line_symbols, int *command_index, int *symbol_index) {
    char *next_command[MAX_LINE_WORDS + 1];

    while (line_symbols[*symbol_index] == '|');
}*/


void pipe_setup(char **second_command, char **first_command) {
    int pfd[2];
    pid_t left_child = -1;
    pid_t right_child = -1;

    if (pipe(pfd) == -1) {
        syserror("Could not create a pipe");
    }

    //Right side of pipe
    switch (right_child = fork()) {
        case -1:
            syserror("First fork failed");
        case 0: {
            if (close(0) == -1) {
                syserror("Could not close stdin");
            }
            dup(pfd[0]);

            if (close(pfd[0]) == -1 || close(pfd[1]) == -1) {
                syserror("Could not close pfds from right child");
            }
            //fprintf(stderr, "%s reading from %d\n", second_command[0], pfd[0]);
            execvp(second_command[0], second_command);
            syserror("exec failed");
        }
    }

    //Left side of pipe
    switch (left_child = fork()) {
        case -1:
            syserror("First fork failed");
        case 0: {
            if (close(1) == -1) {
                syserror("Could not close stdout");
            }
            dup(pfd[1]);

            if (close(pfd[0]) == -1 || close(pfd[1]) == -1) {
                syserror("Could not close pfds from left child");
            }
            //fprintf(stderr, "%s printing to %d\n", first_command[0], pfd[1]);
            execvp(first_command[0], first_command);
            syserror("exec failed");
        }
    }

    if (close(pfd[0]) == -1)
        syserror("Parent could not close stdin");
    if (close(pfd[1]) == -1)
        syserror("Parent could not close stdout");
}



void output_redirect(char **second_command, char **first_command) {
    int fd = open(second_command[0], O_WRONLY | O_CREAT | O_TRUNC);

    if (fd == -1) {
        syserror("Could not open output file");
    }

    if (close(1) == -1) {
        syserror("Could not close stdout");
    }
    dup(fd);
    if (close(fd) == -1) {
        syserror("Could not close output fd");
    }

    single_command(first_command);

}

void input_redirect(char **second_command, char **first_command) {
    int fd = open(second_command[0], O_RDONLY);

    if (fd == -1) {
        syserror("Could not open input file");
    }

    if (close(0) == -1) {
        syserror("Could not close stdin");
    }
    dup(fd);
    if (close(fd) == -1) {
        syserror("Could not close input fd");
    }

    single_command(first_command);
}