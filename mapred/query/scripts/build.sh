javac -classpath ~/hdc/hadoop-core-1.2.1.jar -d ../src/Query_classes ../src/Query.java
jar -cvf ../src/Query.jar -C ../src/Query_classes/ .