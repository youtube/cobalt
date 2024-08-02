Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Android

These instructions explain how to set up Cobalt for your workstation and Android
device. The package being built here is referred to as CoAT (Cobalt on Android TV).

## Preliminary Setup

<aside class="note">
<b>Note:</b> Before proceeding further, refer to the documentation for
<a href="setup-linux.md">"Set up your environment - Linux"</a>. Complete the
sections <b>Set up your workstation</b> and <b>Set up developer tools</b>, then
return and complete the following steps.
</aside>

1.  Download and install [Android Studio](https://developer.android.com/studio/).

1.  To enable parallel gradle builds, add the following to your `~/.bashrc`:

    ```
    export COBALT_GRADLE_BUILD_COUNT=4
    ```

    Where 4 is the number of parallel threads. You can adjust the number of
    parallel threads according to how your workstation performs.

1.  Run `starboard/android/shared/download_sdk.sh` to download the SDK and NDK.
    The SDK and NDK will be downloaded and installed into
    `~/starboard-toolchains`. If you wish to customize the download location
    you must set the relevant environment variables accordingly.

    If prompted, read and accept the license agreement.

1.  Run `cobalt/build/gn.py -p android-x86` to configure the Cobalt build.
    (This step will have to be repeated with 'android-arm' or 'android-arm64'
    to target those architectures.)

    **Note:** If you have trouble building with an error referencing the
    `debug.keystore` you may need to set one up on your system:

    ```
    keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000
    ```

## Setup your device

Configure your device to be in developer mode:

1.  From `Settings`, in the `Device` row, select `About`.
1.  Scroll down to and click on `Build` several times until a toast appears with
    the message, "You are now a developer."
1.  In the newly added "developer options" settings menu, make sure USB
    debugging is enabled.

## Setup your workstation environment

For manually installing Android Studio and the SDK.

**Note:** Instructions moving forward are assuming a Linux environment.

1.  Complete the Preliminary Setup above.
1.  Launch Android Studio.
1.  Android Studio may still prompt to install an SDK if this is the very first
    time you've run it.
Go ahead and click 'yes' to open the SDK manager to install the following:
    *   Edit "Android SDK location" and set the path to:
    `$HOME/starboard-toolchains/AndroidSdk`
      *  NOTE: We use the same SDK for the IDE and the gyp/ninja build. This
         directory will already exist since you've already run gn gen for an
         android target, so you'll see a warning in the setup wizard that an SDK
         was detected, which is okay.
        *  Select both `Android SDK` and `Android SDK Platform` (whatever
           current version is presented should be fine)
    *   On the `SDK Platforms` tab select:
        *  Android API 28 (or whatever was already installed by default)
    *   On the "SDK Tools" tab select
(most of these should already be installed since you already ran gyp):
        1.  Android SDK Build-Tools (e.g. 28.0.3)
        1.  CMake
        1.  LLDB
        1.  Android Emulator
        1.  Android SDK Platform-Tools
        1.  Android SDK Tools
        1.  NDK
        1.  Support Repository > Android Support Repository
        1.  Support Repository > Google Repository
1.  At the welcome screen, choose to open an existing Android Studio project,
    and choose the project in your Cobalt repo at `starboard/android/apk` (just
    select the directory). This is "coat" (Cobalt On Android TV).
    *   NOTE: Do not let it update the 'Android Gradle Plugin' if it prompts for
        that, use the 'Don't remind me again' button. If you inadvertently let
        it upgrade, you can just revert the change to `build.gradle` in your git
        repo.
1.  You may see a popup "Unregistered VCS roots detected" indicating that it has
    detected the cobalt git repo. If you want to use git integration in
    Android Studio, you can add the roots, or if not then choose to ignore them.
1.  If you didn't already get prompted to install the SDK, do it now by going to
    Tools -> SDK Manager (or
    <img src="../../images/android-sdk-manager-icon.png" style="display:inline;"></img>
    ) on the toolbar) and making the same choices as shown in step 4.
1.  Make a new virtual device (= emulator) via
    Tools -> AVD Manager (or
    <img src="../../images/android-avd-manager-icon.png" style="display: inline;"></img>
    on the toolbar).
      *   Category: TV -> Android TV (720p)
      *   System image: Pie (API 28) x86 (you'll have to download the image)
          (The code should work on any API 21+, but there's a bug in the
          emulator preventing API 21 from working, but it does work on API 21
          hardware)
      *   You may be prompted to install some Intel virtualization drivers
          and/or change some BIOS settings to enable it, which will require you
          to reboot your machine.
1.  Run this AVD device. You can keep it running. Remember to restart it if your
    machine reboots. (Or you can start it when prompted for the target device if
    launching the app from Android Studio.)

## Basic Build, Install, and Run (command-line based)

1.  Complete the Preliminary Setup above
1.  Generate the cobalt.apk by building the "cobalt_install" target

    ```
    ninja -C out/android-x86_gold cobalt_install
    ```

    Output can be found in the corresponding `out/android-x86_gold` directory.

    **Note:** If you have trouble building with an error referencing the
    `debug.keystore` you may need to set one up on your system:

<<<<<<< HEAD
    ```
    keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000
=======
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
    mkdir -p $LOCAL_CRX_DIR

    # paste prebuilt library URL and Download it to /tmp
    # Please update URL according to your need
    COBALT_CRX_URL=https://github.com/youtube/cobalt/releases/download/24.lts.40/cobalt_evergreen_4.40.2_arm-softfp_sbversion-15_qa_20240426165046.crx

    wget $COBALT_CRX_URL -O $LOCAL_CRX_DIR/cobalt_prebuilt.crx

    # Unzip the downloaded CRX file
    unzip $LOCAL_CRX_DIR/cobalt_prebuilt.crx -d $LOCAL_CRX_DIR/cobalt_prebuilt
>>>>>>> 98a0b5e715e (Add commands to build Cobalt library locally (#3855))
    ```

1.  Install the resulting APK into your test device with adb:

<<<<<<< HEAD
    ```
    adb install out/android-x86_gold/cobalt.apk
=======
    ```sh
    cd $COBALT_SRC
    mkdir -p out/evergreen-arm-softfp_qa/install/lib
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* out/evergreen-arm-softfp_qa/
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* out/evergreen-arm-softfp_qa/install/lib
    cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/manifest.json out/evergreen-arm-softfp_qa/
    cp -rf $LOCAL_CRX_DIR/cobalt_prebuilt/content out/evergreen-arm-softfp_qa/
>>>>>>> 98a0b5e715e (Add commands to build Cobalt library locally (#3855))
    ```

1.  Start the application with:

    ```
    adb shell am start dev.cobalt.coat/dev.cobalt.app.MainActivity
    ```

1.  For command line parameters use the `--esa` flag to specify the "args" array
    as comma-separated values (with characters backslash-escaped as needed to
    make it through both the shell on your workstation and the shell on the
    device), e.g.:

    ```
    adb shell am start --esa args --flag_arg,--value_arg=something dev.cobalt.coat
    ```

1.  To monitor log output, watch logcat in another shell with a filter for
    starboard messages:

    ```
    adb logcat -s starboard:*
    ```

1.  To kill any existing running application process (even if it's no longer the
    active app) use:

    ```
    adb shell am force-stop dev.cobalt.coat
    ```

## Building/running/debugging (Android Studio IDE)

1.  Manually run `cobalt/build/gn.py -p android-x86` in a shell. (You should
    do this after each time you sync your repo)
1.  From the initial setup above, you should have opened the Android Studio
    project checked in at `starboard/android/apk`.
1.  In the sidebar on the left, you should see `app` appear as bolded top-level
    item.  If you don't see this, restart Android Studio.
1.  To run the app and attach the debugger: Run -> Debug 'app' (or
    <img src="../../images/android-debug-icon.png" style="display: inline;"></img>
    in the toolbar)
1.  If it's taking awhile, it's probably the ninja build. You can see that it is
    still processing by looking for a rotating spinner labeled "Gradle Build
    Running" on the bottom bar.

    **Note:** If you have trouble building with an error referencing the
    `debug.keystore` you may need to set one up on your system:

    ```
    keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000
    ```

1.  To add command line parameters add `--esa` to specify the "args" array as
    comma-separated values (with characters like '&' backslash-escaped to make
    it through the launch command) under:

    Run -> Edit Configurations… -> "app" -> General -> Launch Options -> Launch Flags

    e.g. To run with a different URL: `--esa args --url=<DIFFERENT_URL>`
1.  To monitor log output, see the `Logcat` on the bottom-left of the IDE. You
    can enter "starboard" in the search bubble to filter the output.
1.  To kill the app go to Run -> Stop, or click the red square stop button
    either on the top toolbar, or in the debugger on the bottom-left of the IDE.
1.  To set breakpoints in native code, just open the relevant source file with
    File -> Open… (sorry, files outside the apk project can't appear in the
    Project panel of the IDE) and click in the gutter of the relevant line.
    (Once you have one C++ file open, the path breadcrumbs at the top are useful
    to open other nearby files.)


## Running Tests

The test target itself (e.g. nplb) just builds an .so file (e.g. libnplb.so). To
run that on a device, it needs to be packaged into an APK, which is done by the
associated "install" target (e.g. nplb_install). The Starboard test runner does
all this for you, so just use that to build and run tests. For example, to
build and run "devel" NPLB on an ARM64 device, from the top-level directory:

```
starboard/tools/testing/test_runner.py -p android-arm64 -c devel -b -r -t nplb
```

<<<<<<< HEAD
If you want to debug a test, you can run it from Android Studio. Edit
`build.gradle` in the 'app' module (not to the one in the top 'apk' module) to
change `DEFAULT_COBALT_TARGET` to be the name of the test you want to debug
instead of 'cobalt'. Then you can set breakpoints, etc. in the test the same as
when debugging Cobalt.
=======
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

### Build and run nplb test apk

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

### Build and run nplb evergreen compat test apk

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
>>>>>>> 98a0b5e715e (Add commands to build Cobalt library locally (#3855))

## Debugging

Use `adb logcat` while Cobalt is running, or use `adb bugreport` shortly after
exiting to view Android logs. You will need to filter or search for
Cobalt-related output.

As with the Linux build, use the `debug`, `devel`, or `qa` configs to trace
Cobalt's callstacks.

### Build Cobalt library locally

**Partners should always use the [Google prebuilt binaries from GitHub](https://github.com/youtube/cobalt/releases)
for certification or software release.** However, for testing or debugging,
they can still build the library locally. Ex:

   ```sh
   # Create directory for evergreen-arm-softfp with build_type=qa and sb_api_version=15
   gn gen out/evergreen-arm-softfp_qa --args="target_platform=\"evergreen-arm-softfp\" use_asan=false target_cpu=\"arm\" build_type=\"qa\" sb_api_version=15"

   # Build Cobalt library
   ninja -C out/evergreen-arm-softfp_qa cobalt_install
   ```

   Once the Cobalt library is built, go back to [Compile Android APK using Ninja](#compile-android-apk-using-ninja) to build the APK.

## Removing the Cobalt Android Environment

1.  Unset ANDROID_HOME and or ANDROID_NDK_HOME in your shell and in .bashrc
1.  Delete the SDK:

    ```
    rm -rf ~/starboard-toolchains/AndroidSdk
    ```

1.  Delete NDK toolchains:

    ```
    rm -rf  ~/starboard-toolchains/android*
    ```

1.  Delete cached Android files:

    ```
    rm -rf ~/.android
    ```

    **NOTE:** Removing this directory will remove all signing keys even for
    different projects, so only delete this if you truly want to remove the
    entire Cobalt and Android Studio environment.
