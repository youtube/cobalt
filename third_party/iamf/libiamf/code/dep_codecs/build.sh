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

run ()
{
    if [ "$VERBOSE" = "yes" ] ; then
        echo "##### NEW COMMAND"
        echo "$@"
        $@ 2>&1
    else
        if [ -n "$TMPLOG" ] ; then
            echo "##### NEW COMMAND" >> $TMPLOG
            echo "$@" >> $TMPLOG
            $@ 2>&1 | tee -a $TMPLOG
        else
            $@ > /dev/null 2>&1
        fi
    fi
}

pattern_match ()
{
    echo "$2" | grep -q -E -e "$1"
}

# Find if a given shell program is available.
#
# $1: variable name
# $2: program name
#
# Result: set $1 to the full path of the corresponding command
#         or to the empty/undefined string if not available
#
find_program ()
{
    eval $1=`command -v $2`
}

prepare_download ()
{
    find_program CMD_WGET wget
    find_program CMD_CURL curl
    find_program CMD_SCRP scp
}

# Download a file with either 'curl', 'wget' or 'scp'
#
# $1: source URL (e.g. http://foo.com, ssh://blah, /some/path)
# $2: target file
download_file ()
{
    # Is this HTTP, HTTPS or FTP ?
    if pattern_match "^(http|https|ftp):.*" "$1"; then
        if [ -n "$CMD_WGET" ] ; then
            run $CMD_WGET -O $2 $1 
        elif [ -n "$CMD_CURL" ] ; then
            run $CMD_CURL -L -o $2 $1
        else
            echo "Please install wget or curl on this machine"
            exit 1
        fi
        return
    fi

    # Is this SSH ?
    # Accept both ssh://<path> or <machine>:<path>
    #
    if pattern_match "^(ssh|[^:]+):.*" "$1"; then
        if [ -n "$CMD_SCP" ] ; then
            scp_src=`echo $1 | sed -e s%ssh://%%g`
            run $CMD_SCP $scp_src $2
        else
            echo "Please install scp on this machine"
            exit 1
        fi
        return
    fi

    # Is this a file copy ?
    # Accept both file://<path> or /<path>
    #
    if pattern_match "^(file://|/).*" "$1"; then
        cp_src=`echo $1 | sed -e s%^file://%%g`
        run cp -f $cp_src $2
        return
    fi
}



OPUS_DIR="$( cd "$(dirname "$0")" ; pwd -P )/opus"
AAC_DIR="$( cd "$(dirname "$0")" ; pwd -P )/aac"
FLAC_DIR="$( cd "$(dirname "$0")" ; pwd -P )/flac"
CODECS_DIR="$( cd "$(dirname "$0")" ; pwd -P )"
OPUS_TAR="opus-1.4.tar.gz"
AAC_TAR="fdk-aac-free-2.0.0.tar.gz"
FLAC_TAR="flac-1.4.2.tar.xz"

declare -a CONFIG_FLAGS_OPUS
declare -a CONFIG_FLAGS_AAC
declare -a CONFIG_FLAGS_FLAC
CONFIG_FLAGS_OPUS="-DCMAKE_C_FLAGS="-fPIC""
CONFIG_FLAGS_AAC="--enable-static --disable-shared --with-pic"
CONFIG_FLAGS_FLAC="-DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DCMAKE_C_FLAGS="-fPIC""

OPUS_DOWNLOAD_LINK="https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz"
AAC_DOWNLOAD_LINK="https://people.freedesktop.org/~wtay/fdk-aac-free-2.0.0.tar.gz"
FLAC_DOWNLOAD_LINK="https://downloads.xiph.org/releases/flac/flac-1.4.2.tar.xz"


function show_help()
{
  cat <<EOF
*** IAMF dependent codec compile script ***

Configuration:
  -h, --help                     Display this help and exit
  --x86_64-mingw_toolchain       Use mingw-gcc X86_64 toolchain.
  --arm64-linux_toolchain        Use Linux Arm64 toolchain.
  --x86-macos_toolchain          Use MacOS X86 toolchain.
  --arm64-ios_toolchain          Use IOS Arm64 toolchain.
EOF
exit
}

ECHO=`which echo`
if test "x$ECHO" = x; then
  ECHO=echo
fi

for i in "$@"
do
  case $i in
    -help | --help | -h)
    want_help=yes ;;

    --x86_64-mingw_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86_64-mingw.cmake -DOPUS_STACK_PROTECTOR=OFF -DCMAKE_C_FLAGS="-fPIC""
      CONFIG_FLAGS_AAC="--host=x86_64-w64-mingw32  --enable-static --disable-shared --with-pic"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86_64-mingw.cmake -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DWITH_STACK_PROTECTOR=OFF  -DCMAKE_C_FLAGS="-fPIC""
      shift # past argument with no value
      ;;

    --arm64-linux_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-linux.cmake -DOPUS_STACK_PROTECTOR=OFF -DCMAKE_C_FLAGS="-fPIC""
      CONFIG_FLAGS_AAC="--host=aarch64-linux-gnu  --enable-static --disable-shared --with-pic"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-linux.cmake -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DWITH_STACK_PROTECTOR=OFF -DCMAKE_C_FLAGS="-fPIC""
      shift # past argument with no value
      ;;

    --x86-macos_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86-macos.cmake -DCMAKE_C_FLAGS="-fPIC""
      CONFIG_FLAGS_AAC="--host=x86_64-apple-darwin20.4  --enable-static --disable-shared --with-pic"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86-macos.cmake -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DCMAKE_C_FLAGS="-fPIC""
      shift # past argument with no value
      ;;
      
    --arm64-ios_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-ios.cmake"
      CONFIG_FLAGS_AAC="--host=aarch64-apple-darwin20.4  --enable-static --disable-shared --with-pic"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-ios.cmake -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DCMAKE_C_FLAGS="-fPIC""
      shift # past argument with no value
      ;;

    *)
      # unknown option
      echo "Unknown option: ${i}"
      show_help
      ;;
  esac
done

if test "x$want_help" = xyes; then
show_help
fi

#Delete old libraries
rm -rf $CODECS_DIR/lib

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>Codec Download<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"

CLEAN=yes
DOWNLOAD=yes

if [ $CLEAN = yes ] ; then
	echo "Cleaning: $OPUS_DIR"
	rm -f -r $OPUS_DIR

	echo "Cleaning: $AAC_DIR"
	rm -f -r $AAC_DIR

	echo "Cleaning: $FLAC_DIR"
	rm -f -r $FLAC_DIR

	mkdir $OPUS_DIR
	mkdir $AAC_DIR
	mkdir $FLAC_DIR
  [ "$DOWNLOAD" = "yes" ] || exit 0
fi

#Download OPUS
if [ ! -f $OPUS_DIR/$OPUS_TAR ]
then
	echo "Downloading opus please wait..."
	prepare_download
	download_file $OPUS_DOWNLOAD_LINK $OPUS_DIR/$OPUS_TAR
fi

if [ ! -f $OPUS_DIR/$OPUS_TAR ]
then
	echo "Failed to download opus! Please download manually\nand save it in this directory as $OPUS_TAR"
	exit 1
fi

if [ -f $OPUS_DIR/$OPUS_TAR ]
then
	echo "Unpacking opus"
	tar -zxf $OPUS_DIR/$OPUS_TAR -C $OPUS_DIR
fi

#Download AAC
if [ ! -f $AAC_DIR/$AAC_TAR ]
then
	echo "Downloading aac please wait..."
	prepare_download
	download_file $AAC_DOWNLOAD_LINK $AAC_DIR/$AAC_TAR
fi

if [ ! -f $AAC_DIR/$AAC_TAR ]
then
	echo "Failed to download aac! Please download manually\nand save it in this directory as $AAC_TAR"
	exit 1
fi

if [ -f $AAC_DIR/$AAC_TAR ]
then
	echo "Unpacking aac"
	tar -zxf $AAC_DIR/$AAC_TAR -C $AAC_DIR
fi

#Download FLAC
if [ ! -f $FLAC_DIR/$FLAC_TAR ]
then
	echo "Downloading flac please wait..."
	prepare_download
	download_file $FLAC_DOWNLOAD_LINK $FLAC_DIR/$FLAC_TAR
fi

if [ ! -f $FLAC_DIR/$FLAC_TAR ]
then
	echo "Failed to download flac! Please download manually\nand save it in this directory as $FLAC_TAR"
	exit 1
fi

if [ -f $FLAC_DIR/$FLAC_TAR ]
then
	echo "Unpacking flac"
	tar -xf $FLAC_DIR/$FLAC_TAR -C $FLAC_DIR
fi



echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>OPUS Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $OPUS_DIR/opus-1.4
rm -rf build
cmake -B build ./ $CONFIG_FLAGS_OPUS
cmake --build build --clean-first
cmake --install build --prefix $CODECS_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>AAC Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $AAC_DIR/fdk-aac-free-2.0.0
rm -rf build
mkdir build
cd build
../configure $CONFIG_FLAGS_AAC --prefix=$CODECS_DIR
make
make install

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>FLAC Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $FLAC_DIR/flac-1.4.2
rm -rf build
cmake -B build ./ $CONFIG_FLAGS_FLAC
cmake --build build --clean-first
cmake --install build --prefix $CODECS_DIR



