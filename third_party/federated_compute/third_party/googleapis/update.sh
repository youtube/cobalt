#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this file to update the checkout of googleapis for federated compute.
# In order not to check out the whole googleapis repository (which is huge), we
# check out only few required files, and strip them with features not supported
# by Chrome (that are not needed by FCP anyway).

set -e
shopt -s extglob dotglob
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

GOOGLEAPIS_REPO="https://chromium.googlesource.com/external/github.com/googleapis/googleapis"
# Get the revision from README.chromium.
GOOGLEAPIS_REV=$(sed -n 's/^Revision: \(.*\)/\1/p' "${SCRIPT_DIR}/README.chromium")

if [[ -z "${GOOGLEAPIS_REV}" ]]; then
  echo "Error: Could not extract Revision from README.chromium"
  exit 1
fi

echo "Using repo ${GOOGLEAPIS_REPO}"
echo "Using revision ${GOOGLEAPIS_REV}"

GOOGLEAPIS_FILES=(
  "google/longrunning/operations.proto"
  "google/type/date.proto"
  "google/type/datetime.proto"
  "google/type/timeofday.proto"
  "google/rpc/status.proto"
  "google/rpc/code.proto"
  "LICENSE"
)

function check_out() {
  git init
  git remote add upstream ${GOOGLEAPIS_REPO}
  git sparse-checkout init --no-cone
  git sparse-checkout set ${GOOGLEAPIS_FILES[*]}
  git fetch --depth 1 upstream "${GOOGLEAPIS_REV}"
  git checkout FETCH_HEAD 2>/dev/null
}

function apply_patches() {
  for patch in ../patches/*; do
    if [[ ! -f "$patch" ]]; then
      continue
    fi
    echo Applying patch $patch...
    patch -s -p1 < $patch
  done

  echo Patches applied
}

pushd "${SCRIPT_DIR}/"

mkdir .tmp-checkout
pushd .tmp-checkout

check_out
apply_patches

popd # .tmp-checkout

rm -rf src/
mkdir src/
cp -r .tmp-checkout/!(.git) src/
rm -rf .tmp-checkout/

popd # ${SCRIPT_DIR}/
