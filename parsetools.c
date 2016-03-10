#include <string.h>
#include "constants.h"
#include "parsetools.h"
#include <stdio.h>

// Parse a command line into a list of words,
// separated by whitespace
// Return the number of words 
int split_cmd_line_words(char *line, char **list_to_populate) {
    char *saveptr;  // for strtok_r; see http://linux.die.net/man/3/strtok_r
    char *delimiters = " \t\n"; // whitespace
    int i = 0;
    char *tmpPtr = NULL;
    char *quotes = "\'\"";

    //Remove quotes
    while (tmpPtr = strpbrk(line, quotes)) {
        *tmpPtr = ' ';
    }
    list_to_populate[0] = __strtok_r(line, delimiters, &saveptr);           //This strips out arguments from line.

    while (list_to_populate[i] != NULL && i < MAX_LINE_WORDS - 1) {
        list_to_populate[++i] = __strtok_r(NULL, delimiters, &saveptr);
    };

    return i;
}

// Parse a command line into a list of commands,
// separated by pipes or redirects
// Return the number of pipes and redirects
int split_cmd_line_commands(char *line, char **list_to_populate) {
    char* saveptr;  // for strtok_r; see http://linux.die.net/man/3/strtok_r
    char* delimiters = "|<>";
    int i = 0;

    list_to_populate[0] = __strtok_r(line, delimiters, &saveptr);

    while(list_to_populate[i] != NULL && i < MAX_LINE_WORDS - 1)  {
        list_to_populate[++i] = __strtok_r(NULL, delimiters, &saveptr);
    };

    return i;
}

/* Parse a command line to find the order of
 * pipes and redirects.
 * Precondition: list_to_populate must point to
 * an array with sufficient space for symbols*/
int get_redirect_symbols(const char *line, char *list_to_populate) {
    const char* delimiters = "|<>";
    char *tmpPtr = NULL;
    char localCopy[MAX_LINE_CHARS];
    int i = 0;

    strcpy(localCopy, line);
    tmpPtr = localCopy;

    do {
        tmpPtr = strpbrk(tmpPtr, delimiters);
        list_to_populate[i] = tmpPtr != 0 ? *tmpPtr : '\0';
        ++tmpPtr;             //Need the plus 1 to get beyond symbol
    } while (list_to_populate[i++] != '\0' && i < MAX_LINE_CHARS);

    return --i;
}



