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
long last_inode = 0;

uint stride;				/* The data field is smaller than BLKSIZE
					 * so our writes to disk are not BLKSIZE but rather 
					 * BLKSIZE - sizeof(other fields in block struct) */

void* fs_getblocksfromdisk(block_t*, int, int);
inode* ErrorInode();
inode_t fs_alloc_inode();

/* Open a file and check for NULL */
FILE* fs_safeopen(char* fname, char* mode) {
	FILE* fp = fopen(fname, mode);
	if (NULL == fp) 
		return (FILE*)FS_ERR;
	return fp;
}

/* Follow a an array of cstrings which are the fields of a path, e.g. input of 
 * { "bob", "dylan", "is", "old" } represents path "/bob/dylan/is/old"
 * Return the inode at the end of this path or ErrorInode if not found.
 */ 
inode* dir_recurse(filesystem* fs, dentry_volatile* dir, uint depth, uint pathFields, char* name, char* path[]) {
	uint i;									// Declaration here to satisfy Visual C compiler
	dentry_volatile *iterator = fs_dentry_volatile_from_dentry(fs, &dir->head->data.dir);
	if (NULL == iterator) return ErrorInode();				// Dir has no subdirs!

	for (i = 0; i < dir->ndirs; i++)					// For each subdir at this level
	{
		if (!strcmp(iterator->name, path[i])) {				// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return iterator->ino;				// Return the inode of the matching dir

			// Else recurse another level
			else return dir_recurse(fs, iterator, depth+1, 
						pathFields-1, name, &path[1]); 
		}
		iterator = fs_dentry_volatile_from_dentry(fs, &iterator->next->data.dir);

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

	if (depth == 0) return fs->root->ino;	// Return if we are at the root
	else return dir_recurse(fs, fs->root, 0, depth,
				name, &path[1]);	// Else traverse the path, get matching inode

	return ErrorInode();
}

dentry* fs_new_dentry(int alloc_inode) {
	dentry* new_dir;
	new_dir = (dentry*)malloc(sizeof(dentry));
	if (alloc_inode)
		new_dir->ino = fs_alloc_inode();
	return new_dir;
}

dentry_volatile* fs_new_dentry_volatile(int alloc_inode) {
	dentry* new_dir;
	dentry_volatile* new_dir_v;

	new_dir_v		= (dentry_volatile*)	malloc(sizeof(dentry_volatile));
	new_dir_v->ino		= (inode*)		malloc(sizeof(inode));
	new_dir_v->files	= (inode*)		malloc(FS_MAXFILES*sizeof(inode));
	new_dir_v->links	= (inode*)		malloc(FS_MAXLINKS*sizeof(inode));

	new_dir_v->tail		= NULL;
	new_dir_v->head		= NULL;
	new_dir_v->parent	= NULL;
	new_dir_v->next		= NULL;
	new_dir_v->prev		= NULL;

	new_dir_v->nfiles	= 0;
	new_dir_v->ndirs	= 0;

	new_dir_v->ino->nlinks	= 0;
	new_dir_v->ino->nblocks	= 1;
	new_dir_v->ino->size	= BLKSIZE;
	new_dir_v->ino->mode	= FS_DIR;

	if (alloc_inode)
		new_dir_v->ino->num = fs_alloc_inode();
	else new_dir_v->ino->num = 0;

	new_dir_v->ino->data.dir = *fs_new_dentry(false);
	new_dir = &new_dir_v->ino->data.dir;
	new_dir->ino = new_dir_v->ino->num;
	strcpy(new_dir->name, new_dir_v->name);

	new_dir->ndirs		= 0;
	new_dir->nfiles		= 0;
	new_dir->head		= 0;
	new_dir->tail		= 0;

	return new_dir_v;
}

inode* fs_get_inode_from_disk(filesystem* fs, inode_t num) {
	inode* ino = NULL;
	
	block_t first_block_num = fs->sb.inode_first_blocks[num];
	ino = (inode*)fs_getblocksfromdisk(&first_block_num, fs->sb.inode_block_counts[num], INODE);

	return ino;
}

dentry_volatile* fs_dentry_volatile_from_dentry(filesystem* fs, dentry* d) {
	dentry_volatile* dv = NULL;
	dv = fs_new_dentry_volatile(false);

	strcpy(dv->name, d->name);
	dv->ino->num		= d->ino;
	dv->ndirs		= d->ndirs;
	dv->nfiles		= d->ndirs;
	dv->ino->data.dir	= *d;

	dv->head		= fs_get_inode_from_disk(fs, d->head);
	dv->tail		= fs_get_inode_from_disk(fs, d->tail);
	dv->parent		= fs_get_inode_from_disk(fs, d->parent);
	dv->next		= fs_get_inode_from_disk(fs, d->next);
	dv->prev		= fs_get_inode_from_disk(fs, d->prev);

	return dv;
}

dentry_volatile* fs_new_dir_volatile(filesystem* fs, char* name) {
	dentry* new_dir;
	dentry_volatile* new_dir_v;
	new_dir = fs_new_dentry(true);
	new_dir_v = fs_dentry_volatile_from_dentry(fs, new_dir);
	free(new_dir);
	strcpy(new_dir_v->name, name);

	fs->sb.inode_first_blocks[new_dir_v->ino->num] = new_dir_v->ino->blocks[0];
	fs->sb.inode_block_counts[new_dir_v->ino->num] = new_dir_v->ino->nblocks;

	return new_dir_v;
}

int fs_mkdir(filesystem *fs, char* cur_dir_name, char* dir_name) {
	dentry* curr_dir = NULL;
	dentry_volatile* curr_dir_v = NULL, *new_dir_v = NULL;

	inode* current_inode = fs_stat(fs, cur_dir_name);
	if (current_inode->num == 0) return FS_ERR;

	curr_dir = &current_inode->data.dir;
	curr_dir_v = fs_dentry_volatile_from_dentry(fs, curr_dir);

	new_dir_v = fs_new_dir_volatile(fs, dir_name);
	
	/* If it's the first entry in cur_dir_name  */
	if (NULL == curr_dir_v->head) {
		curr_dir_v->head = new_dir_v->ino;
		new_dir_v->prev = new_dir_v->ino;
		new_dir_v->next = new_dir_v->ino;
		curr_dir_v->tail = curr_dir_v->head;
	} else {
		curr_dir_v->tail->data.dir.next = new_dir_v->ino->num;
		new_dir_v->prev->data.dir.next = curr_dir_v->tail->num;
		curr_dir_v->tail = new_dir_v->ino;
		new_dir_v->next = curr_dir_v->head;
	}
	curr_dir->ndirs++;

	return FS_OK;
}

/* 
 * Return an inode the fields of which indicate a problem
 */
inode* ErrorInode() {
	inode* ret = (inode*)malloc(sizeof(inode));
	ret->mode = FS_ERR;

	return ret;
}

/* Quick and dirty. Later support deletion etc. */
inode_t fs_alloc_inode() {
	return ++last_inode;
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
		eightBlocks = &fs->fb_map.data[i/8];
		if ((*eightBlocks)++ < 0x07)		// If this char is not full 
			return ++fs->sb.free_blocks_base;
	}
	return -1;
}

int fs_free_block(filesystem* fs, block* blk) {
	if (fs->fb_map.data[blk->num] == 0x0)
		return FS_ERR;	/* Block already free */

	--fs->fb_map.data[blk->num];
	fs->sb.free_blocks_base = blk->num;
	return FS_OK;
}

filesystem* fs_alloc_filesystem(int newfs) {
	filesystem *fs = NULL;
	fs = (filesystem*)malloc(sizeof(filesystem));
	fs->root = fs_new_dentry_volatile(newfs);

	return fs;
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
int fs_allocate_blocks(filesystem* fs, int count, block_t* indices) {
	int i;

	for (i = 0; i < count; i++)	// Allocate free blocks
		indices[i] = fs_allocate_block(fs);

	return FS_OK;
}

/* Return the count of allocated blocks */
int fs_fill_direct_blocks(block_t* blocks, uint count, block_t* blockIndices, uint maxCount) {
	uint i;
	for (i = 0; i < maxCount; i++)
	{
		if (i == count) break;
		blocks[i] = blockIndices[i];
	}
	return i;
}

/* Fill @param count @param blocks into the block pointers of
 * @param ino. Spill into Indirect block pointers, 
 * doubly-indirected block pointers, and triply-indirected block 
 * pointers as needed.
 */
int fs_fill_inode_block_pointers(inode* ino, uint count, block_t* blockIndices) {
	uint alloc_cnt, indirectionLevel, i, j;
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
void* fs_getblocksfromdisk(block_t* blocks, int numblocks, int type) {
	int i;
	int j = blocks[0];
	int type_size = sizeof(block);
	char* buf;

	FILE* fp = fs_safeopen(fname, "r");

	switch (type) {
		case BLOCK:		type_size = sizeof(block);	break;
		case SUPERBLOCK:	type_size = sizeof(dentry);	break;
		case INODE:		type_size = sizeof(inode);	break;
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

		fseek(fp, blocks[j]*BLKSIZE, SEEK_SET);
		fread(block_cache[j], 1, freadSize, fp);
		memcpy(buf, block_cache[j]->data, memcpySize);
		j = block_cache[j]->next;
	}

	return (void*)buf;
}

int fs_writeblockstodisk(block_t* blocks, uint numblocks, int type, void* data) {
	uint i, j, type_size = sizeof(block);
	FILE* fp;
	char* buf;									/* Buffer for writing to disk */

	switch (type) {
		case BLOCK:		type_size = sizeof(block);	break;
		case MAP:		type_size = sizeof(map);	break;
		case SUPERBLOCK:	type_size = sizeof(superblock);	break;
		case INODE:		type_size = sizeof(inode);	break;
	}

	// Buffer blocks for writing
	buf = (char*)malloc(type_size);							// Copy root into a char array so we can copy
	memcpy(buf, data, type_size);							// at a granularity of 1 byte 

	j = blocks[0];	
	for (i = 0; i < numblocks-1; i++) {						// For all blocks except the last

		block_cache[j]		= fs_newBlock();				// Allocate in-memory block
		block_cache[j]->num	= blocks[i];					// Record index of this block
		block_cache[j]->next	= blocks[i+1];					// Record the index of the next block	

		memcpy(block_cache[j]->data, &buf[i*stride], stride);
		j = block_cache[j]->next;
	}
	
	block_cache[j] = fs_newBlock();							// Write the last block separately because it might not occupy a full block
	block_cache[j]->num = blocks[i];
	block_cache[j]->next = 0;							// There is no next block for this last block
	memcpy(block_cache[j]->data, &buf[i*stride], type_size % stride);

	// Write blocks to disk
	fp = fs_safeopen(fname, "r+");
	if (FS_ERR == (int)fp) return FS_ERR;

	j = blocks[0];
	for (i = 0; i < numblocks; i++)
	{
		fseek(fp, BLKSIZE*j, SEEK_SET); 
		fwrite(block_cache[j], BLKSIZE, 1, fp);
		j = block_cache[j]->next;
	}

	return FS_OK;
}

/* Open a filesystem stored fson disk */
filesystem* fs_openfs() {
	FILE* fp;
	filesystem* fs = NULL;
	inode* root_inode = NULL;
	dentry* root_dir = NULL;

	fs = fs_alloc_filesystem(false);

	fp = fs_safeopen(fname, "r");
	if (NULL == fp) return FS_ERR;

	fseek(fp, 0, SEEK_SET);
	fread(&fs->sb, sizeof(superblock), 1, fp);					// Read the superblock stored on disk into memory
		
	fseek(fp, BLKSIZE, SEEK_SET);
	fread(fs->fb_map.data, sizeof(char), BLKSIZE, fp);				// Also read the free block bitmap from disk into memory
							
	fclose(fp);

	root_inode		= (inode*)fs_getblocksfromdisk(fs->sb.blocks, fs->sb.nblocks, INODE);
	root_dir		= &root_inode->data.dir;
	fs->root		= fs_dentry_volatile_from_dentry(fs, root_dir);

	if (!strcmp(root_dir->name,"/")) return fs;					// We identify having the whole root by the last field,
											// i.e. root->name, being correctly filled (i.e., filled with "/")
	return NULL;
}

dentry* fs_make_disk_root(filesystem *fs) {
	dentry* root_dir = NULL;
	root_dir		= &fs->root->ino->data.dir;
	root_dir->parent	= root_dir->ino;
	root_dir->next		= root_dir->ino;
	root_dir->prev		= root_dir->ino;
	strcpy(root_dir->name, "/");

	return root_dir;
}

filesystem* fs_mkfs() {
	filesystem *fs = NULL;
	dentry* root_dir = NULL;
	block_t  block0;

	stride = sizeof(((struct block*)0)->data);					/* block->data is smaller than block, so we have to stride */
	if (0 == stride) return NULL;

	fs			= fs_alloc_filesystem(true);
	root_dir		= fs_make_disk_root(fs);				/* Setup root dir in permanent storage */

	memset(&fs->fb_map, 0, sizeof(fs->fb_map));					// Zero-out
	memset(root_dir->files, 0, sizeof(root_dir->files));				// Zero-out
	memset(root_dir->links, 0, sizeof(root_dir->links));				// Zero-out

	fs->root->ino->nblocks		= 1;
	fs->root->ino->size		= fs->root->ino->nblocks*BLKSIZE;
	strcpy(fs->root->name, root_dir->name);
	memcpy(&fs->sb.ino, fs->root->ino, sizeof(inode));

	fs->sb.nblocks	= sizeof(superblock)/stride + 1; 				/* How many free blocks needed */
	fs->sb.free_blocks_base	= 1;							/* Start allocating from 2nd block. Block 0 is free map */

	block0 = 0;
	fs_writeblockstodisk(&block0, 1, MAP, &fs->fb_map);				// Write map to disk
	
	fs_allocate_blocks(fs, fs->sb.nblocks, fs->sb.blocks);				/* Allocate n blocks, tell us which we got */
	fs_writeblockstodisk(fs->sb.blocks, fs->sb.nblocks, SUPERBLOCK, &fs->sb);	// Write superblock to disk

	fs_allocate_blocks(fs, fs->root->ino->nblocks, fs->root->ino->blocks);				/* Allocate n blocks, tell us which we got */
	fs_fill_inode_block_pointers(fs->root->ino, fs->root->ino->nblocks, fs->root->ino->blocks);	/* Fill in inode block pointers with pointers to the allocated blocks */
	
	fs_preallocate();								/* Make a file on disk */
	fs_zero();									/* Zero-out all filesystem blocks */

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", root_dir->name, BLKSIZE*MAXBLOCKS/1024);
	return fs;
}
