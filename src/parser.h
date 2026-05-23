#ifndef PARSER_H
#define PARSER_H

char **parse_command(const char *input, int *argc);
void free_argv(char **argv, int argc);

#endif
