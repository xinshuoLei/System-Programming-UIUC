/**
 * shell
 * CS 241 - Spring 2021
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include <signal.h>


typedef struct process {
    char *command;
    pid_t pid;
} process;

void signal_handler(int sig_name) {
    if(sig_name == SIGINT) {

    } else if (sig_name == SIGCHLD) {

    }
}

int shell(int argc, char *argv[]) {
    // TODO: This is the entry point for your shell.
    signal(SIGINT, signal_handler);
    return 0;
}
