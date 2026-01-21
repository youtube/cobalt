# Android Browser Test Tools

This directory contains tools for managing and executing `cobalt_browsertests` on Android.

## Artifact Collection Script

`collect_test_artifacts.py` is a utility script designed to package all necessary artifacts (APK/binary, test runner, resources, and test data) from one or more build directories into a single gzip archive. This is particularly useful for running tests in isolated environments such as Docker containers.

### Prerequisites

You must have a successful build for your target platform(s).

```bash
# Example Android build command
autoninja -C out/android-arm_devel cobalt_browsertests

# Example Linux build command
autoninja -C out/linux-x64x11_devel cobalt_browsertests
```

### Usage

Run the script from the root of the Cobalt source tree, providing one or more build directories:

```bash
./cobalt/testing/browser_tests/tools/collect_test_artifacts.py [BUILD_DIR_1] [BUILD_DIR_2] ...
```

Default `BUILD_DIR` is `out/android-arm_devel` if none are specified.

This will generate `cobalt_browsertests_artifacts.tar.gz` in the root directory.

### Archive Structure

The generated archive has the following structure:

- `run_tests.py`: Top-level Python runner script.
- `src/`: Isolated source root containing the build artifacts for all packaged targets.
- `depot_tools/`: Bundled depot_tools for `vpython3` support.

### Running Tests from the Archive

Once you have extracted the archive in your target environment, use the provided `run_tests.py` script. If the archive contains multiple targets, you must specify which one to run:

```bash
# Extract the artifacts
tar -xzf cobalt_browsertests_artifacts.tar.gz -C your_test_env/
cd your_test_env/

# Run tests for a specific target (e.g., android-arm_devel)
export ANDROID_SERIAL=<your_device_serial>
python3 run_tests.py android-arm_devel [additional_args]
```

If only one target was packaged, `run_tests.py` will use it by default.

### Debugging
...
### 2. Package the Artifacts
Generate the tarball containing all necessary test dependencies **inside the tools directory** so the Docker build can access it:

```bash
./cobalt/testing/browser_tests/tools/collect_test_artifacts.py \
  -o cobalt/testing/browser_tests/tools/cobalt_browsertests_artifacts.tar.gz \
  out/android-arm_devel out/linux-x64x11_devel
```

### 3. Build the Docker Image
Build the image using the `tools` directory as the build context:

```bash
docker build -t cobalt-tests:v1 cobalt/testing/browser_tests/tools/
```

### 4. Execute Tests
Use `docker run` to execute tests. The image uses `run_tests.py` as its entrypoint, so you only need to provide the target name and any additional arguments.

To run the packaged tests within a Docker container, the environment must have the following:

1.  **Python 3**: `python3` should be installed.
2.  **Android Tests**: `adb` and `netcat-openbsd` are required.
3.  **Linux Tests**: GUI dependencies like `xvfb` and `openbox` are required for headless execution.

Note: `depot_tools` (providing `vpython3`) is bundled within the archive, so manual installation is not required.

### Sample Dockerfile

A `Dockerfile` is provided in this directory that includes all necessary dependencies for both Android and Linux test execution. You can use it as a starting point for your test environment.

### Docker Permissions

To run Docker commands without `sudo`, add your user to the `docker` group:

```bash
sudo usermod -aG docker $USER
```

**Note**: You must log out and log back in for this to take effect. To apply the change to your current terminal session immediately, run:

```bash
newgrp docker
```

### Execution with USB Access and Caching

When running the container, ensure it has access to the host's USB devices if using a physical Android device. Use the target name (e.g., `android-arm_devel` or `linux-x64x11_devel`) as the argument to the image.

**Pro-tip**: Mount a volume for the `vpython` cache to avoid downloading dependencies every time the container starts.

#### Android Execution

**Note**: Host environment variables are not shared with the container by default. You must explicitly pass `ANDROID_SERIAL` using the `-e` flag.

```bash
docker run --privileged \
  -v /dev/bus/usb:/dev/bus/usb \
  -v ~/.cache/vpython-root:/root/.cache/vpython-root \
  -e ANDROID_SERIAL=$ANDROID_SERIAL \
  <image_name>:<tag> android-arm_devel -v
```

#### Linux Execution (Headless)
```bash
docker run \
  -v ~/.cache/vpython-root:/root/.cache/vpython-root \
  <image_name>:<tag> linux-x64x11_devel -v
```
