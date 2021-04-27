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
#include <sys/epoll.h>
#include "./includes/dictionary.h"
#include "./includes/vector.h"
#include "common.h"
#include <errno.h>
#include <dirent.h>

// name of the directory used for storing uploaded files
static char* dir;
static vector* files;
static dictionary* fd_to_connection_state;
static dictionary* server_file_size;
static int epoll_fd;

typedef struct client_info {
    /**
     * state = 0: ready to parse header 
     * state = 1: ready to execute command
     * state = -1: bad request
     * state = -2: bad file size
     * state = -3: no such file
     */ 
	int state;
	verb command;
    char header[1024];
	char filename[255];
} client_info;

void sigpipe_handler() {
    // do nothing when receive SIGPIPE
}

void close_client(client_fd) {
    // remove from epoll
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    // free memory
    free(dictionary_get(fd_to_connection_state, &client_fd));
    dictionary_remove(fd_to_connection_state, &client_fd);
}

void exit_server() {
    close(epoll_fd);
    vector_destroy(files);
	vector *infos = dictionary_values(fd_to_connection_state);
	VECTOR_FOR_EACH(infos, info, {
    	free(info);
	});
	vector_destroy(infos);
	dictionary_destroy(fd_to_connection_state);
    dictionary_destroy(server_file_size);
    // empty and delete dir
    DIR* d = opendir(dir);
    if (d != NULL) {
        struct dirent* ptr;
        while ((ptr = readdir(d))) {
          // skip "." amd ".."
          if (!strcmp(ptr -> d_name, ".") || !strcmp(ptr -> d_name, "..")) {
            continue;
          }
          char path[strlen(dir) + strlen(ptr -> d_name) + 1];
          sprintf(path, "%s/%s", dir, ptr -> d_name);
          int result = unlink(path);
          if (result != 0) {
              perror("remove file");
          }
      }
      closedir(d);
    } else {
        puts("fail");
    }
    rmdir(dir);
	exit(1);
}

int process_put(int client_fd) {
    client_info* info = dictionary_get(fd_to_connection_state, &client_fd);
    // construct file path
    char path[strlen(dir) + strlen(info -> filename) + 2];
    memset(path, 0, strlen(dir) + strlen(info -> filename) + 2);
    sprintf(path, "%s/%s", dir, info -> filename);
    FILE* fp_check = fopen(path, "r");
    if (fp_check == NULL) {
        vector_push_back(files, info->filename);
    } else {
        fclose(fp_check);
    }
    FILE* fp = fopen(path, "w");
    if (fp == NULL)  {
        perror("fopen in put");
        return 1;
    }
    // read filesize
    size_t file_size_;
    read_all_from_socket(client_fd, (char*) &file_size_, sizeof(size_t));
    size_t read_count = 0;
    // read and write with buffer size of 1024
    while (read_count < file_size_) {
        size_t buffer_size = 0;
        if (file_size_ - read_count > 1024) {
            buffer_size = 1024;
        } else {
            buffer_size = file_size_ - read_count;
        }
        char buffer[buffer_size];
        ssize_t num_read = read_all_from_socket(client_fd, buffer, buffer_size);
        if (num_read == 0) {
            break;
        }
        fwrite(buffer, 1, num_read, fp);
        read_count += num_read;
    }
    fclose(fp);
    if (read_count != file_size_) {
        remove(path);
        return 1;
    }
    dictionary_set(server_file_size, info -> filename, &file_size_);
    return 0;
}

void process_get(int client_fd) {
    client_info* info = dictionary_get(fd_to_connection_state, &client_fd);
    // construct file path
    char path[strlen(dir) + strlen(info -> filename) + 2];
    memset(path, 0, strlen(dir) + strlen(info -> filename) + 2);
    sprintf(path, "%s/%s", dir, info -> filename);
    // read and write in buffer size of 1024
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        // no such file
        info -> state = -3;
        return;
    }
    char* ok = "OK\n";
    write_all_to_socket(client_fd, ok, strlen(ok));
    // write file size
    size_t file_size_ = *((size_t*) dictionary_get(server_file_size, info -> filename));
    write_all_to_socket(client_fd, (char*) &file_size_, sizeof(size_t));
    // write content of file
    size_t get_count = 0;
    while (get_count < file_size_) {
        size_t buffer_size = 0;
        if (file_size_ - get_count > 1024) {
            buffer_size = 1024;
        } else {
            buffer_size = file_size_ - get_count;
        }
        char buffer[buffer_size];
        size_t num_read = fread(buffer, 1, buffer_size, fp);
        if (num_read == 0) {
            break;
        }
        write_all_to_socket(client_fd, buffer, num_read);
        get_count += buffer_size;
    }
    fclose(fp);
    close_client(client_fd);
}

void process_delete(int client_fd) {
    client_info* info = dictionary_get(fd_to_connection_state, &client_fd);
    // construct file path
    char path[strlen(dir) + strlen(info -> filename) + 2];
    memset(path, 0, strlen(dir) + strlen(info -> filename) + 2);
    sprintf(path, "%s/%s", dir, info -> filename);
    if (remove(path) == -1) {
        // np such file
        info -> state = -3;
        return;
    }
    // remove from files and server_file_size
    dictionary_remove(server_file_size, info -> filename);
    size_t i = 0;
    for (; i < vector_size(files); i++) {
        char* file = vector_get(files, i);
        if (strcmp(info -> filename, file) == 0) {
            break;
        }
    }
    if (i >= vector_size(files)) {
        info -> state = -3;
        return;
    }
    vector_erase(files, i);
    char* ok = "OK\n";
	write_all_to_socket(client_fd, ok, strlen(ok));
    close_client(client_fd);
}

void process_list(int client_fd) {
    char* ok = "OK\n";
	write_all_to_socket(client_fd, ok, strlen(ok));
    // no files
    if (vector_size(files) == 0) {
        return;
    }
    size_t responese_size = 0;
    size_t i = 0;
    for (; i < vector_size(files); i++) {
        char* curr_filename = vector_get(files, i);
        responese_size += strlen(curr_filename) + 1;
    }
    // prevent overflow
    if (responese_size >= 1) {
        responese_size--;
    }
    // write response size
    write_all_to_socket(client_fd, (char*) &responese_size, sizeof(size_t));
    // write list
    i = 0;
    for (; i < vector_size(files); i++) {
        char* curr_filename = vector_get(files, i);
        write_all_to_socket(client_fd, curr_filename, strlen(curr_filename));
        if (i != vector_size(files) - 1) {
            write_all_to_socket(client_fd, "\n", 1);
        }
    }
    close_client(client_fd);
}

void parse_header(int client_fd) {
    client_info* info = dictionary_get(fd_to_connection_state, &client_fd);
    // read from client_fd
    size_t read_count = 0;
    while (read_count < 1024) {
        if (info -> header[strlen(info -> header) - 1] == '\n') {
            break;
        }
        ssize_t result = read(client_fd, info -> header + read_count, 1);
        if (result == 0) {
            break;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        } else if (result == -1) {
            perror("read from client_fd");
            exit(1);
        }
        read_count += result;
    }
    if (strncmp(info->header, "PUT", 3) == 0) {
        // process PUT
		info -> command = PUT;
		strcpy(info->filename, info->header + strlen("PUT") + 1);
		info -> filename[strlen(info->filename)-1] = '\0';
        if (process_put(client_fd) == 1) {
            // bad file size
            info -> state = -2;
            info -> state = -1;
            struct epoll_event ev_client;
            ev_client.events = EPOLLOUT;
            ev_client.data.fd = client_fd;
            // change to read
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev_client);
            return;
        }
	} else if (strncmp(info->header, "GET", 3) == 0) {
        // process GET
		info -> command = GET;
		strcpy(info->filename, info->header + strlen("GET") + 1);
		info -> filename[strlen(info->filename) - 1] = '\0';
	} else if (strncmp(info->header, "DELETE", 6) == 0) {
        // process DELETE
		info -> command = DELETE;
		strcpy(info->filename, info->header + strlen("DELETE") + 1);
		info->filename[strlen(info->filename) - 1] = '\0';
	} else if (!strncmp(info->header, "LIST", 4)) {
        // process LIST
		info -> command = LIST;
	} else {
		print_invalid_response();
        // error in processing header
		info -> state = -1;
		struct epoll_event ev_client;
        ev_client.events = EPOLLOUT;
        ev_client.data.fd = client_fd;
        // change to read
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev_client);
        return;
	}
    // ready to execute command
	info -> state = 1;
	struct epoll_event ev_client;
    ev_client.events = EPOLLOUT;
    ev_client.data.fd = client_fd;
    // change to read
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev_client);
}

void process_client(int client_fd) {
    client_info* info = dictionary_get(fd_to_connection_state, &client_fd);
    int client_state = info -> state;
    if (client_state == 0) {
        // ready to parse header
        parse_header(client_fd);
    } else if (client_state == 1) {
        char* ok = "OK\n";
        if (info -> command == PUT) {
            write_all_to_socket(client_fd, ok, strlen(ok));
            close_client(client_fd);
        } else if (info -> command == GET) {
            process_get(client_fd);
        } else if (info -> command == DELETE) {
            process_delete(client_fd);
        } else if (info -> command == LIST) {
            process_list(client_fd);
        }
    } else { 
        // handle error
        char* err = "ERROR\n";
        write_all_to_socket(client_fd, err, strlen(err));
        if (client_state == -1) {
            // bad request       
            write_all_to_socket(client_fd, err_bad_request, strlen(err_bad_request));
        } else if (client_state == -2) {
            // bad file size
            write_all_to_socket(client_fd, err_bad_file_size, strlen(err_bad_file_size));
        } else if (client_state == -3) {
            // no such file
            write_all_to_socket(client_fd, err_no_such_file, strlen(err_no_such_file));
        }
        close_client(client_fd);
    }
}

void run_server(char* port) {
    // start using code from example server in the coursebook
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(1);
    } 

    // use SO_REUSEADDR and SO_REUSEPORT to ensure bind() doesn’t fail
    int optval = 1;
    int retval1 = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (retval1 == -1) {
        perror("setsockopt");
        exit(1);
    }
    int retval2 = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (retval2 == -1) {
        perror("setsockopt");
        exit(1);
    }

    if (bind(sock_fd, result->ai_addr, result->ai_addrlen) != 0) {
        perror("bind()");
        exit(1);
    }

    if (listen(sock_fd, 100) != 0) {
        perror("listen()");
        exit(1);
    }
    freeaddrinfo(result);
    // end using code from example server in the coursebook

    // epoll stuff
    // start using code about epoll in the coursebook
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(1);
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sock_fd;
    // Add the socket in level_triggered mode
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev) == -1) {
        perror("epoll_ctl: sock_fd");
        exit(1);
    }
    // array for all events
    struct epoll_event events[100];
    while (1) {
        // wait and see if epoll has any events
        int num_events = epoll_wait(epoll_fd, events, 100, -1);
        if (num_events == -1) {
            perror("epoll_wait");
            exit(1);
        }
        int i = 0;
        for (; i < num_events; i++) {
            // event on server socket, update epoll with a new client
            if (events[i].data.fd == sock_fd) {
                int conn_sock = accept(sock_fd, NULL, NULL);
                if (conn_sock < 0) {
                    perror("accept()");
                    exit(1);
                }
                struct epoll_event ev_conn;
                ev_conn.events = EPOLLIN;
                ev_conn.data.fd = conn_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &ev_conn);
                client_info* info = calloc(1, sizeof(client_info));
                // add to dictionary
                dictionary_set(fd_to_connection_state, &conn_sock, info);
            } else {
                // event on client socket
                process_client(events[i].data.fd);
            }
        }
    }
    // end using code about epoll in the coursebook
}

int main(int argc, char **argv) {
    // good luck!

    // chceck arguments
    if (argc != 2) {
        print_server_usage();
        exit(1);
    }

    // handle SIGINT and SIGPIPE
    signal(SIGPIPE, sigpipe_handler);

    struct sigaction sigint_act;
    memset(&sigint_act, '\0', sizeof(struct sigaction));
    sigint_act.sa_handler = exit_server;
    // change action when receieve SIGINT
    int sigaction_result = sigaction(SIGINT, &sigint_act, NULL);
    // check if sigaction fail
    if (sigaction_result != 0) {
        perror("sigaction");
        exit(1);
    }

    // make temporart directory for uploaded files
    char dirname[] = "XXXXXX";
    dir = mkdtemp(dirname);
    if (dir == NULL) {
        exit(1);
    }
    print_temp_directory(dir);

    // set up global data structures
    files = string_vector_create();
    fd_to_connection_state = int_to_shallow_dictionary_create();
    server_file_size = string_to_unsigned_long_dictionary_create();
    
    char* port = argv[1];
    run_server(port); 
}
