/**
 * nonstop_networking
 * CS 241 - Spring 2021
 */
#include "format.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <sys/stat.h>

#include "common.h"

char **parse_args(int argc, char **argv);
verb check_args(char **args);

void read_response(char **args, int socket_result, verb method) {
    char* ok = "OK\n";
    char* err = "ERROR\n";
    char* response = calloc(1,strlen(ok)+1);
    size_t num_read = read_all_from_socket(socket_result, response, strlen(ok));
    // if response is "OK"
    if (strcmp(response, ok) == 0) {
        // fprintf(stdout, "%s", response);
        // respone from DELETE and PUT is either "OK\n" or "ERROR\n"
        if (method == DELETE || method == PUT) {
            print_success();
        } else if (method == GET) {
            char* local_filename = args[4];
            FILE *local_file = fopen(local_filename, "w+");
            if (local_file == NULL) {
                perror("fopen");
                exit(-1);
            }
            // get size of file
            size_t size;
            read_all_from_socket(socket_result, (char*) &size, sizeof(size_t));
            // write to local file
            char buffer[size + 1024];
            size_t read_count = read_all_from_socket(socket_result, buffer, size + 1024);
            fwrite(buffer, 1, read_count, local_file);
            // check for possible error
            if (read_count == 0 && read_count != size) {
                print_connection_closed();
                exit(-1);
            } else if (read_count < size) {
                print_too_little_data();
                exit(-1);
            } else if (read_count > size) {
                print_received_too_much_data();
                exit(-1);
            }
            fclose(local_file);
        } else if (method == LIST) {
            size_t size;
            read_all_from_socket(socket_result, (char*)&size, sizeof(size_t));
            char buffer[size + 6];
            // avoid memory error
            memset(buffer, 0, size + 6);
            size_t num_read = read_all_from_socket(socket_result, buffer, size + 5);
            //error detect
            if (num_read == 0 && num_read != size) {
                print_connection_closed();
                exit(-1);
            } else if (num_read < size) {
                print_too_little_data();
                exit(-1);
            } else if (num_read > size) {
                print_received_too_much_data();
                exit(-1);
            }
            fprintf(stdout, "%zu%s", size, buffer);
        }
    } else {
        // attempt to read a length of ERROR
        response = realloc(response, strlen(err) + 1);
        read_all_from_socket(socket_result, response + num_read, strlen(err) - num_read);
        // response is "ERROR\n"
        if (strcmp(response, err) == 0) {
            // fprintf(stdout, "%s", response);
            char error_message[100];
            size_t num_error_read = read_all_from_socket(socket_result, error_message, 100);
            if (num_error_read == 0) {
                print_connection_closed();
                print_error_message(error_message);
            }
        } else {
            print_invalid_response();
        }
    }
    free(response);
}



int main(int argc, char **argv) {
    // Good luck!
    char** args = parse_args(argc, argv);
    verb method  = check_args(args);
    char* host = args[0];
    char* port = args[1];

    // connect to server
    int socket_result = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_result == -1) {
        perror("socket error");
        exit(1);
    }

    struct addrinfo hints;
    struct addrinfo* result; 
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    int info_result = getaddrinfo(host, port, &hints, &result);
    if (info_result != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(info_result));
        exit(1);
    }

    int connect_result = connect(socket_result, result->ai_addr, result->ai_addrlen);
    if (connect_result == -1) {
        perror("connect error");
        exit(1);
    }

    freeaddrinfo(result);

    // write request
    char* request_buffer;
    if (method == LIST) {
        request_buffer = "LIST\n";
    } else {
        char* method_char = args[2];
        char* arg_char = args[3];
        request_buffer = malloc(strlen(method_char) + strlen(arg_char) + 3);
        sprintf(request_buffer, "%s %s\n", method_char, arg_char);
    }
    size_t num_request_write = write_all_to_socket(socket_result, request_buffer, strlen(request_buffer));
    if (num_request_write < strlen(request_buffer)) {
        print_connection_closed();
        exit(-1);
    }
    if (method != LIST) {
        free(request_buffer);
    }

    // opt based on method
    if (method == GET) {
        // deal in read_response
    } else if (method == PUT) {
        char* filename = args[4];
        struct stat s;
        int file_status = stat(filename, &s);
        if (file_status == -1) {
            exit(-1);
        }
        size_t file_size = s.st_size;
        write_all_to_socket(socket_result, ((char*) &file_size), sizeof(size_t));
        // write filedata
        FILE* local_file = fopen(filename, "r");
        if (local_file == NULL) {
            perror("fopen");
            exit(-1);
        }
        size_t put_count = 0;
        // read and write with buffer size of 1024
        while (put_count < file_size) {
            size_t buffer_size = 0;
            if (file_size - put_count > 1024) {
                buffer_size = 1024;
            } else {
                buffer_size = file_size - put_count;
            }
            char put_buffer[buffer_size];
            fread(put_buffer, 1, buffer_size, local_file);
            size_t num_write = write_all_to_socket(socket_result, put_buffer, buffer_size);
            if (num_write < buffer_size) {
                print_connection_closed();
                exit(-1);
            }
            put_count += buffer_size;
        }
        fclose(local_file);
    } else if (method == LIST) {

    } else if (method == DELETE) {
        // deal in read_response
    }
    // finishi writing to server, shutdown WRITE
    int shut_result = shutdown(socket_result, SHUT_WR);
    if (shut_result != 0) {
        perror("shutdown");
    }
    read_response(args, socket_result, method);
    // shutdown READ
    shutdown(socket_result, SHUT_RD);
    // free stuff
    close(socket_result);
    free(args);
}

/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL) {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp) {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3) {
        args[3] = argv[3];
    }
    if (argc > 4) {
        args[4] = argv[4];
    }

    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args) {
    if (args == NULL) {
        print_client_usage();
        exit(1);
    }

    char *command = args[2];

    if (strcmp(command, "LIST") == 0) {
        return LIST;
    }

    if (strcmp(command, "GET") == 0) {
        if (args[3] != NULL && args[4] != NULL) {
            return GET;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "DELETE") == 0) {
        if (args[3] != NULL) {
            return DELETE;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "PUT") == 0) {
        if (args[3] == NULL || args[4] == NULL) {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}
