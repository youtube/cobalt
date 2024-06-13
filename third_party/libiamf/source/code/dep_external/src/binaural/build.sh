#BSD 3-Clause Clear License The Clear BSD License

#Copyright (c) 2023, Alliance for Open Media.

#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:

#1. Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.

#2. Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.

#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

rm -rf build
mkdir build
cd build

git clone https://github.com/pybind/pybind11

git clone https://github.com/ebu/bear

git clone https://github.com/resonance-audio/resonance-audio

BOOST_DIR="$( cd "$(dirname "$0")" ; pwd -P )/../boost_1_82_0"
PYBIND_DIR="$( cd "$(dirname "$0")" ; pwd -P )/pybind11"
VISR_DIR="$( cd "$(dirname "$0")" ; pwd -P )/../rd-audio-visr-public-master"
BEAR_DIR="$( cd "$(dirname "$0")" ; pwd -P )/bear"
RESONANCE_DIR="$( cd "$(dirname "$0")" ; pwd -P )/resonance-audio"
BINAURAL_LIB_DIR="$( cd "$(dirname "$0")" ; pwd -P )/../../../lib/binaural"
IAMF2BEAR_DIR="$( cd "$(dirname "$0")" ; pwd -P )/../iamf2bear"
IAMF2RESONANCE_DIR="$( cd "$(dirname "$0")" ; pwd -P )/../iamf2resonance"

declare -a CONFIG_FLAGS_VISR
CONFIG_FLAGS=""
CONFIG_FLAGS_BEAR=""
CONFIG_FLAGS_VISR="-DBUILD_PYTHON_BINDINGS=ON -DBUILD_DOCUMENTATION=OFF -DBUILD_AUDIOINTERFACES_PORTAUDIO=OFF -DBUILD_USE_SNDFILE_LIBRARY=OFF"
CONFIG_FLAGS_VISR2=""
CONFIG_FLAGS_LIB=""

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>boost Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $BOOST_DIR
./bootstrap.sh --with-libraries=filesystem,system,thread --with-toolset=gcc
./b2 toolset=gcc
cp -f stage/lib/libboost_system.so.1.82.0 $BINAURAL_LIB_DIR
cp -f stage/lib/libboost_filesystem.so.1.82.0 $BINAURAL_LIB_DIR
cp -f stage/lib/libboost_thread.so.1.82.0 $BINAURAL_LIB_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>pybind11 Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $PYBIND_DIR
cmake -B build $CONFIG_FLAGS
cd build
make -j12

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>VISR Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $VISR_DIR
sed -i '1 i set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)' src/libefl/CMakeLists.txt
cmake -B build . $CONFIG_FLAGS_VISR
cd build
cmake --build . $CONFIG_FLAGS_VISR2
sudo make install
cp -f lib/libefl.so $BINAURAL_LIB_DIR
cp -f lib/libobjectmodel.so $BINAURAL_LIB_DIR
cp -f lib/libpanning.so $BINAURAL_LIB_DIR
cp -f lib/libpml.so $BINAURAL_LIB_DIR
cp -f lib/librbbl.so $BINAURAL_LIB_DIR
cp -f lib/librcl.so $BINAURAL_LIB_DIR
cp -f lib/librrl.so $BINAURAL_LIB_DIR
cp -f lib/libvisr.so $BINAURAL_LIB_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>BEAR Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $BEAR_DIR
git submodule update --init --recursive 
cd visr_bear
export PYBIND11_DIR=$PYBIND_DIR
cmake -B build . $CONFIG_FLAGS_BEAR
cd build
make -j12
cp -f src/libbear.a $IAMF2BEAR_DIR
cp -f submodules/libear/src/libear.a $IAMF2BEAR_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>resonance Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $RESONANCE_DIR
./third_party/clone_core_deps.sh
rm -rf third_party/eigen
cp -rf ../bear/visr_bear/submodules/libear/submodules/eigen third_party/
./build.sh -t=RESONANCE_AUDIO_API -p=Debug
cp -rf install/resonance_audio $IAMF2RESONANCE_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>libiamf2bear Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $IAMF2BEAR_DIR
cmake . $CONFIG_FLAGS_LIB
make
cp -f libiamf2bear.so $BINAURAL_LIB_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>libiamf2resonance Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $IAMF2RESONANCE_DIR
cmake . $CONFIG_FLAGS_LIB
make
cp -f libiamf2resonance.so $BINAURAL_LIB_DIR