#include <string.h>
#include "constants.h"
#include "parsetools.h"
#include <stdio.h>

// Parse a command line into a list of words,
// separated by whitespace
// Return the number of words 
int split_cmd_line_words(char *line, char **list_to_populate) {
   char* saveptr;  // for strtok_r; see http://linux.die.net/man/3/strtok_r
   char* delimiters = " \t\n"; // whitespace
   int i = 0;

   list_to_populate[0] = __strtok_r(line, delimiters, &saveptr);

   while(list_to_populate[i] != NULL && i < MAX_LINE_WORDS - 1)  {
       list_to_populate[++i] = __strtok_r(NULL, delimiters, &saveptr);
   };

   return i;
}

// Parse a command line into a list of commands,
// separated by pipes
// Return the number of pipes
int split_cmd_line_commands(char *line, char **list_to_populate) {
    char* saveptr;  // for strtok_r; see http://linux.die.net/man/3/strtok_r
    char* delimiters = "|"; // pipes
    int i = 0;

    list_to_populate[0] = __strtok_r(line, delimiters, &saveptr);

    while(list_to_populate[i] != NULL && i < MAX_LINE_WORDS - 1)  {
        list_to_populate[++i] = __strtok_r(NULL, delimiters, &saveptr);
    };

    return i;
}
