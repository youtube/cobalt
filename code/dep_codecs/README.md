README.md
=========
# Dependent Codec Libraries

## Contents
1. [Downloading the opensource](#Downloading-the-opensource)
2. [Building the libraries](#Building-the-libraries)
    - [Prerequisites](#Prerequisites)
    - [Basic build](#Basic-build)
    - [Cross compiling](#Cross-compiling)


## Downloading the opensource
 1. [opus](https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz)
 2. [fdk-aac](https://people.freedesktop.org/~wtay/fdk-aac-free-2.0.0.tar.gz) 
 3. [flac](https://downloads.xiph.org/releases/flac/flac-1.4.2.tar.xz)
 
Please download the opensource and unzip to directory separately as following:
~~~
    opus/opus-1.4
    aac/fdk-aac-free-2.0.0
    flac/flac-1.4.2
~~~ 

## Building the libraries

### Prerequisites
 1. [CMake](https://cmake.org) version 3.6 or higher.
 2. Tool chains if need cross compiling.


### Basic build
"build.sh" is an example to build, you can run it directly at your side.


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

The following example demonstrates use of the x86-macos.cmake toolchain file on
a x86\_64 MacOS host:

~~~
    $ ./build.sh --x86-macos_toolchain
~~~
