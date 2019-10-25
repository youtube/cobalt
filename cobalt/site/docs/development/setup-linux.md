---
layout: doc
title: "Set up your environment - Linux"
---

These instructions explain how Linux users set up their Cobalt development
environment, fetch a copy of the Cobalt code repository, and build a Cobalt
binary. Note that the binary has a graphical client and must be run locally
on the machine that you are using to view the client. For example, you cannot
SSH into another machine and run the binary on that machine.

1.  Choose where you want to put the `depot_tools` directory, which is used
    by the Cobalt code. An easy option is to put them in `~/depot_tools`.
    Clone the tools by running the following command:

    ```
    $ cd ~/
    $ git clone https://cobalt.googlesource.com/depot_tools
    ```

1.  Add your `depot_tools` directory to the end of your `PATH` variable.
    We recommend adding something like this to your `.bashrc` or `.profile`
    file:

    ```
    $ PATH=${PATH}:/path/to/depot_tools
    ```

1.  Run the following command to install packages needed to build and run
    Cobalt on Linux:

    ```
    $ sudo apt-get install bison build-essential coreutils git gperf \
           libaom-dev libasound2-dev libavformat-dev libavresample-dev \
           libdirectfb-dev libdirectfb-extra libpulse-dev \
           libgl1-mesa-dev libgles2-mesa-dev libvpx-dev libx11-dev \
           libxcomposite-dev libxcomposite1 libxrender-dev libxrender1 \
           libxpm-dev m4 python ruby tar xserver-xephyr xz-utils yasm
    ```

1.  Install the latest version of the standard C++ header files (`libstdc++`).
    For example:

    ```
    sudo apt-get install libstdc++-4.8-dev
    ```

1.  Clone the Cobalt code repository. The following `git` command creates a
    `cobalt` directory that contains the repository:

    ```
    $ git clone https://cobalt.googlesource.com/cobalt
    ```

1.  Modify your path to include the version of Clang that is downloaded
    in the next step of the instructions. The next step will return an
    error if this version of Clang is not in your path before it runs.

    ```
    $PATH="/path/to/cobalt/src/third_party/llvm-build/Release+Asserts/bin:${PATH}"
    ```

1.  Build the code by navigating to the `src` directory in your new
    `cobalt` directory and running the following command. You must
    specify a platform when running this command. On Ubuntu Linux, the
    canonical platform is `linux-x64x11`.

    You can also use the `-C` command-line flag to specify a `build_type`.
    Valid build types are `debug`, `devel`, `qa`, and `gold`. If you
    specify a build type, the command finishes sooner. Otherwise, all types
    are built.

    ```
    $ cobalt/build/gyp_cobalt [-C <build_type>] <platform>
    ```

    <aside class="note"><b>Important:</b> You need to rerun gyp_cobalt every
    time a change is made to a `.gyp` file.</aside>

1.  Compile the code from the `src/` directory:

    ```
    $ ninja -C out/<platform>_<build_type> <target_name>
    ```

    The previous command contains three variables:

    1.  `<platform>` is the [platform
        configuration](/starboard/porting.html#1-enumerate-and-name-your-platform-configurations)
        that identifies the platform. As described in the Starboard porting
        guide, it contains a `family name` (like `linux`) and a
        `binary variant` (like `x64x11`), separated by a hyphen.
    1.  `<build_type>` is the build you are compiling. Possible values are
        `debug`, `devel`, `qa`, and `gold`. These values are also described in
        the Starboard porting guide under the [required file modifications](
        /starboard/porting.html#4-make-required-file-modifications) for the
        `gyp_configuration.gypi` file.
    1.  `<target_name>` is the name assigned to the compiled code and it is
        used to run the code compiled in this step. The most common names are
        `cobalt`, `nplb`, and `all`:
        *   `cobalt` builds the Cobalt app.
        *   `nplb` builds Starboard's platform verification test suite to
            ensure that your platform's code passes all tests for running
            Cobalt.
        *   `all` builds all targets.

    For example:

    ```
    ninja -C out/linux-x64x11_debug cobalt
    ```

    This command compiles the Cobalt `debug` configuration for the
    `linux-x64x11` platform and creates a target named `cobalt` that
    you can then use to run the compiled code.

1.  Run the compiled code to launch the Cobalt client:

    ```
    # Note that 'cobalt' was the <target_name> from the previous step.
    $ out/linux-x64x11_debug/cobalt [--url=<url>]
    ```

    The flags in the following table are frequently used, and the full set
    of flags that this command supports are in <code><a
    href="https://cobalt.googlesource.com/cobalt/+/master/src/cobalt/browser/switches.cc">cobalt/browser/switches.cc</a></code>.

    <table class="details responsive">
      <tr>
        <th colspan="2">Flags</th>
      </tr>
      <tr>
        <td><code>allow_http</code></td>
        <td>Indicates that you want to use `http` instead of `https`.</td>
      </tr>
      <tr>
        <td><code>ignore_certificate_errors</code></td>
        <td>Indicates that you want to connect to an <code>https</code> host
            that doesn't have a certificate that can be validated by our set
            of root CAs.</td>
      </tr>
      <tr>
        <td><code>url</code></td>
        <td>Defines the startup URL that Cobalt will use. If no value is set,
            then Cobalt uses a default URL. This option lets you point at a
            different app than the YouTube app.</td>
      </tr>
    </table>

<!--
<aside class="note">
<b>Note:</b> If you plan to upload reviews to the Cobalt repository, you
also need to <a href="/development/setup-gitcookies.html">follow these
instructions</a> to set up a <code>.gitcookies</code> file.
</aside>
-->
