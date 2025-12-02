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

1. Configure passwordless SSH to your target RDK device:

```bash
ssh-copy-id root@<TARGET_IP>
ssh root@<TARGET_IP> echo "SSH working"
```

**Note:** Replace `<TARGET_IP>` with your device's IP address.

## 1. Fetch Cobalt Sources

Follow the standard Cobalt setup process from [the starboard documentation](/starboard/doc/eap/getting_started.md):

1. **Install required packages and depot_tools** (see [Setup workstation](/starboard/doc/eap/getting_started.md#setup-workstation))
2. **Clone Cobalt repository and configure gclient** (see [Get source code](/starboard/doc/eap/getting_started.md#get-source-code))
3. **Download subrepos** using `gclient sync`

**Important:**
- You do **NOT** need to install build dependencies or build Evergreen for x64 as shown in the starboard documentation.
- The starboard documentation demonstrates building for `evergreen-x64` (Linux desktop). For RDK boards, you will use the ARM platform: `evergreen-arm-hardfp-rdk`. Skip the `evergreen-x64` build and proceed to the RDK-specific build section below.

**RDK-specific directory setup:**

```bash
COBALT_BUILD_DIR="$HOME/cobalt-build"
mkdir -p "$COBALT_BUILD_DIR"
cd "$COBALT_BUILD_DIR"
# Continue with standard setup from starboard docs, then proceed to step 2
```

## 2. Build Docker Image

### Docker Registry Authentication

Before building the Docker image, authenticate with Google Cloud to access the marketplace container registry:

```bash
gcloud auth login
gcloud auth configure-docker marketplace.gcr.io
```

This is required because the `$COBALT_BUILD_DIR/chromium/src/cobalt/docker/rdk/Dockerfile` pulls base images from Google's marketplace registry.

### Building Cobalt

The Cobalt repo includes a `docker-compose.yaml` that describes the build images.

This section is split into two parts: what to do on the host (Outside Container) and what the container will execute (Inside Container).

#### Outside Container

On the host, ensure the key directories and variables are set. These will be bind-mounted into the container so build artifacts are available on the host after the build completes.

```bash
COBALT_BUILD_DIR="$HOME/cobalt-build"
DEPOT_TOOLS_DIR="/path/to/depot_tools"
```

Then, build the `rdk` image and launch an interactive shell inside a temporary container:

```bash
# Build the container image and run an interactive shell in the rdk service container
docker compose -f "$COBALT_BUILD_DIR/chromium/src/docker-compose.yaml" run --rm --build \
  -v "$COBALT_BUILD_DIR:$COBALT_BUILD_DIR" \
  -e COBALT_BUILD_DIR="$COBALT_BUILD_DIR" \
  -v "$DEPOT_TOOLS_DIR:$DEPOT_TOOLS_DIR" \
  -e DEPOT_TOOLS_DIR="$DEPOT_TOOLS_DIR" \
  rdk bash
```

#### Inside Container

After using the command above, the container will start with a interactive shell. Paste the following commands into the container shell to perform the build.

Inside Container (paste these):

```bash
# Configure git for safe directory access
git config --global --add safe.directory '*'

# Add depot_tools to PATH for build tools (gn, ninja)
export PATH=$DEPOT_TOOLS_DIR:$PATH

# Navigate to Cobalt source directory
cd $COBALT_BUILD_DIR/chromium/src

# Generate build files for RDK platform
cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c devel --no-rbe

# Compile Cobalt binaries
ninja -C out/evergreen-arm-hardfp-rdk_devel/ cobalt_loader nplb_loader
```

Build output will be placed in `$COBALT_BUILD_DIR/chromium/src/out/evergreen-arm-hardfp-rdk_devel/` on the host because of the bind mount.

After building successfully, you can use `ctrl+d` to finish the docker execution.

## 3. Deploying Cobalt on RDK

### Configuration Variables

Set these variables on your host machine before proceeding with deployment steps:

```bash
TARGET="root@<TARGET_IP>"
COBALT_CONTENT_DIR="/data/out_cobalt"
COBALT_BUILD_DIR="$HOME/cobalt-build"
OUT="$COBALT_BUILD_DIR/chromium/src/out/evergreen-arm-hardfp-rdk_devel"
```

**Note:** Replace `<TARGET_IP>` with your RDK device's IP address.

### Setup Target Device (One-time)

Create the Cobalt environment setup script on the target device. This script sets up environment variables, kills WPE Framework (RDK UI menu), and ensures westeros is running.

**Note:** This setup only needs to be done once on a fresh install of RDK.

```bash
ssh "$TARGET" "
    mkdir -p $COBALT_CONTENT_DIR /data/cobalt/evergreen-arm-hardfp-rdk_devel /home/root
    cat > /home/root/setup-cobalt-env.sh << 'SCRIPT_EOF'
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

    # Always kill and restart westeros compositor
    killall -9 westeros westeros-init 2>/dev/null || true
    sleep 1
    /usr/bin/westeros-init >/dev/null 2>&1 &
    sleep 3

    echo 'Cobalt environment ready. WPE Framework stopped and westeros running.'
    SCRIPT_EOF
    chmod +x /home/root/setup-cobalt-env.sh"
```

**What the setup script does:**

The `/home/root/setup-cobalt-env.sh` script is an auxiliary script used for development only. It sets up the Cobalt environment by:
- Stopping WPE Framework (RDK UI) to free up the display
- Starting westeros compositor for Cobalt's display functionality
- Setting up environment variables and system parameters
- Ensuring Cobalt has exclusive access to the display

**Note:** WPE Framework is stopped as a development workaround to ensure Cobalt has exclusive display access. Future RDK patches will integrate Cobalt as a WPE plugin, eliminating the need for this workaround. The westeros compositor is required to provide Cobalt with keyboard input events and other RDK services.

### Transfer Cobalt Files

**Note:** This step can be repeated multiple times during development.

Transfer Cobalt files to target using the variables defined in the Configuration Variables section above:

**Method 1: Full deployment (tar) - Faster for transferring all the build artifacts**

Use this method for initial deployment or when you want to ensure a clean state:

```bash
cd "$OUT"
tar czf - -T cobalt_loader.runtime_deps | ssh "$TARGET" "cd $COBALT_CONTENT_DIR && tar xzf -"

# For NPLB (Non-Platform Loader Binary) testing:
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

### Run on Target

SSH into the target:

```bash
ssh "$TARGET"
```

Then, on the target device, run the commands interactively.

**Important:** You must execute the `setup-cobalt-env.sh` script before running Cobalt each new `ssh` session.

```bash
source /home/root/setup-cobalt-env.sh; /data/out_cobalt/loader_app --content=/data/out_cobalt --url="https://www.youtube.com/tv"
```

## 4. Additional Information

### Display Configuration

The `/home/root/setup-cobalt-env.sh` script configures `WAYLAND_DISPLAY=wayland-0` and starts the westeros compositor for Cobalt display functionality. Execute this script before running Cobalt each time.

### Runtime Dependencies

Available build targets:
- `cobalt_loader.runtime_deps` - Main Cobalt application
- `nplb_loader.runtime_deps` - Non-Platform Loader Binary for testing

### Important Notes

- **Deployment Directory:** Uses `/data` for storage due to RDK storage restrictions. The `/data/` partition typically has more space and is designed for application deployments.
- **Environment Setup:** Execute `/home/root/setup-cobalt-env.sh` before running Cobalt each time. This script handles all Cobalt environment setup including westeros compositor and WPE Framework management.
- **Build Configuration:** This guide uses the `devel` configuration for faster iteration. For production builds, use `-c qa` or `-c gold`.
