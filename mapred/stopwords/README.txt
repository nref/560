stopwords
=========
Generate "stop words" given a set of text files. Stop words are words that appear too often, in our case with greater frequency than 3 standard deviations above the mean word frequency.

Instructions
============

1. Put text files to be counted in ./input
2. Build the Java MapReduce job:	
	cd scripts; ./build.sh
3. Run the Java MapReduce job and the python stop word generator:
	cd scripts; ./run.sh
4. View the results:
	cd scripts; ./result.sh
	(or look in ./output)
	
To do steps 2-4 at once:

	cd scripts; ./do.sh