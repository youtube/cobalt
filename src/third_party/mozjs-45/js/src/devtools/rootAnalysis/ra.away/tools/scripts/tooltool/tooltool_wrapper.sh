#!/bin/sh

set -e

TT_MANIFEST=$1
TT_BASEURL=$2
TT_BOOTSTRAP=$3
TT_CMD=$4

# shift so "$@" is now TT_CMD and all extra parameters
shift 3

if [ -e "$TT_MANIFEST" ]; then

    if [ ! -x "$TT_CMD" ] && ! which "$TT_CMD" >/dev/null 2>&1; then
        echo "Cannot execute $TT_CMD, exiting"
        exit 1
    fi

    echo "$TT_MANIFEST content"
    echo "======================================================="
    cat "$TT_MANIFEST"
    echo "======================================================="
    echo "Fetching..."
    python $(cd $(dirname $0) && pwd)/../../buildfarm/utils/retry.py -- \
      "$@" --url "$TT_BASEURL" --overwrite -m "$TT_MANIFEST" fetch ${TOOLTOOL_CACHE:+ -c ${TOOLTOOL_CACHE}}
    if [ -e "$TT_BOOTSTRAP" ]; then
        echo "Bootstraping..."
        bash -xe "$TT_BOOTSTRAP"
    fi
else
    echo "$TT_MANIFEST doesn't exist, skipping..."
fi
