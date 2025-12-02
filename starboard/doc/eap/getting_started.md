# Open Source Setup

<<<<<<< HEAD
=======
The following steps show how to get set up with the new Cobalt repository
format. Note that this was tested with git version 2.51, and some older
versions of git are known not to work with gclient. We expected versions 2.25
and greater to succeed, but recommend you use version 2.51 or later.

## Setup workstation ##

Install necessary packages.

>>>>>>> 791376a3e02 (Add git version requirement to EAP getting_started (#7704))
```
# Install necessary packages
sudo apt update && sudo apt install -qqy --no-install-recommends \
  git curl python3-dev xz-utils lsb-release file

# Install depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

# Consider adding the following to your ~/.bashrc or equivalent as well:
export PATH="/path/to/depot_tools:$PATH"

# Download the repository. Use whichever protocol you prefer.
git clone --single-branch git@github.com:youtube/cobalt.git chromium/src
git -C chromium/src remote add _gclient git@github.com:youtube/cobalt.git

# Configure gclient and download subrepos
cd chromium
gclient config --name=src git@github.com:youtube/cobalt.git
cd src
gclient sync --no-history -r $(git rev-parse @)

# Install build dependencies.
./build/install-build-deps.sh

# Build Evergreen for linux
cobalt/build/gn.py -p evergreen-x64 -c devel --no-rbe
autoninja -C out/evergreen-x64_devel/ cobalt_loader nplb_loader

# Run Cobalt
out/evergreen-x64_devel/loader_app
# Run NPLB
out/evergreen-x64_devel/elf_loader_sandbox --evergreen_content=. --evergreen_libary=libnplb.so
```

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
