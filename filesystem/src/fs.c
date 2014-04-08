/*
 * Doug Slater and Christopher Craig
 * mailto:cds@utk.edu, mailto:ccraig7@utk.edu
 * CS560 Filesystem Lab submission
 * Dr. Qing Cao
 * University of Tennessee, Knoxville
 */

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#endif

static char* fname = "fs";	/* The name our filesystem will have on disk */
filesystem* fs;				/* Pointer to the in-memory representation of the fs, e.g. freelist && superblock */
dirent* root;				/* Pointer to the root directory entry */

/* 
 * Return an inode the fields of which obviously indicate a problem
 */
inode* ErrorInode() {
	inode* ret = (inode*)malloc(sizeof(inode));

	ret->num = 0;
	ret->nlinks = -1;
	ret->size = -1;
	ret->mode = FS_ERR;

	return ret;
}

/* Follow a an array of cstrings which are the fields of a path, e.g. input of 
 * { "bob", "dylan", "is", "old" } represents path "/bob/dylan/is/old"
 * Return the inode at the end of this path or ErrorInode if not found.
 */ 
inode* dir_recurse(dirent* dir, int depth, int pathFields, char* name, char* path[]) {
	int i;													// Declaration here to satisfy Visual C compiler

	for (i = 0; i < dir->numDirs; i++)						// For each subdir at this level
	{
		if (!strcmp(dir->dirents[i]->name, path[i])) {		// If we have a matching directory name
			if (depth == pathFields)						// If we can't go any deeper
				return &dir->dirents[i]->ino;				// Return the inode of the matching dir
			else return dir_recurse(dir->dirents[i],		// Else recurse another level
									depth+1, 
									pathFields-1, 
									name, 
									&path[1]); 
		}
	}
	return ErrorInode();									// If nothing found, return a special error inode
}

/*
 * Return the inode of the directory at the path "name"
 */
inode* fs_stat(char* name) {

	char* path[FS_MAXPATHFIELDS];
	char* next_field = NULL;
	char* name_cpy = (char*)malloc(strlen(name)*sizeof(char));
	const char* delimiter = "/";				// Path separator. Who doesn't use "/" ?
	int depth = 0;								// The depth of the path, e.g. depth of "/a/b/c" is 3

	strcpy(name_cpy, name);						// Copy the path because strtok replaces delimieter with '\0'
	if (NULL == &root) { return NULL; }			// No filesystem yet, bail!

	// Split the path on '/'
	next_field = strtok(name_cpy, delimiter);
	while (NULL != next_field) {
		path[depth] = (char*)malloc(strlen(next_field)*sizeof(char));
		strcpy(path[depth], next_field);
		next_field = strtok(NULL, delimiter);
		depth++;
	}

	if (depth == 0) return &(root->ino);		// Return if we are at the root
	else return dir_recurse(root, 0, depth,
							name, &path[1]);	// Else traverse the path, get matching inode

	return ErrorInode();
}

int fs_mkdirent(char* currentdir, char* dirname) {
	inode* current_inode = fs_stat(currentdir);
	if (current_inode->num == 0) return FS_ERR;

	// TODO: Get dirent using current_inode

	return FS_ERR;
}

int fs_mkfs() {
   	int i, ERR;
	char buf[BLKSIZE];
	FILE* fp = fopen(fname, "w");

/* Preallocate a contiguous file. Differs across platforms */
#if defined(_WIN64) || defined(_WIN32)
	LARGE_INTEGER offset;
	offset.QuadPart = BLKSIZE*MAXBLOCKS;
	ERR = SetFilePointerEx(fp, offset, NULL, FILE_BEGIN);
	SetEndOfFile(fp);
#elif __APPLE__	/* __MACH__ also works */
    fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, BLKSIZE*MAXBLOCKS};
    ERR = fcntl(fileno(fp), F_PREALLOCATE, &store);
#elif __unix__
    ERR = posix_fallocate(fp, 0, BLKSIZE*MAXBLOCKS);
#endif
    
	if (ERR < 0) { 
		perror("allocation error");
		return ERR;
	}

	fs = (filesystem*)malloc(sizeof(filesystem));
	fs->blocks[0] = (block*)malloc(sizeof(block));
	fs->blocks[1] = (block*)malloc(sizeof(block));

	fs->superblock = fs->blocks[0];
	fs->free_block_bitmap = fs->blocks[1];

	memset(fs->superblock->data, 0, BLKSIZE);				// Zero-out the superblock
	memset(fs->free_block_bitmap->data, 0, BLKSIZE);		// Zero-out the free block

	for (i = 0; i < MAXBLOCKS; i++) {
		fseek(fp, BLKSIZE*i, SEEK_SET);

		if (i==0) fputs(fs->superblock->data, fp);
		else if (i==1) fputs(fs->free_block_bitmap->data, fp);
		else {
			block* newBlock = (block*)malloc(sizeof(block));	// Make a new block
			memset(newBlock, 0, BLKSIZE);						// Zero it out

			sprintf(newBlock->data, "-block %d-", i);
			fputs(newBlock->data, fp);
		}
	}

	// Setup root directory entry
	root = (dirent*)malloc(sizeof(dirent));
	strcpy(root->name, "/");
	root->ino.num = 1;
	root->ino.nlinks = 0;
	root->ino.mode = FS_DIR;
	root->ino.size = BLKSIZE;
	root->numDirs = 0;
	root->numFiles = 0;
	root->numLinks = 0;

	// Allocate memory all of this directory entry's direct blocks 
	for (i = 0; i < NBLOCKS; i++)
		root->ino.blocks[i] = (block*)malloc(sizeof(block));

	// Put the the root directory entry at the beginning 
	// of the filesystem superblock
	memcpy(fs->superblock->data, root, sizeof(*root));

	// Write the in-memory copy of the filesystem superblock to the disk
	fseek(fp, 0, SEEK_SET);
	fwrite(fs->superblock->data, sizeof(fs->superblock->data[0]), BLKSIZE, fp);

	// As a test: Read the superblock stored on disk back into memory (works!)
	fseek(fp, 0, SEEK_SET);
	fread(fs->superblock->data, sizeof(fs->superblock->data[0]), BLKSIZE, fp);

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", root->name, BLKSIZE*MAXBLOCKS/1024);
	
	return 0;
}