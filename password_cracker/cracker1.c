/**
 * password_cracker
 * CS 241 - Spring 2021
 */
#include "cracker1.h"
#include "format.h"
#include "utils.h"
#include "./includes/queue.h"
#include <pthread.h>
#include <string.h>
#include <crypt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
// task queue
static queue* tasks;
// total number of tasks
static size_t num_tasks;
// number of cracked passwords
static int recovered_num;


void* crack_password(void* thread_num) {
    size_t index = (size_t) thread_num;
    char username[10];
    char hash[16];
    char known[16];
    struct crypt_data cdata;
    cdata.initialized = 0;
    char* task = NULL;
    while (true) {
        task = queue_pull(tasks);
        if (!task) {
            break;
        }
        // get corresponding parts
        sscanf(task, "%s %s %s", username, hash, known);
        // print starting info
        v1_print_thread_start(index, username);
        int prefix_length = getPrefixLength(known);
        // set to first unknown
        setStringPosition(prefix_length + known, 0);
        int hash_count = 0;
        double start_time = getThreadCPUTime();
        char* current_hash = NULL;
        int fail = 1;
        // finding solution
        while (1) {
            current_hash = crypt_r(known, "xx", &cdata);
            hash_count++;
            // found solution
            if (strcmp(current_hash, hash) == 0) {
                pthread_mutex_lock(&lock);
                recovered_num++;
                pthread_mutex_unlock(&lock);
                fail = 0;
                break;
            }
            // increment fail. cannot recover
            int result = incrementString(prefix_length + known);
            if (result == 0) {
                break;
            }
        }
        double time = getThreadCPUTime() - start_time;
        // print info and free stuff
        v1_print_thread_result(index, username, known, hash_count, time, fail);
        free(task);
        task = NULL;
    }
    return NULL;
}

int start(size_t thread_count) {
    // TODO your code here, make sure to use thread_count!
    // Remember to ONLY crack passwords in other threads
    
    // initialize 
    tasks = queue_create(100);
    pthread_t tids[thread_count];
    
    // getline to get all tasks
    size_t length = 0;
    char* buffer = NULL;
    while(getline(&buffer, &length, stdin) != -1) {
        // replace newline
        if (strlen(buffer) >= 1 && buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }   
        queue_push(tasks, strdup(buffer));
        num_tasks++;
    }

    size_t i = 0;
    for(; i < thread_count; i++) {
        queue_push(tasks, NULL);
    }

    // create threads
    i = 0;
    for(; i < thread_count; i++) {
        pthread_create(tids + i, NULL, crack_password, (void*) i + 1 /* index of thread is 1-indexed */);
    }

    // wait for threads
    i = 0;
    for(; i < thread_count; i++) {
        pthread_join(tids[i], NULL);
    }

    v1_print_summary(recovered_num, num_tasks - recovered_num);

    // free stuff
    free(buffer);
    buffer = NULL;
    queue_destroy(tasks);
    pthread_mutex_destroy(&lock);

    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally
}
