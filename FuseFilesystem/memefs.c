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

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */

static void *memefs_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->kernel_cache = 1;

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
	filler(buf, "slay.txt", NULL, 0, FUSE_FILL_DIR_PLUS);

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

static const struct fuse_operations memefs_oper = {
	.init           = memefs_init,
	.getattr	= memefs_getattr,
	.readdir	= memefs_readdir,
	.open		= memefs_open,
	.read		= memefs_read,
	.write		= memefs_write,
};

int main(int argc, char *argv[]){
	return fuse_main(--argc, ++argv, &memefs_oper, NULL);
}
