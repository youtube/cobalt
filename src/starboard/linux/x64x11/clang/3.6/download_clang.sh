#!/bin/bash
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

# This version of clang depends on libstdc++-7, version 7.4.0.
# The package "libstdc++-7-dev" provides that version.
libstdcxx_version="7.4.0"
libstdcxx_path="libstdc++-7"
libstdcxx_symlink_folder="${libstdcxx_path}/lib/gcc/x86_64-linux-gnu"
symlink_path="${libstdcxx_symlink_folder}/${libstdcxx_version}"

build_duration="about 20 minutes"

scriptfolder=$(dirname $(realpath $0))
cd ${scriptfolder}
source ../../toolchain_paths.sh

(
  # Build a small symlink forest for the clang compiler to point to with the
  # '--gcc-toolchain' parameter to allow it to build against a non-default version
  # of libstdc++.
  mkdir -p "${libstdcxx_symlink_folder}"
  ln -s /usr/include "${libstdcxx_path}"
  ln -s "/usr/lib/gcc/x86_64-linux-gnu/${libstdcxx_version}" ${libstdcxx_symlink_folder}

  git clone --branch ${branch} https://git.llvm.org/git/llvm.git/
  cd llvm/tools/
  git clone --branch ${branch} https://git.llvm.org/git/clang.git/
  cd ../projects/
  git clone --branch ${branch} https://git.llvm.org/git/compiler-rt.git/
  cd ${toolchain_path}

  # Patch for compiling with gcc newer than version 7.
  patch -p1 <${scriptfolder}/hasmd.patch
  patch -p1 <${scriptfolder}/ustat_size.patch
  patch -d llvm/projects/compiler-rt/ -p0 <${scriptfolder}/sigaltstack.patch

  cd llvm
  # Specify a bootstrap compiler that is known to be available.
  CC=gcc CXX=g++ \
  ./configure --enable-optimized --disable-doxygen --prefix=${PWD}/bin
  make -j"$(nproc)"
  cd ${toolchain_path}

  ls -l ${toolchain_binary}
  ${toolchain_binary} --version
) >${logfile} 2>&1
