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

version="3.3"
clang_folder="clang-${version}"
branch="release_33"

# This command will fail and abort the script if the folder does not exist.
base_path=$(realpath ${PWD}/"../../../../../out")

clang_path=${base_path}/${clang_folder}

clang_install_folder=${clang_path}/"llvm/Release+Asserts"
clang_binary=${clang_install_folder}/"bin/clang++"
if [ -x ${clang_binary} ]; then
  # The clang binary already exist.
  echo clang ${version} already available.
  exit 0
fi

if [ -d ${clang_path} ]; then
  cat <<EOF
  ERROR: clang ${version} folder ${clang_path}
  already exists, but it does not contain a clang binary.
  Perhaps a previous download was interrupted or failed?
EOF
  exit -1
fi

mkdir ${clang_path}
cd ${clang_path}

logfile=${clang_path}/"build_log.txt"

echo Downloading and compiling clang version ${version} into ${clang_path}
echo Log file can be found at ${logfile}
echo This may take about 15 minutes.

(
  git clone --branch ${branch} http://llvm.org/git/llvm.git
  cd llvm/tools/
  git clone --branch ${branch} http://llvm.org/git/clang.git
  cd ../projects/
  git clone --branch ${branch} http://llvm.org/git/compiler-rt.git
  cd ${clang_path}

  cd llvm
  ./configure --enable-optimized --disable-doxygen --prefix=${PWD}/bin
  make -j12
  cd ${clang_path}

  ls -l ${clang_install_folder}/bin/
  ${clang_install_folder}/bin/clang++ --version
) >${logfile} 2>&1

