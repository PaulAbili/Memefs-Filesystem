#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

static struct options {
	const char *filename;
	const char *contents;
	int show_help;
} options;

#define OPTION(t, p)				\
	{t, offsetof(struct options, p), 1 }

static const struct fuse_opt option_spec[] = {
	OPTION("--name=%s", filename),
	OPTION("--contents=%s", contents),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

static void *meme_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->kernel_cache = 1;
	printf("Init finished\n");
	return NULL;
}

static int meme_getattr(const char *path, struct stat *buf, struct fuse_file_info *fi){
	(void) fi; // Prevent unused parameters warning
	if(lstat(path, buf) == -1){
		return ENOENT;
	}
	memset(buf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		buf->st_mode = S_IFDIR | 0755;
		buf->st_nlink = 2;
	} else if(strcmp(path+1, options.filename) == 0){
		buf->st_mode = S_IFREG | 0444;
		buf->st_nlink = 1;
		buf->st_size = 512;
	}
	buf->st_uid = getuid();
	buf->st_gid = getgid();
	buf->st_atime = time(NULL);
	buf->st_mtime = time(NULL);

	return 0;
}

static int meme_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags){
	//Prevent unused parameters
	(void) offset;
	(void) fi;
	(void) flags;

	if(strcmp(path, "/") != 0){
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
	filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
	filler(buf, options.filename, NULL, 0, FUSE_FILL_DIR_PLUS);
	return 0;
}

static int meme_open(const char *path, struct fuse_file_info *fi){
	int loc = open(path, fi->flags);
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

static struct fuse_operations fops = {
	.init = meme_init,
        .getattr = meme_getattr,
        .readdir = meme_readdir,
        .open = meme_open,
        .read = meme_read,
};

static void show_help(const char *progname)
{
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    --name=<s>          Name of the \"hello\" file\n"
	       "                        (default: \"hello\")\n"
	       "    --contents=<s>      Contents \"hello\" file\n"
	       "                        (default \"Hello, World!\\n\")\n"
	       "\n");
}


int main(int argc, char *argv[]){
	printf("Start of all\n");
	int ret;
 	struct fuse_args args = FUSE_ARGS_INIT(++argc, --argv);
	options.filename = strdup("Test.txt");
	options.contents = strdup("I am a test\n");
	printf("Before fuse_opt_parse\n");
	if(fuse_opt_parse(&args, &options, option_spec, NULL) == -1){ // Segfaults here
		//Not inside of here
		return 1;
	}
	printf("After fuse_opt_parse\n");
	if(options.show_help){
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	ret = fuse_main(args.argc, args.argv, &fops, NULL);
	printf("Before fuse_opt_main\n");
	fuse_opt_free_args(&args);
	printf("After fuse_opt_main\n");
	return ret;
}

/**
int main(int argc, char *argv[]) {
	if (argc < 2) {
    	fprintf(stderr, "Usage: %s <filesystem image> <mount point>\n", argv[0]);
    	return 1;
	}

	// Open filesystem image
	img_fd = open(argv[1], O_RDWR);
	if (img_fd < 0) {
    	perror("Failed to open filesystem image");
    	return 1;
	}

	// HINT: Define helper functions: load_superblock and load_directory
	if (load_superblock() < 0 || load_directory() < 0) {
    	fprintf(stderr, "Failed to load superblock or directory\n");
    	close(img_fd);
    	return 1;
	}

	return fuse_main(argc - 1, argv + 1, &fops, NULL);
}

**/
