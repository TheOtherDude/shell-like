#include <stdio.h>
#include "constants.h"
#include "parsetools.h"
#include <sys/wait.h>
#include <unistd.h>

int main() {
    // Buffer for reading one line of input
    char line[MAX_LINE_CHARS];
    char* line_words[MAX_LINE_WORDS + 1];
    char* line_commands[MAX_LINE_WORDS + 1];

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while( fgets(line, MAX_LINE_CHARS, stdin) ) {
        int num_pipes = split_cmd_line_commands(line, line_commands);
        for (int i = 0; i < num_pipes; ++i) {
            int num_words = split_cmd_line_words(line_commands[i], line_words);
            __pid_t child = fork();

            if (child == 0) {
                execvp(line_words[0], line_words);
            }

            while ( wait(NULL) != -1); // Wait for processes to finish before taking more

        }
    }

    return 0;
}
