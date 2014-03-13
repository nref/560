hadoop fs -mkdir /user/cslater3/input
hadoop fs -put ../input/* /user/cslater3/input
hadoop fs -rmr /user/cslater3/output
hadoop jar ../src/filterandwordcount.jar edu.utk.eecs.FilterAndWordCount /user/cslater3/input/* /user/cslater3/output
hadoop fs -get /user/cslater3/output/part-00000 ../output
python ../src/generate_stopwords.py ../output/part-00000 > stopwords.txt
hadoop fs -rmr /user/cslater3/input