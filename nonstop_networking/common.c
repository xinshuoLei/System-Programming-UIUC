/**
 * nonstop_networking
 * CS 241 - Spring 2021
 */
#include "common.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// code from charming_chatroom, slightly modified
ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {
    // Your Code Here
    size_t word_count = 0;
    while (word_count < count) {
        ssize_t result = read(socket, buffer + word_count, count - word_count);
        // no more bytes to read
        if (result == 0) {
            break;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        } else if (result == -1) {
            perror("read from socket");
            return -1;
        }
        word_count += result;
    }
    return word_count;
}

// code from charming_chatroom, slightly modified
ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    // Your Code Here
    size_t word_count = 0;
    while (word_count < count) {
        ssize_t result = write(socket, buffer + word_count, count - word_count);
        // no more bytes to read
        if (result == 0) {
            break;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        } else if (result == -1) {
            perror("write to socket");
            return -1;
        }
        word_count += result;
    }
    return word_count;
}
