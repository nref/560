/*
 * Doug Slater and Christopher Craig
 * mailto:cds@utk.edu, mailto:ccraig7@utk.edu
 * CS560 Filesystem Lab submission
 * Dr. Qing Cao
 * University of Tennessee, Knoxville
 */

#include <fcntl.h>
#include <stdio.h>
#include "fs.h"

static char* file = "fs";

void mkfs() {
    
	FILE* fp = fopen(file, "w");
    
/* Preallocate a contiguous file. Differs across platforms */
#ifdef _WIN64
#elseif _WIN32
#elseif __APPLE__
    fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, BLKSIZE*NBLOCKS};
    int OK = fcntl((int)fp, F_PREALLOCATE, &store);
#elseif __unix__
    int OK = posix_fallocate(fp, 0, BLKSIZE*NBLOCKS);
#endif
    
	for (int i = 0; i < NBLOCKS; i++) {
		fseek(fp, BLKSIZE*i, SEEK_SET);
        
        char out[BLKSIZE];
        sprintf(out, "-block %d-", i);
		fputs(out, fp);
	}
}