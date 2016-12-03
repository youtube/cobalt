#!/bin/bash

set -ex

function usage() {
    echo "Usage: $0 [--build=browser|shell] [mozharness args...]"
    exit 1
}

PRODUCT=shell
if [[ $# -eq 0 ]]; then
    : default
elif [[ "$1" = "-h" ]] || [[ "$1" = "--help" ]]; then
    usage
elif [[ "${1#--build=}" = "$1" ]]; then
    : pass through args
else
    PRODUCT="${1#--build=}"
    shift
fi

SRCDIR="$PWD/../../../.."
$SRCDIR/testing/mozharness/scripts/spidermonkey_build.py -c hazards/common.py -c hazards/build_$PRODUCT.py -c developer_config.py --source "$SRCDIR" --no-purge "$@"
