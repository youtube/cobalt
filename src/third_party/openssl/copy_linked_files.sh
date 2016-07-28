#!/bin/bash
if [ `uname` = "Linux" ] ; then
  p=$2
else
  # make sure it picks up the cygwin find, not the ms dos one
  PATH="/usr/local/bin:/usr/bin:$PATH"
  p=`cygpath $2`
fi
mkdir $p
mkdir $p/openssl
# copy only the symbolic links into the shared dir ($2)
find $1 -type l -exec cp -L \{\} $p/openssl \;
