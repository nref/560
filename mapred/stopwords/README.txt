To build everythin in this directory:

./build.sh

To run everything in this directory on MapReduce:

./run.sh

To view results:

./result.sh

To do all of these:

./do.sh

Source files: 
FilterAndWordCount.java - Given a directory, count all the words in all of the files. This is a MapReduce job.


generate_stopwords.py

Given a 2-column tab-separated text file, output all words on stdout which are > 3 std devs above the mean, i.e.:

a	50
I	45
government	1

will output "government\n" on stdout
