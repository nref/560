#!/usr/local/bin/bash
if [ ! -e ../../stopwords/output/stopwords.txt ]
	then
		echo "Please run the stopwords map reduce program"
fi
hadoop fs -mkdir input # Will be /user/{username}/input
hadoop fs -put ../input/*.txt input
hadoop fs -put ../../stopwords/output/stopwords.txt input
hadoop fs -rmr output # Will be /user/{username}/output
hadoop jar ../src/InvertedIndex.jar edu.utk.eecs.InvertedIndex input/*.txt output
rm ../output/part-00000
hadoop fs -get output/part-00000 ../output/part-00000
hadoop fs -rmr input
#rm ../input/*.cpy

#PREFIX="../input"
#files=`ls $PREFIX/*.bkup`
#rename -f 's/\\.bkup//' $files
#for file in $files; do
	#rename -f 's/(.*).bkup/$1.txt/' $file
#done
