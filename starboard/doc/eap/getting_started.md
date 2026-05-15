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

There are two supported approaches to configuring your local checkout.

### Option A: Automated Checkout Setup (Recommended)
If you are initializing a fresh workspace, you can utilize the automated Cobalt checkout orchestration script located in the repository tools:

```bash
python3 .agent/skills/cobalt-new-checkout/scripts/cobalt_new_checkout.py --non-interactive --internal --github-user "<your_github_username>"
```

### Option B: Standard Chromium Checkout (Manual)
If you prefer managing the checkout manually:

```bash
mkdir ~/chromium && cd ~/chromium
fetch --nohooks chromium
# Note: Use --no-history for a faster checkout without full commit history.
```


## Building Evergreen for x64 ##

Install build dependencies:

```bash
cd src
build/install-build-deps.sh
gclient runhooks
```

Build Evergreen for Linux:

```bash
gn args out/evergreen-x64_devel
# Enter: target_os="linux" target_cpu="x64" is_cobalt=true use_evergreen=true build_type="devel"

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
