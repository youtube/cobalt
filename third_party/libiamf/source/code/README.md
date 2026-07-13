README.md
=========
# [IAMF](https://aomediacodec.github.io/iamf/) Library

## Contents
1. [Building the lib](#Building-the-library)
    - [Prerequisites](#Prerequisites)
    - [Get the code](#Get-the-code)
    - [Basics](#Basic-build)
    - [Configuration options](#Configuration-options)
    - [Dylib builds](#Dylib-builds)
    - [Cross compiling](#Cross-compiling)
    - [MSVC builds](#Microsoft-Visual-Studio-builds)
    - [MacOS builds](#MacOS-builds)
2. [Testing the library](#Testing-the-IAMF)
    - [Build application](#1-Build-application)
    - [Decoder tests](#2-Decoder-tests)
3. [Coding style](#Coding-style)
4. [Bug reports](#Bug-reports)
5. [License](#License)


## Building the library

### Prerequisites
 1. [CMake](https://cmake.org) version 3.6 or higher.
 2. [Git](https://git-scm.com/).
 3. Building the libiamf requires dependent audio codec libraries: [opus](https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz) [fdk-aac](https://people.freedesktop.org/~wtay/fdk-aac-free-2.0.0.tar.gz) [flac](https://downloads.xiph.org/releases/flac/flac-1.4.2.tar.xz)
 4. Enabling the binaural rendering in libiamf requires external libraries: [bear](https://github.com/ebu/bear) [resonance-audio](https://github.com/resonance-audio/resonance-audio)
 
### Get the code

The IAMF library source code is stored in the Alliance for Open Media Git
repository:

~~~
    $ git clone https://github.com/AOMediaCodec/libiamf
    # By default, the above command stores the source in the libiamf/code directory:
    $ cd libiamf/code
~~~

### Basic build

"build.sh" is an example to build, you can run it directly at your side.  
(dependent [codec libraries](dep_codecs/lib) and [external libraries](dep_external/lib/binaural) complied under x64 linux have been provided in advance)

CMake replaces the configure step typical of many projects. Running CMake will
produce configuration and build files for the currently selected CMake
generator. For most systems the default generator is Unix Makefiles. The basic
form of a makefile build is the following:

~~~
    $ cmake .
    $ make
~~~

### Configuration options

The IAMF library has few configuration options, There are one option which is used to enable binaural rendering:
    Build binaural rendering configuration options. These have the form `MULTICHANNEL_BINAURALIZER` and `HOA_BINAURALIZER`.
	(If binaural rendering is not enabled, there is no need to provide external libraries)
~~~
    $ cmake ./ -DMULTICHANNEL_BINAURALIZER=ON -DHOA_BINAURALIZER=ON
    $ make
~~~

### Dylib builds

A dylib (shared object) build of the IAMF library is enabled by default.
~~~
    $ cmake .
    $ make
~~~

### Cross compiling

For the purposes of building the codec and applications and relative to the
scope of this guide, all builds for architectures differing from the native host
architecture will be considered cross compiles. The CMake build handles
cross compiling via the use of toolchain files included in the repository.
The toolchain files available at the time of this writing are:
 
 - arm64-ios.cmake
 - arm64-linux.cmake
 - x86_64-mingw.cmake
 - x86-macos.cmake

Please note that all [dependent codecs](dep_codecs/README.md) and [dependent externals](dep_external/src/binaural/README.md) should have been cross compiled already.

The following example demonstrates use of the x86-macos.cmake toolchain file on
a x86\_64 MacOS host:

~~~
    $ cmake ./ -DCMAKE_TOOLCHAIN_FILE=build/cmake/toolchains/x86-macos.cmake
    $ make
~~~

### Microsoft Visual Studio builds

Building the IAMF library in Microsoft Visual Studio is supported. Visual
Studio 2022(v143) solution has been provided.

Open win64/VS2022/iamf.sln directly.

### MacOS builds
Please note that all [dependent codecs](dep_codecs/README.md) and [dependent externals](dep_external/src/binaural/README.md) should have been MacOS compiled already.

~~~
    $ cmake .
    $ make
~~~

## Testing the IAMF

The iamfdec is a test application to decode an IAMF bitstream or mp4 file with IAMF encapsulation.

### 1. Build application

~~~
    $ cd test/tools/iamfdec

    $ cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS} .
    # ${BUILD_LIBS} is the iamf library and header files installing directory.
    # If enable binaural rendering, add option `MULTICHANNEL_BINAURALIZER` and `HOA_BINAURALIZER`

    $ make
~~~

### 2. Decoder tests

To produce binaural output, please download the following file and place it in your working directory [default.tf](https://github.com/ebu/bear/releases/download/v0.0.1-pre/default.tf).
~~~ 
    ./iamfdec <options> <input file>
    options:
    -i[0-1]    0 : IAMF bitstream input.(default)
               1 : MP4 input.
    -o[2-3]    2 : WAVE output, same path as binary.(default)
               3 [path]
                 : WAVE output, user setting path.
    -r [rate]    : Audio signal sampling rate, 48000 is the default.
    -s[0~11,b]   : Output layout, the sound system A~J and extensions (Upper + Middle + Bottom).
               0 : Sound system A (0+2+0)
               1 : Sound system B (0+5+0)
               2 : Sound system C (2+5+0)
               3 : Sound system D (4+5+0)
               4 : Sound system E (4+5+1)
               5 : Sound system F (3+7+0)
               6 : Sound system G (4+9+0)
               7 : Sound system H (9+10+3)
               8 : Sound system I (0+7+0)
               9 : Sound system J (4+7+0)
              10 : Sound system extension 712 (2+7+0)
              11 : Sound system extension 312 (2+3+0)
              12 : Sound system mono (0+1+0)
               b : Binaural.
    -p [dB]      : Peak threshold in dB.
    -l [LKFS]    : Normalization loudness(<0) in LKFS.
    -d [bit]     : Bit depth of WAVE output.
    -mp [id]     : Set mix presentation id.
    -m           : Generate a metadata file with the suffix .met.
    -disable_limiter
                 : Disable peak limiter.

    Example:  ./iamfdec -o2 -s9 simple_profile.iamf
              ./iamfdec -i1 -o2 -s9 simple_profile.mp4
              ./iamfdec -i1 -o3 /tmp/out.wav -s9 simple_profile.mp4
~~~

## Coding style

We are using the Google C Coding Style defined by the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

The coding style used by this project is enforced with clang-format using the
configuration contained in the
[.clang-format](.clang-format)
file in the root of the repository.

Before pushing changes for review you can format your code with:

~~~
    # Apply clang-format to modified .c, .h files
    $ clang-format -i --style=file \
      $(git diff --name-only --diff-filter=ACMR '*.[hc]')
~~~

## Bug reports

Bug reports can be filed in the Alliance for Open Media
[issue tracker](https://github.com/AOMediaCodec/libiamf/issues/list).

## License

~~~
    BSD 3-Clause Clear License The Clear BSD License

    Copyright (c) 2022, Alliance for Open Media

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted (subject to the limitations in the disclaimer below) provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the distribution.

    3. Neither the name of the Alliance for Open Media nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.


    NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
    THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
    OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
~~~

This IAMF reference software decoder uses the following open source software.
Each open source software complies with its respective license terms, and the license files
have been stored in a directory with their respective source code or library used. The open 
source software listed below is not considered to be part of the IAMF Final Deliverable.

~~~
    https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz (code/dep_codecs/lib/opus.license)
    https://people.freedesktop.org/~wtay/fdk-aac-free-2.0.0.tar.gz (code/dep_codecs/lib/fdk_aac.license)
    https://downloads.xiph.org/releases/flac/flac-1.4.2.tar.xz (code/dep_codecs/lib/flac.license)
    https://svn.xiph.org/trunk/speex/libspeex/resample.c (code/src/iamf_dec/resample.license)
    https://github.com/BelledonneCommunications/opencore-amr/blob/master/test/wavwriter.c (code/dep_external/src/wav/dep_wavwriter.license)
~~~
