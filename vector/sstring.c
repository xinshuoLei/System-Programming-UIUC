/**
 * vector
 * CS 241 - Spring 2021
 */
#include "sstring.h"
#include "vector.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>

struct sstring {
    // Anything you want
    char* s;
};

sstring *cstr_to_sstring(const char *input) {
    // your code goes here
    sstring* ptr = malloc(sizeof(sstring));
    ptr -> s = strdup(input);
    return ptr;
}

char *sstring_to_cstr(sstring *input) {
    // your code goes her
    assert(input);
    return strdup(input -> s);
}

int sstring_append(sstring *this, sstring *addition) {
    // your code goes here
    if (this && addition) {
        this -> s = realloc(this -> s, strlen(this -> s) + strlen(addition -> s) + 1);
        strcat(this -> s, addition -> s);
        return (strlen(this -> s));
    } else if (this) {
        return strlen(this -> s);
    } else if (addition) {
        return strlen(addition -> s);
    }
    return -1;
}

vector *sstring_split(sstring *this, char delimiter) {
    // your code goes here
    assert(this);
    vector* v = string_vector_create();
    char* itr = this -> s;
    char* start = this -> s;
    while(*itr) {
        if (*itr == delimiter) {
            *itr = '\0';
            vector_push_back(v, start);
            start = itr + 1;
            *itr = delimiter;
        }
        itr++;
    }
    vector_push_back(v, start);
    return v;
}

int sstring_substitute(sstring *this, size_t offset, char *target,
                       char *substitution) {
    // your code goes here
    assert(this);
    char* first_occurence = strstr(this -> s + offset, target);
    if (first_occurence) {
        char* temp = malloc(strlen(this -> s) + strlen(substitution) - strlen(target) + 1);
        int before_target = first_occurence - (this -> s);
        char* sub_in_temp = temp + before_target;
        char* remain_in_temp = sub_in_temp + strlen(substitution);
        char* remain = first_occurence + strlen(target);
        strncpy(temp, this -> s, before_target);
        strcpy(sub_in_temp, substitution);
        strcpy(remain_in_temp, remain);
        free(this -> s);
        this -> s = temp;
        return 0;
    }
    return -1;
}

char *sstring_slice(sstring *this, int start, int end) {
    // your code goes here
    char* slice = malloc(end - start + 1);
    strncpy(slice, this -> s + start, end - start);
    slice[end - start] = '\0';
    return slice;
}

void sstring_destroy(sstring *this) {
    // your code goes here
    assert(this);
    if (this -> s) {
        free(this -> s);
        this -> s = NULL;
    }
    free(this);
    this = NULL;
}
