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
    * [ninja](https://ninja-build.org/) (see the `Getting Ninja` instructions)
    * [nodejs](https://nodejs.org/en)
    * [python3](https://www.python.org/downloads/)
    * The following [VS2022](https://visualstudio.microsoft.com/vs/) components:
      * Microsoft.VisualStudio.Component.VC.14.34.17.4.x86.x64
      * Microsoft.VisualStudio.Component.VC.Llvm.Clang
      * Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset
      * Microsoft.VisualStudio.Component.Windows10SDK.18362
      * Microsoft.VisualStudio.Workload.NativeDesktop
    * [winflexbison](https://github.com/lexxmark/winflexbison)

    <aside class="note">
      <b>Note:</b> By default, Cobalt's build system will check
      C:\Program Files (x86)\ for the Visual Studio install directory. If you
      installed it elsewhere, you can set the `VSINSTALLDIR` environment
      variable to point to the correct location. For example
      `C:/Program Files/Microsoft Visual Studio/2022/Professional`
    </aside>

1.  Install GN, which we use for our build system code. There are a few ways to
    get the binary, follow the instructions for whichever way you prefer
    [here](https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/#getting-a-binary).

1.  (Optional)
    [Install Sccache](https://github.com/mozilla/sccache#installation) to
    support build acceleration.

1.  Make sure all of the above installed packages are on your Path environment
    variable.

    ```
    "C:\Program Files\Git"
    "C:\Program Files\Ninja"
    "C:\Program Files\nodejs"
    "C:\python_38" # Python 3.8 is the oldest supported python version. You may have a newer version installed.
    "C:\python_38\Scripts"
    "C:\winflexbison" # Or wherever you chose to unpack the zip file
    "C:\gn"
    "C:\sccache"
    ```

1.  Clone the Cobalt code repository. The following `git` command creates a
    `cobalt` directory that contains the repository:

    ```
    $ git clone https://github.com/youtube/cobalt.git
    ```

    <aside class="note">
      If you plan to contribute to the Cobalt codebase it is recommended that
      you create your own
      [fork](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/about-forks)
      of the [Cobalt repository](https://github.com/youtube/cobalt), apply
      changes to the fork, and then
      [create a pull request](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork)
      to merge those changes into the Cobalt repository.
    </aside>

1.  Set the `PYTHONPATH` environment variable to include the full path to the
    top-level `cobalt` directory from the previous step.

### Set up Developer Tools

1.  Enter your new `cobalt` directory:

    ```
    $ cd cobalt
    ```

1.  Create a virtual evnrionment by running the following in cmd:

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
    $ git checkout -b <my-branch-name> origin/main
    ```

## Build and Run Cobalt

1.  Build the code running the following command in the top-level `cobalt`
    directory. You must specify a platform when running this command. On Windows
    the canonical platform is `win-win32`.

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
    of flags that this command supports are in
    [cobalt/browser/switches.cc](https://github.com/youtube/cobalt/blob/main/cobalt/browser/switches.cc).

    <table class="details responsive">
      <tr>
        <th colspan="2">Flags</th>
      </tr>
      <tr>
        <td><code>allow_http</code></td>
        <td>Indicates that you want to use <code>http</code> instead of
            <code>https</code>.</td>
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

## Cobalt on Xbox One

In order to build Cobalt for Xbox One, you will need access to Microsoft's XDK.
In order to sideload and run custom apps on Xbox you will need either an Xbox
devkit or the ability to put an Xbox into developer mode. Those steps are
outside the scope of this document.

### AppxManifest Settings

Cobalt makes use of several template files and a settings file to generate an
AppxManifest.xml during the ninja step. The settings can be found in
`starboard/xb1/appx_product_settings.py`. Most of the default values are stubs
and intended to be overwritten by developers creating their own app with Cobalt,
but they should work for local testing.

<aside class="note">
  <b>Note:</b> if you change the value of `PUBLISHER` in
  `appx_product_settings.py` you <b>must</b> regenerate a pfx file in order for
  the packaging step below to work correctly. Follow the instructions in
  `starboard/xb1/cert/README.md` to generate your own pfx.
</aside>

### Build Cobalt

To build Cobalt for the Xbox One, set the platform to `xb1` in the gn step:

```
$ python cobalt/build/gn.py [-c <build_type>] -p xb1
```

Then specify the `cobalt_install` build target in the ninja step:

```
ninja -C out/xb1_devel cobalt_install
```

### Package an Appx

There's a convenience script at `starboard/xb1/tools/packager.py` for packaging
the compiled code into an appx and then signing the appx with the pfx file
located at `starboard/xb1/cert/cobalt.pfx`. The source, output, and product
flags must be specified, and the only valid product for an external build is
`cobalt`. Here's an example usage:

```
python starboard/xb1/tools/packager.py -s out/xb1_devel/ -o out/xb1_devel/package -p cobalt
```

Alternatively, you can use the
[MakeAppx](https://learn.microsoft.com/en-us/windows/win32/appxpkg/make-appx-package--makeappx-exe-)
and
[SignTool](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)
PowerShell commands to manually perform those steps.

Once the appx has been created and signed, it can be deployed to an Xbox using
the
[WinAppDeployCmd](https://learn.microsoft.com/en-us/windows/uwp/packaging/install-universal-windows-apps-with-the-winappdeploycmd-tool)
PowerShell command.
