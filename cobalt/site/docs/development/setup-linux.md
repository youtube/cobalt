Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Linux

These instructions explain how Linux users set up their Cobalt development environment, clone a copy of the Cobalt source code repository, and build a Cobalt binary. Note that the binary has a graphical client and must be run locally on the machine that you are using to view the client. For example, you cannot SSH into another machine and run the binary on that machine.

These instructions were tested on a clean ubuntu:22.04 environment. We recommend using git version 2.51 or later (versions 2.25 and greater are known to succeed, while older versions may fail with gclient). Required libraries can differ depending on your Linux distribution and version.

## Set up your workstation

1. Run the following command to install packages needed to build and run Cobalt on Linux:

   ```bash
   sudo apt update && sudo apt install -qqy --no-install-recommends \
     git curl python3-dev xz-utils lsb-release file
   ```

2. Install `ccache` to support build acceleration. Build acceleration is enabled by default and must be disabled if `ccache` is not installed.

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

To configure your local checkout, you will clone the Cobalt repository and use `gclient` to synchronize dependencies.

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

1. Build the code running the following command in the top-level `src` directory. You must specify a platform when running this command. On Ubuntu Linux, the canonical platform is `linux-x64x11`.

   You can also use the `-c` command-line flag to specify a `build_type`. Valid build types are `debug`, `devel`, `qa`, and `gold`. If you specify a build type, the command finishes sooner. Otherwise, all types are built.

   ```bash
   cobalt/build/gn.py [-c <build_type>] -p <platform> --no-rbe
   ```

2. Compile the code from the `src` directory:

   ```bash
   autoninja -C out/<platform>_<build_type> <target_name>
   ```

   The previous command contains three variables:

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

## Debugging Cobalt

`debug`, `devel`, and `qa` configs of Cobalt expose a feature enabling developers to trace Cobalt's callstacks per-thread. This is not only a great way to debug application performance, but also a great way to debug issues and better understand Cobalt's execution flow in general.

Simply build and run one of these configs and observe the terminal output.

## Running Cobalt in Evergreen Mode

As Evergreen support is mandatory for certification, Cobalt can also be executed in Evergreen mode on Linux.

1. Initialize an Evergreen build directory for `evergreen-x64`:

   ```bash
   cobalt/build/gn.py -p evergreen-x64 -c qa --no-rbe
   ```

2. Compile the `cobalt_loader` target, which packages the compressed Evergreen library:

   ```bash
   autoninja -C out/evergreen-x64_qa cobalt_loader
   ```

3. To launch Cobalt in Evergreen mode, execute the generated helper script:

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

## Removing the Cobalt Environment

If you ever need to completely reset your Linux build environment or purge cached toolchains:

1. To clean build artifacts and free up disk space without deleting your source repository:

   ```bash
   # Purge all compiled object files and artifacts in the build directory
   gn clean out/linux-x64x11_devel
   ```

2. To permanently delete the entire development environment, including all uncommitted local changes, branches, and the source repository:

   ```bash
   rm -rf ~/cobalt
   ```
