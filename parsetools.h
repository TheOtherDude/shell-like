#ifndef PARSETOOLS_H
#define PARSETOOLS_H

// Parse a command line into a list of words,
// separated by whitespace
// Return the number of words 
int split_cmd_line_words(char *line, char **list_to_populate);
int split_cmd_line_commands(char *line, char **list_to_populate);

#endif
