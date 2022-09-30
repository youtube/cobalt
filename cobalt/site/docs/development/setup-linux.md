---
layout: doc
title: "Set up your environment - Linux"
---

These instructions explain how Linux users set up their Cobalt development
environment, clone a copy of the Cobalt code repository, and build a Cobalt
binary. Note that the binary has a graphical client and must be run locally
on the machine that you are using to view the client. For example, you cannot
SSH into another machine and run the binary on that machine.

These instructions were tested on a fresh ubuntu:20.04 Docker image. (1/12/22)
Required libraries can differ depending on your Linux distribution and version.

## Set up your workstation

1.  Run the following command to install packages needed to build and run
    Cobalt on Linux:

    ```
    $ sudo apt update && sudo apt install -qqy --no-install-recommends \
        pkgconf ninja-build bison yasm binutils clang libgles2-mesa-dev \
        mesa-common-dev libpulse-dev libavresample-dev libasound2-dev \
        libxrender-dev libxcomposite-dev libxml2-dev curl git \
        python3.8-venv
    ```

1.  Install Node.js via `nvm`:

    ```
    $ export NVM_DIR=~/.nvm
    $ export NODE_VERSION=12.17.0

    $ curl --silent -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.3/install.sh | bash

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

1.  Install GN, which we use for our build system code. There are a few ways to
    get the binary, follow the instructions for whichever way you prefer
    [here](https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/#getting-a-binary).

1.  Clone the Cobalt code repository. The following `git` command creates a
    `cobalt` directory that contains the repository:

    ```
    $ git clone https://cobalt.googlesource.com/cobalt
    ```

1.  Set `PYTHONPATH` environment variable to include the full path to the
    top-level `cobalt` directory from the previous step. Add the following to
    the end of your ~/.bash_profile (replacing `fullpathto` with the actual
    path where you cloned the repo):

    ```
    export PYTHONPATH="/fullpathto/cobalt:${PYTHONPATH}"
    ```

    You should also run the above command in your termainal so it's available
    immediately, rather than when you next login.

### Set up Developer Tools

1.  Enter your new `cobalt` directory:

    ```
    $ cd cobalt
    ```

    <aside class="note">
    <b>Note:</b> Pre-commit is only available on branches later than 22.lts.1+,
    including trunk. The below commands will fail on 22.lts.1+ and earlier branches.
    For earlier branches, run `cd src` and move on to the next section.
    </aside>

1.  Create a Python 3 virtual environment for working on Cobalt (feel free to use `virtualenvwrapper` instead):

    ```
    $ python3 -m venv ~/.virtualenvs/cobalt_dev
    $ source ~/.virtualenvs/cobalt_dev/bin/activate
    $ pip install -r requirements.txt
    ```

1.  Install the pre-commit hooks:

    ```
    $ pre-commit install -t post-checkout -t pre-commit -t pre-push --allow-missing-config
    $ git checkout -b <my-branch-name> origin/master
    ```

## Build and Run Cobalt

1.  Build the code running the following command in the top-level `cobalt`
    directory. You must specify a platform when running this command. On Ubuntu
    Linux, the canonical platform is `linux-x64x11`.

    You can also use the `-c` command-line flag to specify a `build_type`.
    Valid build types are `debug`, `devel`, `qa`, and `gold`. If you
    specify a build type, the command finishes sooner. Otherwise, all types
    are built.

    ```
    $ python cobalt/build/gn.py [-c <build_type>] -p <platform>
    ```

1.  Compile the code from the `cobalt/` directory:

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
        `debug`, `devel`, `qa`, and `gold`.
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
    href="https://cobalt.googlesource.com/cobalt/+/master/cobalt/browser/switches.cc">cobalt/browser/switches.cc</a></code>.

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

## Debugging Cobalt

`debug`, `devel`, and `qa` configs of Cobalt expose a feature enabling
developers to trace Cobalt's callstacks per-thread. This is not only a great way
to debug application performance, but also a great way to debug issues and
better understand Cobalt's execution flow in general.

Simply build and run one of these configs and observe the terminal output.
<!--
<aside class="note">
<b>Note:</b> If you plan to upload reviews to the Cobalt repository, you
also need to <a href="/development/setup-gitcookies.html">follow these
instructions</a> to set up a <code>.gitcookies</code> file.
</aside>
-->
