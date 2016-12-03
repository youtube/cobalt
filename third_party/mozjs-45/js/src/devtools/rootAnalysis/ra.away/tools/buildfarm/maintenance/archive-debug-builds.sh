#!/bin/bash

BASE_PATH=/home/ftp/pub/mozilla.org/firefox/tinderbox-builds
ARCHIVE_PATH=/home/ftp/pub/mozilla.org/firefox/nightly
DATE_BASE=$(date +%Y/%m)

DIRS=$(ls $BASE_PATH | grep 'mozilla.*debug$')
for dir in $DIRS
do
  branch=$(echo $dir | cut -d '-' -f1,2)
  builddir="$BASE_PATH/$dir"
  DATE_DIR=$(date +%Y-%m-%d-$branch-debug)
  cd $builddir
  archivedir="$(find . -maxdepth 1 -type d -mtime -1 -name '1?????????' | sort -n | tail -1 | cut -c3-)"
  if [[ -n $archivedir && -d "$builddir/$archivedir" ]]; then
    files="$(find $builddir/$archivedir/ -name jsshell\* -o -name \*.dmg -o -name \*.txt -o -name \*.bz2 -o -name \*.exe)"
    if [ -n "$files" ]; then
      echo "Creating archive directory: $ARCHIVE_PATH/$DATE_BASE/$DATE_DIR"
      mkdir -p "$ARCHIVE_PATH/$DATE_BASE/$DATE_DIR"
      if [ ! -e $ARCHIVE_PATH/$DATE_DIR ]; then
        ln -s $DATE_BASE/$DATE_DIR $ARCHIVE_PATH/$DATE_DIR
      fi
      for file in $files
      do
        echo "Found recent nightly: $file"
        backup=$(basename $file | sed s/en-US\./en-US.debug-/)
        if [ -e $file ]; then
          echo "Copying $file to $ARCHIVE_PATH/$DATE_BASE/$DATE_DIR/$backup"
          cp -a $file "$ARCHIVE_PATH/$DATE_BASE/$DATE_DIR/$backup"
        fi
      done
    fi
  else
    echo "skipping invalid dir"
  fi
done
