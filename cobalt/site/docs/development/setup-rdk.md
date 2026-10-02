Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - RDK

These instructions explain how to build the RDK Starboard library (`libloader_app.so`) on an Ubuntu 22.04 host and run Cobalt 27.lts on the Amlogic S905X4 (AH212) reference device.

The source for the RDK Starboard implementation originates in the [RDK Central repository](https://github.com/rdkcentral/larboard), which Cobalt merges and customizes under [`starboard/contrib/rdk/`](https://github.com/youtube/cobalt/tree/27.lts/starboard/contrib/rdk).

## Prerequisites

1. An AH212 running an RDK 6 image built on `20260420` or later, connected to your host over `USB-C`. To flash it, see [How do I flash the AH212?](#how_do_i_flash_the_ah212).

2. Install the required packages:

   ```bash
   sudo apt update
   sudo apt install -y wget curl git python3 python3-dev python3-pip \
     xz-utils lsb-release file sudo ninja-build pkg-config \
     android-tools-adb picocom unzip
   ```

3. Install Chromium `depot_tools`:

   ```bash
   git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
   export PATH="$HOME/depot_tools:$PATH"
   echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.bashrc
   ```

4. Install the RDK ARM toolchain:

   ```bash
   export RDK_HOME=$HOME/rdk/toolchain
   mkdir -p ${RDK_HOME}
   wget https://storage.googleapis.com/cobalt-static-storage-public/20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh \
     -O /tmp/rdk-toolchain.sh
   sh /tmp/rdk-toolchain.sh -d ${RDK_HOME} -y
   echo 'export RDK_HOME=$HOME/rdk/toolchain' >> ~/.bashrc
   ```

5. Get the Cobalt source code and install the build dependencies:

   ```bash
   source ~/.bashrc
   mkdir -p ~/cobalt && cd ~/cobalt
   git clone --branch 27.lts --single-branch https://github.com/youtube/cobalt.git src
   git -C src remote add _gclient https://github.com/youtube/cobalt.git
   gclient config --unmanaged --name=src https://github.com/youtube/cobalt.git
   gclient sync --no-history -r src@$(git -C src rev-parse @)

   cd ~/cobalt/src
   ./build/install-build-deps.sh
   python3 build/linux/sysroot_scripts/install-sysroot.py --arch=arm
   ```

6. Generate the build files:

   ```bash
   cd ~/cobalt/src
   python3 cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c qa --no-rbe
   ```

## Running in Evergreen Mode

### Use the prebuilt Cobalt library

For testing and certification, always use the official prebuilt Cobalt library and build only `libloader_app.so`.

1. Download the `arm-hardfp` Evergreen package for your `27.lts` version from [GitHub Releases](https://github.com/youtube/cobalt/releases). The filename has the format `cobalt_evergreen_<version>_arm-hardfp_sbversion-18_qa_compressed_<timestamp>.crx`, where `<version>` is the Evergreen version (such as `7.3.2` for `27.lts.3`). Unpack it into `app/cobalt/`:

   ```bash
   export STAGE_DIR=~/rdk_stage
   rm -rf ${STAGE_DIR} && mkdir -p ${STAGE_DIR}/app/cobalt ${STAGE_DIR}/gen
   unzip /path/to/cobalt_evergreen_<version>_arm-hardfp_sbversion-18_qa_compressed_<timestamp>.crx \
     -d ${STAGE_DIR}/app/cobalt
   ```

2. Build `libloader_app.so`:

   ```bash
   cd ~/cobalt/src
   autoninja -C out/evergreen-arm-hardfp-rdk_qa libloader_app.so cobalt:cobalt_build_info
   ```

3. Package the archive:

   ```bash
   export STAGE_DIR=~/rdk_stage
   export OUT_DIR=~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa
   cp ${OUT_DIR}/libloader_app.so ${STAGE_DIR}/
   cp ${OUT_DIR}/gen/build_info.json ${STAGE_DIR}/gen/
   cd ${STAGE_DIR}
   tar -czvf ~/archive.tar.gz libloader_app.so gen/build_info.json app/cobalt
   ```

4. Deploy `~/archive.tar.gz` as described in [Deploy and run Cobalt](#deploy_and_run_cobalt).

### Build Cobalt from source for debugging

1. Build Cobalt and `libloader_app.so`:

   ```bash
   cd ~/cobalt/src
   autoninja -C out/evergreen-arm-hardfp-rdk_qa \
     cobalt:cobalt_loaded_content libloader_app.so cobalt:cobalt_build_info
   ```

2. Package the archive:

   ```bash
   export OUT_DIR=~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa
   cp ~/cobalt/src/cobalt/updater/version_manifest/manifest.json ${OUT_DIR}/app/cobalt/
   cd ${OUT_DIR}
   tar -czvf ~/archive.tar.gz libloader_app.so gen/build_info.json app/cobalt
   ```

3. Deploy `~/archive.tar.gz` as described in [Deploy and run Cobalt](#deploy_and_run_cobalt). In that section, before you run the step 3 commands, run only the two `find` commands from [Why is the device not running your Cobalt build?](#why_is_the_device_not_running_your_cobalt_build).

### Deploy and run Cobalt

1. Push the archive to the device:

   ```bash
   adb push ~/archive.tar.gz /data/archive.tar.gz
   ```

2. Open a device shell with `adb shell`, then extract the archive and copy the system fonts:

   ```bash
   rm -rf /data/out_cobalt && mkdir -p /data/out_cobalt
   tar -xzvf /data/archive.tar.gz -C /data/out_cobalt
   chmod -R 755 /data/out_cobalt
   cp -r /usr/share/content/data/fonts /data/out_cobalt/
   ```

3. Switch to your build and reboot. Run this every time you deploy a new build:

   ```bash
   chCobalt custom_cobalt
   sync
   reboot -f
   ```

4. After the device reboots, open `adb shell` again and launch YouTube:

   ```bash
   rdkDisplay remove 2>/dev/null
   curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.deactivate","params":{"callsign":"YouTube"}}'
   sleep 2
   curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"YouTube"}}'
   ```

5. In the YouTube app, open **Settings** and confirm that the Cobalt version matches your build. If it doesn't, see [Why is the device not running your Cobalt build?](#why_is_the_device_not_running_your_cobalt_build).

## Running Tests

NPLB (No Platform Left Behind) verifies the Starboard implementation.

1. Build NPLB:

   ```bash
   cd ~/cobalt/src
   autoninja -C out/evergreen-arm-hardfp-rdk_qa elf_loader_sandbox nplb
   ```

2. Package and push the test files:

   ```bash
   cd ~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa
   tar -czvf ~/nplb_archive.tar.gz elf_loader_sandbox libnplb.so icudtl.dat fonts ssl test
   adb push ~/nplb_archive.tar.gz /data/nplb_archive.tar.gz
   ```

3. In `adb shell`, extract and run NPLB:

   ```bash
   rm -rf /data/test && mkdir -p /data/test
   tar -xzvf /data/nplb_archive.tar.gz -C /data/test
   chmod -R 755 /data/test
   cd /data/test

   rdkDisplay remove 2>/dev/null; sleep 2
   rdkDisplay create; sleep 2
   XDG_RUNTIME_DIR=/run WAYLAND_DISPLAY=test-0 ./elf_loader_sandbox \
     --evergreen_content=. --evergreen_library=libnplb.so
   rdkDisplay remove
   ```

   To run a subset of tests, add `--gtest_filter=<pattern>`.

## FAQ

### How do I flash the AH212?

1. Download the latest `aml_upgrade_package_<YYYYMMDD>.img` from the [RDK Developer Images folder](https://drive.google.com/drive/folders/1BnzCFLoceTFFkiTK74aFLsHJukeNa0EP?resourcekey=0-Crb3ms5c7BGy9b-ZB_cbPw) and the Amlogic burning tool `adnl_burn_pkg` for [Linux](https://drive.google.com/file/d/1b9ibJTD1K4AFuDeBB-999VjzJiJrf0E6/view?usp=drive_link), [macOS](https://drive.google.com/file/d/1uBmCtziNwra9SzhraIiPQAXhHo6_j8mc/view?usp=drive_link), or [Windows](https://drive.google.com/file/d/1zeqtMG1i88G9-GqJFgJeLPiom84tKM40/view?usp=sharing). Ask your Google point of contact if you need access.

2. Connect the AH212 `SERIAL` port (Micro-USB) and `USB-C` port to your host with data cables, then open the serial console (run `ls /dev/ttyUSB*` to find the device name):

   ```bash
   sudo picocom /dev/ttyUSB0 --imap lfcrlf -b 921600
   ```

3. Hold `Enter` while powering on the device to stop at the U-Boot prompt, then run `adnl`.

4. In a second host terminal, flash the image:

   ```bash
   sudo /path/to/adnl_burn_pkg -r 1 -e 1 -p /path/to/aml_upgrade_package_<YYYYMMDD>.img
   ```

5. On first boot after flashing, follow the on-screen prompt to pair the remote control.

More device information is available on the [AH212 compliance page](https://developers.google.com/youtube/devices/living-room/compliance/ah212) and the [RDK Central wiki](https://wiki.rdkcentral.com/spaces/RDK/pages/188521050/RDK-Google+IPSTB+profile+stack+on+Amlogic+reference+board).

### Why is the device not running your Cobalt build?

The device runs a downloaded Cobalt update instead of your build unless your build has a higher version. On the device, remove the downloaded updates and reboot:

```bash
find /opt/persistent /home/root /data -maxdepth 5 -type d -name "installation_*" -exec rm -rf {} + 2>/dev/null
find /opt/persistent /home/root /data -name "installation_store_*.pb" -exec rm -f {} + 2>/dev/null
sync
reboot -f
```

After the device reboots, launch YouTube as described in step 4 of [Deploy and run Cobalt](#deploy_and_run_cobalt). Cobalt can download an update again after it starts. If that happens, repeat these steps.

### How do I view the logs?

On the device, run:

```bash
journalctl -t YouTube -t Cobalt -t loader_app -t WPEFramework -f
```

### How do I switch back to the factory Cobalt?

On the device, run:

```bash
chCobalt c25 && sync && reboot -f
```

### Why does `gn.py` fail with an `RDK_HOME` error?

If `gn.py` or `autoninja` fails with `RDK builds require the 'RDK_HOME' environment variable to be set.`, set `RDK_HOME` to the toolchain directory and re-run the command:

```bash
export RDK_HOME=$HOME/rdk/toolchain
```
