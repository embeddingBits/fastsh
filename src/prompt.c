#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

char *get_prompt(void) {
    const char *user = getenv("USER");
    if (!user) user = "user";

    char hostname[64];
    if (gethostname(hostname, sizeof(hostname)) != 0)
        snprintf(hostname, sizeof(hostname), "unknown");

    char *cwd = getcwd(NULL, 0);
    char *dir_name;
    if (cwd) {
        dir_name = basename(cwd);
    } else {
        dir_name = "?";
    }

    char *prompt = NULL;
    int len = asprintf(&prompt,
        "[\x1b[33m%s\x1b[0m\x1b[31m@\x1b[0m\x1b[92m%s\x1b[0m \x1b[34m%s\x1b[0m]$ ",
        user, hostname, dir_name);

    free(cwd);
    if (len < 0) return strdup("$ ");
    return prompt;
}
