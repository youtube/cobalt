#!/bin/bash
# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

toolchain_name="gcc"
version="6.3.0"
toolchain_folder="x86_64-linux-gnu-${toolchain_name}-${version}"

binary_path="gcc/bin/g++"
build_duration="about 40 minutes"

scriptfolder=$(dirname $(realpath $0))
cd ${scriptfolder}
source ../../toolchain_paths.sh

(
  # Download gcc
  if [ ! -e gcc-${version}/"README" ]; then
    file="gcc-${version}.tar.bz2"
    wget -c http://ftpmirror.gnu.org/gcc/gcc-${version}/${file}
    wget -c http://ftpmirror.gnu.org/gcc/gcc-${version}/${file}.sig
    wget -c http://ftp.gnu.org/gnu/gnu-keyring.gpg
    signature_invalid=`gpg --verify --no-default-keyring --keyring ./gnu-keyring.gpg ${file}.sig`
    if [ $signature_invalid ]; then
      echo "Invalid signature for " $file
      exit 1
    fi
    echo "f06ae7f3f790fbf0f018f6d40e844451e6bc3b7bc96e128e63b09825c1f8b29f  gcc-6.3.0.tar.bz2" | sha256sum -c || (
      echo "Invalid Checksum for " $file
      exit 1
    )
    rm -rf gcc-${version}
    tar -xjf ${file}
    cd gcc-${version}

    if [ -f ./contrib/download_prerequisites ]; then
      ./contrib/download_prerequisites
    fi

    cat <<EOF  | sha256sum -c || ( echo "Invalid Checksum for prerequisites"; exit 1 )
c7e75a08a8d49d2082e4caee1591a05d11b9d5627514e678f02d66a124bcf2ba  mpfr-2.4.2.tar.bz2
936162c0312886c21581002b79932829aa048cfaf9937c6265aeaa14f1cd1775  gmp-4.3.2.tar.bz2
e664603757251fd8a352848276497a4c79b7f8b21fd8aedd5cc0598a38fee3e4  mpc-0.8.1.tar.gz
8ceebbf4d9a81afa2b4449113cee4b7cb14a687d7a549a963deb5e2a41458b6b  isl-0.15.tar.bz2
EOF
    cd ${toolchain_path}
  fi

  # Patches for compiling with gcc 7.2
  patch -p1 <${scriptfolder}/ucontext.diff
  patch -p1 <${scriptfolder}/nosanitizer.diff

  # Create clean build folder for gcc
  rm -rf gcc gcc-${version}-build
  mkdir gcc-${version}-build
  cd gcc-${version}-build

  export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu
  export CPLUS_INCLUDE_PATH=/usr/include/x86_64-linux-gnu

  gcc_install_folder=$(realpath ${PWD}/"..")/"gcc"
  # Configure gcc for installation into ${gcc_install_folder}
  ${toolchain_path}/gcc-${version}/configure \
    --prefix=${gcc_install_folder} \
    --disable-multilib \
    --enable-languages="c,c++"

  # Build and 'install' gcc
  make -j"$(nproc)" && make install-strip
  cd ${toolchain_path}

  ls -l ${toolchain_binary}
  ${toolchain_binary} --version
) >${logfile} 2>&1
