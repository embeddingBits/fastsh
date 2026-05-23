#ifndef BUILTINS_H
#define BUILTINS_H

int builtin_echo(int argc, char **argv);
int builtin_pwd(void);
int builtin_clear(void);
int builtin_cd(int argc, char **argv);
int builtin_builtin(void);
int builtin_type(int argc, char **argv);

int is_builtin(const char *cmd);
int exec_builtin(int argc, char **argv);

#endif
