# Operating Systems OS_hw5
## 21900842 Abigail Jenos Yong Wai Mun

This project implements a simple file system using the FUSE (Filesystem in Userspace) library in C. The file system is designed to work with JSON configuration files, representing the file system structure.  
Youtube link: https://youtu.be/RjgnzfelE3U  


## 1. Features

- Supports the creation and deletion of files and directories.
- Allows reading from and writing to files.
- Implements basic file system operations such as getting file attributes and reading directory entries.

## 2. Dependencies

The following libraries are required to build and run the file system:

- FUSE library (`libfuse`)
- JSON-C library (`libjson-c`)
- PThread library (`libpthread`)

Make sure you have these dependencies installed before compiling the code.

## 3. How to compile

To compile the code, use the following command:
```
gcc -Wall -o fuse_fs fuse_fs.c -lfuse -ljson-c -lpthread
```


## 4. Usage

To run the file system, execute the compiled binary with the mount path and the path to the JSON configuration file as command-line arguments:
```
./fuse_fs <mount_path> <fs_json_path>
```


- `<mount_path>`: The directory where the file system will be mounted.
- `<fs_json_path>`: The path to the JSON configuration file representing the file system structure.

## 5. JSON Configuration

The file system structure is defined using a JSON configuration file. The JSON file should contain an array of objects, where each object represents a file or a directory in the file system.

## 6. File Object

- `inode` (integer): The inode number of the file.
- `type` (string): The type of the file (`reg` for regular file).
- `data` (string): The content of the file.

## 7. Directory Object

- `inode` (integer): The inode number of the directory.
- `type` (string): The type of the directory (`dir`).
- `entries` (array): An array of directory entries.

## 8. Directory Entry Object

- `inode` (integer): The inode number of the entry.
- `name` (string): The name of the entry.
- `parent_inode` (integer): The inode number of the parent directory.

**Example JSON Configuration:**

```json
[
  {
    "inode": 0,
    "type": "dir",
    "entries": [
      {
        "inode": 1,
        "name": "file1.txt",
        "parent_inode": 0
      },
      {
        "inode": 2,
        "name": "file2.txt",
        "parent_inode": 0
      }
    ]
  },
  {
    "inode": 1,
    "type": "reg",
    "data": "This is the content of file1.txt"
  },
  {
    "inode": 2,
    "type": "reg",
    "data": "This is the content of file2.txt"
  }
]
```

## 9. File System Operations & defintions
The file system supports the following operations:

getattr: Get file attributes.  
readdir: Read directory entries.  
open: Open a file.  
read: Read from a file.  
write: Write to a file.  
create: Create a file.  
mkdir: Create a directory.  
unlink: Delete a file or directory.  
Each operation is implemented in a separate function within the code.

## 10. Homework details
Name: Abigail Jenos Yong Wai Mun  
Student ID: 21900842
OS HW5
