hadoop fs -mkdir /user/cslater3/input
hadoop fs -put ./input/* /user/cslater3/input
hadoop fs -rmr /user/cslater3/output
hadoop jar filterandwordcount.jar edu.utk.eecs.FilterAndWordCount /user/cslater3/input/* /user/cslater3/output > stopwords.txt
hadoop fs -rmr /user/cslater3/input