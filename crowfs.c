#include "crowfs.h"
#include <stdbool.h>

#ifdef __CROWOS__
#include "lib.h"
#else

#include <string.h>

#endif
/**
 * The dnode which superblock resides in
 */
#define SUPERBLOCK_DNODE 1
/**
 * Try to do an IO operation
 */
#define TRY_IO(func) do { if (func) {result = CROWFS_ERR_IO; goto end;} } while (0);

#define MIN(x, y) ((x < y) ? (x) : (y))

/**
 * Gets the length of the next part in the path. For example, if the given string
 * is "hello/world/path" the result would be 5. An empty string yields 0.
 * @param str The path
 * @return Length of next part in path
 */
size_t path_next_part_len(const char *str) {
    size_t len = 0;
    while (str[len] != '\0' && str[len] != '/')
        len++;
    return len;
}

/**
 * Detects if this is the last part of a path or not. For example,
 * "hello" is the last part of the path same is "hello/". However,
 * "hello/world" is not the last part of the path.
 * @param str The path to check
 * @return True if last part, otherwise false
 */
bool path_last_part(const char *str) {
    size_t current_len = path_next_part_len(str);
    if (str[current_len] == '\0') // null terminator is always the last part
        return true;
    // Here it means that the last char is /
    return str[current_len + 1] == '\0';
}

/**
 * Sets the nth bit in a bitmap to one
 * @param bitmap The bitmap
 * @param index The index in range [0, CROWFS_BLOCK_SIZE*8]
 */
void bitmap_set(struct CrowFSBitmapBlock *bitmap, uint32_t index) {
    size_t char_index = index / 8;
    uint32_t bit_index = index % 8;
    if (char_index >= CROWFS_BLOCK_SIZE) // out of bounds
        return;
    bitmap->bitmap[char_index] |= 1 << bit_index;
}

/**
 * Clears the nth bit in a bitmap to one
 * @param bitmap The bitmap
 * @param index The index in range [0, CROWFS_BLOCK_SIZE*8]
 */
void bitmap_clear(struct CrowFSBitmapBlock *bitmap, uint32_t index) {
    size_t char_index = index / 8;
    uint32_t bit_index = index % 8;
    if (char_index >= CROWFS_BLOCK_SIZE) // out of bounds
        return;
    bitmap->bitmap[char_index] &= ~(1 << bit_index);
}

/**
 * Allocates a free dnode and returns it
 * @param fs The filesystem
 * @return The dnode number or zero if no free dnode is available
 */
uint32_t block_alloc(struct CrowFS *fs) {
    uint32_t allocated_dnode = 0;
    union CrowFSBlock *block = fs->allocate_mem_block();
    // Look for free blocks
    for (uint32_t free_block = 0; free_block < fs->free_bitmap_blocks; free_block++) {
        // Read the bitmap
        if (fs->read_block(free_block + 1 + 1, block)) // well fuck?
            goto end;
        // Look for free block...
        for (uint32_t i = 0; i < CROWFS_BLOCK_SIZE; i++)
            if (block->bitmap.bitmap[i] != 0) {
                allocated_dnode =
                        free_block * CROWOS_BITSET_COVERED_BLOCKS + i * 8 + __builtin_ctz(block->bitmap.bitmap[i]);
                break;
            }
        if (allocated_dnode != 0)
            break;
    }
    // Anything found?
    if (allocated_dnode == 0)
        goto end;
    // Mark this dnode as occupied
    if (fs->read_block(allocated_dnode / 8 / CROWFS_BLOCK_SIZE + 1 + 1, block)) {
        allocated_dnode = 0;
        goto end;
    }
    bitmap_clear(&block->bitmap, allocated_dnode % CROWOS_BITSET_COVERED_BLOCKS);
    if (fs->write_block(allocated_dnode / 8 / CROWFS_BLOCK_SIZE + 1 + 1, block)) {
        allocated_dnode = 0;
        goto end;
    }

    end:
    fs->free_mem_block(block);
    return allocated_dnode;
}

/**
 * Gets the block index which a pointer is pointing to. If the block does not exists
 * it will try to allocate a new block on disk and return the new block number.
 * @param fs The filesystem
 * @param from The block index to check
 * @return The block index if successful or 0 if disk is full
 */
uint32_t get_or_allocate_block(struct CrowFS *fs, uint32_t *from) {
    uint32_t content_block = *from;
    if (content_block == 0) {
        content_block = block_alloc(fs);
        if (content_block == 0)
            return 0;
        *from = content_block;
    }
    return content_block;
}

/**
 * Frees an allocated block
 * @param dnode The dnode or block number
 */
void block_free(struct CrowFS *fs, uint32_t dnode) {
    union CrowFSBlock *block = fs->allocate_mem_block();
    if (fs->read_block(dnode / 8 / CROWFS_BLOCK_SIZE + 1 + 1, block))
        goto end;

    bitmap_set(&block->bitmap, dnode % CROWOS_BITSET_COVERED_BLOCKS);
    fs->write_block(dnode / 8 / CROWFS_BLOCK_SIZE + 1 + 1, block);

    end:
    fs->free_mem_block(block);
}

/**
 * Counts the number of files or folders inside a folder
 * @param dir The folder to count
 * @return The number of files or folder in the given directory
 */
uint32_t folder_content_count(const struct CrowFSDirectoryBlock *dir) {
    uint32_t count;
    for (count = 0; count < CROWFS_MAX_DIR_CONTENTS; count++)
        if (dir->content_dnodes[count] == 0)
            return count;
    return count; // dir is full!
}

/**
 * Removes a file dnode from folder content. This function does not free the dnode
 * or do anything with the file. It just removes it from the content_dnodes list.
 * @param dir The directory to perform the action on
 * @param target_dnode The target dnode to remove from the directory
 * @return 0 if target is removed, 1 if the target is not found
 */
int folder_remove_content(struct CrowFSDirectoryBlock *dir, uint32_t target_dnode) {
    int dnode_index = -1;
    for (int i = 0; i < CROWFS_MAX_DIR_CONTENTS; i++)
        if (dir->content_dnodes[i] == target_dnode) {
            dnode_index = i;
            break;
        }
    if (dnode_index == -1)
        return 1; // what?
    int last_index = (int) (folder_content_count(dir) - 1);
    if (last_index == 1) { // only one content so remove it
        dir->content_dnodes[0] = 0;
    } else if (last_index == dnode_index) { // dnode is last. Replace it
        dir->content_dnodes[last_index] = 0;
    } else { // perform a swap
        dir->content_dnodes[dnode_index] = dir->content_dnodes[last_index];
        dir->content_dnodes[last_index] = 0;
    }
    return 0;
}

int crowfs_new(struct CrowFS *fs) {
    int result = CROWFS_OK;
    // Check if all functions exists
    if (fs->allocate_mem_block == NULL || fs->free_mem_block == NULL || fs->write_block == NULL ||
        fs->read_block == NULL || fs->current_date == NULL || fs->total_blocks == NULL)
        return CROWFS_ERR_ARGUMENT;
    // Overwrite the superblock
    union CrowFSBlock *block = fs->allocate_mem_block();
    block->superblock = (struct CrowFSSuperblock) {
            .magic = {0}, // fill later
            .version = CROWFS_VERSION,
            .blocks = fs->total_blocks(),
    };
    memcpy(block->superblock.magic, CROWFS_MAGIC, sizeof(block->superblock.magic));
    // Check if the blocks on the disk is enough
    // We need 4 blocks at least:
    // 1. Bootloader
    // 2. Superblock
    // 3. Free bitmap
    // 4. Root directory
    if (block->superblock.blocks <= 4) {
        result = CROWFS_ERR_TOO_SMALL;
        goto end;
    }
    fs->superblock = block->superblock;
    // Write the superblock
    TRY_IO(fs->write_block(SUPERBLOCK_DNODE, block))
    // Calculate the free bitmap size
    fs->free_bitmap_blocks =
            (block->superblock.blocks + CROWOS_BITSET_COVERED_BLOCKS - 1) / CROWOS_BITSET_COVERED_BLOCKS;
    if (block->superblock.blocks <= 3 + fs->free_bitmap_blocks) {
        result = CROWFS_ERR_TOO_SMALL;
        goto end;
    }
    // bootloader + superblock
    fs->root_dnode = 1 + 1 + fs->free_bitmap_blocks;
    // Set the free bitmaps to all one in non-last and first pages
    memset(block->bitmap.bitmap, 0xFF, sizeof(block->bitmap.bitmap));
    // Write to disk
    for (uint32_t i = 1; i < fs->free_bitmap_blocks - 1; i++)
        TRY_IO(fs->write_block(1 + 1 + i, block))
    // Set the first free block
    // Note: Because at last we are going to use 32 free blocks
    // this works fine
    for (uint32_t i = 0; i < fs->free_bitmap_blocks + 3; i++)
        bitmap_clear(&block->bitmap, i);
    TRY_IO(fs->write_block(2, block))
    // If the last block is different from the first block, zero the block
    if (fs->free_bitmap_blocks != 1)
        memset(block->bitmap.bitmap, 0xFF, sizeof(block->bitmap.bitmap));

    for (uint32_t last_block_id = CROWOS_BITSET_COVERED_BLOCKS * fs->free_bitmap_blocks - 1;
         last_block_id >= fs->superblock.blocks;
         last_block_id--)
        bitmap_clear(&block->bitmap, last_block_id);
    TRY_IO(fs->write_block(2 + fs->free_bitmap_blocks - 1, block))
    // Create the root directory
    block->folder = (struct CrowFSDirectoryBlock) {
            .header = (struct CrowFSDnodeHeader) {
                    .type = CROWFS_ENTITY_FOLDER,
                    .name = "/",
                    .creation_date = fs->current_date(),
            },
            .content_dnodes = {0},
    };
    TRY_IO(fs->write_block(fs->root_dnode, block))

    end:
    fs->free_mem_block(block);
    return result;
}

int crowfs_init(struct CrowFS *fs) {
    int result = CROWFS_OK;
    // Check if all functions exists
    if (fs->allocate_mem_block == NULL || fs->free_mem_block == NULL || fs->write_block == NULL ||
        fs->read_block == NULL || fs->current_date == NULL)
        return CROWFS_ERR_ARGUMENT;
    // Check for superblock
    union CrowFSBlock *block = fs->allocate_mem_block();
    TRY_IO(fs->read_block(SUPERBLOCK_DNODE, block))
    if (memcmp(block->superblock.magic, CROWFS_MAGIC, sizeof(block->superblock.magic)) != 0) {
        result = CROWFS_ERR_INIT_INVALID_FS;
        goto end;
    }
    fs->superblock = block->superblock;
    // Calculate the root dnode index
    fs->free_bitmap_blocks =
            (block->superblock.blocks + CROWOS_BITSET_COVERED_BLOCKS - 1) / CROWOS_BITSET_COVERED_BLOCKS;
    fs->root_dnode = 1 + 1 + fs->free_bitmap_blocks;

    end:
    fs->free_mem_block(block);
    return result;
}

int crowfs_open(struct CrowFS *fs, const char *path, uint32_t *dnode, uint32_t *parent_dnode, uint32_t flags) {
    if (path[0] != '/') // paths must be absolute
        return CROWFS_ERR_ARGUMENT;
    path++; // skip the /
    *parent_dnode = 0; // for '/' path
    int result = CROWFS_OK;
    union CrowFSBlock *current_dnode = fs->allocate_mem_block(),
            *temp_dnode = fs->allocate_mem_block();
    uint32_t current_dnode_index = fs->root_dnode;
    TRY_IO(fs->read_block(current_dnode_index, current_dnode))
    // Traverse the file system
    while (true) {
        size_t next_path_size = path_next_part_len(path);
        // Search for this file in the directory
        uint32_t dnode_search_result = 0;
        for (int i = 0; i < CROWFS_MAX_DIR_CONTENTS; i++) {
            if (current_dnode->folder.content_dnodes[i] == 0) // File/Folder not found
                break;
            // Compare filenames
            TRY_IO(fs->read_block(current_dnode->folder.content_dnodes[i], temp_dnode))
            if (memcmp(temp_dnode->header.name, path, next_path_size) == 0 &&
                temp_dnode->header.name[next_path_size] == '\0') {
                // Matched!
                dnode_search_result = current_dnode->folder.content_dnodes[i];
                break;
            }
            // Continue searching...
        }
        // There are some ways this can go...
        if ((flags & CROWFS_O_CREATE) && dnode_search_result == 0) {
            // File not found
            if (path_last_part(path)) {
                // Create the file/folder
                // Does the parent directory has empty slots?
                size_t folder_size = folder_content_count(&current_dnode->folder);
                if (folder_size == CROWFS_MAX_DIR_CONTENTS) {
                    result = CROWFS_ERR_LIMIT;
                    goto end;
                }
                // Allocate dnode
                *dnode = block_alloc(fs);
                if (*dnode == 0) {
                    result = CROWFS_ERR_FULL;
                    goto end;
                }
                current_dnode->folder.content_dnodes[folder_size] = *dnode;
                *parent_dnode = current_dnode_index;
                // Create the dnode
                temp_dnode->header = (struct CrowFSDnodeHeader) {
                        .creation_date = fs->current_date(),
                };
                memcpy(temp_dnode->header.name, path, next_path_size);
                temp_dnode->header.name[next_path_size] = '\0';
                if (flags & CROWFS_O_DIR) {
                    temp_dnode->header.type = CROWFS_ENTITY_FOLDER;
                    memset(temp_dnode->folder.content_dnodes, 0, sizeof(temp_dnode->folder.content_dnodes));
                } else {
                    temp_dnode->header.type = CROWFS_ENTITY_FILE;
                    temp_dnode->file.indirect_block = 0;
                    memset(temp_dnode->file.direct_blocks, 0, sizeof(temp_dnode->file.direct_blocks));
                }
                // Write to disk
                TRY_IO(fs->write_block(*parent_dnode, current_dnode));
                TRY_IO(fs->write_block(*dnode, temp_dnode));
                break;
            } else { // well shit.
                result = CROWFS_ERR_NOT_FOUND;
                goto end;
            }
        } else if (!(flags & CROWFS_O_CREATE) && dnode_search_result == 0) {
            // File not found
            result = CROWFS_ERR_NOT_FOUND;
            goto end;
        }
        // We have found something
        if (path_last_part(path)) { // Is it the thing?
            *dnode = dnode_search_result;
            *parent_dnode = current_dnode_index;
            break;
        } else {
            // Traverse more into the directories...
            TRY_IO(fs->read_block(dnode_search_result, current_dnode));
            if (current_dnode->header.type != CROWFS_ENTITY_FOLDER) {
                // We found a file instead of a folder...
                result = CROWFS_ERR_NOT_FOUND;
                goto end;
            }
            current_dnode_index = dnode_search_result;
            path += next_path_size + 1; // skip the current directory
        }
    }

    end:
    fs->free_mem_block(current_dnode);
    fs->free_mem_block(temp_dnode);
    return result;
}

int crowfs_write(struct CrowFS *fs, uint32_t dnode, const char *data, size_t size, size_t offset) {
    int result = CROWFS_OK;
    // Read the dnode block at first
    union CrowFSBlock *dnode_block = fs->allocate_mem_block(),
            *data_block = fs->allocate_mem_block(),
            *indirect_block = fs->allocate_mem_block();
    TRY_IO(fs->read_block(dnode, dnode_block))
    if (dnode_block->header.type != CROWFS_ENTITY_FILE) { // this is a file right?
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    // Will we pass the size limit of files?
    if (size + offset > CROWOS_MAX_FILESIZE) {
        result = CROWFS_ERR_LIMIT;
        goto end;
    }
    // File growing. Allocate empty dnodes
    if (offset > dnode_block->file.size) {
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    // Read the indirect block list as well
    if (dnode_block->file.indirect_block != 0)
        TRY_IO(fs->read_block(dnode_block->file.indirect_block, indirect_block))
    // Copy to disk
    size_t to_write_bytes = size;
    while (to_write_bytes > 0) {
        size_t content_block_index = offset / CROWFS_BLOCK_SIZE;
        size_t raw_data_index = offset % CROWFS_BLOCK_SIZE;
        uint32_t content_block;
        if (content_block_index >= CROWFS_DIRECT_BLOCKS) {
            // Is indirect block available?
            if (dnode_block->file.indirect_block == 0) {
                dnode_block->file.indirect_block = block_alloc(fs);
                if (dnode_block->file.indirect_block == 0) {
                    result = CROWFS_ERR_FULL;
                    goto end;
                }
            }
            // Get from indirect block
            content_block = get_or_allocate_block(fs, &indirect_block->indirect_block[content_block_index -
                                                                                      CROWFS_DIRECT_BLOCKS]);
            if (content_block == 0) {
                result = CROWFS_ERR_FULL;
                goto end;
            }
        } else {
            content_block = get_or_allocate_block(fs, &dnode_block->file.direct_blocks[content_block_index]);
            if (content_block == 0) {
                result = CROWFS_ERR_FULL;
                goto end;
            }
        }
        // We might need to partially write to a block. For this, we must issue a
        // read and then issue a write to disk.
        if (offset > 0)
            TRY_IO(fs->read_block(content_block, data_block))
        size_t to_copy = MIN(CROWFS_BLOCK_SIZE - raw_data_index, to_write_bytes);
        memcpy(data_block->raw_data + raw_data_index, data, to_copy);
        TRY_IO(fs->write_block(content_block, data_block))
        data += to_copy;
        to_write_bytes -= to_copy;
        offset += to_copy;
    }
    // Update dnode and indirect blocks
    if (dnode_block->file.indirect_block != 0)
        TRY_IO(fs->write_block(dnode_block->file.indirect_block, indirect_block))
    dnode_block->file.size += size;
    TRY_IO(fs->write_block(dnode, dnode_block))

    end:
    fs->free_mem_block(dnode_block);
    fs->free_mem_block(data_block);
    fs->free_mem_block(indirect_block);
    return result;
}

int crowfs_read(struct CrowFS *fs, uint32_t dnode, char *buf, size_t size, size_t offset) {
    int result = CROWFS_OK, read_bytes = 0;
    // Read the dnode
    union CrowFSBlock *dnode_block = fs->allocate_mem_block(),
            *data_block = fs->allocate_mem_block(),
            *indirect_block = fs->allocate_mem_block();
    TRY_IO(fs->read_block(dnode, dnode_block))
    if (dnode_block->header.type != CROWFS_ENTITY_FILE) { // this is a file right?
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    if (dnode_block->file.indirect_block != 0)
        TRY_IO(fs->read_block(dnode_block->file.indirect_block, indirect_block))
    if (offset >= dnode_block->file.size) // nothing to read...
        goto end;
    int to_read_bytes = MIN(dnode_block->file.size - offset, size);
    // Read the corresponding data blocks
    while (to_read_bytes > 0) {
        size_t content_block_index = offset / CROWFS_BLOCK_SIZE;
        size_t raw_data_index = offset % CROWFS_BLOCK_SIZE;
        uint32_t content_block;
        if (content_block_index >= CROWFS_DIRECT_BLOCKS)
            content_block = indirect_block->indirect_block[content_block_index - CROWFS_DIRECT_BLOCKS];
        else
            content_block = dnode_block->file.direct_blocks[content_block_index];
        TRY_IO(fs->read_block(content_block, data_block))
        int to_copy = MIN(CROWFS_BLOCK_SIZE - raw_data_index, to_read_bytes);
        memcpy(buf, data_block->raw_data + raw_data_index, to_copy);
        buf += to_copy;
        to_read_bytes -= to_copy;
        offset += to_copy;
        read_bytes += to_copy;
    }

    end:
    fs->free_mem_block(dnode_block);
    fs->free_mem_block(data_block);
    fs->free_mem_block(indirect_block);
    if (result == CROWFS_OK)
        return read_bytes;
    else
        return result;
}

int crowfs_read_dir(struct CrowFS *fs, uint32_t dnode, struct CrowFSStat *stat, size_t offset) {
    int result = CROWFS_OK;
    // Read the dnode block at first
    union CrowFSBlock *dnode_block = fs->allocate_mem_block();
    TRY_IO(fs->read_block(dnode, dnode_block))
    if (dnode_block->header.type != CROWFS_ENTITY_FOLDER) { // this is a folder right?
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    // Is the offset out of the bonds?
    if (offset >= CROWFS_MAX_DIR_CONTENTS) {
        result = CROWFS_ERR_LIMIT;
        goto end;
    }
    uint32_t requested_dnode = dnode_block->folder.content_dnodes[offset];
    if (requested_dnode == 0) {
        result = CROWFS_ERR_LIMIT;
        goto end;
    }
    // Get the stats of the dnode
    fs->free_mem_block(dnode_block);
    return crowfs_stat(fs, requested_dnode, stat);

    end:
    fs->free_mem_block(dnode_block);
    return result;
}

int crowfs_delete(struct CrowFS *fs, uint32_t dnode, uint32_t parent_dnode) {
    int result = CROWFS_OK;
    // Read the dnode block at first
    union CrowFSBlock *dnode_block = fs->allocate_mem_block(),
            *indirect_block = fs->allocate_mem_block();
    TRY_IO(fs->read_block(dnode, dnode_block))
    if (dnode_block->header.type != CROWFS_ENTITY_FOLDER) { // this is a folder right?
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    // What is this entity?
    switch (dnode_block->header.type) {
        case CROWFS_ENTITY_FILE:
            // Delete each indirect block of file
            if (dnode_block->file.indirect_block != 0) {
                TRY_IO(fs->read_block(dnode_block->file.indirect_block, indirect_block))
                for (int i = 0; i < CROWFS_INDIRECT_BLOCK_COUNT && indirect_block->indirect_block[i] != 0; i++)
                    block_free(fs, indirect_block->indirect_block[i]);
            }
            // Delete direct blocks
            for (int i = 0; i < CROWFS_DIRECT_BLOCKS && dnode_block->file.direct_blocks[i] != 0; i++)
                block_free(fs, dnode_block->file.direct_blocks[i]);
            break;
        case CROWFS_ENTITY_FOLDER:
            // Is the folder emtpy?
            for (int i = 0; i < CROWFS_MAX_DIR_CONTENTS; i++)
                if (dnode_block->folder.content_dnodes[i] != 0) {
                    result = CROWFS_ERR_NOT_EMPTY;
                    goto end;
                }
            break;
        default:
            result = CROWFS_ERR_ARGUMENT;
            goto end;
    }
    // Delete in parent as well
    TRY_IO(fs->read_block(parent_dnode, dnode_block))
    if (dnode_block->header.type != CROWFS_ENTITY_FOLDER) {
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    if (folder_remove_content(&dnode_block->folder, dnode) != 0) {
        result = CROWFS_ERR_ARGUMENT; // child does not exist in parent
        goto end;
    }
    TRY_IO(fs->write_block(parent_dnode, dnode_block))

    // Delete this dnode/block as well
    block_free(fs, dnode);

    end:
    fs->free_mem_block(dnode_block);
    fs->free_mem_block(indirect_block);
    return result;
}

int crowfs_stat(struct CrowFS *fs, uint32_t dnode, struct CrowFSStat *stat) {
    int result = CROWFS_OK;
    union CrowFSBlock *dnode_block = fs->allocate_mem_block();
    TRY_IO(fs->read_block(dnode, dnode_block))
    // Read the header
    stat->type = dnode_block->header.type;
    memcpy(stat->name, dnode_block->header.name, sizeof(stat->name));
    stat->creation_date = dnode_block->file.header.creation_date;
    // Fill the size based on type
    switch (dnode_block->header.type) {
        case CROWFS_ENTITY_FILE:
            stat->size = dnode_block->file.size;
            break;
        case CROWFS_ENTITY_FOLDER:
            stat->size = folder_content_count(&dnode_block->folder);
            break;
        default:
            result = CROWFS_ERR_ARGUMENT;
            goto end;
    }

    end:
    fs->free_mem_block(dnode_block);
    return result;
}

int crowfs_move(struct CrowFS *fs, uint32_t dnode, uint32_t old_parent, uint32_t new_parent) {
    int result = CROWFS_OK;
    union CrowFSBlock *dnode_block = fs->allocate_mem_block();
    // Add to new parent
    TRY_IO(fs->read_block(new_parent, dnode_block))
    if (dnode_block->header.type != CROWFS_ENTITY_FOLDER) {
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    uint32_t new_dnode_index = folder_content_count(&dnode_block->folder);
    if (new_dnode_index == CROWFS_MAX_DIR_CONTENTS) {
        result = CROWFS_ERR_LIMIT; // we have reached the maximum nodes for this directory
        goto end;
    }
    dnode_block->folder.content_dnodes[new_dnode_index] = dnode;
    TRY_IO(fs->write_block(new_parent, dnode_block))

    // Remove from old parent
    TRY_IO(fs->read_block(old_parent, dnode_block))
    if (dnode_block->header.type != CROWFS_ENTITY_FOLDER) {
        result = CROWFS_ERR_ARGUMENT;
        goto end;
    }
    if (folder_remove_content(&dnode_block->folder, dnode) != 0) {
        result = CROWFS_ERR_ARGUMENT; // child does not exist in parent
        goto end;
    }
    TRY_IO(fs->write_block(old_parent, dnode_block))

    end:
    fs->free_mem_block(dnode_block);
    return result;
}