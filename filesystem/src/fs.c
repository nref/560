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
	
	/* Write root inode to disk. TODO: Need to write iblocks */
	_fs.writeblocks( fs->root->ino, fs->root->ino->blocks, fs->root->ino->nblocks, sizeof(inode)); 

	/* Write superblock and other important first blocks */
	if (FS_ERR == _fs._sync(fs)) {
		free(fs);
		return NULL;
	}

	printf("Root is \"%s\". Filesystem size is %d KByte.\n", fs->root->name, BLKSIZE*MAXBLOCKS/1024);
	return fs;
}

/* cur_path should always be an absolute path; dir_path can be relative
 * to cur_dir
 */
static int fs_mkdir(filesystem *thefs, char* cur_path, char* dir_path) {
	dentv* parent_dv = NULL, *new_dv = NULL;

	inode* parent_inode;
	uint i;

	fs_path* abs_path;
	fs_path* rel_path;

	char* path_str;
	char* parent_str;
	char* newdir_name;

	if (strlen(cur_path) + strlen(dir_path) + 1 >= FS_MAXPATHLEN)
		return BADPATH;

	if ('/' != cur_path[0])					// Require leading "/"
		return BADPATH;

	if ('/' == dir_path[0])					// If there's a leading forward slash, we have an abs. path
		abs_path = _fs._pathFromString(dir_path);
	else {							// Have to make abs. path from cur_path + dir_name
		rel_path = _fs._pathFromString(dir_path);

		abs_path = _fs._newPath();
		_fs._path_append(abs_path, cur_path);

		for (i = 0; i < rel_path->nfields; i++)
			_fs._path_append(abs_path, rel_path->fields[i]);
	}

	path_str = _fs._stringFromPath(abs_path);

	newdir_name = _fs._pathGetLast(abs_path);
	parent_str = _fs._pathSkipLast(abs_path);

	if (NULL != fs.stat(thefs, path_str)) 
		return DIREXISTS;				// The requested directory already exists

	parent_inode = fs.stat(thefs, parent_str);
	if (NULL == parent_inode)	return BADPATH;

	// Get parent dir from memory or disk
	if (!parent_inode->v_attached)
		parent_dv = _fs._ino_to_dv(thefs, parent_inode);	 // Try from memory
	else parent_dv = parent_inode->datav.dir;

	if (!parent_inode->v_attached)
		parent_dv = _fs._load_dir(thefs, parent_dv->ino->num);	// Try from disk

	if (NULL == parent_dv) return NOTONDISK;			// Give up

	new_dv = _fs._new_dir(thefs, parent_dv, newdir_name);
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
		ino = _fs._recurse(fs, fs->root, 0, depth-1, dPath->fields);	
		for (i = 0; i < depth; i++) 
			free(dPath->fields[i]);
		return ino;
	}
	return NULL;
}

/* Load a dentv from disk and put it in an inode*/
static int attach_datav(filesystem* fs, inode* ino) {
	return _fs._attach_datav(fs, ino);
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
	stat, attach_datav, fs_open, fs_close, 
	fs_rmdir, fs_read, fs_write, fs_seek,
	fs_link, fs_unlink
};
