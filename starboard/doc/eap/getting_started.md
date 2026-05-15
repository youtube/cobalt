# Open Source Setup

The following steps show how to get set up with the new Cobalt repository
format. Note that this was tested with git version 2.51, and some older
versions of git are known not to work with gclient. We expected versions 2.25
and greater to succeed, but recommend you use version 2.51 or later.

## Setup workstation ##

Install necessary packages.

```
sudo apt update && sudo apt install -qqy --no-install-recommends \
  git curl python3-dev xz-utils lsb-release file
```

Clone `depot_tools` and add the directory to `PATH` for this session.

```
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="/path/to/depot_tools:$PATH"
```

Consider adding the `depot_tools` directory to `PATH` in `.bashrc`, `.profile`, or equivalent, to make it permanent.

## Get source code ##

To configure your local checkout, you will clone the Cobalt repository and use `gclient` to synchronize Chromium dependencies.

```bash
# Create a working directory for the Chromium source tree
mkdir ~/chromium && cd ~/chromium

# Clone the Cobalt repository into the 'src' directory expected by gclient
git clone --single-branch https://github.com/youtube/cobalt.git src

# Configure gclient to track the Cobalt repository as the primary source
gclient config --name=src https://github.com/youtube/cobalt.git

# Synchronize all Chromium dependencies, sysroots, and toolchains.
# Note: Using --no-history performs a shallow clone for a significantly faster checkout.
cd src
gclient sync --no-history -r $(git rev-parse @)
```

## Building Evergreen for x64 ##

Install build dependencies:

```bash
# Execute Chromium's automated dependency script to install required Linux system packages and fonts
build/install-build-deps.sh

# Run gclient hooks to ensure all GN toolchains and sysroots are fully configured
gclient runhooks
```

Build Evergreen for Linux:

```bash
# Open an interactive text editor to configure your Evergreen GN build directory
gn args out/evergreen-x64_devel
```

In the text editor that opens, enter your desired configuration:

```gn
# Set target operating system and architecture to Linux x64
target_os = "linux"
target_cpu = "x64"

# Enable Cobalt browser engine and Evergreen binary updaters
is_cobalt = true
use_evergreen = true

# Set build optimization level to development
build_type = "devel"
```

```bash
# Compile the cobalt_loader and nplb_loader targets using autoninja, which automatically optimizes CPU core utilization
autoninja -C out/evergreen-x64_devel cobalt_loader nplb_loader
```

## Running Cobalt ##

After the build completes, you can run Cobalt in Evergreen mode with:

```bash
cd out/evergreen-x64_devel
./cobalt_loader.py
```

To run `nplb` in Evergreen mode:

```bash
cd out/evergreen-x64_devel
./nplb_loader.py
```

## Additional Tips: ##

Before building, you can optionally set cc_wrapper to a build accelerator of your choice. For example, setting it to ccache:

```
sudo apt install -qqy --no-install-recommends ccache

# Choose whatever size works well for your build.
ccache --max-size=20G

# Run this before autoninja but after running gn.py
gn args out/evergreen-x64_devel

# A text editor will open. Add a new line: `cc_wrapper = "ccache"`
```

Follow the docs at https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#faster-builds for other tips on faster builds.
