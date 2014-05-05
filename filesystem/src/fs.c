#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"


filesystem* shfs = NULL; /* The current filesystem */

/* Free the memory occupied by a filesystem 
 * TODO: Traverse the whole directory tree and free */
static void destruct() {
	//uint i;
	if (shfs) {

		if (shfs->root) {
			free(shfs->root->ino);

			//for (i = 0; i < shfs->root->nfiles; i++)
			//	free(shfs->root->files[i]);
			//free(shfs->root->files);

			//for (i = 0; i < shfs->root->nlinks; i++)
			//	free(shfs->root->links[i]);
			//free(shfs->root->links);

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
		_fs._pathAppend(abs_path, cur_path);

		for (i = 0; i < rel_path->nfields; i++)
			_fs._pathAppend(abs_path, rel_path->fields[i]);
	}

	path_str = _fs._stringFromPath(abs_path);

	newdir_name = _fs._pathGetLast(abs_path);
	parent_str = _fs._pathSkipLast(abs_path);

	if (NULL != fs.stat(path_str)) 
		return DIREXISTS;				// The requested directory already exists

	parent_inode = fs.stat(parent_str);
	if (NULL == parent_inode)	return BADPATH;
	parent_dv = parent_inode->datav.dir;
	
	// Get parent dir from memory or disk
//	parent_dv = _fs._load_dir(shfs, parent_inode->num);
//	parent_inode->datav.dir = parent_dv;
	
//	if (!parent_inode->v_attached)
//		parent_dv = _fs._ino_to_dv(shfs, parent_inode);	 // Try from memory
//	else parent_dv = parent_inode->datav.dir;

//	if (!parent_inode->v_attached)
//		parent_dv = _fs._load_dir(shfs, parent_dv->ino->num);	// Try from disk

	if (NULL == parent_dv) return NOTONDISK;			// Give up

	new_dv = _fs._new_dir(shfs, parent_dv, newdir_name);
	if (NULL == new_dv) return ERR;
	
//	_fs._unload_dir(shfs, new_dv->ino);
//	_fs._unload_dir(shfs, parent_dv->ino);
	return OK;
}

/*
 * Return the inode of the directory at the path "name"
 */
static inode* stat(char* name) {

	uint i		= 0;
	size_t depth	= 0;			// The depth of the path, e.g. depth of "/a/b/c" is 3
	inode* ino	= NULL;
	fs_path* dPath	= NULL;

	if (NULL == shfs) return NULL;		// No filesystem yet, bail!
	if (NULL == name || 0 == strlen(name))
		return NULL;

	dPath = _fs._tokenize(name, "/");
	depth = dPath->nfields;

	if (depth == 0) return shfs->root->ino;	// Return if we are at the root
	else {					// Else traverse the path, get matching inode
		ino = _fs._stat_recurse(shfs, shfs->root, 0, depth-1, dPath);
		for (i = 0; i < depth; i++) 
			free(dPath->fields[i]);
		return ino;
	}
}

/* Stat using an inode number */
static inode* statI(inode_t num) {
	inode* ino = _fs._inode_load(shfs, num);
	if (NULL == ino) return NULL;
	
	if (!ino->v_attached) {
		if (FS_ERR == _fs._v_attach(shfs, ino)) {
			free(ino);
			return NULL;
		}
	}
	return ino;
}

static void	pathFree(fs_path* p)				{ _fs._pathFree(p); }
static fs_path*	newPath()					{ return _fs._newPath(); }
static fs_path*	tokenize(const char* str, const char* delim)	{ return _fs._tokenize(str, delim); }
static fs_path*	pathFromString(const char* str)			{ return _fs._pathFromString(str); }
static char*	stringFromPath(fs_path*p )			{ return _fs._stringFromPath(p); }
static char*	pathSkipLast(fs_path* p)			{ return _fs._pathSkipLast(p); }
static char*	pathGetLast(fs_path* p)				{ return _fs._pathGetLast(p); }
static int	pathAppend(fs_path* p, const char* str)		{ return _fs._pathAppend(p, str); }
static char*	getAbsolutePathDV(dentv* dv, fs_path* p)	{ return _fs._getAbsolutePathDV(shfs, dv, p); }
static char*	getAbsolutePath(char* current_path, char* next)	{ return _fs._getAbsolutePath(current_path, next); }
static char*	pathTrimSlashes(char* path)			{ return _fs._pathTrimSlashes(path); }
static char*	strSkipFirst(char* cpy)				{ return _fs._strSkipFirst(cpy); }
static char*	strSkipLast(char* cpy)				{ return _fs._strSkipLast(cpy); }
static char*	trim(char* cpy)					{ return _fs._trim(cpy); }
static int	isNumeric(char* str)				{ return _fs._isNumeric(str); }

static void	inodeUnload(inode* ino)				{ _fs._inode_unload(shfs, ino); }
static inode*	inodeLoad(inode_t num)				{ return _fs._inode_load(shfs, num); }

/* Return a file descriptor (just an inode number) corresponding to the file at the path*/
static int open(char* parent_dir, char* name, char* mode) { 
	fs_mode_t mode_i = FS_READ;
	fd_t fd = 0;
	inode* p_ino = NULL;	/* Path inode */
	inode* f_ino = NULL;	/* File inode */
	char* f_path;		/* Fully-qualified path to the file */
	filev* newfv;

	if (!strcmp("r", mode))		mode_i = FS_READ;
	else if (!strcmp("w", mode))	mode_i = FS_WRITE;
	else if (!strcmp("rw", mode))	mode_i = FS_RW;
	else {
		printf("open: Bad mode \"%s\"\n",mode);
		return -1;
	}
	
	p_ino = stat(parent_dir);
	f_path = fs.getAbsolutePath(parent_dir, name);
	f_ino = stat(f_path);

	if (NULL == p_ino) {
		printf("open: Parent of \"%s\" does not exist.\n", name);
		return FS_ERR;
	}
	
	if (FS_FILE == p_ino->mode) {
		printf("open: Parent of \"%s\" is not a directory.\n", name);
		return FS_ERR;
	}

	if (NULL != f_ino && FS_LINK == f_ino->mode) {
		const size_t max_recursion = 8;
		size_t recursion = 0;
		
		/* Follow a path of links to the file */
		while (FS_LINK == f_ino->mode) {
			if (max_recursion == recursion) {
				printf("open: Too many nested links (%zu).\n", recursion);
				return FS_ERR;
			}
			
			if (NULL == f_ino->datav.link) {
				printf("open: Link \"%s\" leads to NULL datav.\n", f_path);
				return FS_ERR;
			}
			
			f_ino = f_ino->datav.link->dest;
			if (!f_ino->v_attached)
				_fs._v_attach(shfs, f_ino);
			++recursion;
		}
		
		if (FS_FILE != f_ino->mode) {
			printf("open: Link \"%s\" does not point to a file.\n", f_path);
			return FS_ERR;
		}
	}

	if (NULL == f_ino) {
		
		if (FS_READ == mode_i) {
			printf("open: File does not exist \"%s\"\n", name);
			return FS_ERR;
		}
		
		if (FS_WRITE == mode_i) {
			newfv = _fs._new_file(shfs, p_ino->datav.dir, name);	// Create file
			
			if (NULL == newfv) return FS_ERR;
			
			f_ino = newfv->ino;
			if (NULL == f_ino) return FS_ERR;
		}
	}
	
	if (NULL == f_ino->datav.file) return FS_ERR;
	
	f_ino->datav.file->mode = mode_i;

	/* Return a file descriptor which indexes to the filev */
	fd = _fs._get_fd(shfs);
	shfs->fds[fd] = f_ino->datav.file;
	
	return fd;
}

static void close(fd_t fd) { 
	inode* ino = NULL;

	if (NULL == shfs) {
		printf("No filesystem.\n");
		return;
	}

	if (fd >= FS_MAXOPENFILES) {
		printf("Invalid file descriptor.\n");
		return;
	}

	if (false == shfs->allocated_fds[fd]) {
		printf("File descriptor \"%d\" not open. \n", fd);
		return; /* fd not allocated */
	}

	if (shfs->fds[fd]) {

		ino = shfs->fds[fd]->ino;
		if (ino->v_attached)
			_fs._v_detach(shfs, ino);
	}
	_fs._free_fd(shfs, fd);
}

static dentv* opendir(char* path) { 
	inode* ino = NULL;
	inode* parent = NULL;
	fs_path* p = NULL;
	
	p = pathFromString(path);

	ino = stat(path);

	if (NULL == ino) {
		if (!strcmp(path, "/"))
			printf("No filesystem.\n");
		else printf("opendir: Could not stat \"%s\"\n", path);
		return NULL;
	}

	if (!strcmp(path, "/")) parent = ino;
//		return ino->datav.dir;
	else	parent = stat(pathSkipLast(p));
	free(p);

	if (NULL == parent) {
		printf("opendir: Could not stat parent directory for \"%s\"\n", path);
		return NULL;
	}

	if (FS_FILE == ino->mode) {
		printf("opendir: Found \"%s\", but it's a file.\n", path);
		return NULL;
	}
	
	if (FS_LINK == ino->mode) {
		const size_t max_recursion = 8;
		size_t recursion = 0;

		/* Follow a path of links */
		while (FS_LINK == ino->mode) {
			if (max_recursion == recursion) {
				printf("opendir: Too many nested links (%zu).\n", recursion);
				return NULL;
			}
			
			if (NULL == ino->datav.link) {
				printf("opendir: Link \"%s\" leads to NULL datav.\n", path);
				return NULL;
			}
			
			ino = ino->datav.link->dest;
			++recursion;
		}
		
		if (NULL== ino) {
			printf("opendir: Link\"%s\" has NULL destination.\n", path);
			return NULL;
		}
		
		if (FS_DIR != ino->mode) {
			printf("opendir: Link \"%s\" does not point to a directory.\n", path);
			return NULL;
		}
		
		if (!ino->v_attached) {
			if (FS_ERR == _fs._v_attach(shfs, ino))
				return NULL;
		}
		
		return ino->datav.dir;
		
	}

	if (NULL == ino->datav.dir) {
		printf("opendir: Could not load \"%s\".\n", path);
		return NULL;
	}
//
//	if (!ino->v_attached) {
//		//printf("opendir: Attaching directory \"%s\".\n", ino->data.dir.name);
//		_fs._v_attach(shfs, ino);
//	}

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

static void rmdir(fd_t fd) { printf("fs_rmdir: %d\n", fd); }

/* Read text from a file
 * @ param fd file descriptor
 * @ param size number of bytes to read from file */
static char* read(fd_t fd, size_t size) {
	//printf("fs_read: %d %d\n", fd, size);
	filev* fv = NULL;
	char* buf;
	
	if (NULL == shfs) {
		printf("No filesystem.\n");
		return NULL;
	}

	if (fd >= FS_MAXOPENFILES) {
		printf("Invalid file descriptor.\n");
		return NULL;
	}

	// Check if the fd has been loaded into the shellfs
	if (false == shfs->allocated_fds[fd]) {
		printf("File descriptor \"%d\" is not open\n", fd);
		return NULL; /* fd not allocated, file not open */
	}

	// Load the file
	fv = shfs->fds[fd];
	
	// Check file mode
	if (fv->mode != FS_READ && fv->mode != FS_RW) {
		printf("File \"%s\" is not opened for reading\n",fv->name);
		return NULL;
	}
	
	/* No need to check file size; _inode_read_data
	 * will read as many are are allocated */
	_fs._inode_fill_blocks_from_disk(fv->ino);
	buf = _fs._inode_read_data(fv->ino, fv->seek_pos, size);
	fv->seek_pos += strlen(buf);

	return buf;
}


/* Write text to a file
 * @param str the string to write
 * @param fd the file descriptor to write to */
static size_t write (fd_t fd, char* str) {
	filev* fv = NULL;
	size_t slen;
	
	if (NULL == shfs) {
		printf("No filesystem.\n");
		return 0;
	}

	if (fd >= FS_MAXOPENFILES) {
		printf("Invalid file descriptor.\n");
		return 0;
	}

	if (false == shfs->allocated_fds[fd]) {
		printf("File descriptor \"%d\" is not open\n", fd);
		return 0; /* fd not allocated, file not open */
	}

	fv = shfs->fds[fd];
	
	if (FS_WRITE != fv->mode) {
		printf("File descriptor \"%d\" is not open for writing.\n", fd);
		return 0;
	}
	
	if (NULL == fv || !fv->ino->v_attached ) { 
		printf("Could not write to file.\n");
		return 0;
	}
	if (NULL == str || '\0' == str[0])
		return 0;

	slen = strlen(str);

	_fs._inode_fill_blocks_from_data(shfs, fv->ino, fv->seek_pos, str);
	fv->seek_pos += slen;

	return slen;
}

/* Sets the offset of the corresponding file
 *  @param fd file descriptor
 *  @param offset the offest of the file */
static void seek(fd_t fd, size_t offset) {
	filev* fv = NULL;
	
	//Check if the file is open (fd is in the allocated
	if (NULL == shfs) {
		printf("No filesystem\n");
		return;
	}
	
	if (fd >= FS_MAXOPENFILES) {
		printf("Invalid file descriptor\n");
		return;
	}
	
	if (false == shfs->allocated_fds[fd]) {
		printf("File descriptor \"%d\" is not open.\n", fd);
		return; /* fd not allocated, file not open */
	}
	
	// Get the file from the file descriptor
	fv = shfs->fds[fd];
	
	// Check that the fd is a file
	if (FS_FILE != fv->ino->mode) {
		printf("File descriptor \"%d\" is not a file.\n", fd);
		return;
	}
	
	fv->seek_pos = offset;
	
	return;
}

/* Creats a hardlink from a src file to a destination
 * @param path of src file
 * @param path of dst link
 */
static int link (char* from, char* to) {
	hlinkv* newlv = NULL;
	fs_path* dst_path = NULL;
	inode* src_ino = NULL;
	inode* p_ino = NULL;
	char* parent = NULL;

	dst_path = fs.pathFromString(to);
	parent = fs.pathSkipLast(dst_path);

	src_ino = stat(from);
	p_ino = stat(parent);

	newlv = _fs._new_link(shfs, p_ino->datav.dir, src_ino, fs.pathTrimSlashes(to));
	if (NULL == newlv) return FS_ERR;

	return FS_OK;
}
static int	ulink	(char* target) {
	printf("fs_unlink: NOT IMPLEMENTED %s\n", target);
	return FS_ERR;
}

/* Exports a file in memory to the filesystem*/
//static void export (inode* src, FILE* dst_fp){
	
	//this is kinda useless
	
	/*
	for(i=0;src->nblocks;++i){
		fwrite(src->blocks[i], BLKSIZE, 1, dst_fp);
	}*/
//}



fs_public_interface const fs = 
{ 
	pathFree, newPath, tokenize, pathFromString, stringFromPath,		/* Path management */
	pathSkipLast, pathGetLast, pathAppend, getAbsolutePathDV, getAbsolutePath, pathTrimSlashes,
	strSkipFirst, strSkipLast, trim, isNumeric,
	
	inodeLoad, inodeUnload,

	destruct, openfs, mkfs, mkdir,
	stat, statI, open, close, opendir, closedir,
	rmdir, read, write, seek,
	link, ulink
};
