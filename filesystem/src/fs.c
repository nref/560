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
long last_inode = 0;

uint stride;				/* The data field is smaller than BLKSIZE
					 * so our writes to disk are not BLKSIZE but rather 
					 * BLKSIZE - sizeof(other fields in block struct) */
block_t  rootblocks[] = { 0, 1 };	/* Indices to the first and second blocks */
FILE* fp = NULL;

int fs_getblocksfromdisk(filesystem* fs, void* dest, block_t* blocks, uint numblocks, uint type);
int fs_writeblockstodisk(filesystem* fs, block_t* blocks, uint numblocks, uint type, void* data);
int fs_fill_inode_block_pointers(inode* ino, uint count, block_t* blockIndices);
int fs_allocate_blocks(filesystem* fs, int count, block_t* indices);
extern dentry_volatile* fs_dentry_volatile_from_dentry(filesystem* fs, dentry* d, uint existsOnDisk);
void fs_debug_print();
void fs_safeopen();
void fs_safeclose();

inode_t fs_alloc_inode();

/* Open a file and check for NULL */
void fs_safeopen(char* fname, char* mode) {
	fs_safeclose();	/* Close whatever was already open */

	fp = fopen(fname, mode);
	if (NULL == fp) {
		printf("fs_safeopen: \"%s\" %s\n", fname, strerror(errno));
		return;
	}
}

void fs_safeclose() {
	if (NULL != fp)
		fclose(fp);
}

/* Follow a an array of cstrings which are the fields of a path, e.g. input of 
 * { "bob", "dylan", "is", "old" } represents path "/bob/dylan/is/old"
 * Return the inode at the end of this path or ErrorInode if not found.
 */ 
inode* dir_recurse(filesystem* fs, dentry_volatile* dir, uint depth, uint pathFields, char* name, char* path[]) {
	uint i;									// Declarations go here to satisfy Visual C compiler
	dentry_volatile* iterator;

	if (NULL == dir || NULL == dir->head || NULL == dir->head->data_v.dir) return NULL;
	iterator = dir->head->data_v.dir;

	for (i = 0; i < dir->ndirs; i++)					// For each subdir at this level
	{
		if (NULL == iterator) return NULL;				// Return if we have iterated over all subdirs

		if (!strcmp(iterator->name, path[i])) {				// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return iterator->ino;				// Return the inode of the matching dir

			// Else recurse another level
			else return dir_recurse(fs, iterator, depth+1, 
						pathFields-1, name, &path[1]); 
		}
		iterator = iterator->next->data_v.dir;

	}
	return NULL;					// If nothing found, return a special error inode
}

/*
 * Return the inode of the directory at the path "name"
 */
inode* fs_stat(filesystem* fs, char* name) {
	uint i;
	uint depth = 0;					// The depth of the path, e.g. depth of "/a/b/c" is 3
	inode* ino = NULL;
	char* path[FS_MAXPATHFIELDS];
	char* next_field = NULL;
	char* name_cpy = (char*)malloc(strlen(name) + 1);
	const char* delimiter = "/";			// Path separator. Who doesn't use "/" ?

	if (NULL == fs) { return NULL; }		// No filesystem yet, bail!
	strcpy(name_cpy, name);				// Copy the path because strtok replaces delimieter with '\0'

	// Split the path on '/'
	next_field = strtok(name_cpy, delimiter);
	while (NULL != next_field) {
		path[depth] = (char*)malloc(strlen(next_field)+1);
		strcpy(path[depth], next_field);
		next_field = strtok(NULL, delimiter);
		depth++;
	}
	free(next_field);
	free(name_cpy);

	if (depth == 0) return fs->root->ino;	// Return if we are at the root
	else {
		ino = dir_recurse(fs, fs->root, 0, depth, name, &path[1]);	// Else traverse the path, get matching inode
		for (i = 0; i < depth; i++) free(path[i]);
		return ino;
	}
	return NULL;
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
	inode* ino = (inode*)malloc(sizeof(inode));
	block_t first_block_num;

	if (MAXBLOCKS < num) return NULL;	/* Sanity check */

	if (0 == fs->sb.inode_first_blocks[num])
		return NULL;	/* An inode of this number does not exist on disk */

	first_block_num = fs->sb.inode_first_blocks[num];
	fs_getblocksfromdisk(fs, ino, &first_block_num, fs->sb.inode_block_counts[num], sizeof(inode));

	return ino;
}

dentry_volatile* fs_dentry_volatile_from_dentry(filesystem* fs, dentry* d, uint existsOnDisk) {
	dentry_volatile* dv = NULL;
	dv = fs_new_dentry_volatile(false, d->name);

	if (MAXBLOCKS < d->ino) return NULL;		/* A little sanity check */
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
	char path[FS_MAXPATHLEN];

	inode* current_inode = fs_stat(fs, cur_dir_name);
	if (current_inode->num == 0) return FS_ERR;

	strcat(path, cur_dir_name);
	strcat(path, "/");
	strcat(path, dir_name);
	if (0 != fs_stat(fs, path)) return FS_ERR;	// dir already exists

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

/* A special version of mkdir that makes the root dir */
dentry_volatile* fs_mkdir_root(filesystem *fs) {
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

	fs->sb.ino	= dv->ino->num;

	return dv;
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
		if ((*eightBlocks)+1 < 0x07) {	// If this char is not full 
			(*eightBlocks)++;
			return fs->sb.free_blocks_base++;
		}
	}
	return FS_ERR;
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

	fs_safeopen(fname, "w");
	if (NULL == fp) return FS_ERR;		

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
    
	fs_safeclose();

	if (ERR < 0) { 
		perror("allocation error");
		return ERR;
	}

	return FS_OK;
}

/* Zero-out filesystem */
int fs_zero() {
	int i;
	fs_safeopen(fname, "r+");
	if (NULL == fp) return FS_ERR;

	for (i = 0; i < MAXBLOCKS; i++) {
		block* newBlock = fs_newBlock();
		char temp[128];
		sprintf(temp, "%d", i);
		memcpy(newBlock->data, temp, strlen(temp));
		
		fseek(fp, BLKSIZE*i, SEEK_SET);
		fputs(newBlock->data, fp);
		free(newBlock);
	}
	fs_safeclose();
	return FS_OK;
}

void fs_delete(filesystem *fs) {
	if (fs) {

		if (fs->root) {
			/* TODO: Traverse the whole directory structure and free */
			free(fs->root->ino);
			free(fs->root->files);
			free(fs->root->links);
			if (fs->root->head)
				free(fs->root->head);

			free(fs->root);

		}
		free(fs);
	}
}

filesystem* fs_alloc_filesystem(int newfs) {
	filesystem *fs = NULL;

	stride = sizeof(((struct block*)0)->data);
	if (0 == stride) return NULL;

	if (newfs) {
		int err1, err2;

		fs_safeclose();
		err1 = fs_preallocate();	/* Make a contiguous file on disk (performance enhancement) */
		err2 = fs_zero();		/* Zero-out all filesystem blocks */

		if (FS_ERR == err1 || FS_ERR == err2) return NULL;
	}

	fs_safeopen(fname, "r+");	/* Test if file exists */
	if (NULL == fp) return NULL;

	fs = (filesystem*)malloc(sizeof(filesystem));

	/* Zero-out fields */
	memset(&fs->fb_map, 0, sizeof(map));
	memset(&fs->sb_i.blocks, 0, SUPERBLOCK_MAXBLOCKS*sizeof(block_t));
	memset(&fs->sb.inode_first_blocks, 0, MAXBLOCKS*sizeof(block_t));
	memset(&fs->sb.inode_block_counts, 0, MAXBLOCKS*sizeof(uint));

	fs->sb.ino = 0;
	fs->sb.free_blocks_base = 2;					/* Start allocating from 3rd block. Block 0 is free map, Block 1 superblock_info */
	fs->sb_i.nblocks = sizeof(superblock)/stride + 1; 		/* How many free blocks needed */
	fs->fb_map.data[0] = 0x02;					/* First two blocks reserved */

	if (newfs) {
		last_inode = 0;
		fs_allocate_blocks(fs, fs->sb_i.nblocks, fs->sb_i.blocks);	/* Allocate n blocks, tell us which we got */
		fs->root = fs_mkdir_root(fs);					/* Setup root dir */
	}
	return fs;
}

/* Allocate @param count blocks if possible. Store indices in @param blocks */
int fs_allocate_blocks(filesystem* fs, int count, block_t* indices) {
	int i, j;

	for (i = 0; i < count; i++) {	// Allocate free blocks
		j = fs_allocate_block(fs);
		if (FS_ERR == j) return FS_ERR;
		indices[i] = j;
	}
	return FS_OK;
}

/* Fill in the direct blocks of an inode. Return the count of allocated blocks */
int fs_fill_direct_blocks(block_t* blocks, uint count, block_t* blockIndices, uint maxCount) {
	uint i;
	for (i = 0; i < maxCount; i++)
	{
		if (i == count) break;
		blocks[i] = blockIndices[i];
	}
	return i;
}

/* Fill @param count @param blocks into the block pointers of @param ino. Spill into 
 * indirect block pointers, doubly-indirected block pointers, and triply-indirected block 
 * pointers as needed. */
int fs_fill_inode_block_pointers(inode* ino, uint count, block_t* blockIndices) {
	uint alloc_cnt = 0, indirection = 0;
	uint i, j;

	while (alloc_cnt < count) {
		switch (indirection)
		{
			case DIRECT: alloc_cnt += fs_fill_direct_blocks(ino->blocks, count, blockIndices, NBLOCKS); break;

			// If we used up all the direct blocks, start using singly indirected blocks
			case INDIRECT1: alloc_cnt += fs_fill_direct_blocks(ino->ib1.blocks, count, blockIndices, NBLOCKS_IBLOCK); break;

			// If we used up all the singly indirected blocks, start using doubly indirected blocks
			case INDIRECT2: for (i = 0; i < NIBLOCKS; i++)	
					alloc_cnt += fs_fill_direct_blocks(	ino->ib2.iblocks[i].blocks, 
										count, blockIndices, NBLOCKS_IBLOCK); break;

			// If we used up all the doubly indirected blocks, start using triply indirected blocks
			case INDIRECT3: for (i = 0; i < NIBLOCKS; i++)
						for (j = 0; j < NIBLOCKS; j++)
							alloc_cnt += fs_fill_direct_blocks(	ino->ib3.iblocks[i].iblocks[j].blocks, 
												count, blockIndices, NBLOCKS_IBLOCK); break;
			default: return FS_ERR; /* Out of blocks */
		}
		++indirection;
	}

	return FS_OK;
}

/* Read a block from disk */
int fs_getblockfromdisk(void* dest, block_t b) {
	if (NULL == fp) return FS_ERR;

	fseek(fp, b*BLKSIZE, SEEK_SET);
	fread(dest, BLKSIZE, 1, fp);

	return FS_OK;
}

/* Write a block to disk */
int fs_writeblocktodisk(block_t b, uint size, void* data) {
	if (NULL == fp) return FS_ERR;

	fseek(fp, BLKSIZE*b, SEEK_SET); 
	fwrite(data, size, 1, fp);

	return FS_OK;
}

/* Read an arbitary number of blocks from disk. */
int fs_getblocksfromdisk(filesystem *fs, void* dest, block_t* blocks, uint numblocks, uint type_size) {
	uint i, memcpySize;
	block_t j = blocks[0];

	if (BLKSIZE != type_size) {

		for (i = 0; i < numblocks; i++) {					/* Get root blocks from disk */
			memcpySize = i+1 == numblocks ? type_size % stride : stride;	/* Get last chunk which may only be a partial block */

			if (FS_ERR == fs_getblockfromdisk(&fs->block_cache[j], j))
				return FS_ERR;

			if (MAXBLOCKS < j) return FS_ERR;
			memcpy(&((char*)dest)[i*memcpySize], &fs->block_cache[j].data, memcpySize);

			j = fs->block_cache[j].next;
			if (0 == j) break;
		}
	} else if (FS_ERR == fs_getblockfromdisk(dest, j))
		return FS_ERR;

	return FS_OK;
}

/* Write an arbitrary number of blocks to disk */
int fs_writeblockstodisk(filesystem* fs, block_t* blocks, uint numblocks, uint type_size, void* data) {
	block_t j = blocks[0];

	// Split @param data into strides if it will not fit in one block
	if (BLKSIZE != type_size) {
		uint i, copysize;
		
		for (i = 0; i < numblocks; i++) {						// For all blocks
			fs->block_cache[j].num = blocks[i];						// Record index of this block

			if (i+1 == numblocks) {							// If this is the last block
				copysize =  type_size % stride;					//   1. Write the remaining chunk that is less than the full block size
				fs->block_cache[j].next = 0;					//   2 .It has no next block
			} else {								// This is not the last block, so:
				copysize = stride;						//   1. Copy the full stride
				j = fs->block_cache[j].next;					//   2. Get an index to the next block
				fs->block_cache[j].next = blocks[i+1];				//   3. Record the index of the next block	
			}
			memcpy(fs->block_cache[j].data, &((char*)data)[i*stride], copysize);
		}

		j = blocks[0];
		for (i = 0; i < numblocks; i++) {

			fs_writeblocktodisk(j, BLKSIZE, &fs->block_cache[j]);
			j = fs->block_cache[j].next;
			if (j == 0) break;	// Sanity check: Do not write inode 0
		}

	// Else write @param data to a whole block directly
	} else fs_writeblocktodisk(j, type_size, data);

	return FS_OK;
}

/* Open a filesystem stored on disk */
filesystem* fs_openfs() {
	filesystem* fs = NULL;
	inode* root_inode = NULL;
	uint k;
	block_t one = 1;

	fs = fs_alloc_filesystem(false);
	if (NULL == fs) return NULL;

	//fs_debug_print();

	//for (k = 0; k < sizeof(superblock); k++)
	//	printf("%x ", ((char*)&fs->sb)[k]);
	//printf("\n");

	fs_getblockfromdisk(&fs->fb_map, 0);
	fs_getblocksfromdisk(fs, &fs->sb_i, &one, 1, sizeof(superblock_i));
	fs_getblocksfromdisk(fs, &fs->sb, fs->sb_i.blocks, fs->sb_i.nblocks, sizeof(superblock));

	root_inode = (inode*)fs_get_inode_from_disk(fs, fs->sb.ino);					// Get the root directory inode from disk
	if (NULL == root_inode)	return NULL;								// Opening the filesystem file failed

	fs->root = fs_dentry_volatile_from_dentry(fs, &root_inode->data.dir, true);			// Convert the permanent storage directory to
	
	if (NULL == fs->root) return NULL;
	if (!strcmp(fs->root->name,"/")) return fs;							// We identify having the whole root by the last field,
													// i.e. root->name, being correctly filled (i.e., filled with "/")
	return NULL;
}

filesystem* fs_mkfs() {
	filesystem *fs = NULL;

	fs = fs_alloc_filesystem(true);
	if (NULL == fs) return NULL;
	
	fs_writeblockstodisk(fs, &rootblocks[0],		1,			sizeof(map),		&fs->fb_map);	/* Write map to disk */
	fs_writeblockstodisk(fs, &rootblocks[1],		1,			sizeof(superblock_i),	&fs->sb_i);	/* Write superblock info to disk */
	fs_writeblockstodisk(fs, fs->sb_i.blocks,		fs->sb_i.nblocks,	sizeof(superblock),	&fs->sb);	/* Write superblock to disk */
	fs_writeblockstodisk(fs, fs->root->ino->blocks,	fs->root->ino->nblocks,	sizeof(inode),		&fs->root->ino);/* Write root inode to disk. TODO: Need to write iblocks */

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", fs->root->name, BLKSIZE*MAXBLOCKS/1024);
	return fs;
}

void fs_debug_print() {
	printf("sizeof(map): %lu\n", sizeof(map));
	printf("sizeof(block): %lu\n", sizeof(block));
	printf("sizeof(iblock1): %lu\n", sizeof(iblock1));
	printf("sizeof(iblock2): %lu\n", sizeof(iblock2));
	printf("sizeof(iblock3): %lu\n", sizeof(iblock3));
	printf("sizeof(superblock): %lu\n", sizeof(superblock));
	printf("sizeof(superblock_i): %lu\n", sizeof(superblock_i));
	printf("sizeof(inode): %lu\n", sizeof(inode));
	printf("sizeof(dentry): %lu\n", sizeof(dentry));
	printf("sizeof(dentry_volatile): %lu\n", sizeof(dentry_volatile));
	printf("sizeof(filesystem): %lu\n", sizeof(filesystem));
}
