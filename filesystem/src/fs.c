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

static char* fname = "fs";

int fs_mkfs() {
   	int i, ERR;
	char buf[BLKSIZE];
	FILE* fp = fopen(fname, "w");
	filesystem* fs;

/* Preallocate a contiguous file. Differs across platforms */
#if defined(_WIN64) || defined(_WIN32)
	LARGE_INTEGER offset;
	offset.QuadPart = BLKSIZE*MAXBLOCKS;
	ERR = SetFilePointerEx(fp, offset, NULL, FILE_BEGIN);
	SetEndOfFile(fp);
#elif __APPLE__	/* __MACH__ also works */
    fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, BLKSIZE*MAXBLOCKS};
    ERR = fcntl(fileno(fp), F_PREALLOCATE, &store);
#elif __unix__
    ERR = posix_fallocate(fp, 0, BLKSIZE*MAXBLOCKS);
#endif
    
	if (ERR < 0) { 
		perror("allocation error");
		return ERR;
	}

	fs = (filesystem*)malloc(sizeof(filesystem));
	fs->superblock = &fs->blocks[0];
	fs->free_block_bitmap = &fs->blocks[1];

	memset(fs->superblock->data, 0, BLKSIZE);				// Zero-out the free block
	memset(fs->free_block_bitmap->data, 0, BLKSIZE);		// Zero-out the free block

	for (i = 0; i < MAXBLOCKS; i++) {
		fseek(fp, BLKSIZE*i, SEEK_SET);

		if (i==0) fputs(fs->superblock->data, fp);
		else if (i==1) fputs(fs->free_block_bitmap->data, fp);
		else {
			sprintf(buf, "-block %d-", i);
			fputs(buf, fp);
		}
	}

	return 0;
}