#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

// --- EXT2 Basic Structures ---
struct Superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    // -- Important: Inode size is at offset 88 --
    uint32_t s_first_ino;
    uint16_t s_inode_size;  // <--- THIS IS CRITICAL
};

struct GroupDescriptor {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
};

struct Inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osync1;
    uint32_t i_block[15]; 
};

struct DirectoryEntry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[255];
};

int fd;
uint32_t block_size;
uint32_t inode_table_block;
uint16_t inode_size;

void read_inode(int inode_no, struct Inode *inode) {
    // Offset = (InodeTableBlock * BlockSize) + (IndexInTable * InodeSize)
    off_t offset = ((off_t)inode_table_block * block_size) + ((inode_no - 1) * inode_size);
    lseek(fd, offset, SEEK_SET);
    read(fd, inode, sizeof(struct Inode));
}

void do_ls() {
    struct Inode root_inode;
    read_inode(2, &root_inode); 

    char *block_buf = malloc(block_size);
    lseek(fd, (off_t)root_inode.i_block[0] * block_size, SEEK_SET);
    read(fd, block_buf, block_size);

    uint32_t current_pos = 0;
    printf("%-10s %-10s %-10s %s\n", "INODE", "TYPE", "SIZE", "NAME");
    
    while (current_pos < root_inode.i_size) {
        struct DirectoryEntry *entry = (struct DirectoryEntry *)(block_buf + current_pos);
        if (entry->inode == 0) break;

        char name[256];
        memcpy(name, entry->name, entry->name_len);
        name[entry->name_len] = '\0';

        // Get file size by reading its inode
        struct Inode temp_inode;
        read_inode(entry->inode, &temp_inode);

        printf("%-10d %-10s %-10d %s\n", 
               entry->inode, 
               (entry->file_type == 2 ? "DIR" : "FILE"), 
               temp_inode.i_size,
               name);

        current_pos += entry->rec_len;
    }
    free(block_buf);
}

void do_cp(char *target_name) {
    struct Inode root_inode;
    read_inode(2, &root_inode);

    char *block_buf = malloc(block_size);
    lseek(fd, (off_t)root_inode.i_block[0] * block_size, SEEK_SET);
    read(fd, block_buf, block_size);

    uint32_t current_pos = 0;
    uint32_t target_inode_no = 0;

    while (current_pos < root_inode.i_size) {
        struct DirectoryEntry *entry = (struct DirectoryEntry *)(block_buf + current_pos);
        if (entry->inode == 0) break;

        if (strlen(target_name) == entry->name_len && 
            strncmp(entry->name, target_name, entry->name_len) == 0) {
            target_inode_no = entry->inode;
            break;
        }
        current_pos += entry->rec_len;
    }

    if (target_inode_no == 0) {
        printf("Error: File '%s' not found in image.\n", target_name);
        free(block_buf);
        return;
    }

    struct Inode file_inode;
    read_inode(target_inode_no, &file_inode);

    int out_fd = open(target_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    uint32_t remaining = file_inode.i_size;

    for (int i = 0; i < 12 && remaining > 0; i++) {
        uint32_t to_read = (remaining < block_size) ? remaining : block_size;
        lseek(fd, (off_t)file_inode.i_block[i] * block_size, SEEK_SET);
        read(fd, block_buf, block_size);
        write(out_fd, block_buf, to_read);
        remaining -= to_read;
    }

    close(out_fd);
    printf("Successfully copied '%s' (%d bytes) to host.\n", target_name, file_inode.i_size);
    free(block_buf);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <image> <ls|cp> [filename]\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("Error opening image"); return 1; }

    struct Superblock sb;
    lseek(fd, 1024, SEEK_SET);
    read(fd, &sb, sizeof(sb));

    // Magic number check (Optional but good)
    if (sb.s_magic != 0xEF53) {
        printf("Error: Not a valid EXT2 filesystem.\n");
        return 1;
    }

    block_size = 1024 << sb.s_log_block_size;
    inode_size = sb.s_inode_size; // Detect automatically (usually 256)

    struct GroupDescriptor gd;
    // If block size is 4KB, SB and GD are in block 0 and 1.
    // If block size is 1KB, SB and GD are in block 1 and 2.
    uint32_t gd_block = (block_size == 1024) ? 2 : 1;
    lseek(fd, (off_t)gd_block * block_size, SEEK_SET);
    read(fd, &gd, sizeof(gd));
    inode_table_block = gd.bg_inode_table;

    if (strcmp(argv[2], "ls") == 0) {
        do_ls();
    } else if (strcmp(argv[2], "cp") == 0 && argc == 4) {
        do_cp(argv[3]);
    } else {
        printf("Invalid command or missing filename.\n");
    }

    close(fd);
    return 0;
}