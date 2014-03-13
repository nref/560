javac -classpath ~/hdc/hadoop-core-1.2.1.jar -d ../src/filterandwordcount_classes ../src/FilterAndWordCount.java
jar -cvf ../src/filterandwordcount.jar -C ../src/filterandwordcount_classes/ .