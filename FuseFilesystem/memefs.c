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
    uint64_t fs_ctime;       // Creation timestamp in BCD format
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
	uint64_t timestamp;
	uint32_t size;
	uint16_t ownerUID;
	uint16_t groupGID;
} __attribute__((packed)) memefs_directory_t;

static int load_superblock();
static int load_FAT();
static int load_directory();
static uint8_t to_bcd(uint8_t num);
static void generate_memefs_timestamp(uint8_t bcd_time[8]);

memefs_superblock_t main_superblock;
memefs_superblock_t backup_superblock;
uint16_t main_FAT[256];
uint16_t backup_FAT[256];
memefs_directory_t directoryBlocks[16 * 14];
uint8_t reserved_blocks[18 * BLOCK_SIZE];
uint8_t user_blocks[220 * BLOCK_SIZE];

static void *memefs_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->kernel_cache = 1;

	load_FAT();
	printf("Filename value: %s\n", main_superblock.signature);
	printf("Superblock info: %s: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s,\n %d\n",
	main_superblock.signature, (uint8_t *)&main_superblock.cleanly_unmounted,
	main_superblock.reserved_bytes, main_superblock.fs_version, main_superblock.fs_ctime,
	main_superblock.main_fat, main_superblock.main_fat_size, main_superblock.backup_fat,
	main_superblock.backup_fat_size, main_superblock.directory_start, main_superblock.directory_size,
	main_superblock.num_user_blocks, main_superblock.first_user_block,
	main_superblock.volume_label, main_superblock.unused);

	return NULL;
}

static int memefs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
	(void) fi;
	int result = lstat(path, stbuf);
	if(result == -1){
		return -ENOENT;
	}
	return 0;
}

static int memefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags){
	(void) offset;
	(void) fi;
	(void) flags;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
	filler(buf, "sample.txt", NULL, 0, FUSE_FILL_DIR_PLUS);

	return 0;
}

static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
	int index = -1;
	for(int i = 0; i < 224; i++){
		if(directory_block[i].type != 0){
			index = i;
			i = 224;
		}
	}
	if(index == -1){
		printf("There is no space\n");
		return 0; // ignore

	}
	char full[12];
	char name[8] = "\0\0\0\0\0\0\0\0";
	char ext[4] = "\0\0\0\0";
	if(strlen(path) > 12){ //size + 1 for .
		return 0; //ignore
	}
	char* token = strtok(path, ".");
	if(strlen(token) > 8){
		return 0; //ignore
	}
	strcpy(name, token);
	token = strtok(path, ".");
	if(strlen(token) > 3){
		return 0; //ignore
	}
	strcpy(ext, token);
	strcat(full, name);
	strcat(full, ext);

	for(int i = 0; i < 11; i++){
		if(!((full[i] >= 65 && full[i] <= 90) || (full[i] >= 97 && full[i] <= 122)  ||
		     	(full[i] >= 48 && full[i] <= 57) || (full[i] == 94) || (full[i] == 95) ||
		     	(full[i] == 45) || (full[i] == 61) || (full[i] == 124) || (full[i] == 0))){
			return 0; //ignore
		}
	}

	for(int i = 0; i < 224; i++){
		if(strcmp(full, directory_block[i]) == 0){
			return 0; //duplicate
		}
	}
	struct stat info;
	stat("myfilesystem.img", &info);
	uint8_t timestamp[8];
	generate_memefs_timestamp(timestamp);

	directory_block[index].type = info.st_mode;
	directory_block[index].start_block = (index + 1) * BLOCK_SIZE;
	strcpy(directory_block[index].filename, full);
	directory_block[index].unused = 0;
	directory_block[index].timestamp = timestamp;
	directory_block[index].size = 512;
	directory_block[index].ownerUID = info.st_uid;
	directory_block[index].groupGID = info.st_gid;

	for(int i = 19; i < 239; i++){
		if(main_FAT[i] != 0){
			main_FAT[i] = 0xFFFF;
			backup_FAT[i] = 0xFFFF;
		}
	}

}

static int memefs_unlink(const char *path){
	return 0;
}

static int memefs_open(const char *path, struct fuse_file_info *fi){
	int loc = open(path, fi->flags);
	if(loc == -1){
		return -ENONET;
	}
	fi->fh = loc;
	return 0;
}

static int memefs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	(void) path;
	int result = pread(fi->fh, buf, size, offset);
	if(result == -1){
		return -ENONET;
	}
	return result;
}

static int memefs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	(void) path;
	int result = pwrite(fi->fh, buf, size, offset);
	if(result == -1){
		return -ENOENT;
	}
	return result;
}

static int memefs_truncate(const char *path, off_t size, struct fuse_file_info *fi){
	return 0;
}

static int load_superblock(){
	FILE* filesystem = fopen("myfilesystem.img", "r+");
	int file_des = fileno(filesystem);

	uint8_t timestamp[8];
	generate_memefs_timestamp(timestamp);
	main_superblock.cleanly_unmounted = 0xFF;
	main_superblock.reserved_bytes = {0x00, 0x00, 0x00};
	main_superblock.fs_version = 0x000001;
	main_superblock.fs_ctime = timestamp;
	main_superblock.main_fat = 0x00FE;
	main_superblock.main_fat_size = 0x0001;
	main_superblock.backup_fat = 0x00EF;
	main_superblock.backup_fat.size = 0x0001;
	main_superblock.directory_start = 0x00FD;
	main_superblock.directory_size = 0x000D;
	main_superblock.num_user_blocks = 0x00DC;
	main_superblock.first_user_block = 0x0001;
	main_superblock.volume_label = "A Sample Volume\0";
	main_superblock.unused = 0x00;
	int i = 0;
	pread(file_des, main_superblock.signature, 16, (255 * BLOCK_SIZE) + i); //signature
	i+=16;
        pwrite(file_des, main_superblock.cleanly_unmounted, 1, (255 * BLOCK_SIZE) + i); //mounted flag
	i++;
	pwrite(file_des, main_superblock.reserved_bytes, 3, (255 * BLOCK_SIZE) + i); //reserved
	i+=3;
	pwrite(file_des, main_superblock.fs_version, 4, (255 * BLOCK_SIZE) + i); //version
	i+=4;
        pwrite(file_des, main_superblock.fs_ctime, 8, (255 * BLOCK_SIZE) + i);//BCD
	i+=8;
        pwrite(file_des, main_superblock.main_fat, 2, (255 * BLOCK_SIZE) + i);//main FAT loc
	i+=2;
        pwrite(file_des, main_superblock.main_fat_size, 2, (255 * BLOCK_SIZE) + i);//main FAT size
        i+=2;
        pwrite(file_des, main_superblock.backup_fat, 2, (255 * BLOCK_SIZE) + i);//backup FAT loc
        i+=2;
        pwrite(file_des, main_superblock.backup_fat_size, 2, (255 * BLOCK_SIZE) + i);//backup FAT size
        i+=2;
        pwrite(file_des, main_superblock.directory_start, 2, (255 * BLOCK_SIZE) + i);//directory loc
        i+=2;
        pwrite(file_des, main_superblock.directory_size, 2, (255 * BLOCK_SIZE) + i);//directory size
        i+=2;
        pwrite(file_des, main_superblock.num_user_blocks, 2, (255 * BLOCK_SIZE) + i);//userblock  count
        i+=2;
        pwrite(file_des, main_superblock.first_user_block, 2, (255 * BLOCK_SIZE) + i);//first user block
        i+=2;
        pwrite(file_des, main_superblock.volume_label, 16, (255 * BLOCK_SIZE) + i); //volume label
	i+=16;
        pwrite(file_des, main_superblock.unused, 448, (255 * BLOCK_SIZE) + i); //unused
	fclose(filesystem);
	backup_superblock = main_superblock;
	return 0;
}

static int load_FAT(){
	for(int i = 0; i < 256; i++){
		main_FAT[i] = 0x0000;
	}

	main_FAT[0] = 0xFFFF;
	for(int i = 1; i < 19; i++){
		main_FAT[i] = 0x0000 + (i + 1);
	}

	main_FAT[19] = 0x0019;
	for(int i = 253; i >= 241; i--){
		main_FAT[i] = 0x0000 + (i - 1);
	}

	main_FAT[254] = 0xFFFF;
	main_FAT[255] = 0xFFFF;

	for(int i = 0; i < 256; i++){
		backup_FAT[i] = main_FAT[i];
	}
	return 0;
}

static int load_directory(){
	return 0;
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

static const struct fuse_operations memefs_oper = {
	.init           = memefs_init,
	.getattr	= memefs_getattr,
	.readdir	= memefs_readdir,
	.create		= memefs_create,
//	.unlink		= memefs_unlink,
	.open		= memefs_open,
	.read		= memefs_read,
	.write		= memefs_write,
//	.truncate	= memefs_truncate,
};

int main(int argc, char *argv[]){
	load_superblock();
	return fuse_main(--argc, ++argv, &memefs_oper, NULL);
}

