#include "builtins.h"
#include "executor.h"
#include "parser.h"
#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

static char *get_history_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char *path = NULL;
    asprintf(&path, "%s/.fastsh_history", home);
    return path;
}

int main(void) {
    char *history_path = get_history_path();
    if (history_path) {
        read_history(history_path);
    }

    while (1) {
        char *prompt = get_prompt();
        char *line = readline(prompt);
        free(prompt);

        if (!line) break;

        if (*line == '\0') {
            free(line);
            continue;
        }

        add_history(line);

        int argc = 0;
        char **argv = parse_command(line, &argc);
        free(line);

        if (!argv || argc == 0) continue;

        int ret = 0;
        if (is_builtin(argv[0])) {
            ret = exec_builtin(argc, argv);
            if (ret == -1) {
                free_argv(argv, argc);
                break;
            }
        } else {
            ret = execute_external(argc, argv);
        }

        free_argv(argv, argc);
    }

    if (history_path) {
        write_history(history_path);
        free(history_path);
    }

    return 0;
}
