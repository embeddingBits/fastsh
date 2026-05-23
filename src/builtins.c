#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *builtins[] = {
    "exit", "cd", "pwd", "type", "echo",
    "builtin", "clear", NULL
};

int builtin_echo(int argc, char **argv) {
    if (argc <= 1) {
        putchar('\n');
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        fputs(argv[i], stdout);
    }
    putchar('\n');
    return 0;
}

int builtin_pwd(void) {
    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        perror("pwd");
        return 1;
    }
    puts(cwd);
    free(cwd);
    return 0;
}

int builtin_clear(void) {
    fputs("\x1B[2J\x1B[H", stdout);
    return 0;
}

int builtin_cd(int argc, char **argv) {
    const char *target;
    if (argc < 2 || strcmp(argv[1], "~") == 0) {
        target = getenv("HOME");
        if (!target) {
            fputs("cd: HOME not set\n", stderr);
            return 1;
        }
    } else {
        target = argv[1];
    }
    if (chdir(target) != 0) {
        fprintf(stderr, "cd: %s: No such directory\n", target);
        return 1;
    }
    return 0;
}

int builtin_builtin(void) {
    for (int i = 0; builtins[i]; i++) {
        puts(builtins[i]);
    }
    return 0;
}

int builtin_type(int argc, char **argv) {
    if (argc < 2) return 0;
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(argv[1], builtins[i]) == 0) {
            printf("%s is a builtin\n", builtins[i]);
            return 0;
        }
    }
    printf("%s not found\n", argv[1]);
    return 0;
}

int is_builtin(const char *cmd) {
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(cmd, builtins[i]) == 0)
            return 1;
    }
    return 0;
}

int exec_builtin(int argc, char **argv) {
    if (argc < 1) return 1;
    const char *cmd = argv[0];
    if (strcmp(cmd, "exit") == 0) return -1;
    if (strcmp(cmd, "echo") == 0) return builtin_echo(argc, argv);
    if (strcmp(cmd, "pwd") == 0) return builtin_pwd();
    if (strcmp(cmd, "clear") == 0) return builtin_clear();
    if (strcmp(cmd, "cd") == 0) return builtin_cd(argc, argv);
    if (strcmp(cmd, "builtin") == 0) return builtin_builtin();
    if (strcmp(cmd, "type") == 0) return builtin_type(argc, argv);
    return 1;
}
