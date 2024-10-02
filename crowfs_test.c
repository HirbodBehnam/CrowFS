#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crowfs.h"

#include <time.h>
#include <stdbool.h>

struct {
    size_t size;
    char *buffer;
} memory_buffer;

union CrowFSBlock *std_allocate_mem_block(void) {
    return calloc(1, sizeof(union CrowFSBlock));
}

void std_free_mem_block(union CrowFSBlock *block) {
    free(block);
}

int mem_write_block(uint32_t block_index, const union CrowFSBlock *block) {
    memcpy(memory_buffer.buffer + block_index * 4096, block, sizeof(union CrowFSBlock));
    return 0;
}

int mem_read_block(uint32_t block_index, union CrowFSBlock *block) {
    memcpy(block, memory_buffer.buffer + block_index * 4096, sizeof(union CrowFSBlock));
    return 0;
}

uint32_t mem_total_blocks(void) {
    return memory_buffer.size / CROWFS_BLOCK_SIZE;
}

int64_t std_current_date(void) {
    return time(NULL);
}

void mem_fs_init(struct CrowFS *fs, size_t size) {
    if (memory_buffer.buffer != NULL)
        free(memory_buffer.buffer);
    memory_buffer.buffer = calloc(size, sizeof(char));
    memory_buffer.size = size;
    fs->allocate_mem_block = std_allocate_mem_block,
            fs->free_mem_block = std_free_mem_block,
            fs->write_block = mem_write_block,
            fs->read_block = mem_read_block,
            fs->total_blocks = mem_total_blocks,
            fs->current_date = std_current_date,
            crowfs_new(fs);
}

int test_open_file();

int test_create_folder();

int test_stat();

int test_read_write_file_small();

int test_read_write_file_direct();

int test_read_write_file_indirect();

int test_write_file_full();

int test_write_folder_full();

int test_delete_file();

int test_delete_folder();

int test_move();

int test_read_dir();

int test_disk_full();

int main(int argc, char **argv) {
    if (argc != 2) {
        puts("Enter the test number as argument");
        return 1;
    }
    int test_number = atoi(argv[1]);
    switch (test_number) {
        case 1:
            return test_open_file();
        case 2:
            return test_create_folder();
        case 3:
            return test_stat();
        case 4:
            return test_read_write_file_small();
        case 5:
            return test_read_write_file_direct();
        case 6:
            return test_read_write_file_indirect();
        case 7:
            return test_write_file_full();
        case 8:
            return test_write_folder_full();
        case 9:
            return test_delete_file();
        case 10:
            return test_delete_folder();
        case 11:
            return test_move();
        case 12:
            return test_read_dir();
        case 13:
            return test_disk_full();
        default:
            puts("invalid test number");
            return 1;
    }
}

int test_open_file() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024);
    uint32_t fd, fd_parent, fd_temp;
    assert(crowfs_open(&fs, "/hello", &fd, &fd_parent, 0) == CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/hello", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    assert(fd_parent == fs.root_dnode);
    assert(crowfs_open(&fs, "/my file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/rng", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/rng", &fd_temp, &fd_parent, 0) == CROWFS_OK);
    assert(fd == fd_temp);
    assert(fd_parent == fs.root_dnode);
    assert(crowfs_open(&fs, "/non existing folder/file", &fd, &fd_parent, 0) == CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/rng/rng", &fd, &fd_parent, 0) == CROWFS_ERR_NOT_FOUND);
    return 0;
}

int test_create_folder() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024);
    uint32_t fd, fd_parent, fd_temp;
    assert(crowfs_open(&fs, "/hello", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(fd_parent == fs.root_dnode);
    fd_temp = fd;
    assert(crowfs_open(&fs, "/hello/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    assert(fd_parent == fd_temp);
    assert(crowfs_open(&fs, "/hello/world/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/hello/world", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(fd_parent == fd_temp);
    fd_temp = fd;
    assert(crowfs_open(&fs, "/hello/world/sup bro", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(fd_parent == fd_temp);
    fd_temp = fd;
    assert(crowfs_open(&fs, "/hello/world/sup bro/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    assert(fd_parent == fd_temp);
    assert(crowfs_open(&fs, "/another dir", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(fd_parent == fs.root_dnode);
    fd_temp = fd;
    assert(crowfs_open(&fs, "/another dir", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(fd_parent == fs.root_dnode);
    assert(fd == fd_temp);
    assert(crowfs_open(&fs, "/another dir/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    assert(fd_parent == fd_temp);
    assert(crowfs_open(&fs, "/not found/directory/welp", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) ==
        CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/hello/file/bro/file", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) ==
        CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/hello/file/nope", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) ==
        CROWFS_ERR_NOT_FOUND);
    return 0;
}

static bool compare_stats(struct CrowFSStat got, struct CrowFSStat expected) {
    return got.type == expected.type && got.size == expected.size && strcmp(got.name, expected.name) == 0;
}

int test_stat() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024);
    const char dummy_buffer[4096 * 16] = {0};
    uint32_t folder1, folder2, file, folder1_file, folder2_file1, folder2_file2, folder2_file3, temp;
    assert(crowfs_open(&fs, "/folder1", &folder1, &temp, CROWFS_O_DIR | CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2", &folder2, &temp, CROWFS_O_DIR | CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/file", &file, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder1/file", &folder1_file, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file1", &folder2_file1, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file2", &folder2_file2, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file3", &folder2_file3, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_write(&fs, folder1_file, dummy_buffer, 1234, 0) == CROWFS_OK);
    assert(crowfs_write(&fs, folder2_file1, dummy_buffer, 10, 0) == CROWFS_OK);
    assert(crowfs_write(&fs, folder2_file2, dummy_buffer, sizeof(dummy_buffer), 0) == CROWFS_OK);
    // Check each file/folder
    struct CrowFSStat got, expected;
    assert(crowfs_stat(&fs, fs.root_dnode, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FOLDER,
        .size = 3,
        .name = "/",
    };
    assert(compare_stats(got, expected));
    assert(crowfs_stat(&fs, folder1, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FOLDER,
        .size = 1,
        .name = "folder1",
    };
    assert(compare_stats(got, expected));
    assert(crowfs_stat(&fs, folder2, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FOLDER,
        .size = 3,
        .name = "folder2",
    };
    assert(compare_stats(got, expected));
    assert(crowfs_stat(&fs, file, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FILE,
        .size = 0,
        .name = "file",
    };
    assert(compare_stats(got, expected));
    assert(crowfs_stat(&fs, folder1_file, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FILE,
        .size = 1234,
        .name = "file",
    };
    assert(compare_stats(got, expected));
    assert(crowfs_stat(&fs, folder2_file1, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FILE,
        .size = 10,
        .name = "file1",
    };
    assert(compare_stats(got, expected));
    assert(crowfs_stat(&fs, folder2_file2, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FILE,
        .size = sizeof(dummy_buffer),
        .name = "file2",
    };
    assert(compare_stats(got, expected));
    assert(crowfs_stat(&fs, folder2_file3, &got) == CROWFS_OK);
    expected = (struct CrowFSStat){
        .type = CROWFS_ENTITY_FILE,
        .size = 0,
        .name = "file3",
    };
    assert(compare_stats(got, expected));
    return 0;
}

int test_read_write_file_small() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024);
    const char to_write_buffer[] = "Hello world!";
    const size_t final_file_size = 2 * (sizeof(to_write_buffer) - 1);
    uint32_t fd, fd_parent;
    assert(crowfs_open(&fs, "/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_write(&fs, fd, to_write_buffer, sizeof(to_write_buffer) - 1, 0) == CROWFS_OK);
    assert(crowfs_write(&fs, fd, to_write_buffer, sizeof(to_write_buffer) - 1, sizeof(to_write_buffer) - 1) ==
        CROWFS_OK);
    assert(crowfs_write(&fs, fd, to_write_buffer, sizeof(to_write_buffer) - 1, 100) == CROWFS_ERR_ARGUMENT);

    // This should have not inflated the file
    struct CrowFSStat stat;
    assert(crowfs_stat(&fs, fd, &stat) == CROWFS_OK);
    assert(stat.size == final_file_size);
    // Read the file
    char read_buffer[1024] = {0}, file_content[1024] = {0};
    strcpy(file_content, to_write_buffer);
    strcat(file_content, to_write_buffer);
    assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), 0) == final_file_size);
    assert(strcmp(read_buffer, file_content) == 0);
    // Read from offset
    memset(read_buffer, 0, sizeof(read_buffer));
    assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), 5) == final_file_size - 5);
    assert(strcmp(read_buffer, file_content + 5) == 0);
    // Out of bound read
    assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), final_file_size) == 0);
    assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), final_file_size + 1) == 0);
    // Check errors
    assert(crowfs_open(&fs, "/folder", &fd, &fd_parent, CROWFS_O_DIR | CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), 0) == CROWFS_ERR_ARGUMENT);
    assert(crowfs_write(&fs, fd, read_buffer, sizeof(read_buffer), 0) == CROWFS_ERR_ARGUMENT);
    return 0;
}

int test_read_write_file_direct() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024 * 16);
    uint32_t fd, fd_parent;
    assert(crowfs_open(&fs, "/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    char block_buffer[256];
    for (int i = 0; i < sizeof(block_buffer); i++)
        block_buffer[i] = (char) i;
    // Fill all direct blocks
    for (int i = 0; i < CROWFS_DIRECT_BLOCKS * (CROWFS_BLOCK_SIZE / sizeof(block_buffer)); i++)
        assert(crowfs_write(&fs, fd, block_buffer, sizeof(block_buffer), i * sizeof(block_buffer)) == CROWFS_OK);
    // Read all back
    for (int i = 0; i < CROWFS_DIRECT_BLOCKS * (CROWFS_BLOCK_SIZE / sizeof(block_buffer)); i++) {
        char read_buffer[256];
        assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), i * sizeof(read_buffer)) == sizeof(block_buffer));
        assert(memcmp(block_buffer, read_buffer, sizeof(read_buffer)) == 0);
    }
    return 0;
}

int test_read_write_file_indirect() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024 * 16);
    uint32_t fd, fd_parent;
    assert(crowfs_open(&fs, "/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    char block_buffer[256];
    for (int i = 0; i < sizeof(block_buffer); i++)
        block_buffer[i] = (char) i;
    // Fill all direct blocks
    for (int i = 0; i < (CROWFS_DIRECT_BLOCKS + CROWFS_INDIRECT_BLOCK_COUNT) * (
                        CROWFS_BLOCK_SIZE / sizeof(block_buffer)); i++)
        assert(crowfs_write(&fs, fd, block_buffer, sizeof(block_buffer), i * sizeof(block_buffer)) == CROWFS_OK);
    // Read all back
    for (int i = 0; i < (CROWFS_DIRECT_BLOCKS + CROWFS_INDIRECT_BLOCK_COUNT) * (
                        CROWFS_BLOCK_SIZE / sizeof(block_buffer)); i++) {
        char read_buffer[256];
        assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), i * sizeof(read_buffer)) == sizeof(block_buffer));
        assert(memcmp(block_buffer, read_buffer, sizeof(read_buffer)) == 0);
    }
    return 0;
}

int test_write_file_full() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024 * 16);
    uint32_t fd, fd_parent;
    assert(crowfs_open(&fs, "/file", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    char block_buffer[256];
    for (int i = 0; i < sizeof(block_buffer); i++)
        block_buffer[i] = (char) i;
    // Fill all direct blocks
    const uint32_t last_block = (CROWFS_DIRECT_BLOCKS + CROWFS_INDIRECT_BLOCK_COUNT) * (
                                    CROWFS_BLOCK_SIZE / sizeof(block_buffer));
    for (int i = 0; i < last_block; i++)
        assert(crowfs_write(&fs, fd, block_buffer, sizeof(block_buffer), i * sizeof(block_buffer)) == CROWFS_OK);
    assert(
        crowfs_write(&fs, fd, block_buffer, sizeof(block_buffer), last_block * sizeof(block_buffer)) ==
        CROWFS_ERR_LIMIT);
    // Read all back
    for (int i = 0; i < (CROWFS_DIRECT_BLOCKS + CROWFS_INDIRECT_BLOCK_COUNT) * (
                        CROWFS_BLOCK_SIZE / sizeof(block_buffer)); i++) {
        char read_buffer[256];
        assert(crowfs_read(&fs, fd, read_buffer, sizeof(read_buffer), i * sizeof(read_buffer)) == sizeof(block_buffer));
        assert(memcmp(block_buffer, read_buffer, sizeof(read_buffer)) == 0);
    }
    assert(
        crowfs_read(&fs, fd, block_buffer, sizeof(block_buffer), last_block * sizeof(block_buffer)) == 0);
    return 0;
}

int test_write_folder_full() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024 * 16);
    uint32_t fd, fd_parent;
    // Write files to fill root
    for (int i = 0; i < CROWFS_MAX_DIR_CONTENTS; i++) {
        char name_buffer[16];
        sprintf(name_buffer, "/file%d", i);
        assert(crowfs_open(&fs, name_buffer, &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    }
    assert(crowfs_open(&fs, "/abkir", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_ERR_LIMIT);
    assert(crowfs_open(&fs, "/abkir", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_ERR_LIMIT);
    // Delete last file and create a folder
    assert(crowfs_delete(&fs, fd, fs.root_dnode) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    // Fill that folder
    for (int i = 0; i < CROWFS_MAX_DIR_CONTENTS; i++) {
        char name_buffer[32];
        sprintf(name_buffer, "/folder/file%d", i);
        assert(crowfs_open(&fs, name_buffer, &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_OK);
    }
    assert(crowfs_open(&fs, "/folder/abkir", &fd, &fd_parent, CROWFS_O_CREATE) == CROWFS_ERR_LIMIT);
    assert(crowfs_open(&fs, "/folder/abkir", &fd, &fd_parent, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_ERR_LIMIT);
    return 0;
}

int test_delete_file() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024);
    uint32_t folder1, folder2, file, folder1_file, folder2_file1, folder2_file2, folder2_file3, temp;
    assert(crowfs_open(&fs, "/folder1", &folder1, &temp, CROWFS_O_DIR | CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2", &folder2, &temp, CROWFS_O_DIR | CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/file", &file, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder1/file", &folder1_file, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file1", &folder2_file1, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file2", &folder2_file2, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file3", &folder2_file3, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    // Delete all files
    assert(crowfs_delete(&fs, file, fs.root_dnode) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder1_file, folder1) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder2_file1, folder2) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder2_file2, folder2) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder2_file3, folder2) == CROWFS_OK);
    // Errors
    assert(crowfs_open(&fs, "/folder1/file", &folder1_file, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder1_file, folder2_file1) == CROWFS_ERR_ARGUMENT);
    assert(crowfs_delete(&fs, folder1_file, folder2) == CROWFS_ERR_ARGUMENT);
    return 0;
}

int test_delete_folder() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024);
    uint32_t folder, folder_dir, folder_dir_help, folder_dir2, folder_dir2_file, temp;
    assert(crowfs_open(&fs, "/folder", &folder, &temp, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder/dir", &folder_dir, &temp, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder/dir/help", &folder_dir_help, &temp, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder/dir2", &folder_dir2, &temp, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder/dir2/file", &folder_dir2_file, &temp, CROWFS_O_CREATE) == CROWFS_OK);
    // Delete each folder
    assert(crowfs_delete(&fs, folder, fs.root_dnode) == CROWFS_ERR_NOT_EMPTY);
    assert(crowfs_delete(&fs, folder_dir, folder) == CROWFS_ERR_NOT_EMPTY);
    assert(crowfs_delete(&fs, folder_dir_help, folder_dir) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder_dir2, folder) == CROWFS_ERR_NOT_EMPTY);
    assert(crowfs_delete(&fs, folder_dir2_file, folder_dir2) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder_dir2, folder) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder, fs.root_dnode) == CROWFS_ERR_NOT_EMPTY);
    assert(crowfs_delete(&fs, folder_dir, folder) == CROWFS_OK);
    assert(crowfs_delete(&fs, folder, fs.root_dnode) == CROWFS_OK);
    struct CrowFSStat stat;
    crowfs_stat(&fs, fs.root_dnode, &stat);
    assert(stat.size == 0);
    // General tests
    assert(crowfs_delete(&fs, fs.root_dnode, 0) == CROWFS_ERR_ARGUMENT);
    return 0;
}

int test_move() {
    struct CrowFS fs;
    mem_fs_init(&fs, 1024 * 1024);
    uint32_t folder1, folder2, folder3, file1, file2, file3, temp1, temp2, new_file1;
    assert(crowfs_open(&fs, "/folder1", &folder1, &temp1, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2", &folder2, &temp1, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder3", &folder3, &temp1, CROWFS_O_CREATE | CROWFS_O_DIR) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder1/file1", &file1, &temp1, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file2", &file2, &temp1, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder3/file3", &file3, &temp1, CROWFS_O_CREATE) == CROWFS_OK);

    // Move one file
    assert(crowfs_move(&fs, file1, folder1, folder2) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder1/file1", &temp1, &temp2, 0) == CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/folder2/file1", &temp1, &temp2, 0) == CROWFS_OK);
    assert(temp1 == file1);
    assert(temp2 == folder2);
    assert(crowfs_move(&fs, file1, folder2, folder1) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file1", &temp1, &temp2, 0) == CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/folder1/file1", &temp1, &temp2, 0) == CROWFS_OK);
    assert(temp1 == file1);
    assert(temp2 == folder1);

    // Move folder
    assert(crowfs_move(&fs, folder2, fs.root_dnode, folder3) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder2/file2", &temp1, &temp2, 0) == CROWFS_ERR_NOT_FOUND);
    assert(crowfs_open(&fs, "/folder3/folder2/file2", &temp1, &temp2, 0) == CROWFS_OK);
    assert(temp1 == file2);
    assert(temp2 == folder2);
    assert(crowfs_open(&fs, "/folder3/folder2", &temp1, &temp2, 0) == CROWFS_OK);
    assert(temp1 == folder2);
    assert(temp2 == folder3);

    // Replace file
    assert(crowfs_open(&fs, "/file1", &temp1, &temp2, CROWFS_O_CREATE) == CROWFS_OK);
    assert(crowfs_move(&fs, temp1, fs.root_dnode, folder1) == CROWFS_OK);
    assert(crowfs_open(&fs, "/folder1/file1", &new_file1, &temp2, 0) == CROWFS_OK);
    assert(temp2 == folder1);
    assert(new_file1 != file1);
    return 0;
}

int test_read_dir() {
    assert(false);
}

int test_disk_full() {
    assert(false);
}
