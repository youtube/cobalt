#!/bin/bash
set -e
SCRIPTS_DIR="$(dirname $0)/../.."

if [ -z "$HG_REPO" ]; then
    export HG_REPO="https://hg.mozilla.org/projects/nanojit-central"
fi

python $SCRIPTS_DIR/buildfarm/utils/hgtool.py $HG_REPO nanojit-src || exit 2

(cd nanojit-src; autoconf) || exit 2

if [ ! -d nanojit-build ]; then
    mkdir nanojit-build
fi
cd nanojit-build
../nanojit-src/configure || exit 2
make || exit 2
make check || exit 1
