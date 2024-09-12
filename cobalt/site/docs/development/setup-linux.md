Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Linux

These instructions explain how Linux users set up their Cobalt development
environment, clone a copy of the Cobalt code repository, and build a Cobalt
binary. Note that the binary has a graphical client and must be run locally
on the machine that you are using to view the client. For example, you cannot
SSH into another machine and run the binary on that machine.

These instructions were tested on a clean ubuntu:20.04 Docker image. (7/11/23)
Required libraries can differ depending on your Linux distribution and version.

## Set up your workstation

1.  Run the following command to install packages needed to build and run
    Cobalt on Linux:

    ```sh
    sudo apt update && sudo apt install -qqy --no-install-recommends \
      bison clang libasound2-dev libgles2-mesa-dev libglib2.0-dev \
      libxcomposite-dev libxi-dev libxrender-dev nasm ninja-build \
      python3-venv
    ```

1.  Install ccache to support build acceleration. Build acceleration is \
    enabled by default and must be disabled if ccache is not installed.

    ```sh
    sudo apt install -qqy --no-install-recommends ccache
    ```

    We recommend adjusting the cache size as needed to increase cache hits:

    ```sh
    ccache --max-size=20G
    ```

1.  Install Node.js via `nvm`:

    ```sh
    export NVM_DIR=~/.nvm
    export NODE_VERSION=12.17.0

    curl --silent -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.3/install.sh | bash

    . $NVM_DIR/nvm.sh \
        && nvm install --lts \
        && nvm alias default lts/* \
        && nvm use default
    ```

1.  Install GN, which we use for our build system code. There are a few ways to
    get the binary, follow the instructions for whichever way you prefer
    [here](https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/#getting-a-binary).

1.  Clone the Cobalt code repository. The following `git` command creates a
    `cobalt` directory that contains the repository:

    ```sh
    git clone https://github.com/youtube/cobalt.git
    ```

1.  Set `PYTHONPATH` environment variable to include the full path to the
    top-level `cobalt` directory from the previous step. Add the following to
    the end of your ~/.bash_profile (replacing `fullpathto` with the actual
    path where you cloned the repo):

    ```sh
    export PYTHONPATH="/fullpathto/cobalt:${PYTHONPATH}"
    ```

    You should also run the above command in your terminal so it's available
    immediately, rather than when you next login.

### Set up Developer Tools

1.  Enter your new `cobalt` directory:

    ```sh
    cd cobalt
    export COBALT_SRC=${PWD}
    export PYTHONPATH=${PWD}:${PYTHONPATH}
    ```

    <aside class="note">
    <b>Note:</b> Pre-commit is only available on branches later than 22.lts.1+,
    including trunk. The below commands will fail on 22.lts.1+ and earlier branches.
    For earlier branches, run `cd src` and move on to the next section.
    </aside>

1.  Create a Python 3 virtual environment for working on Cobalt (feel free to use `virtualenvwrapper` instead):

    ```sh
    python3 -m venv ~/.virtualenvs/cobalt_dev
    source ~/.virtualenvs/cobalt_dev/bin/activate
    pip install -r requirements.txt
    ```

1.  Install the pre-commit hooks:

    ```sh
    pre-commit install -t post-checkout -t pre-commit -t pre-push --allow-missing-config
    git checkout -b <my-branch-name> origin/master
    ```

1.  Download clang++:

    ```sh
    ./starboard/tools/download_clang.sh
    ```

## Build and Run Cobalt

1.  Build the code running the following command in the top-level `cobalt`
    directory. You must specify a platform when running this command. On Ubuntu
    Linux, the canonical platform is `linux-x64x11`.

    You can also use the `-c` command-line flag to specify a `build_type`.
    Valid build types are `debug`, `devel`, `qa`, and `gold`. If you
    specify a build type, the command finishes sooner. Otherwise, all types
    are built.

    ```sh
    python cobalt/build/gn.py [-c <build_type>] -p <platform>
    ```

1.  Compile the code from the `cobalt/` directory:

    ```sh
    $ ninja -C out/<platform>_<build_type> <target_name>
    ```

    The previous command contains three variables:

    1.  `<platform>` is the [platform
        configuration](../starboard/porting.md#1-enumerate-and-name-your-platform-configurations)
        that identifies the platform. As described in the Starboard porting
        guide, it contains a `family name` (like `linux`) and a
        `binary variant` (like `x64x11`), separated by a hyphen.
    1.  `<build_type>` is the build you are compiling. Possible values are
        `debug`, `devel`, `qa`, and `gold`.
    1.  `<target_name>` is the name assigned to the compiled code and it is
        used to run the code compiled in this step. The most common names are
        `cobalt`, `nplb`, and `all`:
        *   `cobalt` builds the Cobalt app.
        *   `nplb` builds Starboard's platform verification test suite to
            ensure that your platform's code passes all tests for running
            Cobalt.
        *   `all` builds all targets.

    For example:

    ```sh
    ninja -C out/linux-x64x11_debug cobalt
    ```

    This command compiles the Cobalt `debug` configuration for the
    `linux-x64x11` platform and creates a target named `cobalt` that
    you can then use to run the compiled code.

1.  Run the compiled code to launch the Cobalt client:

    ```sh
    # Note that 'cobalt' was the <target_name> from the previous step.
    out/linux-x64x11_debug/cobalt [--url=<url>]
    ```

    The flags in the following table are frequently used, and the full set
    of flags that this command supports are in <code><a
    href="https://cobalt.googlesource.com/cobalt/+/master/cobalt/browser/switches.cc">cobalt/browser/switches.cc</a></code>.

    <table class="details responsive">
      <tr>
        <th colspan="2">Flags</th>
      </tr>
      <tr>
        <td><code>allow_http</code></td>
        <td>Indicates that you want to use `http` instead of `https`.</td>
      </tr>
      <tr>
        <td><code>ignore_certificate_errors</code></td>
        <td>Indicates that you want to connect to an <code>https</code> host
            that doesn't have a certificate that can be validated by our set
            of root CAs.</td>
      </tr>
      <tr>
        <td><code>url</code></td>
        <td>Defines the startup URL that Cobalt will use. If no value is set,
            then Cobalt uses a default URL. This option lets you point at a
            different app than the YouTube app.</td>
      </tr>
    </table>

## Debugging Cobalt

`debug`, `devel`, and `qa` configs of Cobalt expose a feature enabling
developers to trace Cobalt's callstacks per-thread. This is not only a great way
to debug application performance, but also a great way to debug issues and
better understand Cobalt's execution flow in general.

Simply build and run one of these configs and observe the terminal output.

# Running Cobalt in evergreen mode

As [Evergreen support](/starboard/doc/evergreen/cobalt_evergreen_overview.md) is
mandatory since 2024 requirement, Cobalt should be executed in Evergreen mode.

## Download and configure the official Google-built Cobalt binaries from GitHub
1. Create output directory for evergreen

    - Create the directory with arguments that meet your need
      - Build type, ex: gold, qa
      - Starboard API version, ex: 15, 16
    - An example to create directory with build_type=qa and sb_api_version=16

      ```sh
      # go to cobalt root folder
      export COBALT_SRC=${PWD}
      export PYTHONPATH=${PWD}:${PYTHONPATH}

      # setup evergreen build arguments
      export EG_PLATFORM=evergreen-x64
      export EG_BUILD_TYPE=qa
      export SB_API_VER=16
      export EVERGREEN_DIR=out/${EG_PLATFORM}_${EG_BUILD_TYPE}

      gn gen $EVERGREEN_DIR --args="target_platform=\"$EG_PLATFORM\" use_asan=false build_type=\"$EG_BUILD_TYPE\" sb_api_version=$SB_API_VER"
      ```

1. Select Google-prebuilt Cobalt binaries from [GitHub](https://github.com/youtube/cobalt/releases)

    - Choose the correct evergreen version based on the target device specification

      Please note that the selected prebuilt binary must meet the settings used to create evergreen directory in previous step.
      - Cobalt version you checked out, ex: 25.lts.1
      - Build type, ex: gold, qa
      - Starboard API version, ex: 15, 16
    - Starting from Cobalt 25 with starboard API version 16, you need to use
      [compressed version](/starboard/doc/evergreen/evergreen_binary_compression.md), ex:
      - [`25.lts.1 release`](https://github.com/youtube/cobalt/releases/tag/25.lts.1)
      - `Gold version`: cobalt_evergreen_5.1.2_x64_sbversion-16_release_compressed_20240629001855.crx

      - `QA version`: cobalt_evergreen_5.1.2_x64_sbversion-16_qa_compressed_20240629001855.crx
    - Right click the file and copy file URL

1. Download and unzip the file

    ```sh
    export LOCAL_CRX_DIR=/tmp/cobalt_dl
    rm -rf $LOCAL_CRX_DIR
    mkdir -p $LOCAL_CRX_DIR

    # paste prebuilt library URL and Download it to /tmp
    # Please update URL according to your need
    COBALT_CRX_URL=https://github.com/youtube/cobalt/releases/download/25.lts.1/cobalt_evergreen_5.1.2_x64_sbversion-16_qa_compressed_20240629001855.crx

    wget $COBALT_CRX_URL -O $LOCAL_CRX_DIR/cobalt_prebuilt.crx

    # Unzip the downloaded CRX file
    unzip $LOCAL_CRX_DIR/cobalt_prebuilt.crx -d $LOCAL_CRX_DIR/cobalt_prebuilt
    ```

1. Copy the files to the appropriate directories

    ```sh
    cd $COBALT_SRC
    mkdir -p $EVERGREEN_DIR/install/lib
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* $EVERGREEN_DIR/
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* $EVERGREEN_DIR/install/lib
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/manifest.json $EVERGREEN_DIR/
    cp -rf $LOCAL_CRX_DIR/cobalt_prebuilt/content $EVERGREEN_DIR/
    ```

## Build executables running on target platform

The Cobalt library is prepared in above steps. The next step is to build
the partner-built Loader App and Crashpad Handler executables. Please refer
[Evergreen build](/starboard/doc/evergreen/cobalt_evergreen_overview.md#build-commands)
for more information. Here is an example of devel build and Starboard API 16.

1. Generate output folder for x64

    ```sh
    # The x64 build type can be any other build type from evergreen
    # The sb_api_version should be the same as evergreen
    export TARGET_PLATFORM=linux-x64x11
    export TARGET_BUILD_TYPE=devel
    export TARGET_DIR=out/${TARGET_PLATFORM}_${TARGET_BUILD_TYPE}
    gn gen $TARGET_DIR --args="target_platform=\"$TARGET_PLATFORM\" build_type=\"$TARGET_BUILD_TYPE\" sb_api_version=$SB_API_VER"
    ```

1. Build executables

    ```sh
    ninja -C $TARGET_DIR loader_app_install
    ninja -C $TARGET_DIR native_target/crashpad_handler
    ninja -C $TARGET_DIR elf_loader_sandbox_install
    ```

1. Copy necessary artifacts from evergreen folder

    ```sh
    mkdir -p $TARGET_DIR/content/app/cobalt
    cp -r $EVERGREEN_DIR/install/lib/ $TARGET_DIR/content/app/cobalt
    cp -r $EVERGREEN_DIR/content/ $TARGET_DIR/content/app/cobalt
    cp $EVERGREEN_DIR/manifest.json $TARGET_DIR/
    cp $TARGET_DIR/native_target/crashpad_handler $TARGET_DIR/

    # The three executables: crashpad_handler, loader_app and elf_loader_sandbox are now placed in $TARGET_DIR/
    ```

## Running Cobalt with loader_app
To launch cobalt, simplely execute `loader_app` in command line:

```sh
cd $TARGET_DIR
./loader_app
```

The loader_app will find cobalt library from `content/app/cobalt/lib/` and
content from `content/app/cobalt/content/`. The library in `content/app/cobalt/lib/`
can be compressed or uncompressed version. For Cobalt 2025 certification,
compressed version should be used.

# Running Tests

There is no prebuilt NPLB library on github server and the partners can build
it from the source code.
However, it is important to note that `the NPLB library should be built using
Cobalt toolchain instead of partner proprietary toolchain`. It is because
NPLB verifies the Starboard implementation from Cobalt perspective. Running NPLB
that is built by Cobalt toolchain accompany with a loader that is built
using partner proprietary can simulate the real use case and detect potential
issue due to toolchain compatibility.

This document describes how to build NPLB library by Cobalt toolchain and
launch NPLB via `elf_loader_sandbox` on x64 platform.

`Note`: Partners should use the Cobalt toolchain to build the NPLB library for
their specific platform. After building, they should copy the NPLB library and
associated test materials to their target devices. Finally, they should use
`elf_loader_sandbox` (built with their own toolchain) to launch the NPLB.

### Build NPLB library using Cobalt toolchain

1. Create output directory for evergreen

    - Create the directory with arguments that meet your need
      - Build type, ex: devel, debug, qa
      - Starboard API version, ex: 15, 16
    - The example here use `devel` build type to get more logs during test

      ```sh
      # go to cobalt root folder
      export COBALT_SRC=${PWD}
      export PYTHONPATH=${PWD}:${PYTHONPATH}

      # setup evergreen build arguments
      export EG_PLATFORM=evergreen-x64
      export EG_BUILD_TYPE=devel
      export SB_API_VER=16
      export EVERGREEN_DIR=out/${EG_PLATFORM}_${EG_BUILD_TYPE}

      gn gen $EVERGREEN_DIR --args="target_platform=\"$EG_PLATFORM\" use_asan=false build_type=\"$EG_BUILD_TYPE\" sb_api_version=$SB_API_VER"
      ```

1. Build NPLB library

    ```sh
    ninja -C $EVERGREEN_DIR nplb_install
    ```

1. Build elf_loader_sandbox

    Follow the steps at [Build executables running on target platform](#build-executables-running-on-target-platform) to build elf_loader_sandbox.

1. Copy necessary artifacts from evergreen folder

    ```sh
    mkdir -p $TARGET_DIR/content/app/cobalt
    cp -rf $EVERGREEN_DIR/install/lib/ $TARGET_DIR/content/app/cobalt
    cp -rf $EVERGREEN_DIR/content/ $TARGET_DIR/content/app/cobalt
    ```

### Launch NPLB with elf_loader_sandbox

As mentioned in [Evergreen document](/starboard/doc/evergreen/cobalt_evergreen_overview.md#building-and-running-tests),
`elf_loader_sandbox` is a lightweight loader than the `loader_app`.
It can be used to load cobalt, NPLB, or other libraries if the given paths are
provided with command line switches:
`--evergreen_library` and `--evergreen_content`.
These switches are the path to the shared library to be run and the path to that shared
library's content. Only uncompressed library is supported by `elf_loader_sandbox`.

`Note`: these paths should be relative to `<location_of_elf_loader_sandbox>/content/`.

For example, after previous steps, the folder structure in `$TARGET_DIR` is

```
...
loader_app
elf_loader_sandbox
content/
├── app
│   └── cobalt
│       ├── content
│       │   ├── fonts
│       │   ├── ssl
│       │   ├── web
│       │   └── webdriver
│       └── lib
│           └── libnplb.so
└── fonts
...
```
- root content folder is `$TARGET_DIR/content`
- NPLB library path related to root content folder is `app/cobalt/lib/libnplb.so`
- NPLB content path related to root content folder is `app/cobalt/content`

The command to launch NPLB via elf_loader_sandbox is

```sh
cd $TARGET_DIR
./elf_loader_sandbox \
--evergreen_library=app/cobalt/lib/libnplb.so \
--evergreen_content=app/cobalt/content/
```

### Using gtest argument
The gtest argument can be passed as usual.

- Output test result as XML file

    ```sh
    ./elf_loader_sandbox \
    --evergreen_library=app/cobalt/lib/libnplb.so \
    --evergreen_content=app/cobalt/content/ \
    --gtest_output=xml:/data/nplb_testResult.xml
    ```
- Run test cases by filter

    ```sh
    ./elf_loader_sandbox \
    --evergreen_library=app/cobalt/lib/libnplb.so \
    --evergreen_content=app/cobalt/content/ --gtest_filter=*Posix* \
    --gtest_output=xml:/data/nplb_testResult.xml
    ```
