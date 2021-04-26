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

// name of the directory used for storing uploaded files
static char* dir;
static vector* files;
static dictionary* fd_to_connection_state;
static int epoll_fd;

typedef struct client_info {
    /**
     * state = 0: ready to parse header 
     * state = 1: ready to execute command
     */ 
	int state;
	verb command;
    size_t file_size;
    char header[1024];
	char filename[255];
} client_info;

void sigpipe_handler() {
    // do nothing when receive SIGPIPE
}

void exit_server() {
    close(epoll_fd);
    vector_destroy(files);
    remove(dir);
	vector *infos = dictionary_values(fd_to_connection_state);
	VECTOR_FOR_EACH(infos, info, {
    	free(info);
	});
	vector_destroy(infos);
	dictionary_destroy(fd_to_connection_state);
	exit(1);
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
        } else if (result == 01) {
            perror("read from client_fd");
            exit(1);
        }
        read_count += result;
    }
    if (strncmp(info->header, "PUT", 3) == 0) {
		info -> command = PUT;
		strcpy(info->filename, info->header + 4);
		info -> filename[strlen(info->filename)-1] = '\0';
	} else if (strncmp(info->header, "GET", 3) == 0) {
		info -> command = GET;
		strcpy(info->filename, info->header + 4);
		info -> filename[strlen(info->filename) - 1] = '\0';
	} else if (strncmp(info->header, "DELETE", 6) == 0) {
		info -> command = DELETE;
		strcpy(info->filename, info->header + 7);
		info->filename[strlen(info->filename) - 1] = '\0';
	} else if (!strncmp(info->header, "LIST", 4)) {
		info -> command = LIST;
	} else {
		print_invalid_response();
        // error in processing header
		info -> state = -1;
		struct epoll_event ev_client;
        ev_client.events = EPOLLOUT;
        ev_client.data.fd = client_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev_client);
	}
	info -> state = 1;
	struct epoll_event ev_client;
    ev_client.events = EPOLLOUT;
    ev_client.data.fd = client_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev_client);
}

void client_handler(int client_fd) {
    client_info* info = dictionary_get(fd_to_connection_state, &client_fd);
    int client_state = info -> state;
    if (client_state == 0) {
        parse_header(client_fd);
    }
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


    // run the server
    char* port = argv[1];
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
                ev.events = EPOLLIN;
                ev.data.fd = conn_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &ev_conn);
                client_info* info = calloc(1, sizeof(client_info));
                // add to dictionary
                dictionary_set(fd_to_connection_state, &conn_sock, info);
            } else {
                // event on client socket
                client_handler(events[i].data.fd);
            }
        }
    }
    
    // end using code about epoll in the coursebook
    
}
