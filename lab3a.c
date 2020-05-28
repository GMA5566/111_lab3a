// NAME: Guanqun Ma, Larry Qu
// EMAIL: maguanqun0212@gmail.com, qularry1100@gmail.com
// ID: 305331164, 105206585

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "ext2_fs.h"


int image_fd;
struct ext2_super_block superBLK;
unsigned int block_size;


void FreeBlockEntries(){
        //1 means block is used, 0 means block is free
        struct ext2_group_desc groupBLK;

        //SUPERBLOCK,1024,1000,1024,128,8192,1000,11
        //GROUP,0,1024,1000,513,567,3,4,5

        //we are only working on single group
        off_t offset = 1024 + block_size; //offset for superblock + superblock's size to get the offset for groups
        pread(image_fd, &groupBLK, 32, offset); //size of the block group descriptor structure is 32 bytes

        //block where bitmap is stored
        int bitmap_loc = groupBLK.bg_block_bitmap;

        //offset of bitmap
        unsigned char bitmap[1024] = {0};
        off_t bitmap_offset = 1024 + (bitmap_loc-1)*block_size; //bitmap is block 3, which is 2 blocks after superblock
        pread(image_fd, &bitmap, 128, bitmap_offset); //size of bitmap is numberofblocks/8 bytes long

        //traverse the bitmap and print the block number if the bitmap bit is 0(free)
        int i=0;
        for(; i<128; i++){
                int j=0;
                for(; j<8; j++){
                        if( ((bitmap[i] >> j) & 1) == 0){ //if the bit is 0
                                dprintf(STDOUT_FILENO, "BFREE,%d\n", i*8+j+1);
                        }
                }
        }
}


void FreeInodeEntries(){
        //1 means block is used, 0 means block is free
        struct ext2_group_desc groupBLK;

        //SUPERBLOCK,1024,1000,1024,128,8192,1000,11
        //GROUP,0,1024,1000,513,567,3,4,5
        //inode numbers go from 1 to 1000

        //we are only working on single group
        off_t offset = 1024 + block_size; //offset for superblock + superblock's size to get the offset for groups
        pread(image_fd, &groupBLK, 32, offset); //size of the block group descriptor structure is 32 bytes

        //block where bitmap is stored
        int bitmap_loc = groupBLK.bg_inode_bitmap;

        //number of inodes starting from 1
        int num_inodes = superBLK.s_inodes_per_group;

        //offset of bitmap
        unsigned char bitmap[1024] = {0};
        off_t bitmap_offset = 1024 + (bitmap_loc-1)*block_size; //bitmap is block 4, which is 3 blocks after superblock
        pread(image_fd, &bitmap, num_inodes/8, bitmap_offset); //size of bitmap is numberofblocks/8 bytes long

        //traverse the bitmap and print the inode number if the bitmap bit is 0(free)
        int i=0;
        for(; i<num_inodes/8; i++){
                int j=0;
                for(; j<8; j++){
                        if( ((bitmap[i] >> j) & 1) == 0){ //if the bit is 0
                                dprintf(STDOUT_FILENO, "IFREE,%d\n", i*8+j+1);
                        }
                }
        }
}

void SuperblockSummary()
{
    off_t offset = 1024; //for ext2: "primary copy of superblock is stored at offset of 1024 bytes from the start of the device"
    pread(image_fd, &superBLK, sizeof(superBLK), offset);
    if(superBLK.s_magic != EXT2_SUPER_MAGIC)
    {
        fprintf(stderr, "Error, could not find super block\n");
        exit(2);
    }
    dprintf(STDOUT_FILENO, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", superBLK.s_blocks_count, superBLK.s_inodes_count, block_size,
        superBLK.s_inode_size, superBLK.s_blocks_per_group, superBLK.s_inodes_per_group, superBLK.s_first_ino);
}


void GroupSummary()
{
    struct ext2_group_desc groupBLK;
    //we are only working on single group
    off_t offset = 1024 + block_size; //offset for superblock + superblock's size to get the offset for groups
    pread(image_fd, &groupBLK, 32, offset); //size of the block group descriptor structure is 32 bytes
    dprintf(STDOUT_FILENO, "GROUP,0,%d,%d,%d,%d,%d,%d,%d\n", superBLK.s_blocks_count, superBLK.s_inodes_per_group, groupBLK.bg_free_blocks_count,
        groupBLK.bg_free_inodes_count, groupBLK.bg_block_bitmap, groupBLK.bg_inode_bitmap, groupBLK.bg_inode_table);
}


void InodeSummary()
{
    __u32 num_inodes = superBLK.s_inodes_count;
    struct ext2_inode* inode_id = (struct ext2_inode*)malloc(num_inodes * sizeof(struct ext2_inode));
    off_t offset = 1024 + 4 * block_size; //offset for superblock + 4 blocks whose size is "block_size"
    size_t inode_size = sizeof(struct ext2_inode);
    __u32 i = 0;
    for(; i < num_inodes; i++)
    {
        pread(image_fd, &inode_id[i], inode_size, offset + i * inode_size);

        if(inode_id[i].i_mode == 0 || inode_id[i].i_links_count == 0)
        {
            continue;
        }

        //file type
        char file_type = '?';
        switch (inode_id[i].i_mode & S_IFMT)
        {
            case S_IFREG:
                file_type = 'f'; //regular file
                break;
            case S_IFDIR:
                file_type = 'd'; //directory
                break;
            case S_IFLNK:
                file_type = 's'; //symbolic link
                break;
        }

        //mode
        __u16 mode = inode_id[i].i_mode & 0xFFF; //to get the low order 12-bits

        //time
        char ctime_storage[20], mtime_storage[20], atime_storage[20];
        time_t temp_time = inode_id[i].i_ctime;
        struct tm *ptm = gmtime(&temp_time);
        strftime(ctime_storage, 20, "%m/%d/%y %H:%M:%S", ptm);
        temp_time = inode_id[i].i_mtime;
        ptm = gmtime(&temp_time);
        strftime(mtime_storage, 20, "%m/%d/%y %H:%M:%S", ptm);
        temp_time = inode_id[i].i_atime;
        ptm = gmtime(&temp_time);
        strftime(atime_storage, 20, "%m/%d/%y %H:%M:%S", ptm);

        dprintf(STDOUT_FILENO, "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d", i+1, file_type, mode, inode_id[i].i_uid,
            inode_id[i].i_gid, inode_id[i].i_links_count, ctime_storage, mtime_storage, atime_storage,
            inode_id[i].i_size, inode_id[i].i_blocks);

        if((file_type != 's') || (file_type == 's' && inode_id[i].i_blocks != 0))
        {
            int j = 0;
            for(; j < 15; j++)
            {
                dprintf(STDOUT_FILENO, ",%d", inode_id[i].i_block[j]);
            }
        }
        dprintf(STDOUT_FILENO, "\n");

        //directory entries
        if(file_type == 'd')
        {
           int k = 0;
           for(; k < EXT2_NDIR_BLOCKS; k++) //first 12 are direct blocks
            {
                if(inode_id[i].i_block[k] != 0) //directory entry is not empty
                {
                    unsigned int logicalOffset = 0;
                    while(logicalOffset < block_size)
                    {
                        off_t dir_offset = inode_id[i].i_block[k] * block_size + logicalOffset;
                        struct ext2_dir_entry dirEntry;
                        pread(image_fd, &dirEntry, sizeof(struct ext2_dir_entry), dir_offset);
                        if(dirEntry.inode != 0) //non-zero inode number
                        {
                            dprintf(STDOUT_FILENO, "DIRENT,%d,%d,%d,%d,%d,'%s'\n", i+1, logicalOffset, dirEntry.inode,
                                dirEntry.rec_len, dirEntry.name_len, dirEntry.name);
                        }

                        logicalOffset += dirEntry.rec_len;
                    }
                }
            }
        }


        //indirect is i_block[12]
        if(inode_id[i].i_block[12] > 0)
        {
            int data_blocks[1024] = {0};
            off_t indirect_offset = inode_id[i].i_block[12]*block_size; //first indirect block
            pread(image_fd, &data_blocks, block_size, indirect_offset); //get all the data blocks pointed to by the indirect block
            int j = 0;
            for(; j < 1024; j++){
                    int referenced_block_number = data_blocks[j];
                    if( referenced_block_number){ //if the block number is not null, follow it
                            int indirect_block_number = inode_id[i].i_block[12];
                            dprintf(STDOUT_FILENO, "INDIRECT,%d,1,%d,%d,%d\n", i+1, j+12,
                                    indirect_block_number, referenced_block_number );
                    }
            }
        }

        //double indirect is i_block[13]
        if(inode_id[i].i_block[13] > 0)
        {
            int level1_indirect_blocks[1024] = {0};
            off_t level2_indirect_offset = inode_id[i].i_block[13]*block_size; //level1 indirect block numbers
            pread(image_fd, &level1_indirect_blocks, block_size, level2_indirect_offset); //get all the data blocks pointed to by level2indirectblock
            int k = 0;
            for(; k < 1024; k++){
                    int referenced_block_number = level1_indirect_blocks[k];
                    if( referenced_block_number){ //if the level1 indirect block number is not null, follow it
                            int data_blocks[1024] = {0};
                            off_t indirect_offset = referenced_block_number*block_size; //level1 indirect block
                            pread(image_fd, &data_blocks, block_size, indirect_offset); //get all the data blocks pointed to by the indirect block
                            int j = 0;
                    
                            int indirect_block_number = inode_id[i].i_block[13];
                            dprintf(STDOUT_FILENO, "INDIRECT,%d,2,%d,%d,%d\n", i+1, 256+12+k,
                                    indirect_block_number, referenced_block_number );
                            
                            for(; j < 1024; j++){
                                    int referenced_block_number = data_blocks[j];
                                    if( referenced_block_number){ //if the block number is not null, follow it
                                            dprintf(STDOUT_FILENO, "INDIRECT,%d,1,%d,%d,%d\n", i+1, 256+12+j,
                                                    level1_indirect_blocks[k], referenced_block_number );
                                    }
                            }
                    }
            }
        }

        //triple indirect is i_block[14]
        if(inode_id[i].i_block[14] > 0)
        {
            // unsigned char level2_indirect_blocks[1024] = {0};
            int level2_indirect_blocks[1024] = {0};
            off_t level3_indirect_offset = inode_id[i].i_block[14]*block_size; //level3 indirect block
            pread(image_fd, &level2_indirect_blocks, block_size, level3_indirect_offset); //get all the level2 blocks pointed to by level3 block
            int m = 0;
            for(; m < 1024; m++){
                    int referenced_block_number = level2_indirect_blocks[m];
                    if( referenced_block_number){ //if the block number is not null, follow it
                            int indirect_block_number = inode_id[i].i_block[14];
                            dprintf(STDOUT_FILENO, "INDIRECT,%d,3,%d,%d,%d\n", i+1, 65536+256+12+m,
                                    indirect_block_number, referenced_block_number );

                            //level2 indirect
                            // unsigned char level1_indirect_blocks[1024] = {0};
                            int level1_indirect_blocks[1024] = {0};
                            off_t level2_indirect_offset = referenced_block_number*block_size; //level1 indirect block numbers
                            pread(image_fd, &level1_indirect_blocks, block_size, level2_indirect_offset);
                            int k = 0;
                            for(; k < 1024; k++){
                                    int referenced_block_number = level1_indirect_blocks[k];
                                    if( referenced_block_number){ //if the level1 indirect block number is not null, follow it
                                            
                                            dprintf(STDOUT_FILENO, "INDIRECT,%d,2,%d,%d,%d\n", i+1, 65536+256+12+k,
                                                    level2_indirect_blocks[m], referenced_block_number );
                                            
                                            // unsigned char data_blocks[1024] = {0};
                                            int data_blocks[1024] = {0};
                                            off_t indirect_offset = referenced_block_number*block_size; //level1 indirect block
                                            pread(image_fd, &data_blocks, block_size, indirect_offset); // to by the indirect block
                                            
                                            int j = 0;
                                            for(; j < 1024; j++){
                                                    int referenced_block_number = data_blocks[j];
                                                    if( referenced_block_number){ //if the block number is not null, follow it
                                                            dprintf(STDOUT_FILENO, "INDIRECT,%d,1,%d,%d,%d\n", i+1, 65536+256+12+j,
                                                                    level1_indirect_blocks[k], referenced_block_number );
                                                    }
                                            }
                                    }
                            }
                    }
            }
        }
    }
    free(inode_id);
}


int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        fprintf(stderr, "Error, incorrect number of commands\n");
        exit(1);
    }
    image_fd = open(argv[1], O_RDONLY);
    if(image_fd < 0)
    {
        fprintf(stderr, "Error, unable to open the image file\n");
        exit(1);
    }

    block_size = EXT2_MIN_BLOCK_SIZE << superBLK.s_log_block_size; //get the size of block

    SuperblockSummary();
    GroupSummary();
    FreeBlockEntries();
    FreeInodeEntries();
    InodeSummary();
    // IndirectReferences();

    return 0;
}
