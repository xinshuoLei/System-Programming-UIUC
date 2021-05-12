// author: xinshuo3

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/wait.h>

void signal_handler(int signame) {
    // both program should exit when user hits CTRL-C
    if (signame == SIGINT) {
        kill(0, SIGKILL);
    }
}

void execute(char* argv[]) {
    pid_t pid = fork();
    if (pid == -1) {
        // error with fork
        fprintf(stderr, "fork error\n");
        exit(1);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
        // restart the buggy program if segfault
        if (WIFSIGNALED(status)) {
            if (WTERMSIG(status) == SIGSEGV) {
                execute(argv);
            }
        }
    } else {
        execvp(argv[1], argv + 1);
        // error in exec
        fprintf(stderr, "exec error\n");
        exit(1);
    }
    
}

int main(int argc, char* argv[]) {
    // handle SIGNINT
    signal(SIGINT, signal_handler);

    // TODO: check program exists before fork()
    
    // int end = 0;
    execute(argv);
    return 0;
}