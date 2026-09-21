Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - RDK

These instructions explain how to set up a Linux build environment (Ubuntu 22.04 LTS), flash the Amlogic S905X4 (AH212) reference device, deploy an official **pre-built Cobalt 27.lts (Evergreen)** binary package alongside a locally compiled **RDK Starboard plugin (`libloader_app.so`)**, and verify both **YouTube** and **YouTube TV** on the device.

The source for the RDK Starboard implementation originates in the [RDK Central repository](https://github.com/rdkcentral/larboard), which Cobalt merges and customizes under [`starboard/contrib/rdk/`](https://github.com/youtube/cobalt/tree/27.lts/starboard/contrib/rdk).

## Prerequisites

### 1. Set up your Linux workstation and RDK ARM toolchain

#### Install required system packages

```bash
sudo apt update
sudo apt install -y wget curl git python3 python3-dev python3-pip \
  xz-utils lsb-release file sudo ccache ninja-build pkg-config \
  binutils-arm-linux-gnueabi binutils-arm-linux-gnueabihf
```

#### Install Chromium `depot_tools`

```bash
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
export PATH="$HOME/depot_tools:$PATH"
echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.bashrc
```

#### Install the RDK ARM toolchain

Download and install the official RDK glibc ARM cross-compilation toolchain into `RDK_HOME`:

```bash
export RDK_HOME=$HOME/rdk/toolchain
mkdir -p ${RDK_HOME}

wget https://storage.googleapis.com/cobalt-static-storage-public/20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh \
  -O /tmp/rdk-toolchain.sh
sh /tmp/rdk-toolchain.sh -d ${RDK_HOME} -y
rm /tmp/rdk-toolchain.sh

echo 'export RDK_HOME=$HOME/rdk/toolchain' >> ~/.bashrc
```

### 2. Get Cobalt source code

Clone the `27.lts` branch into `~/cobalt/src` and synchronize all external subprojects using `gclient`:

```bash
mkdir -p ~/cobalt
cd ~/cobalt

# Clone the 27.lts branch into 'src'
git clone --branch 27.lts --single-branch https://github.com/youtube/cobalt.git src
git -C src remote add _gclient https://github.com/youtube/cobalt.git

# Configure and synchronize gclient subprojects
gclient config --unmanaged --name=src https://github.com/youtube/cobalt.git
gclient sync --no-history -r src@$(git -C src rev-parse @)

# Install Cobalt build dependencies and ARM sysroot
cd ~/cobalt/src
./build/install-build-deps.sh
python3 build/linux/sysroot_scripts/install-sysroot.py --arch=arm
```

> [!TIP]
> **Flash your AH212 while `gclient sync` runs**: Synchronizing Cobalt subprojects for the first time can take 15–30 minutes. While `gclient sync` and `install-build-deps.sh` run in this terminal, you can proceed to **3. Flash the Amlogic AH212 reference device** in a new terminal window.

### 3. Flash the Amlogic AH212 reference device

#### Why you must flash a new system image

To run **Cobalt 26 / 27.lts (Evergreen architecture)** on an Amlogic S905X4 (AH212) reference device, you **must** flash a customized RDK6 system image (build date **`20260420` or newer**).

Stock or older RDK images (such as factory images built for Cobalt 25) cannot run Cobalt 27 properly for the following reasons:

1. **WPEFramework (Thunder) plugin architecture**:
   * Legacy development workflows launched Cobalt as a standalone executable (`./loader_app`) by stopping the UI or manually exporting dozens of `WESTEROS_GL_*` environment variables. However, standalone executable mode lacks support for standard TV lifecycle events (**suspend and resume**) and breaks **DRM/OCDM video decryption** (which relies on WPEFramework middleware).
   * In Cobalt 26 and 27+, Cobalt runs natively as a **WPEFramework plugin** (`libloader_app.so`) inside a Dobby container. Only the updated RDK6 customized images include the required WPEFramework plugin configuration (`Cobalt.json`), container mounts, and the **`chCobalt`** version-switching utility.
2. **System software compatibility**:
   * RDK6 images dated `20260420` or newer provide the required container mount points (`/data/out_cobalt` mapped to `/usr/share/content/data` inside the plugin container) and Wayland display management tools (`rdkDisplay`).

#### Where to download the system image and flashing tools

1. **RDK system image (`aml_upgrade_package.img`)**:
   * **Developer system image repository**: Download the latest customized RDK6 system image (recommended: `aml_upgrade_package_20260420.img` or newer) from the [RDK Developer Images Google Drive folder](https://drive.google.com/drive/folders/1BnzCFLoceTFFkiTK74aFLsHJukeNa0EP?resourcekey=0-Crb3ms5c7BGy9b-ZB_cbPw). Please contact your Google point of contact (or Technical Account Manager) if you require access permissions to this folder.
   * **YouTube Partner Portal and RDK Central resources**: Device compliance specifications and reference documentation can also be found on the [YouTube Living Room Compliance - AH212 page](https://developers.google.com/youtube/devices/living-room/compliance/ah212) (see [Accessing Living Room Partnership Resources](https://developers.google.com/youtube/devices/living-room/access/accessing-lr-partnership-resources) if you need access) and the [RDK Central Amlogic Reference Board Wiki](https://wiki.rdkcentral.com/display/RDK/RDK-Google+IPSTB+profile+stack+on+Amlogic+reference+board).

2. **Amlogic USB burning tool (`adnl_burn_pkg`)**:
   Download and extract the flashing utility corresponding to your host PC operating system:
   * **Linux (Ubuntu x86_64)**: [adnl_burn_pkg_4_ubuntu.zip](https://drive.google.com/file/d/1b9ibJTD1K4AFuDeBB-999VjzJiJrf0E6/view?usp=drive_link)
   * **macOS (Apple Silicon M1/M2/M3)**: [adnl_burn_pkg_4_macos_M1.7z](https://drive.google.com/file/d/1uBmCtziNwra9SzhraIiPQAXhHo6_j8mc/view?usp=drive_link)
   * **Windows**: [Windows Installer](https://drive.google.com/file/d/1zeqtMG1i88G9-GqJFgJeLPiom84tKM40/view?usp=sharing)

#### Host PC prerequisites and cable setup

##### Hardware connections

You must connect **two data-capable cables** between your host PC and the AH212 box:

1. **Micro-USB data cable** (connected to the `SERIAL` UART port on the AH212): Used for serial console output (`picocom`) and interrupting U-Boot at boot time to enter download mode.
2. **USB Type-C data cable** (connected to the `USB-C` OTG port on the AH212): Used by `adnl_burn_pkg` to transfer the system image and by `adb` for post-flash debugging.

> [!WARNING]
> **Beware of charging-only cables**
> * **Micro-USB (UART):** Verify that a serial device node appears under `ls /dev/ttyUSB*` (Linux) or `ls /dev/tty.usbserial*` (macOS), and that `lsusb` lists `Future Technology Devices International, Ltd FT232 Serial (UART) IC`.
> * **USB Type-C (Data and flashing):** When the device is in U-Boot `adnl` download mode, verify that `lsusb` lists `1b8e:c003 Amlogic, Inc.` (or that `adb devices` lists the device when booted into Linux). If no USB device is detected, your cable lacks data lines or is connected through an unsupported USB hub.

##### Host software setup

* **Linux (Ubuntu)**:
  1. Install the serial terminal emulator (`picocom`) and Android platform tools (`adb`):
     ```bash
     sudo apt update && sudo apt install -y picocom android-tools-adb unzip p7zip-full
     ```
  2. **(Optional) Configure USB udev permissions**: Copy `persistent-usb.rules` (included in the burning tool archive) to `/etc/udev/rules.d/` and reload udev rules (`sudo udevadm control --reload-rules && sudo udevadm trigger`), or invoke `adnl_burn_pkg` with **`sudo`**.
* **macOS**:
  1. Install `picocom`, `sevenzip` (for `.7z` extraction), `lsusb`, and Android platform tools (`adb`) via Homebrew:
     ```bash
     brew install picocom sevenzip lsusb
     brew install --cask android-platform-tools
     ```
  2. **Gatekeeper authorization**: When running `adnl_burn_pkg` on macOS for the first time, macOS Gatekeeper may block unsigned binaries. Open **System Settings > Privacy & Security**, click **Allow Anyway** when prompted, and re-run the command.

#### Step-by-step flashing instructions

Follow these steps to flash the AH212 using the U-Boot serial console and `adnl_burn_pkg`:

1. **Connect to the serial console**:
   Connect the Micro-USB cable from the AH212 `SERIAL` port to your host PC and start `picocom` at baudrate `921600`:
   ```bash
   # On Linux:
   sudo picocom /dev/ttyUSB0 --imap lfcrlf -b 921600

   # On macOS (replace with your serial device node):
   picocom /dev/tty.usbserial-A10L3PB6 --imap lfcrlf -b 921600
   ```

2. **Enter U-Boot download mode (`adnl`)**:
   * Unplug the power adapter from the AH212 box.
   * Click inside the active `picocom` terminal window and **press and hold the `Enter` key** on your keyboard.
   * While holding down `Enter`, plug the power adapter back into the AH212 box.
   * Release `Enter` once the bootloader stops at the U-Boot command prompt (`sc2_ah212#` or `axg_s400_v1#`).
   * At the U-Boot prompt, enter Amlogic download mode by running:
     ```text
     sc2_ah212# adnl
     ```
     Verify that `USB RESET` messages appear in the serial console output.

3. **Flash the system image from your host PC**:
   Ensure the **USB Type-C cable** is firmly connected between your host PC and the AH212 box. Open a **new terminal window** on your host PC and run `adnl_burn_pkg`:

   * **Full clean flash (Recommended when upgrading from Cobalt 25 or changing OS profiles)**:
     ```bash
     sudo /path/to/adnl_burn_pkg -r 1 -e 1 -p /path/to/aml_upgrade_package_20260420.img
     ```
   * **Flash while preserving the `/data` partition (`-e 0`)**:
     ```bash
     sudo /path/to/adnl_burn_pkg -r 1 -e 0 -p /path/to/aml_upgrade_package_20260420.img
     ```
   *(Command flags: `-p` specifies the image file path; `-r 1` reboots the device automatically after burning completes; `-e 1` erases all flash partitions for a clean install; `-e 0` preserves existing files in `/data`).*

   > [!TIP]
   > **Troubleshooting "Waiting for Amlogic DNL device ANY"**: If `adnl_burn_pkg` hangs at `Waiting for Amlogic DNL device ANY`, the host PC cannot communicate with the AH212 over USB-C. Reconnect the USB-C cable directly to a host USB port (avoid unpowered hubs) and verify that `lsusb` shows `1b8e:c003`.

4. **Verify the installed system image version**:
   Once the burn process finishes and the device reboots, connect via serial console, SSH, or `adb shell` and inspect `/version.txt`:
   ```bash
   cat /version.txt
   ```
   Confirm that the `imagename:` header is present and the `custom version` date is `20260420` (or newer) with WPEFramework plugin support:
   ```text
   imagename:lib32-rdk-ipstb-image_AMLOGIC_...
   ...
   =========== Customized ===========
   custom version: RDK_V6_AH212_20260420
   - Add support to run Cobalt as a WPEFramework plugin
   ```

5. **Initial device setup and remote control pairing**:
   * Connect an **Ethernet cable** to the AH212 so the device passes initial Out-of-Box Experience (OOBE) network checks.
   * Because a clean flash (`-e 1`) clears previous Bluetooth pairings in `/opt/persistent`, point the RDK remote directly at the box's front panel in **Infrared (IR) mode** (or plug in a **USB keyboard**) to navigate to **Settings > Pair Remote Control > Add a Device**, then hold `BACK + HOME` on the remote to pair via Bluetooth.

## Running in Evergreen Mode

In Cobalt 27 (Evergreen architecture), the platform-agnostic browser engine (`libcobalt.lz4`) is completely decoupled from the platform-specific hardware abstraction layer (**Starboard**).

When developing or testing Starboard changes on RDK, **do not compile the full Cobalt target** (which compiles over **41,260 targets** including Chromium, Blink, and V8). Instead, deploy an official **pre-built Cobalt 27 `.crx` package** and compile only the **RDK Starboard plugin shared library (`libloader_app.so`)**, which requires only **~4,150 targets** and completes in a few minutes.

### Deploying Official Google Prebuilt CRX Packages (Primary Flow)

#### Step 1: Download and unpack the official prebuilt CRX

Download an official pre-built **Cobalt 27 Evergreen package (`libcobalt.lz4`)** compiled for **Starboard API version 18 (`arm-hardfp`)**:

* **GitHub Releases**: Download the `arm-hardfp` Evergreen `.crx` release package from [youtube/cobalt GitHub Releases](https://github.com/youtube/cobalt/releases) (for example, `cobalt_evergreen_27.lts.<version>_arm-hardfp_qa.crx`).
* **Partner Distributions**: If you require a specific partner QA or certification `.crx` build not listed on GitHub Releases, please request the package from your Google point of contact.

Unpack the `.crx` archive on your **host PC**:

```bash
# On your host PC:
export LOCAL_CRX_DIR=/tmp/cobalt_prebuilt_extracted
rm -rf ${LOCAL_CRX_DIR} && mkdir -p ${LOCAL_CRX_DIR}

# Replace with the path to your downloaded arm-hardfp CRX package:
unzip /path/to/cobalt_evergreen_arm-hardfp_qa.crx -d ${LOCAL_CRX_DIR}
```

#### Step 2: Stage Slot 0 layout (`app/cobalt/`)

When running in custom plugin mode (`chCobalt custom_cobalt`), RDK loads Starboard from `/data/out_cobalt/libloader_app.so` and mounts **System Slot 0** (the read-only base Evergreen slot) from `/data/out_cobalt/app/cobalt/`.

> [!IMPORTANT]
> In Cobalt 27.lts, all Slot 0 factory binaries must be located strictly under `<target_root>/app/cobalt/` (which maps to `/data/out_cobalt/app/cobalt/` on RDK).

Stage the unpacked pre-built Cobalt files into a clean staging directory (`STAGE_DIR`) on your **host PC**:

```bash
# On your host PC:
export LOCAL_CRX_DIR=/tmp/cobalt_prebuilt_extracted
export STAGE_DIR=/tmp/rdk_custom_cobalt_stage
rm -rf ${STAGE_DIR}
mkdir -p ${STAGE_DIR}/app/cobalt/lib ${STAGE_DIR}/app/cobalt/content ${STAGE_DIR}/gen

cp -f ${LOCAL_CRX_DIR}/manifest.json ${STAGE_DIR}/app/cobalt/
cp -rf ${LOCAL_CRX_DIR}/lib/* ${STAGE_DIR}/app/cobalt/lib/
cp -rf ${LOCAL_CRX_DIR}/content/* ${STAGE_DIR}/app/cobalt/content/

# (Optional) Bump manifest.json version so Slot 0 takes precedence over cached OTA slots
# Note: Evergreen version strings must be < 20 chars total and each segment <= 9 digits (32-bit int)
python3 -c "import json; p='${STAGE_DIR}/app/cobalt/manifest.json'; d=json.load(open(p)); d['version']='99.99.99.9999999'; json.dump(d, open(p, 'w'), indent=2)"
```

#### Step 3: Configure GN and compile Starboard (`libloader_app.so`)

When building for Amlogic AH212 (which runs a 32-bit ARM hard-float RDK userland), target the `evergreen-arm-hardfp-rdk` platform configuration.

> [!IMPORTANT]
> **Target selection — Why `libloader_app.so` instead of `cobalt_loader`?**
> * **Do NOT build `cobalt_loader`:** In `cobalt/build/modular_executable.gni`, the `cobalt_loader` group depends on `cobalt_loaded_content`, which triggers a full compilation of `libcobalt.so` (**41,260 targets**).
> * **Do NOT build `loader_app_rdk_plugin`:** This legacy target name from older drafts has been removed from the Cobalt 27 codebase.
> * **Build `libloader_app.so`:** In the RDK platform configuration (`cobalt/build/configs/evergreen-arm-hardfp-rdk/args.gn`), `starboard_level_final_executable_type` is set to `"shared_library"`. Consequently, the Starboard `loader_app` target directly outputs **`libloader_app.so`** (the WPEFramework Thunder plugin library).

1. **Verify environment variables and generate GN build files**:
   ```bash
   # On your host PC:
   export PATH="$HOME/depot_tools:$PATH"
   export RDK_HOME=$HOME/rdk/toolchain

   cd ~/cobalt/src
   python3 cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c qa --no-rbe

   # (Optional) Enable ccache to accelerate subsequent rebuilds:
   grep -qxF 'cc_wrapper="ccache"' out/evergreen-arm-hardfp-rdk_qa/args.gn || echo 'cc_wrapper="ccache"' >> out/evergreen-arm-hardfp-rdk_qa/args.gn
   gn gen out/evergreen-arm-hardfp-rdk_qa
   ```

2. **Compile the Starboard plugin library and build metadata only**:
   Run `autoninja` targeting `libloader_app.so` and `cobalt:cobalt_build_info` (which generates `gen/build_info.json` without compiling Chromium):
   ```bash
   autoninja -C out/evergreen-arm-hardfp-rdk_qa libloader_app.so cobalt:cobalt_build_info
   ```

3. **Verify built Starboard artifacts**:
   Before packaging for the AH212 box, verify that `libloader_app.so` was generated and targets the 32-bit ARM hard-float ABI:
   ```bash
   # Check file presence and size:
   ls -lh out/evergreen-arm-hardfp-rdk_qa/libloader_app.so out/evergreen-arm-hardfp-rdk_qa/gen/build_info.json

   # Verify 32-bit ARM ELF header:
   file out/evergreen-arm-hardfp-rdk_qa/libloader_app.so

   # Verify Hard-Float ABI (Tag_ABI_VFP_args: VFP registers):
   readelf -A out/evergreen-arm-hardfp-rdk_qa/libloader_app.so | grep -E "Tag_CPU|Tag_ABI_VFP_args"
   ```
   *(For a `qa` build, `libloader_app.so` is stripped by default to **~1.2 MB**; the unstripped binary with DWARF debug symbols at `out/evergreen-arm-hardfp-rdk_qa/starboard/lib.unstripped/libloader_app.so` is **~31 MB**).*

#### Step 4: Package deployment tarball (`archive.tar.gz`)

Copy your locally compiled `libloader_app.so` and `gen/build_info.json` into `${STAGE_DIR}` alongside the staged `app/cobalt/` directory and create `~/archive.tar.gz`:

```bash
# On your host PC:
export OUT_DIR=~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa
export STAGE_DIR=/tmp/rdk_custom_cobalt_stage
mkdir -p ${STAGE_DIR}/gen

cp -f ${OUT_DIR}/libloader_app.so ${STAGE_DIR}/
cp -f ${OUT_DIR}/gen/build_info.json ${STAGE_DIR}/gen/

cd ${STAGE_DIR}
tar -czvf ~/archive.tar.gz libloader_app.so gen/build_info.json app/cobalt
```

##### Directory layout inside `archive.tar.gz`

```text
/data/out_cobalt/
├── libloader_app.so                <-- Locally built RDK Starboard plugin
├── gen/
│   └── build_info.json             <-- Starboard build metadata
└── app/
    └── cobalt/                     <-- Evergreen System Slot 0
        ├── manifest.json           <-- Pre-built CRX manifest
        ├── lib/
        │   └── libcobalt.lz4       <-- Pre-built Cobalt 27 browser engine
        └── content/                <-- Pre-built pak files, icudtl.dat, fonts, ssl certs
```

#### Step 5: Deploy, clear multi-app OTA cache, and activate via `chCobalt`

##### 5.1 Push and extract archive on device

Transfer `archive.tar.gz` from your host PC to the AH212 `/data` directory using `adb` or `scp`:

```bash
# On your host PC (Option A: Using ADB):
adb push ~/archive.tar.gz /data/archive.tar.gz

# On your host PC (Option B: Using SCP):
scp ~/archive.tar.gz root@<RDK_IP_ADDRESS>:/data/archive.tar.gz
```

Connect to the device console (`adb shell` or `ssh root@<RDK_IP_ADDRESS>`), stop `wpeframework` so no running Dobby container holds open file locks or flushes stale state, and extract into `/data/out_cobalt`:

```bash
# Run on the AH212 device shell:
systemctl stop wpeframework

rm -rf /data/out_cobalt && mkdir -p /data/out_cobalt
cd /data && tar -xzvf archive.tar.gz -C /data/out_cobalt
chmod -R 755 /data/out_cobalt

# Provide system fonts for Skia initialization in the Evergreen environment.
# Without this, SkFontMgr will fail due to missing font definitions in the WPE Dobby container.
cp -r /usr/share/content/data/fonts /data/out_cobalt/
```

##### 5.2 Clear multi-app Evergreen OTA cache (crucial for YouTube and YouTube TV)

On RDK, `/data/out_cobalt/app/cobalt` serves as **Slot 0** (the read-only system base slot). However, Evergreen stores Over-The-Air (OTA) updates in writable persistent slots (`installation_1`, `installation_2`) and tracks active slots in **isolated per-app protobuf state files** named after each application's Base64-encoded URL:

* **YouTube (`https://www.youtube.com/tv`)**: `installation_store_aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHY=.pb`
* **YouTube TV (`https://www.youtube.com/tv/upg`)**: `installation_store_aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHYvdXBn.pb`

> [!CAUTION]
> **Why YouTube or YouTube TV might ignore `/data/out_cobalt` without clearing cache**: Because YouTube and YouTube TV maintain independent `.pb` state files, if either application previously downloaded an OTA update (or if a stale `installation_store_*.pb` points to `installation_1`), `libloader_app.so` will load the cached binary from persistent storage instead of your newly deployed Slot 0 (`/data/out_cobalt/app/cobalt`). Deleting `installation_*` directories without deleting `installation_store_*.pb` will also prevent automatic roll-forward to Slot 0.

To guarantee that **both YouTube and YouTube TV** launch using your newly deployed Pre-built Cobalt + Starboard in `/data/out_cobalt`, remove all cached OTA installations and `.pb` state files on the device while `wpeframework` is stopped:

```bash
# Run on the AH212 device shell:
find /opt/persistent /home/root /data -maxdepth 5 -type d -name "installation_*" -exec rm -rf {} + 2>/dev/null
find /opt/persistent /home/root /data -name "installation_store_*.pb" -exec rm -f {} + 2>/dev/null
```

##### 5.3 Activate custom Cobalt via `chCobalt` and reboot

Run `chCobalt custom_cobalt` on the device shell to point WPEFramework's plugin symlinks (`/usr/lib/libloader_app.so`) and container mounts to `/data/out_cobalt`:

```bash
# Run on the AH212 device shell:
chCobalt custom_cobalt
sync
reboot -f
```

> [!IMPORTANT]
> **Always re-run `chCobalt custom_cobalt` after updating `/data/out_cobalt`**: You must execute `chCobalt custom_cobalt` every time you update or extract new files into `/data/out_cobalt` to refresh system symlinks and container mounts. To revert the device back to the factory default Cobalt 25 at any time, run `chCobalt c25 && reboot -f`.

#### Step 6: Launch and verify YouTube / YouTube TV

##### 6.1 Verify plugin symlink

After the device reboots, confirm that the system plugin symlink targets your custom Starboard library:

```bash
adb shell readlink -f /usr/lib/libloader_app.so
# Expected output:
# /data/out_cobalt/libloader_app.so
```

##### 6.2 Programmatically launch YouTube or YouTube TV via Thunder JSON-RPC

First, ensure any Wayland test display layer (`test-0`) is removed so it does not block WPEFramework rendering:

```bash
# Run on the AH212 device shell:
rdkDisplay remove 2>/dev/null || true
```

Use `curl` against WPEFramework's local JSON-RPC endpoint (`http://127.0.0.1:9998/jsonrpc`) on the device shell to launch or switch applications:

* **Launch YouTube (Main App)**:
  ```bash
  # Deactivate first (if running) to ensure a clean restart:
  curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.deactivate","params":{"callsign":"YouTube"}}'
  sleep 2
  # Activate YouTube plugin:
  curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"YouTube"}}'
  ```

* **Launch YouTube TV (`https://www.youtube.com/tv/upg`)**:
  Because `loader_app` selects the application's Evergreen state file (`installation_store_aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHYvdXBn.pb`) at process startup based on the `--url=` flag, use one of the following cold-launch methods:
  * **Method A (Via dedicated `YouTubeTV` callsign)**:
    If your RDK image configures `YouTubeTV` as a separate WPEFramework plugin callsign:
    ```bash
    curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"YouTubeTV"}}'
    ```
  * **Method B (Via `Controller.1.configuration@YouTube` URL override)**:
    If sharing the `YouTube` plugin callsign, update `sbmainargs` while deactivated so `loader_app` boots directly into YouTube TV:
    ```bash
    curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.deactivate","params":{"callsign":"YouTube"}}'
    sleep 2
    curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.configuration@YouTube","params":{"sbmainargs":["--url=https://www.youtube.com/tv/upg"]}}'
    curl -s 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"YouTube"}}'
    ```
    *(Note: Sending `YouTube.deeplink` only dispatches an in-app link event to an already running instance and does not switch the startup Evergreen slot).*
    *(Note: To restore the standard YouTube app after testing Method B, repeat the configuration command replacing `--url` with `https://www.youtube.com/tv`, or simply reload the default configuration via `systemctl restart wpeframework`.)*

##### 6.3 Verify active Cobalt and Starboard versions (3 methods)

1. **Via YouTube Test Suite (`yts`) CLI from host PC (Recommended)**:
   Query the active DIAL application and inspect the returned `User-Agent` string:
   ```bash
   # Check YouTube (Main App):
   yts check <DEVICE_IP_OR_ID>

   # Check YouTube TV:
   yts check <DEVICE_IP_OR_ID> --app YouTubeTV
   ```
   **Expected output**:
   ```text
   Successfully connected to the device.
   User-Agent: Mozilla/5.0 (RDK; Linux 5.15.137-amlogic) Cobalt/27.lts.<version>-qa (unlike Gecko) v8/13.8.258.54-jit gles Starboard/18, AMLOGIC_STB_AmlogicS905X4_2025/2.2 (RDKCommonPort, AH212)
   ```

2. **Via system journal logs (`journalctl`)**:
   Stream the system journal on the device using syslog tags while launching the app:
   ```bash
   journalctl -t YouTube -t Cobalt -t loader_app -t WPEFramework -f
   ```
   Verify that `loader_app` initializes from `/data/out_cobalt/libloader_app.so` and loads **Slot 0** (`/usr/share/content/data/app/cobalt/lib/libcobalt.lz4`, which maps to `/data/out_cobalt/app/cobalt/lib/libcobalt.lz4`).

3. **Via on-screen UI ("Stats for Nerds")**:
   Open **YouTube** or **YouTube TV** on the TV screen, navigate to **Settings > App Version** (or enable **Stats for Nerds** during video playback), and verify the displayed Cobalt and Starboard build numbers.

---

### Compiling Custom Cobalt Core from Source (For Core Engine Debugging Only)

> [!CAUTION]
> SoC and OEM partners are required to use official Google Prebuilt CRX packages for testing and certification. Compiling Cobalt Core (`libcobalt.so`) from source compiles over **41,260 targets** and is intended only for core developers debugging internal browser engine changes.

1. **Ensure environment variables are set and initialize the RDK Evergreen build directory**:

   ```bash
   # On your host PC:
   export PATH="$HOME/depot_tools:$PATH"
   export RDK_HOME=$HOME/rdk/toolchain
   cd ~/cobalt/src

   # Configure GN with -c qa so llvm-strip automatically strips libcobalt.so
   # (full DWARF symbols are preserved at out/evergreen-arm-hardfp-rdk_qa/lib.unstripped/libcobalt.so)
   python3 cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c qa --no-rbe
   ```

2. **Compile the Cobalt Core shared library, Slot 0 content, host LZ4 compressor, and RDK Starboard plugin**:

   ```bash
   autoninja -C out/evergreen-arm-hardfp-rdk_qa \
     cobalt:cobalt_loaded_content clang_x64/lz4_compress libloader_app.so cobalt:cobalt_build_info
   ```

3. **Compress `libcobalt.so` into `libcobalt.lz4` and stage `manifest.json`**:

   ```bash
   export OUT_DIR=~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa

   # Compress libcobalt.so into Slot 0 libcobalt.lz4
   ${OUT_DIR}/clang_x64/lz4_compress \
     ${OUT_DIR}/libcobalt.so \
     ${OUT_DIR}/app/cobalt/lib/libcobalt.lz4

   # Remove uncompressed/zstd duplicates so archive.tar.gz only ships libcobalt.lz4
   rm -f ${OUT_DIR}/app/cobalt/lib/libcobalt.so ${OUT_DIR}/app/cobalt/lib/libcobalt.zst

   # Stage Slot 0 manifest.json with high version so Slot 0 takes precedence
   cp -f ~/cobalt/src/cobalt/updater/version_manifest/manifest.json ${OUT_DIR}/app/cobalt/manifest.json
   python3 -c "import json; p='${OUT_DIR}/app/cobalt/manifest.json'; d=json.load(open(p)); d['version']='99.99.99.9999999'; json.dump(d, open(p, 'w'), indent=2)"
   ```

4. **Package `archive.tar.gz` and deploy to the AH212 device**:

   *(Unlike desktop Linux, you cannot execute `cobalt_loader.py` on an x86_64 host workstation for an ARM RDK build. Package your custom Slot 0 build into `~/archive.tar.gz` and deploy it to the AH212 device).*

   ```bash
   cd ${OUT_DIR}
   tar -czvf ~/archive.tar.gz libloader_app.so gen/build_info.json app/cobalt
   ```

   Then follow **Step 5 (Deploy, clear multi-app OTA cache, and activate via `chCobalt`)** and **Step 6 (Launch and verify YouTube / YouTube TV)** in the primary flow above to deploy `~/archive.tar.gz` to `/data/out_cobalt` and launch Cobalt via WPEFramework.

## Running Tests

The No Platform Left Behind (**NPLB**) test suite verifies the Starboard hardware abstraction implementation and is mandatory for certification. While Cobalt runs as a shared library plugin (`libloader_app.so`) under WPEFramework, NPLB executes as a standalone binary via `elf_loader_sandbox` on an isolated Wayland test display layer (`test-0`).

### 1. Compile the NPLB test suite and sandbox runner

Compile the `elf_loader_sandbox` runner executable and the `nplb` target (`libnplb.so` along with runtime test data `fonts/`, `icudtl.dat`, `ssl/`, and `test/`) without triggering a Chromium `.pak` build:

```bash
# On your host PC:
export PATH="$HOME/depot_tools:$PATH"
export RDK_HOME=$HOME/rdk/toolchain
cd ~/cobalt/src

autoninja -C out/evergreen-arm-hardfp-rdk_qa elf_loader_sandbox nplb
```

*(Note: Specify `elf_loader_sandbox nplb` rather than `nplb_loader` so Ninja does not pull in `//cobalt/shell:pak` and compile 14,000+ Chromium/Blink targets).*

### 2. Package and push NPLB artifacts to device

> [!IMPORTANT]
> **Why `fonts/fonts.xml` must be deployed at the top level for NPLB**: In `elf_loader_sandbox`, relative paths passed to `--evergreen_library` and `--evergreen_content` are resolved via `SbSystemGetPath(kSbSystemPathContentDirectory)`. On RDK (`system_get_path.cc`), Starboard only recognizes the executable's directory as a valid content directory if **`<dir>/fonts/fonts.xml`** exists at the top level; otherwise, it falls back to `/usr/share/content/data` and fails to locate `libnplb.so`. Always deploy NPLB into a dedicated directory (such as `/data/test`) alongside `fonts/`, `icudtl.dat`, `ssl/`, and `test/`.

On your **host PC**, archive and push `elf_loader_sandbox`, `libnplb.so`, and the required test data directories from your build output directory:

```bash
# On your host PC:
export OUT_DIR=~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa
cd ${OUT_DIR}
tar -czvf ~/nplb_archive.tar.gz elf_loader_sandbox libnplb.so icudtl.dat fonts ssl test

# Push to device via ADB or SCP:
adb push ~/nplb_archive.tar.gz /data/nplb_archive.tar.gz
```

Extract the test archive into `/data/test` on the AH212 device shell:

```bash
# Run on the AH212 device shell:
rm -rf /data/test && mkdir -p /data/test
cd /data && tar -xzvf nplb_archive.tar.gz -C /data/test
chmod -R 755 /data/test
```

### 3. Run NPLB on the device console

Run `elf_loader_sandbox` on the device console using `rdkDisplay` to allocate a Wayland test display layer (`test-0`):

```bash
# Run on the AH212 device shell:
cd /data/test

# 1. Clean up any stale display layer and create the test-0 Wayland display
rdkDisplay remove 2>/dev/null || true
sleep 2
rdkDisplay create
sleep 2

# 2. Run NPLB via elf_loader_sandbox (with optional gtest filters and XML output)
XDG_RUNTIME_DIR=/run WAYLAND_DISPLAY=test-0 ./elf_loader_sandbox \
  --evergreen_content=. \
  --evergreen_library=libnplb.so \
  --gtest_filter=*Posix* \
  --gtest_output=xml:/data/test/nplb_testResult.xml

# 3. Remove the test display layer and restore normal UI control
rdkDisplay remove
sleep 2
```

## Debugging

### 1. Streaming system and container logs (`journalctl`)

Cobalt `debug`, `devel`, and `qa` configurations support thread callstack tracing and Starboard `SB_LOG` output. Because Cobalt executes as a WPEFramework plugin (`libloader_app.so`) inside a Dobby container rather than an interactive terminal binary, `stdout`, `stderr`, and crash stacktraces are routed to `systemd-journald`.

To monitor real-time logs on the AH212 device shell:

```bash
journalctl -t YouTube -t Cobalt -t loader_app -t WPEFramework -f
```

### 2. Symbolicating crashes with unstripped binaries

In `qa` and `gold` builds, Ninja automatically strips deployed binaries (`libloader_app.so` is **~1.2 MB**) while preserving full DWARF debug symbols in `lib.unstripped/` on your host PC:

* **Starboard plugin (`libloader_app.so`)**: `out/evergreen-arm-hardfp-rdk_qa/starboard/lib.unstripped/libloader_app.so` (**~31 MB**)
* **Cobalt Core (`libcobalt.so`, if built from source)**: `out/evergreen-arm-hardfp-rdk_qa/lib.unstripped/libcobalt.so`

Use `addr2line` from the RDK ARM toolchain on your host PC to resolve relative PC offsets from `journalctl` crash traces:

```bash
${RDK_HOME}/sysroots/x86_64-rdksdk-linux/usr/bin/arm-rdk-linux-gnueabi/arm-rdk-linux-gnueabi-addr2line \
  -Cfpe ~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa/starboard/lib.unstripped/libloader_app.so <OFFSET>
```

### 3. Troubleshooting common build issues

| Symptom or error message | Root cause | Resolution |
| :--- | :--- | :--- |
| `Assertion failed: RDK builds require the 'RDK_HOME' environment variable to be set.` or missing headers (`glib.h`, `WPEFrameworkCore`) | `RDK_HOME` is unset or points to an invalid path. | Verify that `echo $RDK_HOME` points to the folder containing `sysroots/` (for example, `$HOME/rdk/toolchain`). Re-run `gn gen` after exporting `RDK_HOME`. |
| Ninja compiles **41,000+ targets** and takes hours | `cobalt_loader` or `cobalt` was passed to `autoninja`. | Abort (`Ctrl+C`) and specify only `libloader_app.so cobalt:cobalt_build_info`. |
| `ninja: error: unknown target 'loader_app_rdk_plugin'` | Outdated target name from older documentation. | Specify **`libloader_app.so`** instead. |
| `ninja: error: unknown target 'lz4_compress'` | Host vs. target toolchain mismatch when manually building compression tools. | Specify the host toolchain explicitly: `clang_x64/lz4_compress`. |

### 4. Troubleshooting common runtime and deployment issues

| Symptom on AH212 device | Root cause | Resolution |
| :--- | :--- | :--- |
| YouTube or YouTube TV launches an older Cobalt version instead of `/data/out_cobalt` | Stale per-app OTA state files (`installation_store_*.pb` / `installation_*`) remain in `/opt/persistent`, or `chCobalt custom_cobalt` was not re-run. | Stop `wpeframework`, delete all `installation_*` directories and `installation_store_*.pb` files (Step 5.2), and run `chCobalt custom_cobalt && reboot -f`. |
| App flashes briefly and crashes on startup inside `SkFontMgr` | Missing `/data/out_cobalt/fonts` inside the WPE Dobby container (`kSbSystemPathFontDirectory`). | Copy the device system fonts into `/data/out_cobalt/`: `cp -r /usr/share/content/data/fonts /data/out_cobalt/`. |
| Black screen when launching YouTube via Thunder JSON-RPC | A leftover Wayland `test-0` display layer from a previous NPLB run is covering the screen. | Run `rdkDisplay remove` on the device shell before activating `YouTube`. |
| `elf_loader_sandbox` fails to locate `libnplb.so` in `/data/test` | `<dir>/fonts/fonts.xml` is missing from `/data/test`, causing `SbSystemGetPath` to fall back to `/usr/share/content/data`. | Ensure `fonts/`, `icudtl.dat`, `ssl/`, and `test/` are packaged and extracted alongside `elf_loader_sandbox` and `libnplb.so` in `/data/test`. |

## Clean up or reset the environment

1. **Clean the host build directory**:
   To remove compiled artifacts on your host PC while preserving your `args.gn` configuration:
   ```bash
   cd ~/cobalt/src
   gn clean out/evergreen-arm-hardfp-rdk_qa
   ```

2. **Reset the AH212 device to factory Cobalt**:
   To switch the AH212 box from `/data/out_cobalt` (`custom_cobalt`) back to the system's factory default Cobalt 25 image:
   ```bash
   # Run on the AH212 device shell:
   chCobalt c25 && reboot -f
   ```
