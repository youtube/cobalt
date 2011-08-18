#!/bin/bash
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang into third_party/llvm and build it.

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BUILD_DIR="${LLVM_DIR}/../llvm-build"
CLANG_DIR="${LLVM_DIR}/tools/clang"
DEPS_FILE="${THIS_DIR}/../../../DEPS"
STAMP_FILE="${LLVM_BUILD_DIR}/cr_build_revision"

# ${A:-a} returns $A if it's set, a else.
LLVM_REPO_URL=${LLVM_URL:-http://llvm.org/svn/llvm-project}

# Die if any command dies.
set -e

# Since people need to run this script anyway to compile clang, let it check out
# clang as well if it's not in DEPS, so that people don't have to change their
# DEPS if they just want to give clang a try.
CLANG_REVISION=$(grep 'clang_revision":' "${DEPS_FILE}" | egrep -o [[:digit:]]+)

# Check if there's anything to be done, exit early if not.
if [ -f "${STAMP_FILE}" ]; then
  PREVIOUSLY_BUILT_REVISON=$(cat "${STAMP_FILE}")
  if [ "${PREVIOUSLY_BUILT_REVISON}" = "${CLANG_REVISION}" ]; then
    echo "Clang already at ${CLANG_REVISION}"
    exit 0
  fi
fi
# To always force a new build if someone interrupts their build half way.
rm -f "${STAMP_FILE}"

if grep -q 'src/third_party/llvm":' "${DEPS_FILE}"; then
  echo LLVM pulled in through DEPS, skipping LLVM update step
else
  echo Getting LLVM r"${CLANG_REVISION}" in "${LLVM_DIR}"
  svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" "${LLVM_DIR}"
fi

if grep -q 'src/third_party/llvm/tools/clang":' "${DEPS_FILE}"; then
  echo clang pulled in through DEPS, skipping clang update step
else
  echo Getting clang r"${CLANG_REVISION}" in "${CLANG_DIR}"
  svn co --force "${LLVM_REPO_URL}/cfe/trunk@${CLANG_REVISION}" "${CLANG_DIR}"
fi

# Echo all commands.
set -x

# Build clang (in a separate directory).
# The clang bots have this path hardcoded in built/scripts/slave/compile.py,
# so if you change it you also need to change these links.
mkdir -p "${LLVM_BUILD_DIR}"
cd "${LLVM_BUILD_DIR}"
if [ ! -f ./config.status ]; then
  ../llvm/configure \
      --enable-optimized \
      --without-llvmgcc \
      --without-llvmgxx
fi

NUM_JOBS=3
if [ "$(uname -s)" = "Linux" ]; then
  NUM_JOBS="$(grep -c "^processor" /proc/cpuinfo)"
elif [ "$(uname -s)" = "Darwin" ]; then
  NUM_JOBS="$(sysctl -n hw.ncpu)"
fi
make -j"${NUM_JOBS}"
cd -

# Build plugin.
# Copy it into the clang tree and use clang's build system to compile the
# plugin.
PLUGIN_SRC_DIR="${THIS_DIR}/../plugins"
PLUGIN_DST_DIR="${LLVM_DIR}/tools/clang/tools/chrome-plugin"
PLUGIN_BUILD_DIR="${LLVM_BUILD_DIR}/tools/clang/tools/chrome-plugin"
rm -rf "${PLUGIN_DST_DIR}"
cp -R "${PLUGIN_SRC_DIR}" "${PLUGIN_DST_DIR}"
rm -rf "${PLUGIN_BUILD_DIR}"
mkdir -p "${PLUGIN_BUILD_DIR}"
cp "${PLUGIN_SRC_DIR}/Makefile" "${PLUGIN_BUILD_DIR}"
make -j"${NUM_JOBS}" -C "${PLUGIN_BUILD_DIR}"

# After everything is done, log success for this revision.
echo "${CLANG_REVISION}" > "${STAMP_FILE}"
