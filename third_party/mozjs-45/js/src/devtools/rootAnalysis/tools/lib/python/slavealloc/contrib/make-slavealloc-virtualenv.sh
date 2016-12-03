#! /bin/sh

# This script lives in /tools on the slavealloc machine and is useful for
# checking out a new virtualenv for a new version of the slave allocator.
#
# It is version-controlled in repo build/tools at lib/python/slavealloc/contrib
#
# Use as follows:
#
# check out a revision from hg.m.o/build/tools:
#  ./make-slavealloc-virtualenv.sh $REV
# check out a revision from your user repo
#  ./make-slavealloc-virtualenv.sh $REV $REPO

REV="$1"
[ -z "$REV" ] && { echo "no revision given"; exit 1; }

REPO="$2"
[ -z "$REPO" ] && REPO="https://hg.mozilla.org/build/tools"

cd /tools || exit 1
virtualenv slavealloc-$REV || exit 1
# install slavealloc, using puppet to find any prerequisite packages
./slavealloc-$REV/bin/pip install -e hg+$REPO@$REV#egg=tools \
   --no-index --find-links=http://repos/python/packages/ || exit 1

# Make symlinks to the bugzilla lookup APIs that live under last-job-per-slave.
if [ -d /tools/last-job-per-slave ]; then
  cd /tools/slavealloc-$REV/src/tools/lib/python/slavealloc/www
  ln -s /tools/last-job-per-slave/icons icons
  cd /tools/slavealloc-$REV/src/tools/lib/python/slavealloc/www/js
  ln -s /tools/last-job-per-slave/bugzilla.js bugzilla.js
else
  echo "# /tools/last-job-per-slave not found."
fi

echo "# if you want to make this default:"
echo "cd /tools; rm slavealloc; ln -s slavealloc-$REV slavealloc"
