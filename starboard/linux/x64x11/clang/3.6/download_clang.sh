#!/bin/bash
# Copyright 2018 Google Inc. All Rights Reserved.
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

# This script downloads and compiles clang version 3.6.

set -e

toolchain_name="clang"
version="3.6"
toolchain_folder="x86_64-linux-gnu-${toolchain_name}-${version}"
branch="release_36"

binary_path="llvm/Release+Asserts/bin/clang++"
build_duration="about 20 minutes"

cd $(dirname $0)
source ../../toolchain_paths.sh

(
  git clone --branch ${branch} https://git.llvm.org/git/llvm.git/
  cd llvm/tools/
  git clone --branch ${branch} https://git.llvm.org/git/clang.git/
  cd ../projects/
  git clone --branch ${branch} https://git.llvm.org/git/compiler-rt.git/
  cd ${toolchain_path}

  cd llvm
  # Specify a bootstrap compiler that is known to be available.
  CC=gcc CXX=g++ \
  ./configure --enable-optimized --disable-doxygen --prefix=${PWD}/bin
  make -j"$(nproc)"
  cd ${toolchain_path}

  ls -l ${toolchain_binary}
  ${toolchain_binary} --version
) >${logfile} 2>&1
