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

char* fname = "fs";		/* The name our filesystem will have on disk */
filesystem* fs;			/* Pointer to the in-memory representation of the fs, e.g. freelist && superblock */
dirent* root;			/* Pointer to the root directory entry */
dir_data* root_data;		/* Pointer to root directory data */
int blocksAllocated[MAXFILEBLOCKS]; // Indices of allocated blocks

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
inode* dir_recurse(dirent* dir, dir_data* root_data, int depth, int pathFields, char* name, char* path[]) {
	int i;									// Declaration here to satisfy Visual C compiler

	for (i = 0; i < root_data->numDirs; i++)				// For each subdir at this level
	{
		if (!strcmp(root_data->dirents[i]->name, path[i])) {		// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return &root_data->dirents[i]->ino;		// Return the inode of the matching dir

			// Else recurse another level
			else return dir_recurse(root_data->dirents[i],
						root_data,			// TODO: need to pass in dir_data
						depth+1, 
						pathFields-1, 
						name, 
						&path[1]); 
		}
	}
	return ErrorInode();					// If nothing found, return a special error inode
}

/*
 * Return the inode of the directory at the path "name"
 */
inode* fs_stat(char* name) {

	char* path[FS_MAXPATHFIELDS];
	char* next_field = NULL;
	char* name_cpy = (char*)malloc(strlen(name)*sizeof(char));
	const char* delimiter = "/";		// Path separator. Who doesn't use "/" ?
	int depth = 0;				// The depth of the path, e.g. depth of "/a/b/c" is 3

	strcpy(name_cpy, name);			// Copy the path because strtok replaces delimieter with '\0'
	if (NULL == &root) { return NULL; }	// No filesystem yet, bail!

	// Split the path on '/'
	next_field = strtok(name_cpy, delimiter);
	while (NULL != next_field) {
		path[depth] = (char*)malloc(strlen(next_field)*sizeof(char));
		strcpy(path[depth], next_field);
		next_field = strtok(NULL, delimiter);
		depth++;
	}

	if (depth == 0) return &(root->ino);		// Return if we are at the root
	else return dir_recurse(root, root_data, 0, depth,
				name, &path[1]);	// Else traverse the path, get matching inode

	return ErrorInode();
}

int fs_mkdirent(char* currentdir, char* dirname) {
	inode* current_inode = fs_stat(currentdir);
	if (current_inode->num == 0) return FS_ERR;

	// TODO: Get dirent using current_inode

	return FS_ERR;
}

int fs_openfs() {
	FILE* fp = fopen(fname, "w+");
	if (NULL == fp) return FS_ERR;

	if (root) free(root);
	root = (dirent*)malloc(sizeof(dirent));

	if (NULL != fs) free(fs);
	if (NULL != fs->blocks) {
		if (NULL != fs->blocks[0]) free(fs->blocks[0]);
		if (NULL != fs->blocks[1]) free(fs->blocks[1]);
	}

	fs = (filesystem*)malloc(sizeof(filesystem));
	fs->blocks[0] = (block*)malloc(sizeof(block));
	fs->blocks[1] = (block*)malloc(sizeof(block));

	fs->superblock = fs->blocks[0];
	fs->free_block_bitmap = fs->blocks[1];

	memset(fs->superblock->data, 0, BLKSIZE);			// Zero-out the superblock
	memset(fs->free_block_bitmap->data, 0, BLKSIZE);		// Zero-out the free block

	fseek(fp, 0, SEEK_SET);						// Read the superblock
	fread(	fs->superblock->data,					// stored on disk back into memory
		sizeof(fs->superblock->data[0]), BLKSIZE, fp);

	fseek(fp, BLKSIZE, SEEK_SET);
	fread(	fs->free_block_bitmap->data,				// Also read the free block bitmap
		sizeof(fs->free_block_bitmap->data[0]), BLKSIZE, fp);	// from disk into memory

	printf("%ld %ld %ld\n", sizeof(*root), sizeof(dirent), sizeof(fs->superblock->data));
	memcpy(root, fs->superblock->data, sizeof(*root));

	if (!strcmp(root->name,"/"))					// If the root is named "/"
		return FS_OK;
	return FS_ERR;
}

// Traverse the free block array and return the index of first free element
int fs_allocate_block() {
	int i;
	int base = 2;	// Blocks 0 and 1 are superblock and free map
	for (i = base; i < MAXBLOCKS; i++)
	{
		if (fs->free_block_bitmap->data[i] == 0x0) { 
			fs->free_block_bitmap->data[i]++;	// TODO: Does this increment by 1 bit?
			return i;
		}
	}
	return FS_ERR;
}

int fs_mkfs() {
   	int i, j, k, l, ERR;
	int blocksNeeded;
	FILE* fp = fopen(fname, "w+");

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

	memset(fs->superblock->data, 0, BLKSIZE);			// Zero-out the superblock
	memset(fs->free_block_bitmap->data, 0, BLKSIZE);		// Zero-out the free block

	// Write filesystem to disk
	for (i = 0; i < MAXBLOCKS; i++) {
		fseek(fp, BLKSIZE*i, SEEK_SET);

		if (i==0) fwrite(fs->superblock->data, sizeof(fs->superblock->data[0]), BLKSIZE, fp);
		else if (i==1) 	fwrite(fs->free_block_bitmap->data, sizeof(fs->free_block_bitmap->data[0]), BLKSIZE, fp);
		else {
			block* newBlock = (block*)malloc(sizeof(block));	// Make a new block
			memset(newBlock, 0, BLKSIZE);				// Zero it out

			sprintf(newBlock->data, "-block %d-", i);
			fputs(newBlock->data, fp);
		}
	}

	// Setup root directory entry
	root = (dirent*)malloc(sizeof(dirent));
	root_data = (dir_data*)malloc(sizeof(dir_data));

	strcpy(root->name, "/");
	root->ino.num = 1;
	root->ino.nlinks = 0;
	root->ino.mode = FS_DIR;
	root->ino.size = BLKSIZE;

	root_data->numDirs = 0;
	root_data->numFiles = 0;
	root_data->numLinks = 0;

	// How many free blocks do we need?
	blocksNeeded = sizeof(*root_data)/BLKSIZE + 1;		// + 1 because we allocate the ceiling
	
	memset(blocksAllocated, 0, MAXFILEBLOCKS);		// Zero out indices of allocated blocks

	for (i = 0; i < blocksNeeded; i++)			// Allocate free blocks
		blocksAllocated[i] = fs_allocate_block();

	// Fill in pointers to allocated blocks
	for (i = 0; i < NBLOCKS; i++)
	{
		if (i == blocksNeeded) break;			// Allocated all blocks needed
		root->ino.blocks[i] = 
			&fs->free_block_bitmap[blocksAllocated[i]];	// Put in the inode the address of the allocated block
	}
	
	if (i < blocksNeeded) {					// If we used up all the direct blocks
								// Start using indirect blocks
		for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
		{
			if (i == blocksNeeded) break;
			root->ino.ib1->blocks[j] = 
				&fs->free_block_bitmap[blocksAllocated[i]];
		}
	}

	if (i < blocksNeeded) {					// If we used up all the singly indirected blocks
								// Start using doubly indirected blocks
		for (k = 0; j < NIBLOCKS; k++)	
		{
			for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
			{
				if (i == blocksNeeded) break;
					root->ino.ib2->iblocks[k]->blocks[j] = 
						&fs->free_block_bitmap[blocksAllocated[i]];
			}
		}
	}

	if (i < blocksNeeded) {					// If we used up all the doubly indirected blocks
								// Start using triply indirected blocks
		for (l = 0; l < NIBLOCKS; l++)
		{
			for (k = 0; k < NIBLOCKS; k++)
			{
				for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
				{
					if (i == blocksNeeded) break;
						root->ino.ib3->iblocks[l]->iblocks[k]->blocks[j] = 
							&fs->free_block_bitmap[blocksAllocated[i]];
				}
			}
		}
	}

	if (i != blocksNeeded)
		printf("Warning: num blocks allocated (%d) != num blocks needed (%d)", i, blocksNeeded);

	// Write changed blocks to disk
	for (i = 0; i < blocksNeeded; i++)
	{
		fseek(fp, BLKSIZE*blocksAllocated[i], SEEK_SET); 
		fwrite(fs->blocks[blocksAllocated[i]], BLKSIZE, 1, fp);
	}

	// Allocate memory for all of this directory entry's direct blocks 
	for (i = 0; i < NBLOCKS; i++)
		root->ino.blocks[i] = (block*)malloc(sizeof(block));

	// Put the the root directory entry at the beginning 
	// of the filesystem superblock
	memcpy(fs->superblock->data, root, sizeof(*root));

	// Write the in-memory copy of the filesystem superblock to the disk
	fseek(fp, 0, SEEK_SET);
	fwrite(fs->superblock->data, sizeof(fs->superblock->data[0]), BLKSIZE, fp);

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", root->name, BLKSIZE*MAXBLOCKS/1024);
	
	return 0;
}