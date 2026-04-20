### Draft: Cobalt Setup and Build

This draft document outlines the steps to set up your environment and build upcoming versions of Cobalt on Linux. While Cobalt leverages some tools from the Chromium project, its repository management and build targets diverge significantly from a standard Chromium build.

**Note:** These steps diverge significantly from older versions of Cobalt, currently available as stable LTS releases (see [Cobalt developer website](https://developers.google.com/youtube/cobalt/docs/development/setup-linux)).

### Relationship with Chromium Build Process

Cobalt's build process utilizes `depot_tools` for utilities like `gn` and `autoninja` (or `ninja`), similar to Chromium (see [Chromium build steps](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md)). However, the method for obtaining the source code, syncing dependencies, and the primary compilation targets are distinct from Chromium's typical workflow.

### System Requirements

To build Cobalt, ensure your system meets the following requirements:

*   An **x86-64 machine** with at least **8GB of RAM** (more than 16GB is highly recommended).
*   At least **100GB of free disk space**.
*   Most development and testing are performed on **Ubuntu** or **Debian** based systems.

### Install `depot_tools`

`depot_tools` provides essential build tools, including `gn` for generating build files and `autoninja` (or `ninja`) for compilation, and `gclient` for fetching code dependencies.

1.  **Clone `depot_tools`**:
    ```sh
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    ```
2.  **Add `depot_tools` to your PATH**: Add the following line to your `~/.bashrc` or `~/.zshrc` file. Use `$HOME` or the absolute path, not `~`, to avoid issues with `gclient runhooks` (though `gclient` is used differently for Cobalt).
    ```sh
    export PATH="${HOME}/depot_tools:$PATH"
    ```

### Get the Cobalt Code

**This step diverges from Chromium's `fetch` command.** Instead of using `fetch`, please clone the Cobalt repository directly.

```sh
git clone git@github.com:youtube/cobalt.git cobalt/src
```

### Configure `gclient`

```sh
cd cobalt
gclient config --name=src git@github.com:youtube/cobalt.git
```

### Download and Sync Sub-repositories

Continue by changing the working directory to `src/`:
```sh
cd src
```
Further instructions always assume `cobalt/src` as the working directory.

```sh
gclient sync --no-history -r $(git rev-parse @)
```

**Note:** This is different from Chromium, the `-r` argument is critical for obtaining the correct version of dependencies.

### Install Build Dependencies

```sh
./build/install-build-deps.sh
```

### Configure and Build Cobalt for Linux

```sh
cobalt/build/gn.py --no-rbe
autoninja -C out/linux-x64x11_devel/ cobalt
```

Note: See note about --no-rbe at the end of this document.

Cobalt can be run as:

```sh
out/linux-x64x11_devel/cobalt
```

### Building for Android TV

Change the platform parameter to:

```sh
cobalt/build/gn.py --no-rbe -p android-arm
autoninja -C out/android-arm_devel/ cobalt_apk
```

This will build `apks/Cobalt.apk` in the output directory.

### Building Chromium

You can also build some of the code in the original Chromium configuration. This is helpful to cross-test and reference Cobalt-related changes.

```sh
cobalt/build/gn.py --no-rbe -p chromium_linux-x64x11
autoninja -C out/chromium_linux-x64x11_devel/ content_shell
```

This will build the Chromium [`content_shell` testing tool](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_tests_in_content_shell.md).
Cobalt differs from content_shell in many ways, but follows similar [Content Embedder](https://chromium.googlesource.com/chromium/src/+/HEAD/content/README.md) principles.

### Continuous Integration

Cobalt uses Github Actions for continuous integration, build, and test. While there are many ongoing changes in the codebase, the build
steps in the Actions code found in the `.github` directory are always current and up to date.

You can also find the current build status on [our build matrix page](https://github.com/youtube/cobalt/blob/main/cobalt/BUILD_STATUS.md) and refer to detailed execution logs for reproducing builds and tests.


### Build acceleration ( RBE )

Build acceleration with RBE ( Remote Build Execution ) is currently not supported. Hence all `gn` commands need the `--no-rbe` flag.

It is possible to configure different build acceleration backends by using `cc_wrapper=ccache`, `cc_wrapper=sccache` or others, but this is currently not tested by Cobalt builds.
