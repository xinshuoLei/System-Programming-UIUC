/**
 * finding_filesystems
 * CS 241 - Spring 2021
 */
#include "minixfs.h"
#include "minixfs_utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/**
 * Virtual paths:
 *  Add your new virtual endpoint to minixfs_virtual_path_names
 */
char *minixfs_virtual_path_names[] = {"info", /* add your paths here*/};

/**
 * Forward declaring block_info_string so that we can attach unused on it
 * This prevents a compiler warning if you haven't used it yet.
 *
 * This function generates the info string that the virtual endpoint info should
 * emit when read
 */
static char *block_info_string(ssize_t num_used_blocks) __attribute__((unused));
static char *block_info_string(ssize_t num_used_blocks) {
    char *block_string = NULL;
    ssize_t curr_free_blocks = DATA_NUMBER - num_used_blocks;
    asprintf(&block_string, "Free blocks: %zd\n"
                            "Used blocks: %zd\n",
             curr_free_blocks, num_used_blocks);
    return block_string;
}

// Don't modify this line unless you know what you're doing
int minixfs_virtual_path_count =
    sizeof(minixfs_virtual_path_names) / sizeof(minixfs_virtual_path_names[0]);

// helper function that gets data block
void* get_block(file_system* fs, inode* parent, uint64_t block_index){
  data_block_number* block;
  // direct block
  if (block_index < NUM_DIRECT_BLOCKS){
    block = parent -> direct;
  } else {
    // indirect
    block = (data_block_number*)(fs -> data_root + parent -> indirect);
    // decrease index
    block_index -= NUM_DIRECT_BLOCKS;
  }
  void* result = (void*) (fs -> data_root + block[block_index]);
  return result;
}

int minixfs_chmod(file_system *fs, char *path, int new_permissions) {
    // Thar she blows!
    inode* node = get_inode(fs, path);
    // path doesn't exist
    if (node == NULL){
        errno = ENOENT;
        return -1;
    }
    uint16_t reserved = node->mode >> RWX_BITS_NUMBER;
    node -> mode = new_permissions | (reserved << RWX_BITS_NUMBER);
    // update ctim
    clock_gettime(CLOCK_REALTIME, &(node -> ctim));
    return 0;
}




int minixfs_chown(file_system *fs, char *path, uid_t owner, gid_t group) {
    // Land ahoy!
    inode* node = get_inode(fs, path);
    // path doesn't exist
    if (node == NULL) {
        errno = ENOENT;
        return -1;
    }
    // If owner is ((uid_t)-1), then don't change the node's uid.
    if (owner != ((uid_t)-1)) {
        node -> uid = owner;
    }
    // If group is ((gid_t)-1), then don't change the node's gid
    if (group != ((gid_t)-1)) {
        node -> gid = group;
    }
    // update ctim
    clock_gettime(CLOCK_REALTIME, &(node -> ctim));
    return 0;
}

inode *minixfs_create_inode_for_path(file_system *fs, const char *path) {
    // Land ahoy!
    inode* node = get_inode(fs, path);
    // already exists
    if (node != NULL) {
        return NULL;
    }
   
    const char* filename;
    inode* parent = parent_directory(fs, path, &filename);
     // invalid path
    if (!valid_filename(filename)) {
        return NULL;
    }
    if (parent == NULL || !is_directory(parent)) {
        return NULL;
    }
    inode_number new_index = first_unused_inode(fs);
    // no more inodes
    if (new_index == -1) {
        return NULL;
    }
    inode* new_node = fs -> inode_root + new_index;
    // initialize unused inode
    init_inode(parent, new_node);
    // add a new dirent to the parent 
    minixfs_dirent d;
    d.inode_num = new_index;
    d.name = (char*) filename;
    // should use indirect but I'm tired
    if ((parent -> size / sizeof(data_block)) >= NUM_DIRECT_BLOCKS) {
        return NULL;
    }
    int offset = parent -> size % sizeof(data_block);
    // cannot create because number of data blocks already full
    if (!offset && (add_data_block_to_inode(fs, parent) == -1)) {
        return NULL;
    }
    void* start = get_block(fs, parent, (parent -> size / sizeof(data_block))) + offset;
    memset(start, 0, FILE_NAME_ENTRY);
    make_string_from_dirent(start, d);
    // increase the parent's size
    parent -> size += MAX_DIR_NAME_LEN;
    return new_node;
}

ssize_t minixfs_virtual_read(file_system *fs, const char *path, void *buf,
                             size_t count, off_t *off) {
    if (!strcmp(path, "info")) {
        // TODO implement the "info" virtual file here
        size_t num_used = 0;
        // loop through the result of GET_DATA_MAP.
        char *map = GET_DATA_MAP(fs->meta);
        uint64_t i = 0;
        for (; i < fs->meta->dblock_count; i++) {
            // in use
            if (map[i] != 0) {
                num_used++;
            }
        }
        // info string
        char* info_str = block_info_string(num_used);
        // calculate the size buffer requested
        size_t size = strlen(info_str);
        if (*off > (long) size) {
            return 0;
        }
        if (count > size - *off) {
            count = size - *off;
        }
        // copy to buf
        memmove(buf, info_str + *off, count);
        // increment off
        *off += count;
        return count;
    }

    errno = ENOENT;
    return -1;
}

ssize_t minixfs_write(file_system *fs, const char *path, const void *buf,
                      size_t count, off_t *off) {
    // X marks the spot
    inode* node = get_inode(fs, path);
    // create the file if it doesn't exist
    if (node == NULL) {
        node = minixfs_create_inode_for_path(fs, path);
        // no more data blocks can be allocated
        if (node == NULL) {
            errno = ENOSPC;
            return -1;
        }
    }
    // requested to write more bytes than the maximu possible filesize
    // fail without writing any data and set errno to ENOSPC
    size_t max_size = sizeof(data_block) * (NUM_DIRECT_BLOCKS + NUM_INDIRECT_BLOCKS);
    if (count + *off > max_size) {
        errno = ENOSPC;
        return -1;
    }
    int required_block = (count + *off + sizeof(data_block) - 1) / sizeof(data_block);
    // If this function fails because no more data blocks can be allocated, set errno to ENOSPC
    if (minixfs_min_blockcount(fs, path, required_block) == -1) {
        errno = ENOSPC;
        return -1;
    }
    size_t block_index = *off / sizeof(data_block);
    size_t block_offset = *off % sizeof(data_block);
    uint64_t size = MIN((sizeof(data_block) - block_offset), count);
    void* block = get_block(fs, node, block_index) + block_offset;
    memcpy(block, buf, size);
    // increament off
    *off += size;
    size_t write_count = size;
    // finish reading the first block
    block_index++;
    while (write_count < count) {
        size = MIN((count - write_count), sizeof(data_block));
        // get new block and copy to buf
        block = get_block(fs, node, block_index);
        memcpy(block, buf + write_count, size);
        // finish writing
        block_index++;
        write_count += size;
        *off += size;
    }
    // update file size
    if (count + *off > node -> size) {
        node -> size  = count + *off;
    }
    // update mtime and atime
    clock_gettime(CLOCK_REALTIME, &(node->mtim));
    clock_gettime(CLOCK_REALTIME, &(node->atim));
    return write_count;
}

ssize_t minixfs_read(file_system *fs, const char *path, void *buf, size_t count,
                     off_t *off) {
    const char *virtual_path = is_virtual_path(path);
    if (virtual_path)
        return minixfs_virtual_read(fs, virtual_path, buf, count, off);
    // 'ere be treasure!
    inode* node = get_inode(fs, path);
    // path does not exist
    if (node == NULL) {
        errno = ENOENT;
        return -1;
    }
    // If *off is greater than the end of the file
    // do nothing and return 0 to indicate end of file.
    if ((uint64_t)*off > node -> size) {
        return 0;
    }
    size_t block_index = *off / sizeof(data_block);
    size_t block_offset = *off % sizeof(data_block);
    // read size
    uint64_t size = MIN(count, (sizeof(data_block) - block_offset));
    void* block = get_block(fs, node, block_index) + block_offset;
    memcpy(buf, block, size);
    // increament off
    *off += size;
    size_t read_count = size;
    // finish reading the first block
    block_index++;
    while (read_count < count) {
        size = MIN((count - read_count), sizeof(data_block));
        // get new block and copy to buf
        block = get_block(fs, node, block_index);
        memcpy(buf + read_count, block, size);
        // finish reading
        block_index++;
        read_count += size;
        *off += size;
    }
    // update atim
    clock_gettime(CLOCK_REALTIME, &(node -> atim));
    return read_count;
}
