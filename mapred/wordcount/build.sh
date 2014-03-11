javac -classpath ~/hdc/hadoop-core-1.2.1.jar -d wordcount_classes WordCount.java
jar -cvf wordcount.jar -C wordcount_classes/ .