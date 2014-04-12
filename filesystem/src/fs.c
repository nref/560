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

void* fs_getblocksfromdisk(block_t*, uint, uint);
int fs_writeblockstodisk(block_t*, uint, uint, void*);
int fs_fill_inode_block_pointers(inode*, uint, block_t*);
int fs_allocate_blocks(filesystem*, int, block_t*);
extern dentry_volatile* fs_dentry_volatile_from_dentry(filesystem*, dentry*, uint);

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
	uint i;									// Declarations go here to satisfy Visual C compiler
	dentry_volatile *iterator = dir->head->data_v.dir;

	if (NULL == iterator) return ErrorInode();				// Dir has no subdirs!

	for (i = 0; i < dir->ndirs; i++)					// For each subdir at this level
	{
		if (NULL == iterator) return ErrorInode();			// Return if we have iterated over all subdirs

		if (!strcmp(iterator->name, path[i])) {				// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return iterator->ino;				// Return the inode of the matching dir

			// Else recurse another level
			else return dir_recurse(fs, iterator, depth+1, 
						pathFields-1, name, &path[1]); 
		}
		iterator = iterator->next->data_v.dir;

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

dentry* fs_new_dentry(int alloc_inode, char* name) {
	dentry* d;
	d = (dentry*)malloc(sizeof(dentry));

	if (alloc_inode)
		d->ino = fs_alloc_inode();

	d->parent	= d->ino;
	d->next		= d->ino;
	d->prev		= d->ino;
	d->head		= 0;
	d->tail		= 0;
	d->ndirs	= 0;
	d->nfiles	= 0;
	d->nlinks	= 0;

	memset(d->files, 0, sizeof(d->files));				// Zero-out
	memset(d->links, 0, sizeof(d->links));				// Zero-out

	strcpy(d->name, name);
	return d;
}

dentry_volatile* fs_new_dentry_volatile(int alloc_inode, char* name) {
	dentry_volatile* dv;

	dv		= (dentry_volatile*)	malloc(sizeof(dentry_volatile));
	dv->ino		= (inode*)		malloc(sizeof(inode));

	dv->files	= (inode*)		malloc(FS_MAXFILES*sizeof(inode));
	dv->links	= (inode*)		malloc(FS_MAXLINKS*sizeof(inode));

	memset(dv->files, 0, FS_MAXFILES*sizeof(inode));
	memset(dv->links, 0, FS_MAXLINKS*sizeof(inode));

	memset(dv->ino->blocks, 0, sizeof(block_t)*NBLOCKS);
	memset(&dv->ino->ib1, 0, sizeof(iblock1));
	memset(&dv->ino->ib2, 0, sizeof(iblock2));
	memset(&dv->ino->ib3, 0, sizeof(iblock3));

	dv->tail	= NULL;
	dv->head	= NULL;
	dv->parent	= NULL;
	dv->next	= NULL;
	dv->prev	= NULL;

	dv->nfiles	= 0;
	dv->ndirs	= 0;

	dv->ino->nlinks	= 0;
	dv->ino->nblocks= 1;
	dv->ino->size	= BLKSIZE;
	dv->ino->mode	= FS_DIR;

	strcpy(dv->name, name);

	if (alloc_inode)
		dv->ino->num = fs_alloc_inode();
	else dv->ino->num = 0;

	memcpy(&dv->ino->data.dir, fs_new_dentry(alloc_inode, name), sizeof(dv->ino->data.dir));
	dv->ino->data_v.dir = dv;

	return dv;
}

inode* fs_get_inode_from_disk(filesystem* fs, inode_t num) {
	inode* ino = NULL;
	block_t first_block_num;

	if (0 == fs->sb.inode_first_blocks[num])
		return NULL;	/* An inode of this number does not exist on disk */

	first_block_num = fs->sb.inode_first_blocks[num];
	ino = (inode*)fs_getblocksfromdisk(&first_block_num, fs->sb.inode_block_counts[num], INODE);

	return ino;
}

dentry_volatile* fs_dentry_volatile_from_dentry(filesystem* fs, dentry* d, uint existsOnDisk) {
	dentry_volatile* dv = NULL;
	dv = fs_new_dentry_volatile(false, d->name);

	/* If never seen this inode before, need to allocate blocks */
	if (0 == fs->sb.inode_block_counts[d->ino]) {

		fs_allocate_blocks(fs, dv->ino->nblocks, dv->ino->blocks);			/* Allocate n blocks, tell us which we got */
		fs_fill_inode_block_pointers(dv->ino, dv->ino->nblocks, dv->ino->blocks);	/* Fill in inode block pointers with pointers to the allocated blocks */

		fs->sb.inode_first_blocks[d->ino] = dv->ino->blocks[0];
		fs->sb.inode_block_counts[d->ino] = dv->ino->nblocks;
	}

	dv->ino->num		= d->ino;
	dv->ndirs		= d->ndirs;
	dv->nfiles		= d->ndirs;
	memcpy(&dv->ino->data.dir, d, sizeof(*d));

	if (existsOnDisk) {
		dv->head		= fs_get_inode_from_disk(fs, d->head);
		dv->tail		= fs_get_inode_from_disk(fs, d->tail);
		dv->parent		= fs_get_inode_from_disk(fs, d->parent);
		dv->next		= fs_get_inode_from_disk(fs, d->next);
		dv->prev		= fs_get_inode_from_disk(fs, d->prev);
	}
	else {
		dv->parent	= NULL;
		dv->next	= NULL;
		dv->prev	= NULL;
		dv->head	= NULL;
		dv->tail	= NULL;
	}

	return dv;
}

dentry_volatile* fs_new_dir_volatile(filesystem* fs, char* name) {
	dentry* new_dir;
	dentry_volatile* new_dir_v;
	new_dir = fs_new_dentry(true, name);
	new_dir_v = fs_dentry_volatile_from_dentry(fs, new_dir, false);
	free(new_dir);

	fs->sb.inode_first_blocks[new_dir_v->ino->num] = new_dir_v->ino->blocks[0];
	fs->sb.inode_block_counts[new_dir_v->ino->num] = new_dir_v->ino->nblocks;

	return new_dir_v;
}

int fs_mkdir(filesystem *fs, char* cur_dir_name, char* dir_name) {
	dentry_volatile* curr_dir_v = NULL, *new_dir_v = NULL;

	inode* current_inode = fs_stat(fs, cur_dir_name);
	if (current_inode->num == 0) return FS_ERR;

	/* If the inode is not loaded from disk into memory, load it from disk */
	if (NULL == &current_inode->data_v)	curr_dir_v = fs_dentry_volatile_from_dentry(fs, &current_inode->data.dir, true);
	else					curr_dir_v = current_inode->data_v.dir;

	new_dir_v = fs_new_dir_volatile(fs, dir_name);
	new_dir_v->parent = current_inode;

	/* If it's the first entry in cur_dir_name  */
	if (NULL == curr_dir_v->head) {
		curr_dir_v->head = new_dir_v->ino;
		new_dir_v->prev = new_dir_v->ino;
		new_dir_v->next = new_dir_v->ino;
		curr_dir_v->tail = curr_dir_v->head;
	} else {
		new_dir_v->prev = curr_dir_v->tail;
		curr_dir_v->tail->data_v.dir->next = new_dir_v->ino;
		curr_dir_v->tail = new_dir_v->ino;
		new_dir_v->next = curr_dir_v->head;
	}
	curr_dir_v->ndirs++;

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

/* Zero-out filesystem */
int fs_zero() {
	int i;
	FILE* fp = fs_safeopen(fname, "r+");
	if (FS_ERR == (int)fp) return FS_ERR;

	for (i = 0; i < MAXBLOCKS; i++) {
		block* newBlock = fs_newBlock();
		char temp[128];
		sprintf(temp, "%d", i);
		memcpy(newBlock->data, temp, strlen(temp));
		
		fseek(fp, BLKSIZE*i, SEEK_SET);
		fputs(newBlock->data, fp);
		free(newBlock);
	}
	fclose(fp);
	return FS_OK;
}

filesystem* fs_alloc_filesystem(int newfs) {
	filesystem *fs = NULL;
	fs = (filesystem*)malloc(sizeof(filesystem));

	if (newfs) {
		fs_preallocate();	/* Make a contiguous file on disk (performance enhancement) */
		fs_zero();		/* Zero-out all filesystem blocks */
	}
	return fs;
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

/* Read an arbitary number of blocks from disk, 
 * starting at block @param start
 * Need the parameter @param type because
 * the last block is not totally occupied,
 * and we need to know how many bytes to read from this last
 * block.
 */
void* fs_getblocksfromdisk(block_t* blocks, uint numblocks, uint type) {
	uint i;
	uint type_size = sizeof(block);
	block_t j = blocks[0];
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

int fs_writeblockstodisk(block_t* blocks, uint numblocks, uint type, void* data) {
	uint i;
	uint type_size = sizeof(block);
	block_t j = blocks[0];
	FILE* fp;
	char* buf;									/* Buffer for writing to disk */

	switch (type) {
		case BLOCK:		type_size = sizeof(block);		break;
		case MAP:		type_size = sizeof(map);		break;
		case SUPERBLOCK_I:	type_size = sizeof(superblock_i);	break;
		case SUPERBLOCK:	type_size = sizeof(superblock);		break;
		case INODE:		type_size = sizeof(inode);		break;
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
	filesystem* fs = NULL;
	inode* root_inode = NULL;
	dentry* root_dir = NULL;
	block_t  blocktmp[] = { 0, 1 };

	fs = fs_alloc_filesystem(false);

	memcpy(&fs->fb_map.data,	fs_getblocksfromdisk(&blocktmp[0], 1, MAP),		sizeof(fs->fb_map.data));	// Read the free block bitmap from disk into memory
	memcpy(&fs->sb_i,		fs_getblocksfromdisk(&blocktmp[1], 1, SUPERBLOCK_I),	sizeof(fs->sb_i));		// Read superblock_info stored on disk into memory
	memcpy(&fs->sb,			fs_getblocksfromdisk(fs->sb_i.blocks, 1, SUPERBLOCK),	sizeof(fs->sb));		// Read superblock stored on disk into memory

	root_inode		= (inode*)fs_get_inode_from_disk(fs, fs->sb.ino);						// Get the root directory inode from disk
	root_dir		= &root_inode->data.dir;
	fs->root		= fs_dentry_volatile_from_dentry(fs, root_dir, true);						// Convert the permanent storage directory to
																// An in-memory version, with pointers etc
	if (!strcmp(root_dir->name,"/")) return fs;		// We identify having the whole root by the last field,
								// i.e. root->name, being correctly filled (i.e., filled with "/")
	return NULL;
}

dentry_volatile* fs_make_disk_root(filesystem *fs) {
	dentry* d = NULL;
	dentry_volatile* dv = NULL;

	d = fs_new_dentry(true, "/");
	dv = fs_dentry_volatile_from_dentry(fs, d, false);	/* Convert to in-memory root dir */

	/* The above either read in bogus data (if there was no preexisting fs),
	 * or read in the root from the preexisting fs. Remove that junk. */
	if (dv->parent) free(dv->parent);
	if (dv->next) free(dv->next);
	if (dv->prev) free(dv->prev);
	if (dv->head) free(dv->head);
	if (dv->tail) free(dv->tail);

	dv->parent	= dv->ino;
	dv->next	= dv->ino;
	dv->prev	= dv->ino;
	dv->head	= NULL;
	dv->tail	= NULL;

	return dv;
}

filesystem* fs_mkfs() {
	filesystem *fs = NULL;
	block_t  blocktmp[] = { 0, 1 };

	stride = sizeof(((struct block*)0)->data);					/* block->data is smaller than block, so we have to stride */
	if (0 == stride) return NULL;

	fs = fs_alloc_filesystem(true);

	memset(&fs->fb_map, 0, sizeof(fs->fb_map));					/* Zero-out map */
	memset(&fs->sb.inode_first_blocks, 0, sizeof(fs->sb.inode_first_blocks));
	memset(&fs->sb.inode_block_counts, 0, sizeof(fs->sb.inode_block_counts));

	fs->sb_i.nblocks	= sizeof(superblock)/stride + 1; 			/* How many free blocks needed */
	fs->sb.free_blocks_base	= 2;							/* Start allocating from 2nd block. Block 0 is free map */
	
	fs_allocate_blocks(fs, fs->sb_i.nblocks, fs->sb_i.blocks);			/* Allocate n blocks, tell us which we got */
	fs_writeblockstodisk(&blocktmp[1], 1, SUPERBLOCK_I, &fs->sb_i);			/* Write superblock info to disk */

	fs->root = fs_make_disk_root(fs);						/* Setup root dir in permanent storage */

	fs_writeblockstodisk(&blocktmp[0], 1, MAP, &fs->fb_map);						/* Write map to disk */
	fs_writeblockstodisk(fs->root->ino->blocks, fs->root->ino->nblocks, INODE, &fs->root->ino);		/* Write root dir to disk. TODO: Need to write iblocks */

	fs->sb.ino = fs->root->ino->num;
	fs_writeblockstodisk(fs->sb_i.blocks, fs->sb_i.nblocks, SUPERBLOCK, &fs->sb);	/* Write superblock to disk */

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", fs->root->name, BLKSIZE*MAXBLOCKS/1024);
	return fs;
}
