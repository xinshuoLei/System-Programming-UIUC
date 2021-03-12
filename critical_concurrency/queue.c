/**
 * critical_concurrency
 * CS 241 - Spring 2021
 partner: haoyul4, peiyuan3
 */
#include "queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * This queue is implemented with a linked list of queue_nodes.
 */
typedef struct queue_node {
    void *data;
    struct queue_node *next;
} queue_node;

struct queue {
    /* queue_node pointers to the head and tail of the queue */
    queue_node *head, *tail;

    /* The number of elements in the queue */
    ssize_t size;

    /**
     * The maximum number of elements the queue can hold.
     * max_size is non-positive if the queue does not have a max size.
     */
    ssize_t max_size;

    /* Mutex and Condition Variable for thread-safety */
    pthread_cond_t cv;
    pthread_mutex_t m;
};

queue *queue_create(ssize_t max_size) {
    /* Your code here */
    struct queue* toReturn = malloc(sizeof(struct queue));
    toReturn->head = NULL;
    toReturn->tail = NULL;
    toReturn->size = 0;
    toReturn->max_size = max_size;
    pthread_mutex_init(&toReturn->m, NULL);
    pthread_cond_init(&toReturn->cv, NULL);
    return NULL;
}

void queue_destroy(queue *this) {
    /* Your code here */
    queue_node* start = this->tail;
    while (start) {
        queue_node* temp = start;
        start = start->next;
        free(temp);
    }
    pthread_mutex_destroy(&(this->m));
    pthread_cond_destroy(&(this->cv));
    free(this);
}

void queue_push(queue *this, void *data) {
    /* Your code here */
    pthread_mutex_lock(&this->m);
    if (this->max_size > 0 && this->size < this->max_size) {
        pthread_cond_wait(&this->cv, &this->m);
    }
    queue_node * toAdd = malloc(sizeof(queue_node));
    toAdd->data = data;
    if (this->head) {
        this->head->next = toAdd;
    } else {
        this->head = toAdd;
    }
    if (!this->tail) {
        this->tail = toAdd;
    }
    this->size++;

    if (this->size > 0) {
        pthread_cond_broadcast(&this->cv);
    }
    pthread_mutex_unlock(&this->m);
}

void *queue_pull(queue *this) {
    /* Your code here */
    pthread_mutex_lock(&this->m);
    while (this->size == 0) {
        pthread_cond_wait(&this->cv, &this->m);

    }
    queue_node * toRemove = this->tail;
    void* toReturn = toRemove->data;
    this->tail = this->tail->next;
    free(toRemove);
    this->size--;
    if (this->size == 0) {
        this->head = NULL;
    }
    if (this->size > 0 && this->size < this->max_size) {
        pthread_cond_broadcast(&this->cv);
    }
    pthread_mutex_unlock(&this->m);
    return toReturn;
}