/**
 * shell
 * CS 241 - Spring 2021
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include "sstring.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <ctype.h>
#include <dirent.h> 

extern char *optarg;



static vector* all_process;
static vector* all_history;
static char* history_file = NULL;
static FILE* input = NULL;


typedef struct process {
    char *command;
    pid_t pid;
} process;

process* create_process(pid_t pid, char* comm) {
    process* ptr = malloc(sizeof(process));
    ptr -> pid = pid;
    ptr -> command = malloc(sizeof(char) * (strlen(comm) + 1));
    strcpy(ptr -> command, comm);
    return ptr;
}

void exit_shell(int status) {
    if (history_file != NULL) {
        FILE *history_fd = fopen(history_file, "w");
        for (size_t i = 0; i < vector_size(all_history); ++i) {
            fprintf(history_fd, "%s\n", (char*)vector_get(all_history, i));
        }
        fclose(history_fd);
    }
    if (all_history) {
        vector_destroy(all_history);
    }
    if (all_process) {
        vector_destroy(all_process);
    }
    if (input != stdin) {
        fclose(input);
    }
    exit(status);
}

char** string_split(const char* toSplit, const char* delimiter) {
    // code form lab 
    char *s = strdup(toSplit);
    size_t token_a = 1;
    size_t token_u = 0;
    char **tokens = malloc(token_a*sizeof(char*));
    char *token = s;
    char *token_start = s;
    while ((token = strsep(&token_start, delimiter))) {
        if (token_u == token_a) {
            token_a *= 2;
            tokens = realloc(tokens, token_a * sizeof(char*));
        }
        tokens[token_u++] = strdup(token);
    }
    if (token_u == 0) {
        free(token);
    } else {
        tokens = realloc(tokens, token_u * sizeof(char*));
    }
    free(s);
    return tokens;
}


char* history_handler(char*file) {
    FILE* fd = fopen(get_full_path(file), "r");
    if (fd) {
        char* buffer = NULL;
        size_t length = 0;
        while(getline(&buffer, &length, fd) != -1) {
            if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') {
                buffer[strlen(buffer) - 1] = '\0';
                vector_push_back(all_history, buffer);
            }
        }
        free(buffer);
        fclose(fd);
        return get_full_path(file);
    } else {
        // treat as empty. create file
        fd = fopen(file, "w");
    }
    fclose(fd);
    return get_full_path(file);
} 


int cd(char* dir) {
    int result = chdir(dir);
    if (result != 0) {
        print_no_directory(dir);
        return 1;
    }
    return 0;
}

void signal_handler(int sig_name) {
    if(sig_name == SIGINT) {

    } else if (sig_name == SIGCHLD) {

    }
}

int execute(char* cmd) {
    if ((strstr(cmd, "&&")) != NULL) {
        // and
    } else if ((strstr(cmd, "||")) != NULL) {
        // or
    } else if ((strstr(cmd, ";")) != NULL) {
        // separator
    }
    
    char** splited = string_split(cmd, " ");
    char** itr = splited;
    int save_history = 1;
    int valid_command = 0;
    size_t size = 0;

    while(*itr) {
        size++;
        itr++;
    }


    if (size != 0) {
        if(strcmp(splited[0], "cd") == 0) {
            // cd
            if (size > 1) {
                valid_command = 1;
                int result = cd(splited[1]);
                return result;
            }
        } else if (strcmp(splited[0], "!history") == 0) {
            // !history
            save_history = 0;
            if (size == 1) {
                valid_command = 1;
                for (size_t i = 0; i < vector_size(all_history); i++) {
                    print_history_line(i, vector_get(all_history, i));
                }
                return 0;
            }
        } else if (splited[0][0] == '#') {
            // #
            save_history = 0;
            if (size == 1 && strlen(splited[0]) != 1) {
                valid_command = 1;
                size_t index = atoi(splited[0] + 1);
                if (index < vector_size(all_history)) {
                    char* exec_cmd = vector_get(all_history, index);
                    print_command(exec_cmd);
                    return execute(exec_cmd);
                } 
                print_invalid_index();
                return 1;
            }
        } else if (splited[0][0] == '!') {
            // !<prefix>
            save_history = 0;
            if (size == 1) {
                valid_command = 1;
                char* prefix = splited[0] + 1;
                for (size_t i = vector_size(all_history); i > 0; --i) {
                    char* another_cmd = vector_get(all_history, i - 1);
                    if (strncmp(another_cmd, prefix, strlen(prefix)) == 0) {
                        print_command(another_cmd);
                        return execute(another_cmd);
                    }
                }
                print_no_history_match();
                return 1;
            }
        } else if (strcmp(splited[0], "ps") == 0) {
            // ps
            if (size == 1) {
                valid_command = 1;
            }
        } else if (strcmp(splited[0], "kill") == 0) {
            // kill
            if (size == 2) {
                valid_command = 1;
            }
        } else if (strcmp(splited[0], "stop") == 0) {
            // stop
            if (size == 2) {
                valid_command = 1;
            }
        } else if (strcmp(splited[0], "cont") == 0) {
            // cont
            if (size == 2) {
                valid_command = 1;
            }
        } else if (strcmp(splited[0], "exit") == 0) {
            // exit
            if (size == 1) {
                valid_command = 1;
                exit_shell(0);
                return 0;
            }
            save_history = 0;
        } else {
            // external;
            valid_command = 1;
            return 1;
        }
        
        if (save_history) {
            vector_push_back(all_history, cmd);
        }
    }

    if (!valid_command) {
        print_invalid_command(cmd);
    }
    
    return 1;
}



int shell(int argc, char *argv[]) {
    // TODO: This is the entry point for your shell.


    // check input
    if (!(argc == 1 || argc == 3 || argc == 5)) {
        print_usage();
        exit(1);
    }

    signal(SIGINT, signal_handler);
    int pid = getpid();
    process* shell = create_process(pid, argv[0]);
    all_process = shallow_vector_create();
    vector_push_back(all_process, shell);
    // path of the histroy file
    
    char* cwd = malloc(256);
    input = stdin;
    if (getcwd(cwd, 256) == NULL) {
        exit_shell(1);
    }
    print_prompt(getcwd(cwd, 256), pid);

    while(getopt(argc, argv, "f:h:") != -1) {
        if (getopt(argc, argv, "f:h:") == 'h') {
            history_file = history_handler(optarg);
        } else if (getopt(argc, argv, "f:h:") == 'f') {
            FILE* script = fopen(optarg, "r");
            if (script == NULL) {
                print_script_file_error();
                exit_shell(1);
            }
        } else {
            print_usage();
            exit_shell(1);
        }
    }
    char* buffer = NULL;
    size_t length = 0;
    while (getline(&buffer, &length, input) != -1) {
        if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) -1] = '\0';
        }
        char* copy = malloc(strlen(buffer));
        strcpy(copy, buffer);
        execute(copy);
        if (getcwd(cwd, 256) == NULL) {
            exit_shell(1);
        }
        print_prompt(getcwd(cwd, 256), pid);
        fflush(stdout);
    }
    free(buffer);
    exit_shell(0);
    return 0;
}
