#!/bin/bash
#
# Print the size differences of data files in two directories.
#
# Note that this tool does not print removed files; only changed and new files.
#
# # Examples
#
# First, you need to create a backup copy of the data files to compare with. For
# example, to compare with `origin/main`:
# ```shell-session
# cd third_party/icu
# git checkout origin/main
# ```
# Then build the data files:
# ```shell-session
# cd scripts
# ./make_data_all.sh
# ```
# You need to keep the `dataout` directory somewhere safe. For example:
# ```shell-session
# mkdir -p ~/icudata/tot
# mv dataout ~/icudata/tot
# ```
# Then change the work directory to what you want to compare.
# For example, to compare with your local branch:
# ```shell-session
# git checkout -f mybranch
# ```
# If there are any source changes, it is often safe to clean the data directory
# before rebuilding the data files.
# ```shell-session
# (cd data; make clean)
# ```
# or if you don't have any uncommitted local changes:
# ```shell-session
# git clean -fd
# ```
# Then rebuild the data files:
# ```shell-session
# ./make_data_all.sh
# ```
#
# You can then compare the two data files:
# ```shell-session
# ./diff_data_all.sh ~/icudata/tot .
# ```

# set -x

if [ $# -lt 3 ];
then
  echo "Usage: "$0" (android|cast|chromeos|common|flutter|flutter_desktop|ios) icubuilddir1 icubuilddir2" >&2
  echo "$0 compare data files of a particular build inside two icu build directory." >&2
  echo "These files were previously archived by backup_outdir in scripts/copy_data.sh." >&2
  echo "The first parameter indicate which build to be compared." >&2
  exit 1
fi

# Helper Functions

size_if_exists() {
  if [ -f "$1" ]; then
    stat --printf="%s" "$1"
  else
    echo 0
  fi
}

# Input Parameters
BUILD=$1
DIR1=$2
DIR2=$3

echo "======================================================="
echo "                ${BUILD} BUILD REPORT"
echo "======================================================="
RESSUBDIR1=`ls ${DIR1}/dataout/${BUILD}/data/out/build`
RESDIR1="${DIR1}/dataout/${BUILD}/data/out/build/${RESSUBDIR1}"
ICUDATA_LST1="${DIR1}/dataout/${BUILD}/data/out/tmp/icudata.lst"
RESSUBDIR2=`ls ${DIR2}/dataout/${BUILD}/data/out/build`
RESDIR2="${DIR2}/dataout/${BUILD}/data/out/build/${RESSUBDIR2}"
ICUDATA_LST2="${DIR2}/dataout/${BUILD}/data/out/tmp/icudata.lst"
SORTED_ICUDATA_LST1=/tmp/${BUILD}1_icudata_lst
SORTED_ICUDATA_LST2=/tmp/${BUILD}2_icudata_lst

sort ${ICUDATA_LST1} >${SORTED_ICUDATA_LST1}
sort ${ICUDATA_LST2} >${SORTED_ICUDATA_LST2}
echo "              ICUDATA.LST DIFFERENCES"
diff -u ${SORTED_ICUDATA_LST1} ${SORTED_ICUDATA_LST2}
echo "-------------------------------------------------------"

echo -n "> Checking and sorting the diff size ."
SIZEFILE=/tmp/${BUILD}size.txt
SIZESORTEDFILE=/tmp/${BUILD}sizesorted.txt
count=0
rm -rf $SIZEFILE
for res in $(cat "$SORTED_ICUDATA_LST1" "$SORTED_ICUDATA_LST2" | sort -u)
do
  # $res may only exist in of the directories
  SIZE1=`size_if_exists "$RESDIR1/$res"`
  SIZE2=`size_if_exists "$RESDIR2/$res"`
  SIZEDIFF=$(($SIZE2 - $SIZE1))
  echo $SIZEDIFF $SIZE1 $SIZE2 $res >> $SIZEFILE
  count=$((count + 1))
  if (( $count % 100 == 0 ))
  then
    echo -n "."
  fi
done

echo "#Increase Old New Res" > $SIZESORTEDFILE
sort -n $SIZEFILE >>$SIZESORTEDFILE

echo ""
echo "-------------------------------------------------------"
echo "              RES SIZE DIFFERENCES"
cat $SIZESORTEDFILE

exit
