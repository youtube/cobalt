---
layout: doc
title: "Set up your environment - Windows"
---

These instructions explain how Windows users can set up their Cobalt development
environment, clone a copy of the Cobalt code repository, and build a Cobalt
binary. Note that the binary has a graphical client and must be run locally on
the machine that you are using to view the client. For example, you cannot SSH
into another machine and run the binary on that machine.

## Set up your workstation

1.  Install the following required packages:
    * [git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git)
    (see the `Installing on Windows` instructions)
    * [python3](https://www.python.org/downloads/)
    * [ninja](https://ninja-build.org/) (see the `Getting Ninja` instructions)
    TODO: verify what needs to be installed for min build
    * VS2017/2022
    * JRE?
    * winflexbison?
    * node?

1.  (Optional)
    [Install Sccache](https://github.com/mozilla/sccache#installation) to
    support build acceleration:

    We recommend adjusting the cache size as needed to increase cache hits:

1.  Install GN, which we use for our build system code. There are a few ways to
    get the binary, follow the instructions for whichever way you prefer
    [here](https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/#getting-a-binary).

1.  Make sure the all of the installed packages are on your Path environment
    variable.

    TODO: update to match everything that's installed above
    ```
    "C:\Program Files\Git"
    "C:\Program Files\Ninja"
    "C:\Program Files\nodejs"
    "C:\python_38"
    "C:\python_38\Scripts"
    "C:\gn"
    "C:\sccache"
    ```

1.  Clone the Cobalt code repository. The following `git` command creates a
    `cobalt` directory that contains the repository:

    ```
    $ git clone https://cobalt.googlesource.com/cobalt
    ```

1.  Set the `PYTHONPATH` environment variable to include the full path to the
    top-level `cobalt` directory from the previous step.

### Set up Developer Tools

1.  Enter your new `cobalt` directory:

    ```
    $ cd cobalt
    ```

    <aside class="note">
    <b>Note:</b> Pre-commit is only available on branches later than 22.lts.1+,
    including trunk. The below commands will fail on 22.lts.1+ and earlier
    branches. For earlier branches, run `cd src` and move on to the next
    section.
    </aside>

1.  Run the following in cmd:

    ```
    py -3 -m venv "%HOME%/.virtualenvs/cobalt_dev"
    "%HOME%/.virtualenvs/cobalt_dev/Scripts/activate.bat"
    pip install -r requirements.txt
    ```

    Or the following in Powershell:

    ```
    py -3 -m venv $env:HOME/.virtualenvs/cobalt_dev
    $env:HOME/.virtualenvs/cobalt_dev/Scripts/activate.ps1
    pip install -r requirements.txt
    ```

    Or the following in Git Bash:

    ```
    py -3 -m venv ~/.virtualenvs/cobalt_dev
    source ~/.virtualenvs/cobalt_dev/Scripts/activate
    pip install -r requirements.txt
    ```
1.  Install the pre-commit hooks:

    ```
    $ pre-commit install -t post-checkout -t pre-commit -t pre-push --allow-missing-config
    $ git checkout -b <my-branch-name> origin/master
    ```

## Build and Run Cobalt

1.  Build the code running the following command in the top-level `cobalt`
    directory. You must specify a platform when running this command. On Windows
    the canonical platform is `win-win32`.

    You can also use the `-c` command-line flag to specify a `build_type`.
    Valid build types are `debug`, `devel`, `qa`, and `gold`. If you
    specify a build type, the command finishes sooner. Otherwise, all types
    are built.

    <aside class="note">
    <b>Note:</b> By default, Windows is treated as an internal build and makes
    use of files that aren't publicly available. To ignore these files and avoid
    build errors, set `is_internal_build` to `false` in
    `starboard/win/win32/args.gn`.
    </aside>

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
    ninja -C out/win-win32_debug cobalt
    ```

    This command compiles the Cobalt `debug` configuration for the
    `win-win32` platform and creates a target named `cobalt` that
    you can then use to run the compiled code.

1.  Run the compiled code to launch the Cobalt client:

    ```
    # Note that 'cobalt' was the <target_name> from the previous step.
    $ out/win-win32_debug/cobalt [--url=<url>]
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
