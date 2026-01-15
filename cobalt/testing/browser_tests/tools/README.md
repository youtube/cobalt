# Android Browser Test Tools

This directory contains tools for managing and executing `cobalt_browsertests` on Android.

## Artifact Collection Script

`collect_android_test_artifacts.sh` is a utility script designed to package all necessary artifacts (APK, test runner, resources, and test data) into a single gzip archive. This is particularly useful for running tests in isolated environments such as Docker containers.

### Prerequisites

You must have a successful Android build in `out/android-arm_devel/` (or your target directory).

```bash
# Example build command
autoninja -C out/android-arm_devel cobalt_browsertests
```

### Usage

Run the script from the root of the Cobalt source tree:

```bash
./cobalt/testing/browser_tests/tools/collect_android_test_artifacts.sh
```

This will generate `cobalt_browsertests_android.tar.gz` in the root directory.

### Archive Structure

The generated archive has the following structure:

- `run_tests.sh`: Top-level runner script.
- `src/`: Isolated source root containing:
    - `out/android-arm_devel/`: Build artifacts and generated files.
    - `testing/`, `third_party/`, etc.: Essential scripts and dependencies.

### Running Tests from the Archive

Once you have extracted the archive in your target environment, use the provided `run_tests.sh` script:

```bash
# Extract the artifacts
tar -xzf cobalt_browsertests_android.tar.gz -C your_test_env/
cd your_test_env/

# Run tests (requires a connected device or ANDROID_SERIAL)
export ANDROID_SERIAL=<your_device_serial>
./run_tests.sh [additional_args]
```

The `run_tests.sh` script automatically manages path resolution and sets the necessary environment variables (`CHROME_SRC`) to ensure the test runner functions correctly within the isolated directory structure.

## Docker Container Requirements

To run the packaged tests within a Docker container, the environment must have the following:

1.  **Python 3**: `python3` should be installed.
2.  **ADB**: Required for communicating with Android devices.

Note: `depot_tools` (providing `vpython3`) is bundled within the archive, so manual installation is not required.

### Sample Dockerfile Snippet

```dockerfile
# Install basic dependencies
RUN apt-get update && apt-get install -y python3 adb
```

### Execution with USB Access and Caching

When running the container, ensure it has access to the host's USB devices if using a physical Android device.

**Pro-tip**: Mount a volume for the `vpython` cache to avoid downloading dependencies every time the container starts:

```bash
docker run --privileged \
  -v /dev/bus/usb:/dev/bus/usb \
  -v ~/.cache/vpython-root:/root/.cache/vpython-root \
  your-image-name ./run_tests.sh --list-tests
```
