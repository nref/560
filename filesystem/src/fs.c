#include <fcntl.h>
#include <stdio.h>
#include "fs.h"

static char* file = "fs";

void mkfs() {
    
	FILE* fp = fopen(file, "w");
    
// Preallocate a contiguous file
#ifdef __APPLE__
    fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, BLKSIZE*NBLOCKS};
    int OK = fcntl((int)fp, F_PREALLOCATE, &store);
#else
	int OK = posix_fallocate(fp, 0, BLKSIZE*NBLOCKS); // Allocate space for the entire filesystem
#endif
    
	for (int i = 0; i < NBLOCKS; i++) {
		fseek(fp, BLKSIZE*i, SEEK_SET);
        
        char out[BLKSIZE];
        sprintf(out, "-block %d-", i);
		fputs(out, fp);
	}
}