#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

char *delim = " \t\n";


char error_message[30] = "An error has occurred\n";

//print error message
void print_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}


char *search_paths[64];
int num_search_paths = 0;

// set path environment
void set_path_environment(char *default_path) {
    static char path_env[1024];
    strcpy(path_env, default_path);
    for (int i = 0; i < num_search_paths; i++) {
        strcat(path_env, search_paths[i]);
        if (i < num_search_paths - 1) {
            strcat(path_env, ":");
        }
    }
    // printf("path_env: %s\n", path_env);
    putenv(path_env);
}

// execute builtin commands
int execute_builtin(char **args) {
    if (args[0] == NULL) {
        return 0;
    }
    if (strcmp(args[0], "exit") == 0) {
        if (args[1] != NULL) {
            print_error();
            return 0;
        }
        exit(0);
    } else if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || args[2] != NULL) {
            print_error();
            return 0;
        }
        if (chdir(args[1]) != 0) {
            print_error();
            return 0;
        }
    } else if (strcmp(args[0], "path") == 0) {
        num_search_paths = 0;
        for (int i = 1; args[i] != NULL; i++) {
            search_paths[num_search_paths++] = args[i];
        }

        if (args[1] == NULL){
            set_path_environment("PATH=");
        } else{
            set_path_environment("PATH=/bin:/usr/bin:");
        }
    }
    return 1;
}

// Preprocess the line for the line with > and & for the execution
char *line_preprocess(char *line) {
    char *new_line = malloc(100 * sizeof(char));
    int j = 0;
    for (int i = 0; i < strlen(line); i++) {
        if (line[i] == '>') {
            new_line[j++] = ' ';
            new_line[j++] = '>';
            new_line[j++] = ' ';
        } else if (line[i] == '&') {
            new_line[j++] = ' ';
            new_line[j++] = '&';
            new_line[j++] = ' ';
        }
        else {
            new_line[j++] = line[i];
        }
    }

    return new_line;
}

// Read line from stdin
char *my_read_line(void) {
    char *line = NULL;
    size_t bufsize = 64; // getline will allocate a buffer
    getline(&line, &bufsize, stdin);

    if (strstr(line, ">") != NULL || strstr(line, "&") != NULL){
        line = line_preprocess(line);
    }

    return line;
}

// Read line from file
char *my_read_file(FILE *file) {
    char *line = NULL;
    size_t bufsize = 64;
    ssize_t read = getline(&line, &bufsize, file);
    if (read == -1) {
        free(line);
        return NULL;
    }

    if (strstr(line, ">") != NULL || strstr(line, "&") != NULL){
        line = line_preprocess(line);
    }

    return line;
}

// Split the line into tokens
char **my_args(char *line, char *delim){
    int bufsize = 64, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;
    token = strsep(&line, delim);
    while (token != NULL) {
        if (strlen(token) > 0) { // Ignore empty tokens
            tokens[position++] = token;
        }

        if (position >= bufsize) {
            bufsize += 64;
            tokens = realloc(tokens, bufsize * sizeof(char*));
        }

        token = strsep(&line, delim);
    }
    tokens[position] = NULL;

    // for (int i = 0; tokens[i] != NULL; i++) {
    //   printf("args[%d]: %s\n", i, tokens[i]);
    // }

    return tokens;
}

// Execute the parallel command
int my_parallel_execute(char **args) {
    char **commands[100];
    int j = 0;
    int m = 0;
    char **sub_command = malloc(100 * sizeof(char*));
    for (int i = 0; args[i] != NULL; i++) {
        // printf("args[i]: %s\n", args[i]);
        sub_command[m] = args[i];
        m++;
        if (strcmp(args[i], "&") == 0) {
            sub_command[m-1] = NULL;
            commands[j] = sub_command;
            j++;
            m = 0;
            sub_command = malloc(100 * sizeof(char*));
        }

        if (args[i+1] == NULL) {
            sub_command[m] = NULL;
            commands[j] = sub_command;
            commands[j+1] = NULL;
        }
    }

    // for (int i = 0; commands[i] != NULL; i++) {
    //   for (int j = 0; commands[i][j] != NULL; j++) {
    //     printf("commands[%d][%d]: %s\n", i, j, commands[i][j]);
    //   }
    // }

    int h = 0;
    pid_t pids[10];
    while (commands[h] != NULL) {
        char **command = commands[h];

        if (!execute_builtin(args)) {
            continue;
        }

        char *output_file = NULL;
        int redirect = 0;
        int correct_redirect = 1;
        for (int i = 0; command[i] != NULL; i++) {
            if (strcmp(command[i], ">") == 0) {
                if (command[i + 2] != NULL || strcmp(command[0] , ">") == 0 || command[i + 1] == NULL){
                    correct_redirect = 0;
                }
                output_file = command[i + 1];
                command[i] = NULL;
                redirect = 1;
            }
        }

        // for (int i = 0; command[i] != NULL; i++) {
        //   printf("command: %s\n", command[i]);
        // }

        pid_t pid = fork(); // chmod 777 p1.sh
        pid_t wpid;

        if (pid == 0) {
            // printf("correct_redirect: %d\n", correct_redirect);

            if (correct_redirect == 0) {
                print_error();
                exit(0);
            }


            if (redirect) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, 1);
                if (execvp(command[0], command) == -1) {
                    print_error();
                    exit(EXIT_FAILURE);
                }
                close(fd);
            } else if (execvp(command[0], command) == -1) {
                print_error();
                exit(EXIT_FAILURE);
            }

            // else if (execvp(command[0], command) == -1) {
            //   perror("lsh");
            // }
        } else if (pid < 0) {
            perror("lsh");
        } else {
            pids[h] = pid;
        }
        h++;
    }


    for (int i = 0; i < h; i++) {
        waitpid(pids[i], NULL, 0);
    }

}

// Execute the given arguments in the child process
int my_execute(char **args) {

    char *output_file = NULL;
    int redirect = 0;
    int correct_redirect = 1;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 2] != NULL || strcmp(args[0] , ">") == 0 || args[i + 1] == NULL){
                correct_redirect = 0;
            }
            output_file = args[i + 1];
            args[i] = NULL;
            redirect = 1;
        }
    }

    pid_t pid, wpid;
    pid = fork();

    // printf("pid: %d\n", pid);

    if (pid == 0) {
        // printf("correct_redirect: %d\n", correct_redirect);
        if (correct_redirect == 0) {
            print_error();
            exit(0);
        }

        // if (execvp(args[0], args) == -1) {
        //   print_error();
        //   exit(EXIT_FAILURE);
        // }

        if (redirect) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, 1);
            if (execvp(args[0], args) == -1) {
                print_error();
                exit(EXIT_FAILURE);
            }
            close(fd);
        } else if (execvp(args[0], args) == -1) {
            print_error();
            exit(EXIT_FAILURE);
        }

    } else if (pid < 0) {
        perror("lsh");
    } else {
        wpid = waitpid(pid, NULL, 0);
        return 1;
    }

    return 0;
}

// command mode
void command_mode(){
    int status;
    char *line;
    char **args;
    do {
        int parallel = 0;
        printf("lsh> ");
        line = my_read_line();
        if (strcmp(line, "exit\n") == 0) {
            exit(0);
            break;
        }

        if (strstr(line, "&") != NULL) {
            parallel = 1;
        }

        args = my_args(line, delim);

        if (args[0] == NULL || strcmp(args[0], "exit") == 0 || strcmp(args[0], "cd") == 0 || strcmp(args[0], "path") == 0) {
            execute_builtin(args);
            continue;
        }

        if (parallel) {
            status = my_parallel_execute(args);
        } else {
            status = my_execute(args);
        }

        free(line);
        free(args);

    } while (status);

    exit(0);
}

// file mode
void file_mode(FILE *file) {
    char *line;
    char **args;
    int status;

    do{
        line = my_read_file(file);

        if (line == NULL) {
            break;
        }

        if (line == "\n") {
            continue;
        }

        int parallel = 0;
        if (strstr(line, "&") != NULL) {
            parallel = 1;
        }

        // printf("line: %s", line);

        args = my_args(line, delim);

        if (args[0] == NULL || strcmp(args[0], "exit") == 0 || strcmp(args[0], "cd") == 0 || strcmp(args[0], "path") == 0) {
            execute_builtin(args);
            continue;
        }

        if (parallel) {
            status = my_parallel_execute(args);
        } else {
            status = my_execute(args);
        }
        // printf("status: %d\n", status);

        free(line);
        free(args);

        if (status == 0){
            exit(0);
        }

    } while (1);

}

//main function: basically runs the shell
int main(int argc, char *argv[]) {
    if (argc == 1) {
        command_mode();
    } else if (argc == 2) {
        FILE *file = fopen(argv[1], "r");
        if (file == NULL) {
            print_error();
            exit(1);
        }

        file_mode(file);
        fclose(file);

    } else {
        print_error();
        exit(1);
    }

    return 0;
}