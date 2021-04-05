/**
 * parallel_make
 * CS 241 - Spring 2021
 */

#include "format.h"
#include "graph.h"
#include "parmake.h"
#include "parser.h"
#include "vector.h"
#include "queue.h"
#include "dictionary.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>

// state = -1 means fails
// state = -3 means cycle detected

// otherwise state means num of unsatisfied dependencies
// state = 0 means satisfied
 



// RAG
graph* g;
// vector of all rules
queue* rules;
// ptheard related
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
// num of finished threads
size_t finished_thread = 0;


int cycle_detection_helper(void* goal, dictionary* visited) {
    if (!dictionary_contains(visited, goal)) {
        return 0;
    }
    // already visited and is in progress, cycle detected
    if(*(int *)dictionary_get(visited, goal) == 1) {
        return 1;
    }
    // vertex is finished
    if(*(int *)dictionary_get(visited, goal) == 2) {
        return 2;
    }  
    int one = 1;
    // mark this vertex as visited
    dictionary_set(visited, goal, &one);
    // check all neighbors
    vector* neighbors = graph_neighbors(g, goal);
    size_t i = 0;
    for (; i < vector_size(neighbors); i++) {
        void* curr = vector_get(neighbors, i);
        // cycle detected
        if (cycle_detection_helper(curr, visited) == 1) {
            vector_destroy(neighbors);
            return 1;
        }
    }
    // mark vertex as finished
    int two = 2;
    dictionary_set(visited, goal, &two);
    vector_destroy(neighbors);
    return 0;
}


int cycle_detection(void* goal) {
    // initialize
    dictionary* visited = string_to_int_dictionary_create();
    int zero = 0;
    size_t i = 0;
    // initialize dictionary, set all vertices to unvisited
    vector* vertices = graph_vertices(g);
    for (; i < vector_size(vertices); i++) {
        void* curr = vector_get(vertices, i);
        dictionary_set(visited, curr, &zero);
    }
    vector_destroy(vertices);
    int cycle_exist = cycle_detection_helper(goal, visited);
    dictionary_destroy(visited);
    return cycle_exist;
}

void failure_resolve(void* rule_vertex) {
    vector* parents = graph_antineighbors(g, rule_vertex);
    size_t j = 0;
    for (; j < vector_size(parents); j++) {
        void* curr = vector_get(parents, j);
        if (strcmp(curr, "") == 0) {
            finished_thread++;
            pthread_cond_signal(&cv);
        } else {
            failure_resolve(curr);
        }
    }
    vector_destroy(parents);
}


void* run_rule(void* place_holder /** to satisfiy the requirement of pthread */) {
    while (1) {
        void* rule_vertex = queue_pull(rules);
        // flag for whether to run the command
        // stop if queue is empty
        if (rule_vertex == NULL) {
            break;
        }
        rule_t* rule = (rule_t*) graph_get_vertex_value(g, rule_vertex);
        int should_run = 0;
        struct stat stat_rule;
        // successfully read file's stat
	    if (stat(rule -> target, &stat_rule) != -1) {
            // get dependencies
            pthread_mutex_lock(&lock);
            vector* dependencies = graph_neighbors(g, rule_vertex);
            pthread_mutex_unlock(&lock);
            // check modified time of all dependencies
            size_t i = 0;
            for (; i < vector_size(dependencies); i++) {
                void* depend = vector_get(dependencies, i);
                struct stat stat_depend;
                rule_t* depend_ptr =  (rule_t*)graph_get_vertex_value(g, depend);
                if (stat(depend_ptr -> target, &stat_rule) == -1 /** not a file */ 
                    || (difftime(stat_rule.st_mtime, stat_depend.st_mtime) < 0)) {
                    should_run = 1;
                    break;
                }   
            } 
            vector_destroy(dependencies);
        } else {
            // not a file
            should_run = 1;
        }
        int failed = 0;
        // run commands
        if (should_run) {
            vector* commands = rule -> commands;
            size_t j = 0;
            for(; j < vector_size(commands); j++) {
                // execution failed
                if (system((char *)vector_get(commands, j)) != 0) {
                    // set the state of current rule to failed
                    failed = 1;
                    break;
	            }
            }
        }
        if (failed) {
            pthread_mutex_lock(&lock);
            failure_resolve(rule_vertex);
            pthread_mutex_unlock(&lock);
        }
        pthread_mutex_lock(&lock);
        vector* parents = graph_antineighbors(g, rule_vertex);
        size_t k = 0;
        for (; k < vector_size(parents); k++) {
            void* curr = vector_get(parents, k);
            rule_t* curr_ptr = graph_get_vertex_value(g, curr);
            if (!failed) {
                // decrease num of unsatisfied dependencies
                curr_ptr -> state -= 1;
                // all dependecies satisified
                if (curr_ptr -> state == 0) {
                    queue_push(rules, curr);
                }
                // if current one is root
                if (strcmp(curr, "") == 0) {
                    finished_thread++;
                    pthread_cond_signal(&cv);
                }
            } 
        }
        pthread_mutex_unlock(&lock);
        vector_destroy(parents);
    }
    return NULL;
}

/**
int check_status(void* rule_vertex) {
    vector* dependencies = graph_neighbors(g, rule_vertex);
    rule_t* curr_rule = (rule_t*) graph_get_vertex_value(g, rule_vertex);
    if (vector_size(dependencies) == 0) {
        // rule is not a file, execute commands
        if (access(rule_vertex, F_OK) == -1) {
            int failed = 0;
            vector* commands = curr_rule -> commands;
            size_t j = 0;
            for(; j < vector_size(commands); j++) {
                // execution failed
                if (system((char *)vector_get(commands, j)) != 0) {
                    // set the state of current rule to failed
                    curr_rule -> state = -1;
                    failed = 1;
                    break;
	            }
            }
            if (failed) {
                vector_destroy(dependencies);
                return -1;
            }
            // set the state of current rule to satisfied
            curr_rule -> state = 10;
            vector_destroy(dependencies);
            return 1;
        }
    } else {
         // if rule_vertex is file
        if (access(rule_vertex, F_OK) != -1) {
            size_t j = 0;
            for (; j < vector_size(dependencies); j++) {
                void *depend = vector_get(dependencies, j);
                // if dependency is also a file
                if (access(depend, F_OK) != -1) {
                    struct stat stat_rule;
                    struct stat stat_depend;
                    // failed to read file's stat
	                if (stat((char *)rule_vertex, &stat_rule) == -1 || stat(depend, &stat_depend) == -1) {
                        vector_destroy(dependencies);
                        return -1;
                    }   
                    // if dependency is newer than target, run command
	                if (difftime(stat_rule.st_mtime, stat_depend.st_mtime) < 0) {
                        int failed = 0;
                        vector* commands = curr_rule -> commands;
                        j = 0;
                        for(; j < vector_size(commands); j++) {
                            // execution failed
                            if (system((char *)vector_get(commands, j)) != 0) {
                                failed = 1;
                                break;
                            }
                        }
                        if (failed) {
                            vector_destroy(dependencies);
                            return -1;
                        }
                        vector_destroy(dependencies);
                        return 1;
                    }
                }
            }
        } else {
            // rule_vertex is not a file, check dependencies status
            size_t j = 0;
            for(; j < vector_size(dependencies); j++) {
                void* current = vector_get(dependencies, j);
                rule_t* t = (rule_t*) graph_get_vertex_value(g, current);
                if (t -> state == -1) {
                    curr_rule -> state = -1;
                }
                if (t -> state != 10) {
                    int result = check_status(current);
                    if (result == -1) {
                        // set the state of current rule to failed
                        curr_rule -> state = -1;
                        vector_destroy(dependencies);
                        return -1;
                    }
                }
               
            }
            // all dependencies sastisfied, run commands
            if (curr_rule -> state == -1) {
                vector_destroy(dependencies);
                return -1;
            }
            int failed = 0;
            vector* commands = curr_rule -> commands;
            j = 0;
            for(; j < vector_size(commands); j++) {
                // execution failed
                if (system((char *)vector_get(commands, j)) != 0) {
                    failed = 1;
                    break;
                }
            }
            if (failed) {
                // set the state of current rule to failed
                curr_rule -> state = -1;
                vector_destroy(dependencies);
                return -1;
            }
            vector_destroy(dependencies);
            // set the state of current rule to satisfied
            curr_rule -> state = 10;
            return 1;
        }
    }
    
    vector_destroy(dependencies);
    
    return 0;
}
*/

void add_to_queue_helper(dictionary* in_queue, vector* rules_vec) {
    size_t i = 0;
    // iterate through all goals, push goal and its dependencies to queue
    for (; i < vector_size(rules_vec); i++) {
        void* goal = vector_get(rules_vec, i);
        // get all its dependencies
        vector* dependencies = graph_neighbors(g, goal);
        // set state
        rule_t* goal_ptr = (rule_t*)graph_get_vertex_value(g, goal);
        goal_ptr -> state = vector_size(dependencies);
        // only add rules without any dependencies (i.e. can be excuted immediately)
        if (vector_size(dependencies) == 0) {
            // currently not in queue
            if ((*(int*)dictionary_get(in_queue, goal)) == 0) {
                // push into queue and set in queue to 1
                queue_push(rules, goal);
                int one = 1;
                dictionary_set(in_queue, goal, &one);
            }
        } else {
            // recursion step
            add_to_queue_helper(in_queue, dependencies);
        }
        vector_destroy(dependencies);
    }
}

void add_to_queue(vector* goals) {
    dictionary* in_queue = string_to_int_dictionary_create();
    vector* vertices = graph_vertices(g);
    size_t i = 0;
    int zero = 0;
    for (; i < vector_size(vertices); i++) {
        // initially all rules are not in queue
        void* v = vector_get(vertices, i);
        dictionary_set(in_queue, v, &zero);
    }
    add_to_queue_helper(in_queue, goals);
    // free stuff
    dictionary_destroy(in_queue);
    vector_destroy(vertices);
}



int parmake(char *makefile, size_t num_threads, char **targets) {
    // good luck!

    // initialize
    rules = queue_create(-1);
    pthread_t pids[num_threads];

    // parse makefile and get goals
    g = parser_parse_makefile(makefile, targets);
    vector* goals = graph_neighbors(g, "");
    size_t num_goals = vector_size(goals);
    rule_t* root = graph_get_vertex_value(g, "");
    root -> state = num_goals;

    // check each goal, see if there is a cycle
    size_t i = 0;
    size_t no_cycle = num_goals;
    for (; i < num_goals; i++) {
        void* curr_goal = vector_get(goals, i);
        if (cycle_detection(curr_goal)) {
            // cycle detected, goal fails
            print_cycle_failure((char*) curr_goal);
            rule_t* curr_rule = (rule_t*) graph_get_vertex_value(g, curr_goal);
            curr_rule -> state = -3;
            no_cycle--;
        }
    }
    ssize_t k = num_goals - 1;
    for (; k >= 0; k--) {
        void* curr_goal = vector_get(goals, k);
        rule_t* curr_rule = (rule_t*) graph_get_vertex_value(g, curr_goal);
        if (curr_rule -> state == -3) {
            vector_erase(goals, k);
        }
    }

    // all goals have cycle, finish execution
    if (no_cycle == 0) {
        graph_destroy(g);
        vector_destroy(goals);
        return 0;
    }

    // add rules to queue
    add_to_queue(goals);

    // create threads
    i = 0;
    for (; i < num_threads; i++) {
        pthread_create(pids + i, NULL, run_rule, NULL);
    }

    /**
    i = 0;
    for (; i < num_goals; i++) {
        void* curr_goal = vector_get(goals, i);
        rule_t* curr_rule = (rule_t*) graph_get_vertex_value(g, curr_goal);
        // rule satisfies, run command
        if (curr_rule -> state != -3) {
            check_status(curr_goal);
        }
    }
    */
    
    pthread_mutex_lock(&lock);
    // wait till all threads finish
    while (finished_thread != num_goals) {
        pthread_cond_wait(&cv, &lock);
    }
    pthread_mutex_unlock(&lock);
    // use NULL to indicate end of queue
    i = 0;
    for (; i < num_threads + 1; i++) {
        queue_push(rules, NULL);
    }
    i = 0;
    for (; i < num_threads; i++) {
        pthread_join(pids[i], NULL);
    }

    vector_destroy(goals);
    graph_destroy(g);
    queue_destroy(rules);
    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&lock);

    return 0;
}

