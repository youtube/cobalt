#!/bin/bash
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Hacky, primitive testing: This runs the style plugin for a set of input files
# and compares the output with golden result files.

E_BADARGS=65

# Prints usage information.
usage() {
  echo "Usage: $(basename "${0}")" \
    "<Path to the llvm build dir, usually Release+Asserts>"
  echo ""
  echo "  Runs all the libFindBadConstructs unit tests"
  echo ""
}

# Runs a single test case.
do_testcase() {
  local output="$("${CLANG_DIR}"/bin/clang -cc1 \
      -load "${CLANG_DIR}"/lib/libFindBadConstructs.so \
      -plugin find-bad-constructs ${1} 2>&1)"
  local diffout="$(echo "${output}" | diff - "${2}")"
  if [ "${diffout}" = "" ]; then
    echo "PASS: ${1}"
  else
    echo "FAIL: ${1}"
    echo "Output of compiler:"
    echo "${output}"
  fi
}

# Validate input to the script.
if [[ -z "${1}" ]]; then
  usage
  exit ${E_BADARGS}
elif [[ ! -d "${1}" ]]; then
  echo "${1} is not a directory."
  usage
  exit ${E_BADARGS}
else
  echo "Using clang directory ${1}..."
  export CLANG_DIR="${1}"
fi

for input in *.cpp; do
  do_testcase "${input}" "${input%cpp}txt"
done
