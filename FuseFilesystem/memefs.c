/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * minimal example filesystem using high-level API
 *
 * Compile with:
 *
 *     gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello
 *
 * ## Source code ##
 * \include hello.c
 */


#define FUSE_USE_VERSION 35
#define BLOCK_SIZE 512

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
typedef struct memefs_superblock {
    char signature[16];        // Filesystem signature
    uint8_t cleanly_unmounted; // Flag for unmounted state
    uint8_t reserved_bytes[3];     // Reserved bytes
    uint32_t fs_version;       // Filesystem version
    uint8_t fs_ctime[8];       // Creation timestamp in BCD format
    uint16_t main_fat;         // Starting block for main FAT
    uint16_t main_fat_size;    // Size of the main FAT
    uint16_t backup_fat;       // Starting block for backup FAT
    uint16_t backup_fat_size;  // Size of the backup FAT
    uint16_t directory_start;  // Starting block for directory
    uint16_t directory_size;   // Directory size in blocks
    uint16_t num_user_blocks;  // Number of user data blocks
    uint16_t first_user_block; // First user data block
    char volume_label[16];     // Volume label
    uint8_t unused[448];       // Unused space for alignment
} __attribute__((packed)) memefs_superblock_t;

typedef struct directory_block {
	uint16_t type;
	uint16_t start_block;
	char filename[11];
	uint8_t unused;
	uint8_t timestamp[8];
	uint32_t size;
	uint16_t ownerUID;
	uint16_t groupGID;
} __attribute__((packed)) memefs_directory_t;

static int mount_memefs();
static int unmount_memefs();
static int convert_filename(char* full, const char *path);
static void reverse_conversion(char* full, char* original);
static uint8_t to_bcd(uint8_t num);
static void generate_memefs_timestamp(uint8_t bcd_time[8]);
void print_bcd_timestamp(const uint8_t bcd_time[8]);

memefs_superblock_t main_superblock;
memefs_superblock_t backup_superblock;
uint16_t main_FAT[256];
uint16_t backup_FAT[256];
memefs_directory_t directory_blocks[16 * 14];
uint8_t reserved_blocks[18 * BLOCK_SIZE];
uint8_t user_blocks[220 * BLOCK_SIZE];
int file_des;

static void *memefs_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->kernel_cache = 1;

	file_des = open("/usr/src/project4/project4-SmilingSupernova/FuseFilesystem/myfilesystem.img", O_RDWR, 0666);
	if(file_des == -1){
		printf("Init Failed");
		return 0;
	}

	return NULL;
}

static void memefs_destroy(void *private_data){
	(void) private_data;
	close(file_des);
}

static int memefs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
	(void) fi;
	memset(stbuf, 0, sizeof(*stbuf));

	if(strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	char full[13];
	memset(full, 0, 13);

	int result = convert_filename(full, path);

	if(result != 0){
		return -ENOENT;
	}


	for(int i = 0; i < 244; i++){
		if(directory_blocks[i].type != 0 && strcmp(directory_blocks[i].filename, full + 1) == 0){
			stbuf->st_mode = directory_blocks[i].type;
			stbuf->st_nlink = 1;
			stbuf->st_size = directory_blocks[i].size;
			stbuf->st_uid = directory_blocks[i].ownerUID;
			stbuf->st_gid = directory_blocks[i].groupGID;
			return 0;
		}
	}

	return -ENOENT;
}

static int memefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags){
	(void) offset;
	(void) fi;
	(void) flags;

	if (strcmp(path, "/") != 0){
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	char original[13];

	for(int i = 0; i < 224; i++){
		if(directory_blocks[i].type != 0 && directory_blocks[i].filename[0] != '\0' && strcmp(directory_blocks[i].filename, " ") != 0){
	                reverse_conversion(directory_blocks[i].filename, original);
			filler(buf, original, NULL, 0, FUSE_FILL_DIR_PLUS);
		}
	}
	return 0;
}

static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
	printf("Starting out\n");
	int index = 224;
	for(int i = 223; i >= 0; i--){
		if(directory_blocks[i].type == 0){
			index = i;
			i = -1;
		}
	}
	printf("1: \n");
	if(index == 224){
		printf("There is no space\n");
		return -ENFILE; // ignore
	}
        printf("2: \n");
        if(strlen(path) > 13){ //size + 1 for .
                return -ENAMETOOLONG; //ignore
        }

	char full[13];
        memset(full, 0, 13);

	int result = convert_filename(full, path);

	if(result != 0){
		return -ENOENT;
	}

	for(int i = 0; i < 12; i++){
		printf("%d:%c ", i, full[i]);
	}
	printf("\n");
	for(int i = 1; i < 12; i++){
		if(!((full[i] >= 65 && full[i] <= 90) || (full[i] >= 97 && full[i] <= 122)  ||
		     	(full[i] >= 48 && full[i] <= 57) || (full[i] == 94) || (full[i] == 95) ||
		     	(full[i] == 45) || (full[i] == 61) || (full[i] == 124) || (full[i] == '\0' || full[i] == 1 || full[i] == 6))){
			printf("This Character is invalid:%c or %d it was found at index: %d\n", full[i], full[i], i);
			return -EBADF; //ignore
		}
	}

	printf("Middle\n");
	for(int i = 0; i < 224; i++){
		if(strcmp(path + 1, directory_blocks[i].filename) == 0){
			return -EEXIST; //duplicate
		}
	}

	uint8_t timestamp[8];
	generate_memefs_timestamp(timestamp);

	directory_blocks[index].type = mode;
	directory_blocks[index].start_block = (index + 15);
	memcpy(directory_blocks[index].filename, full + 1, 11);
	directory_blocks[index].unused = 0;
	directory_blocks[index].size = 512;
	directory_blocks[index].ownerUID = getuid();
	directory_blocks[index].groupGID = getgid();

	for(int i = 0; i < 8; i++){
		directory_blocks[index].timestamp[i] = timestamp[i];
	}

	main_FAT[index + 15] = 0xFFFF;
	backup_FAT[index + 15] = 0xFFFF;
	fi->fh = index;

	printf("getting to it: \n");
	return 0;
}

static int memefs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
	(void) fi;
	(void) tv;

        char full[13];
        memset(full, 0, 13);

        int result = convert_filename(full, path);

	if(result != 0){
		return -ENOENT;
	}

	for(int i = 0; i < 224; i++){
		if(directory_blocks[i].type != 0 && strcmp(directory_blocks[i].filename, full + 1) == 0){
			generate_memefs_timestamp(directory_blocks[i].timestamp);
			return 0;
		}
	}
	return -ENOENT;
}


static int memefs_unlink(const char *path){
	int index = -1;
	char full[13];
        memset(full, 0, 13);

        int result = convert_filename(full, path);

	if(result != 0){
		return 0;
	}

	for(int i = 0; i < 16 * 14; i++){
		if(strcmp(directory_blocks[i].filename, full + 1) == 0){
			index = i;
		}
	}

	if(index == -1){
		printf("Couldn't locate %s\n", path);
	}
	directory_blocks[index].type = 0;
	strcpy(directory_blocks[index].filename, " ");
	int nextFAT = directory_blocks[index].start_block;
	int current;
	do {
		current = nextFAT;
		nextFAT = main_FAT[current];
		main_FAT[current] = 0;
		backup_FAT[current] = 0;
	} while(current != 0xFFFF);

	return 0;
}

static int memefs_open(const char *path, struct fuse_file_info *fi){
	char full[13];
        memset(full, 0, 13);

	int result = convert_filename(full, path);
	if(result != 0) {
        	return -ENOENT;
    	}

    	for(int i = 0; i < 224; i++) {
        	if(directory_blocks[i].type != 0 && strcmp(directory_blocks[i].filename, full + 1) == 0) {
            		fi->fh = i; // store index in directory_blocks for later use
           		 return 0;
        	}
    	}

   	 return -ENOENT;
}

static int memefs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    (void) fi;
    char full[13];
    memset(full, 0, 13);

    if (convert_filename(full, path) != 0) {
        return -ENOENT;
    }

    int index = -1;
    for (int i = 0; i < 224; i++) {
        if (directory_blocks[i].type != 0 && strcmp(directory_blocks[i].filename, full + 1) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return -ENOENT;
    }

	int read_bytes = 0;
	for(int i = 0; (size_t) i < size; i++){
		buf[i] = user_blocks[i];
		read_bytes++;
	}

	return read_bytes;
}

static int memefs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	char full[13];
	memset(full, 0, 13);

	if (convert_filename(full, path) != 0) {
        	return -ENOENT;
    	}

    	int index = -1;
    	for (int i = 0; i < 224; i++) {
    	    if (directory_blocks[i].type != 0 && strcmp(directory_blocks[i].filename, full + 1) == 0) {
    	        index = i;
    	        break;
    	    }
    	}

    	if (index == -1) {
    	    return -ENOENT;
    	}

	int write_bytes = 0;
	for(int i = 0; (size_t) i < size; i++){
		user_blocks[i] = buf[i];
		write_bytes++;
	}

	return write_bytes;
}

static int memefs_truncate(const char *path, off_t size, struct fuse_file_info *fi){
	(void) path;
	(void) size;
	(void) fi;
	return 0;
}
/**
 * Copies signature information from image if version is 1, intializes variables
 */
static int mount_memefs(){
	FILE* filesystem = fopen("myfilesystem.img", "r+");
	if(filesystem == NULL){
		printf("Couldn't Open in mount\n");
		return 0;
	}
        int file_des = fileno(filesystem);
	int i = 0;
	uint16_t temp;
	uint32_t big_temp;
	pread(file_des, &main_superblock.signature, 16, (255 * BLOCK_SIZE) + i); // string
	i+=16;
	pread(file_des, &temp, 1, (255 * BLOCK_SIZE) + i);
	main_superblock.cleanly_unmounted = 0xFF;
	i++;
        //reserved bytes
	i+=3;
	pread(file_des, &big_temp, 4, (255 * BLOCK_SIZE) + i);
    	main_superblock.fs_version = ntohl(big_temp);
	i+=4;
	for(int j = 0; j < 8; j++){
        	pread(file_des, &temp, 1, (255 * BLOCK_SIZE) + i + j);
       		main_superblock.fs_ctime[j] = temp;
	}
	i+=8;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.main_fat = ntohs(temp);
	i+=2;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.main_fat_size = ntohs(temp);
	i+=2;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.backup_fat = ntohs(temp);
	i+=2;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.backup_fat_size = ntohs(temp);
	i+=2;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.directory_start = ntohs(temp);
	i+=2;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.directory_size = ntohs(temp);
	i+=2;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.num_user_blocks = ntohs(temp);
	i+=2;
        pread(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);
        main_superblock.first_user_block = ntohs(temp);
	i+=2;
        pread(file_des, &main_superblock.volume_label, 16, (255 * BLOCK_SIZE) + i); //string
	i+=16;
      	//unused bytes
	backup_superblock = main_superblock;

	i = 0;
	//FAT
	for(int j = 0; j < 256; j++){
		pread(file_des, &temp, 2, (254 * BLOCK_SIZE) + i);
		i+=2;
		main_FAT[j] = temp;
	}
	i = 0;
	//Backup FAT
	for(int j = 0; j < 256; j++){
		pread(file_des, &temp, 2, (239 * BLOCK_SIZE) + i);
		i+=2;
		backup_FAT[j] = temp;
	}
	if(main_superblock.fs_version == 1){
		//intialize vars
		//DIRECTORY
		for(int j = 0; j < 16 * 14; j++){
			directory_blocks[j].type = 0;
			directory_blocks[j].start_block = -1;
			strcpy(directory_blocks[j].filename, " ");
			directory_blocks[j].unused = 0;
			directory_blocks[j].timestamp[0] = 0;
                        directory_blocks[j].timestamp[1] = 0;
                        directory_blocks[j].timestamp[2] = 0;
                        directory_blocks[j].timestamp[3] = 0;
                        directory_blocks[j].timestamp[4] = 0;
                        directory_blocks[j].timestamp[5] = 0;
                        directory_blocks[j].timestamp[6] = 0;
                        directory_blocks[j].timestamp[7] = 0;
			directory_blocks[j].size = 0;
			directory_blocks[j].ownerUID = -1;
			directory_blocks[j].groupGID = -1;
		}
		//USERBLOCKS
		for(int j = 0; j < 220 * BLOCK_SIZE; j++){
			user_blocks[j] = 0;
		}
	} else {
		i = 0;
		//copy everything from img
		//Directory
		for(int j = 0; j < 16 * 14; j++){
			pread(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
			directory_blocks[j].type = ntohs(temp);
			i+=2;
                        pread(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
                	directory_blocks[j].start_block = ntohs(temp);
			i+=2;
                        pread(file_des, &directory_blocks[j].filename, 11, (240 * BLOCK_SIZE) + i);
			i+=11;
                        //unused
			for(int j = 0; j < 8; j++){
                        	pread(file_des, &temp, 1, (240 * BLOCK_SIZE) + i);
                		directory_blocks[j].timestamp[j] = temp;
				i++;
			}
                        pread(file_des, &big_temp, 4, (240 * BLOCK_SIZE) + i);
                	directory_blocks[j].size = ntohl(big_temp);
			i+=4;
                        pread(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
                	directory_blocks[j].ownerUID = ntohs(temp);
			i+=2;
                        pread(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
                	directory_blocks[j].groupGID = ntohs(temp);
		}
		//User Blocks
		i = 0;
		for(int j = 0; j < 220 * BLOCK_SIZE; j++){
			pread(file_des, &temp, 2, (1 * BLOCK_SIZE) + i);
			user_blocks[j] = ntohs(temp);
			i+=2;
		}
	}

	fclose(filesystem);

	return 0;
}

static int unmount_memefs(){
	FILE* filesystem = fopen("myfilesystem.img", "w+");
        int file_des = fileno(filesystem);

	//Superblock
	int i = 0;
	uint16_t temp;
	uint32_t big_temp;

	main_superblock.cleanly_unmounted = 0;
	backup_superblock.cleanly_unmounted = 0;
	pwrite(file_des, &main_superblock.signature, 16, (255 * BLOCK_SIZE) + i); //signature
	i+=16;
	temp = htons(main_superblock.cleanly_unmounted);
        pwrite(file_des, &temp, 1, (255 * BLOCK_SIZE) + i); //mounted flag
	i++;
	i+=3; //reserved
	big_temp = htonl(main_superblock.fs_version + 1);
	pwrite(file_des, &big_temp, 4, (255 * BLOCK_SIZE) + i); //version
	i+=4;
	for(int j = 0; j < 8; j++){
		temp = main_superblock.fs_ctime[j];
        	pwrite(file_des, &temp, 1, (255 * BLOCK_SIZE) + i + j);//BCD
	}
	i+=8;
	temp = htons(main_superblock.main_fat);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//main FAT loc
	i+=2;
	temp = htons(main_superblock.main_fat_size);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//main FAT size
        i+=2;
	temp = htons(main_superblock.backup_fat);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//backup FAT loc
        i+=2;
	temp = htons(main_superblock.backup_fat_size);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//backup FAT size
        i+=2;
	temp = htons(main_superblock.directory_start);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//directory loc
        i+=2;
	temp = htons(main_superblock.directory_size);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//directory size
        i+=2;
	temp = htons(main_superblock.num_user_blocks);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//userblock  count
        i+=2;
	temp = htons(main_superblock.first_user_block);
        pwrite(file_des, &temp, 2, (255 * BLOCK_SIZE) + i);//first user block
        i+=2;
        pwrite(file_des, &main_superblock.volume_label, 16, (255 * BLOCK_SIZE) + i); //volume label
	i+=16;
        //unused

	//Backup Superblock
	i = 0;
	pwrite(file_des, &backup_superblock.signature, 16, (0 * BLOCK_SIZE) + i); //writing the same signature to backup superblock
	i+=16;
	temp = htons(backup_superblock.cleanly_unmounted);
        pwrite(file_des, &temp, 1, (0 * BLOCK_SIZE) + i); //mounted flag
	i++;
	i+=3; //reserved
	big_temp = htonl(backup_superblock.fs_version);
	pwrite(file_des, &big_temp, 4, (0 * BLOCK_SIZE) + i); //version
	i+=4;
	for(int j = 0; j < 8; j++){
		temp = backup_superblock.fs_ctime[j];
        	pwrite(file_des, &temp, 1, (0 * BLOCK_SIZE) + i + j);//BCD
	}
	i+=8;
	temp = htons(backup_superblock.main_fat);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//main FAT loc
	i+=2;
	temp = htons(backup_superblock.main_fat_size);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//main FAT size
        i+=2;
	temp = htons(backup_superblock.backup_fat);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//backup FAT loc
        i+=2;
	temp = htons(backup_superblock.backup_fat_size);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//backup FAT size
        i+=2;
	temp = htons(backup_superblock.directory_start);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//directory loc
        i+=2;
	temp = htons(backup_superblock.directory_size);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//directory size
        i+=2;
	temp = htons(backup_superblock.num_user_blocks);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//userblock  count
        i+=2;
	temp = htons(backup_superblock.first_user_block);
        pwrite(file_des, &temp, 2, (0 * BLOCK_SIZE) + i);//first user block
        i+=2;
        pwrite(file_des, &backup_superblock.volume_label, 16, (0 * BLOCK_SIZE) + i); //volume label
	i+=16;
        //unused
	i = 0;
	//FAT
	for(int j = 0; j < 256; j++){
		temp = htons(main_FAT[j]);
		pwrite(file_des, &temp, 2, (254 * BLOCK_SIZE) + j);
	}
	//Backup FAT
	for(int j = 0; j < 256; j++){
		temp = htons(backup_FAT[j]);
		pwrite(file_des, &temp, 2, (239 * BLOCK_SIZE) + j);
	}
	//Single Directory Blocks
	for(int j = 0; j < 16 * 14; j++){
		temp = htons(directory_blocks[j].type);
		pwrite(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
		i+=2;
		temp = htons(directory_blocks[j].start_block);
		pwrite(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
		i+=2;
		pwrite(file_des, &directory_blocks[j].filename, 11, (240 * BLOCK_SIZE) + i);
		i+=11;
		i++; // unused
		for(int k = 0; k < 8; k++){
			temp = directory_blocks[j].timestamp[k];
			pwrite(file_des, &temp, 1, (240 * BLOCK_SIZE) + i + k);
		}
		i+=8;
		big_temp = htonl(directory_blocks[j].size);
		pwrite(file_des, &big_temp, 4, (240 * BLOCK_SIZE) + i);
		i+=4;
		temp = htons(directory_blocks[j].ownerUID);
		pwrite(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
		i+=2;
		temp = htons(directory_blocks[j].groupGID);
		pwrite(file_des, &temp, 2, (240 * BLOCK_SIZE) + i);
		i+=2;
	}
	//Reserved Blocks

	//User Data Blocks
	for(int j = 0; j < 220 * BLOCK_SIZE; j++){
		pwrite(file_des, &user_blocks[j], 1, (19 * BLOCK_SIZE) + j);
	}

	fclose(filesystem);
	return 0;
}

static int convert_filename(char* full, const char* path){
	int period = 0;
        int postPeriod = 0;
        int difference;
        int ext;
        for(int i = 0; i < 12; i++){
                if(period == 0 && i == 9){
                     return EMSGSIZE;
                }

                if(path[i] == 46){
                    period = 1;
                    if(strlen(path) - i > 4){
                        return EMSGSIZE;
                    }
                    ext = (strlen(path) - i - 2) * -1;
                    difference = (strlen(path) - i - (i - 5)) + ext + 2;
                }

                if(period == 0){
                    full[i] = path[i];
                }

                if(period == 1 && postPeriod < 3){
                    if(i != 9){
          	          full[i] = '\0';
                    }
                    full[i + difference] = path[i + 1];
                    postPeriod++;
                }
        }
	full[12] = '\0';
	return 0;
}

static void reverse_conversion(char* full, char* original){
	memset(original, '\0', 13);
	int counter = 0;

	original[counter++] = '/';

	for(int i = 0; i < 8 && full[i] != '\0'; i++){
		original[counter++] = full[i];
	}

	if(full[8] != '\0'){
		original[counter++] = '.';

		for(int j = 8; j < 11 && full[j] != '\0'; j++){
			original[counter++] = full[j];
		}
	}
	original[counter] = '\0';
}

static uint8_t to_bcd(uint8_t num){
	if(num > 99){
		return 0xFF;
	}
	return ((num/10) << 4) | (num % 10);
}

static void generate_memefs_timestamp(uint8_t bcd_time[8]) {
	time_t now = time(NULL);
	struct tm utc;
	gmtime_r(&now, &utc); // UTC time (MEMEfs uses UTC, not localtime)

	int full_year = utc.tm_year + 1900;
	bcd_time[0] = to_bcd(full_year / 100); 	// Century
	bcd_time[1] = to_bcd(full_year % 100); 	// Year within century
	bcd_time[2] = to_bcd(utc.tm_mon + 1);  	// Month (0-based in tm)
	bcd_time[3] = to_bcd(utc.tm_mday);     	// Day
	bcd_time[4] = to_bcd(utc.tm_hour);     	// Hour
	bcd_time[5] = to_bcd(utc.tm_min);      	// Minute
	bcd_time[6] = to_bcd(utc.tm_sec);      	// Second
	bcd_time[7] = 0x00;                     // Unused (reserved)
}

void print_bcd_timestamp(const uint8_t bcd_time[8]) {
	printf("BCD Timestamp (hex): ");
	for (int i = 0; i < 8; ++i)
    	printf("%02X ", bcd_time[i]);
	printf("\n");

	printf("Readable Timestamp (UTC): 20%02X-%02X-%02X %02X:%02X:%02X\n",
       	bcd_time[1], bcd_time[2], bcd_time[3],
       	bcd_time[4], bcd_time[5], bcd_time[6]);
}

static const struct fuse_operations memefs_oper = {
	.init           = memefs_init,
	.destroy	= memefs_destroy,
	.getattr	= memefs_getattr,
	.readdir	= memefs_readdir,
	.create		= memefs_create,
	.utimens	= memefs_utimens,
	.unlink		= memefs_unlink,
	.open		= memefs_open,
	.read		= memefs_read,
	.write		= memefs_write,
	.truncate	= memefs_truncate,
};

int main(int argc, char *argv[]){
	mount_memefs();
	int result = fuse_main(--argc, ++argv, &memefs_oper, NULL);
	unmount_memefs();
	return result;
}

