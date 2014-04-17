#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

/* Free the memory occupied by a filesystem 
 * TODO: Traverse the whole directory tree and free
 */
static void fs_delete(filesystem *fs) {
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

static filesystem* fs_mkfs() {
	filesystem *fs = NULL;

	fs = _fs._init(true);
	if (NULL == fs) return NULL;
	
	// TODO: Check that writes are not redundant

	/* Write root inode to disk. TODO: Need to write iblocks */
	_fs._writeblockstodisk( fs->root->ino, fs->root->ino->blocks, fs->root->ino->nblocks, sizeof(inode)); 

	/* Write superblock and other important first blocks */
	if (FS_ERR == _fs._sync(fs)) {
		free(fs);
		return NULL;
	}

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", fs->root->name, BLKSIZE*MAXBLOCKS/1024);
	return fs;
}

static int fs_mkdir(filesystem *thefs, char* cur_path, char* dir_name) {
	dentv* cur_dv = NULL, *new_dv = NULL;
	char path[FS_MAXPATHLEN] = "";

	inode* current_inode;

	current_inode = fs.stat(thefs, cur_path);
	if (NULL == current_inode)	return BADPATH;
	
	if (strlen(cur_path) + strlen(dir_name) + 1 >= FS_MAXPATHLEN)
		return BADPATH;

	strcat(path, cur_path);
	if (path[strlen(path)-1] != '/')
		strcat(path, "/");				// Append path separator if it's not already there
	strcat(path, dir_name);
	if (NULL != fs.stat(thefs, path)) return DIREXISTS;	// The requested directory already exists

	// Get parent dir from memory or disk
	cur_dv = current_inode->datav.dir;
	if (NULL == cur_dv)
		cur_dv = _fs._load_dir(thefs, current_inode->num);	
	if (NULL == cur_dv) return NOTONDISK;
	
	new_dv = _fs._new_dir(thefs, cur_dv, dir_name);
	if (NULL == new_dv) return FS_ERROR;

	return OK;
}

/*
 * Return the inode of the directory at the path "name"
 */
static inode* stat(filesystem* fs, char* name) {

	uint i		= 0;
	uint depth	= 0;			// The depth of the path, e.g. depth of "/a/b/c" is 3
	inode* ino	= NULL;
	fs_path* dPath	= NULL;

	if (NULL == fs) return NULL;		// No filesystem yet, bail!

	dPath = _fs._tokenize(name, "/");
	depth = dPath->nfields;

	if (depth == 0) return fs->root->ino;	// Return if we are at the root
	else {					// Else traverse the path, get matching inode
		ino = _fs._recurse(fs, fs->root, depth, &dPath->fields[depth-1]);	
		for (i = 0; i < depth; i++) 
			free(dPath->fields[i]);
		return ino;
	}
	return NULL;
}

static int	fs_open		(char* filename, char* mode) { return 0; }
static void	fs_close	(int fd) { }
static void	fs_rmdir	(int fd) { }
static char*	fs_read		(int fd, int size) { return NULL; }
static void	fs_write	(int fd, char* string) { }
static void	fs_seek		(int fd, int offset) { }
static void	fs_link		(inode_t from, inode_t to) { }
static void	fs_unlink	(inode_t ino) { }

fs_public_interface const fs = 
{ 
	fs_delete, fs_mkfs, fs_mkdir,
	stat, fs_open, fs_close, 
	fs_rmdir, fs_read, fs_write, fs_seek,
	fs_link, fs_unlink
};
