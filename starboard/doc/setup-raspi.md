# Set up your environment - Raspberry Pi

These instructions explain how to set up Cobalt for your workstation and your
Raspberry Pi device.

## Set up your Raspberry Pi

Download the latest Cobalt customized Raspbian image from [GCS bucket](https://storage.googleapis.com/cobalt-static-storage-public/2020-02-13-raspbian-buster-lite_shrunk_20210427.img) (this is built via [this customization tool](https://github.com/youtube/cobalt/tree/25.lts.1+/cobalt/tools/raspi_image))

On MacOS, use an image flashing tool like [balenaEtcher](https://www.balena.io/etcher/) to write the image to a 32GB SD-card.

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

The following steps install the cross-compiling toolchain on your workstation.
The toolchain runs on an x86 workstation and compiles binaries for your ARM
Raspberry Pi.

1.  Run the following command to install packages needed to build and run
    Cobalt for Raspberry Pi:

    ```
    $ sudo apt install -qqy --no-install-recommends g++-multilib \
        wget xz-utils libxml2 binutils-aarch64-linux-gnu \
        binutils-arm-linux-gnueabi libglib2.0-dev
    ```

2.  Choose a location for the installed toolchain &ndash; e.g. `raspi-tools`
    &ndash; and set `$RASPI_HOME` to that location.

3.  Add `$RASPI_HOME` to your `.bashrc` (or equivalent).

4.  Create the directory for the installed toolchain and go to it:

    ```
    $ mkdir -p $RASPI_HOME
    $ cd $RASPI_HOME
    ```

5.  Download the pre-packaged toolchain and extract it in `$RASPI_HOME`.

    ```
    $ curl -O https://storage.googleapis.com/cobalt-static-storage-public/cobalt_raspi_tools.tar.bz2
    $ tar xvpf cobalt_raspi_tools.tar.bz2
    ```

    (This is a combination of old raspi tools and a newer one from linaro
     to support older Raspbian Jessie and newer Raspbian Buster)

# Get the Cobalt Code

**This step diverges from Chromium's `fetch` command.** Instead of using `fetch`, please clone the Cobalt repository directly.

```sh
git clone https://github.com/youtube/cobalt.git cobalt/src
```

## Configure `gclient`

```sh
cd cobalt
gclient config --name=src https://github.com/youtube/cobalt.git
```

## Download and Sync Sub-repositories

Continue by changing the working directory to `src/`:
```sh
cd src
```
Further instructions always assume `cobalt/src` as the working directory.

```sh
gclient sync --no-history -r $(git rev-parse @)
```

**Note:** This is different from Chromium, the `-r` argument is critical for obtaining the correct version of dependencies.

## Install Build Dependencies

```sh
./build/install-build-deps.sh
```

# Build, install, and run Cobalt for Raspberry Pi

##  Config and Build
   Navigate to the `cobalt/src` directory and run the following commands

   Configure and Build Cobalt for raspi-2
   ```
   $ cobalt/build/gn.py --no-rbe -p evergreen-arm-hardfp-raspi --build_type devel
   ```
   Add "use_evergreen = false" to **out/evergreen-arm-hardfp-raspi_devel/args.gn** to turn on Modular
   ```
   $ echo "use_evergreen = false" >> out/evergreen-arm-hardfp-raspi_devel/args.gn
   ```
   Build cobalt
   ```
   $ autoninja -C out/evergreen-arm-hardfp-raspi_devel/ cobalt_loader
   ```
## Strip
   The devel build is 1.2 GB and is too big to load in raspi-2 so just strip it.
   ```
   $ cp out/evergreen-arm-hardfp-raspi_devel/libcobalt.so out/evergreen-arm-hardfp-raspi_devel/libcobalt_unstripped.so
   $ arm-linux-gnueabihf-strip out/evergreen-arm-hardfp-raspi_devel/libcobalt_unstripped.so -o out/evergreen-arm-hardfp-raspi_devel/libcobalt.so
   ```
## Package
   ```
   $ tar czpf cobalt.tgz -C out/evergreen-arm-hardfp-raspi_devel/ -T out/evergreen-arm-hardfp-raspi_devel/cobalt_loader.runtime_deps
   ```
## Copy to raspi-2
   ```
   $ export RASPI_ADDR=<ip address>
   # For organization, consider copying the tgz to a subfolder of ~/
   $ scp ./cobalt.tgz pi@$RASPI_ADDR:~/
   ```
## Extract cobalt on raspi-2
   ```
   $ tar zxpf cobalt.tgz
   ```
## Execute cobalt on raspi-2
   ```
   $ ./cobalt_loader
   ```
## Get stack trace with gdb
   ```
   $ export LD_LIBRARY_PATH=.
   $ gdb ./cobalt_loader
   $ (gdb) run
   $ Thread 10 "Chrome_InProcGp" received signal SIGSEGV, Segmentation fault.
   [Switching to Thread 0x6c4fe440 (LWP 1948)]
   0x00000000 in ?? ()
   (gdb) bt
   #0  0x00000000 in ?? ()
   #1  0x74192dca in ?? () from ./libcobalt.so
   #2  0x742150cc in ?? () from ./libcobalt.so
   #3  0x7418ae1a in ?? () from ./libcobalt.so
   #4  0x74201664 in ?? () from ./libcobalt.so
   #5  0x710b84a6 in ?? () from ./libcobalt.so
   #6  0x74233724 in ?? () from ./libcobalt.so
   #7  0x74232ad8 in ?? () from ./libcobalt.so
   #8  0x7423275e in ?? () from ./libcobalt.so
   #9  0x74232a4e in ?? () from ./libcobalt.so
   #10 0x749181e0 in ?? () from ./libcobalt.so
   #11 0x74a82042 in ?? () from ./libcobalt.so
   #12 0x73168aa6 in __cxa_end_cleanup () from ./libcobalt.so
   #13 0x73179976 in __cxa_end_cleanup () from ./libcobalt.so
   #14 0x769e8494 in start_thread (arg=0x6c4fe440) at pthread_create.c:486
   Backtrace stopped: Cannot access memory at address 0x26
   ```
   While still in gdb use a second terminal to get the base memory address of the cobalt_loader.
   This would be the first address in the process memory mapping of libcobalt.so. In this example 0x6fae5000.
   ```
   pi@raspberrypi:~ $ grep libcobalt /proc/$(pidof cobalt_loader)/maps
   6fae5000-70a92000 r--p 00000000 b3:02 128655     /home/pi/cobalt_modular/libcobalt.so
   70a92000-70aa2000 ---p 00fad000 b3:02 128655     /home/pi/cobalt_modular/libcobalt.so
   70aa2000-76622000 r-xp 00fad000 b3:02 128655     /home/pi/cobalt_modular/libcobalt.so
   76622000-76631000 ---p 06b3d000 b3:02 128655     /home/pi/cobalt_modular/libcobalt.so
   76631000-76866000 r--p 06b2c000 b3:02 128655     /home/pi/cobalt_modular/libcobalt.so
   76866000-76876000 ---p 06d81000 b3:02 128655     /home/pi/cobalt_modular/libcobalt.so
   76876000-768b3000 rw-p 06d61000 b3:02 128655     /home/pi/cobalt_modular/libcobalt.so
   pi@raspberrypi:~ $
   ```
   Run the symbolize script on the cloudtop:

   `stack_trace.txt`
   ```
   #0  0x00000000 in ?? ()
   #1  0x74192dca in ?? () from ./libcobalt.so
   #2  0x742150cc in ?? () from ./libcobalt.so
   #3  0x7418ae1a in ?? () from ./libcobalt.so
   #4  0x74201664 in ?? () from ./libcobalt.so
   #5  0x710b84a6 in ?? () from ./libcobalt.so
   #6  0x74233724 in ?? () from ./libcobalt.so
   #7  0x74232ad8 in ?? () from ./libcobalt.so
   #8  0x7423275e in ?? () from ./libcobalt.so
   #9  0x74232a4e in ?? () from ./libcobalt.so
   #10 0x749181e0 in ?? () from ./libcobalt.so
   #11 0x74a82042 in ?? () from ./libcobalt.so
   ```
   ```
   $ export PYTHONPATH=$(pwd)
   $ python3 starboard/tools/symbolize/symbolize.py  -f stack_trace.txt -l out/evergreen-arm-hardfp-raspi_devel/libcobalt.so 0x6fae5000
    #0  0x00000000 in ?? ()
    #1 0x46addca in b'gl::EGLApiBase::eglQueryStringFn(void*, int)' b'./../../ui/gl/gl_bindings_autogen_gl.cc:3674:1'
    #2 0x47300cc in b'gl::RealEGLApi::eglQueryStringFn(void*, int)' b'./../../ui/gl/gl_egl_api_implementation.cc:89:34'
    #3 0x46a5e1a in b'gl::ClientExtensionsEGL::GetClientExtensions()' b'./../../ui/gl/gl_bindings.cc:56:10'
    #4 0x471c664 in b'__is_long' b'./../../buildtools/third_party/libc++/trunk/include/string:1665:33'
    #5 0x15d34a6 in b'ui::GLOzoneEGL::InitializeStaticGLBindings(gl::GLImplementationParts const&)' b'./../../ui/ozone/common/gl_ozone_egl.cc:42:1'
    #6 0x474e724 in b'gl::init::InitializeStaticGLBindings(gl::GLImplementationParts)' b'./../../ui/gl/init/gl_initializer_ozone.cc:62:1'
    #7 0x474dad8 in b'gl::init::InitializeStaticGLBindingsImplementation(gl::GLImplementationParts, bool)' b'./../../ui/gl/init/gl_factory.cc:192:20'
    #8 0x474d75e in b'gl::init::InitializeStaticGLBindingsOneOff()' b'./../../ui/gl/init/gl_factory.cc:184:1'
    #9 0x474da4e in b'gl::init::InitializeGLNoExtensionsOneOff(bool, gl::GpuPreference)' b'./../../ui/gl/init/gl_factory.cc:159:9'
    #10 0x4e331e0 in b'gpu::GpuInit::InitializeInProcess(base::CommandLine*, gpu::GpuPreferences const&)' b'./../../gpu/ipc/service/gpu_init.cc:864:7'
    #11 0x4f9d042 in b'content::InProcessGpuThread::Init()' b'./../../content/gpu/in_process_gpu_thread.cc:70:3'
   ```

## Remote debugging with gdbserver
   It is a bit slow but is a fully functional gdb with symbols.

   Install on your host(below is an example using google innternal cloudtop)
   ```
   $ sudo apt-get install gdb-multiarch
   ```
   Install on raspi-2
   ```
   $ sudo apt-get install gdbserver
   ```
   Run gdbserver on raspi-2
   ```
   $ sudo LD_LIBRARY_PATH=. gdbserver localhost:3000 ./cobalt_loader
   ```
   Tunnel host(cloudtop) -- raspi-2 from a **laptop**
   ```
   $ ssh -R 9000:<raspi-ip>:3000 <cloudtop-hostname>
   ```
   Connect from the host(cloudtop)
   ```
   $ cd out/evergreen-arm-hardfp-raspi_devel/
   $ gdb-multiarch
   (gdb)  target remote  localhost:9000
   ```
