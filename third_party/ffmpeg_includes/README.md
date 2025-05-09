# FFMPEG header files.

This directory contains the header files of the ffmpeg library.

## How to import ffmpeg header files.

1. Download ffmpeg source and run `configure`

  ```shell

  # Downloads ffmpeg tarball from https://www.ffmpeg.org/download.html#releases.

  # Customize these variabiles.
  # Customization-begin
  TARBALL_PATH=~/Downloads/ffmpeg-7.1.1.tar.xz
  COBALT_ROOT=~/chromium/src
  FFMPEG_TARGET=ffmpeg-7.1.1
  # Customization-end

  FFMPEG_INCLUDE_ROOT=${COBALT_ROOT}/third_party/ffmpeg_includes
  $ tar -xf ${TARBALL_PATH} -C ${FFMPEG_INCLUDE_ROOT}

  $ cd ${FFMPEG_INCLUDE_ROOT}/${FFMPEG_TARGET}

  # Generate avconfig.h
  $ ./configure --disable-x86asm
  ```

2. Trim unneeded files.
  - First, do create initial header file list.

  ```shell
  # Creat initial necessary ffmpeg header file list. 
  # You can create it by copying http://go/paste/5889254032670720

  $ cd ${FFMPEG_INCLUDE_ROOT}/${FFMPEG_TARGET}

  # Delete files that are not listed in this list.
  $ find ./ -type f -print0 | \
    grep --null-data -v -F -x -f necessary_ffmpeg_headers.txt | \
    xargs -0 rm
  ```

  - Build linux cobalt app and, if the compiler complain about missing header files, undelete them.

  - Repeat until there is no missing header files.

3. Delete empty folders.

  ```shell
  # Delete empty folders.
  $ find . -type d -empty -delete
  ```

## Versions of libraries.

SOT: https://www.ffmpeg.org/download.html#releases

- FFmpeg 7.1.1 "Péter"
  - libavutil: 59.39.100
  - libavcodec: 61.19.100
  - libavformat: 61.7.100
