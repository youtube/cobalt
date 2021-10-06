---
layout: doc
title: "Set up your environment - Linux"
---

These instructions explain how Linux users set up their Cobalt development
environment, clone a copy of the Cobalt code repository, and build a Cobalt
binary. Note that the binary has a graphical client and must be run locally
on the machine that you are using to view the client. For example, you cannot
SSH into another machine and run the binary on that machine.

## Set up your workstation

1.  Run the following command to install packages needed to build and run
    Cobalt on Linux:

    ```
    $ sudo apt install -qqy --no-install-recommends pkgconf ninja-build \
        bison yasm binutils clang libgles2-mesa-dev mesa-common-dev \
        libpulse-dev libavresample-dev libasound2-dev libxrender-dev \
        libxcomposite-dev
    ```

1.  Install Node.js via `nvm`:

    ```
    $ export NVM_DIR=~/.nvm
    $ export NODE_VERSION=12.17.0

    $ curl --silent -o- https://raw.githubusercontent.com/creationix/nvm/v0.35.3/install.sh | bash

    $ . $NVM_DIR/nvm.sh \
        && nvm install --lts \
        && nvm alias default lts/* \
        && nvm use default
    ```

1.  Install ccache to support build acceleration. ccache is automatically used
    when available, otherwise defaults to unaccelerated building:

    ```
    $ sudo apt install -qqy --no-install-recommends ccache
    ```

    We recommend adjusting the cache size as needed to increase cache hits:

    ```
    $ ccache --max-size=20G
    ```

1.  Clone the Cobalt code repository. The following `git` command creates a
    `cobalt` directory that contains the repository:

    ```
    $ git clone https://cobalt.googlesource.com/cobalt
    ```

### Set up Developer Tools

Cobalt's developer tools require a different file structure which we are in the
process of moving to. For now, if you want to use these tools, you must unnest
the `src/` directory like so:

```
$ cd cobalt
$ mv src/* ./
$ mv src/.* ./
```

Once you do that, you'll be able to follow the following two steps to have C++
and Python linting and formatting as well as other helpful checks enabled. Keep
in mind that after doing this, you'll want to run following commands in the
top-level directory instead of the `src/` subdirectory.

Git will track this as a large change, we recommend that you create a commit
for it and rebase that commit of our upstream continuously for now.

1.  Create a Python 3 virtual environment for working on Cobalt (feel free to use `virtualenvwrapper` instead):

    ```
    $ python -m venv ~/.virtualenvs/cobalt_dev
    $ source ~/.virtualenvs/cobalt_dev
    $ pip install -r requirements.txt
    ```

1.  Install the pre-commit hooks:

    ```
    $ pre-commit install -t post-checkout -t pre-commit -t pre-push --allow-missing-config
    $ git checkout -b <my-branch-name> origin/COBALT
    ```

## Build and Run Cobalt

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
