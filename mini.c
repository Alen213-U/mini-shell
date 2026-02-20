#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64

/*
===========================================================
MINI UNIX SHELL - FINAL VERSION

FEATURES IMPLEMENTED:
- Command execution using fork() + execvp()
- Built-in commands: cd, go, pwd, exit
- Input redirection (<)
- Output redirection (>)
- Output append (>>)
- Single pipe (|)
- Background execution (&)
- Signal handling:
    - Ctrl+C does NOT kill shell
    - Background processes do not become zombies

HOW IT WORKS (CORE FLOW):
1. Read input from user
2. Parse into:
   - command(s)
   - arguments
   - special symbols (<, >, >>, |, &)
3. If built-in → execute in parent
4. Else:
   - fork() creates child
   - child:
        - applies redirection (dup2)
        - execvp() replaces process with program
   - parent:
        - wait() OR continue (if background)

PIPE LOGIC:
- Create pipe (two file descriptors)
- First child → writes to pipe
- Second child → reads from pipe

REDIRECTION LOGIC:
- dup2() redirects stdin/stdout to file descriptors

SIGNALS:
- Shell ignores Ctrl+C
- Child processes respond normally
- SIGCHLD ignored → prevents zombie processes

===========================================================
*/

// Prevent shell from exiting on Ctrl+C
void sigint_handler(int sig) {
    printf("\n");
}

// Command structure
typedef struct {
    char *args1[MAX_ARGS];
    char *args2[MAX_ARGS];
    int has_pipe;
    int background;
    char *infile;
    char *outfile;
    int append;
} command_t;

// Reset command structure before each input
void init_command(command_t *cmd) {
    memset(cmd, 0, sizeof(command_t));
}

// Parse input into structure
void parse_input(char *input, command_t *cmd) {
    char *token;
    int i = 0, j = 0;
    int second = 0;

    token = strtok(input, " \t\n");

    while (token) {
        if (strcmp(token, "|") == 0) {
            cmd->has_pipe = 1;
            second = 1;
        }
        else if (strcmp(token, "&") == 0) {
            cmd->background = 1;
        }
        else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) cmd->infile = token;
        }
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) {
                cmd->outfile = token;
                cmd->append = 0;
            }
        }
        else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) {
                cmd->outfile = token;
                cmd->append = 1;
            }
        }
        else {
            if (!second)
                cmd->args1[i++] = token;
            else
                cmd->args2[j++] = token;
        }
        token = strtok(NULL, " \t\n");
    }
}

// Built-in commands handled in parent process
int builtin(command_t *cmd) {
    if (!cmd->args1[0]) return 1;

    if (strcmp(cmd->args1[0], "cd") == 0 ||
        strcmp(cmd->args1[0], "go") == 0) {

        if (!cmd->args1[1]) {
            fprintf(stderr, "cd/go: missing argument\n");
        } else if (chdir(cmd->args1[1]) != 0) {
            perror("cd/go");
        }
        return 1;
    }

    if (strcmp(cmd->args1[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)))
            printf("%s\n", cwd);
        return 1;
    }

    if (strcmp(cmd->args1[0], "exit") == 0) {
        exit(0);
    }

    return 0;
}

// Apply input/output redirection using dup2()
void apply_redirection(command_t *cmd) {
    if (cmd->infile) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) {
            perror("input open");
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->outfile) {
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append)
            flags |= O_APPEND;
        else
            flags |= O_TRUNC;

        int fd = open(cmd->outfile, flags, 0644);
        if (fd < 0) {
            perror("output open");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

// Execute parsed command
void execute(command_t *cmd) {
    if (!cmd->args1[0]) return;

    if (builtin(cmd)) return;

    // Handle pipe
    if (cmd->has_pipe) {
        int pipefd[2];

        if (pipe(pipefd) < 0) {
            perror("pipe");
            return;
        }

        pid_t p1 = fork();
        if (p1 == 0) {
            signal(SIGINT, SIG_DFL);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);

            apply_redirection(cmd);
            execvp(cmd->args1[0], cmd->args1);
            perror("exec1");
            exit(1);
        }

        pid_t p2 = fork();
        if (p2 == 0) {
            signal(SIGINT, SIG_DFL);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            close(pipefd[0]);

            execvp(cmd->args2[0], cmd->args2);
            perror("exec2");
            exit(1);
        }

        close(pipefd[0]);
        close(pipefd[1]);

        if (!cmd->background) {
            waitpid(p1, NULL, 0);
            waitpid(p2, NULL, 0);
        } else {
            printf("[Background pids %d, %d]\n", p1, p2);
        }
        return;
    }

    // Normal execution
    pid_t pid = fork();

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        apply_redirection(cmd);

        execvp(cmd->args1[0], cmd->args1);
        perror("exec");
        exit(1);
    }

    if (!cmd->background)
        waitpid(pid, NULL, 0);
    else
        printf("[Background pid %d]\n", pid);
}

int main() {
    char input[MAX_INPUT_SIZE];
    char cwd[PATH_MAX];
    command_t cmd;

    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, SIG_IGN);

    while (1) {
        init_command(&cmd);

        if (getcwd(cwd, sizeof(cwd)))
            printf("%s mini-shell> ", cwd);
        else
            printf("mini-shell> ");

        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break;
        }

        parse_input(input, &cmd);
        execute(&cmd);
    }

    return 0;
}