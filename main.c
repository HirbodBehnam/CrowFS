#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "crowfs.h"

FILE *block_file = NULL;

union CrowFSBlock *std_allocate_mem_block(void) {
    return calloc(1, sizeof(union CrowFSBlock));
}

void std_free_mem_block(union CrowFSBlock *block) {
    free(block);
}

int std_write_block(uint32_t block_index, const union CrowFSBlock *block) {
    if (fseek(block_file, CROWFS_BLOCK_SIZE * block_index, SEEK_SET) == -1)
        return 1;
    if (fwrite(block, sizeof(*block), 1, block_file) != 1)
        return 1;
    return 0;
}

int std_read_block(uint32_t block_index, union CrowFSBlock *block) {
    if (fseek(block_file, CROWFS_BLOCK_SIZE * block_index, SEEK_SET) == -1)
        return 1;
    if (fread(block, sizeof(*block), 1, block_file) != 1)
        return 1;
    return 0;
}

uint32_t std_total_blocks(void) {
    fseek(block_file, 0, SEEK_END);
    return ftell(block_file) / CROWFS_BLOCK_SIZE;
}

int64_t std_current_date(void) {
    return time(NULL);
}

int main(void) {
    block_file = fopen("fs.bin", "r+b");
    if (block_file == NULL) {
        puts(strerror(errno));
        exit(1);
    }
    struct CrowFS fs = {
            .allocate_mem_block = std_allocate_mem_block,
            .free_mem_block = std_free_mem_block,
            .write_block = std_write_block,
            .read_block = std_read_block,
            .total_blocks = std_total_blocks,
            .current_date = std_current_date,
    };
    crowfs_new(&fs);
    uint32_t kir, kir_parent;
    crowfs_open(&fs, "/kir.txt", &kir, &kir_parent, CROWFS_O_CREATE);
    char buf[512] = "please kill me";
    crowfs_write(&fs, kir, buf, 10, 0);
    memset(buf, 0, sizeof(buf));
    crowfs_read(&fs, kir, buf, sizeof(buf), 0);
    fclose(block_file);
}
