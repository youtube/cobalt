#!/bin/bash

. ../common/unpack.sh

release=$1

if [ -z "$release" ]
then
  echo "Syntax: $0 <release_dir>"
  exit 1
fi
shift

# Chomp trailing slash (can interfere with cut below)
release=`echo $release | sed -e 's|\/$||'`

# Are we using the new (releases) directory structure? Check using en-US.
newformat=0
if [ -d $release/*/en-US ]; then
  newformat=1
fi

for platform in $@
do
  rm -rf source/*
  # unpack_build platform dir_name pkg_file
  if [ $newformat -eq 1 ]; then
    unpack_build $platform source $release/${platform}/en-US/*.* en-US 1

    # check for read-only files
    find "./source" -not -perm -u=w -exec echo "FAIL read-only file" {} \;

    # Use 'while read' because 'for x in' splits on spaces.
    find $release/$platform -maxdepth 2 -type f | grep -v 'en-US' | \
    while read package; do
      # this cannot be named $locale, because unpack_build will overwrite it
      l=`echo $package | cut -d / -f3`
      rm -rf target/*
      unpack_build $platform target "$package" $l 1
      # check for read-only files
      find "./target" -not -perm -u=w -exec echo "FAIL read-only file" {} \;
      mkdir -p $release/diffs
      diff -Nr source target > $release/diffs/$platform.$l.diff
    done
 
  else 
    unpack_build $platform source $release/*.en-US.${platform}.* en-US 1

    # check for read-only files
    find "./source" -not -perm -u=w -exec echo "FAIL read-only file" {} \;

    for package in `find $release -maxdepth 1 -iname "*.$platform.*" | \
                    grep -v 'en-US'`
    do
      # strip the directory portion
      package=`basename $package`
      # this cannot be named $locale, because unpack_build will overwrite it
      l=`echo $package | sed -e "s/\.${platform}.*//" -e 's/.*\.//'`
      rm -rf target/*
      unpack_build $platform target $release/$package $l 1
      # check for read-only files
      find "./target" -not -perm -u=w -exec echo "FAIL read-only file" {} \;
      mkdir -p $release/diffs
      diff -Nr source target > $release/diffs/$platform.$l.diff
  done

  fi

done

exit 0
