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

#ifndef min
#define min(a,b)	(((a) < (b)) ? (a) : (b))
#endif

/* http://stackoverflow.com/questions/5349896/print-a-struct-in-c */
#define PRINT_STRUCT(p)  print_mem((p), sizeof(*(p)))

char* errormsgs[5]		= {	"OK", "General Error", "Directory exists", 
					"stat() failed for the given path",
					"An inode was not found on disk"	 };

char* fname = "fs";			/* The name our filesystem will have on disk */
block block_cache[MAXBLOCKS];		/* In-memory copies of blocks on disk */
size_t stride;				/* The data field is smaller than BLKSIZE
					 * so our writes to disk are not BLKSIZE but rather 
					 * BLKSIZE - sizeof(other fields in block struct) */
block_t  rootblocks[] = { 0, 1, 2 };	/* Indices to the first and second blocks */
FILE* fp = NULL;			/* Pointer to file storage */

/* Open the filesystem file and check for NULL */
void fs_safeopen(char* fname, char* mode) {
	fs_safeclose();	/* Close whatever was already open */

	fp = fopen(fname, mode);
	if (NULL == fp) {
		printf("fs_safeopen: \"%s\" %s\n", fname, strerror(errno));
		return;
	}
}

/* Close the filesystem file if is was open */
void fs_safeclose() {
	if (NULL != fp)
		fclose(fp);
	fp = NULL;
}

/* Get the inode of a directory, link, or file, at the end of a path.
 * Follow an array of cstrings which are the fields of a path, e.g. input of 
 * { "bob", "dylan", "is", "old" } represents path "/bob/dylan/is/old"
 * 
 * @param dir The root of the tree to traverse.
 * @param depth The depth of the path
 * @param name ??
 * @param path The fields of the path
 * Return the inode at the end of this path or NULL if not found.
 */ 
inode* fs_recurse(filesystem* fs, dent_v* dir, uint depth, uint pathFields, char* name, char* path[]) {
	uint i;									// Declarations go here to satisfy Visual C compiler
	dent_v* iterator;

	if (NULL == dir || NULL == dir->head || NULL == dir->head->data_v.dir) return NULL;
	iterator = dir->head->data_v.dir;

	for (i = 0; i < dir->ndirs; i++)					// For each subdir at this level
	{
		if (NULL == iterator) return NULL;				// Return if we have iterated over all subdirs

		if (!strcmp(iterator->name, path[depth])) {			// If we have a matching directory name
			if (depth == pathFields)				// If we can't go any deeper
				return iterator->ino;				// Return the inode of the matching dir

			// Else recurse another level
			else return fs_recurse(fs, iterator, depth+1, 
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
	char name_cpy[FS_NAMEMAXLEN];
	const char* delimiter = "/";			// Path separator. Who doesn't use "/" ?

	if (NULL == fs) return NULL;			// No filesystem yet, bail!

	strncpy(name_cpy, name, min(FS_NAMEMAXLEN-1, strlen(name)+1));	// Copy the path because strtok replaces delimieter with '\0'
	name_cpy[FS_NAMEMAXLEN-1] = '\0';

	// Split the path on '/'
	// TODO Move into function "fs_tokenize" or such
	next_field = strtok(name_cpy, delimiter);
	while (NULL != next_field) {
		path[depth] = (char*)malloc(strlen(next_field)+1);
		strcpy(path[depth], next_field);
		next_field = strtok(NULL, delimiter);
		depth++;
	}
	free(next_field);

	if (depth == 0) return fs->root->ino;	// Return if we are at the root
	else {
		ino = fs_recurse(fs, fs->root, 0, depth, name, &path[depth-1]);	// Else traverse the path, get matching inode
		for (i = 0; i < depth; i++) 
			free(path[i]);
		return ino;
	}
	return NULL;
}

/* Createa and zero-out a new on-disk directory entry
 * @param alloc_inode specifies whether this directory
 * gets allocated an inode, else inode 0 is given.
 * @param name the new directory name.
 */
dent* fs_newd(filesystem* fs, const int alloc_inode, const char* name) {
	dent* d = NULL;
	d = (dent*)malloc(sizeof(dent));

	if (alloc_inode)
		d->ino = fs_ialloc(fs);
	else d->ino = 0;

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

	// Copy name
	strncpy(d->name, name, min(FS_NAMEMAXLEN-1, strlen(name)+1));
	d->name[FS_NAMEMAXLEN-1] = '\0';
	
	return d;
}

/* Create and zero-out an in-memory directory entry 
 * @param alloc_inode specifies whether this directory
 * gets allocated an inode, else inode 0 is given.
 * @param name the new directory name
 */
dent_v* fs_newdv(filesystem* fs, const int alloc_inode, const char* name) {
	dent*			d = NULL;
	dent_v*	dv = NULL;

	dv		= (dent_v*)	malloc(sizeof(dent_v));
	dv->ino		= (inode*)		malloc(sizeof(inode));

	dv->files	= (inode*)		malloc(FS_MAXFILES*sizeof(inode));
	dv->links	= (inode*)		malloc(FS_MAXLINKS*sizeof(inode));

	memset(	dv->files, 0, FS_MAXFILES*sizeof(inode));
	memset(	dv->links, 0, FS_MAXLINKS*sizeof(inode));

	memset(	dv->ino->blocks, 0, sizeof(block_t)*NBLOCKS);
	memset(	&dv->ino->ib1, 0, sizeof(iblock1));
	memset(	&dv->ino->ib2, 0, sizeof(iblock2));
	memset(	&dv->ino->ib3, 0, sizeof(iblock3));

	dv->tail	= NULL;
	dv->head	= NULL;
	dv->parent	= NULL;
	dv->next	= NULL;
	dv->prev	= NULL;

	dv->nfiles	= 0;
	dv->ndirs	= 0;
	dv->nlinks	= 0;
	dv->ino->nlinks	= 0;
	dv->ino->nblocks= sizeof(inode)/stride+1;   /* How many blocks the inode consumes */
	dv->ino->size	= BLKSIZE;
	dv->ino->mode	= FS_DIR;

	strncpy(dv->name, name, min(FS_NAMEMAXLEN-1, strlen(name)+1));
	dv->name[FS_NAMEMAXLEN-1] = '\0';

	d = fs_newd(fs, alloc_inode, name);
	memcpy(&dv->ino->data.dir, d, sizeof(dv->ino->data.dir));
	dv->ino->data_v.dir = dv;
	dv->ino->num = d->ino;

	free(d);
	return dv;
}

/* Read from disk the inode to which @param num refers. */
inode* fs_inode_load(filesystem* fs, inode_t num) {
	inode* ino = NULL;
	block_t first_block_num;

	if (NULL == fs) return NULL;
	if (MAXBLOCKS <= num) return NULL;	/* Sanity check */

	first_block_num = fs->sb.inode_first_blocks[num];

	if (0 == first_block_num)
		return NULL;			/* An inode of 0 does not exist on disk */

	ino = (inode*)malloc(sizeof(inode));
	if (FS_ERR == 
		fs_readblocksfromdisk(	ino, &first_block_num, 
					fs->sb.inode_block_counts[num], 
					sizeof(inode))	) {
		/* If the disk read failed */
		if (NULL != ino)
			free(ino);
		return NULL;
	}

	return ino;
}

/* Given an inode number, load the corresponding dent
 * Return a dent_v whose field data.dir contains it. */
dent_v* fs_load_dir(filesystem* fs, inode_t num) {
	dent_v *dv;

	inode* ino = fs_inode_load(fs, num);
	if (NULL == ino) return NULL;

	dv = fs_newdv(fs, false, ino->data.dir.name);
	
	if (NULL == dv) {
		free(ino);
		return NULL;
	}

	dv->ino		= ino;
	dv->head	= fs_inode_load(fs, dv->ino->data.dir.head);
	dv->tail	= fs_inode_load(fs, dv->ino->data.dir.tail);
	dv->parent	= fs_inode_load(fs, dv->ino->data.dir.parent);
	dv->next	= fs_inode_load(fs, dv->ino->data.dir.next);
	dv->prev	= fs_inode_load(fs, dv->ino->data.dir.prev);

	if (	NULL == dv->parent	|| 
		NULL == dv->next	|| 
		NULL == dv->prev	) {

		free(dv);
		return NULL;
	}

	dv->ndirs	= dv->ino->data.dir.ndirs;
	dv->nfiles	= dv->ino->data.dir.ndirs;
	dv->nlinks	= dv->ino->data.dir.nlinks;
	
	dv->ino->data_v.dir = dv;

	return dv;
}

dent_v* fs_new_dir(filesystem *fs, dent_v* parent, const char* name) {
	dent_v* dv = NULL;
	inode* tail = NULL;
	int makingRoot = 0;

	/* We're making the root */
	if (NULL == parent && !strcmp(name, "/")) 
		makingRoot = 1;

	/* Allocate a new inode number */
	dv = fs_newdv(fs, true, name);
	if (NULL == dv) return NULL;

	/* Allocate n blocks, tell us which we got */
	if (FS_ERR == 
		fs_balloc(	fs, dv->ino->nblocks, 
					dv->ino->blocks) ) 
	{
		free(dv);
		return NULL;
	}
		
	/* Fill in inode block indices with indices to the allocated blocks */
	if (FS_ERR == fs_fill_inode_block_pointers(	dv->ino, 
							dv->ino->nblocks,
							dv->ino->blocks ))
	{
		free(dv);
		return NULL;
	}

	fs->sb.inode_first_blocks[dv->ino->num] = dv->ino->blocks[0];
	fs->sb.inode_block_counts[dv->ino->num] = dv->ino->nblocks;
	if (makingRoot) fs->sb.root = dv->ino->num;	// TODO: Make sure this fix works

	if (!makingRoot) {

		/* Need to load the tail (the last dir added to @param parent)*/
		tail = parent->tail;	/* Try in memory */
		if (NULL == tail) {
			parent->ino->data_v.dir->tail = 
				fs_inode_load(fs, parent->ino->data.dir.tail); /* Try from disk */
			tail = parent->ino->data_v.dir->tail;
		}

		/* Setup linked list pointers */
		if (NULL == parent->head) { /* If it's the first entry here  */

			parent->ino->data.dir.head = dv->ino->data.dir.ino;
			dv->ino->data.dir.prev = dv->ino->data.dir.ino;
			dv->ino->data.dir.next = dv->ino->data.dir.ino;
			parent->ino->data.dir.tail = parent->ino->data.dir.head;

		} else {

			dv->ino->data.dir.prev = parent->ino->data.dir.tail;
			dv->ino->data.dir.next = parent->ino->data.dir.head;

			tail->data.dir.next = dv->ino->data.dir.ino;
			parent->ino->data.dir.tail = dv->ino->data.dir.ino;

			/* Update changes to tail */
			fs_writeblockstodisk(tail, tail->blocks, tail->nblocks, sizeof(inode));
		}

		parent->ndirs++;
		parent->ino->data.dir.ndirs++;

		dv->parent = parent->ino;
		dv->ino->data.dir.parent = parent->ino->num;

		/* Do the same setup for the in-memory version */
		if (NULL == parent->head) {
			parent->head = dv->ino;
			dv->prev = dv->ino;
			dv->next = dv->ino;
			parent->tail = parent->head;
		} else {
			dv->prev = parent->tail;
			dv->next = parent->head;
			parent->tail->data_v.dir->next = dv->ino;	/* Updating the tail in-memory requires no disk write */
			parent->tail = dv->ino;
		}

		/* Update change to parent */
		fs_writeblockstodisk(parent->ino, parent->ino->blocks, parent->ino->nblocks, sizeof(inode));

	} else {
		/* Making root */
	}

	/* Write changes to disk */
	fs_writeblockstodisk(dv->ino, dv->ino->blocks, dv->ino->nblocks, sizeof(inode));
	if (FS_ERR == fs_sync(fs))
		return NULL;

	return dv;
}

int fs_mkdir(filesystem *fs, char* cur_path, char* dir_name) {
	dent_v* cur_dv = NULL, *new_dv = NULL;
	char path[FS_MAXPATHLEN] = "";

	inode* current_inode;

	current_inode = fs_stat(fs, cur_path);
	if (NULL == current_inode)	return BADPATH;
	
	if (strlen(cur_path) + strlen(dir_name) + 1 >= FS_MAXPATHLEN)
		return BADPATH;

	strcat(path, cur_path);
	if (path[strlen(path)-1] != '/')
		strcat(path, "/");				// Append path separator if it's not already there
	strcat(path, dir_name);
	if (NULL != fs_stat(fs, path)) return DIREXISTS;	// The requested directory already exists

	// Get parent dir from memory or disk
	cur_dv = current_inode->data_v.dir;
	if (NULL == cur_dv)
		cur_dv = fs_load_dir(fs, current_inode->num);	
	if (NULL == cur_dv) return NOTONDISK;
	
	new_dv = fs_new_dir(fs, cur_dv, dir_name);
	if (NULL == new_dv) return FS_ERROR;

	return OK;
}

/* A special version of mkdir that makes the root dir
 * @param newfs if true, load a preexisting root dir
 * Else make a new root dir
 */
dent_v* fs_mkroot(filesystem *fs, int newfs) {
	dent_v* dv = NULL;
	
	if (newfs) {
		dv = fs_new_dir(fs, NULL, "/");
		if (NULL == dv) return NULL;

		dv->parent	= dv->ino;
		dv->next	= dv->ino;
		dv->prev	= dv->ino;

		dv->ino->data.dir.parent = dv->ino->num;
		dv->ino->data.dir.next = dv->ino->num;
		dv->ino->data.dir.prev = dv->ino->num;

		fs->sb.root = dv->ino->num;
	}
	else {
		dv = fs_load_dir(fs, fs->sb.root);
		if (NULL == dv) return NULL;
	}

	return dv;
}

/* Free the index of an inode and its malloc'd memory
 * @param blk pointer to the block to free
 * Returns FS_OK on success, FS_ERR if the block
 * 
 */
int fs_ifree(filesystem* fs, inode_t num) {
	if (NULL == fs) return FS_ERR;

	if (0x0 == fs->fb_map.data[num])
		return FS_ERR;	/* Block already free */

	--fs->ino_map.data[num];
	fs->sb.free_inodes_base = num;
	return FS_OK;
}

/* Find an unused inode number and return it */
inode_t fs_ialloc(filesystem *fs) {
	uint i, blockidx, blockval;

	for (i = fs->sb.free_inodes_base; i < MAXINODES; i++)
	{
		blockidx = i/255;	/* max int value of char */
		blockval = (int)fs->ino_map.data[blockidx];
		if (blockval < 255) {		// If this char is not full 
			((fs->ino_map).data)[blockidx]++;
			return fs->sb.free_inodes_base++;
		}
	}
	return 0;
}

/* Free the index of a block and its malloc'd memory
 * @param blk pointer to the block to free
 * Returns FS_OK on success, FS_ERR if the block
 * was already free
 */
int fs_bfree(filesystem* fs, block* blk) {
	if (NULL == fs) return FS_ERR;
	if (NULL == blk) return FS_ERR;

	if (0x0 == fs->fb_map.data[blk->num])
		return FS_ERR;	/* Block already free */

	--fs->fb_map.data[blk->num];
	fs->sb.free_blocks_base = blk->num;
	free(blk);
	return FS_OK;
}

/* Traverse the free block array and return a block 
 * whose field num is the index of the first free bock */
int _balloc(filesystem* fs) {
	uint i, blockidx, blockval;

	/* One char in the free block map represents 
	 * 8 blocks (sizeof(char) == 1 byte == 8 bits) */
	
	for (i = fs->sb.free_blocks_base; i < MAXBLOCKS; i++)
	{
		blockidx = i/255;	/* max int value of char */
		blockval = (int)fs->fb_map.data[blockidx];
		if (blockval < 255) {		// If this char is not full 
			((fs->fb_map).data)[blockidx]++;
			return fs->sb.free_blocks_base++;
		}
	}
	return FS_ERR;
}

/* malloc memory for a block, zero-out its fields
 * Returns the allocated block
 */
block* fs_newBlock() {
	block* b = (block*)malloc(sizeof(block));
	memset(b->data, 0, sizeof(b->data));	// Zero-out
	b->next = 0;
	b->num = 0;

	return b;
}

/* Preallocate a contiguous file. Differs across platforms 
 * Note: this does NOT set the file to the given size. The OS
 * attempts to "preallocate" the contiguousspace in its filesystem blocks,
 * but the file will not appear to be of that size. 
 * Therefore, this offers merely a performance benefit.
 * Returns FS_OK on success, FS_ERR on failure.
 */
int fs_prealloc() {
	int ERR;
#if defined(_WIN64) || defined(_WIN32)	// Have to put declaration here.
	LARGE_INTEGER offset;		// because Visual C compiler is OLD school
#endif

	fs_safeopen(fname, "wb");
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

/* Zero-out the on-disk file 
 * Returns FS_OK on success, FS_ERR on failure
 */
int fs_zero() {
	int i;
	fs_safeopen(fname, "rb+");
	if (NULL == fp) return FS_ERR;

	for (i = 0; i < MAXBLOCKS; i++) {
		block* newBlock = fs_newBlock();
		char temp[128];
		sprintf(temp, "%d", i);
		memcpy(newBlock->data, temp, strlen(temp));
		
		if (0 != fseek(fp, BLKSIZE*i, SEEK_SET))
			return FS_ERR;
		fputs(newBlock->data, fp);
		free(newBlock);
	}
	fs_safeclose();
	return FS_OK;
}

/* Free the memory occupied my a filesystem 
 * TODO: Traverse the whole directory tree and free
 */
void fs_delete(filesystem *fs) {
	if (fs) {

		if (fs->root) {
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

/* Create a new filesystem. @param newfs specifies if we 
 * open a file on disk or create a new one (overwriting
 * the previous)
 * Returns a pointer to the allocated filesystem
 */
filesystem* fs_init(int newfs) {
	filesystem *fs = NULL;

	stride = sizeof(((struct block*)0)->data);
	if (0 == stride) return NULL;

	if (newfs) {
		int err1, err2;

		fs_safeclose();
		err1 = fs_prealloc();		/* Make a contiguous file on disk (performance enhancement) */
		err2 = fs_zero();		/* Zero-out all filesystem blocks */

		if (FS_ERR == err1 || FS_ERR == err2) return NULL;
	}

	fs_safeopen(fname, "rb+");	/* Test if file exists */
	if (NULL == fp) return NULL;

	fs = (filesystem*)malloc(sizeof(filesystem));

	/* Zero-out fields */
	memset(&fs->fb_map, 0, sizeof(map));
	memset(&fs->ino_map, 0, sizeof(map));
	memset(&fs->sb_i.blocks, 0, SUPERBLOCK_MAXBLOCKS*sizeof(block_t));
	memset(&fs->sb.inode_first_blocks, 0, MAXBLOCKS*sizeof(block_t));
	memset(&fs->sb.inode_block_counts, 0, MAXBLOCKS*sizeof(uint));

	fs->sb.root = 0;
	fs->sb.free_blocks_base = 4;					/* Start allocating from 5th block */
	fs->sb.free_inodes_base = 1;					/* Start allocating from 1st inode */
	fs->sb_i.nblocks = sizeof(superblock)/stride + 1; 		/* How many free blocks needed */
	fs->fb_map.data[0] = 0x04;					/* First four blocks reserved */

	if (newfs) {
		fs_balloc(fs, fs->sb_i.nblocks, fs->sb_i.blocks);	/* Allocate n blocks, tell us which we got */
		fs->root = fs_mkroot(fs, newfs);			/* Setup root dir */

		if (NULL == fs->root) {
			free(fs);
			return NULL;
		}
	}
	return fs;
}

/* Allocate @param count blocks if possible. Store indices in @param blocks */
int fs_balloc(filesystem* fs, const int count, block_t* indices) {
	int i, j;

	for (i = 0; i < count; i++) {	// Allocate free blocks
		j = _balloc(fs);
		if (FS_ERR == j) return FS_ERR;
		indices[i] = j;
	}
	return FS_OK;
}

/* Fill in the direct blocks of an inode. Return the count of allocated blocks */
int fs_fill_direct_blocks(block_t* blocks, uint count, block_t* blockIndices, uint maxCount) {
	uint i;
	for (i = 0; i < maxCount; i++) {
		if (i == count) break;
		blocks[i] = blockIndices[i];
	}
	return i;
}

/* Fill @param count @param block indices into the block indices of @param ino. 
 * Spill into indirect block pointers, doubly-indirected block pointers, and 
 * triply-indirected block pointers as needed. */
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
int fs_readblockfromdisk(void* dest, block_t b) {
	if (NULL == fp) return FS_ERR;

	if (0 != fseek(fp, b*BLKSIZE, SEEK_SET))
		return FS_ERR;
	if (1 != fread(dest, BLKSIZE, 1, fp))	// fread() returns 0 or 1
		return FS_ERR;			// Return ok only if exactly one block was read
	return FS_OK;
}

/* Write a block to disk */
int fs_writeblocktodisk(block_t b, size_t size, void* data) {
	if (NULL == fp) return FS_ERR;

	if (0 != fseek(fp, b*BLKSIZE, SEEK_SET))
		return FS_ERR;
	if (1 != fwrite(data, size, 1, fp))	// fwrite() returns 0 or 1
		return FS_ERR;			// Return ok only if exactly one block was written

	return FS_OK;
}

/* Read an arbitary number of blocks from disk. */
int fs_readblocksfromdisk(void* dest, block_t* blocks, uint numblocks, size_t type_size) {
	block_t j = blocks[0];

	/* Get strided blocks (more than one block or less than a whole block) */
	if (BLKSIZE != type_size) {
		uint i, k;
		size_t copysize;

		for (i = 0; i < numblocks; i++) {					/* Get root blocks from disk */
			k = j;

			if (0 == k) break;

			copysize = i+1 == numblocks ? type_size % stride : stride;	/* Get last chunk which may only be a partial block */

			if (FS_ERR == fs_readblockfromdisk(&block_cache[k], k))
				return FS_ERR;
			if (MAXBLOCKS <= k) return FS_ERR;

			memcpy(&((char*)dest)[i*stride], &block_cache[k].data, copysize);
			j = block_cache[k].next;
		}
	/* Get exactly one block */
	} else if (FS_ERR == fs_readblockfromdisk(dest, j))
		return FS_ERR;

	return FS_OK;
}

/* Write an arbitrary number of blocks to disk */
int fs_writeblockstodisk(void* source, block_t* blocks, uint numblocks, size_t type_size) {
	block_t j = blocks[0];

	// Split @param source into strides if it will not fit in one block
	if (BLKSIZE != type_size) {
		uint i; 
		size_t copysize;
		
		for (i = 0; i < numblocks; i++) {					// For all blocks
			if (0 == j) break;

			block_cache[j].num = blocks[i];

			copysize		= i+1 == numblocks ? type_size % stride : stride;	// Copy either full block or remaining chunk
			block_cache[j].next	= i+1 == numblocks ? 0 : blocks[i+1];			// Index of next block (0 if no next block)

			memcpy(block_cache[j].data, &((char*)source)[i*stride], copysize);
			j = block_cache[j].next;
		}
		
		j = blocks[0];
		for (i = 0; i < numblocks; i++) {
			if (0 == j) break;	// Sanity check: Do not write inode 0

			if (FS_ERR == fs_writeblocktodisk(j, BLKSIZE, &block_cache[j]))
				return FS_ERR;
			j = block_cache[j].next;
		}

	// Else write @param source to a whole block directly
	} else return fs_writeblocktodisk(j, type_size, source);

	return FS_OK;
}

/* Open a filesystem stored on disk */
filesystem* fs_openfs() {
	filesystem* fs = NULL;
	block_t sb_i_location = 2;

	fs = fs_init(false);
	if (NULL == fs) return NULL;

	fs_debug_print();

	fs_readblockfromdisk(&fs->fb_map, 0);
	fs_readblockfromdisk(&fs->ino_map, 1);
	fs_readblocksfromdisk(&fs->sb_i, &sb_i_location, 1, sizeof(superblock_i));
	fs_readblocksfromdisk(&fs->sb, fs->sb_i.blocks, fs->sb_i.nblocks, sizeof(superblock));

	fs->root = fs_mkroot(fs, false);

	if (NULL == fs->root || strcmp(fs->root->name,"/")) {	// We determine it's the root by name "/"
		free(fs);
		return NULL;
	}

	return fs;
}

/* Synchronize on-disk copies of the free block map,
 * free inode map, and superblock within-memory copies */
int fs_sync(filesystem* fs) {
	int status[4] = { 0 };
	int i;

	status[0] = fs_writeblockstodisk( &fs->fb_map,	&rootblocks[0],		1,			sizeof(map));		/* Write block map to disk */
	status[1] = fs_writeblockstodisk( &fs->ino_map,	&rootblocks[1],		1,			sizeof(map));		/* Write inode map to disk */
	status[2] = fs_writeblockstodisk( &fs->sb_i,	&rootblocks[2],		1,			sizeof(superblock_i));	/* Write superblock info to disk */
	status[3] = fs_writeblockstodisk( &fs->sb,	fs->sb_i.blocks,	fs->sb_i.nblocks,	sizeof(superblock));	/* Write superblock to disk */

	for (i = 0; i < 4; i++)
		if (FS_ERR == status[i])
			return FS_ERR;
	return FS_OK;
}

filesystem* fs_mkfs() {
	filesystem *fs = NULL;

	fs = fs_init(true);
	if (NULL == fs) return NULL;
	
	// TODO: Check that writes are not redundant

	/* Write root inode to disk. TODO: Need to write iblocks */
	fs_writeblockstodisk( fs->root->ino, fs->root->ino->blocks, fs->root->ino->nblocks, sizeof(inode)); 

	/* Write superblock and other important first blocks */
	if (FS_ERR == fs_sync(fs)) {
		free(fs);
		return NULL;
	}

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", fs->root->name, BLKSIZE*MAXBLOCKS/1024);
	return fs;
}

/* Print a hex dump of a struct, color nonzeros red
 * Can also use hexdump -C filename 
 * or :%!xxd in vim
 */
void print_mem(void const *vp, size_t n)
{
	size_t i;
	char* red = "\x1b[31m";
	char* reset = "\x1b[0m";
	unsigned char const* p = (unsigned char const*)vp;

#if defined(_WIN64) || defined(_WIN32)
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	WORD saved_attributes;
	GetConsoleScreenBufferInfo(console, &info);
	saved_attributes = info.wAttributes;
#endif

	for (i = 0; i < n; i++) {

		// Color nonzeros red
		if (0 != p[i]) {
#if defined(_WIN64) || defined(_WIN32)
			SetConsoleTextAttribute(console, FOREGROUND_RED);
#else
			printf("%s",red);
#endif
		}

		printf("%02x ", p[i]);

		// Restore color
		if (0 != p[i]) {
#if defined(_WIN64) || defined(_WIN32)
			SetConsoleTextAttribute(console, saved_attributes);
#else
			printf("%s",reset);
#endif
		}
	}
	putchar('\n');
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
	printf("sizeof(dent): %lu\n", sizeof(dent));
	printf("sizeof(dent_v): %lu\n", sizeof(dent_v));
	printf("sizeof(filesystem): %lu\n", sizeof(filesystem));
	printf("MAXFILEBLOCKS: %lu\n", MAXFILEBLOCKS);
	printf("FS_MAXPATHLEN: %lu\n", FS_MAXPATHLEN);
}
