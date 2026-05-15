Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Linux

These instructions explain how Linux users set up their Cobalt development environment, clone a copy of the Cobalt source code repository, and build a Cobalt binary.

## Set up your workstation

Install necessary packages.

```bash
sudo apt update && sudo apt install -qqy --no-install-recommends \
  git curl python3-dev xz-utils lsb-release file
```

Clone `depot_tools` and add the directory to `PATH` for this session.

```bash
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
export PATH="$PATH:$HOME/depot_tools"
```

Consider adding the `depot_tools` directory to `PATH` in `.bashrc` or `.profile` to make it permanent.

## Get source code

Clone the Cobalt repository and use `gclient` to synchronize dependencies.

```bash
mkdir ~/cobalt && cd ~/cobalt
git clone --single-branch https://github.com/youtube/cobalt.git src
gclient config --name=src https://github.com/youtube/cobalt.git
cd src
gclient sync --no-history -r $(git rev-parse @)
```

## Build and Run Cobalt

Install build dependencies:

```bash
build/install-build-deps.sh
gclient runhooks
```

Build Cobalt for Linux:

```bash
cobalt/build/gn.py -p linux-x64x11 -c qa
autoninja -C out/linux-x64x11_qa cobalt
```

After the build completes, you can run Cobalt with:

```bash
out/linux-x64x11_qa/cobalt [--url=<url>]
```

## Running Cobalt in Evergreen Mode

To build and run Cobalt in Evergreen mode:

```bash
cobalt/build/gn.py -p evergreen-x64 -c qa
autoninja -C out/evergreen-x64_qa cobalt_loader nplb_loader
```

To launch Cobalt in Evergreen mode:

```bash
out/evergreen-x64_qa/cobalt_loader.py
```

To run the `nplb` test suite in Evergreen mode:

```bash
out/evergreen-x64_qa/nplb_loader.py
```
