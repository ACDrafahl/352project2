/*
    allocation of global open_file_table and its guarding mutex; 
    routines for open file entry
*/

#include "def.h"

struct open_file_entry open_file_table[NUM_OPEN_FILE];
pthread_mutex_t open_file_table_mutex;

// allocate an available entry in open file table and return fd (file descriptor);
// return -1 if no entry is found
// (Cleaned this one up a bit)
int allocate_open_file_entry(int access_flag, int inode_number){
    int fd = -1;

    pthread_mutex_lock(&open_file_table_mutex);
    for(int i = 0; i < NUM_OPEN_FILE; i++){
        struct open_file_entry *entry = &open_file_table[i];
        if(entry->used == 0){ // find an empty entry
            fd = i;
            entry->used = 1;
            entry->access_flag = access_flag;
            entry->inode_number = inode_number;
            entry->position = 0;

            break;
        }
    }
    pthread_mutex_unlock(&open_file_table_mutex);

    return fd;
}

// Changed the function so it resets everything
void free_open_file_entry(int fd) {
    if(fd < 0 || fd >= NUM_OPEN_FILE) return;
    pthread_mutex_lock(&open_file_table_mutex);

    open_file_table[fd].used = 0;
    open_file_table[fd].access_flag = -1;
    open_file_table[fd].inode_number = -1;
    open_file_table[fd].position = 0;

    pthread_mutex_unlock(&open_file_table_mutex);
}