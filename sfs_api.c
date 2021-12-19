//real version
#include "sfs_api.h"
#include <stdlib.h>
#include <stdio.h>
#include "disk_emu.h"
#include <string.h>
//according to the handout
#define block_size 1024
#define max_file_name 20    

//we can modify to choose the number
//probably not the most beautiful solution (due to internal fragmentation)
#define number_of_data_block 1011
#define number_of_inode 200
#define total_number_of_block 3000
#define my_disk "jojo_disk"
#define Dir_block 14
//define data structure
typedef struct superblock{
    int Magic;
    int Block_size;
    int File_System_size;
    int inode_tab_length;
    int rootdirect;
} superblock;
typedef struct inode {
    char free;
    int size;
    int pointer[12];
    int indpointer;
} inode;
 typedef struct indirectblock {
     int pointer[block_size/sizeof(int)];
 } indirectblock;
typedef struct fd{
    char free;
    int i_node;
    int rw_pointer;
} fd;
typedef struct bitmap {
    char map[total_number_of_block];
} bitmap;
typedef struct directory_en {
    char free;
    int inode;
    char filename[max_file_name + 1];
} directory_en;
typedef struct inode_table {
    inode inode_tab[number_of_inode];
}inode_table;
typedef struct file_descriptor{
    fd file_descrip[number_of_inode]; 
} file_descriptor;
typedef struct directory{
directory_en director[number_of_inode];
int current_node;
}directory;
//global variable 
inode_table *itable; 
directory *root_directory;
file_descriptor *fdtable;
bitmap bit_map; 
void initialization(file_descriptor *fdtabl, directory *root_directo, inode_table *itab){
    for (int i = 0; i < number_of_inode; i++){
        fdtabl->file_descrip[i].free = 1;
        fdtabl->file_descrip[i].i_node = -1;
        fdtabl->file_descrip[i].rw_pointer = -1;
        root_directo->director[i].free = 1;
        root_directo->director[i].inode = -1;
        memset(root_directo->director[i].filename, 0, max_file_name);
        itab->inode_tab[i].free = 1;
        itab->inode_tab[i].size = -1;
        for (int j = 0; j < 12; j++){
            itab->inode_tab[i].pointer[j] = -1;
        }
        itab->inode_tab[i].indpointer = -1;
    }
}
//main dishes
void mksfs(int fresh) {
    //overall structure 
    fdtable = malloc(sizeof(file_descriptor));
    root_directory = malloc(sizeof(directory));
    itable = malloc(sizeof(inode_table));
    initialization(fdtable, root_directory, itable); 
    if (fresh == 1) {
        remove(my_disk);
        root_directory->current_node = 0;
        init_fresh_disk(my_disk, block_size, total_number_of_block);
        superblock super_block;
        super_block.Block_size = 1024;
        super_block.Magic = 0xACBD0005;
        super_block.File_System_size = 3000;
        /*
        printf("inode_table_length %d \n", (int) (((sizeof(inode_table))/(1024)) + 1));
        printf("dir_table_length %d \n", (int) (((sizeof(directory))/(1024)) + 1));
        printf("bitmap_length %d \n", (int) ((sizeof(bitmap)/(1024)) + 1));
        */
        super_block.inode_tab_length = ((200 / (1024 /sizeof(inode)) + 1));
        super_block.rootdirect = 0;
        itable->inode_tab[0].free = 0;
        //printf("super_block_length %d \n", (int) ((sizeof(superblock)/(1024)) + 1));
        inode *current = &itable->inode_tab[0];
        current->size = sizeof(directory);
        current->pointer[0] = Dir_block;
        current->pointer[1] = Dir_block + 1;
        current->pointer[2] = Dir_block + 2;
        current->pointer[3] = Dir_block + 3;
        current->pointer[4] = Dir_block + 4;
        current->pointer[5] = Dir_block + 5;
        current->pointer[6] = Dir_block + 6;
        current->pointer[7] = Dir_block + 7;
        for (int i=0; i < total_number_of_block; i++ ){
            bit_map.map[i] = 1;
        }
        for(int i=0; i <= 25; i++){
             bit_map.map[i] = 0;
        }
        /*
        printf("super block content\n");
        printf("magic is %d \n", super_block.Magic);
        printf("block size is %d \n", super_block.Block_size);
        printf("File System size is %d \n", super_block.File_System_size);
        printf("File i-node tbale length is %d \n", super_block.inode_tab_length);
        printf("root dir is %d \n", super_block.rootdirect);
        printf("size of this is %d \n", sizeof(super_block)/1024);
        */
        //bitmap set 0 -> 15 to used due to directory block 
        //printf("Do i appear? \n");
        write_blocks(0, 1, &super_block);
        //printf("Do i appear? \n");
        //printf("where is the out of bound error\n");
        write_blocks(1, 12, itable);
       // printf("Do i appear? \n");
        write_blocks(13, 7, root_directory);
        //printf("Do i appear? \n");
        write_blocks(20, 3, &bit_map);
        //printf("Do i appear? \n");
        
        //initialize  
    } else {
        superblock super_block;
        init_disk(my_disk, block_size, total_number_of_block);
        read_blocks(0, 1, &super_block);
        read_blocks(1, 12, itable);
        read_blocks(13, 7, root_directory);
        read_blocks(20, 3, &bit_map);
        

    }
    return;
}
int check_exist_dir(char *filename){
    for (int i = 0; i < 200; i++){
        if (root_directory->director[i].free == 1){
            continue;
        } else {
            if (strcmp(filename, root_directory->director[i].filename) == 0){
                return root_directory->director[i].inode;
            } 
        }
    }
    return -1;
}
int find_availiable_inode_index(inode_table *itale){
    for (int i = 0; i < 200; i++){
        if (itale->inode_tab[i].free == 1){
            return i;
        } 
    }
    return -1;
}
int find_availiable_dir_index(directory *dir){
    for (int i = 0; i < 200; i++){
        if (dir->director[i].free == 1){
            return i;
        } 
    }
    return -1;
}
int find_availiable_fd_index(file_descriptor *fdtabl) {
    for (int i = 0; i < 200; i++){
        if (fdtabl->file_descrip[i].free == 1){
            return i;
        } 
    }
    return -1;
}
int check_inode_in_fd(file_descriptor *fdtabl, int inode){
    for (int i = 0; i < 200; i++){
        if (fdtabl->file_descrip[i].i_node == inode){
            return i;
        } 
    }
    return -1;
}
int check_dir_pos(char *filename){
        for (int i = 0; i < 200; i++){
        if (root_directory->director[i].free == 1){
            continue;
        } else {
            if (strcmp(filename, root_directory->director[i].filename) == 0){
                return i;
            } 
        }
    }
    return -1;
}
int next_availiable_bit(){
    for (int i =0; i < total_number_of_block; i++){
        if (bit_map.map[i] == 1) {
            return i;
        }
    }
    return -1;
}
int sfs_fopen(char *file){
    //printf("where is the out of bound error\n");
    if (strlen(file) >= 20){
        return -1;
    }
    int i_node = check_exist_dir(file);
    //case 1 the file dont exist in our directory
    //printf("should be negative 1 for new file %d\n",i_node);

    if (i_node < 0) {
        //add it to our i_node table 
        //also add it to our directory
        //also add it to our fd 
        int i_node_loc = find_availiable_inode_index(itable);
        int dir_loc = find_availiable_dir_index(root_directory);
        int fd_loc = find_availiable_fd_index(fdtable);
        //printf("%d %d %d \n", dir_loc, fd_loc, i_node_loc);
        if (i_node_loc == -1 || dir_loc == -1 || fd_loc == -1){
            printf("Full Memory !");
            return -1;
        }
        //printf("%d\n", i_node_loc);  
        //printf("%d\n", dir_loc);
        //printf("%d\n", fd_loc);
        //set free variable to 0;
        itable->inode_tab[i_node_loc].free = 0;
        root_directory->director[dir_loc].free = 0;
        fdtable->file_descrip[fd_loc].free = 0;
        //set up directory
        strcpy(root_directory->director[dir_loc].filename, file);
        //printf("string is %s \n", root_directory->director[dir_loc].filename);
        root_directory->director[dir_loc].inode = i_node_loc;
        //set up inode_tab
        //printf("we store i_node_to %d\n", i_node_loc);
        itable->inode_tab[i_node_loc].size = 0;
        itable->inode_tab[i_node_loc].indpointer = -1;
        //set up fd_table
        fdtable->file_descrip[fd_loc].i_node = i_node_loc;
        fdtable->file_descrip[fd_loc].rw_pointer = 0;
        //write inode table and directory back to the memory
        //...yet complete
        write_blocks(1, 12, itable);
        write_blocks(13, 7, root_directory);
        
        //printf("result fd loc is at %d \n", fd_loc);
        return fd_loc;
    } else {
        //case 2 the file exist in our directory
        int fd_loc = check_inode_in_fd(fdtable, i_node);
        if (fd_loc >= 0){
            return fd_loc;
        } else {
            //add it to our fd table
            fd_loc = find_availiable_fd_index(fdtable);
            fdtable->file_descrip[fd_loc].free = 0;
            fdtable->file_descrip[fd_loc].i_node = i_node;
            fdtable->file_descrip[fd_loc].rw_pointer = itable->inode_tab[i_node].size;
            return fd_loc;
        }
    }
}
int check_existence_in_fd(int fd_pointer){
    if (fdtable->file_descrip[fd_pointer].free == 1){
        return -1;
    }
    return 0;
}
int sfs_fclose(int fd_pointer){
    if (fd_pointer < -1){
        return -1;
    }
    int a = check_existence_in_fd(fd_pointer);
    if (a == -1){
        return -1;
    }
    fdtable->file_descrip[fd_pointer].free = 1;
    fdtable->file_descrip[fd_pointer].i_node = -1;
    fdtable->file_descrip[fd_pointer].rw_pointer = -1;
    return 0;
}
int sequencial_block(int num_of_block){
    for (int i =0; i < total_number_of_block; i++){
        if (bit_map.map[i] == 0) {
            continue;
        } else {
            int result = i;
            int k = 0;
            for(int j = 0; j < num_of_block; j++){
                if(bit_map.map[i + j] == 1){
                    k++;
                }
            }
            if(k == num_of_block){
                for (int f = 0; f < num_of_block; f++){
                    bit_map.map[i + f] = 0;
                }
                return result;
            } else {
                continue;
            }
        }
    }
    return -1;
}
int sfs_fwrite(int fd_pointer, const char* buf, int size_of_buf){
    //things to consider...  
    //we will do three cases (approximately)
    //one: we are writing less than one block and starting from 0
    //second: we are writing more than one block and starting from 0
    //third: we are writing more than one block and dont start from 0 (require read command)
    //case 0:
    // since we are writing less than one block and starting from 0
    // we can just allocate a single new block and write the content into there
    // make sure to update the bitmap in the memory
    //check the number of the pointer used using <- (i_nodesize/1024)
            //case 2 ((size_of_buf/1024) > 1;inode_size == 0): 
                    //we are going to start at size 0
                    //and allocate more than one block
                    //allocate ((sizeof(buffer)/1024) + 1) number of sequential new block
                    //write them into there
                    //if the block > 12, we will allocate blocks for undirector pointer
                    //see how much indirect block do we need by doing 1024*12 - size(buffer)
                    //allocate them randomly into bitmap
            //case 3 (((i_nodesize/1024) != 0))
                    // if say (i_node.size/1024) == 7, we will take out the content of
                    //pointer[1] -> pointer[7] and combine it with our current buffer 
                    //read from pointer[7]
                    //check the size of the combine buffer and do result <- (sizeof(newbuffer)/1024)
                    //if say the result = 3
                    //we will allocate 3 more block for this inode and write them to the sfs
                    //say we allocate three more block from before, then we might have
                    //    pointer[7] = x (1/3 of new buffer write to here)
                    //    pointer[8] = y (1/3 of new buffer write to here)
                    //    pointer[9] = z (1/3 of new buffer write to here)
                    //update the size with the size of new buffer
                    //update rw pointer to the new buffer
                    //update the bit map
                    //write all of them into memory
                    //if the block > 12, we will allocate blocks for undirector pointer
                    //see how many undirected block do we need by doing 1024*12 - size(buffer)
                    //allocate them randomly into bitmap
    //get the inode_index
    //no pointer exist
    if(fdtable->file_descrip[fd_pointer].free == 1){
        return -1;
    }
    int i_node_ind = fdtable->file_descrip[fd_pointer].i_node;
    int size_of_i_node = itable->inode_tab[i_node_ind].size;
    int current_rw_pointer = fdtable->file_descrip[fd_pointer].rw_pointer;
    //printf("size of i node = %d \n", size_of_i_node);
    //printf("current_rw_pointer = %d \n", current_rw_pointer );  
    //printf("we are working with inode %d \n", i_node_ind);
    if (current_rw_pointer > size_of_i_node){
        printf("rw pointer is pointing at somewhere weird\n");
        return -1;
    }
    //if using the knowledge learnt from data structure course
    //the following case will be the most simple case 
    //where we are only going to write to the first line
    //printf("%d\n", size_of_i_node);
    indirectblock indblock;
    int bit;
    //printf("size of inode is%d\n", size_of_i_node);
    if (itable->inode_tab[i_node_ind].indpointer == -1){
        //intialize a pointer
        bit = next_availiable_bit();
        if (bit == -1){
            printf("no more space");
            return -1;
        }
        bit_map.map[bit] = 0;
        itable->inode_tab[i_node_ind].indpointer = bit;
    } else {
        read_blocks(itable->inode_tab[i_node_ind].indpointer, 1, &indblock);
    }
    if (size_of_i_node == 0 && size_of_buf < 1024){ //case 1
        char *Buffers = malloc(1024);
        strcpy(Buffers, buf);
        //find out how many block that we have to allocate using size_of_buf
        int num_block = ((size_of_buf)/1024) + 1;
        //only one block to allocate
        if (num_block == 1) {
            //find out the next avaliable slot using bitmap
            int bits = next_availiable_bit();
            bit_map.map[bits] = 0;
            //make pointer[0] point to "that node"
            itable->inode_tab[i_node_ind].pointer[0] = bits;
            //printf("%d\n", bit );
            //update the size 
            itable->inode_tab[i_node_ind].size += (size_of_buf);
            //update the rw pointer 
            fdtable->file_descrip[fd_pointer].rw_pointer = itable->inode_tab[i_node_ind].size;
            //printf("we are writing to block %d \n", bit);
            //printf("we are writigin %s \n", diskBuffer);
            //write the content to "bit location for 1 block"
            write_blocks(bits, 1, Buffers);
            write_blocks(bit, 1, &indblock);
            //printf("%d\n", itable->inode_tab[i_node_ind].pointer[0]);
            //printf("%d\n",  itable->inode_tab[i_node_ind].size);
            //printf("%d\n",  fdtable->file_descrip[fd_pointer].rw_pointer);
            //printf("current file size is %d \n",  itable->inode_tab[i_node_ind].size);
            //printf("current file rw pointer is %d \n",  fdtable->file_descrip[fd_pointer].rw_pointer);
        }
        return size_of_buf;
   } else if (size_of_i_node == 0){ //case 2 size != 0 
                    //case 2 ((i_nodesize/1024) > 1;inode_size == 0): 
                    //we are going to start at size 0
                    char *Bufferss = malloc(((size_of_buf/1024) + 1)*1024);
                    strcpy(Bufferss, buf);
                    int num_block_to_allocate = ((size_of_buf/1024) + 1);
                    if (num_block_to_allocate <= 12){
                    int a = sequencial_block(num_block_to_allocate);
                    if (a == -1){
                        printf("no availiable sequential block\n");
                        return -1;
                    }
                    //add the pointer to our i_node
                    write_blocks(a, num_block_to_allocate, Bufferss);
                    itable->inode_tab[i_node_ind].size = (size_of_buf);
                    fdtable->file_descrip[fd_pointer].rw_pointer = itable->inode_tab[i_node_ind].size;
                    for(int i = 0; i < num_block_to_allocate; i ++){
                        itable->inode_tab[i_node_ind].pointer[i] = a;
                        a = a + 1;
                    }
                    write_blocks(bit, 1, &indblock);
                    } else { //we are required to use indirect pointer
                        int a = sequencial_block(num_block_to_allocate);
                        if (a == -1){
                        printf("no availiable sequential block\n");
                        return -1;
                    }
                        write_blocks(a, num_block_to_allocate, Bufferss);
                        itable->inode_tab[i_node_ind].size = (size_of_buf);
                        fdtable->file_descrip[fd_pointer].rw_pointer = itable->inode_tab[i_node_ind].size;
                        int ind = 0;
                        for(int i = 0; i < num_block_to_allocate; i ++){
                           if( i < 12){
                           itable->inode_tab[i_node_ind].pointer[i] = a;
                           a = a + 1;} else {
                               indblock.pointer[ind] = a;
                               a = a + 1;
                               ind = ind + 1;
                           }}
                      write_blocks(bit, 1, &indblock);
                    }
                    //allocate ((sizeof(buffer)/1024) + 1) number of sequential new block
                    //write them into there
                    //if the block >= 12, we will allocate blocks for undirector pointer
                    //see how much indirect block do we need by doing 1024*12 - size(buffer)
                    //allocate them randomly into bitmap
                    //printf("current file size is %d \n",  itable->inode_tab[i_node_ind].size);
                    //printf("current file rw pointer is %d \n",  fdtable->file_descrip[fd_pointer].rw_pointer);
        return size_of_buf;
   } 
   else {
       /*
       int i_node_ind = fdtable->file_descrip[fd_pointer].i_node;
       int size_of_i_node = itable->inode_tab[i_node_ind].size;
       int current_rw_pointer = fdtable->file_descrip[fd_pointer].rw_pointer;
       */
       //block we will start writing at
       char *Buffee = malloc((((size_of_buf)/1024) + 1)*1024);
       strcpy(Buffee, buf);
       int block_writing_at = ((current_rw_pointer/1024) + 1);
       int byte_writing_at = (current_rw_pointer);
       char *Bufferss = malloc((((size_of_buf + current_rw_pointer)/1024) + 1)*1024);
       int num_block_to_allocate = (((size_of_buf + current_rw_pointer)/1024) + 1);
       //read from original block
       //int a =(((size_of_buf + current_rw_pointer)/1024) + 1)*1024;
       //printf("the string that we are writing in will be %s \n", Buffee);
       //printf("block writing at is that we are writing in will be %d \n", block_writing_at);
       //printf("the amount of space that we malloc for bufferss is %d \n", a);
       read_blocks(itable->inode_tab[i_node_ind].pointer[0], block_writing_at, Bufferss);
       //printf("the string that we took out is %s \n", Bufferss);
       memcpy(Bufferss + (byte_writing_at - 2), Buffee, size_of_buf);
       //printf("the result buffers will be %s \n", Bufferss);
       //bufferss = the block the we will be working with
       int num_block_in_inode = ((size_of_i_node/1024) + 1);
       //clean the i_node since we are not using the original content
       //also clean the original map
       for (int i = 0; i < num_block_in_inode; i++){
            bit_map.map[itable->inode_tab[i_node_ind].pointer[i]] = 1;
            itable->inode_tab[i_node_ind].pointer[i] = -1;
       }
       if (num_block_to_allocate <= 12){
           int a = sequencial_block(num_block_to_allocate);
            if (a == -1){
                printf("no availiable sequential block\n");
                    return -1;}
            write_blocks(a, num_block_to_allocate,Bufferss);
            itable->inode_tab[i_node_ind].size = size_of_buf + current_rw_pointer;
            fdtable->file_descrip[fd_pointer].rw_pointer = itable->inode_tab[i_node_ind].size;
            for(int i = 0; i < num_block_to_allocate; i ++){
                        itable->inode_tab[i_node_ind].pointer[i] = a;
                        a = a + 1;
                    }
            write_blocks(bit, 1, &indblock);} 
            else {
                    int a = sequencial_block(num_block_to_allocate);
                    if (a == -1){
                        printf("no availiable sequential block\n");
                        return -1;}
                        write_blocks(a, num_block_to_allocate,Bufferss);
                        itable->inode_tab[i_node_ind].size = size_of_buf + current_rw_pointer;
                        fdtable->file_descrip[fd_pointer].rw_pointer = itable->inode_tab[i_node_ind].size;
                        int ind = 0;
                        for(int i = 0; i < num_block_to_allocate; i ++){
                           if( i < 12){
                           itable->inode_tab[i_node_ind].pointer[i] = a;
                           a = a + 1;} else {
                               indblock.pointer[ind] = a;
                               a = a + 1;
                               ind = ind + 1;
                           }}
                            write_blocks(bit, 1, &indblock);
                    }
                    // printf("current file size is %d \n",  itable->inode_tab[i_node_ind].size);
                    // printf("current file rw pointer is %d \n",  fdtable->file_descrip[fd_pointer].rw_pointer);
                    return size_of_buf;
       }
       //case 3 (((i_nodesize/1024) != 0))
                    // if say (i_node.size/1024) == 7, we will take out the content of
                    //pointer[7] and combine it with our current buffer 
                    //read from pointer[7]
                    //check the size of the combine buffer and do result <- (sizeof(newbuffer)/1024)
                    //if say the result = 3
                    //we will allocate 3 more block for this inode and write them to the sfs
                    //say we allocate three more block from before, then we might have
                    //    pointer[7] = x (1/3 of new buffer write to here)
                    //    pointer[8] = y (1/3 of new buffer write to here)
                    //    pointer[9] = z (1/3 of new buffer write to here)
                    //update the size with the size of new buffer
                    //update rw pointer to the new buffer
                    //update the bit map
                    //write all of them into memory
                    //if the block > 12, we will allocate blocks for undirector pointer
                    //see how many undirected block do we need by doing 1024*12 - size(buffer)
                    //allocate them randomly into bitmap
}
int sfs_fread(int fd_pointer, char* buf, int size_of_buf){
    //find the inode index
    //what is the size of the inode 
    //separate into three cases
    //case 1: most simple 1, rw pointer at 0 and only one block (already implmented)
    //case 2: read more than one block and starting from 0
    //        this will be easy since we implment the block sequentially
    //        **read(location of pointer[0], (sizeof(buffer))/1024, buf)
    //        make sure to get the block from indirect pointer when 
    //        (sizeof(buffer) / 1024 + 1) > 12
    //        update the rw pointer and memory in the end
    //case 3: read more than one block and starting from x
    //        get the block where x is, using sizeof(x)/1024
    //        for example: sizeof(x)/1024 = 6, we will start at pointer[6]
    //        for example: sizeof(x)/1024 = 8, we will start at pointer[8]
    //        for example: sizeof(x)/1024 = 13, we will start at first element of the indirect pointer
    //        read the content starting from the pointer
    //        **read(location of pointer[0], sizeof(buffer)/1024, buf)
    //        update the rw pointer and memory in the end
    if (fdtable->file_descrip[fd_pointer].free == 1){
        return -1;
    }
    //find the i_node in the i_node table as well as the size
    int i_node_ind = fdtable->file_descrip[fd_pointer].i_node;
    int size_of_i_node = itable->inode_tab[i_node_ind].size;
    int pointer =  fdtable->file_descrip[fd_pointer].rw_pointer;
    if (pointer > size_of_i_node){
        printf("reading something that is out of range");
        return -1;
    }
    //where is the current rw pointer
    //case 1 rw pointer = 0
    if (pointer == 0 && size_of_i_node < 1024){
        //start reading from the first pointer
        int block = itable->inode_tab[i_node_ind].pointer[0];
        //allocate a space for 1 block
        char *bufferess = malloc(1024);
        read_blocks(block, 1, bufferess);
        strcpy(buf, bufferess);
        if (size_of_buf > size_of_i_node){
            fdtable->file_descrip[fd_pointer].rw_pointer = size_of_i_node;}
        else {
            fdtable->file_descrip[fd_pointer].rw_pointer = size_of_buf;
        }
        free(bufferess);
    } else if (pointer == 0){
        //when size of buffer > size of i node 
        //we are just going to read everything into buf
        //since we have sequential block, once we have the rwpointer 
        //we can just read the sequential number of the block starting from the location 
        //of the pointer
        int num_block = ((size_of_buf/1024) + 1);
        char *bufferss = malloc(((size_of_buf/1024) + 1)*1024);
        int block = itable->inode_tab[i_node_ind].pointer[0];
        read_blocks(block, num_block, bufferss);
        strcpy(buf, bufferss);
        if (size_of_buf >size_of_i_node ){
        fdtable->file_descrip[fd_pointer].rw_pointer = size_of_i_node;}
        else {
            fdtable->file_descrip[fd_pointer].rw_pointer = size_of_buf;
        }
        free(bufferss);
    } else if (size_of_buf + pointer >= size_of_i_node){
        char* buffeerrs = malloc(((size_of_i_node/1024)+1)*1024);
        int num_block = ((size_of_i_node/1024) + 1);
        int block = itable->inode_tab[i_node_ind].pointer[0];
        //read everything into buffeerrrs
        read_blocks(block, num_block, buffeerrs);
        //copy the part that we are interested (where the rw is pointing to into buf)
        memcpy(buf, buffeerrs + pointer, size_of_buf);
        fdtable->file_descrip[fd_pointer].rw_pointer = size_of_i_node;
        free(buffeerrs);
    } 
    else {
        //where pointer is not 0
       // printf("we have arrived here\n");
        char* buffeerrs = malloc(((size_of_i_node/1024)+1)*1024);
        int num_block = ((size_of_i_node/1024) + 1);
        int block = itable->inode_tab[i_node_ind].pointer[0];
        //read everything into buffeerrrs
        read_blocks(block, num_block, buffeerrs);
        //copy it to buf
        //printf("buffeerrs is %s \n", buffeerrs);
        //also again copy everything that we are interested into buf
        memcpy(buf, buffeerrs + pointer , (size_of_i_node));
        fdtable->file_descrip[fd_pointer].rw_pointer = size_of_buf + pointer;
        free(buffeerrs);
    }
    return size_of_buf;
}
int sfs_fseek(int fd_pointer, int size){
    //make rw pointer of the fd pointer -> pointing to 0
    if (fdtable->file_descrip[fd_pointer].free == 1){
        return -1;
    }
    fdtable->file_descrip[fd_pointer].rw_pointer = size;
    return 1;
}
int sfs_remove(char* buf){
    //case 1 only one block to clean (already implemented)
    //case 2 more than 1 block to clean
    //        also very easy, if we have more than 12 blocks then we loop throught 12 block
    //        if we have 13 blocks then we loop throught 12 block cleaning + 1 indirect pointer cleaning
    //        this is not implmented simply because write and read is not done yet.
    //        rest of cleaning is the same as case 1 (already implmented)
    //        the key of this problem is to clean every pointer block with 0
    int i_node = check_exist_dir(buf);
    if (i_node == -1){
        printf("no need to remove since the file dont even exist");
        return -1;
    }
    if (itable->inode_tab[i_node].size < 1024){
        //clean the memory 
        int block = itable->inode_tab[i_node].pointer[0];
        void* bufe = malloc(1024);
        memset(bufe, 0, 1024);
        write_blocks(block, 1, bufe);
        //reset bitmap
        int indblock = itable->inode_tab[i_node].indpointer;
        void* bufe2 = malloc(1024);
        memset(bufe2, 0, 1024);
        write_blocks(indblock, 1, bufe2);
        bit_map.map[indblock] = 1;
        bit_map.map[block] = 1;
        //reset inode table
        itable->inode_tab[i_node].free = 1;
        itable->inode_tab[i_node].size = -1;
        itable->inode_tab[i_node].pointer[0] = -1;
        itable->inode_tab[i_node].indpointer = -1;
        //reset dir
        int dir_index = check_dir_pos(buf);
        root_directory->director[dir_index].free = 1;
        root_directory->director[dir_index].inode = -1;
        char* string = " ";
        memcpy(root_directory->director[dir_index].filename, string, 19);
        //reset fd
        int fd_pointers = check_inode_in_fd(fdtable, i_node);
        sfs_fclose(fd_pointers);
        free(bufe);
        free(bufe2);
        //write the new thing into table
        write_blocks(1, 12, itable);
        write_blocks(13, 7, root_directory);
        write_blocks(20, 3, &bit_map);
    }
    else {
        //clean the memory 
        int block = itable->inode_tab[i_node].pointer[0];
        int size_of_inode =  itable->inode_tab[i_node].size;
        int num_block_in_inode = ((size_of_inode/1024) + 1);
        int a = itable->inode_tab[i_node].pointer[0];
        for (int i = 0; i < num_block_in_inode; i++){
            if (num_block_in_inode < 12){
            bit_map.map[itable->inode_tab[i_node].pointer[i]] = 1;
            itable->inode_tab[i_node].pointer[i] = -1;} else {
                bit_map.map[a] = 1;
            }
            a++;
       }

        int indblock = itable->inode_tab[i_node].indpointer;
        void* bufe2 = malloc(1024);
        memset(bufe2, 0, 1024);
        write_blocks(indblock, 1, bufe2);

        void* bufe = malloc(size_of_inode);
        memset(bufe, 0, size_of_inode);
        write_blocks(block, num_block_in_inode, bufe);
        //reset inode table
        bit_map.map[indblock] = 1;
        itable->inode_tab[i_node].indpointer = -1;
        itable->inode_tab[i_node].free = 1;
        itable->inode_tab[i_node].size = -1;
        //reset dir
        int dir_index = check_dir_pos(buf);
        root_directory->director[dir_index].free = 1;
        root_directory->director[dir_index].inode = -1;
        char* string = " ";
        memcpy(root_directory->director[dir_index].filename, string, 19);
        //reset fd
        int fd_pointers = check_inode_in_fd(fdtable, i_node);
        sfs_fclose(fd_pointers);
        free(bufe);
        free(bufe2);
        //write the new thing into table
        write_blocks(1, 12, itable);
        write_blocks(13, 7, root_directory);
        write_blocks(20, 3, &bit_map);
    }
    return 1;
}
int sfs_getnextfilename(char* file){
    for (int i = root_directory->current_node; i < 200; i++){
        if (root_directory->director[i].free == 0){
            strcpy(file, root_directory->director[i].filename);
            root_directory->current_node++;
            return 1;
        }
    }
    root_directory->current_node = 0;
    return -1;
}
int sfs_getfilesize(const char* filename){
    char *Buffers = malloc(sizeof(filename));
    strcpy(Buffers, filename);
    int result = check_exist_dir(Buffers);
    if (result == -1){return -1;}else {
        return itable->inode_tab[result].size;
        }
}
/*
int main(){
    mksfs(1);
    int f = sfs_fopen("some_name.txt");
    char my_data[] = "The quick brown fox jumps over the lazy dog";
    char out_data[1024];
    sfs_fwrite(f, my_data, sizeof(my_data)+1);
    sfs_fseek(f, 0);
    printf("rw pointer now has number %d \n", fdtable->file_descrip[f].rw_pointer);
    printf("fd pointing to slot %d \n", fdtable->file_descrip[f].i_node);
    sfs_fread(f, out_data, sizeof(out_data)+1);
    //printf("%s\n", out_data);
    sfs_fclose(f);
    return 1;
}
*/

