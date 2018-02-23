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

toolchain_name="gcc"
version="6.3.0"
toolchain_folder="x86_64-linux-gnu-${toolchain_name}-${version}"

binary_path="gcc/bin/g++"
build_duration="about 40 minutes"

cd $(dirname $0)
source ../../toolchain_paths.sh

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
    cd ${toolchain_path}
  fi

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
