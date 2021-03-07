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
static int first = 1;

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
    
    while(ptr) {
        if (ptr -> free && ptr -> size >= size) {
            // use first-fit
            chosen = ptr;
            break;
        }
        ptr = ptr -> next;
    }

    if (chosen) {
        // implement split later

        chosen -> free = 0;

        
        // remove from FREE memory list
        entry_t* chosen_prev = chosen -> prev;
        entry_t* chosen_next = chosen -> next;
        if(chosen_prev && chosen_next) {
			chosen_prev -> next = chosen_next;
			chosen_next -> prev = chosen_prev;
		} else if(chosen_next) {
			chosen_next -> prev = NULL;
			head = chosen-> next;
		} else if(chosen_prev) {
			chosen_prev -> next = NULL;
		} else {
            // chosen is not linked to anything
            head = NULL;
        }
        
        return chosen -> ptr;
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
    
    if (first) {
        if (original_head) {
            original_head -> prev = chosen;
        }
        chosen -> next = original_head;
        chosen -> prev = NULL;
        head = chosen;
        first = 0;
    }

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

    
    // put block back in linked list
    entry_t* original_head = head;
    if (original_head) {
        original_head -> prev = entry;
    }
    entry -> next = original_head;
    entry -> prev = NULL;
    head = entry;
    
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
