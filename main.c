#include <assert.h>
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

char file_type_to_char(uint8_t type) {
    switch (type) {
        case CROWFS_ENTITY_FILE:
            return 'F';
        case CROWFS_ENTITY_FOLDER:
            return 'D';
        default:
            assert(0);
    }
}

int main(int argc, char *argv[]) {
    FILE *host_file = NULL;
    // Check arguments
    if (argc < 3) {
        puts("Please pass the filename and command as arguments");
        exit(1);
    }
    // Open the block file
    block_file = fopen(argv[1], "r+b");
    if (block_file == NULL) {
        perror("cannot open file");
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
    // Check what is the command
    int exit_code = 0;
    if (strcmp(argv[2], "new") == 0) {
        // Create a new filesystem
        int result = crowfs_new(&fs);
        if (result != CROWFS_OK) {
            printf("cannot create the filesystem: error %d\n", result);
            exit_code = 1;
            goto end;
        }
        printf("File system created with %u blocks\n", std_total_blocks());
    } else if (strcmp(argv[2], "copyin") == 0) {
        // Open the filesystem
        int result = crowfs_init(&fs);
        if (result != CROWFS_OK) {
            printf("cannot open the filesystem: error %d\n", result);
            exit_code = 1;
            goto end;
        }
        // Copy a file from host to the file system
        if (argc < 5) {
            puts("Please pass the source file and destination filename to the program");
            exit_code = 1;
            goto end;
        }
        // Open the host file
        host_file = fopen(argv[3], "rb");
        if (host_file == NULL) {
            perror("cannot open host file");
            exit_code = 1;
            goto end;
        }
        // Open the file in the file system
        uint32_t fs_file, temp;
        result = crowfs_open(&fs, argv[4], &fs_file, &temp, CROWFS_O_CREATE);
        if (result != CROWFS_OK) {
            printf("cannot create the file: error %d\n", result);
            exit_code = 1;
            goto end;
        }
        // Read all the file in memory because why not
        size_t offset = 0;
        while (1) {
            char buffer[512];
            // Read a chunk
            size_t n = fread(buffer, sizeof(char), sizeof(buffer), host_file);
            if (n == 0 && feof(host_file)) {
                // did we reach eof?
                break;
            }
            // Write to file system
            result = crowfs_write(&fs, fs_file, buffer, n, offset);
            if (result != CROWFS_OK) {
                printf("cannot write the file: error %d\n", result);
                exit_code = 1;
                goto end;
            }
            // Advance pointer
            offset += n;
        }
        // Done
        printf("Copied %zu bytes to file system\n", offset);
    } else if (strcmp(argv[2], "copyout") == 0) {
        // Open the filesystem
        int result = crowfs_init(&fs);
        if (result != CROWFS_OK) {
            printf("cannot open the filesystem: error %d\n", result);
            exit_code = 1;
            goto end;
        }
        // Copy a file from file system to the host
        if (argc < 5) {
            puts("Please pass the source file and destination filename to the program");
            exit_code = 1;
            goto end;
        }
        // Open the host file
        host_file = fopen(argv[4], "wb");
        if (host_file == NULL) {
            perror("cannot open host file");
            exit_code = 1;
            goto end;
        }
        // Open the file in the file system
        uint32_t fs_file, temp;
        result = crowfs_open(&fs, argv[3], &fs_file, &temp, 0);
        if (result != CROWFS_OK) {
            printf("cannot open the file: error %d\n", result);
            exit_code = 1;
            goto end;
        }
        // Read all the file in memory because why not
        size_t offset = 0;
        while (1) {
            char buffer[512];
            // Read a chunk
            result = crowfs_read(&fs, fs_file, buffer, sizeof(buffer), offset);
            if (result < 0) {
                printf("cannot read the file: error %d\n", result);
                fclose(host_file);
                exit_code = 1;
                goto end;
            }
            if (result == 0)
                break; // EOF
            // Write to file system
            size_t fwrite_result = fwrite(buffer, sizeof(char), result, host_file);
            if (fwrite_result != result) {
                puts("short write");
                exit_code = 1;
                goto end;
            }
            // Advance pointer
            offset += result;
        }
        // Done
        printf("Copied %zu bytes from file system\n", offset);
    } else if (strcmp(argv[2], "ls") == 0) {
        // Open the filesystem
        int result = crowfs_init(&fs);
        if (result != CROWFS_OK) {
            printf("cannot open the filesystem: error %d\n", result);
            exit_code = 1;
            goto end;
        }
        // Copy a file from file system to the host
        if (argc < 4) {
            puts("Please pass the folder path to list to the program");
            exit_code = 1;
            goto end;
        }
        // Open the directory in the file system
        uint32_t directory, temp;
        result = crowfs_open(&fs, argv[3], &directory, &temp, 0);
        if (result != CROWFS_OK) {
            printf("cannot open the directory: error %d\n", result);
            exit_code = 1;
            goto end;
        }
        // Read each file
        printf("Listing all files and directories in %s\n", argv[3]);
        size_t offset = 0;
        while (1) {
            struct CrowFSStat stat;
            result = crowfs_read_dir(&fs, directory, &stat, offset);
            if (result == CROWFS_ERR_LIMIT) // end
                break;
            if (result != CROWFS_OK) {
                printf("cannot read the directory: error %d\n", result);
                exit_code = 1;
                goto end;
            }
            // Print the data
            printf("%c\t%s\t%u\t%lld\n",
                   file_type_to_char(stat.type), stat.name, stat.size, stat.creation_date);
            // Read next dir
            offset++;
        }
    } else {
        puts("Invalid command");
        exit_code = 1;
        goto end;
    }
    // Done
end:
    if (host_file != NULL)
        fclose(host_file);
    fclose(block_file);
    return exit_code;
}
