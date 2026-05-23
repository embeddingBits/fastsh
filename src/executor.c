#include "executor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int spawn_and_wait(char **argv) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int execute_external(int argc, char **argv) {
    (void)argc;
    const char *cmd = argv[0];

    if (strchr(cmd, '/')) {
        return spawn_and_wait(argv);
    }

    const char *path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";

    char *path_copy = strdup(path);
    if (!path_copy) return -1;

    char *save = NULL;
    char *dir = strtok_r(path_copy, ":", &save);
    int result = -1;

    while (dir) {
        size_t len = strlen(dir) + 1 + strlen(cmd) + 1;
        char *full = malloc(len);
        if (!full) { dir = strtok_r(NULL, ":", &save); continue; }
        snprintf(full, len, "%s/%s", dir, cmd);

        char **new_argv = malloc(sizeof(char *) * (argc + 1));
        if (!new_argv) { free(full); dir = strtok_r(NULL, ":", &save); continue; }
        new_argv[0] = full;
        for (int i = 1; i < argc; i++) new_argv[i] = argv[i];
        new_argv[argc] = NULL;

        int ret = spawn_and_wait(new_argv);
        free(full);
        free(new_argv);

        if (ret != -1 && ret != 127) {
            result = ret;
            break;
        }
        dir = strtok_r(NULL, ":", &save);
    }

    free(path_copy);

    if (result == -1) {
        fprintf(stderr, "%s: command not found\n", cmd);
    }
    return result;
}
