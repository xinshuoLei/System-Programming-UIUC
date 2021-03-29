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

// state = -1 means fails



// RAG
graph* g;
// vector of all rules
queue* rules;


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
            vector_destroy(commands);
            if (failed) {
                return -1;
            }
            // set the state of current rule to satisfied
            curr_rule -> state = 10;
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
                        vector_destroy(commands);
                        if (failed) {
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
            vector_destroy(commands);
            if (failed) {
                // set the state of current rule to failed
                curr_rule -> state = -1;
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

int parmake(char *makefile, size_t num_threads, char **targets) {
    // good luck!

    // parse makefile and get goals
    g = parser_parse_makefile(makefile, targets);
    vector* goals = graph_neighbors(g, "");
    size_t num_goals = vector_size(goals);

    // check each goal, see if there is a cycle
    size_t i = 0;
    for (; i < num_goals; i++) {
        void* curr_goal = vector_get(goals, i);
        if (cycle_detection(curr_goal)) {
            // cycle detected, goal fails
            print_cycle_failure((char*) curr_goal);
            vector_erase(goals, i);
            rule_t* curr_rule = (rule_t*) graph_get_vertex_value(g, curr_goal);
            curr_rule -> state = -1;
        }
    }

    // all goals have cycle, finish execution
    if (vector_empty(goals)) {
        return 0;
    }

    i = 0;
    for (; i < num_goals; i++) {
        void* curr_goal = vector_get(goals, i);
        check_status(curr_goal);
        // rule satisfies, run command
    }
    
    vector_destroy(goals);

    return 0;
}

