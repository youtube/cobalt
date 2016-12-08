# Setting up Starboard to use DirectFB on a Raspberry Pi

Starboard's Blitter API contains a DirectFB implementation.  There does not
currently exist a supported Starboard configuration for DirectFB on the
Raspberry Pi, but it can be created easily by copying the raspi-1 configuration
and modifying the setup to use the Blitter API with DirectFB instead of Dispmanx
and EGL/GLES2.

You will also need to do some configuration of your Raspberry Pi's enviornment,
and for that you should consult with the following steps.

## Modifications to /boot/config.txt

Open '/boot/config.txt' (as root) and uncomment/add the lines:

    framebuffer_width=1920
    framebuffer_height=1080
    framebuffer_depth=32
    framebuffer_ignore_alpha=1

## Modifications to /etc/fb.modes

Open '/etc/fb.modes' (as root) and add the lines:

    mode "1920x1080-24p"
      geometry 1920 1080 1920 1080 32
      timings 13468 148 638 36 4 44 5
      hsync high
      vsync high
    endmode

## Modifications to /etc/directfbrc

Open or create 'etc/directfbrc' (as root) and add the lines:

    graphics-vt
    hardware
    mode=1920x1080
    depth=32
    pixelformat=ARGB

## Run fbset

At a terminal, execute the command:

    $ fbset -depth 32

Now, when you type

    $ fbset -i

You should see the output:

    mode "1920x1080"
        geometry 1920 1080 1920 1080 32
        timings 0 0 0 0 0 0 0
        rgba 8/16,8/8,8/0,8/24
    endmode

    Frame buffer device information:
        Name        : BCM2708 FB
        Address     : 0x3d402000
        Size        : 8294400
        Type        : PACKED PIXELS
        Visual      : TRUECOLOR
        XPanStep    : 1
        YPanStep    : 1
        YWrapStep   : 0
        LineLength  : 7680
        Accelerator : No

## Running your executable

At this point, you should be able to run your executable.  You will need to run
it as root.  You should be able to run as a normal user instead if you modify
/etc/udev/rules.d/99-input.rules by adding the following contents:

    SUBSYSTEM=="input", GROUP="input", MODE="0660"
    KERNEL=="tty[0-9]*", GROUP="root", MODE="0666"
    KERNEL=="mice", MODE="0666"
    KERNEL=="fb0", OWNER="root", MODE="0660"

## Sources

For more information, see https://taoofmac.com/space/blog/2013/01/09/2339.
