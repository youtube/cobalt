---
layout: doc
title: "Set up your environment - Raspberry Pi"
---

These instructions explain how to set up Cobalt for your workstation and your
Raspberry Pi device. They have been tested with Ubuntu:20.04 and a Raspberry Pi
3 Model B.

## Set up your device

Download the latest Cobalt customized Raspbian image from <a
href="https://storage.googleapis.com/cobalt-static-storage/2020-02-13-raspbian-buster-lite_shrunk_20210427.img">GCS bucket</a>
(this is built via <a
href="https://github.com/youtube/cobalt/tree/master/tools/raspi_image#readme">this
customization tool</a>)

On MacOS, use an image flashing tool like <a href="https://www.balena.io/etcher/">balenaEtcher</a> to write the image to a 32GB SD-card.

On Linux, follow the steps below.

Check the location of your SD card (/dev/sdX or /dev/mmcblkX)

```
$ sudo fdisk -l
```
Make sure the card isn't mounted ( `umount /dev/sdX` ).

Copy the downloaded image to your SD card (the disk, not the partition. E.g. /dev/sdX or /dev/mmcblkX):

```
$ sudo dd bs=4M if=2020-02-13-raspbian-buster-lite_shrunk_20210427.img of=/dev/sdX
```
After flashing your device, you'll still need to setup your wifi. Login with the
default pi login, and run `sudo raspi-config`. You'll find wifi settings under
`1. System Options`, then `S1 Wireless LAN`.


## Set up your workstation

<aside class="note">
<b>Note:</b> Before proceeding further, refer to the documentation for
<a href="setup-linux.html">"Set up your environment - Linux"</a>. Complete the
sections <b>Set up your workstation</b> and <b>Set up developer tools</b>, then
return and complete the following steps.
</aside>

The following steps install the cross-compiling toolchain on your workstation.
The toolchain runs on an x86 workstation and compiles binaries for your ARM
Raspberry Pi.

1.  Run the following command to install packages needed to build and run
    Cobalt for Raspberry Pi:

    ```
    $ sudo apt install -qqy --no-install-recommends g++-multilib \
        wget xz-utils libxml2  binutils-aarch64-linux-gnu \
        binutils-arm-linux-gnueabi  libglib2.0-dev
    ```

1.  Choose a location for the installed toolchain &ndash; e.g. `raspi-tools`
    &ndash; and set `$RASPI_HOME` to that location.

1.  Add `$RASPI_HOME` to your `.bashrc` (or equivalent).

1.  Create the directory for the installed toolchain and go to it:

    ```
    $ mkdir -p $RASPI_HOME
    $ cd $RASPI_HOME
    ```

1.  Download the pre-packaged toolchain and extract it in `$RASPI_HOME`.

    ```
    $ curl -O https://storage.googleapis.com/cobalt-static-storage/cobalt_raspi_tools.tar.bz2
    $ tar xvpf cobalt_raspi_tools.tar.bz2
    ```

    (This is a combination of old raspi tools and a newer one from linaro
     to support older Raspbian Jessie and newer Raspbian Buster)

## Build, install, and run Cobalt for Raspberry Pi

1.  Build the code by navigating to the `cobalt` directory and run the
    following command:

    ```
    $ cobalt/build/gn.py -p raspi-2
    ```

1.  Compile the code from the `cobalt/` directory:

    ```
    $ ninja -C out/raspi-2_debug cobalt
    ```

1.  Run the following command to install your Cobalt binary (and content)
    on the device:

    ```
    $ rsync -avzLPh --exclude="obj*" --exclude="gen/" \
          $COBALT_SRC/out/raspi-2_debug pi@$RASPI_ADDR:~/
    ```

    The `rsyncs` get somewhat faster after the first time, as `rsync` is good at
    doing the minimum necessary effort.

1.  Even if you have a keyboard hooked up to the RasPi, you should SSH
    into the device to run Cobalt. Using SSH will make it easy for you
    to quit or restart Cobalt.

    ```
    $ ssh pi@$RASPI_ADDR
    $ cd raspi-2_debug
    $ ./cobalt
    ```

    With this approach, you can just hit `[CTRL-C]` to close Cobalt. If you
    were to run it from the console, you would have no way to quit without
    SSHing into the device and killing the Cobalt process by its PID.

    Note that you can also exit YouTube on Cobalt by hitting the `[Esc]` key
    enough times to bring up the "Do you want to quit YouTube?" dialog and
    selecting "yes".
