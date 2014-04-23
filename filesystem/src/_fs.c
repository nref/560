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
#include "_fs.h"
#
#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#endif

/* Type-agnostic way to print the binary data of a struct */
/* http://stackoverflow.com/questions/5349896/print-a-struct-in-c */
#define PRINT_STRUCT(p)  print_mem((p), sizeof(*(p)))

char* fs_responses[6]		= {	"OK", "General Error", "Directory exists", 
					"stat() failed for the given path",
					"An inode was not found on disk",
					"Too few arguments" };

char* fname = "fs";			/* The name our filesystem will have on disk */
block block_cache[MAXBLOCKS];		/* In-memory copies of blocks on disk */
size_t stride;				/* The data field is smaller than BLKSIZE
					 * so our writes to disk are not BLKSIZE but rather 
					 * BLKSIZE - sizeof(other fields in block struct) */
block_t  rootblocks[] = { 0, 1, 2 };	/* Indices to the first blocks */
FILE* fp = NULL;			/* Pointer to file storage */

/* Close the filesystem file if is was open */
static void _safeclose() {
	if (NULL != fp)
		fclose(fp);
	fp = NULL;
}

static int _isNumeric(char* str) {
	uint i;

	for (i = 0; i < strlen(str); i++) {
		if (!isdigit(str[i]))
			return false;
	}
	return true;
}

/* Open the filesystem file and check for NULL */
static void _safeopen(char* fname, char* mode) {
	_safeclose();	/* Close whatever was already open */

	fp = fopen(fname, mode);
	if (NULL == fp) {
		//printf("fopen: \"%s\" %s\n", fname, strerror(errno));
		return;
	}
}

/* If fields of an fs_path are { "a", "b", "c"},
 * Then the string representing the path is "/a/b/c" */
static fs_path* _newPath() {
	uint i = 0;
	fs_path* path = (fs_path*)malloc(sizeof(fs_path));

	for (i = 0; i < FS_MAXPATHFIELDS; i++) {
		path->fields[i] = (char*)malloc(FS_NAMEMAXLEN);
		memset(path->fields[i], 0, FS_NAMEMAXLEN);
	}

	path->nfields = 0;
	path->firstField = 0;

	return path;
}

static void _pathFree(fs_path* p) {
	uint i;
	if (NULL == p) return;

	for (i = 0; i < FS_MAXPATHFIELDS; i++)
		free(p->fields[i]);
	free(p);
}

/* Split a string on delimiter(s) */
static fs_path* _tokenize(const char* str, const char* delim) {
	size_t i = 0;
	char* next_field	= NULL;
	size_t len;
	char* str_cpy;
	fs_path* path;

	if (NULL == str || '\0' == str[0]) return NULL;

	len = strlen(str);
	str_cpy = (char*)malloc(sizeof(char) * len + 1);
	path = _newPath();
	
	// Copy the input because strtok replaces delimieter with '\0'
	strncpy(str_cpy, str, min(FS_NAMEMAXLEN-1, len+1));	
	str_cpy[len] = '\0';

	// Split the path on delim
	next_field = strtok(str_cpy, delim);
	while (NULL != next_field) {

		i = path->nfields;
		len = strlen(next_field);

		strncpy(path->fields[i], next_field, min(FS_NAMEMAXLEN-1, len+1));	
		path->fields[i][len] = '\0';

		if (path->nfields + 1 == FS_MAXPATHFIELDS)
			break;

		next_field = strtok(NULL, delim);
		path->nfields++;
	}

	free(str_cpy);
	return path;
}

/* Synchronize on-disk copies of the free block map,
 * free inode map, and superblock within-memory copies */
static int _sync(filesystem* fs) {
	int status[4] = { 0 };
	int i;

	status[0] = _fs.writeblocks( &fs->fb_map,	&rootblocks[0],		1,			sizeof(map));		/* Write block map to disk */
	status[1] = _fs.writeblocks( &fs->ino_map,	&rootblocks[1],		1,			sizeof(map));		/* Write inode map to disk */
	status[2] = _fs.writeblocks( &fs->sb_i,	&rootblocks[2],		1,			sizeof(superblock_i));	/* Write superblock info to disk */
	status[3] = _fs.writeblocks( &fs->sb,	fs->sb_i.blocks,	fs->sb_i.nblocks,	sizeof(superblock));	/* Write superblock to disk */

	for (i = 0; i < 4; i++)
		if (FS_ERR == status[i])
			return FS_ERR;
	return FS_OK;
}

/* Read from disk the inode to which @param num refers. */
static inode* _inode_load(filesystem* fs, inode_t num) {
	inode* ino = NULL;
	block_t first_block_num;

	if (NULL == fs) return NULL;
	if (MAXBLOCKS <= num) return NULL;	/* Sanity check */

	first_block_num = fs->sb.inode_first_blocks[num];

	if (0 == first_block_num)
		return NULL;			/* An inode of 0 does not exist on disk */

	ino = _fs._new_inode();

	/* Load the block(s) for the inode itlsef */
	if (FS_ERR == 
		_fs.readirectblocks(	ino, &first_block_num, 
					sizeof(inode)/stride + 1,
					sizeof(inode))	) 
	{
		/* If the disk read failed */
		if (NULL != ino)
			free(ino);
		return NULL;
	}

	ino->v_attached = 0;
	return ino;
}

/* Write an inode to disk and free its associated memory */
static int _inode_unload(filesystem* fs, inode* ino) {
	if (FS_ERR == _fs.write_commit(fs, ino))
		return FS_ERR;

	_fs._free_inode(ino);
	ino = NULL;

	return FS_OK;
}

/* Free the index of an inode and its malloc'd memory
 * @param blk pointer to the block to free
 * Returns FS_OK on success, FS_ERR if the blockv*/
static int _ifree(filesystem* fs, inode_t num) {
	if (NULL == fs) return FS_ERR;

	if (0x0 == fs->fb_map.data[num])
		return FS_ERR;	/* Block already free */

	--fs->ino_map.data[num];
	fs->sb.free_inodes_base = num;

	if (FS_ERR == _sync(fs))
		return FS_ERR;

	return FS_OK;
}

/* Find an unused inode number and return it */
static int _ialloc(filesystem *fs) {
	uint i, blockidx, blockval;

	for (i = fs->sb.free_inodes_base; i < MAXINODES; i++)
	{
		blockidx = i/255;	/* max int value of char */
		blockval = (int)fs->ino_map.data[blockidx];
		if (blockval < 255) {		// If this char is not full 
			((fs->ino_map).data)[blockidx]++;
			fs->sb.free_inodes_base++;

			if (FS_ERR == _sync(fs))
				return FS_ERR;

			return fs->sb.free_inodes_base;
		}
	}
	return 0;
}

/* Free the index of a block and its malloc'd memory
 * @param blk pointer to the block to free
 * Returns FS_OK on success, FS_ERR if the block
 * was already free */
static int _bfree(filesystem* fs, block* blk) {
	if (NULL == fs) return FS_ERR;
	if (NULL == blk) return FS_ERR;

	if (0x0 == fs->fb_map.data[blk->num])
		return FS_ERR;	/* Block already free */

	--fs->fb_map.data[blk->num];
	fs->sb.free_blocks_base = blk->num;
	free(blk);

	if (FS_ERR == _sync(fs))
		return FS_ERR;

	return FS_OK;
}

/* Traverse the free block array and return a block 
 * whose field num is the index of the first free bock */
static int __balloc(filesystem* fs) {
	size_t i, blockidx, blockval;

	/* One char in the free block map represents 
	 * 8 blocks (sizeof(char) == 1 byte == 8 bits) */
	
	for (i = fs->sb.free_blocks_base; i < MAXBLOCKS; i++)
	{
		blockidx = i/255;	/* max int value of char */
		blockval = (int)fs->fb_map.data[blockidx];
		if (blockval < 255) {		// If this char is not full 
			((fs->fb_map).data)[blockidx]++;
			fs->sb.free_blocks_base++;

			if (FS_ERR == _sync(fs))
				return FS_ERR;

			return (int)fs->sb.free_blocks_base;
		}
	}
	return FS_ERR;
}

/* Allocate @param count blocks if possible. Store indices in @param blocks */
static int _balloc(filesystem* fs, const size_t count, block_t* bindices) {
	size_t i;
	int j;

	//if (NULL != ino) 
	//	bindices = ino->blocks;

	for (i = 0; i < count; i++) {	// Allocate free blocks
		j = __balloc(fs);
		if (FS_ERR == j) return FS_ERR;
		bindices[i] = (block_t)j;
	}

	return FS_OK;
}

/* Create a and zero-out a new on-disk directory entry
 * @param alloc_inode specifies whether this directory
 * gets allocated an inode, else inode 0 is given.
 * @param name the new directory name.
 */
static dent* _newd(filesystem* fs, const int alloc_inode, const char* name) {
	dent* d = NULL;
	d = (dent*)malloc(sizeof(dent));

	if (alloc_inode)
		d->ino = (inode_t)_ialloc(fs);
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
static dentv* _newdv(filesystem* fs, const int alloc_inode, const char* name) {
	dent*	d	= NULL;
	dentv*	dv	= NULL;

	dv		= (dentv*)	malloc(sizeof(dentv));
	dv->ino		= (inode*)	malloc(sizeof(inode));

	dv->files	= (inode**)	malloc(FS_MAXFILES*sizeof(inode*));
	dv->links	= (inode**)	malloc(FS_MAXLINKS*sizeof(inode*));

	memset(	dv->files, 0, FS_MAXFILES*sizeof(inode*));
	memset(	dv->links, 0, FS_MAXLINKS*sizeof(inode*));

	memset(	dv->ino->blocks, 0, sizeof(block_t)*MAXFILEBLOCKS);
	memset(	dv->ino->directblocks, 0, sizeof(block_t)*NBLOCKS);

	dv->tail	= NULL;
	dv->head	= NULL;
	dv->parent	= NULL;
	dv->next	= NULL;
	dv->prev	= NULL;

	dv->nfiles	= 0;
	dv->ndirs	= 0;
	dv->nlinks	= 0;
	dv->ino->nlinks	= 0;
	dv->ino->ninoblocks = (uint16_t) (sizeof(inode)/stride+1);	/* How many blocks the inode consumes */
	dv->ino->ndatablocks=0;
	dv->ino->nblocks= dv->ino->ninoblocks + dv->ino->ndatablocks;	/* How many blocks the inode data consumes */
	dv->ino->size	= dv->ino->ninoblocks*BLKSIZE;
	dv->ino->mode	= FS_DIR;
	dv->ino->v_attached = 1;

	strncpy(dv->name, name, min(FS_NAMEMAXLEN-1, strlen(name)+1));
	dv->name[FS_NAMEMAXLEN-1] = '\0';

	d = _newd(fs, alloc_inode, name);
	memcpy(&dv->ino->data.dir, d, sizeof(dv->ino->data.dir));
	dv->ino->datav.dir = dv;
	dv->ino->num = d->ino;

	free(d);
	return dv;
}

/* Create a new directory in the directory tree.
 * Sets up pointers (dentv) and indices (dent) for 
 * linked-list traversal. Writes new dent and
 * changed existing dents to disk. Calls _sync();
 */
static dentv* _new_dir(filesystem *fs, dentv* parent, const char* name) {
	dentv* dv = NULL;
	inode* tail = NULL;
	int makingRoot = 0;

	/* We're making the root */
	if (NULL == parent && !strcmp(name, "/")) 
		makingRoot = 1;

	/* Allocate a new inode number */
	dv = _newdv(fs, true, name);
	if (NULL == dv) return NULL;

	/* Allocate n blocks, tell us which we got */
	if (FS_ERR == 
		_balloc(	fs, dv->ino->ninoblocks, 
				dv->ino->blocks) ) 
	{
		free(dv);
		return NULL;
	}
		
	fs->sb.inode_first_blocks[dv->ino->num] = dv->ino->blocks[0];
	fs->sb.inode_block_counts[dv->ino->num] = dv->ino->nblocks;

	if (!makingRoot) {

		/* If we need to load the tail (the last dir added to @param parent)*/
		tail = parent->tail;	/* Try in memory */
		if (NULL == tail || NULL == tail->datav.dir || !tail->v_attached) {
			parent->ino->datav.dir->tail = 
				_inode_load(fs, parent->ino->data.dir.tail); /* Try from disk */
			tail = parent->ino->datav.dir->tail;

			if (NULL != tail)
				tail->v_attached = 1;

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
			_fs.writeblocks(tail, tail->blocks, tail->nblocks, sizeof(inode));
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
			dv->prev->datav.dir->next = dv->ino;
			//parent->tail->datav.dir->next = dv->ino;	/* Updating the tail in-memory requires no disk write */
			parent->tail = dv->ino;
		}

		/* Update change to parent */
		_fs.writeblocks(parent->ino, parent->ino->blocks, parent->ino->ninoblocks, sizeof(inode));

	} else {
		/* Making root */
	}

	/* Write changes to disk */
	_fs.writeblocks(dv->ino, dv->ino->blocks, dv->ino->ninoblocks, sizeof(inode));
	if (FS_ERR == _sync(fs))
		return NULL;

	return dv;
}

/* Create an on-disk file*/
static file* _newf(filesystem* fs, const int alloc_inode, const char* name) {
	file* f = NULL;
	f = (file*)malloc(sizeof(file));

	if (alloc_inode)
		f->ino = (inode_t)_ialloc(fs);
	else f->ino = 0;
	f->seek_pos = 0;

	// Copy name
	strncpy(f->name, name, min(FS_NAMEMAXLEN-1, strlen(name)+1));
	f->name[FS_NAMEMAXLEN-1] = '\0';

	return f;
}

/* Create an in-memory file */
static filev* _newfv(filesystem* fs, const int alloc_inode, const char* name) {
	file*	f	= NULL;
	filev*	fv	= NULL;

	fv = (filev*)malloc(sizeof(filev));
	fv->ino = _fs._new_inode();

	memset(	fv->ino->blocks, 0, sizeof(block_t)*MAXFILEBLOCKS);

	fv->ino->nlinks	= 0;
	fv->ino->ninoblocks = (uint16_t) (sizeof(inode)/stride+1);	/* How many blocks the inode consumes */
	fv->ino->ndatablocks=0;
	fv->ino->nblocks= fv->ino->ninoblocks + fv->ino->ndatablocks;	/* How many blocks the inode data consumes */
	fv->ino->size	= fv->ino->ninoblocks*BLKSIZE;
	fv->ino->mode = FS_FILE;
	fv->ino->v_attached = true;
	fv->ino->datav.file = fv;

	strncpy(fv->name, name, min(FS_NAMEMAXLEN-1, strlen(name)+1));
	fv->name[FS_NAMEMAXLEN-1] = '\0';
	fv->seek_pos = 0;

	f = _newf(fs, alloc_inode, name);
	memcpy(&fv->ino->data.file, f, sizeof(fv->ino->data.file));
	fv->ino->num = f->ino;
	free(f);

	if (FS_ERR == _balloc(fs, fv->ino->ninoblocks, fv->ino->blocks))
		return NULL;

	return fv;
}

/* Create a new file of @param name in the given directory @parent */
static filev* _new_file(filesystem* fs, dentv* parent, const char* name) {
	filev* fv = NULL;

	/* Allocate a new inode number */
	fv = _newfv(fs, true, name);
	if (NULL == fv) return NULL;

	parent->files[parent->nfiles] = fv->ino;
	parent->ino->data.dir.files[parent->ino->data.dir.nfiles] = fv->ino->num;

	parent->nfiles++;
	parent->ino->data.dir.nfiles++;

	_fs.writeblocks(parent->ino, parent->ino->blocks, parent->ino->ninoblocks, sizeof(inode));
	fs->sb.inode_first_blocks[fv->ino->num] = fv->ino->blocks[0];
	
	if (FS_ERR == _fs._sync(fs)) {
		return NULL;
	}

	return fv;
}

static inode* _new_inode() {
	uint i, j;
	inode* ino = NULL;

	ino = (inode*)malloc(sizeof(inode));

	ino->ib1 = (iblock1*)malloc(sizeof(iblock1));

	ino->ib2 = (iblock2*)malloc(sizeof(iblock2));
	for (i = 0; i < NIBLOCKS; i++) {
		ino->ib2->iblocks[i] = (iblock1*)malloc(sizeof(iblock1));
	}

	ino->ib3 = (iblock3*)malloc(sizeof(iblock3));
	for (i = 0; i < NIBLOCKS; i++) {
		ino->ib3->iblocks[i] = (iblock2*)malloc(sizeof(iblock2));

		for (j = 0; j < NIBLOCKS; j++)
			ino->ib3->iblocks[i]->iblocks[j] = (iblock1*)malloc(sizeof(iblock1));
	}

	return ino;
}

static void _free_inode(inode* ino) {
	uint i, j, k;
	uint blks_freed = 0;

	if (NULL == ino) return;

	if (ino->directblocks) {
		for (i = 0; i < MAXBLOCKS_DIRECT; i++) {
			if (blks_freed >= ino->ndatablocks)
				break;

			if (!ino->directblocks[i])
				continue;

			free(ino->directblocks[i]);
			++blks_freed;
		}
	}

	if (ino->ib1) {
		for (i = 0; i < NBLOCKS_IBLOCK; i++) {
			if (blks_freed >= ino->ndatablocks)
				break;

			if (!ino->ib1->blocks[i])
				continue;

			free(ino->ib1->blocks[i]);
			++blks_freed;
		}
		free(ino->ib1);
	}

	if (ino->ib2) {
		uint stop = false;

		for (i = 0; i < NIBLOCKS; i++) {
			if (stop) break;

			if (!ino->ib2->iblocks[i]) 
				continue;

			for (j = 0; j < NBLOCKS_IBLOCK; j++) {	
				if (blks_freed >= ino->ndatablocks) {
					stop = true;
					break;
				}
				if (!ino->ib2->iblocks[i])
					continue;

				free(ino->ib2->iblocks[i]->blocks[j]);
				++blks_freed;
			}

			free(ino->ib2->iblocks[i]);
		}
		free(ino->ib2);
	}

	if (ino->ib3)
	{
		uint stop = false;
		for (i = 0; i < NIBLOCKS; i++) {
			if (stop) break;

			if(!ino->ib3->iblocks[i])
				continue;

			for (j = 0; j < NIBLOCKS; j++) {
				if (stop) break;

				if (!ino->ib3->iblocks[i]->iblocks[j])
					continue;

				for (k = 0; k < NBLOCKS_IBLOCK; k++) {
					if (blks_freed >= ino->ndatablocks) {
						stop = true;
						break;
					}

					if (!ino->ib3->iblocks[i]->iblocks[j]->blocks[k])
						continue;
					
					free(ino->ib3->iblocks[i]->iblocks[j]->blocks[k]);
					++blks_freed;
				}

				free(ino->ib3->iblocks[i]->iblocks[j]);
			}
			free(ino->ib3->iblocks[i]);
		}
		free(ino->ib3);
	}

}

/* Get an unallocated file descriptor */
static int _get_fd(filesystem* fs) {
	uint i;

	for (i = fs->first_free_fd; i < FS_MAXOPENFILES; i++){
		if (false == fs->allocated_fds[i]) {
			fs->allocated_fds[i] = true;
			fs->first_free_fd = i+1;
			return i;
		}
	}

	return FS_ERR;
}

/* Free an allocated file descriptor */
static int _free_fd(filesystem* fs, int fd) {
	if (false == fs->allocated_fds[fd])	/* fd was already free */
		return FS_ERR;

	fs->allocated_fds[fd] = false;
	fs->first_free_fd = fd;

	return FS_OK;
}

/* Convert an inode to an in-memory directory */
static dentv *_ino_to_dv(filesystem* fs, inode* ino) {
	dentv *dv;

	if (NULL == ino) return NULL;

	dv = _newdv(fs, false, ino->data.dir.name);
	if (NULL == dv) return NULL;

	dv->head		= _inode_load(fs, ino->data.dir.head);

	if (ino->data.dir.head != ino->data.dir.tail)
		dv->tail	= _inode_load(fs, ino->data.dir.tail);
	else	dv->tail	= dv->head;

	if (ino->num != ino->data.dir.parent)
		dv->parent	= _inode_load(fs, ino->data.dir.parent);
	else	dv->parent	= ino;

	if (ino->num != ino->data.dir.next)
		dv->next	= _inode_load(fs, ino->data.dir.next);
	else	dv->next	=ino;

	if (ino->num != ino->data.dir.prev)
		dv->prev	= _inode_load(fs, ino->data.dir.prev);
	else	dv->prev	= ino;

	if (	NULL == dv->parent	|| 
		NULL == dv->next	|| 
		NULL == dv->prev	) {

		free(dv);
		return NULL;
	}

	free(dv->ino);

	dv->ino			= ino;
	dv->ino->datav.dir	= dv;

	dv->ndirs		= dv->ino->data.dir.ndirs;
	dv->nfiles		= dv->ino->data.dir.nfiles;
	dv->nlinks		= dv->ino->data.dir.nlinks;

	dv->ino->v_attached	= true;

	return dv;
}

/* Convert an inode to an in-memory file */
static filev* _ino_to_fv(filesystem* fs, inode* ino) {
	filev* fv = NULL;
	if (NULL == ino) return NULL;

	fv = _newfv(fs, false, ino->data.file.name);
	if (NULL == fv) return NULL;

	fv->ino			= ino;
	fv->ino->datav.file	= fv;
	fv->ino->v_attached	= true;
	fv->ino->mode		= FS_FILE;
	fv->mode		= FS_READ;
	return fv;
}

/* Given an inode number, load the corresponding file structure. 
 * Wrap it in an inode which contains the in-memory version of the file */
static filev* _load_file(filesystem* fs, inode_t num) {
	uint i;
	inode* ino = _inode_load(fs, num);
	ino->datav.file = _ino_to_fv(fs, ino);

	for (i = 0; i < ino->ndatablocks; i++)
		_fs.readblock(ino->directblocks[i], ino->blocks[i + ino->ninoblocks]);

	return ino->datav.file;
}

/* Free the memory allocated to an in-memory file structure */
static int _unload_file(filesystem* fs, inode* ino) {
	int status1, status2;
	
	status1 = _fs._sync(fs);

	free(ino->datav.file);
	ino->datav.file = NULL;
	ino->v_attached = false;

	status2 = _fs.writeblocks(ino, ino->blocks, ino->ninoblocks, sizeof(inode));
	_inode_unload(fs, ino);

	if (FS_ERR == status1 || FS_ERR == status2)
		return FS_ERR;
	return FS_OK;
}

/* Given an inode number, load the corresponding dent
 * Return a dentv whose field data.dir contains it. */
static dentv* _load_dir(filesystem* fs, inode_t num) {
	dentv *dv;
	uint i;

	inode* ino = _inode_load(fs, num);
	if (NULL == ino) return NULL;

	dv = _ino_to_dv(fs, ino);
	if (NULL == dv) {
		free(ino);
		return NULL;
	}

	if (NULL != dv->head)	{
		if (!dv->head->v_attached) {
			dv->head->datav.dir = _ino_to_dv(fs, dv->head);
			dv->head->v_attached = true;
		}
	}
	if (NULL != dv->tail)	{
		if (!dv->tail->v_attached) {
			dv->tail->datav.dir = _ino_to_dv(fs, dv->tail);
			dv->tail->v_attached = true;
		}
	}
	if (NULL != dv->parent) {
		if (!dv->parent->v_attached) {
			dv->parent->datav.dir = _ino_to_dv(fs, dv->parent);
			dv->parent->v_attached = true;
		}
	}
	if (NULL != dv->next)	{
		if (!dv->next->v_attached) {
			dv->next->datav.dir = _ino_to_dv(fs, dv->next);
			dv->next->v_attached = true;
		}
	}
	if (NULL != dv->prev)	{
		if (!dv->prev->v_attached) {
			dv->prev->datav.dir = _ino_to_dv(fs, dv->prev);
			dv->prev->v_attached = true;
		}
	}

	if (	(NULL != dv->parent && NULL == dv->parent->datav.dir)	||
		(NULL != dv->next && NULL == dv->next->datav.dir)	||
		(NULL != dv->prev && NULL == dv->prev->datav.dir)	) {

		free(dv);
		return NULL;
	}

	for (i = 0; i < dv->nfiles; i++) {
		filev* fv = _load_file(fs, dv->ino->data.dir.files[i]);
		dv->files[i] = fv->ino;
	}
	//for (i = 0; i < dv->nlinks; i++)	// TODO
	//	dv->files[i] = _load_link(fs, dv->ino->data.dir.links[i]);
	return dv;
}

/* Free the memory occupied by a dentv*/
static int _unload_dir(filesystem* fs, inode* ino) {
	size_t i;
	int status1, status2;
	dentv* dv = NULL;
	inode* iterator;
	inode* next;

	if (FS_DIR != ino->mode) return FS_ERR;

	dv = ino->datav.dir;
	ino->v_attached = false;

	status1 = _fs._sync(fs);
	status2 = _fs.writeblocks(ino, ino->blocks, ino->nblocks, sizeof(inode));
	
	if (FS_ERR == status1 || FS_ERR == status2)
		return FS_ERR;

	/* Free the whole subtree under this directory */
	iterator = dv->head;
	next = dv->head;
	while (NULL != next) {
		next = iterator->datav.dir->next;
		if (iterator->v_attached)
			_unload_dir(fs, iterator);
	}

	for (i = 0; i < dv->nfiles; i++)
		_unload_file(fs, dv->files[i]);
	//for (int i = 0; i < dv->nlinks; i++) /* TODO */
	//	_unload_link(fs, dv->files[i]);

	free(ino->datav.dir);
	ino->datav.dir = NULL;
	_inode_unload(fs, ino);

	return FS_OK;
}

/* Load a dentv from disk and put it in an inode*/
static int _v_attach(filesystem* fs, inode* ino) {
	if (NULL == ino) return FS_ERR;

	if (!ino->v_attached) {
		switch (ino->mode) {

		case FS_DIR: 
		{
			ino->datav.dir = _load_dir(fs, ino->num);
			break;
		}
		case FS_FILE:
			ino->datav.file = _load_file(fs, ino->num);
			break;
		case FS_LINK:
			break; /* TODO */
		}

		if (NULL == ino->datav.dir) return FS_ERR;
		ino->v_attached = true;
	}
	return FS_OK;
}

/* Write the data in an inode disk and free the in-memory data */
static int _v_detach(filesystem* fs, inode* ino) {
	if (NULL == ino) return FS_ERR;
	
	if (ino->v_attached) {
		switch (ino->mode) {

		case FS_DIR: 
		{
			_unload_dir(fs, ino);
			break;
		}
		case FS_FILE:
			_unload_file(fs, ino);
			break;
		case FS_LINK:
			break; /* TODO */
		}
	}

	return FS_OK;
}

/* Get the inode of a directory, link, or file, at the end of a path.
 * Follow an array of cstrings which are the fields of a path, e.g. input of 
 * { "bob", "dylan", "is", "old" } represents path "/bob/dylan/is/old"
 * 
 * @param dir The root of the tree to traverse.
 * @param depth The depth of the path
 * @param path The fields of the path
 * Return the inode at the end of this path or NULL if not found.
 */ 
static inode* _recurse(filesystem* fs, dentv* dv, size_t current_depth, size_t max_depth, char* path[]) {
	uint i;									// Declarations go here to satisfy Visual C compiler
	dentv* iterator;

	if (NULL == dv) 
		return NULL;
	
	if (NULL == dv->head && 
		dv->ndirs == 0 &&
		dv->nfiles == 0 &&
		dv->nlinks == 0) 
	{
		return NULL;							/* Dead-end in the filesystem tree */
	}

	/* Recurse into subdirectories if there are any */
	if (NULL != dv->head) {

		if (!dv->head->v_attached) {					// Load the first directory from disk if not already in memory
			if (FS_ERR == _v_attach(fs, dv->head))
				return NULL;
		}

		if (NULL == dv->head->datav.dir) 
			return NULL;

		iterator = dv->head->datav.dir;

		/* For each subdirectory */
		for (i = 0; i < dv->ndirs; i++)
		{
			if (NULL == iterator) return NULL;				// This happens if there are no subdirs

			if (!strcmp(iterator->name, path[current_depth])) {		// If we have a matching directory name
				if (max_depth == current_depth)				// If we can't go any deeper
					return iterator->ino;				// Return the inode of the matching dir

				else return _recurse(fs, iterator,			// Else recurse into subdir
					current_depth + 1, max_depth, path); 
			}

			if (NULL == iterator->next) return NULL;			// Return if we have iterated over all subdirs

			if (!iterator->next->v_attached) {				// Load the next directory from disk if not already in memory
				if (FS_ERR == _v_attach(fs, iterator->next))
					return NULL;
			}

			iterator = iterator->next->datav.dir;
		}
	}

	/* Iterate over files */
	for (i = 0; i < dv->nfiles; i++) {						// For each file at this level
		
		filev* fv = _load_file(fs, dv->ino->data.dir.files[i]);

		if (!strcmp(fv->name, path[current_depth])) {
			dv->files[i] = fv->ino;

			if (!dv->files[i]->v_attached) {				// Load the file from disk if not already in memory
				if (FS_ERR == _v_attach(fs, dv->files[i]))
					break;
			}
			return dv->files[i]->datav.file->ino;
		}
		else _unload_file(fs, fv->ino);		// TODO: this _load_file, _unload_file pair results in disk writes. Make write-free.
	}

	/* Iterate over links */
	for (i = 0; i < dv->ino->nlinks; i++) {						// For each link at this level
		if (!strcmp(dv->links[i]->data.link.name, path[current_depth])) {

			if (!dv->links[i]->v_attached) {				// Load the file from disk if not already in memory
				if (FS_ERR == _v_attach(fs, dv->links[i]))
					break;
			}
			return dv->links[i]->datav.link->ino;
		}
	}
	return NULL;	/* No matching inode found */
}

/* Return the given string less the first character */
static char* _strSkipFirst(char* str) {
	return &str[1];
}

/* Return the given string less the last character */
static char* _strSkipLast(char* str) {
	size_t len;
	if (NULL == str || '\0' == str[0]) 
		return NULL;

	len = strlen(str);
	str[len - 1] = '\0';
	return str;
}

/* Trim the leading and trailing white space from a string. 
 * The string is shifted into the first position so that free()
 * still works. http://stackoverflow.com/a/122974/472308 */
static char* _trim(char *str)
{
	size_t len = 0;
	char *frontp = str - 1;
	char *endp = NULL;

	if( str == NULL )
		return NULL;

	if( str[0] == '\0' )
		return str;

	len = strlen(str);
	endp = str + len;

	/* Move the front and back pointers to address
	* the first non-whitespace characters from
	* each end.
	*/
	while( isspace(*(++frontp)) );
	while( isspace(*(--endp)) && endp != frontp );

	if( str + len - 1 != endp )
		*(endp + 1) = '\0';
	else if( frontp != str &&  endp == frontp )
		*str = '\0';

	/* Shift the string so that it starts at str so
	* that if it's dynamically allocated, we can
	* still free it on the returned pointer.  Note
	* the reuse of endp to mean the front of the
	* string buffer now.
	*/
	endp = str;
	if( frontp != str )
	{
		while( *frontp ) *endp++ = *frontp++;
		*endp = '\0';
	}


	return str;
}
/* Remove leading and final forward slashes from a string
 * if they exist */
static char* _pathTrimSlashes(char* path) {
	size_t len;
	if (NULL == path || '\0' == path[0]) 
		return NULL;

	if ('/' == path[0])		
		path = _strSkipFirst(path);	/* Remove leading forward slash */
	
	len = strlen(path);
	if ('/' == path[len - 1])	
		path = _strSkipLast(path);	/* Remove trailing forward slash */

	return path;
}

/* Split a string on "/" */
static fs_path* _pathFromString(const char* str) {
	return _tokenize(str, "/");
}

/* Return an absolute path */
static char* _stringFromPath(fs_path* p) {
	size_t i;
	char* path;
	
	if (NULL == p) return NULL;

	path = (char*)malloc(FS_MAXPATHLEN);
	memset(path, 0, FS_MAXPATHLEN);

	strcat(path, "/");

	for (i = p->firstField; i < min(p->nfields, FS_MAXPATHFIELDS); i++) {
		strcat(path, p->fields[i]);
		strcat(path, "/");
	}
	path[FS_NAMEMAXLEN] = '\0';
	return path;
}

/* Get the path minus the last field, e.g. "a/b" from "a/b/c" */
static char* _pathSkipLast(fs_path* p) {
	char* path;

	if (NULL == p) return NULL;

	p->nfields--;
	path = _stringFromPath(p);
	p->nfields++;

	return path;
}

/* Get the last field in the path, e.g. "c" from "a/b/c" */
static char* _pathGetLast(fs_path* p) {
	char* p_str;

	if (NULL == p) return NULL;

	p->firstField = p->nfields - 1;
	p_str = _stringFromPath(p);
	p->firstField = 0;

	p_str = _pathTrimSlashes(p_str);

	return p_str;
}

/* Append a path element to a path structure*/
static int _pathAppend(fs_path* p, const char* appendage) {
	size_t len;
	char* cpy;
	char* cpy_free_ptr;
	
	if (	NULL == p || 
		NULL == appendage || 
		'\0' == appendage[0]	) 
	{ return FS_ERR; }

	if (FS_MAXPATHFIELDS == p->nfields)
		return FS_ERR;

	len = strlen(appendage);
	cpy_free_ptr = (char*)malloc(len+1);
	cpy = cpy_free_ptr;

	strncpy(cpy, appendage, min(len, FS_MAXPATHLEN));
	cpy[len] = '\0';

	cpy = _pathTrimSlashes(cpy);

	if (0 == strlen(cpy) || !strcmp("/", cpy)){ 	/* Skip empty string and "/" */
		free(cpy_free_ptr);
		return FS_ERR;
	}

	strncpy(	p->fields[p->nfields], 
			cpy, 
			min(FS_MAXPATHLEN, strlen(cpy))	);

	free(cpy_free_ptr);
	p->nfields++;
	return FS_OK;
}

static char* _getAbsolutePath(char* current_dir, char* path) {
	fs_path* p = NULL;
	char* abs_path = NULL;
	
	p = _pathFromString(current_dir);

	if ('/' == path[0])	/* Path is already absolute*/
		return path;

	if ('.' == path[0]) {
		if ('.' == path[1])	printf("_getAbsolutePath(): .. not yet implemented\n");
		else			printf("_getAbsolutePath(): . not yet implemented\n");
		return NULL;
	}

	_pathAppend(p, path);
	abs_path = _stringFromPath(p);

	free(p);
	return abs_path;
}

/* Traverse a directory up to the root, append dir ames while recursing back down*/
static char* _getAbsolutePathDV(dentv* dv, fs_path *p) {
	if (NULL == dv || NULL == dv->parent) return NULL;

	if (!strcmp(dv->name, "/"))	/* Stop at root or we will loop forever */
		return "/";

	if (!dv->parent->v_attached) return NULL;

	_getAbsolutePathDV(dv->parent->datav.dir, p);
	_pathAppend(p, dv->name);

	return _stringFromPath(p);
}

/* A special version of mkdir that makes the root dir
 * @param newfs if true, load a preexisting root dir
 * Else make a new root dir */
static dentv* _mkroot(filesystem *fs, int newfs) {
	dentv* dv = NULL;
	
	if (newfs) {
		dv = _new_dir(fs, NULL, "/");
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
		dv = _load_dir(fs, fs->sb.root);
		if (NULL == dv) return NULL;
	}

	return dv;
}

/* malloc memory for a block, zero-out its fields
 * Returns the allocated block */
static block* _newBlock() {
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
 * Returns FS_OK on success, FS_ERR on failure. */
static int _prealloc() {
	int status;
#if defined(_WIN64) || defined(_WIN32)	// Have to put declaration here.
	LARGE_INTEGER offset;		// because Visual C compiler is OLD school
#endif

	_safeopen(fname, "wb");
	if (NULL == fp) return FS_ERR;		

#if defined(_WIN64) || defined(_WIN32)
	offset.QuadPart = BLKSIZE*MAXBLOCKS;
	status = SetFilePointerEx(fp, offset, NULL, FILE_BEGIN);
	SetEndOfFile(fp);
#elif __APPLE__	/* __MACH__ also works */
	fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, BLKSIZE*MAXBLOCKS, 0};
	status = fcntl(fileno(fp), F_PREALLOCATE, &store);
#elif __unix__
	status = posix_fallocate(fp, 0, BLKSIZE*MAXBLOCKS);
#endif

	_safeclose();

	if (status < 0) { 
		perror("allocation error");
		return status;
	}

	return FS_OK;
}

/* Zero-out the on-disk file 
 * Returns FS_OK on success, FS_ERR on failure */
static int _zero() {
	int i;
	_safeopen(fname, "rb+");
	if (NULL == fp) return FS_ERR;

	for (i = 0; i < MAXBLOCKS; i++) {
		block* newBlock = _newBlock();
		char temp[128];
		sprintf(temp, "%d", i);
		memcpy(newBlock->data, temp, strlen(temp));
		
		if (0 != fseek(fp, BLKSIZE*i, SEEK_SET))
			return FS_ERR;
		fputs(newBlock->data, fp);
		free(newBlock);
	}
	_safeclose();
	return FS_OK;
}

/* Create a new filesystem. @param newfs specifies if we 
 * open a file on disk or create a new one (overwriting
 * the previous)
 * Returns a pointer to the allocated filesystem */
static filesystem* _init(int newfs) {
	filesystem *fs = NULL;

	stride = sizeof(((struct block*)0)->data);
	if (0 == stride) return NULL;

	if (newfs) {
		int err1, err2;

		_safeclose();
		err1 = _prealloc();		/* Make a contiguous file on disk (performance enhancement) */
		err2 = _zero();		/* Zero-out all filesystem blocks */

		if (FS_ERR == err1 || FS_ERR == err2) return NULL;
	}

	_safeopen(fname, "rb+");	/* Test if file exists */
	if (NULL == fp) return NULL;

	fs = (filesystem*)malloc(sizeof(filesystem));

	/* Zero-out fields */
	memset( &fs->fb_map, 0,			sizeof(map));
	memset( &fs->ino_map, 0,		sizeof(map));
	memset( &fs->sb_i.blocks, 0,		SUPERBLOCK_MAXBLOCKS*sizeof(block_t));
	memset( &fs->sb.inode_first_blocks, 0,	MAXBLOCKS*sizeof(block_t));
	memset( &fs->sb.inode_block_counts, 0,	MAXBLOCKS*sizeof(uint));
	memset( &fs->allocated_fds, 0,		FS_MAXOPENFILES*sizeof(fd_t));

	fs->fb_map.data[0]	= 0x04;					/* First four blocks reserved */
	fs->sb.free_blocks_base	= 4;					/* Start allocating from 5th block */
	fs->sb.free_inodes_base	= 1;					/* Start allocating from 2nd inode */
	fs->sb.root		= 0;
	fs->sb_i.nblocks	= sizeof(superblock)/stride + 1; 	/* How many free blocks needed for superblock */
	fs->first_free_fd = 0;

	if (newfs) {
		_balloc(fs, fs->sb_i.nblocks, fs->sb_i.blocks);		/* Allocate n blocks, tell us which we got */
		fs->root = _mkroot(fs, newfs);				/* Setup root dir */

		if (NULL == fs->root) {
			free(fs);
			return NULL;
		}
	}
	return fs;
}

/* Fill the input @param data into the blocks pointed to by
 * @param ino. Start at @param seek_pos. Spill into Indirect block pointers, 
 * doubly-indirected block pointers, and triply-indirected block 
 * pointers as needed.
 */
static int _inode_fill_blocks_from_data(filesystem* fs, inode* ino, size_t seek_pos, char* data) {
	size_t write_cnt = 0;		/* Number of bytes written */
	size_t blocks_written = 0;	/* Number of blocks filled */
	uint indirection = DIRECT;	/* Which kind of blocks we are writing to (direct, indirect)*/
	size_t slen;			/* Size in bytes of the input data */
	
	block_t blk;			/* First block to begin writing at */
	size_t offset;			/* Byte offset in first block to begin writing at */

	size_t ib1 = 0;
	size_t ib2 = 0;
	size_t db = 0;
	size_t i = 0;

	if (NULL == data) return FS_ERR;

	slen = strlen(data);
	offset = seek_pos % stride;
	blk = (block_t) (seek_pos / stride);

	while (write_cnt < slen) {

		size_t write_increment = min(stride - offset, slen - write_cnt);

		if	(blk < MAXBLOCKS_DIRECT)					indirection = DIRECT;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1)			indirection = INDIRECT1;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1 + MAXBLOCKS_IB2)	indirection = INDIRECT2;
		else if (blk < MAXFILEBLOCKS)						indirection = INDIRECT3;
		else									return FS_ERR;

		switch (indirection)
		{
			case DIRECT:		// Start filling in direct blocks
			{
				db = blk;
				if (blocks_written >= ino->ndatablocks) _fs._inode_extend_datablocks(fs, ino, ino->directblocks);
				memcpy(&ino->directblocks[db]->data[offset], &data[write_cnt], write_increment);
				break;
			}
			case INDIRECT1:		// If we used up all the direct blocks, start using singly indirected blocks
			{
				db = blk - MAXBLOCKS_DIRECT;
				if (blocks_written >= ino->ndatablocks) _fs._inode_extend_datablocks(fs, ino, ino->ib1->blocks);
				memcpy(&ino->ib1->blocks[db]->data[offset], &data[write_cnt], write_increment);
				break;
			}
			case INDIRECT2:		// If we used up all the singly indirected blocks, start using doubly indirected blocks
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1;
				ib1 = i / MAXBLOCKS_IB1;
				db = i - MAXBLOCKS_IB1*ib1;

				if (blocks_written >= ino->ndatablocks) _fs._inode_extend_datablocks(fs, ino, ino->ib2->iblocks[ib1]->blocks);
				memcpy(&ino->ib2->iblocks[ib1]->blocks[db]->data[offset], &data[write_cnt], write_increment); 
				break;
			}
			case INDIRECT3: // If we used up all the doubly indirected blocks, start using triply indirected blocks
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1 - MAXBLOCKS_IB2;
				ib2 = i / MAXBLOCKS_IB2;
				ib1 = i / MAXBLOCKS_IB1;
				db = i - MAXBLOCKS_IB1*ib1 - MAXBLOCKS_IB2*ib2;

				if (blocks_written >= ino->ndatablocks) _fs._inode_extend_datablocks(fs, ino, ino->ib3->iblocks[ib2]->iblocks[ib1]->blocks);
				memcpy(&ino->ib3->iblocks[ib2]->iblocks[ib1]->blocks[db]->data[offset], &data[write_cnt], write_increment); 
				break;
			}
			default: return FS_ERR;	/* Out of blocks */
		}

		/* Going to the next block. Reset byte offset to 0. */
		offset++;
		write_cnt++;
		if (blk < (seek_pos + write_cnt) / stride) {
			offset = 0;
			blocks_written++;
			blk++;
		}
	}

	return FS_OK;
}

/* Read data from blocks on disk into the blocks and iblocks of an inode */
static int _inode_fill_blocks_from_disk(inode* ino) {
	block_t blk = 0;
	size_t ib1 = 0;
	size_t ib2 = 0;
	size_t db = 0;
	size_t i = 0;

	size_t nblocks_read = 0;
	size_t nblocks_to_read = ino->ndatablocks;

	uint indirection = DIRECT;


	if (NULL == ino) return FS_ERR;

	while (nblocks_read < nblocks_to_read) {
		blk = (block_t)(ino->ninoblocks + nblocks_read);

		if	(blk < MAXBLOCKS_DIRECT)					indirection = DIRECT;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1)			indirection = INDIRECT1;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1 + MAXBLOCKS_IB2)	indirection = INDIRECT2;
		else if (blk < MAXFILEBLOCKS)						indirection = INDIRECT3;
		else return FS_ERR;

		switch (indirection) {

			case DIRECT:
			{
				db = blk;
				_fs.readblock(&ino->directblocks[db], ino->blocks[blk]);
				break;
			}
			case INDIRECT1:
			{
				db = blk - MAXBLOCKS_DIRECT;
				_fs.readblock(&ino->ib1->blocks[db], ino->blocks[blk]);
				break;
			}
			case INDIRECT2:
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1;
				ib1 = i / MAXBLOCKS_IB1;
				db = i - MAXBLOCKS_IB1*ib1;
				_fs.readblock(	&ino->ib2->iblocks[ib1]->blocks[db],
						ino->blocks[blk]);
				break;
			}
			case INDIRECT3:
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1 - MAXBLOCKS_IB2;
				ib2 = i / MAXBLOCKS_IB2;
				ib1 = i / MAXBLOCKS_IB1;
				db = i - MAXBLOCKS_IB1*ib1 - MAXBLOCKS_IB2*ib2;
				_fs.readblock(	&ino->ib3->iblocks[ib2]->iblocks[ib1]->blocks[db],
						ino->blocks[blk]);
				break;
			}
			default: return FS_ERR;
		}
		nblocks_read += 1;
	}

	return FS_OK;
}

static int _inode_extend_datablocks(filesystem* fs, inode* ino, block** directblocks) {
	size_t i;

	if (FS_ERR == _balloc(fs, MAXBLOCKS_DIRECT, &ino->blocks[ino->nblocks]))
		return FS_ERR; /* Failed */

	if (NULL != ino) {

		/* Allocate memory for the direct blocks */
		for (i = 0; i < MAXBLOCKS_DIRECT; i++) {
			directblocks[i] = (block*)malloc(sizeof(block));
			memset(directblocks[i], 0, BLKSIZE);
		}

		for (i = 0; i < MAXBLOCKS_DIRECT; i++)
		{
			directblocks[i]->num = ino->blocks[i + ino->nblocks];

			if (i+1 < MAXBLOCKS_DIRECT)
				directblocks[i]->next = ino->blocks[i+1 + ino->nblocks];
		}
		ino->nblocks += MAXBLOCKS_DIRECT;
		ino->ndatablocks += MAXBLOCKS_DIRECT;

		_fs.writeblocks(ino, ino->blocks, ino->ninoblocks, sizeof(inode));

		fs->sb.inode_block_counts[ino->num] = ino->nblocks;

		if (FS_ERR == _fs._sync(fs))
			return FS_ERR;
	}
	return FS_OK;
}

/* Read the string data from an array of direct blocks */
static size_t _inode_read_direct_blocks(char* buf, block** blocks, size_t offset, size_t count) {
	uint i;
	size_t read_cnt = 0;
	size_t remainder = count;

	for (i = 0; i < count; i++) {
		if (i == count) break;

		if (NULL == blocks[i])
			return read_cnt;

		memcpy(buf, &blocks[i]->data[offset], min(BLKSIZE, remainder));
		read_cnt += min(BLKSIZE, remainder);
		
		offset = 0; /* Only applies to first read */
		if (read_cnt == count) break;
	}

	buf[read_cnt] = '\0'; /* Null terminator*/

	return read_cnt;
}

/* Read the string data from the direct and indirect blocks of an inode */
static char* _inode_read_data(inode* ino, size_t seek_pos, size_t len) {
	size_t max_seek = 0;
	size_t read_cnt = 0;

	size_t ib1 = 0;
	size_t ib2 = 0;
	size_t i = 0;

	block_t blk;			/* First block to begin writing at */
	size_t offset;			/* Byte offset in first block to begin writing at */
	uint indirection = DIRECT;
	
	char* buf = NULL;
	if (NULL == ino) return NULL;

	max_seek = (ino->ndatablocks)*BLKSIZE;

	buf = (char*)malloc(len);

	offset = seek_pos % stride;
	blk = (block_t)(seek_pos / stride);

	while (read_cnt < len) {
		if (seek_pos >= max_seek || seek_pos + len >= max_seek)
			break; /* Next read would go out-of-bounds */

		if	(blk < MAXBLOCKS_DIRECT)					indirection = DIRECT;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1)			indirection = INDIRECT1;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1 + MAXBLOCKS_IB2)	indirection = INDIRECT2;
		else if (blk < MAXFILEBLOCKS)						indirection = INDIRECT3;
		else break;

		switch (indirection) {

			case DIRECT:
			{
				read_cnt += _inode_read_direct_blocks(&buf[read_cnt], ino->directblocks, offset, min(len - read_cnt, MAXBLOCKS_DIRECT));
				break;
			}
			case INDIRECT1:
			{
				read_cnt += _inode_read_direct_blocks(&buf[read_cnt], ino->ib1->blocks, offset, min(len - read_cnt, MAXBLOCKS_DIRECT));
				break;
			}
			case INDIRECT2:
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1;
				ib1 = i / MAXBLOCKS_IB1;
				read_cnt += _inode_read_direct_blocks(&buf[read_cnt], ino->ib2->iblocks[ib1]->blocks, offset, min(len - read_cnt, MAXBLOCKS_DIRECT));
				break;
			}
			case INDIRECT3:
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1 - MAXBLOCKS_IB2;
				ib2 = i / MAXBLOCKS_IB2;
				ib1 = i / MAXBLOCKS_IB1;
				read_cnt += _inode_read_direct_blocks(&buf[read_cnt], ino->ib3->iblocks[ib2]->iblocks[ib1]->blocks, offset, min(len - read_cnt, MAXBLOCKS_DIRECT));
				break;
			}
			default: break;
		}
		
		/* Going to the next chunk of direct blocks */
		if (blk < (read_cnt + offset) / stride) {
			offset = 0;
			blk += MAXBLOCKS_DIRECT;
		}
		read_cnt++;
	}

	return buf;
}

/* Write the data blocks pointed to by an inode to disk */
static int _inode_commit_data(inode* ino) {
	size_t ib1 = 0;
	size_t ib2 = 0;
	size_t i = 0;
	size_t read_cnt = 0;	/* How many bytes read from blocks */

	block_t blk = 0;	/* ith block being read */
	uint indirection = DIRECT;

	char* buf = NULL;
	buf = (char*)malloc(MAXBLOCKS_DIRECT*BLKSIZE);

	while (blk < ino->ndatablocks) {

		if	(blk < MAXBLOCKS_DIRECT)					indirection = DIRECT;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1)			indirection = INDIRECT1;
		else if (blk < MAXBLOCKS_DIRECT + MAXBLOCKS_IB1 + MAXBLOCKS_IB2)	indirection = INDIRECT2;
		else if (blk < MAXFILEBLOCKS)						indirection = INDIRECT3;
		else break;

		switch (indirection) {

			case DIRECT:
			{
				read_cnt += _inode_read_direct_blocks(buf, ino->directblocks, 0, MAXBLOCKS_DIRECT);
				break;
			}
			case INDIRECT1:
			{
				read_cnt += _inode_read_direct_blocks(&buf[read_cnt], ino->ib1->blocks, 0, MAXBLOCKS_DIRECT);
				break;
			}
			case INDIRECT2:
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1;
				ib1 = i / MAXBLOCKS_IB1;
				read_cnt += _inode_read_direct_blocks(&buf[read_cnt], ino->ib2->iblocks[ib1]->blocks, 0, MAXBLOCKS_DIRECT);
				break;
			}
			case INDIRECT3:
			{
				i = blk - MAXBLOCKS_DIRECT - MAXBLOCKS_IB1 - MAXBLOCKS_IB2;
				ib2 = i / MAXBLOCKS_IB2;
				ib1 = i / MAXBLOCKS_IB1;
				read_cnt += _inode_read_direct_blocks(&buf[read_cnt], ino->ib3->iblocks[ib2]->iblocks[ib1]->blocks, 0, MAXBLOCKS_DIRECT);
				break;
			}
			default: break;
		}

		if (FS_ERR == _fs.writeblocks(buf, &ino->blocks[ino->ninoblocks + blk], MAXBLOCKS_DIRECT, MAXBLOCKS_DIRECT*BLKSIZE))
			return FS_ERR;

		blk += MAXBLOCKS_DIRECT;
	}

	free(buf);
	return FS_OK;
}

/* Read a block from disk */
static int readblock(void* dest, block_t b) {
	if (NULL == fp) return FS_ERR;

	if (0 != fseek(fp, b*BLKSIZE, SEEK_SET))
		return FS_ERR;
	if (1 != fread(dest, BLKSIZE, 1, fp))	// fread() returns 0 or 1
		return FS_ERR;			// Return ok only if exactly one block was read
	return FS_OK;
}

/* Write a block to disk */
static int writeblock(block_t b, size_t size, void* data) {
	if (NULL == fp) return FS_ERR;

	if (0 != fseek(fp, b*BLKSIZE, SEEK_SET))
		return FS_ERR;
	if (1 != fwrite(data, size, 1, fp))	// fwrite() returns 0 or 1
		return FS_ERR;			// Return ok only if exactly one block was written

	return FS_OK;
}

/* Read an arbitary number of blocks from disk. */
static int readirectblocks(void* dest, block_t* blocks, size_t numblocks, size_t type_size) {
	block_t j = blocks[0];

	/* Get strided blocks (more than one block or less than a whole block) */
	if (BLKSIZE != type_size) {
		size_t i;
		block_t k;
		size_t copysize;

		for (i = 0; i < numblocks; i++) {					/* Get root blocks from disk */
			k = j;

			if (0 == k) break;

			copysize = i+1 == numblocks ? type_size % stride : stride;	/* Get last chunk which may only be a partial block */

			if (FS_ERR == _fs.readblock(&block_cache[k], k))
				return FS_ERR;
			if (MAXBLOCKS <= k) return FS_ERR;

			memcpy(&((char*)dest)[i*stride], &block_cache[k].data, copysize);
			j = block_cache[k].next;
		}
	/* Get exactly one block */
	} else if (FS_ERR == _fs.readblock(dest, j))
		return FS_ERR;

	return FS_OK;
}

/* Write an arbitrary number of blocks to disk */
static int writeblocks(void* source, block_t* blocks, size_t numblocks, size_t type_size) {
	block_t j = blocks[0];

	// Split @param source into strides if it will not fit in one block
	if (BLKSIZE != type_size) {
		size_t i; 
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

			if (FS_ERR == _fs.writeblock(j, BLKSIZE, &block_cache[j]))
				return FS_ERR;
			j = block_cache[j].next;
		}

	// Else write @param source to a whole block directly
	} else return _fs.writeblock(j, type_size, source);

	return FS_OK;
}

/* Write the blocks of an inode to disk 
 * Inteded context is a file was just written to,
 * and the changes need to be put on disk. */
static int write_commit(filesystem* fs, inode* ino) {

	/* Write the inode metadata. */
	writeblocks( ino, ino->blocks, ino->ninoblocks, sizeof(inode)); 

	/* Write the data the inode points to. TODO: write iblocks */
	_inode_commit_data(ino);

	return _fs._sync(fs);
}

/* Open a filesystem stored on disk */
static filesystem* _open() {
	filesystem* fs = NULL;
	block_t sb_i_location = 2;

	fs = _init(false);
	if (NULL == fs) return NULL;

	_fs.readblock(&fs->fb_map, 0);
	_fs.readblock(&fs->ino_map, 1);
	_fs.readirectblocks(&fs->sb_i, &sb_i_location, 1, sizeof(superblock_i));
	_fs.readirectblocks(&fs->sb, fs->sb_i.blocks, fs->sb_i.nblocks, sizeof(superblock));

	fs->root = _mkroot(fs, false);

	if (NULL == fs->root || strcmp(fs->root->name,"/")) {	// We determine it's the root by name "/"
		free(fs);
		return NULL;
	}

	return fs;
}

/* Make a brand-new filesystem. Overwrite any previous. */
static filesystem* _mkfs() {
	filesystem *fs = NULL;

	fs = _init(true);
	if (NULL == fs) return NULL;
	
	/* Write root inode to disk. TODO: Need to write iblocks */
	writeblocks( fs->root->ino, fs->root->ino->blocks, fs->root->ino->nblocks, sizeof(inode)); 

	/* Write superblock and other important first blocks */
	if (FS_ERR == _fs._sync(fs)) {
		free(fs);
		return NULL;
	}
	return fs;
}

/* Print a hex dump of a struct, color nonzeros red
 * Can also use hexdump -C filename 
 * or :%!xxd in vim */
static void _print_mem(void const *vp, size_t n)
{
	size_t i;
	unsigned char const* p = (unsigned char const*)vp;

#if defined(_WIN64) || defined(_WIN32)
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	WORD saved_attributes;
	GetConsoleScreenBufferInfo(console, &info);
	saved_attributes = info.wAttributes;
#else
	char* red = "\x1b[31m";
	char* reset = "\x1b[0m";
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

/* Print the sizes of various structs, defines, and fields. */
static void _debug_print() {
	printf("Welcome to the 560 shell by Doug Slater and Chris Craig\n");
	printf("Here is some useful information about the filesystem:\n\n");
	printf("\tsizeof(map): %lu\n", sizeof(map));
	printf("\tsizeof(block): %lu\n", sizeof(block));
	printf("\tsizeof(block->data)): %ld\n", sizeof(((struct block*)0)->data));
	printf("\tsizeof(iblock1): %lu\n", sizeof(iblock1));
	printf("\tsizeof(iblock2): %lu\n", sizeof(iblock2));
	printf("\tsizeof(iblock3): %lu\n", sizeof(iblock3));
	printf("\tsizeof(superblock): %lu\n", sizeof(superblock));
	printf("\tsizeof(superblock_i): %lu\n", sizeof(superblock_i));
	printf("\tsizeof(inode): %lu\n", sizeof(inode));
	printf("\tsizeof(dent): %lu\n", sizeof(dent));
	printf("\tsizeof(dentv): %lu\n", sizeof(dentv));
	printf("\tsizeof(filesystem): %lu\n", sizeof(filesystem));
	printf("\tMAXFILEBLOCKS: %d\n", MAXFILEBLOCKS);
	printf("\tMAX FILE SIZE (KByte): %d\n", MAXFILEBLOCKS*BLKSIZE/1024);
	printf("\tFS_MAXPATHLEN: %d\n", FS_MAXPATHLEN);
	printf("\n");
	fflush(stdout);
}

fs_private_interface const _fs = 
{ 
	/* Path and string management */
	_pathFree, _newPath, _tokenize, _pathFromString, _stringFromPath,		
	_pathSkipLast, _pathGetLast, _pathAppend, _pathTrimSlashes, 
	_getAbsolutePathDV, _getAbsolutePath,
	_strSkipFirst, _strSkipLast, _trim,

	_isNumeric,

	/* Directory management */
	_newd, _newdv,
	_newf, _newfv, 
	_ino_to_dv, _ino_to_fv, _mkroot, 
	_load_file, _unload_file,
	_load_dir, _unload_dir,	
	_new_dir, _new_file,
	_v_attach, _v_detach,

	_get_fd, _free_fd,			/* File descriptors */
	_prealloc, _zero,			/* Native filsystem file allocation */
	_open, _mkfs, _init,			/* Filesystem, opening, creation */
	__balloc, _balloc, _bfree, _newBlock,	/* Block allocation */
	
	/* Inode allocation */
	_new_inode, _free_inode,
	_ialloc, _ifree,

	_inode_fill_blocks_from_data, _inode_fill_blocks_from_disk,
	
	_inode_extend_datablocks, _inode_read_direct_blocks, 
	_inode_read_data, _inode_commit_data,
	_inode_load, _inode_unload,

	/* Reading and writing disk blocks */
	readblock, writeblock,
	readirectblocks, writeblocks,

	/* Write commits, superblock synchronization, tree traversal */
	write_commit, 
	_recurse, _sync, 

	/* Native filesystem interaction */
	_safeopen, _safeclose,

	/* Debug helpers */
	_print_mem, _debug_print
};
