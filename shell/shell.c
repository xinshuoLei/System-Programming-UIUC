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

void ps() {
    // mothing for now
}

size_t process_index(pid_t pid) {
    ssize_t index = -1;
    size_t i = 0;
    for (;i < vector_size(all_process); i++) {
        process* ptr = vector_get(all_process, i);
        if (ptr -> pid == pid) {
            index = i;
            break;
        }
    }
    return index;
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

    } 
}


int execute_external(char*cmd) {
    int background = 0;
    pid_t pid = fork();

    process* p = create_process(pid, cmd);
    vector_push_back(all_process, p);

    sstring* s = cstr_to_sstring(cmd);
    vector* splited = sstring_split(s, ' ');
    size_t size = vector_size(splited);
    char* first_string = vector_get(splited, 0);
    

    // check if it is a background process
    char* last_string = vector_get(splited, size - 1);
    if (size >= 2) {
       if (strcmp(last_string, "&") == 0) {
            background = 1;
            vector_set(splited, size - 1, NULL); 
        }
    }

    char* arr [size + 1];
    size_t i = 0;
    for (;i < size; i++) {
        arr[i] = vector_get(splited, i);
    }
    arr[size] = NULL;
    
    if (pid == -1) {
        print_fork_failed();
        exit_shell(1);
        return 1;
    }

    if (pid > 0) {
        print_command_executed(pid);
        int status = 0;
        if (background) {
            waitpid(pid, &status, WNOHANG);
        } else {
            pid_t pid_w = waitpid(pid, &status, 0);
            if (pid_w == -1) {
                print_wait_failed();
            } else if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) != 0) {
                    // did not exit with status 0
                    exit_shell(1);
                }
                fflush(stdout);
            } else if (WIFSIGNALED(status)) {

            }
            free(s);
            vector_destroy(splited);
            return status;
        }
    } else if (pid == 0) {
        if (background) {
            if (setpgid(getpid(), getpid()) == -1) {
                print_setpgid_failed();
                fflush(stdout);
                exit_shell(1);
            }
        }
        fflush(stdout);
        execvp(first_string, arr);
        // failed
        print_exec_failed(cmd);
        exit(1);
    }
    free(s);
    vector_destroy(splited);
    return 1;
}


int execute(char* cmd) {
    if ((strstr(cmd, "&&")) != NULL) {
        // and
    } else if ((strstr(cmd, "||")) != NULL) {
        // or
    } else if ((strstr(cmd, ";")) != NULL) {
        // separator
    }
    
    sstring* s = cstr_to_sstring(cmd);
    vector* splited = sstring_split(s, ' ');
    int valid_command = 0;
    size_t size = vector_size(splited);
    char* first_string = vector_get(splited, 0);


    if (size != 0) {
        if(strcmp(first_string, "cd") == 0) {
            // cd
            if (size > 1) {
                valid_command = 1;
                vector_push_back(all_history, cmd);
                int result = cd(vector_get(splited, 1));
                free(s);
                vector_destroy(splited);
                return result;
            }
        } else if (strcmp(first_string, "!history") == 0) {
            // !history
            if (size == 1) {
                valid_command = 1;
                for (size_t i = 0; i < vector_size(all_history); i++) {
                    print_history_line(i, vector_get(all_history, i));
                }
                free(s);
                vector_destroy(splited);
                return 0;
            }
        } else if (first_string[0] == '#') {
            // #
            if (size == 1 && strlen(first_string) != 1) {
                valid_command = 1;
                size_t index = atoi(first_string + 1);
                if (index < vector_size(all_history)) {
                    char* exec_cmd = vector_get(all_history, index);
                    print_command(exec_cmd);
                    free(s);
                    vector_destroy(splited);
                    return execute(exec_cmd);
                } 
                print_invalid_index();
                free(s);
                vector_destroy(splited);
                return 1;
            }
        } else if (first_string[0] == '!') {
            // !<prefix>
            if (size == 1) {
                valid_command = 1;
                char* prefix = first_string + 1;
                for (size_t i = vector_size(all_history); i > 0; --i) {
                    char* another_cmd = vector_get(all_history, i - 1);
                    if (strncmp(another_cmd, prefix, strlen(prefix)) == 0) {
                        print_command(another_cmd);
                        free(s);
                        vector_destroy(splited);
                        return execute(another_cmd);
                    }
                }
                print_no_history_match();
                free(s);
                vector_destroy(splited);
                return 1;
            }
        } else if (strcmp(first_string, "ps") == 0) {
            // ps
            vector_push_back(all_history, cmd);
            if (size == 1) {
                valid_command = 1;
            }
        } else if (strcmp(first_string, "kill") == 0) {
            // kill
            vector_push_back(all_history, cmd);
            if (size == 2) {
                valid_command = 1;
                pid_t target = atoi(vector_get(splited, 1));
                ssize_t index = process_index(target);
                if (index == -1) {
                    print_no_process_found(target);
                    free(s);
                    vector_destroy(splited);
                    return 1;
                }
                kill(target, SIGKILL);
                print_killed_process(target, ((process*) vector_get(all_process, index)) -> command);
                free(s);
                vector_destroy(splited);
                return 0;
            }
        } else if (strcmp(first_string, "stop") == 0) {
            // stop
            vector_push_back(all_history, cmd);
            if (size == 2) {
                valid_command = 1;
                pid_t target = atoi(vector_get(splited, 1));
                ssize_t index = process_index(target);
                if (index == -1) {
                    print_no_process_found(target);
                    free(s);
                    vector_destroy(splited);
                    return 1;
                }
                kill(target, SIGSTOP);
                print_stopped_process(target, ((process*) vector_get(all_process, index)) -> command);
                free(s);
                vector_destroy(splited);
                return 0;
            }
        } else if (strcmp(first_string, "cont") == 0) {
            // cont
            vector_push_back(all_history, cmd);
            if (size == 2) {
                valid_command = 1;
                pid_t target = atoi(vector_get(splited, 1));
                ssize_t index = process_index(target);
                if (index == -1) {
                    print_no_process_found(target);
                    free(s);
                    vector_destroy(splited);
                    return 1;
                }
                kill(target, SIGCONT);
                print_continued_process(target, ((process*) vector_get(all_process, index)) -> command);
                free(s);
                vector_destroy(splited);
                return 0;
            }
        } else if (strcmp(first_string, "exit") == 0) {
            // exit
            if (size == 1) {
                valid_command = 1;
                exit_shell(0);
            }
        } else {
            // external;
            vector_push_back(all_history, cmd);
            valid_command = 1;
            free(s);
            vector_destroy(splited);
            fflush(stdout);
            return execute_external(cmd);
        }
       
    }

    if (valid_command == 0) {
        print_invalid_command(cmd);
    }
    free(s);
    vector_destroy(splited);
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

    // create shell process and store in process vector
    process* shell = create_process(pid, argv[0]);
    all_process = shallow_vector_create();
    vector_push_back(all_process, shell);
    
    // history vector
    all_history = string_vector_create();


    char* cwd = malloc(256);
    input = stdin;
    if (getcwd(cwd, 256) == NULL) {
        exit_shell(1);
    }
    print_prompt(getcwd(cwd, 256), pid);

    int argument;
    while((argument = getopt(argc, argv, "f:h:")) != -1) {
        if (argument == 'h') {
            // load history and store name of the history file
            history_file = history_handler(optarg);
        } else if (argument == 'f') {
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
