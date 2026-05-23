#include "parser.h"
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_wildcard(const char *s) {
    for (; *s; s++) {
        if (*s == '*' || *s == '?' || *s == ']')
            return 1;
    }
    return 0;
}

static void expand_arg(const char *arg, char ***argv, int *argc, int *cap) {
    if (!has_wildcard(arg)) {
        if (*argc >= *cap) {
            *cap = *cap ? *cap * 2 : 8;
            *argv = realloc(*argv, sizeof(char *) * *cap);
        }
        (*argv)[(*argc)++] = strdup(arg);
        return;
    }

    glob_t glob_result;
    int flags = GLOB_NOCHECK | GLOB_TILDE;
    if (glob(arg, flags, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            if (*argc >= *cap) {
                *cap = *cap ? *cap * 2 : 8;
                *argv = realloc(*argv, sizeof(char *) * *cap);
            }
            (*argv)[(*argc)++] = strdup(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
    } else {
        fprintf(stderr, "DEBUG: glob failed or no match for '%s'\n", arg);
        if (*argc >= *cap) {
            *cap = *cap ? *cap * 2 : 8;
            *argv = realloc(*argv, sizeof(char *) * *cap);
        }
        (*argv)[(*argc)++] = strdup(arg);
    }
}

char **parse_command(const char *input, int *argc) {
    if (!input || !*input) {
        *argc = 0;
        return NULL;
    }

    char *buf = strdup(input);
    if (!buf) return NULL;

    int cap = 8;
    int count = 0;
    char **argv = malloc(sizeof(char *) * cap);
    if (!argv) { free(buf); return NULL; }

    char *save = NULL;
    char *token = strtok_r(buf, " \t\r\n", &save);
    while (token) {
        expand_arg(token, &argv, &count, &cap);
        token = strtok_r(NULL, " \t\r\n", &save);
    }

    free(buf);
    *argc = count;
    return argv;
}

void free_argv(char **argv, int argc) {
    if (!argv) return;
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}
