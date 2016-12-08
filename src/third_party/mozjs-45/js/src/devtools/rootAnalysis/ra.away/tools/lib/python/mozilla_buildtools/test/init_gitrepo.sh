#!/bin/bash
# Set up a git repo for testing
dest=$1
if [ -z "$dest" ]; then
    echo You must specify a destination directory 1>&2
    exit 1
fi

rm -rf $dest
mkdir $dest
cd $dest
git init -q

echo "Hello world $RANDOM" > hello.txt
git add hello.txt
git commit -q -m "Adding hello"
git tag TAG1

git checkout -q -b branch2 > /dev/null
echo "So long, farewell" >> hello.txt
git add hello.txt
git commit -q -m "Changing hello on branch"

git checkout -q master
echo "Is this thing on?" >> hello.txt
git add hello.txt
git commit -q -m "Last change on master"
