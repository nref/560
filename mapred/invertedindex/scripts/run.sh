hadoop fs -mkdir /user/cslater3/input
hadoop fs -put ../input/* /user/cslater3/input
hadoop fs -rmr /user/cslater3/output
hadoop jar ../src/InvertedIndex.jar edu.utk.eecs.InvertedIndex /user/cslater3/input/* /user/cslater3/output
rm ../output/part-00000
hadoop fs -get /user/cslater3/output/part-00000 ../output/part-00000
hadoop fs -rmr /user/cslater3/input