# FFMPEG header files.

This directory contains the header files of the ffmpeg library.

## How to import ffmpeg header files.

```shell

# Downloads ffmpeg tarball from https://www.ffmpeg.org/download.html#releases.

# Customize these variabiles.
# Customization-begin
TARBALL_PATH=~/Downloads/ffmpeg-7.1.1.tar.xz
COBALT_ROOT=~/chromium/src
FFMPEG_TARGET=ffmpeg-7.1.1
# Customization-end

FFMPEG_INCLUDE_ROOT=${COBALT_ROOT}/third_party/ffmpeg_includes
tar -xf ${TARBALL_PATH} -C ${FFMPEG_INCLUDE_ROOT}

cd ${FFMPEG_INCLUDE_ROOT}/${FFMPEG_TARGET}

# Generate avconfig.h
./configure --disable-x86asm

# Keep only header files and COPYING.LGPLv2.1.
find . -type f -not \( -name "COPYING.LGPLv2.1" -o -name "*.h" \) -delete

# Delete empty folders.
find . -type d -empty -delete
```