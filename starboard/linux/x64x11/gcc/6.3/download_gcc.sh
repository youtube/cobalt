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

# This script downloads and compiles gcc version 6.3.0.

set -e

version="6.3.0"
gcc_folder="gcc-${version}"

# This command will fail and abort the script if the folder does not exist.
base_path=$(realpath ${PWD}/"../../../../..")

out_path=${base_path}/out
gcc_path=${out_path}/${gcc_folder}

gcc_install_folder=${gcc_path}/gcc
gcc_binary=${gcc_install_folder}/bin/gcc
if [ -x ${gcc_binary} ]; then
  # The gcc binary already exist.
  echo gcc ${version} already available.
  exit 0
fi

if [ -d ${gcc_path} ]; then
  cat <<EOF 1>&2
  ERROR: gcc ${version} folder ${gcc_path}
  already exists, but it does not contain a gcc binary.
  Perhaps a previous download was interrupted or failed?
EOF
  rm -rf ${gcc_path}
fi

mkdir ${gcc_path}
cd ${gcc_path}

logfile=${gcc_path}/"build_log.txt"

cat <<EOF 1>&2
Downloading and compiling gcc version ${version} into ${gcc_path}
Log file can be found at ${logfile}
This may take about 40 minutes.
EOF

(
  # Download gcc
  if [ ! -e gcc-${version}/"README" ]; then
    file="gcc-${version}.tar.bz2"
    wget -c http://ftpmirror.gnu.org/gcc/gcc-${version}/${file}
    wget -c http://ftpmirror.gnu.org/gcc/gcc-${version}/${file}.sig
    wget -c http://ftp.gnu.org/gnu/gnu-keyring.gpg
    signature_invalid=`gpg --verify --no-default-keyring --keyring ./gnu-keyring.gpg ${file}.sig`
    if [ $signature_invalid ]; then echo "Invalid signature" ; exit 1 ; fi
    rm -rf gcc-${version}
    tar -xjf ${file}
    cd gcc-${version}

    if [ -f ./contrib/download_prerequisites ]; then
      ./contrib/download_prerequisites
    fi
    cd ${gcc_path}
  fi

  # Create clean build folder for gcc
  rm -rf gcc gcc-${version}-build
  mkdir gcc-${version}-build
  cd gcc-${version}-build

  export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu
  export CPLUS_INCLUDE_PATH=/usr/include/x86_64-linux-gnu

  gcc_install_folder=$(realpath ${PWD}/"..")/"gcc"
  # Configure gcc for installation into ${gcc_install_folder}
  ${gcc_path}/gcc-${version}/configure \
    --prefix=${gcc_install_folder} \
    --disable-multilib \
    --enable-languages="c,c++"

  # Build and 'install' gcc
  make -j"$(nproc)" && make install-strip
  cd ${gcc_path}

  ls -l gcc/bin
  ./gcc/bin/g++ --version
) >${logfile} 2>&1
