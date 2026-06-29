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

Because Evergreen support is required for certification, you can also run Cobalt in Evergreen mode on Linux.

1. Initialize an Evergreen build directory for `evergreen-x64`:

   ```bash
   cobalt/build/gn.py -p evergreen-x64 -c qa --no-rbe
   ```

2. Compile the `cobalt_loader` target, which packages the compressed Evergreen library:

   ```bash
   autoninja -C out/evergreen-x64_qa cobalt_loader
   ```

3. Launch Cobalt in Evergreen mode by running the generated helper script:

   ```bash
   out/evergreen-x64_qa/cobalt_loader.py [--url=<url>]
   ```

## Running Tests

The No Platform Left Behind (NPLB) test suite verifies Starboard implementation and is mandatory for certification.

1. Compile the NPLB test suite in Evergreen mode:

   ```bash
   cobalt/build/gn.py -p evergreen-x64 -c devel --no-rbe
   autoninja -C out/evergreen-x64_devel nplb_loader
   ```

2. Execute NPLB using the generated Python loader script, which invokes `elf_loader_sandbox` under the hood:

   ```bash
   out/evergreen-x64_devel/nplb_loader.py
   ```

3. To pass standard Google Test filtering or output arguments:

   ```bash
   # Run NPLB with a specific test filter and output results to an XML file
   out/evergreen-x64_devel/nplb_loader.py -- --gtest_filter=*Posix* --gtest_output=xml:/tmp/nplb_results.xml
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
