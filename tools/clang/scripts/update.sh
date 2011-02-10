#!/bin/bash
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang into third_party/llvm and build it.

CLANG_REVISION=125186

THIS_DIR=$(dirname $0)
LLVM_DIR=$THIS_DIR/../../../third_party/llvm
CLANG_DIR=$LLVM_DIR/tools/clang

# Die if any command dies.
set -e

# Echo all commands.
set -x

# Check out.
svn co --force http://llvm.org/svn/llvm-project/llvm/trunk@$CLANG_REVISION $LLVM_DIR
svn co --force http://llvm.org/svn/llvm-project/cfe/trunk@$CLANG_REVISION $CLANG_DIR

# Build (in a separate directory).
# The clang bots have /usr/local/clang be a symbolic link into this hardcoded
# directory, so if you change it you also need to change these links.
mkdir -p $LLVM_DIR/../llvm-build
cd $LLVM_DIR/../llvm-build
../llvm/configure --enable-optimized
# TODO(thakis): Make this the number of cores (use |sysctl hw.ncpu| on OS X and
#               some grepping of /proc/cpuinfo on linux).
make -j3
cd -
