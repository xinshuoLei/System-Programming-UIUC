/**
 * extreme_edge_cases
 * CS 241 - Spring 2021
 */
#include "camelCaser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

char **camel_caser(const char *input_str) {
    // if input is a null pointer
    if (!input_str) {
        return NULL;
    }

    //count number of sentences
    int num_sentence = 0;
    int i = 0;

    while(input_str[i]) {
        if (ispunct(input_str[i])) {
            num_sentence++;
        }
        i++;
    }


    // allocate space for result and set last element to null
    char** result = malloc((num_sentence + 1) * sizeof(char*));
    result[num_sentence] = NULL;

    // allocate space for each sentence. 
    i = 0;
    int j = 0;
    int num_characters = 0;

    while(input_str[i]) {
        if (ispunct(input_str[i])) {
            result[j] = malloc((num_characters + 1) * sizeof(char));
            result[j][num_characters] = '\0';
            num_characters = 0;
            j++;
        } else {
            if (!isspace(input_str[i])) {
                num_characters++;
            }
        }
        i++;
    }

    // fill in result
    i = 0;
    j = 0;
    int char_count = 0;
    int cap = 0;
    int first_char = 1;
    char c;

    while(input_str[i]) {
        // reached end of result
        if (j == num_sentence) {
            break;
        }
        if (ispunct(input_str[i])) {
            // start a new sentence
            char_count = 0;
            j++;
            first_char = 1;
            cap = 0;
        } else if (isspace(input_str[i])) {
            cap = 1;
        } else {
            c = input_str[i];
            if (isalpha(c)) {
                if (cap && !first_char) {
                    c = toupper(input_str[i]);
                    cap = 0;
                } else {
                    c = tolower(input_str[i]);
                    cap = 0;
                }
            }
            if (!isalpha(c) && first_char) {
                cap = 0;
            }
            first_char = 0;
            result[j][char_count] = c;
            char_count++;
        }
        i++;
    }
    return result;
}

void destroy(char **result) {
    // TODO: Implement me!
    int k = 0;
    while (result[k]) {
        free(result[k]);
        result[k] = NULL;
        k++;
    }
    free(result);
    result = NULL;
    return;
}
