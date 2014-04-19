#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"


filesystem* shfs = NULL;

/* Free the memory occupied by a filesystem 
 * TODO: Traverse the whole directory tree and free */
static void destruct() {
	if (shfs) {

		if (shfs->root) {
			free(shfs->root->ino);
			free(shfs->root->files);
			free(shfs->root->links);
			if (shfs->root->head)
				free(shfs->root->head);

			free(shfs->root);
		}
		free(shfs);
	}
}

/* Open an existing filesystem on disk */
static void openfs() { shfs = _fs._open(); }

/* Make a new filesystem, write it to disk, and keep an open pointer */
static void mkfs() { 
	destruct();
	shfs =  _fs._mkfs(); 
}

/* cur_path should always be an absolute path; 
 * dir_path can be relative to cur_path */
static int mkdir(char* cur_path, char* dir_path) {
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

	if (NULL != fs.stat(path_str)) 
		return DIREXISTS;				// The requested directory already exists

	parent_inode = fs.stat(parent_str);
	if (NULL == parent_inode)	return BADPATH;

	// Get parent dir from memory or disk
	if (!parent_inode->v_attached)
		parent_dv = _fs._ino_to_dv(shfs, parent_inode);	 // Try from memory
	else parent_dv = parent_inode->datav.dir;

	if (!parent_inode->v_attached)
		parent_dv = _fs._load_dir(shfs, parent_dv->ino->num);	// Try from disk

	if (NULL == parent_dv) return NOTONDISK;			// Give up

	new_dv = _fs._new_dir(shfs, parent_dv, newdir_name);
	if (NULL == new_dv) return FS_ERROR;

	return OK;
}

/*
 * Return the inode of the directory at the path "name"
 */
static inode* stat(char* name) {

	uint i		= 0;
	uint depth	= 0;			// The depth of the path, e.g. depth of "/a/b/c" is 3
	inode* ino	= NULL;
	fs_path* dPath	= NULL;

	if (NULL == shfs) return NULL;		// No filesystem yet, bail!

	dPath = _fs._tokenize(name, "/");
	depth = dPath->nfields;

	if (depth == 0) return shfs->root->ino;	// Return if we are at the root
	else {					// Else traverse the path, get matching inode
		ino = _fs._recurse(shfs, shfs->root, 0, depth-1, dPath->fields);	
		for (i = 0; i < depth; i++) 
			free(dPath->fields[i]);
		return ino;
	}
	return NULL;
}

static void	pathFree(fs_path* p)				{ _fs._pathFree(p); }
static fs_path*	newPath()					{ return _fs._newPath(); }
static fs_path*	tokenize(const char* str, const char* delim)	{ return _fs._tokenize(str, delim); }
static fs_path*	pathFromString(const char* str)			{ return _fs._pathFromString(str); }
static char*	stringFromPath(fs_path*p )			{ return _fs._stringFromPath(p); }
static char*	pathSkipLast(fs_path* p)			{ return _fs._pathSkipLast(p); }
static char*	pathGetLast(fs_path* p)				{ return _fs._pathGetLast(p); }
static int	pathAppend(fs_path* p, const char* str)		{ return _fs._path_append(p, str); }
static char*	getAbsolutePathDV(dentv* dv, fs_path* p)	{ return _fs._getAbsolutePathDV(dv, p); }
static char*	getAbsolutePath(char* current_path, char* next)	{ return _fs._getAbsolutePath(current_path, next); }
static char*	pathTrimSlashes(char* path)			{ return _fs._pathTrimSlashes(path); }
static char*	strSkipFirst(char* cpy)				{ return _fs._strSkipFirst(cpy); }
static char*	strSkipLast(char* cpy)				{ return _fs._strSkipLast(cpy); }

/* Return a file descriptor (just an inode number) corresponding to the file at the path*/
static filev* open(char* path, char* mode) { 

	printf("open: %s %s\n", path, mode);
	return 0; 
}

static void close (filev* fv) { 

	printf("close: %s\n", fv->name); 
}

static dentv* opendir(char* path) { 
	inode* ino = NULL;
	inode* parent = NULL;
	fs_path* p = NULL;

	p = pathFromString(path);

	ino = stat(path);
	parent = stat(pathSkipLast(p));
	
	free(p);

	if (NULL == parent) {
		printf("opendir: Could not stat parent directory for \"%s\"\n", path);
		return NULL;
	}

	if (NULL == ino) {
		printf("opendir: Could not stat \"%s\"\n", path);
		return NULL;
	}

	if (FS_DIR != ino->mode) {
		printf("opendir: Found \"%s\", but it's not a directory.\n", path);
		return NULL;
	}

	if (NULL == ino->datav.dir) {
		printf("opendir: Could not load \"%s\".\n", path);
		return NULL;
	}

	if (!ino->v_attached) {
		//printf("opendir: Attaching directory \"%s\".\n", ino->data.dir.name);
		_fs._v_attach(shfs, ino);
	}

	ino->datav.dir->parent = parent;

	return ino->datav.dir; 
}

static void closedir (dentv* dv) { 
	if (NULL == dv) return;

	/* Only free dir if it's not the root */
	if (strcmp(dv->name, "/")) {
		//printf("closedir: Detaching directory \"%s\".\n", dv->name);
		_fs._v_detach(shfs, dv->ino);
		dv = NULL;
	}
}


static void	rmdir		(int fd) { printf("fs_rmdir: %d\n", fd); }
static char*	read		(int fd, int size) { printf("fs_read: %d %d\n", fd, size); return NULL; }
static void	write		(filev* fv, char* str) { printf("fs_write: %s %s\n", fv->name, str);}
static void	seek		(int fd, int offset) { printf("fs_seek: %d %d\n", fd, offset); }
static void	link		(inode_t from, inode_t to) { printf("fs_link: %d %d\n", from, to); }
static void	ulink		(inode_t ino) { printf("fs_unlink: %d\n", ino); }

fs_public_interface const fs = 
{ 
	pathFree, newPath, tokenize, pathFromString, stringFromPath,		/* Path management */
	pathSkipLast, pathGetLast, pathAppend, getAbsolutePathDV, getAbsolutePath, pathTrimSlashes,
	strSkipFirst, strSkipLast, 

	destruct, openfs, mkfs, mkdir,
	stat, open, close, opendir, closedir, 
	rmdir, read, write, seek,
	link, ulink
};
