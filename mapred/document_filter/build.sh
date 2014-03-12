javac -classpath ~/hdc/hadoop-core-1.2.1.jar -d filterandwordcount_classes FilterAndWordCount.java
jar -cvf filterandwordcount.jar -C filterandwordcount_classes/ .