Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml
# Set up your environment - Cobalt 27.lts for RDK

These instructions explain how to set up the build environment and build Cobalt 27.lts for the RDK platform (`evergreen-arm-hardfp-rdk`). These steps have been verified inside a clean Ubuntu 22.04 environment and on the Amlogic (AH212) reference device.

Starboard for RDK is now integrated into the Cobalt 27.lts branch. The RDK `loader_app`, which includes Starboard, can be built using the Cobalt build mechanism. These instructions explain how to build it and update the library on an RDK device.

For the RDK Starboard implementation, you can find the source in the [RDK Central repository](https://github.com/rdkcentral/larboard). Cobalt merges and customizes it in [starboard/contrib/rdk](/starboard/contrib/rdk/).

## Prerequisites

These instructions assume you are running on Ubuntu 22.04. Required libraries can differ depending on your Linux distribution and version.

### Install Initial Dependencies
First, install the basic tools required to download the toolchain and manage packages.

```sh
sudo apt update
sudo apt install -y wget curl git python3-dev xz-utils lsb-release file sudo
```

### Get an RDK device running the correct image
You need to have a YouTube reference device (Amlogic S905X4 (AH212)) to run the commands.

Please get the device information from the YouTube Partner portal: [AH212](https://developers.google.com/youtube/devices/living-room/compliance/ah212). Note that if you do not have access to the page above, please see [Accessing Living Room Partnership Resources](https://developers.google.com/youtube/devices/living-room/access/accessing-lr-partnership-resources).

More information about the RDK reference box can be found on the RDK Central wiki: [RDK-Google IPSTB profile stack on Amlogic reference board](https://wiki.rdkcentral.com/display/RDK/RDK-Google+IPSTB+profile+stack+on+Amlogic+reference+board).

## Build Cobalt Starboard for RDK

### Install RDK Toolchain

We need to install the specific RDK toolchain required for building Cobalt.

1.  **Define environment variables**:
    ```sh
    export RDK_HOME=$HOME/rdk/toolchain
    ```

2.  **Create the target directory**:
    ```sh
    mkdir -p ${RDK_HOME}
    ```

3.  **Download and install the toolchain**:
    ```sh
    # Download toolchain installer
    wget https://storage.googleapis.com/cobalt-static-storage-public/20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh
    
    # Run the installer
    sh 20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh -d ${RDK_HOME} -y
    ```

### Clone depot_tools

Chromium and Cobalt use `depot_tools` for package management and code retrieval.

```sh
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
export PATH="$HOME/depot_tools:$PATH"
```

### Download Cobalt Source Code

Clone the Cobalt source code repository and checkout the `27.lts` branch.

```sh
mkdir -p ~/chromium
cd ~/chromium
git clone --branch 27.lts --single-branch https://github.com/youtube/cobalt.git src
git -C src remote add _gclient https://github.com/youtube/cobalt.git
```

### Sync Subprojects

Use `gclient` to fetch all external dependencies required by Cobalt.

```sh
cd ~/chromium
gclient config --unmanaged --name=src https://github.com/youtube/cobalt.git
gclient sync --no-history -r src@$(git -C src rev-parse @)
```

### Install Build Dependencies

Run the installer script provided by Cobalt to install all necessary system libraries.

```sh
cd ~/chromium/src

# Run installer
./build/install-build-deps.sh

# Install sysroot for ARM
python3 build/linux/sysroot_scripts/install-sysroot.py --arch=arm
```

### Build RDK Version Simulator

Now you can generate the build files and compile Cobalt.

1.  **Ensure environment variables are set**:
    ```sh
    export PATH="$HOME/depot_tools:$PATH"
    export RDK_HOME=$HOME/rdk/toolchain
    ```

2.  **Generate build files**:
    ```sh
    cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c qa --no-rbe
    ```

3.  **Compile targets**:
    ```sh
    autoninja -C out/evergreen-arm-hardfp-rdk_qa/ cobalt_loader nplb_loader loader_app_rdk_plugin
    ```

### Generate Archive

Bundle the build artifacts into a tarball for deployment.

```sh
tar -czvf archive.tar.gz -C out/evergreen-arm-hardfp-rdk_qa/ \
  -T out/evergreen-arm-hardfp-rdk_qa/cobalt_loader.runtime_deps libloader_app.so \
  -T out/evergreen-arm-hardfp-rdk_qa/nplb_loader.runtime_deps gen/build_info.json
```
