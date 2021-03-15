/**
 * malloc
 * CS 241 - Spring 2021
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))


// double linked list struct, based on lecture handout
typedef struct _metadata_entry{
  void* ptr;
  size_t size;
  int free;
  struct _metadata_entry* next;
  struct _metadata_entry* prev;
} entry_t;


/**
 * linked list start of FREE memory
 */ 
static entry_t* head = NULL;
static size_t free_memory_size = 0;


void merge_with_prev(entry_t* entry){
    entry_t* prev_entry = entry -> prev;
    // skip prev entry
    entry->size += prev_entry ->size + sizeof(entry_t);
    if (prev_entry -> prev) {
        prev_entry -> prev -> next = entry;
        entry->prev = prev_entry->prev;
    } else {
        entry -> prev = NULL;
        head = entry;
    }
}

void split(entry_t* entry, size_t size){
    // get the ptr for the extra space
    entry_t* extra_space = (void*) entry->ptr + size;
    extra_space->ptr = extra_space + 1;
    // calculate size, extra space will be free memory
    extra_space->size = entry->size - size - sizeof(entry_t);
    extra_space->free = 1;
    free_memory_size += extra_space->size;

    // deal with the memory for the exact size
    entry->size = size;

    extra_space->next = entry;
    extra_space->prev = entry->prev;
    if(entry->prev){
        entry->prev->next = extra_space;
    }else{
        head = extra_space;
    }
    entry->prev = extra_space;
    // check for possible merge
    if(extra_space->prev && extra_space->prev->free) {
        merge_with_prev(extra_space);
    }
}



/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    // implement calloc!
    void* ptr = malloc(num * size);
    // check if malloc is successful
    if (ptr == NULL) {
        return NULL;
    }
    // initialize all bits to zero
    memset(ptr, 0, num*size);
    return ptr;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size) {
    // implement malloc!

    // based on code in lecture and handout
    entry_t* ptr = head;
    entry_t* chosen = NULL;
    
    if (free_memory_size >= size) {
        while(ptr) {
            if (ptr -> free && ptr -> size >= size) {
                // use first-fit
                free_memory_size -= size;
                chosen = ptr;
                break;
            }
            ptr = ptr -> next;
        }

        if (chosen) {
            // implement split later
            if((chosen->size - size >= size) && (chosen->size - size >= sizeof(entry_t))) { 
                split(chosen,size);
            }
            chosen -> free = 0;
            return chosen -> ptr;
        }
    }

    // call sbrk
    chosen = sbrk(sizeof(entry_t));
    chosen -> ptr = sbrk(0);
    if (sbrk(size) == (void*) -1) {
        return NULL;
    }

    
    chosen -> size = size;
    chosen -> free = 0;

    entry_t* original_head = head;
    
    if (original_head) {
        original_head -> prev = chosen;
    }
    chosen -> next = original_head;
    chosen -> prev = NULL;
    head = chosen;

    return chosen -> ptr;
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    // implement free!

    /**
     * If a null pointer is passed as argument, no action occurs.
     */
    if (ptr == NULL) {
        return;
    }
    entry_t* entry = ((entry_t*) ptr) - 1;

    entry -> free = 1;
    free_memory_size += (entry -> size + sizeof(entry_t));
    
    if(entry->prev && entry->prev->free == 1) {
        merge_with_prev(entry);
    }
    if(entry->next && entry->next->free == 1) {
        merge_with_prev(entry->next);
    }
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size) {
    // implement realloc!

    /**
     * In case that ptr is NULL, the function behaves exactly as malloc
     */ 
    if (ptr == NULL) {
        return malloc(size);
    }
    /**
     * In case that the size is 0, the memory previously allocated in ptr is
     * deallocated as if a call to free was made, and a NULL pointer is returned.
     */ 
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // based on code in lecture 

    // get pointer to metadata entry
    entry_t* entry = ((entry_t*) ptr) - 1;

    // if original size is large enough
    if (entry -> size >= size) {
        // split if there is extra space
        if(entry->size - size >= sizeof(entry_t)){
            split(entry,size);
            return entry->ptr;
        }
        return ptr;
    }

    // allocate new memory and free previous one
    void* new_ptr = malloc(size);
    // check if malloc is successful
    if (new_ptr == NULL) {
        return NULL;
    }
    memcpy(new_ptr, ptr, min(size, entry -> size));
    free(ptr);
    return new_ptr;
}
