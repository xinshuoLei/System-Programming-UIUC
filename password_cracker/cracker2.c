/**
 * password_cracker
 * CS 241 - Spring 2021
 */
#include "cracker2.h"
#include "format.h"
#include "utils.h"
#include "./includes/queue.h"
#include <pthread.h>
#include <string.h>
#include <crypt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static int finished_all = 0;
static int found_password = 0;
static char* result = NULL;
static int hash_count = 0;
static int num_thread = 0;

static char username[10];
static char hash[16];
static char known[16];

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

void* crack_password(void* input) {
    struct crypt_data cdata;
    cdata.initialized = 0;
    size_t index = (size_t) input;
    char* known_copy = malloc(16* sizeof(char));

    while (true) {
        // wait for main thread to read task, first barrier
        pthread_barrier_wait(&barrier);
        // stop if all tasks finished
        if (finished_all) {
            break;
        }

        long starting_position = 0;
        long range_count;
        int count = 0;
        int printed = 0;
        strcpy(known_copy, known);

        // get subrange and set statring position
        int prefix_length = getPrefixLength(known_copy);
        int unknown_length = strlen(known_copy) - prefix_length;
        getSubrange(unknown_length, num_thread, index, &starting_position, &range_count); 
        setStringPosition(known_copy + prefix_length, starting_position);
        // print info
        v2_print_thread_start(index, username, starting_position, known_copy);
        
        // finding solution
        long i = 0;
        for(; i < range_count; i++) {
            char* current_hash = crypt_r(known_copy, "xx", &cdata);
            count++;
            // current thread found solution
            if (strcmp(current_hash, hash) == 0) {
                pthread_mutex_lock(&lock);
                result = known_copy;
                found_password = 1;
                v2_print_thread_result(index, count, 0);
                hash_count += count; 
                pthread_mutex_unlock(&lock);
                printed = 1;
                break;
            }
            // other thread found solution, stop working
            pthread_mutex_lock(&lock);
            if (found_password) {
                v2_print_thread_result(index, count, 1);
                hash_count += count;
                pthread_mutex_unlock(&lock);
                printed = 1;
                break;
            }
            pthread_mutex_unlock(&lock);
            incrementString(known_copy + prefix_length);
        } 
        
        // finish range, found nothing
        if (!printed) {
            pthread_mutex_lock(&lock);
            v2_print_thread_result(index, count, 2);
            hash_count += count;
            pthread_mutex_unlock(&lock);
        }

        // waiting fro other thread, second barrier
        pthread_barrier_wait(&barrier); 
    }

    // free stuff
    free(known_copy);
    return NULL;
}

int start(size_t thread_count) {
    // TODO your code here, make sure to use thread_count!
    // Remember to ONLY crack passwords in other threads

    // initialize
    pthread_t tids[thread_count];
    pthread_barrier_init(&barrier, NULL, thread_count + 1);
    num_thread = thread_count;

    // create thread
    size_t i = 0;
    for(; i < thread_count; i++) {
        pthread_create(tids + i, NULL, crack_password, (void*) i + 1);
    }

    // getline to get all tasks
    size_t length = 0;
    char* buffer = NULL;
    while(getline(&buffer, &length, stdin) != -1) {
        // replace newline
        if (strlen(buffer) >= 1 && buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }
        // get corresponding parts
        sscanf(buffer, "%s %s %s", username, hash, known);  
        // print start info
        v2_print_start_user(username);

        // start time
        double start_time = getTime();
        double cpu_start_time = getCPUTime();

        // finish reading task and printing info.results, first barrier
        pthread_barrier_wait(&barrier);
        // idle
        // wait for worker threads, second barrier
        pthread_barrier_wait(&barrier);
        
        // get time and print result
        double total_time = getTime() - start_time;
        double total_cpu_time = getCPUTime() - cpu_start_time;
        if (found_password) {
            v2_print_summary(username, result, hash_count, total_time, total_cpu_time, 0);
        } else {
            v2_print_summary(username, result, hash_count, total_time, total_cpu_time, 1);
        }

        // reset global variables
        found_password = 0;
        hash_count = 0;
    }   

    // all tasks
    finished_all = 1;
    pthread_barrier_wait(&barrier);

    // wait for threads to finish
    i = 0;
    for (; i < thread_count; i++) {
      pthread_join(tids[i], NULL);
    }

    // free stuff
    if (buffer) {
        free(buffer);
    }
    pthread_mutex_destroy(&lock);
    pthread_barrier_destroy(&barrier);

    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally
}
