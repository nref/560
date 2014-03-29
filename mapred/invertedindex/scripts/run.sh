#!/usr/local/bin/bash
if [ ! -e ../../stopwords/output/stopwords.txt ]
	then
	  echo "Please run the stopwords map reduce job first"
	else
      hadoop fs -mkdir input # Will be /user/{username}/input
      hadoop fs -put ../input/*.txt input
	  hadoop fs -mkdir other # Will be /user/{username}/other
      hadoop fs -put ../../stopwords/output/stopwords.txt other
      hadoop fs -rmr output # Will be /user/{username}/output
      hadoop jar ../src/InvertedIndex.jar edu.utk.eecs.InvertedIndex input/*.txt output
      rm ../output/part-\*
      hadoop fs -get output/part-\* ../output/part-\*
      hadoop fs -rmr input
fi
