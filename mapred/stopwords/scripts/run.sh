hadoop fs -mkdir /user/ccraig7/input
hadoop fs -put ../input/* /user/ccraig7/input
hadoop fs -rmr /user/ccraig7/output
hadoop jar ../src/filterandwordcount.jar edu.utk.eecs.FilterAndWordCount /user/ccraig7/input/* /user/ccraig7/output
rm ../output/part-00000
hadoop fs -get /user/ccraig7/output/part-00000 ../output
python ../src/generate_stopwords.py ../output/part-00000 > ../output/stopwords.txt
hadoop fs -rmr /user/ccraig7/input
