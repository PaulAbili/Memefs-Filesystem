
# Project 4: MEMEfs — A Custom FUSE Filesystem

The MEMEfs Project was designed to simulate a filesystem. Within this project it supports common filesystem operations, read, write, readdir, getattr and a few others. This project was designed to give students experience with file system operations and secondary memory.

**** I made a small modification to mkmemefs (I really didn’t like how the signature didn’t end in a null terminator so I removed one of the ‘+’s so it now looks like "?MEMEFS+CMSC421\0" instead of "?MEMEFS++CMSC421"

Finally updates will only appear on myfilesystem.img AFTER unmounting

## How to Build?
You've been provided a Makefile. If you need to change something, please document it here. Otherwise read the following instructions.

The following will run you through how to compile and fuse setup + Build Project Explained:

```bash
# Step 1: Compile executable files - Complies memefs.c and mkmemefs.c

make all

# Step 2: Run mkmemefs to create an memefs image - Creates a filesystem.img by executing mkmemefs.c

make create_memefs_img

# Step 3: Create Test Dir (under /tmp) - Creates the memefs directory: /tmp/memefs 

make create_dir

# Step 4: Mount the Filesystem - Mounts memefs filesystem by executing memefs.c

make mount_memefs

# Step 5: Unmount the Filesystem - Unmounts the filesystem and updates myfilesystem.img

make unmount_memefs

```

# Explain Memefs Source Code
In my implementation, I store filesystem information locally, before fuse_main is called I read the information already on myfilesystem.img and after fuse_main ends I write to myfilesystem.img

The basis of this implementation was based on the hello.c and hello_11.c source code. 
Generally when searching through the directory array, the reversion_conversion function is also used inorder to convert the file name back into a path.

Memefs_getattr
After clearing the buffer and ensuring and checking if the path is empty (/). Locates the path by searching through the directory_block array. After the filename is found, set the file information into stbuf. If file cannot be found returns -ENOENT.

Memefs_readdir
After clearing the buffer and ensuring the path is empty, prints out the filename of each used directory block ( when the filename is != to “ “). Uses the reverse_conversion method to return the filename into a path and print the path (without /). 

Memefs_create
First finds an empty directory block, if it cannot be found returns -ENOSPC.
Checks the length of the path + 2 for . and /, if the file name is too large, returns out of function.
Determines whether the path filename or path extension is too large, if so returns out of function.
Checks if there are any invalid characters, if so returns out of function
Check if filename already exists if so returns out of function

Finally allocates space in at that directory index, using convert file name and adds file to FAT table

Memefs_unlink
Converts the filename back into the path and finds the file in the directory_block array.
Copies the next index information in the FAT table, and unlinks the used indexes in the FAT table. 

Memefs_open
Finds the file and sets its fh gto the index inside of the directory_blocks

Memefs_read
Finds file inside of directory_blocks
If file couldn’t be be found returns -ENOENT
Next, reads entire user blocks until FAT table is a end of block
Then reads the file user block until its a 0
Finally syncs backup fat with main fat

Memefs_write
Finds file inside of directory_blocks,
If file cannot be found return -ENOENT
Finds the last block in the main_FAT block chain, and finds out the last index used in the block.
Finds out how many FAT table blocks are required to write all of the contents into a user block and links them back into the initial 0xFFFF.
Then starts writing to the initial 0xFFFF in the FAT table and iterates until on the last index necessary in the FAT table.

Syncs back up the fat table to the main fat table.

Memefs_truncate
Adding more file blocks to a file is already done in main so this function doesn’t do anything.

Memefs_utimes
Sets the time at a certain index, by finding path in directory block and updating timestamp through generate_memefs_timestamp.

Mount_memefs

Copies information from myfilesystem.img, initially copies the superblock, if version is 1, writes default information into structures, if version number is not 1, reads information from the rest of myfilesystem.img.

Unmount_memefs
Writes information to my myfilesystem.img adds 1 to the version number

Convert_filename
Converts the path into the necessary path required by specifications
With the first 8 indices being for the filename and the next 3 indices for the file extension, and the final index for a null terminator
 
Starts with a \
Reverse_conversion 
Converts the stored file names back into the path given

To_bcd
Given in project doc

Generate_memefs_timestamp
Given in project doc

Print_bcd_timestamp
Given in project doc

# References
https://developer.ibm.com/articles/l-fuse/
https://libfuse.github.io/doxygen/fuse_8h_source.html
https://libfuse.github.io/doxygen/structfuse__file__info.html

https://wiki.osdev.org/FUSE
https://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
https://www.cs.hmc.edu/~geoff/classes/hmc.cs137.201801/homework/fuse/fusexmp_fh.c
https://github.com/libfuse/libfuse/blob/master/example/hello.c

https://man.openbsd.org/fuse_main.3
https://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html

https://linux.die.net/man/3/htons

## Authors

- [@SmilingSupernova]
