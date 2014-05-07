Filesystem Lab Submission
===

This the Spring 2014 CS560 filesystem project submission for Doug Slater and Christopher Craig.

Implemented is a basic user-level filesystem and shell. The filesystem exists inside a single file in the native filesystem. The filesystem persists across shell instances. There is no synchronization or permissions. It is intended for a single user in a single concurrent shell instance.

Only text files are supported in the filesystem. Binary files are not supported.

The project is written in C and is cross-platform and tested on OSX 10.9.2 / XCode 5.1.1 Apple LLVM 5.1, Ubuntu 12.04 LTS / gcc-4.6.3, and Windows 8.1 / Visual Studio 2012 Visual C.

We support all required commands: 

mkfs
open filename flag<br>
read fd size<br>
write fd size<br>
seek fd offset<br>
close fd<br>
mkdir name<br>
rmdir name<br>
cd<br>
cd name<br>
link src dst<br>
unlink name<br>
stat name<br>
ls<br>
ls name<br>
cp src dst<br>
tree<br>
tree name<br>
import src dst<br>
export src dst<br>

License is BSD<br>

Doug Slater and Christopher Craig<br>

<a href="mailto:cds@utk.edu">mailto:cds@utk.edu</a><br>
<a href="mailto:ccraig7@utk.edu">mailto:ccraig7@utk.edu</a>

Dr. Qing Cao, Spring 2014

University of Tennessee, Knoxville
