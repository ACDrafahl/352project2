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
//open a file with RSFS_RDONLY or RSFS_RDWR flags
//return a file descriptor if succeed; 
//otherwise return a negative integer value
int RSFS_open(char file_name, int access_flag){
    // 2.3.3 Check to make sure access_flag is either RSFS_RDONLY or RSFS_RDWR
    if (!(access_flag == RSFS_RDONLY || access_flag == RSFS_RDWR)) {
        printf("[open] access_flag is invalid.\n");
        return -1;
    }
    if (DEBUG) printf("[open] access_flag is valid.\n");
    // 2.3.3 Find dir_entry matching file_name
    struct dir_entry *dir_entry = search_dir(file_name);
    if (dir_entry == NULL) {
        printf("[open] file (%c) does not exist.\n", file_name);
        return -1;
    }
    if (DEBUG) printf("[open] file (%c) exists.\n", file_name);
    //to do: find the corresponding inode 
    int inode_number = dir_entry->inode_number;
    if (inode_number < 0 || inode_number >= NUM_INODES) {
        printf("[open] inode number (%d) is invalid.\n", inode_number);
        return -1;
    }
    if (DEBUG) printf("[open] inode number (%d) is valid.\n", inode_number);
    // 2.3.3 Find an unused open-file-entry in open-file-table and fill the fields of the entry properly
    int fd = allocate_open_file_entry(access_flag, inode_number);
    if (fd < 0) {
        printf("[open] no available open file entry.\n");
        return -1;
    }
    if (DEBUG) printf("[open] open file entry is allocated with fd:%d.\n", fd);
    // 2.3.3 Return the index of the open-file-entry in open-file-table as file descriptor
    return fd;
}



//append the content in buf to the end of the file of descriptor fd
//return the number of bytes actually appended to the file
int RSFS_append(int fd, void *buf, int size){

    // 2.3.4 to do: check the sanity of the arguments: 
    // fd should be in [0,NUM_OPEN_FILE] and size>0.
    if (fd < 0 || fd >= NUM_OPEN_FILE || size <= 0) {
        printf("[append] fd is invalid or size is not positive.\n");
        return -1;
    }

    // 2.3.4 to do: get the open file entry corresponding to fd
    struct open_file_entry *entry = &open_file_table[fd];
    if (!entry->used) {
        return -1;
    }

    // 2.3.4 locking for advanced level
    pthread_mutex_lock(&entry->entry_mutex);

    // 2.3.4 to do: check if the file is opened with RSFS_RDWR mode; 
    // otherwise return 0
    if (entry->access_flag != RSFS_RDWR) {
        printf("[append] file is not opened with RSFS_RDWR mode.\n");
        pthread_mutex_unlock(&entry->entry_mutex);
        return 0;
    }

    // 2.3.4 to do: get the current position
    int current_position = entry->position;

    // 2.3.4 to do: get the inode 
    int inode_number = entry->inode_number;
    struct inode *inode = &inodes[inode_number];

    int bytes_written = 0;
    char *data_buf = (char *)buf;

    // 2.3.4 to do: append the content in buf to the data blocks of the file 
    // from the end of the file; allocate new block(s) when needed 
    // - (refer to lecture L22 on how)
    while (bytes_written < size && current_position < NUM_POINTERS * BLOCK_SIZE) {
        int block_index = current_position / BLOCK_SIZE;
        int offset = current_position % BLOCK_SIZE;

        // allocate a new block if needed
        if (inode->block[block_index] == -1) {
            pthread_mutex_lock(&data_bitmap_mutex);
            int new_block = allocate_data_block();
            pthread_mutex_unlock(&data_bitmap_mutex);

            if (new_block < 0) {
                printf("[append] no more space available for new data block.\n");
                break;
            }
            inode->block[block_index] = new_block;
        }

        int block_number = inode->block[block_index];
        char *block_ptr = (char *)data_blocks[block_number];

        int space_in_block = BLOCK_SIZE - offset;
        int bytes_remaining = size - bytes_written;
        int bytes_to_write;

        if (size - bytes_written < space_in_block) {
            bytes_to_write = bytes_remaining;
        } else {
            bytes_to_write = space_in_block;
        }

        // copy data from buf to the block
        memcpy(block_ptr + offset, data_buf + bytes_written, bytes_to_write);

        bytes_written += bytes_to_write;
        current_position += bytes_to_write;
    }
    //to do: update the current position in open file entry
    if (current_position > inode->length) {
        inode->length = current_position;
    }
    entry->position = current_position;

    // 2.3.4: unlock the entry mutex
    pthread_mutex_unlock(&entry->entry_mutex);

    //to do: return the number of bytes appended to the file
    return bytes_written;
}





//update current position of the file (which is in the open_file_entry) to offset
//return -1 if fd is invalid; otherwise return the current position after the update
int RSFS_fseek(int fd, int offset){

    //to do: sanity test of fd; if fd is not valid, return -1    
    

    //to do: get the correspondng open file entry
    

    //to do: get the current position
    

    //to do: get the inode and file length
    

    //to do: check if argument offset is not within 0...length, 
    // do not proceed and return current position
    

    //to do: update the current position to offset, and 
    // return the new current position
}






//read up to size bytes to buf from file's current position towards the end
//return -1 if fd is invalid; otherwise return the number of bytes actually read
int RSFS_read(int fd, void *buf, int size){

    //to do: sanity test of fd and size (the size should not be negative)    
    

    //to do: get the corresponding open file entry
    

    //to do: get the current position
    
    
    //to do: get the corresponding inode 
    
    //to do: read from the file
    
    //to do: update the current position in open file entry
    

    //to do: return the actual number of bytes read

}


//close file: return 0 if succeed; otherwise return -1
int RSFS_close(int fd){

    //2.3.3 sanity test of fd   
    

    //to do: get the corresponding open file entry
    

    //to do: get the corresponding inode 
    

    //to do: release this open file entry in the open file table
    
}



//write the content of size (bytes) in buf to the file (of descripter fd) 
int RSFS_write(int fd, void *buf, int size){

}





