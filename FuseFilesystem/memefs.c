#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>

static int meme_getattr(const char *path, struct stat *buf, struct fuse_file_info *fi){
	(void) fi; // Prevent unused parameters warning
	if(lstat(path, buf) == -1){
		return ENOENT;
	}

	return 0;
}

static int meme_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags){
	//Prevent unused parameters
	(void) path;
	(void) offset;
	(void) fi;
	(void) flags;

	filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
	filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
	filler(buf, "test.txt", NULL, 0, FUSE_FILL_DIR_PLUS);
	return 0;
}

static int meme_open(const char *path, struct fuse_file_info *fi){
	int loc = open(path, fi->fh);
	if(loc == -1){
		return ENOENT;
	}
	fi->fh = loc; // setting the internal id to the result of the open
	return 0;
}

static int meme_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	(void) path; //Prevent unused parameters warning
	int result = pread(fi->fh, buf, size, offset);
	if(result == -1){
		return ENOENT;
	}
	return result;
}

struct fuse_operations fops = {
        .getattr = meme_getattr,
        .readdir = meme_readdir,
        .open = meme_open,
        .read = meme_read,
};

int main(int argc, char *argv[]){
	return fuse_main(argc, argv, &fops, NULL);
}
