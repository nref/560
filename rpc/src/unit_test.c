#include <stdio.h>      /* printf, scanf, puts, NULL */
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

char* test_hosts[] = { 	"localhost", 
						"localhost", "127.0.0.1", 
						"localhost", "127.0.0.1", 
						"localhost", "127.0.0.1", 
						"localhost", "127.0.0.1", 
						"localhost", "127.0.0.1", 						
						"localhost", "127.0.0.1", 
						"localhost", "127.0.0.1", 
						"localhost", "127.0.0.1", 
						"google.com", "localhost" };

char* cmds[] = { "transfer", "inquiry", "deposit", "withdraw" };
 
/* http://stackoverflow.com/questions/2509679/ */ 
/* Accepts semi-open interval [min, max) */
int randRange(unsigned int min, unsigned int max) {
  int base_random = rand(); /* in [0, RAND_MAX] */
  if (RAND_MAX == base_random) return randRange(min, max);
  /* now guaranteed to be in [0, RAND_MAX) */
  int range       = max - min,
      remainder   = RAND_MAX % range,
      bucket      = RAND_MAX / range;
  /* There are range buckets, plus one smaller interval
     within remainder of RAND_MAX */
  if (base_random < RAND_MAX - remainder) {
    return min + base_random/bucket;
  } else {
    return randRange (min, max);
  }
}

/* Random double between min and max */
double randRangeD(double min, double max) {
	return (max - min) * ( (double)rand() / (double)RAND_MAX ) + min;
}

int main() {

	int randAcc1;
	int randAcc2; 
	int randNumArgs;
	
	int randCmd;
	int randHost; 
	
	double randAmt;
	
	void* ptrs[] = { &randAcc1, &randAcc2, &randAmt };
	
	srand((unsigned)time(NULL));
	
	for (int i = 0; i < 500; i++) {
		randAcc1 = randRange(998, 1012);
		randAcc2 = randRange(998, 1012);
		randAmt = randRangeD(-50, 1000);
		
		randNumArgs = randRange(0, 3);
		randCmd = randRange(0, 4);
		randHost = randRange(0, 19);

		printf("%s %s %d %d %lf \n", test_hosts[randHost], cmds[randCmd], 
			*(int*)ptrs[0], *(int*)ptrs[1], *(double*)ptrs[2]);
	}

	return 0;
}