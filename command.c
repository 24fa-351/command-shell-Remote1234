// By Eymard Alarcon //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h> 

#define BUFFER_SIZE 1024
#define ENV_SIZE 256

typedef struct {
    char name[BUFFER_SIZE];
    char value[BUFFER_SIZE];
} EnvVar;

EnvVar xsh_env[ENV_SIZE];
int env_count = 0;

void execute_command(char *input);
void change_directory(char *path);
void print_working_directory();
void set_env_var(char *name, char *value);
void unset_env_var(char *name);
char* get_env_var(char *name);
void replace_env_vars(char *input, char *output);
void parse_and_execute(char *input);
void handle_pipes_and_redirections(char *input);

int main() {
    char input[BUFFER_SIZE];

    printf("Welcome to xsh - Your Custom Shell!\n");
    while (1) {
        printf("xsh# ");
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            printf("\n");
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            break;
        }
        execute_command(input);
    }
    return 0;
}

void execute_command(char *input) {
    char expanded_input[BUFFER_SIZE];
    replace_env_vars(input, expanded_input);
    handle_pipes_and_redirections(expanded_input);
}

void change_directory(char *path) {
    if (chdir(path) != 0) {
        perror("cd");
    }
}

void print_working_directory() {
    char cwd[BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

void set_env_var(char *name, char *value) {
    for (int i = 0; i < env_count; ++i) {
        if (strcmp(xsh_env[i].name, name) == 0) {
            strcpy(xsh_env[i].value, value);
            return;
        }
    }
    if (env_count < ENV_SIZE) {
        strcpy(xsh_env[env_count].name, name);
        strcpy(xsh_env[env_count].value, value);
        env_count++;
    } else {
        fprintf(stderr, "Environment storage full.\n");
    }
}

void unset_env_var(char *name) {
    for (int i = 0; i < env_count; ++i) {
        if (strcmp(xsh_env[i].name, name) == 0) {
            for (int j = i; j < env_count - 1; ++j) {
                xsh_env[j] = xsh_env[j + 1];
            }
            env_count--;
            return;
        }
    }
}

char* get_env_var(char *name) {
    for (int i = 0; i < env_count; ++i) {
        if (strcmp(xsh_env[i].name, name) == 0) {
            return xsh_env[i].value;
        }
    }
    return NULL;
}

void replace_env_vars(char *input, char *output) {
    char *start = input, *end;
    char var_name[BUFFER_SIZE], *var_value;

    while ((start = strchr(start, '$')) != NULL) {
        strncpy(output, input, start - input);
        output += start - input;
        start++;
        end = start;
        while (*end && (isalnum(*end) || *end == '_')) {
            end++;
        }
        strncpy(var_name, start, end - start);
        var_name[end - start] = '\0';
        var_value = get_env_var(var_name);
        if (var_value) {
            strcpy(output, var_value);
            output += strlen(var_value);
        }
        input = end;
        start = end;
    }
    strcpy(output, input);
}

void handle_pipes_and_redirections(char *input) {
    char *commands[BUFFER_SIZE];
    int command_count = 0;
    char *token = strtok(input, "|");

    while (token) {
        commands[command_count++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2], prev_fd = -1;
    for (int i = 0; i < command_count; ++i) {
        pipe(pipefd);
        if (fork() == 0) {
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < command_count - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
            }
            close(pipefd[0]);
            close(pipefd[1]);
            parse_and_execute(commands[i]);
            exit(0);
        } else {
            close(pipefd[1]);
            if (prev_fd != -1) {
                close(prev_fd);
            }
            prev_fd = pipefd[0];
        }
    }
    while (wait(NULL) > 0);
}

void parse_and_execute(char *input) {
    char *args[BUFFER_SIZE];
    char *filename = NULL;
    int append = 0, fd;

    if (strchr(input, '>')) {
        char *output = strchr(input, '>');
        *output = '\0';
        filename = strtok(output + 1, " ");
    }

    int arg_count = 0;
    char *token = strtok(input, " ");
    while (token) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (strcmp(args[0], "cd") == 0) {
        change_directory(args[1]);
    } else if (strcmp(args[0], "pwd") == 0) {
        print_working_directory();
    } else if (strcmp(args[0], "set") == 0) {
        set_env_var(args[1], args[2]);
    } else if (strcmp(args[0], "unset") == 0) {
        unset_env_var(args[1]);
    } else if (filename) {
        fd = open(filename, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
        if (fd < 0) {
            perror("open");
        } else {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else {
        execvp(args[0], args);
        perror("execvp");
    }
}