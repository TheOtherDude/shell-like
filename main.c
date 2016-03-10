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

//define DEBUG

void single_command(char **command, pid_t *first_pid);

void pipe_setup(char **left_command, pid_t *first_pid);

void output_redirect(char **second_command, char **first_command, pid_t *first_pid);

void input_redirect(char **second_command, char **first_command, pid_t *first_pid);

void double_redirect(char **input_argument, char **output_argument, char **command);


int main() {
    char line[MAX_LINE_CHARS];                  // Buffer for reading one line of input
    char *current_command[MAX_LINE_CHARS];      // One set of command + arguments
    char *next_command[MAX_LINE_CHARS];         // Another set of command + arguments
    char *final_command[MAX_LINE_CHARS];        // Third set of command + arguments
    char *line_commands[MAX_LINE_WORDS + 1];    // Array of commands entered
    char line_symbols[MAX_LINE_CHARS];          // Array of |, <, and >. Should be null terminated

    //printf("SHELL-LIKE:");

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while (fgets(line, MAX_LINE_CHARS, stdin)) {

        get_redirect_symbols(line, line_symbols);
        int num_commands = split_cmd_line_commands(line, line_commands);
        int symbol_index = 0;
        int command_index = 0;
        char cur_symbol = line_symbols[symbol_index++];
        pid_t first_pid = -1;
#ifdef DEBUG
        fprintf(stderr, "shell-like's pid is %d\n", getpid());
#endif


        //Loop through commands on this line.
        while (command_index < num_commands) {
#ifdef DEBUG
            fprintf(stderr, "%d (parent: %d) entered command loop with cur_symbol = %x and first_pid = %d\n", getpid(),
                    getppid(), cur_symbol, first_pid);
#endif
            split_cmd_line_words(line_commands[command_index++], current_command);
            char next_symbol = cur_symbol != '\0' ? line_symbols[symbol_index++] : '\0';

            switch (cur_symbol) {
                case '\0':
                    single_command(current_command, &first_pid);
                    break;
                case '|':
                    pipe_setup(current_command, &first_pid);
                    break;
                case '>':
                    if (next_symbol == '<') {

                        split_cmd_line_words(line_commands[command_index++], next_command);
                        split_cmd_line_words(line_commands[command_index++], final_command);
                        double_redirect(final_command, next_command, current_command);

                    } else {
                        split_cmd_line_words(line_commands[command_index++], next_command);
                        output_redirect(next_command, current_command, &first_pid);

                    }
                    break;
                case '<':
                    if (next_symbol == '>') {

                        split_cmd_line_words(line_commands[command_index++], next_command);
                        split_cmd_line_words(line_commands[command_index++], final_command);
                        double_redirect(next_command, final_command, current_command);

                    } else {

                        split_cmd_line_words(line_commands[command_index++], next_command);
                        input_redirect(next_command, current_command, &first_pid);

                        if (next_symbol == '|') {

                            cur_symbol = next_symbol;
                            next_symbol = line_symbols[symbol_index++]; //Consuming the pipe so we don't do another setup

                            pipe_setup(current_command, &first_pid);

                        } else if (next_symbol == '\0') {

                            //Have to do the exec here because we will not re-enter loop
                            single_command(current_command, &first_pid);

                        }
                    }
                    break;
                default:
                    fprintf(stderr, "Tried to use invalid symbol. Somehow.");
                    break;
            }

            cur_symbol = next_symbol;
        }

        while (wait(NULL) != -1); // Wait for processes to finish before taking more
        //printf("SHELL-LIKE:");
    }

    return 0; //TODO: Exits with status 11 instead of 0. Why?!?
}

void syserror(const char *s) {
    extern int errno;

    fprintf(stderr, "%s\n", s);
    fprintf(stderr, " (%s)\n", strerror(errno));
    exit(1);
}


void single_command(char **command, pid_t *first_pid) {
    pid_t child;

    if (*first_pid == -1) {

        switch (child = fork()) {
            case -1:
                syserror("Fork failed for single command");
            case 0:
#ifdef DEBUG
                fprintf(stderr, "single_command(-1) Exec'ing %s from %d (parent: %d)\n", command[0], getpid(),
                        getppid());
#endif
                execvp(command[0], command);
                syserror("exec failed");
        }
    }
    else if (*first_pid == 0) {
#ifdef DEBUG
        fprintf(stderr, "single_command(0) Exec'ing %s from %d (parent: %d)\n", command[0], getpid(), getppid());
#endif
        execvp(command[0], command);
        syserror("exec failed");

    }

}


void pipe_setup(char **left_command, pid_t *first_pid) {
    int pfd[2];
    pid_t right_child = -1;

    if (pipe(pfd) == -1) {
        syserror("Could not create a pipe");
    }

    if (*first_pid == -1 || *first_pid == 0) {

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
#ifdef DEBUG
                fprintf(stderr, "pipe_setup(right side) forked %d (parent: %d)\n", getpid(), getppid());
#endif
                *first_pid = right_child;
                return; //right child is done for now
            }
        }
    }


    if (*first_pid == -1) {

        //Left side of pipe
        switch (*first_pid = fork()) {
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
#ifdef DEBUG
                fprintf(stderr, "pipe_setup(-1) Exec'ing %s from %d (parent: %d)\n", left_command[0], getpid(),
                        getppid());
#endif
                execvp(left_command[0], left_command);
                syserror("exec failed");
            }
        }
    }
    else if (*first_pid == 0) {
        if (close(1) == -1) {
            syserror("Could not close stdout");
        }
        dup(pfd[1]);

        if (close(pfd[0]) == -1 || close(pfd[1]) == -1) {
            syserror("Could not close pfds from left child");
        }
#ifdef DEBUG
        fprintf(stderr, "pipe_setup(0) Exec'ing %s from %d (parent: %d)\n", left_command[0], getpid(), getppid());
#endif
        execvp(left_command[0], left_command);
        syserror("exec failed");
    }

    if (close(pfd[0]) == -1)
        syserror("Parent could not close pfd0");
    if (close(pfd[1]) == -1)
        syserror("Parent could not close pfd1");
}


void output_redirect(char **second_command, char **first_command, pid_t *first_pid) {
    pid_t child = -1;
    int fd = -1;

    if (*first_pid == -1) {

        switch (*first_pid = fork()) {
            case -1:
                syserror("Fork failed for single command (output redirect)");
            case 0:
                fd = open(second_command[0], O_WRONLY | O_CREAT | O_TRUNC, 0640);
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
#ifdef DEBUG
                fprintf(stderr, "output_redirect(-1) Exec'ing %s from %d (parent: %d)\n", first_command[0], getpid(),
                        getppid());
#endif
                execvp(first_command[0], first_command);
                syserror("exec failed");

        }
    } else if (*first_pid == 0) {
        fd = open(second_command[0], O_WRONLY | O_CREAT | O_TRUNC, 0640);
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
#ifdef DEBUG
        fprintf(stderr, "output_redirect(0) Exec'ing %s from %d (parent: %d)\n", first_command[0], getpid(), getppid());
#endif
        execvp(first_command[0], first_command);
        syserror("exec failed");
    }


}

void input_redirect(char **second_command, char **first_command, pid_t *first_pid) {
    int fd = -1;

    switch (*first_pid = fork()) {
        case -1:
            syserror("Fork failed for single command (input redirect)");
        case 0:
            fd = open(second_command[0], O_RDONLY);
            if (fd == -1) {
                syserror("Could not open input file");
            }

            if (close(0) == -1) {
                syserror("Could not close stdin");
            }
            dup(fd);
            if (close(fd) == -1) {
                syserror("Could not close input file fd");
            }
#ifdef DEBUG
            fprintf(stderr, "input_redirect forked %d (parent: %d)\n", getpid(), getppid());
#endif
            //Exec happens in single_command() or pipe_setup() from main()
    }

}

void double_redirect(char **input_argument, char **output_argument, char **command) {
    pid_t child = -1;
    int input_fd = -1;
    int output_fd = -1;

    switch (child = fork()) {
        case -1:
            syserror("Fork failed for single command (double redirect)");
        case 0:
            // Input redirect
            input_fd = open(input_argument[0], O_RDONLY);
            if (input_fd == -1) {
                syserror("Could not open input file");
            }

            if (close(0) == -1) {
                syserror("Could not close stdin");
            }
            dup(input_fd);
            if (close(input_fd) == -1) {
                syserror("Could not close input file fd");
            }

            // Output redirect
            output_fd = open(output_argument[0], O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (output_fd == -1) {
                syserror("Could not open output file");
            }

            if (close(1) == -1) {
                syserror("Could not close stdout");
            }
            dup(output_fd);
            if (close(output_fd) == -1) {
                syserror("Could not close output fd");
            }
#ifdef DEBUG
            fprintf(stderr, "double_redirect() Exec'ing %s from %d (parent: %d)\n", command[0], getpid(), getppid());
#endif
            execvp(command[0], command);
            syserror("exec failed");

    }
}