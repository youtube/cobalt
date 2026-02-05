# Test Artifact Archival Tools

This directory contains scripts for packaging Cobalt test artifacts into portable archives (tarballs) that include all necessary runtime dependencies.

## Scripts

### `archive_test_artifacts.py`

This is the primary entry point for archiving test targets. It parses GN-generated `.runtime_deps` files to identify and collect all files required to run a specific test suite on another machine or environment.

#### Usage: Packaging Browsertests for Docker

To package `cobalt_browsertests` so it can be built into a Docker image, run the following from the **project root**:

```bash
python3 cobalt/build/archive_test_artifacts.py \
  -s . \
  -o out/android-arm_devel \
  -d cobalt/testing/browser_tests/tools/ \
  -t cobalt/testing/browser_tests:cobalt_browsertests \
  --compression gz
```

This will place `cobalt_browsertests_artifacts.tar.gz` in the same directory as the `Dockerfile`.

#### Building and Running the Docker Image

Once the archive is created, you can build and run the portable test environment:

```bash
# 1. Build the image (from the root)
docker build -t cobalt-browsertests cobalt/testing/browser_tests/tools/

# 2. Run the tests
docker run --rm \
  -v /tmp/test_results:/app/results \
  cobalt-browsertests \
  --target android-arm_devel \
  --device_id <your_device_serial>
```

#### Standard Test Archival

To package a standard test suite (e.g., `base_unittests`):

```bash
python3 cobalt/build/archive_test_artifacts.py \
  -s . \
  -o out/linux-x64x11_devel \
  -d out/linux-x64x11_devel \
  -t base/base_unittests:base_unittests
```

### Arguments

- `-s`, `--source-dir`: Path to the Cobalt source root.
- `-o`, `--out-dir`: Path to the build output directory (containing binaries and `.runtime_deps`).
- `-d`, `--destination-dir`: Directory where the resulting `.tar` archive will be saved.
- `-t`, `--targets`: Comma-separated list of fully qualified GN targets (e.g., `path/to:target`).
- `--compression`: Compression algorithm to use (`gz`, `xz`, or `zstd`). Defaults to `zstd`.

## Testing

Regression tests for the archival logic:

```bash
python3 cobalt/build/test_archive_test_artifacts.py
```
