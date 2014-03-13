javac -classpath ~/hdc/hadoop-core-1.2.1.jar -d ../src/InvertedIndex_classes ../src/InvertedIndex.java
jar -cvf ../src/InvertedIndex.jar -C ../src/InvertedIndex_classes/ .