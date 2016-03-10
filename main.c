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

void single_command(char**, pid_t*);
void pipe_setup(char**, pid_t*);
void output_redirect(char**, char**, pid_t*);
void input_redirect(char**, char**, pid_t*);
void double_redirect(char**, char**, char**);

int main() {
    char line[MAX_LINE_CHARS];                  // Buffer for reading one line of input
    char *current_command[MAX_LINE_CHARS];      // One set of command + arguments
    char *next_command[MAX_LINE_CHARS];         // Another set of command + arguments
    char *final_command[MAX_LINE_CHARS];        // Third set of command + arguments
    char *line_commands[MAX_LINE_WORDS + 1];    // Array of commands entered
    char line_symbols[MAX_LINE_CHARS];          // Array of |, <, and >. Should be null terminated

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while (fgets(line, MAX_LINE_CHARS, stdin)) {

        get_redirect_symbols(line, line_symbols);
        int num_commands = split_cmd_line_commands(line, line_commands);
        int symbol_index = 0;
        int command_index = 0;
        char cur_symbol = line_symbols[symbol_index++];
        pid_t first_pid = -1;

        // Loop through commands on this line.
        while (command_index < num_commands) {

            // Split the current command into an array of the command and its arguments
            // so it can be passed to execvp.
            split_cmd_line_words(line_commands[command_index++], current_command);

            // Check if at the end of the line (when reaching a null terminator). If not
            // at the end, then get the next symbol to look ahead when processing redirects.
            char next_symbol = cur_symbol != '\0' ? line_symbols[symbol_index++] : '\0';

            switch (cur_symbol) {

                // A null terminator signifies the end of the current line. If there was a
                // pipe previous to the current_command, then it will already have been forked
                // by pipe_setup and will not be forked again by single_command.
                case '\0':
                    single_command(current_command, &first_pid);
                    break;

                // Set up a the file descriptor for the current_command as well as for which ever
                // command comes next.
                case '|':
                    pipe_setup(current_command, &first_pid);
                    break;

                // Handle output redirection
                case '>':

                    // Prepare the execvp arguments for the next command
                    split_cmd_line_words(line_commands[command_index++], next_command);

                    // If the next symbol is input redirection, then a double redirect needs
                    // to be set up by looking ahead to the command after next_command
                    if (next_symbol == '<') {

                        // Prepare the execvp arguments for the final command
                        split_cmd_line_words(line_commands[command_index++], final_command);

                        // Link the three commands together via redirects
                        double_redirect(final_command, next_command, current_command);

                    // Otherwise, just set up output redirection
                    } else {
                        output_redirect(next_command, current_command, &first_pid);
                    }
                    break;

                // Handle input redirection
                case '<':

                    // Prepare the execvp arguments for the next command
                    split_cmd_line_words(line_commands[command_index++], next_command);

                    // This indicates double redirection again, and is handled identically
                    // to the previous example of double redirection
                    if (next_symbol == '>') {
                        split_cmd_line_words(line_commands[command_index++], final_command);

                        double_redirect(next_command, final_command, current_command);

                    // Otherwise, just set up input redirection
                    } else {
                        input_redirect(next_command, current_command, &first_pid);

                        // Check if the next symbol is a pipe operator, in which case
                        // the pipe will need to be set up before executing the command
                        if (next_symbol == '|') {

                            cur_symbol = next_symbol;
                            next_symbol = line_symbols[symbol_index++]; // Consuming the pipe so we don't do another setup

                            pipe_setup(current_command, &first_pid);

                        // The command can simply be executed if it's at the end of the line
                        } else if (next_symbol == '\0') {
                            single_command(current_command, &first_pid);
                        }
                    }
                    break;

                default:
                    fprintf(stderr, "Tried to use invalid symbol. Somehow.");
                    break;
            }

            // Progress to the next symbol for the next loop iteration
            cur_symbol = next_symbol;
        }

        // Wait for any forked processes to finish before taking a new line of commands
        while (wait(NULL) != -1);
    }
    return 0;
}

void syserror(const char *s) {
    extern int errno;
    fprintf(stderr, "%s\n", s);
    fprintf(stderr, " (%s)\n", strerror(errno));
    exit(1);
}

// Executes a single command which may have been set up by a pipe or redirect function.
// This is the case when first_pid is not -1, so the fork() is skipped and the command
// is immediately executed. The other situation is that the command is by itself (not
// connected to any pipes or redirects) and is forked and executed by single_command.
void single_command(char **command, pid_t *first_pid) {
    pid_t child;

    // The command has not been set up with a pipe or redirect, so it can
    // be forked here and executed.
    if (*first_pid == -1) {
        switch (child = fork()) {
            case -1:
                syserror("Fork failed for single command");

            case 0:
                execvp(command[0], command);
                syserror("exec failed");
        }

    // If first_pid is not -1, then it has already been set up and is ready
    // to be executed now.
    } else if (*first_pid == 0) {
        execvp(command[0], command);
        syserror("exec failed");
    }
}

// Sets up a pipe after being given the left command. The right command's
// file descriptor is set up, and then the program waits to execute the
// right command until it is read. When the right command of a pipe is read,
// it is executed as either the left command to the next pipe, or by
// output_redirect, or as a single command if at the end of the line.
void pipe_setup(char **left_command, pid_t *first_pid) {
    int pfd[2];

    // Keep track of the right child's PID
    pid_t right_child = -1;

    // Create the pipe for the left and right commands to use.
    // If there are multiple pipes in a row, then this means that the
    // right command will use the first pipe for input, and then becomes
    // the left command on the next call and uses a new pipe for output.
    if (pipe(pfd) == -1) {
        syserror("Could not create a pipe");
    }

    // Right side of pipe: set up the fork and then wait until the right
    // command is processed and handled by pipe_setup, output_redirect,
    // or single command. One of these three functions will make the
    // actual execvp() call.
    if (*first_pid == -1 || *first_pid == 0) {
        switch (right_child = fork()) {
            case -1:
                syserror("First fork failed");

            case 0: {
                // Close stdin in preparation for pipe input
                if (close(0) == -1) {
                    syserror("Could not close stdin");
                }

                // Use the pipe's output side for input from the left command
                dup(pfd[0]);

                if (close(pfd[0]) == -1 || close(pfd[1]) == -1) {
                    syserror("Could not close pfds from right child");
                }

                *first_pid = right_child;

                // Right has not necessarily been fully set up, so defer
                // execution until it's handled later.
                return;
            }
        }
    }

    // Left side of the pipe will now be set up
    if (*first_pid == -1) {
        switch (*first_pid = fork()) {
            case -1:
                syserror("First fork failed");

            case 0: {

                // Close stdout in preparation for pipe output
                if (close(1) == -1) {
                    syserror("Could not close stdout");
                }

                // Use the pipe's input side for output to the right command
                dup(pfd[1]);

                if (close(pfd[0]) == -1 || close(pfd[1]) == -1) {
                    syserror("Could not close pfds from left child");
                }

                // Left has now been fully set up, so it can be executed.
                execvp(left_command[0], left_command);
                syserror("exec failed");
            }
        }
    }

    // If the right command was also part of the next pipe, it will become the
    // left command on the next pipe.
    else if (*first_pid == 0) {

        // Close stdout for the previously set up right command, so that it can
        // pipe its output.
        if (close(1) == -1) {
            syserror("Could not close stdout");
        }

        // Use the newly created pipe for the previous right command's output
        dup(pfd[1]);

        if (close(pfd[0]) == -1 || close(pfd[1]) == -1) {
            syserror("Could not close pfds from left child");
        }

        // Execute the previous right command (which has become the left command)
        execvp(left_command[0], left_command);
        syserror("exec failed");
    }

    // Cleanup on open pipes
    if (close(pfd[0]) == -1)
        syserror("Parent could not close pfd0");
    if (close(pfd[1]) == -1)
        syserror("Parent could not close pfd1");
}

// Sets up output redirection
void output_redirect(char **second_command, char **first_command, pid_t *first_pid) {
    int fd = -1;

    // Set up the redirection and execution for the command if it hasn't already been handled by a different operation
    if (*first_pid == -1) {
      switch (*first_pid = fork())
      {
          case -1:
              syserror("Fork failed for single command (output redirect)");

          case 0:

              // Open the file as writeable and get its descriptor
              fd = open(second_command[0], O_WRONLY | O_CREAT | O_TRUNC, 0640);

              if (fd == -1) {
                  syserror("Could not open output file");
              }

              // Close stdout so that output can be sent to the file
              if (close(1) == -1) {
                  syserror("Could not close stdout");
              }

              // Set the opened file as output
              dup(fd);

              if (close(fd) == -1) {
                  syserror("Could not close output fd");
              }

              // Execute the command
              execvp(first_command[0], first_command);
              syserror("exec failed");
        }

    // If the command has already been set up to receive input via pipe then don't fork and just execute it
    } else if (*first_pid == 0) {
        // Open the file as writeable and get its descriptor
        fd = open(second_command[0], O_WRONLY | O_CREAT | O_TRUNC, 0640);

        if (fd == -1) {
            syserror("Could not open output file");
        }

        // Close stdout so that output can be sent to the file
        if (close(1) == -1) {
            syserror("Could not close stdout");
        }
        // Set the opened file as output
        dup(fd);

        if (close(fd) == -1) {
            syserror("Could not close output fd");
        }

        // Execute the command
        execvp(first_command[0], first_command);
        syserror("exec failed");
    }
}

// Very simliar to output_redirect, but the file is opened as readonly and input
// to the command is sourced from the file.
void input_redirect(char **second_command, char **first_command, pid_t *first_pid) {
    int fd = -1;

    switch (*first_pid = fork())
    {
        case -1:
            syserror("Fork failed for single command (input redirect)");

        case 0:
            // Open the file as readonly and get its descriptor
            fd = open(second_command[0], O_RDONLY);

            if (fd == -1) {
                syserror("Could not open input file");
            }

            // Close stdin so that input can come from the file
            if (close(0) == -1) {
                syserror("Could not close stdin");
            }

            // Set the opened file as input
            dup(fd);

            if (close(fd) == -1) {
                syserror("Could not close input file fd");
            }
    }
}

// Set up a double_redirect so that a command can take a file as input and write
// to a file for output.
void double_redirect(char **input_argument, char **output_argument, char **command) {
    pid_t child = -1;
    int input_fd = -1;
    int output_fd = -1;

    switch (child = fork()) {
        case -1:
            syserror("Fork failed for single command (double redirect)");

        case 0:
            // Open the input file as readonly and get its descriptor
            input_fd = open(input_argument[0], O_RDONLY);

            if (input_fd == -1) {
                syserror("Could not open input file");
            }

            // Close stdin so that input can come from the file
            if (close(0) == -1) {
                syserror("Could not close stdin");
            }

            // Set the opened file as input
            dup(input_fd);

            // Close the input file once finished with it
            if (close(input_fd) == -1) {
                syserror("Could not close input file fd");
            }

            // Open the ouput file as writeable and get its descriptor
            output_fd = open(output_argument[0], O_WRONLY | O_CREAT | O_TRUNC, 0640);

            if (output_fd == -1) {
                syserror("Could not open output file");
            }

            // Close stdout so that output can be sent to the file
            if (close(1) == -1) {
                syserror("Could not close stdout");
            }

            // Set the opened file as output
            dup(output_fd);

            // Close the output file once finished with it
            if (close(output_fd) == -1) {
                syserror("Could not close output fd");
            }

            // Execute the command
            execvp(command[0], command);
            syserror("exec failed");
    }
}
