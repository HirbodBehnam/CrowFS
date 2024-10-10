# CrowFS

Random filesystem based on Unix filesystem but without logging and a lot of features. Please don't use.

## Features

* About 8 MB max file size
* Folders with 958 entries in them
* 2 TB partition size max
* 254 character long filenames
* Directory structure without depth limit

## Building

This is basically a library, so you can include `crowfs.h/c` files in your project. It only uses libc itself and nothing
else. So to build this, you need to install these packages on Debian based systems.

```bash
apt install cmake build-essentials
```

Now to build the app, follow these steps. In nutshell, these steps will create a makefile to compile the app with full
optimizations.

```bash
git clone https://github.com/HirbodBehnam/CrowFS
cd CrowFS
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Running Tests

This app contains some unit tests. To run them, you need to at first build the app using the steps above; Then run `make
test` to test the app. The result should look like this:

```
Running tests...
Test project /home/hirbod/Desktop/Projects/C/CrowFS/build
      Start  1: crowfs_tests_open_file
 1/15 Test  #1: crowfs_tests_open_file ..................   Passed    0.00 sec
      Start  2: crowfs_tests_create_folder
 2/15 Test  #2: crowfs_tests_create_folder ..............   Passed    0.00 sec
      Start  3: crowfs_tests_stat
 3/15 Test  #3: crowfs_tests_stat .......................   Passed    0.00 sec
      Start  4: crowfs_tests_read_write_file_small
 4/15 Test  #4: crowfs_tests_read_write_file_small ......   Passed    0.00 sec
      Start  5: crowfs_tests_read_write_file_direct
 5/15 Test  #5: crowfs_tests_read_write_file_direct .....   Passed    0.02 sec
      Start  6: crowfs_tests_read_write_file_indirect
 6/15 Test  #6: crowfs_tests_read_write_file_indirect ...   Passed    0.04 sec
      Start  7: crowfs_tests_write_file_full
 7/15 Test  #7: crowfs_tests_write_file_full ............   Passed    0.04 sec
      Start  8: crowfs_tests_write_folder_full
 8/15 Test  #8: crowfs_tests_write_folder_full ..........   Passed    0.28 sec
      Start  9: crowfs_tests_delete_file
 9/15 Test  #9: crowfs_tests_delete_file ................   Passed    0.00 sec
      Start 10: crowfs_tests_delete_folder
10/15 Test #10: crowfs_tests_delete_folder ..............   Passed    0.00 sec
      Start 11: crowfs_tests_move
11/15 Test #11: crowfs_tests_move .......................   Passed    0.00 sec
      Start 12: crowfs_tests_read_dir
12/15 Test #12: crowfs_tests_read_dir ...................   Passed    0.00 sec
      Start 13: crowfs_tests_disk_full
13/15 Test #13: crowfs_tests_disk_full ..................   Passed    0.01 sec
      Start 14: crowfs_tests_rename
14/15 Test #14: crowfs_tests_rename .....................   Passed    0.00 sec
      Start 15: crowfs_tests_rename_move
15/15 Test #15: crowfs_tests_rename_move ................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 15

Total Test time (real) =   0.41 sec
```

> [!NOTE]
> You need to compile the program in debug in order to run tests.

## Usage

Please refer to `crowfs.h` header file and comments of functions in order to read the use of the library.

## Internals

The file system structure is very like the one in [xv6](https://github.com/mit-pdos/xv6-riscv). The boot general data on
disk looks like this:

```
+------+-------+--------------+----------+-------------+
| Boot | Super | Free  Bitmap | Root Dir | Data/Dnodes |
+------+-------+--------------+----------+-------------+
 4096B   4096B   Variable Len    4096B    Until the End
```

The boot block is free to use and is ignored by the file system. It can be used anyhow by a bootloader. Super block
contains the information about the file system and is build when the file system is created. Free Bitmap is a variable
length chunk of disk which contains a bitmap that shows which blocks are free and which blocks are not free. 1 is for
free blocks and 0 is for free blocks. Root Directory comes just after the bitmap and contains the files and folders in
the root directory of file system. From that to the end of the disk, comes the data blocks and dnode blocks in a mixed
fashion.

Each dnode is either a folder or a file's dnode. Each dnode has a header which is shared between all dnode types. This
makes it easy for us to read the metadata of dnodes without actually checking the type at first. The header looks like
this:

```
+---------------+
|     Type      |
|     uint8     |
+---------------+
| File/Dir name |
|   char[255]   |
+---------------+
| Creation Date |
|    uint64     |
+---------------+
```

Type is the type of this dnode. This can currently be either folder or file. Filename or directory name is an array of
255 characters with one null terminator. This means that the max filename for each file is 254. Creation date is the
unix timestamp of the time which the file was created. This structure is embedded for each file and folder dnode.

Each directory's dnode is like this:

```
+----------------+
|  Dnode Header  |
+----------------+
| Content Dnodes |
|  uint32[958]   |
+----------------+
```

Content dnodes are dnodes of the files and folders which are in the given folder. The end of the list is indicated with
either a zero dnode or all 958 dnodes being full. This structure implies that each folder and only contain 958 entities.

Each file's dnode is like this:

```
+----------------+
|  Dnode Header  |
+----------------+
|      Size      |
|     uint32     |
+----------------+       +----------------+
| Indirect Block | ----> | List of Blocks |
|     uint32     |       |  uint32[1024]  |
+----------------+       +----------------+
| Direct Blocks  |
|  uint32[956]   |
+----------------+
```

Size is the file size. The maximum filesize in CrowFS is $4096 \times (1024 + 956) = 8110080$ bytes. This is based on
the limitations of file structure. Direct blocks point to the disk blocks which contain the file data. The last direct
block can determined by the file size. If the filesize is more than $4096 \times 956 = 3915776$, we also need to
allocate an indirect block to store more block numbers of data.

There is still more work to do. For example, we can have support for especial files such as pipes or sockets.
We could also potentially have support for softlinks. However, softlinks will limit the path size
to $958 \times 4 = 3832$ bytes.