#include "builtins.h"
#include "executor.h"
#include "parser.h"
#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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

void sigint_handler(int sig)
{
      (void)sig;

      write(STDOUT_FILENO, "\n", 1);

      rl_on_new_line();
      rl_replace_line("", 0);
      rl_redisplay();
}

int main(void) {
    char *history_path = get_history_path();
    if (history_path) {
        read_history(history_path);
    }

    signal(SIGINT, sigint_handler);

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

