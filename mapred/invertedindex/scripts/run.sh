hadoop fs -mkdir input # Will be /user/{username}/input
hadoop fs -put ../input/*.cpy input
hadoop fs -rmr output # Will be /user/{username}/output
hadoop jar ../src/InvertedIndex.jar edu.utk.eecs.InvertedIndex input/*.cpy output
rm ../output/part-00000
hadoop fs -get output/part-00000 ../output/part-00000
hadoop fs -rmr input
rm ../input/*.cpy