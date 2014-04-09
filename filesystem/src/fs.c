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

char* fname = "fs";			/* The name our filesystem will have on disk */
filesystem* fs;				/* Pointer to the in-memory representation of the fs, 
					 * e.g. freelist && superblock && root
					 */
FILE* fp;				/* Pointer to the file containing the filesystem */


/* Open a file and check for NULL */
FILE* fs_safeopen(char* fname, char* mode) {
	FILE* fp = fopen(fname, mode);
	if (NULL == fp) 
		return (FILE*)FS_ERR;
}

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
inode* dir_recurse(dentry* dir, int depth, int pathFields, char* name, char* path[]) {
	int i;									// Declaration here to satisfy Visual C compiler

	for (i = 0; i < dir->ndirs; i++)					// For each subdir at this level
	{
		if (!strcmp(dir->subdirs[i].name, path[i])) {			// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return &dir->subdirs[i].ino;			// Return the inode of the matching dir

			// Else recurse another level
			else return dir_recurse(&dir->subdirs[i],
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
	if (NULL == &fs->root) { return NULL; }	// No filesystem yet, bail!

	// Split the path on '/'
	next_field = strtok(name_cpy, delimiter);
	while (NULL != next_field) {
		path[depth] = (char*)malloc(strlen(next_field)*sizeof(char));
		strcpy(path[depth], next_field);
		next_field = strtok(NULL, delimiter);
		depth++;
	}

	if (depth == 0) return &(fs->root->ino);	// Return if we are at the root
	else return dir_recurse(fs->root, 0, depth,
				name, &path[1]);	// Else traverse the path, get matching inode

	return ErrorInode();
}

int fs_mkdentry(char* currentdir, char* dirname) {
	inode* current_inode = fs_stat(currentdir);
	if (current_inode->num == 0) return FS_ERR;

	// TODO: Get dentry, dir_data using current_inode

	return FS_ERR;
}

block* fs_newBlock() {
	block* b = (block*)malloc(sizeof(block));
	memset(b->data, 0, BLKSIZE);	// Zero-out
	return b;
}

/* Traverse the free block array and return a block 
 * whose field num is the index of the first free bock */
block* fs_allocate_block() {
	int i;

	/* One char in the free block map represents 
	 * 8 blocks (sizeof(char) == 1 byte == 8 bits) 
	 */
	char* eightBlocks;
	block* newBlock;

	for (i = fs->free_blocks_base; i < MAXBLOCKS; i++)
	{
		eightBlocks = &fs->free_block_bitmap->data[i];
		if (*eightBlocks < 0x07) {		// If this char is not full 
			fs->free_blocks_base = i + *eightBlocks;
			++(*eightBlocks);

			newBlock = fs_newBlock();
			newBlock->num = fs->free_blocks_base;
			return newBlock;
		}
	}
	return NULL;
}

int fs_free_block(block* blk) {
	if (fs->free_block_bitmap->data[blk->num] == 0x0)
		return FS_ERR;	/* Block already free */

	--fs->free_block_bitmap->data[blk->num];
	fs->free_blocks_base = blk->num;
	return FS_OK;
}

/* Preallocate a contiguous file. Differs across platforms */
int fs_preallocate() {
	int ERR;
	FILE* fp = fs_safeopen(fname, "w");
	//if (FS_ERR == (int)fp) return FS_ERR;		// Breaks Visual C compiler

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
    
	fclose(fp);

	if (ERR < 0) { 
		perror("allocation error");
		return ERR;
	}

	return ERR;
}

/* Allocate @param numBlocks blocks if possible 
 * Store indices in @param blocks
 */
int fs_allocate_blocks(int numBlocks, int* blocks) {
	int i;
	memset(blocks, 0, MAXFILEBLOCKS);	// Zero out indices of allocated blocks

	for (i = 0; i < numBlocks; i++)	{	// Allocate free blocks
		block* newBlock = fs_allocate_block();
		blocks[i] = newBlock->num;
		free(newBlock);
	}

	return FS_OK;
}

/* Fill @param numBlocks from @param blocks into the inode @param ino
 * block indices. Spill into Indirect block pointers, 
 * doubly-indirected block pointers, and triply-indirected block 
 * pointers as needed.
 */
int fs_fill_inode_block_pointers(inode* ino, int numBlocks, int* blocks) {
	int i,j,k,l;

	// Fill in indices to allocated blocks
	for (i = 0; i < NBLOCKS; i++)
	{
		if (i == numBlocks) break;					
			ino->blocks[i] = blocks[i];		// Put in the inode the address of the allocated block
	}
	
	if (i < numBlocks) {					// If we used up all the direct blocks
								// Start using indirect blocks
		for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
		{
			if (i == numBlocks) break;
				ino->ib1->blocks[j] = blocks[i];
		}
	}

	if (i < numBlocks) {					// If we used up all the singly indirected blocks
								// Start using doubly indirected blocks
		for (k = 0; j < NIBLOCKS; k++)	
		{
			for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
			{
				if (i == numBlocks) break;
					ino->ib2->iblocks[k]->blocks[j] = blocks[i];
			}
		}
	}

	if (i < numBlocks) {					// If we used up all the doubly indirected blocks
								// Start using triply indirected blocks
		for (l = 0; l < NIBLOCKS; l++)
		{
			for (k = 0; k < NIBLOCKS; k++)
			{
				for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
				{
					if (i == numBlocks) break;
						ino->ib3->iblocks[l]->iblocks[k]->blocks[j] = blocks[i];
				}
			}
		}
	}

	if (i != numBlocks)
		printf("Warning: num blocks allocated (%d) != num blocks needed (%d)", i, numBlocks);
	return FS_OK;
}

/* Zero-out filesystem */
int fs_zero() {
	int i;
	fp = fs_safeopen(fname, "w+");
	if (FS_ERR == (int)fp) return FS_ERR;

	for (i = 0; i < MAXBLOCKS; i++) {
		block* newBlock = fs_newBlock();
		sprintf(newBlock->data, "-block %d-", i);
		
		fseek(fp, BLKSIZE*i, SEEK_SET);
		fputs(newBlock->data, fp);
		free(newBlock);
	}
	fclose(fp);
	return FS_OK;
}

/* Open a filesystem stored on disk */
int fs_openfs() {
	int i;
	int numRootBlocks;				/* Number of blocks the root dir occupies */
	FILE* fp;
	int rootBlockIndices[FS_ROOTMAXBLOCKS];		/* Assume root never occupies more than FS_ROOTMAXBLOCKS blocks */

	if (NULL != fs) {
		if (fs->root) free(fs->root);
		free(fs);
	}
	if (NULL != fs->blocks) {
		for (i = 0; i < MAXBLOCKS; i++)
			if (NULL != fs->blocks[i]) free(fs->blocks[i]);
	}

	fs		= (filesystem*)	malloc(sizeof(filesystem));
	fs->root	= (dentry*)	malloc(sizeof(dentry));

	fs->blocks[0] = fs_newBlock();
	fs->blocks[1] = fs_newBlock();

	fs->superblock = fs->blocks[0];
	fs->free_block_bitmap = fs->blocks[1];

	fp = fs_safeopen(fname, "r");
	if (FS_ERR == (int)fp) return FS_ERR;
	
	fseek(fp, 0, SEEK_SET);						// Read the superblock
	fread(	fs->superblock->data,					// stored on disk back into memory
		sizeof(char), BLKSIZE, fp);

	fseek(fp, BLKSIZE, SEEK_SET);
	fread(	fs->free_block_bitmap->data,				// Also read the free block bitmap
		sizeof(char), BLKSIZE, fp);				// from disk into memory

	memcpy(rootBlockIndices, fs->superblock->data, FS_ROOTMAXBLOCKS*sizeof(int));			// Get root block indices
	memcpy(&numRootBlocks, &fs->superblock->data[FS_ROOTMAXBLOCKS*sizeof(int)+1], sizeof(int));	// Get num root blocks

	/* Get full root blocks from disk */
	for (i = 0; i < numRootBlocks; i++)
	{
		int memcpySize = BLKSIZE;
		int freadSize = BLKSIZE;

		fs->blocks[rootBlockIndices[i]] = (block*) malloc(sizeof(block));	// Allocate in-memory block
		memset(fs->blocks[rootBlockIndices[i]], 0, sizeof(block));		// Zero out block

		if (i+1 == numRootBlocks)				/* If this is the last block */
		{							/* Get last chunk which is only a partial block */
			memcpySize = sizeof(*fs->root);
			freadSize = memcpySize % BLKSIZE;	
		}
		fseek(fp, rootBlockIndices[i]*BLKSIZE, SEEK_SET);
		fread(fs->blocks[rootBlockIndices[i]]->data, sizeof(char), freadSize, fp);
		memcpy(&fs->root[i],fs->blocks[rootBlockIndices[i]]->data, memcpySize);
	}

	fclose(fp);
	if (!strcmp(fs->root->name,"/"))				// If the root is named "/"
		return FS_OK;

	return FS_ERR;
}

int fs_mkfs() {
	int i;
	int blockIndex[MAXFILEBLOCKS];			/* Indices of allocated blocks */

	fs_preallocate();				/* Make a file on disk */
	fs_zero();					/* Zero-out all filesystem blocks */


	fs = (filesystem*)malloc(sizeof(filesystem));	/* Make in-memory representation of filesystem. 
							 * This will be always be kept in synch with disk
							 */
	fs->blocks[0] = fs_newBlock();
	fs->blocks[1] = fs_newBlock();

	fs->superblock		= fs->blocks[0];
	fs->free_block_bitmap	= fs->blocks[1];

	fs->root = (dentry*)malloc(sizeof(dentry));	/* Setup root directory entry */

	fs->free_blocks_base = 2;			/* Start allocating from 3rd block. 
							 * Blocks 0 and 1 are superblock and free map
							 */

	strcpy(fs->root->name, "/");
	fs->root->ino.num		= 1;
	fs->root->ino.nlinks		= 0;
	fs->root->ndirs			= 0;
	fs->root->nfiles		= 0;
	fs->root->ino.mode		= FS_DIR;
	fs->root->ino.nblocks		= sizeof(*fs->root)/BLKSIZE + 1; 	/* How many free blocks do we need?
										 * + 1 because we allocate the ceiling 
										 * in integer arithmetic
										 */
	fs->root->ino.size		= fs->root->ino.nblocks*BLKSIZE;
	fs->root->ino.owner.dir_o	= fs->root;

	fs->root->subdirs	= (dentry*)	malloc(FS_MAXDIRS*sizeof(dentry));
	fs->root->files		= (file*)	malloc(FS_MAXDIRS*sizeof(file));
	fs->root->links		= (link*)	malloc(FS_MAXDIRS*sizeof(link));

	fs_allocate_blocks(fs->root->ino.nblocks, blockIndex);	/* Allocate n blocks */
	
	/* Fill in inode block pointers 
	 * with pointers to the allocated blocks
	 */
	fs_fill_inode_block_pointers(&fs->root->ino, fs->root->ino.nblocks, blockIndex);

	/* Put indices to root directory entry at the beginning 
	 * of the filesystem superblock
	 */ 
	memcpy(fs->superblock->data, blockIndex, fs->root->ino.nblocks*sizeof(int));

	// Allocate in-memory blocks for root dir
	for (i = 0; i < fs->root->ino.nblocks; i++)
		fs->blocks[blockIndex[i]] = fs_newBlock();

	// Store root dir in regular blocks
	for (i = 0; i < fs->root->ino.nblocks-1; i++)
		memcpy(fs->blocks[blockIndex[i]]->data, fs->root, BLKSIZE);
	// Remainder to write (final piece which does not occupy a full block)
	memcpy(fs->blocks[blockIndex[i]]->data, fs->root, sizeof(*fs->root) % BLKSIZE);

	// Write root directory blocks to disk
	fp = fs_safeopen(fname, "w+");
	if (FS_ERR == (int)fp) return FS_ERR;

	for (i = 0; i < fs->root->ino.nblocks; i++)
	{
		fseek(fp, BLKSIZE*blockIndex[i], SEEK_SET); 
		fwrite(fs->blocks[blockIndex[i]]->data, BLKSIZE, 1, fp);
	}

	// Write the number of blocks to the superblock.
	memcpy(&fs->superblock->data[FS_ROOTMAXBLOCKS*sizeof(int)+1], &fs->root->ino.nblocks, sizeof(int));

	fseek(fp, 0, SEEK_SET);
	fwrite(fs->blocks[0]->data, sizeof(char), BLKSIZE, fp);		// superblock
	fseek(fp, BLKSIZE, SEEK_SET);
	fwrite(fs->blocks[1]->data, sizeof(char), BLKSIZE, fp);		// free block map

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", fs->root->name, BLKSIZE*MAXBLOCKS/1024);
	
	fclose(fp);
	return FS_OK;
}