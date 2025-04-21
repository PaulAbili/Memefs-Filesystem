
# Project 4: MEMEfs — A Custom FUSE Filesystem

<!-- TODO (1): A brief description of what this project does and who it's for! -->

## How to Build?
You've been provided a Makefile. If you need to change something, please document it here. Otherwise read the following instructions.

The following will run you through how to compile and fuse setup:

<!-- NOTE: below is how you represent a code block in markdown! -->
```bash
# Step 1: Compile mkmemefs

make build_mkmemefs

# Step 2: Run mkmemefs to create an memefs image

./mkmemefs <image_filename> <volume-name>

# Step 3: Create Test Dir (under /tmp). Note that this dir only needs to be created once. DO NOT PULL FILES IN THIS DIR (IT NEEDS TO REMAIN EMPTY). YOU'VE BEEN WARNED. 

make create_dir

# Step 4: Mount the Filesystem
<!-- TODO (2): Read the makefile and figure out the remaining commands.  -->

```


# Explain the Build Process
<!-- TODO (3): Complete this.  -->

# Explain Memefs Source Code
<!-- TODO (4): Complete this.  -->

# References
[IBM: l-fuse](https://developer.ibm.com/articles/l-fuse/)
<!-- You're encouraged to add references. Above is the Markdown.md method to add references -->


## Authors
<!-- You should always credit yourself in readme's of projects you work on -->
- [@<your-github-account>](https://www.github.com/<your-github-repo>)
