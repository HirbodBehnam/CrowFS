#pragma once

#include <stddef.h>
#include <stdint.h>

#define CROWFS_MAGIC "CrFS"
#define CROWFS_VERSION 1
/**
 * CrowFS expects each block of the disk to be 4096 bytes
 */
#define CROWFS_BLOCK_SIZE 4096
/**
 * In each indirect block, we have this much entries.
 */
#define CROWFS_INDIRECT_BLOCK_COUNT (CROWFS_BLOCK_SIZE / sizeof(uint32_t))
/**
 * Maximum number of characters which each folder/file can have excluding the trailing zero.
 */
#define CROWFS_MAX_FILENAME 254
/**
 * Number of direct blocks in a file dnode.
 */
#define CROWFS_DIRECT_BLOCKS 956
/**
 * Maximum number of entities (directories or files) in a directory
 */
#define CROWFS_MAX_DIR_CONTENTS 957
/**
 * Maximum file size in CrowFS
 */
#define CROWFS_MAX_FILESIZE (CROWFS_BLOCK_SIZE*(1024+CROWFS_DIRECT_BLOCKS))
/**
 * Number of blocks that a single bitset can contain
 */
#define CROWFS_BITSET_COVERED_BLOCKS (CROWFS_BLOCK_SIZE * 8)

/**
 * Structure of the super block for CrowFS
 */
struct __attribute__((__packed__)) CrowFSSuperblock {
    // Magic number for CrowFS. Must be equal to "CrFS"
    char magic[4];
    /**
     * Version of filesystem.
     */
    uint32_t version;
    /**
     * Number of blocks which this disk has.
     */
    uint32_t blocks;
};

#define CROWFS_ENTITY_FILE 1
#define CROWFS_ENTITY_FOLDER 2

struct __attribute__((__packed__)) CrowFSDnodeHeader {
    // One of CROWFS_ENTITY_*
    uint8_t type;
    // Filename with a null terminator
    char name[CROWFS_MAX_FILENAME + 1];
    //  When was this file created? In Unix timestamp.
    int64_t creation_date;
};

/**
 * Each file dnode is like this on disk.
 *
 * Note to myself: I have removed packed attribute to remove
 * the GCC warning about unaligned pointers. For now, everything is
 * packed and aligned and if in any case a padding is issued, the
 * static assertion will forbid the code from compiling and thus
 * preventing runtime bugs.
 * More info:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96293
 */
struct CrowFSFileBlock {
    // The header of this file
    struct CrowFSDnodeHeader header;
    // Size of the file
    uint32_t size;
    // Might point to a struct CrowFSIndirectBlock for the continuation of the data.
    // Is null if no indirect block exists.
    uint32_t indirect_block;
    // Direct blocks for this file
    uint32_t direct_blocks[CROWFS_DIRECT_BLOCKS];
};

/**
 * Each folder dnode is like this on disk
 */
struct __attribute__((__packed__)) CrowFSDirectoryBlock {
    // The header of this folder
    struct CrowFSDnodeHeader header;
    // Parent directory of this folder
    uint32_t parent;
    // Points to the dnodes of the content of this folder.
    // The last dnode is indicated with zero block number.
    // However, there is a very edge case that this folder uses all the
    // available dnodes. In this case, we use the last of the content_dnodes
    // as well and there is no zero terminator.
    uint32_t content_dnodes[CROWFS_MAX_DIR_CONTENTS];
};

/**
 * Bitmap blocks contains a bitmap which marks 1 for each available block and 0 for
 */
struct CrowFSBitmapBlock {
    uint8_t bitmap[CROWFS_BLOCK_SIZE];
};


/**
 * Each disk block can be represented with this structure.
 */
union CrowFSBlock {
    struct CrowFSSuperblock superblock;
    struct CrowFSBitmapBlock bitmap;
    struct CrowFSDnodeHeader header;
    struct CrowFSFileBlock file;
    struct CrowFSDirectoryBlock folder;
    /**
     * The indirect block which contains links to other blocks.
     * Each value in the points to a raw_data block. The indexing is zero based
     * from the disk offset. This means that the bootloader is index zero.
     */
    uint32_t indirect_block[CROWFS_INDIRECT_BLOCK_COUNT];
    uint8_t raw_data[CROWFS_BLOCK_SIZE];
};

_Static_assert(sizeof(union CrowFSBlock) == CROWFS_BLOCK_SIZE, "Each block should be 4096 bytes");

/**
 * Result of a file stat
 */
struct CrowFSStat {
    // The type of this dnode
    uint8_t type;
    // Folder/File name with a null terminator
    char name[CROWFS_MAX_FILENAME + 1];
    //  When was this folder created? In Unix timestamp.
    int64_t creation_date;
    // The file size or the number of entries in a directory
    uint32_t size;
    // (Folders only) the parent of this folder
    uint32_t parent;
    // dnode of this file/folder
    uint32_t dnode;
};

/**
 * CrowFS is a very simple non-logged filesystem best for read mostly scenarios.
 * Maximum disk size is 2^32-1 bytes.
 * Maximum filesize is 4096*(1024+956) = 8110080 bytes ~ 8 MB
 * Maximum files in directory is 957
 *
 * Most of the concepts of this file system comes from Unix Basic Filesystem (UFS).
 *
 * The disk layout is like this:
 * [ Bootloader | Superblock | Free Block Bitmap (variable size)
 *                           | Root Folder Block | Other Blocks ]
 * Bootloader block which is the very first block is free to have any data in it.
 * Superblock contains information about the filesystem.
 * Free Block Bitmap contains a bitmap of free blocks. The size of this part
 * is variable, and it can be inferred based on the size of the disk stored in
 * the superblock.
 * Root Folder block just comes after the free block bitmap and is a folder
 * that contains the files and folders in the root of the filesystem.
 * After that, blocks of the filesystem are located.
 */
struct CrowFS {
    /**
     * Allocates an in memory block of filesystem for use.
     * Allocated block must be filled with zero.
     * @return The allocated block or NULL.
     * @note Use free_mem_block to free the memory
     */
    union CrowFSBlock *(*allocate_mem_block)(void);

    /**
     * Frees an memory block of filesystem allocated with allocate_mem_block
     */
    void (*free_mem_block)(union CrowFSBlock *);

    /**
     * Write a block to the disk
     * @param block_index The block index to write. Zero based. Zeroth block is the
     * bootloader.
     * @param block The block to write.
     * @return 0 if ok, 1 otherwise
     */
    int (*write_block)(uint32_t block_index, const union CrowFSBlock *block);

    /**
     * Read a block from the disk
     * @param block_index The block index to read. Zero based. Zeroth block is the
     * bootloader.
     * @param block The block to read and fill.
     * @return 0 if ok, 1 otherwise
     */
    int (*read_block)(uint32_t block_index, union CrowFSBlock *block);

    /**
     * Gets number of blocks in the disk. This function is only used
     * if you are going to use crowfs_new()
     * @return 0 if failure or the number of blocks on the disk
     */
    uint32_t (*total_blocks)(void);

    /**
     * Gets the current date in unix epoch format.
     * @return The unix date
     */
    int64_t (*current_date)(void);

    /**
     * Superblock of this filesystem cached in the memory to reduce
     * memory access.
     */
    struct CrowFSSuperblock superblock;

    /**
     * Number of free bitmap blocks on disk
     */
    uint32_t free_bitmap_blocks;

    /**
     * The root folder dnode index.
     */
    uint32_t root_dnode;
};

#define CROWFS_OK 0
#define CROWFS_ERR_ARGUMENT (-1)
#define CROWFS_ERR_INIT_INVALID_FS (-2)
#define CROWFS_ERR_LIMIT (-3)
#define CROWFS_ERR_NOT_FOUND (-4)
#define CROWFS_ERR_FULL (-5)
#define CROWFS_ERR_NOT_EMPTY (-6)
#define CROWFS_ERR_TOO_SMALL (-7)
#define CROWFS_ERR_IO (-8)

/**
 * Creates a new filesystem on the given disk.
 * @param fs The block device functions
 * @return CROWFS_OK if everything is fine or CROWFS_ERR_ARGUMENT
 * (if functions are not filled)
 */
int crowfs_new(struct CrowFS *fs);

/**
 * Initialize the CrowFS filesystem structure in order to be able to use
 * the filesystem. Before calling this function, all of the
 *
 * @param fs The filesystem to open.
 * @return CROWFS_OK or CROWFS_ERR_ARGUMENT (if functions are not filled)
 * or CROWFS_ERR_INIT_INVALID_FS if the filesystem is corrupt
 */
int crowfs_init(struct CrowFS *fs);

/**
 * Create a new file/directory if it does not exists
 */
#define CROWFS_O_CREATE 0b1

/**
 * Look for directory instead of a file
 */
#define CROWFS_O_DIR 0b10

/**
 * Opens a file or directory
 * @param path The absolute of the file
 * @param dnode The dnode on disk which is CrowFSFileBlock
 * @param parent_dnode The dnode of the parent folder of this file. In case of /,
 * it will return 0.
 * @param flags Flags to control the file/directory opening
 * @return CROWFS_OK or CROWFS_ERR_NOT_FOUND if the file does not exists.
 * If CROWFS_O_CREATE is set and the file exists, CROWFS_ERR_EXISTS will
 * be returned.
 * @note To create a directory, you can fuse CROWFS_O_DIR and CROWFS_O_CREATE
 */
int crowfs_open(struct CrowFS *fs, const char *path, uint32_t *dnode, uint32_t *parent_dnode, uint32_t flags);

/**
 * Write to a file at the given offset
 * @param dnode The file dnode to write into
 * @param data The data buffer to write into
 * @param size The size of the buffer to write
 * @param offset The offset of the file to write into
 * @return CROWFS_OK or CROWFS_ERR_LIMIT if the file is very big
 */
int crowfs_write(struct CrowFS *fs, uint32_t dnode, const char *data, size_t size, size_t offset);

/**
 * Read from a file from a offset.
 * @param dnode The file dnode to read from
 * @param buf The buffer to write the data into
 * @param size The size of the buffer (maximum read size)
 * @param offset The offset of the file to read from
 * @return The number of bytes read (more than zero) if everything was ok.
 * Will return zero on EOF
 */
int crowfs_read(struct CrowFS *fs, uint32_t dnode, char *buf, size_t size, size_t offset);

/**
 * Opens a directory
 * @param dnode The dnode on disk which represents a directory.
 * @param stat The child file in this directory.
 * @param offset The offset in the directory to look. Start by
 * zero for this parameter and increase it by one until this function
 * returns CROWFS_ERR_LIMIT to iterate over all children.
 * @return CROWFS_OK or CROWFS_ERR_LIMIT if offset is more than the entities
 * in this directory.
 */
int crowfs_read_dir(struct CrowFS *fs, uint32_t dnode, struct CrowFSStat *stat, size_t offset);

/**
 * Deletes a dnode and frees blocks. Dnode can be either an empty folder
 * or a file
 * @param dnode An empty directory or a file. This cannot be root dnode
 * @param parent_dnode Parent's folder dnode
 * @return CROWFS_OK or CROWFS_ERR_NOT_EMPTY if the directory not emtpy
 */
int crowfs_delete(struct CrowFS *fs, uint32_t dnode, uint32_t parent_dnode);

/**
 * Gets the stats of a file dnode
 * @param dnode The file dnode
 * @param stat The result of stats goes here
 * @return CROWFS_OK if ok
 */
int crowfs_stat(struct CrowFS *fs, uint32_t dnode, struct CrowFSStat *stat);

/**
 * Moves a file or directory to another folder.
 * This method will overwrite the file in the destination folder.
 *
 * If you try to replace a folder with another folder, two things can happen.
 * Either the destination folder is free or it isn't. If the destination folder
 * is empty, this acts like a delete of destination folder and a rename. If the
 * destination folder is not empty, this function will return CROWFS_ERR_NOT_EMPTY
 * @param dnode The dnode to move
 * @param old_parent The old parent of dnode
 * @param new_parent The new parent of dnode
 * @param new_name If not NULL, is the new name of the file
 * @return CROWFS_OK if ok. Might return CROWFS_ERR_LIMIT if the destination folder is full.
 */
int crowfs_move(struct CrowFS *fs, uint32_t dnode, uint32_t old_parent, uint32_t new_parent, const char *new_name);

/**
 * Counts the free blocks in a filesystem
 * @param fs The filesystem to count the free blocks in
 * @return The number of free blocks
 */
uint32_t crowfs_free_blocks(struct CrowFS *fs);
