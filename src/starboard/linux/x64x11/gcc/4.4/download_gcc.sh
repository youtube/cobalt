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

# This script downloads and compiles gcc version 4.4.7

set -e
unset GOMA_DIR
export GOMA_DIR

toolchain_name="gcc"
version="4.4.7"
toolchain_folder="x86_64-linux-gnu-${toolchain_name}-${version}"

binary_path="gcc/bin/g++"
build_duration="about 10 minutes"

cd $(dirname $0)
source ../../toolchain_paths.sh
cd ${toolchain_path}

(
  texinfo_install_folder=${toolchain_path}/texinfo
  if [ ! -f ${texinfo_install_folder}/bin/info ]; then
    # Download and compile texinfo
    texinfo_version="texinfo-4.13"
    texinfo_tar_file=${texinfo_version}a.tar.gz
    wget -c https://ftp.gnu.org/gnu/texinfo/${texinfo_tar_file}

    checksum_invalid=$(sha256sum ${texinfo_tar_file} | grep -q -c 1303e91a1c752b69a32666a407e9fbdd6e936def4b09bc7de30f416301530d68)
    if [ $checksum_invalid ]; then echo "Invalid checksum"; exit 1 ; fi
    rm -rf texinfo ${texinfo_version}
    tar -xzf ${texinfo_tar_file}
    cd ${texinfo_version}/
    ./configure --prefix=${texinfo_install_folder}
    make -j$(nproc) && make install
    cd ${toolchain_path}
  fi
  export PATH=${texinfo_install_folder}/bin:${PATH}

  if [ ! -e gmp/lib/libgmp.a ]; then
    gmp_version="gmp-4.3.2"
    gmp_tar_file=${gmp_version}.tar.bz2
    wget -c https://ftp.gnu.org/gnu/gmp/${gmp_tar_file}

    checksum_invalid=$(sha256sum ${gmp_tar_file} | grep -q -c 936162c0312886c21581002b79932829aa048cfaf9937c6265aeaa14f1cd1775)
    if [ $checksum_invalid ]; then echo "Invalid checksum" ; exit 1 ; fi

    rm -rf gmp ${gmp_version}
    tar -xjf ${gmp_tar_file}
    cd ${gmp_version}
    ./configure --disable-shared --enable-static --prefix=${PWD}/../gmp
    make -j$(nproc) && make install
    cd ${toolchain_path}
  fi

  if [ ! -e mpfr/lib/libmpfr.a ]; then
    mpfr_version="mpfr-2.4.2"
    mpfr_tar_file=${mpfr_version}.tar.xz
    wget -c https://ftp.gnu.org/gnu/mpfr/${mpfr_version}.tar.xz

    checksum_invalid=$(sha256sum ${mpfr_tar_file} | grep -q -c d7271bbfbc9ddf387d3919df8318cd7192c67b232919bfa1cb3202d07843da1b)
    if [ $checksum_invalid ]; then echo "Invalid checksum" ; exit 1 ; fi

    rm -rf mpfr ${mpfr_version}
    tar -xJf ${mpfr_version}.tar.xz
    cd ${mpfr_version}
    ./configure --disable-shared --enable-static --with-gmp=../gmp --prefix=${PWD}/../mpfr
    make -j$(nproc) && make install
    cd ${toolchain_path}
  fi

  # Download gcc
  if [ ! -e gcc-${version}/README ]; then
    file="gcc-${version}.tar.bz2"
    wget -c https://ftp.gnu.org/gnu/gcc/gcc-${version}/${file}
    wget -c https://ftp.gnu.org/gnu/gcc/gcc-${version}/${file}.sig
    wget -c https://ftp.gnu.org/gnu/gnu-keyring.gpg
    signature_invalid=`gpg --verify --no-default-keyring --keyring ./gnu-keyring.gpg ${file}.sig`
    if [ $signature_invalid ]; then echo "Invalid signature" ; exit 1 ; fi

    checksum_invalid=$(sha256sum ${file} | grep -q -c 5ff75116b8f763fa0fb5621af80fc6fb3ea0f1b1a57520874982f03f26cd607f)
    if [ $checksum_invalid ]; then echo "Invalid checksum" ; exit 1 ; fi

    rm -rf gcc-${version}
    tar -xjf ${file}
    cd gcc-${version}
    if [ -f ./contrib/download_prerequisites ]; then
      ./contrib/download_prerequisites
    fi

    # Replace 'struct siginfo' with 'siginfo_t' in linux-unwind.h (compilation fix).
    sed -i 's:struct siginfo:siginfo_t:g' ${toolchain_path}/gcc-${version}/gcc/config/i386/linux-unwind.h

    cd ${toolchain_path}
  fi

  # Create clean build folder for gcc
  rm -rf gcc gcc-${version}-build
  mkdir gcc-${version}-build
  cd gcc-${version}-build

  export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/lib32:${toolchain_path}/gmp/lib:${toolchain_path}/mpfr/lib
  export LD_LIBRARY_PATH=$LIBRARY_PATH
  echo LIBRARY_PATH = $LIBRARY_PATH

  export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu
  export CPLUS_INCLUDE_PATH=/usr/include/x86_64-linux-gnu
  export TARGET_SYSTEM_ROOT=/usr/include/x86_64-linux-gnu

  export CC="gcc -fgnu89-inline"
  export CXX="g++ -fgnu89-inline"

  ${toolchain_path}/gcc-${version}/configure \
    --prefix=${toolchain_path}/gcc \
    --disable-multilib \
    --enable-languages=c,c++ \
    --with-gmp=${toolchain_path}/gmp \
    --with-mpfr=${toolchain_path}/mpfr \
    --with-mpfr-lib=${toolchain_path}/mpfr/lib

  make -j$(nproc) && make install
  cd ${toolchain_path}

  ls -l ${toolchain_binary}
  ${toolchain_binary} --version
) >${logfile} 2>&1

