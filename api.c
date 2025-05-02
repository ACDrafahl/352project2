/*
    implementation of API
*/

#include "def.h"

pthread_mutex_t mutex_for_fs_stat;//mutex used by RSFS_stat()


//initialize file system - should be called as the first thing before accessing this file system 
int RSFS_init(){
    char *debugTitle = "RSFS_init";

    //initialize data blocks
    for(int i=0; i<NUM_DBLOCKS; i++){
      void *block = malloc(BLOCK_SIZE); //a data block is allocated from memory
      if(block==NULL){
        printf("[%s] fails to init data_blocks\n", debugTitle);
        return -1;
      }
      data_blocks[i] = block;  
    } 

    //initialize bitmaps
    for(int i=0; i<NUM_DBLOCKS; i++) data_bitmap[i]=0;
    pthread_mutex_init(&data_bitmap_mutex,NULL);
    for(int i=0; i<NUM_INODES; i++) inode_bitmap[i]=0;
    pthread_mutex_init(&inode_bitmap_mutex,NULL);    

    //initialize inodes
    for(int i=0; i<NUM_INODES; i++){
        inodes[i].length=0;
    }
    pthread_mutex_init(&inodes_mutex,NULL); 

    //initialize open file table
    for(int i=0; i<NUM_OPEN_FILE; i++){
        struct open_file_entry entry=open_file_table[i];
        entry.used=0; //each entry is not used initially
        pthread_mutex_init(&entry.entry_mutex,NULL);
        entry.position=0;
        entry.access_flag=-1;
        // entry.ref=0;
        entry.inode_number=-1;
    }
    pthread_mutex_init(&open_file_table_mutex,NULL); 

    //initialize root inode
    root_inode_number = allocate_inode();
    if(root_inode_number<0){
        printf("[%s] fails to allocate root inode\n", debugTitle);
        return -1;
    }
    pthread_mutex_init(&root_dir_mutex,NULL); 
    
    
    //initialize mutex_for_fs_stat
    pthread_mutex_init(&mutex_for_fs_stat,NULL);

    //return 0 means success
    return 0;
}


//create file
//if file does not exist, create the file and return 0;
//if file_name already exists, return -1; 
//otherwise (other errors), return -2.
int RSFS_create(char file_name){

    //search root_dir for dir_entry matching provided file_name
    struct dir_entry *dir_entry = search_dir(file_name);

    if(dir_entry){//already exists
        printf("[create] file (%c) already exists.\n", file_name);
        return -1;
    }else{

        if(DEBUG) printf("[create] file (%c) does not exist.\n", file_name);

        //get a free inode 
        char inode_number = allocate_inode();
        if(inode_number<0){
            printf("[create] fail to allocate an inode.\n");
            return -2;
        } 
        if(DEBUG) printf("[create] allocate inode with number:%d.\n", inode_number);

        //insert (file_name, inode_number) to root directory entry
        dir_entry = insert_dir(file_name, inode_number);
        if(DEBUG) printf("[create] insert a dir_entry with file_name:%c.\n", dir_entry->name);
        
        return 0;
    }
}



//delete file
int RSFS_delete(char file_name){

    char debug_title[32] = "[RSFS_delete]";

    //to do: find the corresponding dir_entry
    struct dir_entry *dir_entry = search_dir(file_name);
    if(dir_entry==NULL){
        printf("%s director entry does not exist for file (%c)\n", 
            debug_title, file_name);
        return -1;
    }

    //to do: find the corresponding inode
    int inode_number = dir_entry->inode_number;
    if(inode_number<0 || inode_number>=NUM_INODES){
        printf("%s inode number (%d) is invalid.\n", 
            debug_title, inode_number);
        return -2;
    }
    struct inode *inode = &inodes[inode_number];

    //to do: find the data blocks, free them in data-bitmap
    pthread_mutex_lock(&data_bitmap_mutex);
    for(int i = 0; i <= inode->length/BLOCK_SIZE; i++){
        int block_number = inode->block[i];
        if(block_number>=0) data_bitmap[block_number]=0;    
    }
    pthread_mutex_unlock(&data_bitmap_mutex);

    //to do: free the inode in inode-bitmap
    pthread_mutex_lock(&inode_bitmap_mutex);
    inode_bitmap[inode_number]=0;
    pthread_mutex_unlock(&inode_bitmap_mutex);

    //to do: free the dir_entry
    int ret = delete_dir(file_name);
    
    return 0;
}


//print status of the file system
void RSFS_stat(){

    pthread_mutex_lock(&mutex_for_fs_stat);


    printf("\nCurrent status of the file system:\n\n %16s%10s%10s\n", "File Name", "Length", "iNode #");

    //list files
    for(int i=0; i<BLOCK_SIZE/sizeof(struct dir_entry); i++){
        struct dir_entry *dir_entry = (struct dir_entry *)root_data_block + i;
        if(dir_entry->name==0) continue;
        
        int inode_number = dir_entry->inode_number;
        struct inode *inode = &inodes[inode_number];
        
        printf("%16c%10d%10d\n", dir_entry->name, inode->length, inode_number);
    }
    
    
    //data blocks
    int db_used=0;
    for(int i=0; i<NUM_DBLOCKS; i++) db_used+=data_bitmap[i];
    printf("\nTotal Data Blocks: %4d,  Used: %d,  Unused: %d\n", NUM_DBLOCKS, db_used, NUM_DBLOCKS-db_used);

    //inodes
    int inodes_used=0;
    for(int i=0; i<NUM_INODES; i++) inodes_used+=inode_bitmap[i];
    printf("Total iNode Blocks: %3d,  Used: %d,  Unused: %d\n", NUM_INODES, inodes_used, NUM_INODES-inodes_used);

    //open files
    int of_num=0;
    for(int i=0; i<NUM_OPEN_FILE; i++) of_num+=open_file_table[i].used;
    printf("Total Opened Files: %3d\n\n", of_num);

    pthread_mutex_unlock(&mutex_for_fs_stat);
}





//------ implementation of the following functions is incomplete --------------------------------------------------------- 



// 2.3.3
// open a file with RSFS_RDONLY or RSFS_RDWR flags
// return a file descriptor if succeed; 
// otherwise return a negative integer value
int RSFS_open(char file_name, int access_flag) {
    //to do: check to make sure access_flag is either RSFS_RDONLY or RSFS_RDWR
    if(access_flag != RSFS_RDONLY && access_flag != RSFS_RDWR) {
        printf("[open] access_flag is invalid.\n");
        return -1;
    }
    //to do: find dir_entry matching file_name
    struct dir_entry *entry = search_dir(file_name);
    if(entry == NULL) {
        printf("[open] file (%c) does not exist.\n", file_name);
        return -2;
    }
    
    //to do: find the corresponding inode 
    int inode_number = entry->inode_number;
    struct inode *node = &inodes[inode_number];

    // 2.3.3 Synchronization, enforce reader-writer concurrency
    pthread_mutex_lock(&node->rw_mutex);
    if(access_flag == RSFS_RDONLY) {
        // Wait while a writer is active
        while(node->writer_active) {
            pthread_cond_wait(&node->rw_cond, &node->rw_mutex);
        }
        node->reader_count++;
    }
    else if(access_flag == RSFS_RDWR) {
        // Wait while there are active readers or another writer
        while(node->reader_count > 0 || node->writer_active) {
            pthread_cond_wait(&node->rw_cond, &node->rw_mutex);
        }
        node->writer_active = 1;
    }
    pthread_mutex_unlock(&node->rw_mutex);
    
    //to do: find an unused open-file-entry in open-file-table and fill the fields of the entry properly
    int fd = allocate_open_file_entry(access_flag, inode_number);
    if (fd < 0) {
        // Decrement reader/writer count if allocation fails
        pthread_mutex_lock(&node->rw_mutex);
        if(access_flag == RSFS_RDONLY) {
            node->reader_count--;
            if(node->reader_count == 0) {
                pthread_cond_broadcast(&node->rw_cond);
            }
        }
        else if(access_flag == RSFS_RDWR) {
            node->writer_active = 0;
            pthread_cond_broadcast(&node->rw_cond);
        }
        pthread_mutex_unlock(&node->rw_mutex);
        printf("[open] fail to allocate an open file entry.\n");
        return -3;        
    }
    
    //to do: return the index of the open-file-entry in open-file-table as file descriptor
    return fd;
}


// 2.3.4
// append the content in buf to the end of the file of descriptor fd
// return the number of bytes actually appended to the file
int RSFS_append(int fd, void *buf, int size){

    //to do: check the sanity of the arguments: 
    // fd should be in [0,NUM_OPEN_FILE] and size>0.
    if(fd < 0 || fd >= NUM_OPEN_FILE || size <= 0) {
        printf("[append] invalid file descriptor (%d) or size (%d)\n", fd, size);
        return 0; // 0 because no bytes appended
    }
    
    //to do: get the open file entry corresponding to fd
    struct open_file_entry *entry = &open_file_table[fd];
    if(entry->used == 0) {
        printf("[append] file descriptor (%d) is not in use\n", fd);
        return 0; // 0 because no bytes appended
    }
    
    //to do: check if the file is opened with RSFS_RDWR mode; 
    // otherwise return 0
    if(entry->access_flag != RSFS_RDWR) {
        printf("[append] file descriptor (%d) is not opened with RSFS_RDWR mode\n", fd);
        return 0; // 0 because no bytes appended
    }
    
    //to do: get the inode 
    struct inode *node = &inodes[entry->inode_number];

    //to do: get the current position (moved this to fix length issue)
    int current_position = node->length;
    entry->position = current_position;
    
    //to do: append the content in buf to the data blocks of the file 
    // from the end of the file; allocate new block(s) when needed 
    // - (refer to lecture L22 on how)
    int bytes_written = 0;

    while (bytes_written < size) {
        int byte_offset = current_position + bytes_written;
        int block_index = byte_offset / BLOCK_SIZE;
        int offset_in_block = byte_offset % BLOCK_SIZE;

        // Check if we need to allocate a new block
        if(block_index >= NUM_POINTERS) {
            // No more space in the inode's pointers
            printf("[append] file size exceeds maximum limit\n");
            break;
        }

        // Allocate a new data block if needed
        if(node->block[block_index] == -1) {
            int new_block = allocate_data_block();
            if(new_block < 0) {
                printf("[append] fail to allocate a new data block\n");
                break;
            }
            node->block[block_index] = new_block;
        }

        // Get block's memory
        char *block = (char *)data_blocks[node->block[block_index]];
        int space_in_block = BLOCK_SIZE - offset_in_block;
        int bytes_remaining = size - bytes_written;
        int chunk;

        if (bytes_remaining < space_in_block) {
            chunk = bytes_remaining;
        } else {
            chunk = space_in_block;
        }

        // Copy data to the block
        memcpy(block + offset_in_block, (char *)buf + bytes_written, chunk);
        bytes_written += chunk;
    }

    //to do: update the current position in open file entry
    entry->position += bytes_written;

    // update file length if need be
    if(entry->position > node->length) {
        node->length = entry->position;
    }

    //to do: return the number of bytes appended to the file
    return bytes_written;
}


// 2.3.5
// update current position of the file (which is in the open_file_entry) to offset
// return -1 if fd is invalid; otherwise return the current position after the update
int RSFS_fseek(int fd, int offset){
    //to do: sanity test of fd; if fd is not valid, return -1    
    if(fd < 0 || fd >= NUM_OPEN_FILE) {
        printf("[fseek] invalid file descriptor (%d)\n", fd);
        return -1; 
    }

    //to do: get the correspondng open file entry
    struct open_file_entry *entry = &open_file_table[fd];
    if(entry->used == 0) {
        printf("[fseek] file descriptor (%d) is not in use\n", fd);
        return -1; 
    }

    //to do: get the current position
    int current_position = entry->position;

    //to do: get the inode and file length
    int inode_number = entry->inode_number;
    struct inode *node = &inodes[inode_number];
    int file_length = node->length;

    //to do: check if argument offset is not within 0...length, 
    // do not proceed and return current position
    if(offset < 0 || offset > file_length) {
        printf("[fseek] offset (%d) is out of bounds (0, %d)\n", offset, file_length);
        return current_position; 
    }
    
    //to do: update the current position to offset, and 
    // return the new current position
    entry->position = offset;
    return entry->position;
}

// 2.3.6
// read up to size bytes to buf from file's current position towards the end
// return -1 if fd is invalid; otherwise return the number of bytes actually read
int RSFS_read(int fd, void *buf, int size){

    //to do: sanity test of fd and size (the size should not be negative)    
    if(fd < 0 || fd >= NUM_OPEN_FILE || size < 0) {
        printf("[read] invalid file descriptor (%d) or size (%d)\n", fd, size);
        return -1; 
    }

    //to do: get the corresponding open file entry
    struct open_file_entry *entry = &open_file_table[fd];
    if(entry->used == 0) {
        printf("[read] file descriptor (%d) is not in use\n", fd);
        return -1; 
    }

    //to do: get the current position
    int current_position = entry->position;
    
    //to do: get the corresponding inode 
    struct inode *node = &inodes[entry->inode_number];
    
    //to do: read from the file
    int bytes_read = 0;

    while (bytes_read < size) {
        int byte_offset = current_position + bytes_read;
        
        // Stop if we reach the end of the file
        if (byte_offset >= node->length) {
            break;
        }

        int block_index = byte_offset / BLOCK_SIZE;
        int offset_in_block = byte_offset % BLOCK_SIZE;
        
        // Check if we need to read from a new block
        if (block_index >= NUM_POINTERS || node->block[block_index] == -1) {
            printf("[read] file size exceeds maximum limit\n");
            break;
        }

        // Get block's memory
        char *block = (char *)data_blocks[node->block[block_index]];
        int space_in_block = BLOCK_SIZE - offset_in_block;
        int bytes_remaining = size - bytes_read;
        int bytes_left_in_file = node->length - byte_offset;
        int chunk;

        if(bytes_remaining < space_in_block) {
            chunk = bytes_remaining;
        } else {
            chunk = space_in_block;
        }

        if(chunk > bytes_left_in_file) {
            chunk = bytes_left_in_file;
        }

        // Copy data from the block to buf
        memcpy((char *)buf + bytes_read, block + offset_in_block, chunk);
        bytes_read += chunk;
    }
    
    //to do: update the current position in open file entry
    entry->position += bytes_read;
    
    //to do: return the actual number of bytes read
    return bytes_read;
}

// 2.3.8
// close file: return 0 if succeed; otherwise return -1
int RSFS_close(int fd){

    //to do: sanity test of fd    
    if(fd < 0 || fd >= NUM_OPEN_FILE) {
        printf("[close] invalid file descriptor (%d)\n", fd);
        return -1;
    }
    

    //to do: get the corresponding open file entry
    struct open_file_entry *entry = &open_file_table[fd];
    if(entry->used == 0) {
        printf("[close] file descriptor (%d) is not in use\n", fd);
        return -1;
    }
    
    //to do: get the corresponding inode 
    int inode_number = entry->inode_number;
    struct inode *node = &inodes[inode_number];

    // 2.3.3 Synchronization, update reader-writer tracking
    pthread_mutex_lock(&node->rw_mutex);
    if(entry->access_flag == RSFS_RDONLY) {
        node->reader_count--;
        if(node->reader_count == 0) {
            pthread_cond_broadcast(&node->rw_cond); // notify waiting writer
        }
    }
    else if(entry->access_flag == RSFS_RDWR) {
        node->writer_active = 0;
        pthread_cond_broadcast(&node->rw_cond); // notify waiting readers/writers
    }
    pthread_mutex_unlock(&node->rw_mutex);

    //to do: release this open file entry in the open file table
    free_open_file_entry(fd);

    return 0;
}


// 2.3.7
// write the content of size (bytes) in buf to the file (of descripter fd) 
int RSFS_write(int fd, void *buf, int size){
    // Sanity check
    if(fd < 0 || fd >= NUM_OPEN_FILE || size <= 0) {
        printf("[write] invalid file descriptor (%d) or size (%d)\n", fd, size);
        return -1; 
    }

    // Get the corresponding open file entry
    struct open_file_entry *entry = &open_file_table[fd];
    if(entry->used == 0) {
        printf("[write] file descriptor (%d) is not in use\n", fd);
        return -1; 
    }

    // Check if the file is opened with RSFS_RDWR mode
    if(entry->access_flag != RSFS_RDWR) {
        printf("[write] file descriptor (%d) is not opened with RSFS_RDWR mode\n", fd);
        return -1; 
    }

    // Get the current position
    int current_position = entry->position;
    // Get the inode
    struct inode *node = &inodes[entry->inode_number];

    int bytes_written = 0;

    while(bytes_written < size) {
        int byte_offset = current_position + bytes_written;
        int block_index = byte_offset / BLOCK_SIZE;
        int offset_in_block = byte_offset % BLOCK_SIZE;

        if(block_index >= NUM_POINTERS) {
            // No more space in the inode's pointers
            printf("[write] file size exceeds maximum limit\n");
            break;
        }

        // Allocate a new data block if needed
        if(node->block[block_index] == -1) {
            int new_block = allocate_data_block();
            if(new_block < 0) {
                printf("[write] fail to allocate a new data block\n");
                break;
            }
            node->block[block_index] = new_block;
        }

        // Get block's memory
        char *block = (char *)data_blocks[node->block[block_index]];
        int space_in_block = BLOCK_SIZE - offset_in_block;
        int bytes_remaining = size - bytes_written;
        int chunk;

        if(bytes_remaining < space_in_block) {
            chunk = bytes_remaining;
        } else {
            chunk = space_in_block;
        }

        // Copy data to the block
        memcpy(block + offset_in_block, (char *)buf + bytes_written, chunk);
        bytes_written += chunk;
    }

    // Update the current position in open file entry
    entry->position += bytes_written;

    // Truncate the file, wipe remaining blocks
    int new_length = entry->position;
    int last_used_block = (new_length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for(int i = last_used_block; i < NUM_POINTERS; i++) {
        if(node->block[i] != -1) {
            free_data_block(node->block[i]);
            node->block[i] = -1;
        }
    }

    // Update inode length
    node->length = new_length;

    return bytes_written;
}





