Source files
============ 

1. FilterAndWordCount.java
2. generate_stopwords.py

Descriptions
============

1. Given a directory, count all the words in all of the files. This is a MapReduce job.
2. Given a 2-column tab-separated text file, where the first column are words and the second column are word counts, output all words on stdout which are > 3 std devs above the mean, i.e.:

a	50
I	45
government	1

will output "a\nI" on stdout
