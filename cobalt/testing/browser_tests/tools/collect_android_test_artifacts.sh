#!/bin/bash
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Script to collect Android test artifacts for cobalt_browsertests into a gzip file.

set -e

BUILD_DIR="out/android-arm_devel"
OUTPUT_FILE="cobalt_browsertests_android.tar.gz"
TEMP_LIST="test_files_list.txt"

if [ ! -d "$BUILD_DIR" ]; then
  echo "Error: Build directory $BUILD_DIR not found."
  exit 1
fi

RUNTIME_DEPS="$BUILD_DIR/gen.runtime/cobalt/testing/browser_tests/cobalt_browsertests__test_runner_script.runtime_deps"

if [ ! -f "$RUNTIME_DEPS" ]; then
  echo "Error: runtime_deps file not found at $RUNTIME_DEPS"
  exit 1
fi

echo "Collecting files from $RUNTIME_DEPS..."

# Start the list with the test runner and some essential build scripts
cat <<EOF > "$TEMP_LIST"
$BUILD_DIR/bin/run_cobalt_browsertests
build/android/
build/skia_gold_common/
build/util/
testing/
third_party/android_build_tools/
third_party/catapult/common/
third_party/catapult/dependency_manager/
third_party/catapult/devil/
third_party/catapult/third_party/
third_party/logdog/
third_party/android_sdk/public/platform-tools/adb
EOF

# Add top-level build python scripts
find build -maxdepth 1 -name "*.py" >> "$TEMP_LIST"

# Process runtime_deps.
# Paths in runtime_deps are relative to the BUILD_DIR.
while IFS= read -r line; do
  # Skip empty lines or comments
  [[ -z "$line" || "$line" == \#* ]] && continue

  # Resolve path relative to BUILD_DIR
  FULL_PATH=$(python3 -c "import os; print(os.path.normpath(os.path.join('$BUILD_DIR', '$line')))")

  if [ -e "$FULL_PATH" ]; then
    echo "$FULL_PATH" >> "$TEMP_LIST"
  else
    echo "Warning: File not found: $FULL_PATH"
  fi
done < "$RUNTIME_DEPS"

# Sort and unique the list
sort -u "$TEMP_LIST" -o "$TEMP_LIST"

# Prepare a temporary directory with the desired structure
STAGE_DIR="temp_stage_src"
mkdir -p "$STAGE_DIR/src"

echo "Copying files to staging directory..."
while IFS= read -r line; do
  [[ -z "$line" || "$line" == \#* ]] && continue
  FULL_PATH=$(python3 -c "import os; print(os.path.normpath(os.path.join('$BUILD_DIR', '$line')))")
  if [ -e "$FULL_PATH" ]; then
    # Create the directory structure in staging
    DEST_PATH="$STAGE_DIR/src/$FULL_PATH"
    mkdir -p "$(dirname "$DEST_PATH")"
    if [ -d "$FULL_PATH" ]; then
        # Use cp -a to preserve structure and symlinks, but avoid infinite recursion
        cp -a "$FULL_PATH" "$STAGE_DIR/src/$(dirname "$FULL_PATH")/" 2>/dev/null || true
    else
        cp -a "$FULL_PATH" "$DEST_PATH" 2>/dev/null || true
    fi
  fi
done < "$RUNTIME_DEPS"

# Add the essential directories manually to ensure they are there
# We use rsync to avoid issues with copying directories into themselves if any path overlaps
for dir in build/android build/util build/skia_gold_common testing third_party/android_build_tools third_party/catapult/common third_party/catapult/dependency_manager third_party/catapult/devil third_party/catapult/third_party third_party/logdog third_party/android_sdk/public/platform-tools; do
  if [ -d "$dir" ]; then
    mkdir -p "$STAGE_DIR/src/$dir"
    cp -a "$dir/." "$STAGE_DIR/src/$dir/" 2>/dev/null || true
  fi
done

# Add the test runner
mkdir -p "$STAGE_DIR/src/$BUILD_DIR/bin"
cp -a "$BUILD_DIR/bin/run_cobalt_browsertests" "$STAGE_DIR/src/$BUILD_DIR/bin/"

# Add top-level build python scripts
mkdir -p "$STAGE_DIR/src/build"
find build -maxdepth 1 -name "*.py" -exec cp -a {} "$STAGE_DIR/src/build/" \;

# Add depot_tools
DEPOT_TOOLS_DIR=$(dirname $(which vpython3))
echo "Adding depot_tools from $DEPOT_TOOLS_DIR..."
cp -a "$DEPOT_TOOLS_DIR" "$STAGE_DIR/depot_tools"

# Explicitly copy the runtime_deps file
DEPS_FILE="out/android-arm_devel/gen.runtime/cobalt/testing/browser_tests/cobalt_browsertests__test_runner_script.runtime_deps"
mkdir -p "$STAGE_DIR/src/$(dirname "$DEPS_FILE")"
cp -a "$DEPS_FILE" "$STAGE_DIR/src/$DEPS_FILE"

# Create the top-level runner script in the root (outside src/)
cat <<EOF > "$STAGE_DIR/run_tests.sh"
#!/bin/bash

# Get the absolute path of the script directory
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"

# Add bundled depot_tools to PATH
export PATH="\$SCRIPT_DIR/depot_tools:\$PATH"
# Disable depot_tools auto-update to save time
export DEPOT_TOOLS_UPDATE=0

# Check for vpython3
if ! command -v vpython3 &> /dev/null; then
  echo "Error: vpython3 not found in bundled depot_tools."
  exit 1
fi

cd "\$SCRIPT_DIR/src"
# Set CHROME_SRC to the absolute path of our isolated src/ directory
export CHROME_SRC=\$(pwd)

# Calculate absolute path for runtime-deps-path
DEPS_PATH="\$CHROME_SRC/out/android-arm_devel/gen.runtime/cobalt/testing/browser_tests/cobalt_browsertests__test_runner_script.runtime_deps"

# Check if deps file exists
if [ ! -f "\$DEPS_PATH" ]; then
  echo "Error: runtime_deps file not found at \$DEPS_PATH"
  exit 1
fi

./out/android-arm_devel/bin/run_cobalt_browsertests \\
  --runtime-deps-path "\$DEPS_PATH" \\
  "\$@"
EOF
chmod +x "$STAGE_DIR/run_tests.sh"

echo "Creating $OUTPUT_FILE from $STAGE_DIR..."
# Create the archive from the contents of STAGE_DIR
tar -C "$STAGE_DIR" -czf "$OUTPUT_FILE" .

# Cleanup
rm -rf "$STAGE_DIR"

echo "Done! Artifacts collected in $OUTPUT_FILE"

echo "Done! Artifacts collected in $OUTPUT_FILE"
