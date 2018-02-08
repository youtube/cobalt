# MIPS support

starboard/contrib/creator directory contains port of Cobalt for Creator CI20 platform:
https://creatordev.io/ci20.html


# Building Cobalt for CI20

Cobalt can be built for CI20 using LLVM toolchain (Clang and lld),
or GCC 4.9 toolchain. In both cases sysroot based on Debian Jessie is needed.

## Sysroot

Directory third_party/ci20 contains script mipsel-toolchain-sysroot-creator.sh,
which will create Debian Jessie based sysroot. In addition this script will
also build GCC toolchain.

In order to create sysroot and toolchain run:

    ./mipsel-toolchain-sysroot-creator.sh

Result will be a package, mipsel_toolchain_sysroot.tgz, which will contain
sysroot and GCC toolchain.

Directory where this package is extracted should be exported as CI20_HOME.

    export CI20_HOME=<PATH_TO_DIRECTORY_WHERE_PACKAGE_IS_EXTRACTED>

## Building

Follow all generic instructions from:

    http://cobalt.foo/development/setup-linux.html

### Building with Clang

Clang is the default toolchain for building Cobalt for CI20.
Configuration files are located in creator/ci20x11.

Commands for building are:

    cobalt/build/gyp_cobalt -C debug creator-ci20x11
    ninja -j4 -C out/creator-ci20x11_debug all

    cobalt/build/gyp_cobalt -C devel creator-ci20x11
    ninja -j4 -C out/creator-ci20x11_devel all

    cobalt/build/gyp_cobalt -C qa creator-ci20x11
    ninja -j4 -C out/creator-ci20x11_qa cobalt

    cobalt/build/gyp_cobalt -C gold creator-ci20x11
    ninja -j4 -C out/creator-ci20x11_gold cobalt

### Building with GCC 4.9

Configuration files for GCC build are located in creator/ci20x11/gcc/4.9

Commands for building are:

    cobalt/build/gyp_cobalt -C debug creator-ci20x11-gcc-4-9
    ninja -j4 -C out/creator-ci20x11-gcc-4-9_debug all

    cobalt/build/gyp_cobalt -C devel creator-ci20x11-gcc-4-9
    ninja -j4 -C out/creator-ci20x11-gcc-4-9_devel all

    cobalt/build/gyp_cobalt -C qa creator-ci20x11-gcc-4-9
    ninja -j4 -C out/creator-ci20x11-gcc-4-9_qa cobalt

    cobalt/build/gyp_cobalt -C gold creator-ci20x11-gcc-4-9
    ninja -j4 -C out/creator-ci20x11-gcc-4-9_gold cobalt


# Running Cobalt on CI20

Cobalt might complain about missing libraries on CI20 platform with stock
Debian rootfs.

To check which libraries are missing, run:

    ldd ./cobalt

And install missing libraries using apt-get.

## Pulseaudio

Before running Cobalt on CI20, pulseaudio must be disabled.

a) Global config (all users):
edit /etc/pulse/client.conf
or
b) Local config (single user):
edit ~/.config/pulse/client.conf

Add following new line to client.conf:
autospawn = no

## Starting Cobalt

To start Cobalt on CI20, mount "out" directory on platform and run:

    cd out/creator-ci20x11_gold
    ./cobalt

By default Cobalt will open youtube.com/tv page.


# Notes about performance

## Audio

With pulseaudio disabled, audio on youtube.com/tv is working OK,
with occasional audible artifacts.

## Video

Since CI20 still uses software video decoding, video performance is not good.
Video playback will start, but most of the frames will be dropped.
