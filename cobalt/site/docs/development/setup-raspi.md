---
layout: doc
title: "Set up your environment - Raspberry Pi"
---

These instructions explain how to set up Cobalt for your workstation and your
Raspberry Pi device.

## Set up your device

<aside class="note">
<b>Note:</b> Raspberry Pi <em>cannot</em> have MesaGL installed and will return
an error, like `DRI2 not supported` or `DRI2 failed to authenticate` if MesaGL
is installed.
</aside>

<aside class="note">
<b>Note:</b> The current builds of Cobalt currently are verified to work only
with a fairly old version of Raspbian Lite from 2017-07-05 (
<a href="http://downloads.raspberrypi.org/raspbian_lite/images/raspbian_lite-2017-07-05/2017-07-05-raspbian-jessie-lite.zip">download link</a>
).  If you have a newer version, you may encounter linker errors when building
Cobalt as the sysroot system libraries will differ in the latest version of
Raspbian.
</aside>

Configure the Raspberry Pi memory split.

1.  `sudo raspi-config`
1.  Go to Advanced
1.  Memory Split: 256 for RasPi-0, 512 for all others.

Cobalt assumes the Raspberry Pi is configured to use non-default thread
schedulers and priorities. Ensure that **/etc/security/limits.conf** sets
**rtprio** and **nice** limits for the user. For example, if the user is **pi**,
then limits.conf should have the following lines:

```
@pi hard rtprio 99
@pi soft rtprio 99
@pi hard nice -20
@pi soft nice -20
```

The following commands update the package configuration on your Raspberry Pi
so that Cobalt can run properly:

```
$ apt-get remove -y --purge --auto-remove libgl1-mesa-dev \
          libegl1-mesa-dev libgles2-mesa libgles2-mesa-dev
$ apt-get install -y libpulse-dev libasound2-dev libavformat-dev \
          libavresample-dev rsync
```

## Set up your workstation

<aside class="note">
<b>Note:</b> Before proceeding further, refer to the documentation for <a href="setup-linux.html">"Set up your environment - Linux"</a>. Complete the section **Set up your workstation**, then return and complete the following steps.
</aside>

The following steps install the cross-compiling toolchain on your workstation.
The toolchain runs on an x86 workstation and compiles binaries for your ARM
Raspberry Pi.

1.  Run the following command to install packages needed to build and run
    Cobalt for Raspberry Pi:

    ```
    $ sudo apt install -qqy --no-install-recommends g++-multilib \
          python-requests wget xz-utils
    ```

1.  Choose a location for the installed toolchain &ndash; e.g. `raspi-tools`
    &ndash; and set `$RASPI_HOME` to that location.

1.  Add `$RASPI_HOME` to your `.bashrc` (or equivalent).

1.  Create the directory for the installed toolchain and go to it:

    ```
    mkdir -p $RASPI_HOME
    cd $RASPI_HOME
    ```

1.  Clone the GitHub repository for Raspberry Pi tools:

    ```
    git clone git://github.com/raspberrypi/tools.git
    ```

1.  Sync your sysroot by completing the following steps:

    1.  Boot up your RasPi, and set `$RASPI_ADDR` to the device's IP address.
    1.  Run `mkdir -p $RASPI_HOME/sysroot`
    1.  Run:

        ```
        rsync -avzh --safe-links \
              --delete-after pi@$RASPI_ADDR:/{opt,lib,usr} \
              --exclude="lib/firmware" --exclude="lib/modules" \
              --include="usr/lib" --include="usr/include" \
              --include="usr/local/include" --include="usr/local/lib" \
              --exclude="usr/*" --include="opt/vc" --exclude="opt/*" \
              $RASPI_HOME/sysroot
        password: raspberry
        ```

## Build, install, and run Cobalt for Raspberry Pi

<aside class="note">
<b>Note:</b> If you've unnested the 'src' directory, build and compile the code
in your top-level checkout of Cobalt instead of the 'src' subdirectory.
</aside>

1.  Build the code by navigating to the src directory in your cobalt directory and run the
    following command :

    ```
    $ cobalt/build/gyp_cobalt raspi-2
    ```

1.  Compile the code from the `src/` directory:

    ```
    $ ninja -C out/raspi-2_debug cobalt
    ```

1.  Run the following command to install your Cobalt binary (and content)
    on the device:

    ```
    rsync -avzLPh --exclude="obj*" --exclude="gen/" \
          $COBALT_SRC/out/raspi-2_debug pi@$RASPI_ADDR:~/
    ```

    The `rsyncs` get somewhat faster after the first time, as `rsync` is good at
    doing the minimum necessary effort.

1.  Even if you have a keyboard hooked up to the RasPi, you should SSH
    into the device to run Cobalt. Using SSH will make it easy for you
    to quit or restart Cobalt.

    ```
    ssh pi@$RASPI_ADDR
    cd raspi-2_debug
    ./cobalt
    ```

    With this approach, you can just hit `[CTRL-C]` to close Cobalt. If you
    were to run it from the console, you would have no way to quit without
    SSHing into the device and killing the Cobalt process by its PID.

    Note that you can also exit YouTube on Cobalt by hitting the `[Esc]` key
    enough times to bring up the "Do you want to quit YouTube?" dialog and
    selecting "yes".

### Improving Cobalt performance on Raspberry Pi

1.  You will find that there are some processes installed by default that run on the
    Raspberry Pi and can take away CPU time from Cobalt. You may wish to consider
    disabling these processes for maximum (and more consistent) performance, as they
    have been found to occasionally take >10% of the CPU according to `top`.
    You can do this by typing:

    ```
    apt-get remove -y --auto-remove [PACKAGE_NAME, ...]
    ```

    For example:

    ```
    apt-get remove -y --auto-remove avahi-daemon
    ```
