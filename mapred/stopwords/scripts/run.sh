hadoop fs -mkdir input
hadoop fs -put ../input/* input
hadoop fs -rmr output
hadoop jar ../src/filterandwordcount.jar edu.utk.eecs.FilterAndWordCount input/* output
rm ../output/part-00000
hadoop fs -get output/part-00000 ../output
python ../src/generate_stopwords.py ../output/part-00000 > ../output/stopwords.txt
hadoop fs -rmr input
