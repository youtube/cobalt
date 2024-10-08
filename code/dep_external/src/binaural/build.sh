# Copyright (c) 2024, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
#

set -e

PROGDIR=`pwd`
VISR_DIR="$( cd "$(dirname "$0")" ; pwd -P )/visr"
BEAR_DIR="$( cd "$(dirname "$0")" ; pwd -P )/bear"
RESONANCE_DIR="$( cd "$(dirname "$0")" ; pwd -P )/resonance-audio"
EXTERNALS_DIR="$( cd "$(dirname "$0")" ; pwd -P )/../.."
IAMF2BEAR_DIR="$( cd "$(dirname "$0")" ; pwd -P )/iamf2bear"
IAMF2RESONANCE_DIR="$( cd "$(dirname "$0")" ; pwd -P )/iamf2resonance"


declare -a CONFIG_FLAGS_BEAR
declare -a CONFIG_FLAGS_BEAR2
declare -a CONFIG_FLAGS_VISR

CONFIG_FLAGS_BEAR="-DBUILD_PYTHON_BINDINGS=OFF"
CONFIG_FLAGS_BEAR2="$VISR_DIR/build/cmake"
CONFIG_FLAGS_VISR="-DBUILD_PYTHON_BINDINGS=OFF -DBUILD_DOCUMENTATION=OFF -DBUILD_AUDIOINTERFACES_PORTAUDIO=OFF -DBUILD_USE_SNDFILE_LIBRARY=OFF"

#Delete old libraries
rm -rf $EXTERNALS_DIR/lib/binaural/*

CLEAN=yes
DOWNLOAD=yes

if [ $CLEAN = yes ] ; then
	echo "Cleaning: $VISR_DIR"
	rm -f -r $VISR_DIR

	echo "Cleaning: $BEAR_DIR"
	rm -f -r $BEAR_DIR

	echo "Cleaning: $RESONANCE_DIR"
	rm -f -r $RESONANCE_DIR
  [ "$DOWNLOAD" = "yes" ] || exit 0
fi

cd $PROGDIR
git clone -b visr --single-branch https://github.com/ebu/bear.git visr
echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>VISR Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $VISR_DIR
sed -i '1 i set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)' src/libefl/CMakeLists.txt
rm -f -r build
cmake -B build . $CONFIG_FLAGS_VISR  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=NEVER
cmake --build build
cmake --install build --prefix $IAMF2BEAR_DIR

cd $PROGDIR
git clone https://github.com/ebu/bear
echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>BEAR Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $BEAR_DIR
git submodule update --init --recursive
cd visr_bear
rm -f -r build
cmake -B build . $CONFIG_FLAGS_BEAR -DVISR_DIR=$CONFIG_FLAGS_BEAR2
cmake --build build --clean-first
cmake --install build --prefix $IAMF2BEAR_DIR


echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>iamf2bear Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $IAMF2BEAR_DIR
cmake . -DCMAKE_INSTALL_PREFIX=$EXTERNALS_DIR
make
make install

cd $PROGDIR
git clone https://github.com/resonance-audio/resonance-audio
echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>Resonance Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $RESONANCE_DIR
./third_party/clone_core_deps.sh
rm -rf third_party/eigen
cp -rf ../bear/visr_bear/submodules/libear/submodules/eigen third_party/
./build.sh -t=RESONANCE_AUDIO_API -p=Debug
cp -rf install/resonance_audio $IAMF2RESONANCE_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>iamf2resonance Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $IAMF2RESONANCE_DIR
cmake . -DCMAKE_INSTALL_PREFIX=$EXTERNALS_DIR
make
make install


