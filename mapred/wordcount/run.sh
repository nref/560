hadoop fs -put ./input /user/cslater3/input
hadoop fs -put stopwords.txt /user/cslater3/
hadoop fs -rmr /user/cslater3/output
hadoop jar wordcount.jar edu.utk.eecs.WordCount /user/cslater3/input/* /user/cslater3/output
hadoop fs -rmr input