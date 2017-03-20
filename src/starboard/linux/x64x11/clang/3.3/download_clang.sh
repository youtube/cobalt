#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script downloads and compiles clang version 3.3.

set -e

toolchain_name="clang"
version="3.3"
toolchain_folder="x86_64-linux-gnu-${toolchain_name}-${version}"
branch="release_33"

binary_path="llvm/Release+Asserts/bin/clang++"
build_duration="about 15 minutes"

source ../../toolchain_paths.sh

(
  git clone --branch ${branch} http://llvm.org/git/llvm.git
  cd llvm/tools/
  git clone --branch ${branch} http://llvm.org/git/clang.git
  cd ../projects/
  git clone --branch ${branch} http://llvm.org/git/compiler-rt.git
  cd ${toolchain_path}

  cd llvm
  # Specify a bootstrap compiler that is known to be available.
  CC=clang-3.6 CXX=clang++-3.6 \
  ./configure --enable-optimized --disable-doxygen --prefix=${PWD}/bin
  make -j"$(nproc)"
  cd ${toolchain_path}

  ls -l ${toolchain_binary}
  ${toolchain_binary} --version
) >${logfile} 2>&1
