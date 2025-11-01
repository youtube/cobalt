# Building and Deploying Cobalt for RDK

## Prerequisites

### Get an RDK device running the correct image
1. Get an RDK reference device

    You need to have a YouTube reference device (Amlogic S905X4 (AH212)) to run the commands.

    If you don't have one, you can purchase a reference device from
[rdklogic](https://rdklogic.tv/collections/all-products/products/amlogic-s905x4-developer-box-rdk?variant=43178224484592).

2. Get a detailed overview of the RDK reference port

    Please get the device information from YouTube Partner portal: [AH212](https://developers.google.com/youtube/devices/living-room/compliance/ah212).

    Note: If you do not have access to the page above, please see
    [Accessing Living Room Partnership Resources.](https://developers.google.com/youtube/devices/living-room/access/accessing-lr-partnership-resources)

    More information about the RDK reference box can be found on RDK Central wiki: [RDK-Google IPSTB profile stack on Amlogic reference board](https://wiki.rdkcentral.com/display/RDK/RDK-Google+IPSTB+profile+stack+on+Amlogic+reference+board).

3. Ensure that you have flashed the latest system image. If you are a google partner, see the [Cobalt Development on RDK box doc](https://docs.google.com/document/d/1hTlGU15_BKnaa_yT_Z8Omolb7rdZAZj57wtADoKgAfg/edit?resourcekey=0-v-QJNWJumUuksd8JTu76eQ&tab=t.0).

### SSH Setup

1. Configure passwordless SSH to your target device:

```bash
ssh-copy-id root@<TARGET_IP>
ssh root@<TARGET_IP> echo "SSH working"
```

**Note:** Replace `<TARGET_IP>` with your device's IP address.

## 1. Fetch Cobalt Sources

1. **Install required packages and depot_tools** (see [the starboard documentation](cobalt/starboard/doc/eap/getting_started.md))
2. **Clone Cobalt repository and configure gclient** (see [the starboard documentation](cobalt/starboard/doc/eap/getting_started.md))
3. **Follow the standard Cobalt setup** until you configure gclient and download subrepos. **Note:** For the instructions below, you do **NOT** need to install build dependencies or build Evergreen for x64, as it is demonstrated on the doc.

**Important Platform Difference:** [The starboard documentation](cobalt/starboard/doc/eap/getting_started.md) demonstrates the building for `evergreen-x64` (Linux desktop). For RDK boards, you will use the ARM platform instead: `evergreen-arm-hardfp-rdk`. Skip the starboard build for `evergreen-x64` and proceed to the RDK-specific build section below.

**RDK-specific directory setup:**
```bash
COBALT_BUILD_DIR="$HOME/cobalt-build"
mkdir -p "$COBALT_BUILD_DIR"
cd "$COBALT_BUILD_DIR"
# Continue with standard setup from starboard docs, then return here for RDK-specific build
```

## 2. Build Docker Image

### Docker Registry Authentication

Before building the Docker image, authenticate with Google Cloud to access the marketplace container registry:

```bash
gcloud auth login
gcloud auth configure-docker marketplace.gcr.io
```

This is required because the `$COBALT_BUILD_DIR/chromium/src/cobalt/docker/rdk/Dockerfile` pulls base images from Google's marketplace registry.

### Build the RDK Docker Image

Navigate to the Docker directory and build the image:

```bash
cd chromium/src/cobalt/docker/rdk
docker build --progress=plain -t rdk -f "$COBALT_BUILD_DIR/chromium/src/cobalt/docker/rdk/Dockerfile" "$COBALT_BUILD_DIR/chromium/src/cobalt/docker/rdk"
```

This creates a Docker image named `rdk` that contains the RDK toolchain and build environment.

## 3. Build Cobalt

The Docker container bind-mounts both the Cobalt build directory and the `depot_tools` directory to access sources and build tools. The `depot_tools` directory is added to the container's PATH for the build process.

**Note:** Adjust the `COBALT_BUILD_DIR` and `DEPOT_TOOLS_DIR` variables according to your setup. For more information, see [the starboard documentation](cobalt/starboard/doc/eap/getting_started.md).

First, set up the required variables:

```bash
COBALT_BUILD_DIR="$HOME/cobalt-build"
DEPOT_TOOLS_DIR="$HOME/depot_tools"
```

Then, run the build:

```bash
docker run --rm \
    -v $COBALT_BUILD_DIR:$COBALT_BUILD_DIR \
    -v $DEPOT_TOOLS_DIR:$DEPOT_TOOLS_DIR \
    -t rdk bash -c "
    # Configure git for safe directory access
    git config --global --add safe.directory '*'
    
    # Add depot_tools to PATH for build tools (gn, ninja)
    export PATH=$DEPOT_TOOLS_DIR:\$PATH
    
    # Navigate to Cobalt source directory
    cd $COBALT_BUILD_DIR/chromium/src
    
    # Generate build files for RDK platform
    cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c devel --no-rbe
    
    # Compile Cobalt binaries
    ninja -C out/evergreen-arm-hardfp-rdk_devel/ cobalt_loader nplb_loader
"
```

**Build Output Location:** `$COBALT_BUILD_DIR/chromium/src/out/evergreen-arm-hardfp-rdk_devel/`

The bind mount ensures all build artifacts are directly accessible on your host machine after the build completes.

## 4. Setup Target Device (One-time)

On your host machine's terminal, set:

```bash
TARGET="root@<TARGET_IP>"
COBALT_CONTENT_DIR="/data/out_cobalt"
```

Create Cobalt environment profile for automatic setup of environment variables when a login session is started. This script will also kill WPE Framework (RDK UI menu) and ensure westeros is running.

**Note:** This setup only needs to be done once on a fresh install of RDK.

```bash
ssh "$TARGET" "
    mkdir -p $COBALT_CONTENT_DIR /data/cobalt/evergreen-arm-hardfp-rdk_devel /home/root
    cat > /home/root/.profile << 'PROFILE_EOF'
    #!/bin/sh
    # Cobalt environment preparation for busybox
    cd $COBALT_CONTENT_DIR
    killall -9 elf_loader_sandbox loader_app westeros-init 2>/dev/null || true
    sleep 1

    # Stop WPE Framework if running
    if systemctl is-active --quiet wpeframework; then
        systemctl stop wpeframework >/dev/null 2>&1
        sleep 2
    fi

    # Set up westeros environment
    eval \$(systemctl show-environment | sed 's/^/export /') >/dev/null 2>&1
    export WAYLAND_DISPLAY=wayland-0
    export XDG_RUNTIME_DIR=/tmp

    # Configure system parameters
    echo 0 > /sys/module/drm/parameters/vblankoffdelay 2>/dev/null
    echo 1 > /sys/module/aml_drm/parameters/video_axis_zoom 2>/dev/null

    # Start westeros compositor if not already running
    WESTEROS_RUNNING=false
    if ps aux | grep -v grep | grep -q westeros; then
        WESTEROS_RUNNING=true
    elif [ -n \"\$XDG_RUNTIME_DIR\" ] && [ -S \"\$XDG_RUNTIME_DIR/wayland-0\" ] 2>/dev/null; then
        WESTEROS_RUNNING=true
    elif [ -S \"/tmp/wayland-0\" ] 2>/dev/null; then
        WESTEROS_RUNNING=true
    fi

    if [ \"\$WESTEROS_RUNNING\" = \"false\" ]; then
        /usr/bin/westeros-init >/dev/null 2>&1 &
        sleep 3
    fi

    echo 'Cobalt environment ready. Env vars from \"/home/root/.profile\" sourced and WPE Framework stopped'
    PROFILE_EOF"
```

### 4.1 What the .profile file does

The `/home/root/.profile` file automatically sets up the Cobalt environment on every SSH login by:
- Stopping WPE Framework (RDK UI) to free up the display
- Starting westeros compositor for Cobalt's display functionality
- Setting up environment variables and system parameters
- Ensuring Cobalt has exclusive access to the display

**Note:** We kill WPE Framework as a workaround on development phase to ensure Cobalt is in front during the tests. The RDK patches to create a WPE plugin for Cobalt are still in progress.

**Important:** Without westeros compositor, Cobalt will lose access to target's keyboard input events and other RDK services. So we keep westeros always running for now.

## 5. Transfer Cobalt Files

**Note:** This step can be repeated multiple times during development.

Set up variables and transfer Cobalt files to target:

```bash
TARGET="root@<TARGET_IP>"
COBALT_CONTENT_DIR="/data/out_cobalt"
OUT="$COBALT_BUILD_DIR/chromium/src/out/evergreen-arm-hardfp-rdk_devel"
```

**Method 1: Full deployment (tar) - Faster for transferring all the build artifacts**

Use this method for initial deployment or when you want to ensure a clean state:

```bash
cd "$OUT"
tar czf - -T cobalt_loader.runtime_deps | ssh "$TARGET" "cd $COBALT_CONTENT_DIR && tar xzf -"

# For NPLB testing:
# tar czf - -T nplb_loader.runtime_deps | ssh "$TARGET" "cd $COBALT_CONTENT_DIR && tar xzf -"
```

**Method 2: Incremental deployment (rsync) - Faster for updates**

Use this method for faster incremental updates as it only transfers changed files:

```bash
cd "$OUT"
rsync -avz --ignore-missing-args --files-from=cobalt_loader.runtime_deps ./ "$TARGET:$COBALT_CONTENT_DIR/"

# For NPLB loader - only sync changed files:
# rsync -avz --ignore-missing-args --files-from=nplb_loader.runtime_deps ./ "$TARGET:$COBALT_CONTENT_DIR/"
```

**Alternative: Using ADB for file transfer**

If you have a USB type-C cable connected, you can use `adb` to deploy the runtime dependencies:

```bash
cd "$OUT"
tar czf /tmp/cobalt_runtime_deps.tar.gz -T cobalt_loader.runtime_deps
adb push /tmp/cobalt_runtime_deps.tar.gz $COBALT_CONTENT_DIR/
adb shell "cd $COBALT_CONTENT_DIR && tar xzf cobalt_runtime_deps.tar.gz && rm cobalt_runtime_deps.tar.gz"
rm /tmp/cobalt_runtime_deps.tar.gz
```

## 6. Run on Target

**Note:** The environment is automatically set up via `/home/root/.profile` on every interactive SSH session. See previous section on creation of this file.

```bash
ssh "$TARGET" '/data/out_cobalt/loader_app --content=/data/out_cobalt  --url="https://www.youtube.com/tv"'
```

## Display Configuration

**Wayland Display Setup:** The `/home/root/.profile` automatically configures `WAYLAND_DISPLAY=wayland-0` and starts the westeros compositor for Cobalt display functionality.

## Runtime Dependencies

**Available Targets:**
- `cobalt_loader.runtime_deps` - Main Cobalt application
- `nplb_loader.runtime_deps` - Non-Platform Loader Binary for testing

## Notes

- **Deployment Directory:** Uses `/data` for storage due to RDK storage restrictions. The `/data/` partition typically has more space and is designed for application deployments.
- **Environment Setup:** The `/home/root/.profile` file automatically handles all Cobalt environment setup including westeros compositor and WPE Framework management.
- **Build Configuration:** This guide uses the `devel` configuration for faster iteration. For production builds, use `-c qa` or `-c gold`.