# MEMEfs — A Custom FUSE Filesystem

The MEMEfs Project was designed to simulate a filesystem. Within this project it supports common filesystem operations, read, write, readdir, getattr and a few others. This project was designed to give students experience with file system operations and secondary memory.

Finally updates will only appear on myfilesystem.img AFTER unmounting

## How to Build?
The following will run you through how to compile and fuse setup + the project explained:

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

## Explain Memefs Source Code

<p> In my implementation, I store filesystem information locally, before fuse_main is called, I read the information already on myfilesystem.img and after fuse_main ends, I write to myfilesystem.img <br>

<p>The basis of this implementation was based on the hello.c and hello_11.c source code. 
Generally when searching through the directory array, the reversion_conversion function is also used inorder to convert the file name back into a path. </p>

### Memefs_getattr

<p>After clearing the buffer and ensuring and checking if the path is empty (/). Locates the path by searching through the directory_block array. After the filename is found, set the file information into stbuf. If file cannot be found returns -ENOENT.</p>

### Memefs_readdir

<p>After clearing the buffer and ensuring the path is empty, prints out the filename of each used directory block ( when the filename is != to “ “). Uses the reverse_conversion method to return the filename into a path and print the path (without /).</p>

### Memefs_create

<p>First finds an empty directory block, if it cannot be found returns -ENOSPC. <br>
Checks the length of the path + 2 for . and /, if the file name is too large, returns out of function. <br>
Determines whether the path filename or path extension is too large, if so returns out of function. <br>
Checks if there are any invalid characters, if so returns out of function <br>
Check if filename already exists if so returns out of function <br>

Finally allocates space in at that directory index, using convert file name and adds file to FAT table</p>

### Memefs_unlink

<p>Converts the filename back into the path and finds the file in the directory_block array.<br>
Copies the next index information in the FAT table, and unlinks the used indexes in the FAT table.</p>

### Memefs_open

Finds the file and sets its fh gto the index inside of the directory_blocks

### Memefs_read
<p>Finds file inside of directory_blocks<br>
If file couldn’t be be found returns -ENOENT <br>
Next, reads entire user blocks until FAT table is a end of block <br>
Then reads the file user block until its a 0 <br>
Finally syncs backup fat with main fat</p> 

### Memefs_write
<p>Finds file inside of directory_blocks, <br>
If file cannot be found return -ENOENT <br>
Finds the last block in the main_FAT block chain, and finds out the last index used in the block. <br>
Finds out how many FAT table blocks are required to write all of the contents into a user block and links them back into the initial 0xFFFF. <br>
Then starts writing to the initial 0xFFFF in the FAT table and iterates until on the last index necessary in the FAT table. <br>

Syncs back up the fat table to the main fat table.</p>

### Memefs_truncate
Adding more file blocks to a file is already done in main so this function doesn’t do anything.

### Memefs_utimes
Sets the time at a certain index, by finding path in directory block and updating timestamp through generate_memefs_timestamp.

### Mount_memefs

<p>Copies information from myfilesystem.img, initially copies the superblock, if version is 1, writes default information into structures, if version number is not 1, reads information from the rest of myfilesystem.img.</p>

### Unmount_memefs
Writes information to my myfilesystem.img adds 1 to the version number

### Convert_filename
<p>Converts the path into the necessary path required by specifications<br>
With the first 8 indices being for the filename and the next 3 indices for the file extension, and the final index for a null terminator <br>
 
Starts with a \ < br>

### Reverse_conversion 
Converts the stored file names back into the path given

### References

https://developer.ibm.com/articles/l-fuse/ <br>
https://libfuse.github.io/doxygen/fuse_8h_source.html <br>
https://libfuse.github.io/doxygen/structfuse__file__info.html <br>
https://wiki.osdev.org/FUSE <br>
https://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/ <br>
https://www.cs.hmc.edu/~geoff/classes/hmc.cs137.201801/homework/fuse/fusexmp_fh.c <br>
https://github.com/libfuse/libfuse/blob/master/example/hello.c <br>
https://man.openbsd.org/fuse_main.3 <br>
https://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html <br>
https://linux.die.net/man/3/htons <br>

### Contact

**Author:** Paul Abili <br>
**Email:** pabili1@umbc.edu <br>
