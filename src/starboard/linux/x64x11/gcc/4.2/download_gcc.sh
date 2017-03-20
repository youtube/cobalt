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

# This script downloads and compiles gcc version 4.2.4.

# Note: This gcc version has a quadruplet 'x86_64-unknown-linux-gnu' instead
# of the'x86_64-linux-gnu' triplet. As a result, the compiler flag
# "-isystem /usr/include/x86_64-linux-gnu" must be used when any system
# headers are needed.

set -e

toolchain_name="gcc"
version="4.2.4"
toolchain_folder="x86_64-linux-gnu-${toolchain_name}-${version}"

binary_path="gcc/bin/g++"
build_duration="about 10 minutes"

source ../../toolchain_paths.sh

(
  texinfo_install_folder=${PWD}/"texinfo"
  # Download and compile texinfo
  if [ ! -f texinfo/bin/info ]; then
    texinfo_version="texinfo-4.13"
    wget -c https://ftp.gnu.org/gnu/texinfo/${texinfo_version}a.tar.gz
    rm -rf texinfo ${texinfo_version}
    tar -xzf ${texinfo_version}a.tar.gz
    cd ${texinfo_version}/
    ./configure --prefix=${texinfo_install_folder}
    make -j"$(nproc)" && make install
    cd ${toolchain_path}
  fi
  export PATH=${texinfo_install_folder}/bin:${PATH}

  # Download gcc
  if [ ! -e gcc-${version}/"README" ]; then
    file="gcc-${version}.tar.bz2"
    wget -c https://ftp.gnu.org/gnu/gcc/gcc-${version}/${file}
    wget -c https://ftp.gnu.org/gnu/gcc/gcc-${version}/${file}.sig
    wget -c https://ftp.gnu.org/gnu/gnu-keyring.gpg
    signature_invalid=`gpg --verify --no-default-keyring --keyring ./gnu-keyring.gpg ${file}.sig`
    if [ $signature_invalid ]; then echo "Invalid signature" ; exit 1 ; fi
    rm -rf gcc-${version}
    tar -xjf ${file}
    cd gcc-${version}

    if [ -f ./contrib/download_prerequisites ]; then
      ./contrib/download_prerequisites
    fi
    cd ${toolchain_path}

    # Replace 'struct siginfo' with 'siginfo_t' in linux-unwind.h (compilation fix).
    sed -i 's:struct siginfo:siginfo_t:g' gcc-${version}/gcc/config/i386/linux-unwind.h

    # Apply patch for "Bug 21706 - MAXPATHLEN usage in [gcc]/gcc/tlink.c"
    # from https://gcc.gnu.org/bugzilla/show_bug.cgi?id=21706
    wget -c -O maxpathlen.patch https://gcc.gnu.org/bugzilla/attachment.cgi?id=16643
    patch -d gcc-${version} -p0 <maxpathlen.patch
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

  LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/lib32
  export LIBRARY_PATH

  # Build and 'install' gcc
  make -j"$(nproc)" && make install
  cd ${toolchain_path}

  ls -l ${toolchain_binary}
  ${toolchain_binary} --version
) >${logfile} 2>&1
