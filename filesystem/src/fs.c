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
					 * e.g. freelist && superblock 
					 */
dirent* root;				/* Pointer to the root directory entry */
dir_data* root_data;			/* Pointer to root directory data */
int free_blocks_base;			/* Index of lowest free block */
FILE* fp;				/* Pointer to the file containing the filesystem */

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

block* fs_newBlock() {
	block* b = (block*)malloc(sizeof(block));
	memset(b->data, 0, BLKSIZE);	// Zero-out
	return b;
}

int fs_openfs() {
	FILE* fp = fopen(fname, "r");
	if (NULL == fp) return FS_ERR;

	if (root) free(root);
	if (root_data) free(root_data);
	if (NULL != fs) free(fs);
	if (NULL != fs->blocks) {
		if (NULL != fs->blocks[0]) free(fs->blocks[0]);
		if (NULL != fs->blocks[1]) free(fs->blocks[1]);
	}

	root = (dirent*)malloc(sizeof(dirent));
	root_data = (dir_data*)malloc(sizeof(dir_data));
	fs = (filesystem*)malloc(sizeof(filesystem));

	fs->blocks[0] = fs_newBlock();
	fs->blocks[1] = fs_newBlock();
	fs->superblock = fs->blocks[0];
	fs->free_block_bitmap = fs->blocks[1];

	fseek(fp, 0, SEEK_SET);						// Read the superblock
	fread(	fs->superblock->data,					// stored on disk back into memory
		sizeof(char), BLKSIZE, fp);

	fseek(fp, BLKSIZE, SEEK_SET);
	fread(	fs->free_block_bitmap->data,				// Also read the free block bitmap
		sizeof(char), BLKSIZE, fp);				// from disk into memory

	printf("%ld %ld %ld\n", sizeof(*root), sizeof(dirent), sizeof(fs->superblock->data));	// Copy into root struct
	memcpy(root, fs->superblock->data, sizeof(*root));

	fseek(fp, 2*BLKSIZE, SEEK_SET);					// Also read the 3rd block into root_data
	fread(root_data, sizeof(char), sizeof(*root_data), fp); 

	if (!strcmp(root->name,"/"))					// If the root is named "/"
		return FS_OK;
	return FS_ERR;
}

/* Traverse the free block array and return a block 
 * whose field num is the index of the first free bock */
block* fs_allocate_block() {
	int i;

	/* One char in the free block map represents 
	 * 8 blocks (char == 1 byte == 8 bits) 
	 */
	char* eightBlocks;
	block* newBlock;

	for (i = free_blocks_base; i < MAXBLOCKS; i++)
	{
		eightBlocks = &fs->free_block_bitmap->data[i];
		if (*eightBlocks < 0x07) { 
			free_blocks_base = i + *eightBlocks;
			++(*eightBlocks);

			newBlock = fs_newBlock();
			newBlock->num = free_blocks_base;
			return newBlock;
		}
	}
	return NULL;
}

int fs_free_block(block* blk) {
	if (fs->free_block_bitmap->data[blk->num] == 0x0)
		return FS_ERR;	/* Block already free */

	--fs->free_block_bitmap->data[blk->num];
	free_blocks_base = blk->num;
	return FS_OK;
}

/* Preallocate a contiguous file. Differs across platforms */
int fs_preallocate() {
	int ERR;
	FILE* fp = fopen(fname, "w");

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

int fs_mkfs() {
	int i;
	int numBlocks;				/* How many blocks to allocate */
	int blockIndex[MAXFILEBLOCKS];		/* Indices of allocated blocks */

	fs_preallocate();	/* Make a file on disk */
	free_blocks_base = 2;	/* Start allocating from 3rd block. 
				 * Blocks 0 and 1 are superblock and free map
				 */

	/* Make in-memory representation of filesystem. 
	 * This will be always be kept in synch with disk
	 */
	fs = (filesystem*)malloc(sizeof(filesystem));
	fs->blocks[0] = fs_newBlock();
	fs->blocks[1] = fs_newBlock();

	fs->superblock		= fs->blocks[0];
	fs->free_block_bitmap	= fs->blocks[1];

	// Setup root directory entry
	root		= (dirent*)malloc(sizeof(dirent));
	root_data	= (dir_data*)malloc(sizeof(dir_data));

	strcpy(root->name, "/");
	root->ino.num		= 1;
	root->ino.nlinks	= 0;
	root->ino.mode		= FS_DIR;
	root->ino.size		= BLKSIZE;

	root_data->numDirs	= 0;
	root_data->numFiles	= 0;
	root_data->numLinks	= 0;

	/* How many free blocks do we need?
	 * + 1 because we allocate the ceiling in integer arithmetic
	 */
	numBlocks = sizeof(*root_data)/BLKSIZE + 1;
	fs_allocate_blocks(numBlocks, blockIndex);	// Allocate all blocks needed
	fs_fill_inode_block_pointers(&root->ino, numBlocks, blockIndex);

	// Put the the root directory entry at the beginning 
	// of the filesystem superblock
	memcpy(fs->superblock->data, root, sizeof(*root));

	// Write filesystem to disk
	fp = fopen(fname, "w+");
	if (NULL == fp) return FS_ERR;

	fwrite(fs->blocks[0]->data, sizeof(char), BLKSIZE, fp);		// superblock
	fwrite(fs->blocks[1]->data, sizeof(char), BLKSIZE, fp);		// free block map

	// Zero-out filesystem
	for (i = free_blocks_base; i < MAXBLOCKS; i++) {
		block* newBlock = fs_newBlock();
		sprintf(newBlock->data, "-block %d-", i);
		
		fseek(fp, BLKSIZE*i, SEEK_SET);
		fputs(newBlock->data, fp);
		free(newBlock);
	}

	// Allocate blocks in-memory for root_data
	for (i = 0; i < numBlocks; i++)
		fs->blocks[blockIndex[i]] = fs_newBlock();

	// Write root_data to filesystem blocks
	for (i = 0; i < numBlocks-1; i++)
		memcpy(fs->blocks[blockIndex[i]]->data, root_data, BLKSIZE);

	// Remainder to write (which does not occupy a full block)
	memcpy(fs->blocks[blockIndex[i]]->data, root_data, sizeof(*root_data) % BLKSIZE);

	// Write changed filesystem blocks to disk
	for (i = 0; i < numBlocks; i++)
	{
		fseek(fp, BLKSIZE*blockIndex[i], SEEK_SET); 
		fwrite(fs->blocks[blockIndex[i]]->data, BLKSIZE, 1, fp);
	}

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", root->name, BLKSIZE*MAXBLOCKS/1024);
	
	fclose(fp);
	return 0;
}