# Android Browser Test Tools

This directory contains tools for managing and executing `cobalt_browsertests` on Android.

## Artifact Collection Script

`collect_test_artifacts.py` is a utility script designed to package all necessary artifacts (APK/binary, test runner, resources, and test data) into a single gzip archive. This is particularly useful for running tests in isolated environments such as Docker containers.

### Prerequisites

You must have a successful build for your target platform.

```bash
# Example Android build command
autoninja -C out/android-arm_devel cobalt_browsertests
```

### Usage

Run the script from the root of the Cobalt source tree, optionally providing the build directory:

```bash
./cobalt/testing/browser_tests/tools/collect_test_artifacts.py [BUILD_DIR]
```

Default `BUILD_DIR` is `out/android-arm_devel`.

This will generate `cobalt_browsertests_artifacts.tar.gz` in the root directory.

### Archive Structure

The generated archive has the following structure:

- `run_tests.py`: Top-level Python runner script.
- `src/`: Isolated source root containing build artifacts and test data.
- `depot_tools/`: Bundled depot_tools for `vpython3` support.

### Running Tests from the Archive

Once you have extracted the archive in your target environment, use the provided `run_tests.py` script:

```bash
# Extract the artifacts
tar -xzf cobalt_browsertests_artifacts.tar.gz -C your_test_env/
cd your_test_env/

# Run tests (Android requires a connected device or ANDROID_SERIAL)
export ANDROID_SERIAL=<your_device_serial>
python3 run_tests.py [additional_args]
```

### Debugging

If the script appears to hang or is running slowly, you can enable debug mode to see timestamped progress:

```bash
DEBUG=1 python3 run_tests.py --list-tests
```

The `run_tests.py` script automatically manages path resolution and sets the necessary environment variables (`CHROME_SRC`) to ensure the test runner functions correctly within the isolated directory structure.

## End-to-End Docker Workflow

Follow these steps to build, package, and run tests within a Docker container.

### 1. Build the Test Targets
Ensure you have a successful build (e.g., Android):
```bash
autoninja -C out/android-arm_devel cobalt_browsertests
```

### 2. Package the Artifacts
Generate the tarball containing all necessary test dependencies:
```bash
./cobalt/testing/browser_tests/tools/collect_test_artifacts.py out/android-arm_devel
```

### 3. Build the Docker Image
Ensure your `Dockerfile` includes `python3` and `adb`. From the directory containing both the `Dockerfile` and the generated `cobalt_browsertests_artifacts.tar.gz`:
```bash
sudo docker build -t <image_name>:<tag> .
```

### 4. Run the Docker Container
Start the container with host network access and Android configuration mounted:
```bash
sudo docker run -it --rm \
  --network host \
  -v ~/.android:/root/.android \
  <image_name>:<tag>
```

### 5. Execute Tests Inside Container
Inside the running container's shell:
```bash
cd /opt/cobalt_browsertests/
export ANDROID_SERIAL=<your_device_serial>
python3 run_tests.py -v --list-tests
```

## Docker Container Requirements

To run the packaged tests within a Docker container, the environment must have the following:

1.  **Python 3**: `python3` should be installed.
2.  **Android Tests**: `adb` and `netcat-openbsd` are required.
3.  **Linux Tests**: GUI dependencies like `xvfb` and `openbox` are required for headless execution.

Note: `depot_tools` (providing `vpython3`) is bundled within the archive, so manual installation is not required.

### Sample Dockerfile

```dockerfile
FROM ubuntu:latest

# Install all dependencies for Android and Linux test execution
RUN apt-get update && apt-get install -y \
    adb \
    netcat-openbsd \
    python3 \
    ca-certificates \
    xvfb \
    openbox \
    libnss3 \
    libasound2 \
    libxcomposite1 \
    libxdamage1 \
    libxrandr2 \
    libgbm1 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*
```

### Execution with USB Access and Caching

When running the container, ensure it has access to the host's USB devices if using a physical Android device.

**Pro-tip**: Mount a volume for the `vpython` cache to avoid downloading dependencies every time the container starts:

```bash
docker run --privileged \
  -v /dev/bus/usb:/dev/bus/usb \
  -v ~/.cache/vpython-root:/root/.cache/vpython-root \
  <image_name>:<tag> python3 run_tests.py --list-tests
```
