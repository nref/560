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
block* block_cache[MAXBLOCKS];		// In-memory copies of blocks on disk


/* Open a file and check for NULL */
FILE* fs_safeopen(char* fname, char* mode) {
	FILE* fp = fopen(fname, mode);
	if (NULL == fp) 
		return (FILE*)FS_ERR;
	return fp;
}

/* 
 * Return an inode the fields of which indicate a problem
 */
inode* ErrorInode() {
	inode* ret = (inode*)malloc(sizeof(inode));
	ret->mode = FS_ERR;

	return ret;
}

/* Follow a an array of cstrings which are the fields of a path, e.g. input of 
 * { "bob", "dylan", "is", "old" } represents path "/bob/dylan/is/old"
 * Return the inode at the end of this path or ErrorInode if not found.
 */ 
inode* dir_recurse(dentry* dir, uint depth, uint pathFields, char* name, char* path[]) {
	uint i;									// Declaration here to satisfy Visual C compiler
	dentry *iterator = dir->head;
	if (NULL == iterator) return ErrorInode();				// Dir has no subdirs!

	for (i = 0; i < dir->ndirs; i++)					// For each subdir at this level
	{
		if (!strcmp(iterator->name, path[i])) {				// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return &iterator->ino;				// Return the inode of the matching dir

			// Else recurse another level
			else return dir_recurse(iterator, depth+1, 
						pathFields-1, name, &path[1]); 
		}
		iterator = iterator->next;
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
	if (NULL == &fs->sb->root) { return NULL; }	// No filesystem yet, bail!

	// Split the path on '/'
	next_field = strtok(name_cpy, delimiter);
	while (NULL != next_field) {
		path[depth] = (char*)malloc(strlen(next_field)*sizeof(char));
		strcpy(path[depth], next_field);
		next_field = strtok(NULL, delimiter);
		depth++;
	}

	if (depth == 0) return &(fs->sb->root);	// Return if we are at the root
	else return dir_recurse(fs->sb->root.owner.dir_o, 0, depth,
				name, &path[1]);	// Else traverse the path, get matching inode

	return ErrorInode();
}

int fs_mkdir(char* cur_dir_name, char* dir_name) {
	dentry* curr_dir, *new_dir;
	inode* current_inode = fs_stat(cur_dir_name);
	if (current_inode->num == 0) return FS_ERR;

	curr_dir = current_inode->owner.dir_o;
	curr_dir->ndirs++;				// TODO: Fix segfault after mkdir and then reload fs
	new_dir = (dentry*) malloc(sizeof(dentry));	// TODO: Replace with constructor

	// TODO: Store in blocks
	strcpy(new_dir->name, dir_name);
	new_dir->tail = NULL;
	new_dir->head = NULL;
	new_dir->nfiles = 0;
	new_dir->ndirs = 0;
	new_dir->ino.nlinks = 0;
	new_dir->ino.num = 2;				// TODO: Write inode allocator
	new_dir->ino.size = BLKSIZE;
	new_dir->ino.nblocks = 1;
	new_dir->files = NULL;
	new_dir->links = NULL;

	/* First entry TODO: Try this in fewer LOC */
	if (NULL == curr_dir->head) {
		curr_dir->head = new_dir;
		new_dir->prev = new_dir;
		new_dir->next = new_dir;
		curr_dir->tail = curr_dir->head;
	} else {
		curr_dir->tail->next = new_dir;
		new_dir->prev = curr_dir->tail;
		curr_dir->tail = new_dir;
		new_dir->next = curr_dir->head;
	}
	return FS_OK;
}

block* fs_newBlock() {
	block* b = (block*)malloc(sizeof(block));
	memset(b->data, 0, sizeof(b->data));	// Zero-out
	b->next = 0;
	b->num = 0;

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

	for (i = fs->sb->free_blocks_base; i < MAXBLOCKS; i++)
	{
		eightBlocks = &fs->free_block_bitmap->data[i/8];
		if (*eightBlocks < 0x07) {		// If this char is not full 
			fs->sb->free_blocks_base = i + *eightBlocks;
			++(*eightBlocks);

			newBlock = fs_newBlock();
			newBlock->num = fs->sb->free_blocks_base;
			return newBlock;
		}
	}
	return NULL;
}

int fs_free_block(block* blk) {
	if (fs->free_block_bitmap->data[blk->num] == 0x0)
		return FS_ERR;	/* Block already free */

	--fs->free_block_bitmap->data[blk->num];
	fs->sb->free_blocks_base = blk->num;
	return FS_OK;
}

/* Preallocate a contiguous file. Differs across platforms */
int fs_preallocate() {
	int ERR;
#if defined(_WIN64) || defined(_WIN32)	// Have to put declaration here.
	LARGE_INTEGER offset;		// because Visual C compiler is OLD school
#endif

	FILE* fp = fs_safeopen(fname, "w");
	if (FS_ERR == (uint)fp) return FS_ERR;		

#if defined(_WIN64) || defined(_WIN32)
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
		ino->blocks[i] = blocks[i];		// Put in the inode the index of the allocated block
	}
	
	if (i < numBlocks) {					// If we used up all the direct blocks
								// Start using indirect blocks
		for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
		{
			if (i == numBlocks) break;
			ino->ib1.blocks[j] = blocks[i];
		}
	}

	if (i < numBlocks) {					// If we used up all the singly indirected blocks
								// Start using doubly indirected blocks
		for (k = 0; j < NIBLOCKS; k++)	
		{
			for (j = 0; j < NBLOCKS_IBLOCK; j++, i++)	
			{
				if (i == numBlocks) break;
				ino->ib2.iblocks[k].blocks[j] = blocks[i];
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
					ino->ib3.iblocks[l].iblocks[k].blocks[j] = blocks[i];
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
	int i, j, numRootBlocks;
	FILE* fp;
	dentry *root;

	fp = fs_safeopen(fname, "r");
	if (NULL == fp) return FS_ERR;

	if (NULL != fs) {
		if (NULL != &fs->sb->root) free(&fs->sb->root);
		free(fs);
	}
	if (NULL != block_cache) {
		for (i = 0; i < MAXBLOCKS; i++)
			if (NULL != block_cache[i]) free(block_cache[i]);
	}

	fs			= (filesystem*)	malloc(sizeof(filesystem));
	fs->sb			= (superblock*)	malloc(sizeof(superblock));	/* Will be stored in block 0 */
	fs->sb->root.owner.dir_o= (dentry*)	malloc(sizeof(dentry));
	fs->free_block_bitmap	= fs_newBlock();

	root = fs->sb->root.owner.dir_o;

	fseek(fp, 0, SEEK_SET);							// Read the superblock
	fread(fs->sb, sizeof(superblock), 1, fp);				// stored on disk back into memory
		
	fseek(fp, BLKSIZE, SEEK_SET);
	fread(	fs->free_block_bitmap->data,					// Also read the free block bitmap
		sizeof(char), BLKSIZE, fp);					// from disk into memory

	numRootBlocks = fs->sb->root_last_block - fs->sb->root_first_block;

	/* Get full root blocks from disk */
	j = fs->sb->root_first_block;
	for (i = 0; i < numRootBlocks; i++)
	{
		int memcpySize = BLKSIZE;
		int freadSize = BLKSIZE;

		block_cache[j] = fs_newBlock();

		if (i+1 == numRootBlocks)					/* If this is the last block */
		{								/* Get last chunk which is only a partial block */
			memcpySize = sizeof(*root);
			freadSize = memcpySize % BLKSIZE;	
		}

		fseek(fp, j*BLKSIZE, SEEK_SET);
		fread(block_cache[j], 1, BLKSIZE, fp);
		memcpy(&root[i], block_cache[j]->data, memcpySize);

		j = block_cache[j]->next;
	}

	fclose(fp);
	if (!strcmp(root->name,"/"))					// If the root is named "/"
		return FS_OK;

	return FS_ERR;
}

int fs_mkfs() {
	int i, j, first_block, nBlocks, chunk_size;
	int blockIndex[MAXFILEBLOCKS];						/* Indices of allocated blocks */
	dentry *root;

	fs_preallocate();							/* Make a file on disk */
	fs_zero();								/* Zero-out all filesystem blocks */


	fs = (filesystem*)malloc(sizeof(filesystem));				/* Make in-memory representation of filesystem. 
										 * This will be always be kept in synch with disk
										 */

	block_cache[1]		= fs_newBlock();
	fs->free_block_bitmap	= block_cache[1];


	fs->sb			= (superblock*)malloc(sizeof(superblock));	/* Will be stored in block 0 */
	fs->sb->numBlocks	= 0;
	fs->sb->next		= 0;
	
	fs->sb->root.owner.dir_o	= (dentry*)malloc(sizeof(dentry));	/* Setup root directory entry */
	fs->sb->free_blocks_base	= 2;					/* Start allocating from 3rd block. 
										 * Blocks 0 and 1 are superblock and free map
										 */
	root = fs->sb->root.owner.dir_o;
	root->ndirs			= 0;
	root->nfiles			= 0;
	strcpy(root->name, "/");

	fs->sb->root.num		= 1;					/* TODO: Write inode allocator and use it here */
	fs->sb->root.nlinks		= 0;
	fs->sb->root.mode		= FS_DIR;
	fs->sb->root.nblocks		= sizeof(*root)/BLKSIZE + 1; 		/* How many free blocks do we need?
										 * + 1 because we allocate the ceiling 
										 * in integer arithmetic
										 */
	fs->sb->root.size		= fs->sb->root.nblocks*BLKSIZE;

	root->files			= (file*)	malloc(FS_MAXDIRS*sizeof(file));
	root->links			= (link*)	malloc(FS_MAXDIRS*sizeof(link));

	root->head			= NULL;					// First added subdir
	root->tail			= NULL;					// Lastly added subdir
	root->parent			= root;					// Parent of root is root
	root->next			= root;					// root has no parent; next goes to self
	root->prev			= root;					// root has no parent; prev go to self

	nBlocks = fs->sb->root.nblocks;
	fs->sb->numBlocks += nBlocks;						// Superblock remembers # allocated

	fs_allocate_blocks(nBlocks, blockIndex);				/* Allocate n blocks, tell us which */
	
	/* Fill in inode block pointers 
	 * with pointers to the allocated blocks
	 */
	fs_fill_inode_block_pointers(&fs->sb->root, nBlocks, blockIndex);

	/* Put index of first block of root at the beginning 
	 * of the filesystem superblock
	 */ 
	fs->sb->root_first_block	= blockIndex[0];
	fs->sb->root_last_block		= fs->sb->root_first_block + nBlocks;

	chunk_size = sizeof(block_cache[0]->data);		// The data field is smaller than BLKSIZE
	first_block = blockIndex[0];				// Index of the first allocated block
	j = first_block;

	// Store root in fs
	for (i = 0; i < nBlocks-1; i++) {			// For all blocks except the last

		block_cache[j]		= fs_newBlock();	// Allocate in-memory block
		block_cache[j]->num	= blockIndex[i];	// Record index of this block
		block_cache[j]->next	= blockIndex[i+1];	// Record the index of the next block	

		memcpy(block_cache[j]->data, &root[i*chunk_size], chunk_size);
		j = block_cache[j]->next;
	}
	
	block_cache[j] = fs_newBlock();				// Write the last block separately 
	block_cache[j]->num = blockIndex[i];			// because it might not occupy a full block
	block_cache[j]->next = 0;				// There is no next block for this guy
	memcpy(block_cache[j]->data, &root[i*chunk_size], sizeof(*root) % chunk_size);

	// Write blocks to disk
	fp = fs_safeopen(fname, "w+");
	if (FS_ERR == (int)fp) return FS_ERR;

	j = first_block;
	for (i = 0; i < nBlocks; i++)
	{
		fseek(fp, BLKSIZE*j, SEEK_SET); 
		fwrite(block_cache[j], BLKSIZE, 1, fp);
		j = block_cache[j]->next;
	}

	fseek(fp, 0, SEEK_SET);
	fwrite(fs->sb, sizeof(fs->sb), 1, fp);
	fseek(fp, BLKSIZE, SEEK_SET);
	fwrite(fs->free_block_bitmap, sizeof(fs->free_block_bitmap), 1, fp);

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", root->name, BLKSIZE*MAXBLOCKS/1024);
	
	// TODO: Clear block cache here

	fclose(fp);
	return FS_OK;
}