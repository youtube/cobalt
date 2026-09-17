Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment: Cobalt 27.lts for RDK

These instructions explain how to flash the Amlogic S905X4 (AH212) reference device, set up a Linux build environment (Ubuntu 22.04 LTS), compile **only** the RDK Starboard layer (`libloader_app.so`), combine it with a **pre-built Cobalt 27 (Evergreen)** binary package, and deploy and verify both **YouTube** and **YouTube TV** on the device.

The source for the RDK Starboard implementation originates in the [RDK Central repository](https://github.com/rdkcentral/larboard), which Cobalt merges and customizes under [`starboard/contrib/rdk/`](https://github.com/youtube/cobalt/tree/27.lts/starboard/contrib/rdk).

---

## Part 1: Flashing the Amlogic AH212 reference device

### Why you must flash a new system image

To run **Cobalt 26 / 27.lts (Evergreen architecture)** on an Amlogic S905X4 (AH212) reference device, you **must** flash a customized RDK6 system image (build date **`20260420` or newer**).

Stock or older RDK images (such as factory images built for Cobalt 25) cannot run Cobalt 27 properly for the following reasons:

1. **WPEFramework (Thunder) plugin architecture**:
   * Legacy development workflows launched Cobalt as a standalone executable (`./loader_app`) by stopping the UI or manually exporting dozens of `WESTEROS_GL_*` environment variables. However, standalone executable mode lacks support for standard TV lifecycle events (**suspend and resume**) and breaks **DRM/OCDM video decryption** (which relies on WPEFramework middleware).
   * In Cobalt 26 and 27+, Cobalt runs natively as a **WPEFramework plugin** (`libloader_app.so`) inside a Dobby container. Only the updated RDK6 customized images include the required WPEFramework plugin configuration (`Cobalt.json`), container mounts, and the **`chCobalt`** version-switching utility.
2. **System software compatibility**:
   * RDK6 images dated `20260420` or newer provide the required container mount points (`/data/out_cobalt` mapped to `/usr/share/content/data` inside the plugin container) and Wayland display management tools (`rdkDisplay`).

### Where to download the system image and flashing tools

#### 1. RDK system image (`aml_upgrade_package.img`)

* **Developer system image repository**: Download the latest customized RDK6 system image (recommended: `aml_upgrade_package_20260420.img` or newer) from the [RDK Developer Images Google Drive folder](https://drive.google.com/drive/folders/1BnzCFLoceTFFkiTK74aFLsHJukeNa0EP?resourcekey=0-Crb3ms5c7BGy9b-ZB_cbPw). Please contact your Google point of contact (or Technical Account Manager) if you require access permissions to this folder.
* **YouTube Partner Portal and RDK Central resources**: Device compliance specifications and reference documentation can also be found on the [YouTube Living Room Compliance - AH212 page](https://developers.google.com/youtube/devices/living-room/compliance/ah212) (see [Accessing Living Room Partnership Resources](https://developers.google.com/youtube/devices/living-room/access/accessing-lr-partnership-resources) if you need access) and the [RDK Central Amlogic Reference Board Wiki](https://wiki.rdkcentral.com/display/RDK/RDK-Google+IPSTB+profile+stack+on+Amlogic+reference+board).

#### 2. Amlogic USB burning tool (`adnl_burn_pkg`)

Download and extract the flashing utility corresponding to your host PC operating system:

* **Linux (Ubuntu x86_64)**: [adnl_burn_pkg_4_ubuntu.zip](https://drive.google.com/file/d/1b9ibJTD1K4AFuDeBB-999VjzJiJrf0E6/view?usp=drive_link)
* **macOS (Apple Silicon M1/M2/M3)**: [adnl_burn_pkg_4_macos_M1.7z](https://drive.google.com/file/d/1uBmCtziNwra9SzhraIiPQAXhHo6_j8mc/view?usp=drive_link)
* **Windows**: [Windows Installer](https://drive.google.com/file/d/1zeqtMG1i88G9-GqJFgJeLPiom84tKM40/view?usp=sharing)

### Host PC prerequisites and cable setup

#### Hardware connections

You must connect **two data-capable cables** between your host PC and the AH212 box:

1. **Micro-USB data cable** (connected to the `SERIAL` UART port on the AH212): Used for serial console output (`picocom`) and interrupting U-Boot at boot time to enter download mode.
2. **USB Type-C data cable** (connected to the `USB-C` OTG port on the AH212): Used by `adnl_burn_pkg` to transfer the system image and by `adb` for post-flash debugging.

<aside class="warning">
  <strong>Warning: Beware of charging-only cables</strong>
  <ul>
    <li><strong>Micro-USB (UART):</strong> Verify that a serial device node appears under <code>ls /dev/ttyUSB*</code> (Linux) or <code>ls /dev/tty.usbserial*</code> (macOS), and that <code>lsusb</code> lists <code>Future Technology Devices International, Ltd FT232 Serial (UART) IC</code>.</li>
    <li><strong>USB Type-C (Data and flashing):</strong> When the device is in U-Boot <code>adnl</code> download mode, verify that <code>lsusb</code> lists <code>1b8e:c003 Amlogic, Inc.</code> (or that <code>adb devices</code> lists the device when booted into Linux). If no USB device is detected, your cable lacks data lines or is connected through an unsupported USB hub.</li>
  </ul>
</aside>

#### Host software setup

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

### Step-by-step flashing instructions

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

   <aside class="tip">
     <strong>Tip: Troubleshooting "Waiting for Amlogic DNL device ANY"</strong>
     <p>If <code>adnl_burn_pkg</code> hangs at <code>Waiting for Amlogic DNL device ANY</code>, the host PC cannot communicate with the AH212 over USB-C. Reconnect the USB-C cable directly to a host USB port (avoid unpowered hubs) and verify that <code>lsusb</code> shows <code>1b8e:c003</code>.</p>
   </aside>

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

---

## Part 2: Building RDK Starboard only (without building full Cobalt)

In Cobalt 27 (Evergreen architecture), the platform-agnostic browser engine (`libcobalt.lz4`) is completely decoupled from the platform-specific hardware abstraction layer (**Starboard**).

When developing or testing Starboard changes on RDK, **do not compile the full Cobalt target** (which compiles over **41,260 targets** including Chromium, Blink, and V8). Instead, compile only the **RDK Starboard plugin shared library (`libloader_app.so`)**, which requires only **~4,150 targets** and completes in a few minutes.

### 1. Set up the Linux build environment (Ubuntu 22.04 LTS)

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

### 2. Clone Cobalt source code and sync dependencies

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

### 3. Generate GN configuration and compile Starboard (`libloader_app.so`)

When building for Amlogic AH212 (which runs a 32-bit ARM hard-float RDK userland), target the `evergreen-arm-hardfp-rdk` platform configuration.

<aside class="important">
  <strong>Important: Target selection — Why <code>libloader_app.so</code> instead of <code>cobalt_loader</code>?</strong>
  <ul>
    <li><strong>Do NOT build <code>cobalt_loader</code>:</strong> In <code>cobalt/build/modular_executable.gni</code>, the <code>cobalt_loader</code> group depends on <code>cobalt_loaded_content</code>, which triggers a full compilation of <code>libcobalt.so</code> (<strong>41,260 targets</strong>).</li>
    <li><strong>Do NOT build <code>loader_app_rdk_plugin</code>:</strong> This legacy target name from older drafts has been removed from the Cobalt 27 codebase.</li>
    <li><strong>Build <code>libloader_app.so</code>:</strong> In the RDK platform configuration (<code>cobalt/build/configs/evergreen-arm-hardfp-rdk/args.gn</code>), <code>starboard_level_final_executable_type</code> is set to <code>"shared_library"</code>. Consequently, the Starboard <code>loader_app</code> target directly outputs <strong><code>libloader_app.so</code></strong> (the WPEFramework Thunder plugin library).</li>
  </ul>
</aside>

1. **Verify environment variables**:
   ```bash
   export PATH="$HOME/depot_tools:$PATH"
   export RDK_HOME=$HOME/rdk/toolchain
   ```

2. **Generate GN build files**:
   ```bash
   cd ~/cobalt/src
   python3 cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c qa --no-rbe

   # (Optional) Enable ccache to accelerate subsequent rebuilds:
   grep -qxF 'cc_wrapper="ccache"' out/evergreen-arm-hardfp-rdk_qa/args.gn || echo 'cc_wrapper="ccache"' >> out/evergreen-arm-hardfp-rdk_qa/args.gn
   gn gen out/evergreen-arm-hardfp-rdk_qa
   ```

3. **Compile the Starboard plugin library and build metadata only**:
   Run `autoninja` targeting `libloader_app.so` and `cobalt:cobalt_build_info` (which generates `gen/build_info.json` without compiling Chromium):
   ```bash
   autoninja -C out/evergreen-arm-hardfp-rdk_qa libloader_app.so cobalt:cobalt_build_info
   ```
   * **Optional Starboard NPLB test targets**: To also compile the Starboard No-Platform-Left-Behind (NPLB) verification test suite (`libnplb.so`) and its standalone sandbox runner (`elf_loader_sandbox`) without triggering a Chromium pak build, append `elf_loader_sandbox nplb`:
     ```bash
     autoninja -C out/evergreen-arm-hardfp-rdk_qa libloader_app.so cobalt:cobalt_build_info elf_loader_sandbox nplb
     ```

### 4. Troubleshooting common build issues

| Symptom or error message | Root cause | Resolution |
| :--- | :--- | :--- |
| `Error: RDK toolchain is not set up in RDK_HOME` or missing headers (`glib.h`, `WPEFrameworkCore`) | `RDK_HOME` is unset or points to an invalid path. | Verify that `echo $RDK_HOME` points to the folder containing `sysroots/` (for example, `$HOME/rdk/toolchain`). Re-run `gn gen` after exporting `RDK_HOME`. |
| Ninja compiles **41,000+ targets** and takes hours | `cobalt_loader` or `cobalt` was passed to `autoninja`. | Abort (`Ctrl+C`) and specify only `libloader_app.so cobalt:cobalt_build_info`. |
| `ninja: error: unknown target 'loader_app_rdk_plugin'` | Outdated target name from older documentation. | Specify **`libloader_app.so`** instead. |
| `ninja: error: unknown target 'lz4_compress'` | Host vs. target toolchain mismatch when manually building compression tools. | Specify the host toolchain explicitly: `clang_x64/lz4_compress`. |

### 5. Verify built Starboard artifacts

Before deploying to the AH212 box, verify that `libloader_app.so` was generated and targets the 32-bit ARM hard-float ABI:

1. **Check file presence and size**:
   ```bash
   ls -lh out/evergreen-arm-hardfp-rdk_qa/libloader_app.so out/evergreen-arm-hardfp-rdk_qa/gen/build_info.json
   ```
   *(For a `qa` build, `libloader_app.so` is stripped by default to **~1.2 MB**; the unstripped binary at `out/evergreen-arm-hardfp-rdk_qa/starboard/lib.unstripped/libloader_app.so` is **~31 MB**).*

2. **Verify 32-bit ARM ELF header**:
   ```bash
   file out/evergreen-arm-hardfp-rdk_qa/libloader_app.so
   ```
   **Expected output**:
   ```text
   out/evergreen-arm-hardfp-rdk_qa/libloader_app.so: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, ...
   ```

3. **Verify Hard-Float ABI (`Tag_ABI_VFP_args`)**:
   ```bash
   readelf -A out/evergreen-arm-hardfp-rdk_qa/libloader_app.so | grep -E "Tag_CPU|Tag_ABI_VFP_args"
   ```
   **Expected output**:
   ```text
   Tag_ABI_VFP_args: VFP registers
   ```

---

## Part 3: Combining pre-built Cobalt 27 + local Starboard and deploying to AH212

### 1. Where to download Cobalt 27 pre-built binaries (`.crx`)

Download an official pre-built **Cobalt 27 Evergreen package (`libcobalt.lz4`)** compiled for **Starboard API version 18 (`arm-hardfp`)**:

* **GitHub Releases**: Download the `arm-hardfp` Evergreen `.crx` release package from [youtube/cobalt GitHub Releases](https://github.com/youtube/cobalt/releases) (for example, `cobalt_evergreen_27.lts.<version>_arm-hardfp_qa.crx`).
* **Partner Distributions**: If you require a specific partner QA or certification `.crx` build not listed on GitHub Releases, please request the package from your Google point of contact.

### 2. Stage and package pre-built Cobalt with local Starboard

When running in custom plugin mode (`chCobalt custom_cobalt`), RDK loads Starboard from `/data/out_cobalt/libloader_app.so` and mounts **System Slot 0** (the read-only base Evergreen slot) from `/data/out_cobalt/app/cobalt/`.

Run the following commands on your **host PC** to combine your locally compiled `libloader_app.so` with the downloaded pre-built Cobalt `.crx` package:

```bash
# 1. Define build output and create a clean staging directory
export OUT_DIR=~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa
export STAGE_DIR=/tmp/rdk_custom_cobalt_stage
rm -rf ${STAGE_DIR}
mkdir -p ${STAGE_DIR}/app/cobalt/lib ${STAGE_DIR}/app/cobalt/content ${STAGE_DIR}/gen

# 2. Extract the downloaded pre-built Cobalt CRX archive
mkdir -p /tmp/cobalt_prebuilt_extracted
unzip /path/to/cobalt_evergreen_arm-hardfp_qa.crx -d /tmp/cobalt_prebuilt_extracted

# 3. Stage pre-built Cobalt core into System Slot 0 layout (app/cobalt/)
cp -f /tmp/cobalt_prebuilt_extracted/manifest.json ${STAGE_DIR}/app/cobalt/
cp -rf /tmp/cobalt_prebuilt_extracted/lib/* ${STAGE_DIR}/app/cobalt/lib/
cp -rf /tmp/cobalt_prebuilt_extracted/content/* ${STAGE_DIR}/app/cobalt/content/

# 4. Copy locally compiled Starboard plugin (libloader_app.so) and build metadata
cp -f ${OUT_DIR}/libloader_app.so ${STAGE_DIR}/
cp -f ${OUT_DIR}/gen/build_info.json ${STAGE_DIR}/gen/

# 5. (Optional) Bump manifest.json version so Slot 0 takes precedence over cached OTA slots
# Note: Evergreen version strings must be < 20 chars total and each segment <= 9 digits (32-bit int)
python3 -c "import json; p='${STAGE_DIR}/app/cobalt/manifest.json'; d=json.load(open(p)); d['version']='99.99.99.9999999'; json.dump(d, open(p, 'w'), indent=2)"

# 6. Create deployment tarball
cd ${STAGE_DIR}
tar -czvf ~/archive.tar.gz libloader_app.so gen/build_info.json app/cobalt
```

#### Directory layout inside `archive.tar.gz`

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

### 3. Deploy to AH212 and switch active Cobalt version for YouTube and YouTube TV

#### Step 1: Push and extract archive on device

Transfer `archive.tar.gz` to the AH212 `/data` directory using `adb` or `scp`:

```bash
# Option A: Using ADB
adb push ~/archive.tar.gz /data/archive.tar.gz

# Option B: Using SCP
scp ~/archive.tar.gz root@<RDK_IP_ADDRESS>:/data/archive.tar.gz
```

Connect to the device console (`adb shell` or `ssh root@<RDK_IP_ADDRESS>`) and extract into `/data/out_cobalt`:

```bash
rm -rf /data/out_cobalt && mkdir -p /data/out_cobalt
cd /data && tar -xzvf archive.tar.gz -C /data/out_cobalt
chmod -R 755 /data/out_cobalt

# Provide system fonts for Skia initialization in the Evergreen environment.
# Without this, SkFontMgr will fail due to missing font definitions in the WPE Dobby container.
cp -r /usr/share/content/data/fonts /data/out_cobalt/
```

#### Step 2: Clear multi-app Evergreen OTA cache (crucial for YouTube and YouTube TV)

On RDK, `/data/out_cobalt/app/cobalt` serves as **Slot 0** (the read-only system base slot). However, Evergreen stores Over-The-Air (OTA) updates in writable persistent slots (`installation_1`, `installation_2`) and tracks active slots in **isolated per-app protobuf state files** named after each application's Base64-encoded URL:

* **YouTube (`https://www.youtube.com/tv`)**: `installation_store_aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHY=.pb`
* **YouTube TV (`https://www.youtube.com/tv/upg`)**: `installation_store_aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHYvdXBn.pb`

<aside class="caution">
  <strong>Caution: Why YouTube or YouTube TV might ignore <code>/data/out_cobalt</code> without clearing cache</strong>
  <p>Because YouTube and YouTube TV maintain independent <code>.pb</code> state files, if either application previously downloaded an OTA update (or if a stale <code>installation_store_*.pb</code> points to <code>installation_1</code>), <code>libloader_app.so</code> will load the cached binary from persistent storage instead of your newly deployed Slot 0 (<code>/data/out_cobalt/app/cobalt</code>). Deleting <code>installation_*</code> directories without deleting <code>installation_store_*.pb</code> will also prevent automatic roll-forward to Slot 0.</p>
</aside>

To guarantee that **both YouTube and YouTube TV** launch using your newly deployed Pre-built Cobalt + Starboard in `/data/out_cobalt`, remove all cached OTA installations and `.pb` state files on the device:

```bash
# Run on the AH212 device shell:
find /opt/persistent /home/root /data -maxdepth 5 -type d -name "installation_*" -exec rm -rf {} + 2>/dev/null
find /opt/persistent /home/root /data -name "installation_store_*.pb" -exec rm -f {} + 2>/dev/null
```

#### Step 3: Activate custom Cobalt via `chCobalt` and reboot

Run `chCobalt custom_cobalt` on the device shell to point WPEFramework's plugin symlinks (`/usr/lib/libloader_app.so`) and container mounts to `/data/out_cobalt`:

```bash
chCobalt custom_cobalt
sync
reboot -f
```

<aside class="important">
  <strong>Important: Always re-run <code>chCobalt custom_cobalt</code> after updating <code>/data/out_cobalt</code></strong>
  <p>You must execute <code>chCobalt custom_cobalt</code> every time you update or extract new files into <code>/data/out_cobalt</code> to refresh system symlinks and container mounts. To revert the device back to the factory default Cobalt 25 at any time, run <code>chCobalt c25 &amp;&amp; reboot -f</code>.</p>
</aside>

---

### 4. Launch and verify both YouTube and YouTube TV

#### Step 1: Verify plugin symlink

After the device reboots, confirm that the system plugin symlink targets your custom Starboard library:

```bash
adb shell readlink -f /usr/lib/libloader_app.so
# Expected output:
# /data/out_cobalt/libloader_app.so
```

#### Step 2: Programmatically launch YouTube or YouTube TV via Thunder JSON-RPC

First, ensure any Wayland test display layer (`test-0`) is removed so it does not block WPEFramework rendering:

```bash
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

#### Step 3: Verify active Cobalt and Starboard versions (3 methods)

1. **Via `yts check` from host PC (Recommended)**:
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

## Part 4: Running NPLB tests with `elf_loader_sandbox`

While Cobalt runs as a shared library plugin (`libloader_app.so`) under WPEFramework, Starboard hardware verification tests (**NPLB**) execute as a standalone binary via `elf_loader_sandbox` on an isolated Wayland test display layer (`test-0`).

When you compile the `elf_loader_sandbox` and `nplb` targets (as shown in Part 2), Ninja outputs the `elf_loader_sandbox` runner executable, the `libnplb.so` shared library, and required runtime test data (`fonts/`, `icudtl.dat`, `ssl/`, and `test/`).

<aside class="important">
  <strong>Important: Why <code>fonts/fonts.xml</code> must be deployed at the top level for NPLB</strong>
  <p>In <code>elf_loader_sandbox</code>, relative paths passed to <code>--evergreen_library</code> and <code>--evergreen_content</code> are resolved via <code>SbSystemGetPath(kSbSystemPathContentDirectory)</code>. On RDK (<code>system_get_path.cc</code>), Starboard only recognizes the executable's directory as a valid content directory if <strong><code>&lt;dir&gt;/fonts/fonts.xml</code></strong> exists at the top level; otherwise, it falls back to <code>/usr/share/content/data</code> and fails to locate <code>libnplb.so</code>. Always deploy NPLB into a dedicated directory (such as <code>/data/test</code>) alongside <code>fonts/</code>, <code>icudtl.dat</code>, <code>ssl/</code>, and <code>test/</code>.</p>
</aside>

### 1. Package and push NPLB artifacts to device

On your **host PC**, archive and push `elf_loader_sandbox`, `libnplb.so`, and the required test data directories from your build output directory:

```bash
export OUT_DIR=~/cobalt/src/out/evergreen-arm-hardfp-rdk_qa
cd ${OUT_DIR}
tar -czvf ~/nplb_archive.tar.gz elf_loader_sandbox libnplb.so icudtl.dat fonts ssl test

# Push to device via ADB or SCP:
adb push ~/nplb_archive.tar.gz /data/nplb_archive.tar.gz
```

Extract the test archive into `/data/test` on the AH212 device shell:

```bash
rm -rf /data/test && mkdir -p /data/test
cd /data && tar -xzvf nplb_archive.tar.gz -C /data/test
chmod -R 755 /data/test
```

### 2. Run NPLB on the device console

Run `elf_loader_sandbox` on the device console using `rdkDisplay` to allocate a Wayland test display layer (`test-0`):

```bash
# On the AH212 device shell:
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
