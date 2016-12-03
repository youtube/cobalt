#!/bin/bash

set -e

DIR=~/signing-work
TARGET_FREE_SPACE=40000000

function get_df(){
    df -k $1 |tail -n 1 | awk '{print $4}'
}

while getopts ":d:s:" opt; do
    case $opt in
        d)
            DIR=$OPTARG
            ;;
        s)
            TARGET_FREE_SPACE=$OPTARG
            ;;
        *)
            echo "Invalid option: -$OPTARG" >&2
            echo "Usage: $0 -d directory_to_clean -s space_needed_in_KB" >&2
            exit 1
            ;;
    esac
done

echo "Directories available:"
echo "========================================================================"
ls -rt $DIR
echo "========================================================================"

for dir in $(ls -rt $DIR | grep -v removed_binaries); do
    curr_df=$(get_df $DIR)
    if [ $curr_df -lt $TARGET_FREE_SPACE ]; then
        echo "Do you really want to remove binaries from $dir? [y/n]" >&2
        read yesno
        if [ "$yesno" = "y" ] || [ "$yesno" = "Y" ]; then
            echo "Removing binaries from $dir..." >&2
            for d in $(ls -d $DIR/$dir/*signed-build* $DIR/$dir/signed $DIR/$dir/*.tar 2>/dev/null); do
                rm -rf $d
            done
            echo "Moving $DIR/$dir to $DIR/removed_binaries..." >&2
            mkdir -p $DIR/removed_binaries
            mv $DIR/$dir $DIR/removed_binaries
        else
            echo "Skipping $dir..." >&2
        fi
    else
        echo "Free space required : $TARGET_FREE_SPACE KB"
        echo "Free space available: $curr_df KB"
        exit 0
    fi
done

curr_df=$(get_df $DIR)
if [ $curr_df -lt $TARGET_FREE_SPACE ]; then
    echo "Free space required : $TARGET_FREE_SPACE KB"
    echo "Free space available: $curr_df KB"
    echo "Remove unneeded files manually"
    exit 1
fi
