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
inode* dir_recurse(dentry_volatile* dir, uint depth, uint pathFields, char* name, char* path[]) {
	uint i;									// Declaration here to satisfy Visual C compiler
	dentry_volatile *iterator = dir->head;
	if (NULL == iterator) return ErrorInode();				// Dir has no subdirs!

	for (i = 0; i < dir->ndirs; i++)					// For each subdir at this level
	{
		if (!strcmp(iterator->name, path[i])) {				// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return iterator->ino;				// Return the inode of the matching dir

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
inode* fs_stat(filesystem* fs, char* name) {

	char* path[FS_MAXPATHFIELDS];
	char* next_field = NULL;
	char* name_cpy = (char*)malloc(strlen(name)*sizeof(char));
	const char* delimiter = "/";			// Path separator. Who doesn't use "/" ?
	int depth = 0;					// The depth of the path, e.g. depth of "/a/b/c" is 3

	strcpy(name_cpy, name);				// Copy the path because strtok replaces delimieter with '\0'
	if (NULL == &fs->sb.ino) { return NULL; }	// No filesystem yet, bail!

	// Split the path on '/'
	next_field = strtok(name_cpy, delimiter);
	while (NULL != next_field) {
		path[depth] = (char*)malloc(strlen(next_field)*sizeof(char));
		strcpy(path[depth], next_field);
		next_field = strtok(NULL, delimiter);
		depth++;
	}

	if (depth == 0) return fs->sb.root->ino;	// Return if we are at the root
	else return dir_recurse(fs->sb.root, 0, depth,
				name, &path[1]);	// Else traverse the path, get matching inode

	return ErrorInode();
}

int fs_mkdir(filesystem *fs, char* cur_dir_name, char* dir_name) {
	dentry* curr_dir = NULL, *new_dir = NULL;
	dentry_volatile* curr_dir_v = NULL, *new_dir_v = NULL;

	inode* current_inode = fs_stat(fs, cur_dir_name);
	if (current_inode->num == 0) return FS_ERR;

	curr_dir = &current_inode->owner.dir_o;
	curr_dir_v = NULL;

	new_dir = (dentry*)malloc(sizeof(dentry));			// TODO: Replace with constructor
	new_dir_v = (dentry_volatile*)malloc(sizeof(dentry_volatile));	// TODO: Replace with constructor
	new_dir_v->ino = (inode*) malloc(sizeof(inode));		// TODO: Write inode allocator

	strcpy(new_dir_v->name, dir_name);
	new_dir_v->tail = NULL;
	new_dir_v->head = NULL;
	new_dir_v->nfiles = 0;
	new_dir_v->ndirs = 0;
	new_dir_v->ino->nlinks = 0;
	new_dir_v->ino->num = 2;				
	new_dir_v->ino->size = BLKSIZE;
	new_dir_v->ino->nblocks = 1;
	new_dir_v->files = NULL;
	new_dir_v->links = NULL;

	/* If it's the first entry in cur_dir_name  */
	if (NULL == curr_dir_v->head) {
		curr_dir_v->head = new_dir_v;
		new_dir_v->prev = new_dir_v;
		new_dir_v->next = new_dir_v;
		curr_dir_v->tail = curr_dir_v->head;
	} else {
		curr_dir_v->tail->next = new_dir_v;
		new_dir_v->prev = curr_dir_v->tail;
		curr_dir_v->tail = new_dir_v;
		new_dir_v->next = curr_dir_v->head;
	}
	curr_dir->ndirs++;

	/* TODO: Write dentry to disk */


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
int fs_allocate_block(filesystem* fs) {
	int i;

	/* One char in the free block map represents 
	 * 8 blocks (sizeof(char) == 1 byte == 8 bits) 
	 */
	char* eightBlocks;

	for (i = fs->sb.free_blocks_base; i < MAXBLOCKS; i++)
	{
		eightBlocks = &fs->map.data[i/8];
		if (*eightBlocks < 0x07) {		// If this char is not full 
			fs->sb.free_blocks_base = i + *eightBlocks;
			++(*eightBlocks);

			return fs->sb.free_blocks_base;
		}
	}
	return -1;
}

int fs_free_block(filesystem* fs, block* blk) {
	if (fs->map.data[blk->num] == 0x0)
		return FS_ERR;	/* Block already free */

	--fs->map.data[blk->num];
	fs->sb.free_blocks_base = blk->num;
	return FS_OK;
}

/* Preallocate a contiguous file. Differs across platforms 
 * Note: this does NOT set the file to the given size. The OS
 * attempts to "preallocate" the contiguousspace in its filesystem blocks,
 * but the file will not appear to be of that size. 
 * Therefore, this offers merely a performance benefit.
 */
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

/* Allocate @param count blocks if possible 
 * Store indices in @param blocks
 */
int fs_allocate_blocks(filesystem* fs, int count, int indices[]) {
	int i;

	for (i = 0; i < count; i++)	// Allocate free blocks
		indices[i] = fs_allocate_block(fs);

	return FS_OK;
}

/* Return the count of allocated blocks */
int fs_fill_direct_blocks(block_t* blocks, int count, int* blockIndices, uint maxCount) {
	uint i;
	for (i = 0; i < maxCount; i++)
	{
		if (i == count) break;
		blocks[i] = blockIndices[i];
	}
	return i;
}

/* Fill @param count from @param blocks into the inode @param ino
 * block indices. Spill into Indirect block pointers, 
 * doubly-indirected block pointers, and triply-indirected block 
 * pointers as needed.
 */
int fs_fill_inode_block_pointers(inode* ino, int count, int* blockIndices) {
	int alloc_cnt, indirectionLevel, i, j;
	alloc_cnt = 0;
	indirectionLevel = 0;

	while (alloc_cnt < count) {
		switch (indirectionLevel)
		{
			case 0: alloc_cnt += fs_fill_direct_blocks(ino->blocks, count, blockIndices, NBLOCKS); break;

			// If we used up all the direct blocks, start using singly indirected blocks
			case 1: alloc_cnt += fs_fill_direct_blocks(ino->ib1.blocks, count, blockIndices, NBLOCKS_IBLOCK); break;

			// If we used up all the singly indirected blocks, start using doubly indirected blocks
			case 2: for (i = 0; i < NIBLOCKS; i++)	
					alloc_cnt += fs_fill_direct_blocks(	ino->ib2.iblocks[i].blocks, 
										count, blockIndices, NBLOCKS_IBLOCK); break;

			// If we used up all the doubly indirected blocks, start using triply indirected blocks
			case 3: for (i = 0; i < NIBLOCKS; i++)
					for (j = 0; j < NIBLOCKS; j++)
						alloc_cnt += fs_fill_direct_blocks(	ino->ib3.iblocks[i].iblocks[j].blocks, 
											count, blockIndices, NBLOCKS_IBLOCK); break;
			case 4: return FS_ERR;	/* Out of blocks */
			default: if (FS_ERR == alloc_cnt) return FS_ERR;
		}
		indirectionLevel++;
	}

	return FS_OK;
}

/* Zero-out filesystem */
int fs_zero() {
	int i;
	FILE* fp = fs_safeopen(fname, "r+");
	if (FS_ERR == (int)fp) return FS_ERR;

	for (i = 0; i < MAXBLOCKS; i++) {
		block* newBlock = fs_newBlock();
		char temp[128];
		sprintf(temp, "-block %d-", i);
		memcpy(newBlock->data, temp, strlen(temp));
		
		fseek(fp, BLKSIZE*i, SEEK_SET);
		fputs(newBlock->data, fp);
		free(newBlock);
	}
	fclose(fp);
	return FS_OK;
}

/* Read an arbitary number of blocks from disk, 
 * starting at block @param start
 * Need the parameter @param type because
 * the last block is not totally occupied,
 * and we need to know how many bytes to read from this last
 * block.
 */
void* fs_getblocksfromdisk(int start, int numblocks, int type) {
	int i;
	int j = start;
	int type_size;
	char* buf;

	FILE* fp = fs_safeopen(fname, "r");

	switch (type) {
		case BLOCK:	type_size = sizeof(block);	break;
		case DENTRY:	type_size = sizeof(dentry);	break;
		case INODE:	type_size = sizeof(inode);	break;
	}

	buf = (char*)malloc(type_size);

	for (i = 0; i < numblocks; i++)					/* Get root blocks from disk */
	{
		int memcpySize = BLKSIZE;
		int freadSize = BLKSIZE;

		block_cache[j] = fs_newBlock();

		if (i+1 == numblocks)					/* If this is the last block */
		{							/* Get last chunk which may only be a partial block */
			memcpySize = type_size;				/* For block type, this is not necessary */
			freadSize = memcpySize % BLKSIZE;	
		}

		fseek(fp, j*BLKSIZE, SEEK_SET);
		fread(block_cache[j], 1, BLKSIZE, fp);

		memcpy(buf, block_cache[j]->data, memcpySize);
		j = block_cache[j]->next;
	}

	return (void*)buf;
}

/* Open a filesystem stored fson disk */
filesystem* fs_openfs() {
	int first_block, block_count;
	FILE* fp;
	filesystem* fs = NULL;
	dentry* root = NULL;
	dentry_volatile* root_v = NULL;

	fp = fs_safeopen(fname, "r");
	if (NULL == fp) 
		return FS_ERR;

	fs			= (filesystem*)malloc(sizeof(filesystem));
	fs->sb.root		= (dentry_volatile*)malloc(sizeof(dentry_volatile));
	fs->sb.root->ino	= (inode*)malloc(sizeof(inode));
	
	root_v			= fs->sb.root;
	root			= &root_v->ino->owner.dir_o;

	fseek(fp, 0, SEEK_SET);							// Read the superblock
	fread(&fs->sb, sizeof(superblock), 1, fp);				// stored on disk back into memory
		
	fseek(fp, BLKSIZE, SEEK_SET);
	fread(	fs->map.data,							// Also read the free block bitmap
		sizeof(char), BLKSIZE, fp);					// from disk into memory

	strcpy(root->name, "!");
	if (!strcmp(root->name,"/"))						// We can return the root now if all of it fits in the superblock
		return fs;							// We identify having the whole root by the last field, name,
										// being correctly filled (i.e., filled with "/")

	/* If program flow reaches here, the root didn't fit in the superblock.
	 * Get all of the blocks from disk.
	 */
	first_block = fs->sb.root_first_block;
	block_count = fs->sb.root_last_block - fs->sb.root_first_block + 1;
	root = (dentry*)fs_getblocksfromdisk(first_block, block_count, DENTRY);

	fclose(fp);
	if (!strcmp(root->name,"/"))						
		return fs;

	return NULL;
}

filesystem* fs_mkfs() {
	uint i, j, stride, root_num_blks;
	int* indices = NULL;							/* Indices of allocated blocks */
	filesystem *fs = NULL;
	dentry *root = NULL;
	FILE* fp;

	fs_preallocate();							/* Make a file on disk */
	fs_zero();								/* Zero-out all filesystem blocks */

	fs			= (filesystem*)malloc(sizeof(filesystem));	/* Make in-memory representation of filesystem. 
										 * This will be always be kept in synch with disk
										 */
	fs->sb.root		= (dentry_volatile*)malloc(sizeof(dentry_volatile));
	fs->sb.root->ino	= (inode*)malloc(sizeof(inode));

	stride			= sizeof(fs->map.data);				// The data field is smaller than BLKSIZE
	memset(fs->map.data, 0, BLKSIZE);					// Zero out block
	
	fs->sb.fs_blk_alloc_cnt	= 0;
	fs->sb.free_blocks_base	= 2;						/* Start allocating from 3rd block. 
										 * Blocks 0 and 1 are superblock and free map
										 */
	fs->sb.ino			= 1;
	fs->sb.root->ino->num		= fs->sb.ino;				/* TODO: Write inode allocator and use it here */
	fs->sb.root->ino->nlinks	= 0;
	fs->sb.root->ino->mode		= FS_DIR;
	fs->sb.root->ino->		nblocks	= sizeof(fs->sb)/BLKSIZE + 1; 	/* How many free blocks do we need?
										 * + 1 because we allocate the ceiling 
										 * in integer arithmetic
										 */
	fs->sb.root->ino->size		= fs->sb.root->ino->nblocks*BLKSIZE;

	// Setup on-disk root dir
	root				= &fs->sb.root->ino->owner.dir_o;
	root->ino = fs->sb.ino;
	strcpy(root->name, "/");
	root->ndirs = 0;
	root->nfiles = 0;
	root->head = 0;
	root->tail = 0;
	root->parent = root->ino;
	root->next = root->ino;
	root->prev = root->ino;

	// Setup volatile (in-memory) root dir
	strcpy(fs->sb.root->name, root->name);
	fs->sb.root->ndirs			= root->ndirs;
	fs->sb.root->nfiles			= root->ndirs;
	fs->sb.root->files			= (file*)malloc(FS_MAXDIRS*sizeof(file));
	fs->sb.root->links			= (link*)malloc(FS_MAXDIRS*sizeof(link));
	fs->sb.root->head			= NULL;					// First added subdir
	fs->sb.root->tail			= NULL;					// Lastly added subdir
	fs->sb.root->parent			= fs->sb.root;				// Parent of root is root
	fs->sb.root->next			= fs->sb.root;				// root has no parent; next goes to self
	fs->sb.root->prev			= fs->sb.root;				// root has no parent; prev go to self

	root_num_blks = fs->sb.root->ino->nblocks;
	fs->sb.fs_blk_alloc_cnt += root_num_blks;					// Superblock remembers # allocated blocks

	indices = (int*)malloc(root_num_blks*sizeof(indices));
	fs_allocate_blocks(fs, root_num_blks, indices);					/* Allocate n blocks, tell us which we got */
	
	fs_fill_inode_block_pointers(fs->sb.root->ino, root_num_blks, indices);		/* Fill in inode block pointers
											 * with pointers to the allocated blocks
											 */
	free(indices);

	fs->sb.root_first_block	= fs->sb.root->ino->blocks[0];
	fs->sb.root_last_block	= fs->sb.root->ino->blocks[fs->sb.root->ino->nblocks-1]; 
	
	// Store root in fs
	j = fs->sb.root_first_block;	
	for (i = 0; i < root_num_blks-1; i++) {						// For all blocks except the last

		block_cache[j]		= fs_newBlock();				// Allocate in-memory block
		block_cache[j]->num	= fs->sb.root->ino->blocks[i];			// Record index of this block
		block_cache[j]->next	= fs->sb.root->ino->blocks[i+1];		// Record the index of the next block	

		memcpy(block_cache[j]->data, &root[i*stride], stride);
		j = block_cache[j]->next;
	}
	
	block_cache[j] = fs_newBlock();							// Write the last block separately 
	block_cache[j]->num = fs->sb.root->ino->blocks[i];				// because it might not occupy a full block
	block_cache[j]->next = 0;							// There is no next block for this last block
	printf("%d %d %d\n", sizeof(*root), sizeof(root), sizeof(*root) % stride);
	memcpy(block_cache[j]->data, &root[i*stride], sizeof(*root) % stride);

	// Write blocks to disk
	fp = fs_safeopen(fname, "r+");
	if (FS_ERR == (int)fp) return FS_ERR;

	j = fs->sb.root_first_block;
	for (i = 0; i < root_num_blks; i++)
	{
		fseek(fp, BLKSIZE*j, SEEK_SET); 
		fwrite(block_cache[j], BLKSIZE, 1, fp);
		j = block_cache[j]->next;
	}

	fseek(fp, 0, SEEK_SET);
	fwrite(&fs->sb, sizeof(fs->sb), 1, fp);
	fseek(fp, BLKSIZE, SEEK_SET);
	fwrite(&fs->map, sizeof(fs->map), 1, fp);

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", root->name, BLKSIZE*MAXBLOCKS/1024);
	
	fclose(fp);
	return fs;
}