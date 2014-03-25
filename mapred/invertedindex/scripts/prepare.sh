PREFIX="../input"
files=`ls $PREFIX`
for file in $files; do
	cp "$PREFIX/$file" "$PREFIX/$file.bkup"
	/usr/bin/env ruby make_lines.rb "$PREFIX/$file" "$PREFIX/$file.cpy"
	mv $PREFIX/$file.cpy $PREFIX/$file
done
#rm -r $PREFIX/tmput
