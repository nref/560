#!/usr/local/bin/bash
if [ ! -e ../../stopwords/output/stopwords.txt ]
	then
	  echo "Please run the stopwords map reduce job first"
	else
      hadoop fs -rmr input
      hadoop fs -rmr output
      hadoop fs -rmr other
      hadoop fs -mkdir input # Will be /user/{username}/input
      #hadoop fs -mkdir output # Will be /user/{username}/output
      hadoop fs -mkdir other # Will be /user/{username}/other

      hadoop fs -put ../input/* input
      hadoop fs -put ../../stopwords/output/stopwords.txt other

      hadoop jar ../src/InvertedIndex.jar edu.utk.eecs.InvertedIndex input/*.txt output

      rm ../output/part-00000
      hadoop fs -get output/part--00000 ../output/part-00000
fi
