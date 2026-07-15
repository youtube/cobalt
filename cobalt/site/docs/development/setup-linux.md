Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Linux

These instructions explain how to set up a Cobalt development environment, clone the repository, and build a Cobalt binary on Linux. The binary includes a graphical client that you must run locally. For example, you cannot run the binary over an SSH connection.

These instructions are tested on a clean Ubuntu 22.04 environment. We recommend using Git version 2.51 or later (versions 2.25 and greater are known to work, but older versions may fail with `gclient`). Required libraries may vary depending on your Linux distribution.

## Set up your workstation

1. Run the following command to install packages needed to build and run Cobalt on Linux:

   ```bash
   sudo apt update && sudo apt install -qqy --no-install-recommends \
     git curl python3-dev xz-utils lsb-release file
   ```

2. Install `ccache` for build acceleration. Acceleration is enabled by default; you must disable it if `ccache` is not installed.

   ```bash
   sudo apt install -qqy --no-install-recommends ccache
   ```

   We recommend adjusting the cache size as needed to increase cache hits:

   ```bash
   ccache --max-size=20G
   ```

   To explicitly configure `ccache` for your build, run `gn args` *after* initializing your build directory with `gn.py` and append the wrapper configuration:

   ```bash
   gn args out/linux-x64x11_devel
   # Add a new line: cc_wrapper = "ccache"
   ```

3. Clone Google's `depot_tools` repository and add the directory to `PATH` for this session:

   ```bash
   git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
   export PATH="$PATH:$HOME/depot_tools"
   ```

   Consider adding the `depot_tools` directory to `PATH` in `.bashrc` or `.profile` to make it permanent across sessions.

## Get source code

To configure your local workspace, clone the Cobalt repository and use `gclient` to synchronize dependencies.

1. Create a working directory and clone the Cobalt repository into the `src` directory expected by `gclient`:

   ```bash
   mkdir ~/cobalt && cd ~/cobalt
   git clone --single-branch https://github.com/youtube/cobalt.git src
   ```

2. Configure `gclient` to track the Cobalt repository as the primary source and synchronize dependencies:

   ```bash
   gclient config --name=src https://github.com/youtube/cobalt.git
   cd src
   gclient sync --no-history -r $(git rev-parse @)
   ```

3. Install system build dependencies and execute gclient hooks:

   ```bash
   build/install-build-deps.sh
   gclient runhooks
   ```

## Build and Run Cobalt

1. Build the code by running the following command in the top-level `src` directory. You must specify a platform. For Ubuntu Linux, the canonical platform is `linux-x64x11`.

   Use the `-c` flag to specify a `build_type` (`debug`, `devel`, `qa`, or `gold`). Specifying a build type speeds up the configuration step because only that type is configured; otherwise, all types are configured.

   ```bash
   cobalt/build/gn.py [-c <build_type>] -p <platform> --no-rbe
   ```

2. Compile the code from the `src` directory:

   ```bash
   autoninja -C out/<platform>_<build_type> <target_name>
   ```

   The command contains three variables:

   1. `<platform>` is the platform configuration that identifies the platform. As described in the Starboard porting guide, it contains a `family name` (like `linux`) and a `binary variant` (like `x64x11`), separated by a hyphen.
   2. `<build_type>` is the build you are compiling. Possible values are `debug`, `devel`, `qa`, and `gold`.
   3. `<target_name>` is the name assigned to the compiled code and it is used to run the code compiled in this step. The most common names are `cobalt`, `nplb`, and `all`:
      * `cobalt` builds the Cobalt app.
      * `nplb` builds Starboard's platform verification test suite to ensure that your platform's code passes all tests for running Cobalt.
      * `all` builds all targets.

   For example:

   ```bash
   cobalt/build/gn.py -p linux-x64x11 -c devel --no-rbe
   autoninja -C out/linux-x64x11_devel cobalt
   ```

   This command configures the Cobalt `devel` configuration for the `linux-x64x11` platform and compiles a target named `cobalt` that you can then use to run the compiled code.

   For additional tips on speeding up compilation, refer to Chromium's [faster builds documentation](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#faster-builds).

3. Run the compiled code to launch the Cobalt client:

   ```bash
   # Note that 'cobalt' was the <target_name> from the previous step.
   out/linux-x64x11_devel/cobalt [--url=<url>]
   ```

   The flags in the following table are frequently used, and the full set of flags that this command supports are in `cobalt/browser/switches.cc`.

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
       <td>Indicates that you want to connect to an <code>https</code> host that doesn't have a certificate that can be validated by our set of root CAs.</td>
     </tr>
     <tr>
       <td><code>url</code></td>
       <td>Defines the startup URL that Cobalt will use. If no value is set, then Cobalt uses a default URL. This option lets you point at a different app than the YouTube app.</td>
     </tr>
   </table>

## Debugging

Cobalt `debug`, `devel`, and `qa` configurations support thread callstack tracing. This is a powerful tool for debugging application performance and issues, and understanding Cobalt's overall execution flow.

To use this feature, build and run one of these configurations and monitor the terminal output.

## Running in Evergreen Mode

Because Evergreen support is required for certification, you can also run Cobalt in Evergreen mode on Linux. In this mode, partners only need to build the Starboard platform layer (`loader_app` and `crashpad_handler`) and link against the official Google-prebuilt Cobalt Core library.

### Download and configure the official Google-built Cobalt binaries from GitHub

1. Create output directory for evergreen:

   Create the directory with arguments matching your required build type (e.g., `qa`, `gold`) and Starboard API version (e.g., `16`, `17`).
   An example to create directory with `build_type=qa` and `sb_api_version=16`:

   ```bash
   export COBALT_SRC=${PWD}
   export PYTHONPATH=${PWD}${PYTHONPATH:+:${PYTHONPATH}}
   export EG_PLATFORM=evergreen-x64
   export EG_BUILD_TYPE=qa
   export SB_API_VER=16
   export EVERGREEN_DIR=out/${EG_PLATFORM}_${EG_BUILD_TYPE}

   gn gen $EVERGREEN_DIR --args="target_platform=\"$EG_PLATFORM\" use_asan=false build_type=\"$EG_BUILD_TYPE\" sb_api_version=$SB_API_VER"
   ```

2. Select Google-prebuilt Cobalt binaries from [GitHub Releases](https://github.com/youtube/cobalt/releases):

   * Choose the correct evergreen version based on the target device specification (e.g., `27.lts.1`).
   * The selected prebuilt binary must match your build type (`qa`/`gold`) and Starboard API version (`16`/`17`).
   * For Cobalt certification, the compressed CRX package must be used (e.g., `cobalt_evergreen_..._qa_compressed_....crx`).
   * Right-click the file and copy the file URL.

3. Download and unzip the prebuilt package:

   ```bash
   export LOCAL_CRX_DIR=/tmp/cobalt_dl
   rm -rf $LOCAL_CRX_DIR
   mkdir -p $LOCAL_CRX_DIR

   # Paste the prebuilt library URL from GitHub Releases
   COBALT_CRX_URL=https://github.com/youtube/cobalt/releases/download/27.lts.1/cobalt_evergreen_x64_sbversion-16_qa_compressed.crx

   wget $COBALT_CRX_URL -O $LOCAL_CRX_DIR/cobalt_prebuilt.crx
   unzip $LOCAL_CRX_DIR/cobalt_prebuilt.crx -d $LOCAL_CRX_DIR/cobalt_prebuilt
   ```

4. Copy the files to appropriate directories:

   ```bash
   cd $COBALT_SRC
   mkdir -p $EVERGREEN_DIR/install/lib
   cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* $EVERGREEN_DIR/
   cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* $EVERGREEN_DIR/install/lib
   cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/manifest.json $EVERGREEN_DIR/
   cp -rf $LOCAL_CRX_DIR/cobalt_prebuilt/content $EVERGREEN_DIR/
   ```

### Build executables running on target platform (Starboard Only)

With the prebuilt Cobalt library prepared, build the partner-built `loader_app` and `crashpad_handler` executables using your platform toolchain.

1. Generate output folder for your target platform:

   ```bash
   export TARGET_PLATFORM=linux-x64x11
   export TARGET_BUILD_TYPE=devel
   export TARGET_DIR=out/${TARGET_PLATFORM}_${TARGET_BUILD_TYPE}

   gn gen $TARGET_DIR --args="target_platform=\"$TARGET_PLATFORM\" build_type=\"$TARGET_BUILD_TYPE\" sb_api_version=$SB_API_VER"
   ```

2. Build Starboard loader and crashpad executables:

   ```bash
   autoninja -C $TARGET_DIR loader_app native_target/crashpad_handler elf_loader_sandbox
   ```

3. Copy necessary artifacts from evergreen folder:

   ```bash
   mkdir -p $TARGET_DIR/app/cobalt
   cp -r $EVERGREEN_DIR/install/lib/ $TARGET_DIR/app/cobalt
   cp -r $EVERGREEN_DIR/content/ $TARGET_DIR/app/cobalt
   cp $EVERGREEN_DIR/manifest.json $TARGET_DIR/app/cobalt/
   cp $TARGET_DIR/native_target/crashpad_handler $TARGET_DIR/
   ```

### Running Cobalt with loader_app

To launch Cobalt in Evergreen mode, execute `loader_app` from your target directory:

```bash
cd $TARGET_DIR
./loader_app
```

## Running Tests

The No Platform Left Behind (NPLB) test suite verifies Starboard implementation and is mandatory for certification. In Evergreen mode, build the NPLB library using the Cobalt toolchain and launch it via `elf_loader_sandbox`.

1. Build NPLB library using Cobalt toolchain:

   ```bash
   autoninja -C $EVERGREEN_DIR nplb
   ```

2. Copy NPLB artifacts to target platform directory:

   ```bash
   mkdir -p $TARGET_DIR/app/cobalt
   cp -rf $EVERGREEN_DIR/install/lib/ $TARGET_DIR/app/cobalt
   cp -rf $EVERGREEN_DIR/content/ $TARGET_DIR/app/cobalt
   ```

3. Launch NPLB with elf_loader_sandbox:

   ```bash
   cd $TARGET_DIR
   ./elf_loader_sandbox \
     --evergreen_library=app/cobalt/lib/libnplb.so \
     --evergreen_content=app/cobalt/content/
   ```

4. To pass standard Google Test filtering or output arguments:

   ```bash
   # Run NPLB with a specific test filter and output results to an XML file
   ./elf_loader_sandbox \
     --evergreen_library=app/cobalt/lib/libnplb.so \
     --evergreen_content=app/cobalt/content/ \
     --gtest_filter=*Posix* --gtest_output=xml:/tmp/nplb_results.xml
   ```

## Clean up or reset the environment

To reset your Linux build environment or purge cached toolchains:

1. To clean build artifacts and free up disk space without deleting your source repository:

   ```bash
   # Purge all compiled object files and artifacts in the build directory
   gn clean out/linux-x64x11_devel
   ```

2. To permanently delete the entire development environment, including all uncommitted local changes, branches, and the source repository:

   ```bash
   rm -rf ~/cobalt
   ```
