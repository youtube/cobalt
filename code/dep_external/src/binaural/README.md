README.md
=========
# Dependent Externals Libraries

## Contents
1. [Downloading the opensource](#Downloading-the-opensource)
2. [Building the libraries](#Building-the-libraries)
    - [Prerequisites](#Prerequisites)
    - [Basic build](#Basic-build)


## Downloading the opensource
 1. [bear](https://github.com/ebu/bear)
 2. [resonance-audio](https://github.com/resonance-audio/resonance-audio)


## Building the libraries

### Prerequisites
 1. [CMake](https://cmake.org) version 3.6 or higher.
 2. [boost](https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.zip), version 1.82
~~~
    $ ./bootstrap.sh --with-toolset=gcc
    $ ./b2 toolset=gcc
    $ ./b2 install
~~~

### Basic build
"build.sh" is an example to build, you can run it directly at your side.