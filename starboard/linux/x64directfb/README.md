# Starboard on DirectFB

Starboard features a configuration for DirectFB, allowing applications targeting
Starboard to be run on devices that support DirectFB.

Building Starboard to target DirectFB is setup as its own configuration which
must be specifically selected by gyp.  The configuration files can be found
in starboard/linux/x64directfb, the same directory as this README file.  The
configuration assumes that DirectFB is running on a Linux x64 platform.

## Building an application for Starboard DirectFB x64 Linux

First, ensure that the DirectFB libraries are installed and accessible.

    $ sudo apt-get install libdirectfb-dev libdirectfb-extra

Next, run gyp for this DirectFB configuration:

    $ cobalt/build/gyp_cobalt linux_x64directfb

And finally, run ninja to build the application:

    $ ninja -C out/linux-x64directfb_debug starboard_blitter_example

## Running an application built for Starboard DirectFB x64 Linux

If you are using a desktop Linux workstation to run the executable, you will
likely not have framebuffer support enabled in your Linux build.  In order to
have DirectFB applications run under X11, you will need to modify your
DirectFB configuration file.

    $ vi ~/.directfbrc
        > system=x11
        > quiet

Adding the "quiet" flag is optional, it will suppress log output from DirectFB.

You can now run applications built under this configuration.  However, if you
run an application, you may encounter a message (if you don’t specify “quiet”
in your .directfbrc file) indicating that XShm is not being used.  If you see
this, you will likely encounter a crash when DirectFB is shutdown (such as
during application shutdown).  To avoid this, you can run all DirectFB apps
through Xephyr.

First, install Xephyr:

    $ sudo apt-get install xserver-xephyr

There is a script, starboard/linux/x64/directfb/xephyr_run.sh
which will automatically run an executable (and its parameters) within a
Xephyr window.  For example, to run the nplb executable under Xephyr:

    $ starboard/linux/x64/directfb/xephyr_run.sh out/linux-x64directfb_debug/nplb

Note that the script will start an instance of Xephyr, run the application
within it, and then shutdown Xephyr when the application terminates.

If you would like to manually start Xephyr and run applications within it (e.g.
to make debugging easier), you can do so by first starting Xephyr in the
background:

    $ Xephyr -screen 1920x1080 :1 2> /dev/null &

And then launch the application and have it target the Xephyr display:

    $ DISPLAY=:1 out/linux-x64directfb_debug/starboard_blitter_example

or

    $ DISPLAY=:1 out/linux-x64directfb_debug/nplb
