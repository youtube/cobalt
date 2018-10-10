# MIPS support

starboard/contrib/creator directory contains port of Cobalt for Creator CI20 platform:
https://www.elinux.org/MIPS_Creator_CI20


# Building Cobalt for CI20

Cobalt can be built for CI20 using LLVM toolchain (Clang and lld),
GCC 4.9 and GCC MTI 6.3 toolchains. In all cases sysroot based on Debian is needed.
For building Cobalt with Clang or GCC 4.9, Debian Jessie is used as sysroot,
and for building with GCC MTI 6.3, sysroot is based on Debian Stretch due to
dependance on newer versions of libstdc++. For this reason, Cobalt built with GCC MTI 6.3
toolchain can only be executed on CI20 platform with Debian Stretch or later.

## Sysroot

Directory third_party/ci20 contains script mipsel-toolchain-sysroot-creator.sh,
which will create Debian based sysroot. In addition this script will
also build GCC 4.9 toolchain or download prebuilt GCC MTI 6.3 from https://codescape.mips.com.

In order to create sysroot and toolchain run:

    ./mipsel-toolchain-sysroot-creator.sh <toolchain>

toolchain argument can be:

clang - Creates sysroot based on Debian Jessie with addition of GCC 4.9 libs
gcc_4-9 - Builds GCC 4.9 toolchain and creates sysroot based on Debian Jessie
gcc_mti_6-3 - Downloads prebuilt MTI GCC 6.3 toolchain and creates sysroot
              based on Debian Stretch

Result will be a package:

mipsel_clang_jessie_sysroot.tgz - contains Debian Jessie sysroot, and required GCC libs
mipsel_gcc_4-9_jessie_sysroot.tgz - contains built GCC 4.9 toolchain and Debian Jessie sysroot
mipsel_gcc_mti_6-3_stretch_sysroot.tgz - contains downloaded GCC MTI 6.3 toolchain and Debian Stretch sysroot

Directory where this package is extracted should be exported as CI20_HOME.

    export CI20_HOME=<PATH_TO_DIRECTORY_WHERE_PACKAGE_IS_EXTRACTED>

## JavaScript engine

Default configuration for JavaScript engine is V8 with JIT enabled.
Alternative configuration is to use mozjs-45, which has JIT disabled by defualt.
Configuration with mozjs-45 is only available when building Cobalt with Clang.

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

Using mozjs-45 JavaScript engine:

    cobalt/build/gyp_cobalt -C debug creator-ci20x11-mozjs
    ninja -j4 -C out/creator-ci20x11-mozjs_debug all

    cobalt/build/gyp_cobalt -C devel creator-ci20x11-mozjs
    ninja -j4 -C out/creator-ci20x11-mozjs_devel all

    cobalt/build/gyp_cobalt -C qa creator-ci20x11-mozjs
    ninja -j4 -C out/creator-ci20x11-mozjs_qa cobalt

    cobalt/build/gyp_cobalt -C gold creator-ci20x11-mozjs
    ninja -j4 -C out/creator-ci20x11-mozjs_gold cobalt

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

### Building with GCC MTI 6.3

Configuration files for GCC build are located in creator/ci20x11/gcc/6.3.mti

Commands for building are:

    cobalt/build/gyp_cobalt -C debug creator-ci20x11-gcc-6-3-mti
    ninja -j4 -C out/creator-ci20x11-gcc-6-3-mti_debug all

    cobalt/build/gyp_cobalt -C devel creator-ci20x11-gcc-6-3-mti
    ninja -j4 -C out/creator-ci20x11-gcc-6-3-mti_devel all

    cobalt/build/gyp_cobalt -C qa creator-ci20x11-gcc-6-3-mti
    ninja -j4 -C out/creator-ci20x11-gcc-6-3-mti_qa cobalt

    cobalt/build/gyp_cobalt -C gold creator-ci20x11-gcc-6-3-mti
    ninja -j4 -C out/creator-ci20x11-gcc-6-3-mti_gold cobalt


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
