PREFIX="../input"
files=`ls $PREFIX`
for file in $files; do
	/usr/bin/env ruby make_lines.rb "$PREFIX/$file" "$PREFIX/$file.cpy"
done
#rm -r $PREFIX/tmput
