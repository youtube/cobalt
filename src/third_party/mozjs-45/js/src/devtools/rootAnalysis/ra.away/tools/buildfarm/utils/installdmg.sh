#!/bin/bash
# How long to wait before giving up waiting for the mount to fininsh
TIMEOUT=90

# If mnt already exists, then the previous run may not have cleaned up
# properly.  We should try to umount and remove the mnt directory.
if [ -d mnt ]; then
    echo "mnt already exists, trying to clean up"
    hdiutil detach mnt -force
    rm -rdf mnt
fi

# Install an on-exit handler that will unmount and remove the 'mnt' directory
trap "{ if [ -d mnt ]; then hdiutil detach mnt -force; rm -rdfv mnt; fi; }" EXIT

mkdir -p mnt

hdiutil attach -noautoopen -mountpoint ./mnt "$1"
# Wait for files to show up
# hdiutil uses a helper process, diskimages-helper, which isn't always done its
# work by the time hdiutil exits.  So we wait until something shows up in the
# mnt directory.
i=0
while [ "$(echo mnt/*)" == "mnt/*" ]; do
    if [ $i -gt $TIMEOUT ]; then
        echo "No files found, exiting"
        exit 1
    fi
    sleep 1
    i=$(expr $i + 1)
done
# Now we can copy everything out of the mnt directory into the current directory
rsync -a ./mnt/* .
hdiutil detach mnt
rm -rdf mnt
# Sleep for a bit to let messages from diskimage-helper go away
sleep 5
