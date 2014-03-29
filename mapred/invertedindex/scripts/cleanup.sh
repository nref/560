PREFIX="../input"
files=`ls $PREFIX/*.bkup`
ls -1 $PREFIX/*.bkup > /dev/null;
if [ "$?" = "0" ] 
	then
	rename -f 's/.bkup//' $files
	fi
hadoop fs -rmr input
hadoop fs -rmr other 
