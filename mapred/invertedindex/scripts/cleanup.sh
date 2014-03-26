PREFIX="../input"
files=`ls $PREFIX/*.bkup`
rename -f 's/.bkup//' $files
hadoop fs -rmr input
