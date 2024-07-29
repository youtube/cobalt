Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Android

These instructions explain how to build Cobalt with evergreen enabled for your workstation and run the api on Android device.

## Build Instruction

<aside class="note">
<b>Note:</b> Before proceeding further, refer to the documentation for
<a href="setup-linux.md">"Set up your environment - Linux"</a>. Complete the
sections <b>Set up your workstation</b> and <b>Set up developer tools</b>, then
return and complete the following steps.
</aside>

1. Download source code and setup build environment

    Please checkout to the latest LTS version. Ex: 24.lts.40

    ```sh
    git clone https://github.com/youtube/cobalt.git
    cd cobalt
    export COBALT_SRC=${PWD}
    export PYTHONPATH=${PWD}
    export COBALT_USE_INTERNAL_BUILD=0
    git checkout tags/24.lts.40
    ```

1. Enter the top-level of the Cobalt directory

    ```sh
    cd $COBALT_SRC
    ```

1. Download the Starboard toolchain and Android SDK

    ```sh
    ./starboard/tools/download_clang.sh
    ./starboard/android/shared/download_sdk.sh
    ```
    If you encountered `Failed to find package patcher;v4` error, please edit starboard/android/shared/download_sdk.sh
    and remove `"patcher;v4" \ ` near line 53.

1. Install additional Linux packages

    ```sh
    sudo apt install binutils-arm-linux-gnueabi libgles2-mesa-dev mesa-common-dev
    ```

1. Make sure Android debug keystore is setup

    ```sh
    keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000
    ```

1. Enable python3 virtual environment

    ```sh
    python3 -m venv ~/.virtualenvs/cobalt_dev
    source ~/.virtualenvs/cobalt_dev/bin/activate
    ```

### Download and configure the official Google-built Cobalt binaries from GitHub

1. Create output directory for evergreen

    - Create the directory with arguments that meet the target device specification
      - Platform type, ex: arm-hardfp, arm-softfp, arm-64, etc
      - CPU type, ex: arm
      - Build type, ex: gold, qa
      - Starboard API version, ex: 15
    - An example to create directory for evergreen-arm-softfp with build_type=qa and sb_api_version=15
      ```sh
      gn gen out/evergreen-arm-softfp_qa --args="target_platform=\"evergreen-arm-softfp\" use_asan=false target_cpu=\"arm\" build_type=\"qa\" sb_api_version=15"
      ```

1. Select Google-prebuilt Cobalt binaries from [GitHub](https://github.com/youtube/cobalt/releases)

    - Choose the correct evergreen version based on the target device specification

      Please note that the selected prebuilt binary must meet the settings used to create evergreen directory in previous step.
      - Cobalt version you checked out, ex: 24.lts.40
      - Build type, ex: gold, qa
      - Starboard API version, ex: 15
    - Here is an example of 24.lts.40 with starboard API version 15
      - [`24.lts.40 release`](https://github.com/youtube/cobalt/releases/tag/24.lts.40)
      - `Gold version`: cobalt_evergreen_4.40.2_arm-softfp_sbversion-15_release_20240426165046.crx
      - `QA version`: cobalt_evergreen_4.40.2_arm-softfp_sbversion-15_qa_20240426165046.crx
    - For Cobalt 25 with starboard API version 16, you need to use compressed version
      - [`25.lts.1 release`](https://github.com/youtube/cobalt/releases/tag/25.lts.1)
      - `Gold version`: cobalt_evergreen_5.1.2_arm-softfp_sbversion-16_release_compressed_20240629001855.crx
      - `QA version`: cobalt_evergreen_5.1.2_arm-softfp_sbversion-16_qa_compressed_20240629001855.crx
    - Right click the file and copy file URL

1. Download and unzip the file

    ```sh
    export LOCAL_CRX_DIR=/tmp/cobalt_dl
    mkdir $LOCAL_CRX_DIR

    # paste prebuilt library URL and Download it to /tmp
    # Please update URL according to your need
    COBALT_CRX_URL=https://github.com/youtube/cobalt/releases/download/24.lts.40/cobalt_evergreen_4.40.2_arm-softfp_sbversion-15_qa_20240426165046.crx

    wget $COBALT_CRX_URL -O $LOCAL_CRX_DIR/cobalt_prebuilt.crx

    # Unzip the downloaded CRX file
    unzip $LOCAL_CRX_DIR/cobalt_prebuilt.crx -d $LOCAL_CRX_DIR/cobalt_prebuilt
    ```

1. Copy the files to the appropriate directories for building

    ```sh
    cd $COBALT_SRC
    mkdir out/evergreen-arm-softfp_qa/install/lib
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* out/evergreen-arm-softfp_qa/
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* out/evergreen-arm-softfp_qa/install/lib
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/manifest.json out/evergreen-arm-softfp_qa/
    cp -rf $LOCAL_CRX_DIR/cobalt_prebuilt/content out/evergreen-arm-softfp_qa/
    ```

### Compile Android APK using Ninja

1. Generate output folder

    ```sh
    gn gen out/android-arm_qa --args="target_platform=\"android-arm\" target_os=\"android\" target_cpu=\"arm\" build_type=\"qa\" sb_api_version=15 sb_is_evergreen_compatible=true"
    ```

    Note that we removed the `sb_evergreen_compatible_enable_lite` build param.
    To enable Evergreen-lite, you may either pass in command line flag `--evergreen_lite` or
    set the `cobalt.EVERGREEN_LITE` property to be `true` in the AndroidManifest.xml

1. Build Crashpad handler first

    ```sh
    ninja -C out/android-arm_qa native_target/crashpad_handler
    ```

1. Build loader_app APK

    ```sh
    ninja -C out/android-arm_qa loader_app_install
    ```

1. Check the output apk file. The output file is available at

    ```sh
    out/android-arm_qa/loader_app.apk
    ```

## Setup your device and deploy the apk

### Configure your device to be in developer mode

1.  From `Settings`, in the `System` row, select `About`
1.  Scroll down to and click on `Android TV OS build` several times until a toast appears with
    the message, "You are now a developer"
1.  In the newly added "Developer options" settings menu, make sure `USB`
    debugging is enabled

**NOTE:** This instruction is based on Chromecast (Google TV). If you do not find the same setting, please check similar setting under **System, About and Build**.

### Check the device is connected

1. Connect your workstation with the Android device via USB

1. Check device is connected

    ```sh
    adb devices
    # It shows the device if it is connected
    List of devices attached
    35091HFGN5HVC2  device
   ```

### Install the Evergreen loader APK on the device

```
adb install out/android-arm_qa/loader_app.apk
```

### Launch the APK

1.  Start the application with

    ```sh
    adb shell am start dev.cobalt.coat/dev.cobalt.app.MainActivity
    ```

1.  For command line parameters use the `--esa` flag to specify the "args" array
    as comma-separated values (with characters backslash-escaped as needed to
    make it through both the shell on your workstation and the shell on the
    device), e.g.:

    ```sh
    adb shell am start --esa args --flag_arg,--value_arg=something dev.cobalt.coat
    ```

1. For Evergreen-lite, two ways to launch it depending on how you enable it
    - Via command line flag

    ```sh
    adb shell "am start --esa args '--evergreen_lite=true' dev.cobalt.coat"
    ```

    - Via AndroidManifest.xml
    ```sh
    adb shell "am start dev.cobalt.coat"
    ```

1.  To monitor log output, watch logcat in another shell with a filter for
    starboard messages

    ```sh
    adb logcat -s starboard:*
    ```

1.  To kill any existing running application process (even if it's no longer the
    active app) use

    ```sh
    adb shell am force-stop dev.cobalt.coat
    ```


## Running Tests

There is no prebuilt nplb library on github server and the partners can build it
from the source code. The build target just builds an .so file (e.g. libnplb.so). To
run that on a device, it needs to be packaged into another loader APK.

### Build nplb library

Similar to loader_app, create the directory with arguments that meet the target device specification. Here is an example:
1. Generate evergreen folder

    ```sh
    gn gen out/evergreen-arm-softfp_devel --args="target_platform=\"evergreen-arm-softfp\" target_cpu=\"arm\" use_asan=false build_type=\"devel\" sb_api_version=15"
    ```

1. Build nplb library

    ```sh
    ninja -C out/evergreen-arm-softfp_devel nplb_install
    ```

1. Generate apk output folder

    ```sh
    gn gen out/android-arm_devel --args="target_platform=\"android-arm\" target_cpu=\"arm\" target_os=\"android\" sb_is_evergreen_compatible=true build_type=\"devel\" sb_api_version=15"
    ```

### Build and run nplb apk for evergreen

1. Build nplb apk

    ```sh
    ninja -C out/android-arm_devel nplb_evergreen_loader_install
    ```

1. Check the output apk file. The output file is available at

    ```sh
    out/android-arm_devel/nplb_evergreen_loader.apk
    ```

1. To run the nplb test, execute following command

    ```sh
    # install the apk
    adb install out/android-arm_devel/nplb_evergreen_loader.apk

    # launch the apk
    adb shell "am start --esa args '--evergreen_library=app/cobalt/lib/libnplb.so,--evergreen_content=app/cobalt/content' dev.cobalt.coat"
    ```

### Build and run nplb compat test apk for evergreen

1. Build nplb_evergreen_compat_tests apk

    **NOTE:** Please finish nplb build in previous step before building nplb compat test

    ```sh
    ninja -C out/android-arm_devel nplb_evergreen_compat_tests_install
    ```

1. Check the output apk file. The output file is available at

    ```sh
    out/android-arm_devel/nplb_evergreen_compat_tests.apk
    ```

1. To run the nplb compat test, execute following command

    ```sh
    # install the apk
    adb install out/android-arm_devel/nplb_evergreen_compat_tests.apk

    # launch the apk
    adb shell am start dev.cobalt.coat
    ```

## Debugging (Terminal)

Use `adb logcat` while Cobalt is running, or use `adb bugreport` shortly after
exiting to view Android logs. You will need to filter or search for
Cobalt-related output.

As with the Linux build, use the `debug`, `devel`, or `qa` configs to trace
Cobalt's callstacks.

## Removing the Cobalt Android Environment

1.  Unset ANDROID_HOME and or ANDROID_NDK_HOME in your shell and in .bashrc

1.  Delete the SDK

    ```sh
    rm -rf ~/starboard-toolchains/AndroidSdk
    ```

1.  Delete NDK toolchains

    ```sh
    rm -rf  ~/starboard-toolchains/android*
    ```

1.  Delete cached Android files

    ```sh
    rm -rf ~/.android
    ```

    **NOTE:** Removing this directory will remove all signing keys even for
    different projects, so only delete this if you truly want to remove the
    entire Cobalt and Android Studio environment.

1. Uninstall APK from device

   ```sh
   adb uninstall dev.cobalt.coat
   ```
