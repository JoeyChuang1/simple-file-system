# Mountable Simple File System (SFS)

## Overview
The Mountable Simple File System (SFS) is a lightweight file system implemented for Linux. It can be mounted under a directory and provides basic file system operations, suitable for embedded applications. SFS introduces intentional restrictions to simplify the design, such as single-level directory structures and limited filename lengths. 

This project uses FUSE (Filesystem in Userspace) to interact with the Linux kernel, leveraging an emulated disk system for persistent storage.

## Features and Specifications

### Core Features
- **Mountable File System**: Allows users to mount the SFS under any directory on a Linux machine.
- **Single Root Directory**: A flat directory structure with no subdirectories.
- **Filename and Extension Restrictions**: Filenames are limited to 16 characters, and extensions are capped at 3 characters.
- **Persistence**: Data stored in the emulated disk persists across program executions.

### Key Functions
Below are descriptions of the functions implemented in the SFS:

#### 1. `void mksfs(int fresh)`
Creates and initializes the SFS. If `fresh` is set to `1`, a new file system is created from scratch, formatting the disk. If set to `0`, an existing file system is loaded from the disk.

#### 2. `int sfs_getnextfilename(char *fname)`
Iterates through the files in the root directory. Copies the name of the next file into `fname`. Returns `1` if there are more files to iterate, or `0` otherwise.

#### 3. `int sfs_getfilesize(const char *path)`
Returns the size of the file specified by `path` in bytes.

#### 4. `int sfs_fopen(char *name)`
Opens a file. If the file does not exist, it creates a new file. Returns a file descriptor for the opened file.

#### 5. `int sfs_fclose(int fileID)`
Closes an opened file identified by `fileID`. Returns `0` on success, or a negative value on failure.

#### 6. `int sfs_fwrite(int fileID, char *buf, int length)`
Writes data from `buf` into the file associated with `fileID`. Returns the number of bytes written.

#### 7. `int sfs_fread(int fileID, char *buf, int length)`
Reads data from the file associated with `fileID` into `buf`. Returns the number of bytes read.

#### 8. `int sfs_fseek(int fileID, int loc)`
Moves the read/write pointer of the file associated with `fileID` to `loc`. Returns `0` on success or a negative value on failure.

#### 9. `int sfs_remove(char *file)`
Deletes a file from the root directory. Frees the associated data blocks and updates metadata.

## Optimization Details

### 1. In-Memory Caching
- **Directory Table Cache**: Frequently accessed directory entries are cached in memory to reduce disk I/O and improve performance.
- **i-Node Cache**: Maintains active i-Node structures in memory for faster file access.

### 2. Efficient Disk Block Allocation
- **Bitmap for Free Blocks**: Utilized a bitmap to track free and allocated blocks, ensuring constant-time block allocation.
- **Sequential Writes**: Data is written in sequential blocks to minimize fragmentation and enhance read/write speeds.

### 3. Reduced Overhead
- The single-level directory simplifies path resolution, reducing computational overhead.
- Only essential metadata is maintained to minimize memory usage.

## Edge Cases and Considerations

### 1. Filename and Extension Validation
- Ensures that filenames exceed neither the 16-character limit nor the 3-character extension limit. Returns appropriate error codes for invalid names.

### 2. Handling Disk Full Scenarios
- Validates available space before writing. If the disk is full, write operations return an error code.

### 3. Concurrent Access Restriction
- Prevents multiple processes from simultaneously accessing the same file, as SFS does not support concurrent access.

### 4. File Pointer Management
- Ensures that read/write pointers stay within file bounds. Returns errors for invalid seek operations.

### 5. Persistent Storage Corruption
- Validates the superblock and metadata integrity during initialization to detect and recover from corrupted states.

### 6. Deletion of Open Files
- Safeguards against deleting files that are currently open. If attempted, the operation fails with an error message.

### 7. Large File Handling
- Implements single indirect pointers in the i-Node structure, allowing support for files larger than direct pointer capacity.

## Testing and Debugging
- **Test Suite**: Includes five test files (`sfs_test[0-4].c`) to validate core functionalities.
  - `sfs_test0.c`: Tests file creation, deletion, and basic I/O.
  - `sfs_test1.c`: Validates filename restrictions and edge cases.
  - `sfs_test2.c`: Tests persistence by reopening the file system.
  - `sfs_test3.c`: Verifies i-Node table and metadata integrity.
  - `sfs_test4.c`: Performs stress testing with large files and boundary conditions.

- **Debugging Tools**: Utilized GDB and custom logging mechanisms to trace errors and inspect memory.

## How to Run
1. **Build the Project**:
   ```bash
   make
   ```

2. **Run the File System**:
   ```bash
   ./MyFilesystem_sfs mountpoint
   ```

3. **Unmount the File System**:
   ```bash
   fusermount -u mountpoint
   ```

## Conclusion
The Mountable Simple File System is a robust, minimalistic file system ideal for embedded environments. Its design prioritizes simplicity and correctness while offering fundamental file operations. With its modular architecture and adherence to FUSE, SFS demonstrates the core principles of file system implementation.
