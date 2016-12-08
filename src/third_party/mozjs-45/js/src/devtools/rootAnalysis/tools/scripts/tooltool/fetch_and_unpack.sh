#!/bin/sh

set -e

TT_MANIFEST=$1
TT_BASEURL=$2
TT_CMD=$3
TT_BOOTSTRAP=$4

if [ -e "$TT_MANIFEST" ]; then

    if [ ! -x "$TT_CMD" ]; then
        echo "Cannot execute $TT_CMD, exiting"
        exit 1
    else
        if [  ! -z $TOOLTOOL_CACHE ]; then
            TT_CMD="${TT_CMD} -c ${TOOLTOOL_CACHE}"
        fi
    fi

    echo "$TT_MANIFEST content"
    echo "======================================================="
    cat "$TT_MANIFEST"
    echo "======================================================="
    echo "Fetching..."
    python $(cd $(dirname $0) && pwd)/../../buildfarm/utils/retry.py -- \
      "$TT_CMD" --url "$TT_BASEURL" --overwrite -m "$TT_MANIFEST" fetch
    if [ -e "$TT_BOOTSTRAP" ]; then
        echo "Bootstraping..."
        bash -xe "$TT_BOOTSTRAP"
    fi
else
    echo "$TT_MANIFEST doesn't exist, skipping..."
fi
