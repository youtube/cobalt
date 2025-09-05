Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - RDK

Starboard for RDK is now integrated into the Cobalt 25.lts.1+ branch. The RDK `loader_app`,
which includes Starboard, can be built using the Cobalt build mechanism.
These instructions explain how to build it and update the library on an RDK device.

For the RDK Starboard implementation, you can find the source in the [RDK Central repository](https://github.com/rdkcentral/larboard). Cobalt merges and customizes it in [starboard/contrib/rdk](/starboard/contrib/rdk/).

## Pre-condition
### Set up your environment
Go through the [Linux setup instructions](setup-linux.md) and ensure you have finished setting up the
environment for your workstation. After setting up the build environment, you should be able to use the `ninja` command
to build `loader_app` or `nplb`.

### Get an RDK device running the correct image
1. Get an RDK reference device

    You need to have a YouTube reference device (Amlogic S905X4 (AH212)) to run the commands.

    If you don't have one, you can purchase a reference device from
[rdklogic](https://rdklogic.tv/collections/all-products/products/amlogic-s905x4-developer-box-rdk?variant=43178224484592).

2. Download the RDK image

    The prebuilt RDK image for AH212 is available on the YouTube developer page. Please select the
[RDK Certified Image(2025-02)](https://developers.google.com/youtube/devices/living-room/compliance/ah212#rdk_certified_image2025-02) image.

3. Update images and getting logs

    Visit the [YouTube developer page](https://developers.google.com/youtube/devices/living-room/compliance/ah212)
    to get detailed information about:
    1. The specification of the reference device.
    2. Set up a terminal to access the debug console and get logs.
    2. Download the RDK image updating tool.
    3. The instructions to update the RDK image.

## Build Cobalt Starboard for RDK

Please note that these instructions work on the 25.lts.1+ branch only.
The related instructions for the main branch will be updated once it is supported.

```sh
# Step 1: Install toolchain
# Select your RDK toolchain path
export RDK_SDK=/workspaces/rdk/toolchain
export RDK_HOME=${RDK_SDK}

# Download toolchain
wget https://storage.googleapis.com/cobalt-static-storage-public/20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh
# Install the toolchain to the $RDK_SDK folder
sh 20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh -d ${RDK_SDK} -y

# Step 2: Checkout the source code
# Ensure you checkout the 25.lts.1+ branch and update the code to the latest version
cd <cobalt_folder>
git checkout 25.lts.1+

# Step 3: Build RDK Starboard
# Generate the output folder for RDK
gn gen out/rdk-arm_devel '--args=target_platform="rdk-arm" build_type="devel" target_cpu="arm" target_os="linux" is_clang=false using_old_compiler=true rdk_build_with_yocto=false rdk_starboard_root="//starboard/contrib/rdk/src" rdk_enable_securityagent=false'

# Build the RDK Starboard libraries and executables
ninja -v -j 32 -C out/rdk-arm_devel loader_app_install native_target/crashpad_handler elf_loader_sandbox_install elf_loader_sandbox_bin loader_app_bin
```

Once the build completes successfully, the output library is available at `out/rdk-arm_devel/libloader_app.so`.

## Update the library and perform a test
### Kill the Cobalt process if it is running
```sh
kill -9 $(ps -aux | grep Cobalt | awk '{print $2}')
```

### Copy the library to the device
If you have a USB type-C cable connected, you can use `adb` to update the file
```sh
adb push libloader_app.so /usr/lib/libloader_app.so
```

Otherwise, you can use `scp` to update the file
```sh
scp -O libloader_app.so root@<device_ip>:/usr/lib/libloader_app.so

# The root password is empty. Press the Enter key when prompted.
```

### Verify Starboard
Once `libloader_app.so` is updated, you can launch Cobalt from the UI. The Starboard log is available in `journalctl`
```sh
journalctl -f
```