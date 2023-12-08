Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Android

These instructions explain how to set up Cobalt for your workstation and Android
device. The package being built here is referred to as CoAT (Cobalt on Android TV).

## Preliminary Setup

<aside class="note">
<b>Note:</b> Before proceeding further, refer to the documentation for
<a href="setup-linux.html">"Set up your environment - Linux"</a>. Complete the
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
    <img src="/images/android-sdk-manager-icon.png" style="display:inline;"></img>
    ) on the toolbar) and making the same choices as shown in step 4.
1.  Make a new virtual device (= emulator) via
    Tools -> AVD Manager (or
    <img src="/images/android-avd-manager-icon.png" style="display: inline;"></img>
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

    ```
    keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000
    ```

1.  Install the resulting APK into your test device with adb:

    ```
    adb install out/android-x86_gold/cobalt.apk
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
    <img src="/images/android-debug-icon.png" style="display: inline;"></img>
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

If you want to debug a test, you can run it from Android Studio. Edit
`build.gradle` in the 'app' module (not to the one in the top 'apk' module) to
change `DEFAULT_COBALT_TARGET` to be the name of the test you want to debug
instead of 'cobalt'. Then you can set breakpoints, etc. in the test the same as
when debugging Cobalt.

## Debugging (Terminal)

Use `adb logcat` while Cobalt is running, or use `adb bugreport` shortly after
exiting to view Android logs. You will need to filter or search for
Cobalt-related output.

As with the Linux build, use the `debug`, `devel`, or `qa` configs to trace
Cobalt's callstacks.

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
