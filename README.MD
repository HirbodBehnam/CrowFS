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

TODO