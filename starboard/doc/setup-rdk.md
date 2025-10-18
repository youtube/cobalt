# Building and Deploying Cobalt for RDK

## Prerequisites

- Docker installed on your build machine
- SSH access to your RDK target device
- ~20GB free disk space for Cobalt sources and build artifacts

### SSH Setup

Configure passwordless SSH to your target device:

```bash
ssh-copy-id root@<TARGET_IP>
ssh root@<TARGET_IP> echo "SSH working"
```

Replace `<TARGET_IP>` with your device's IP address.

## Docker Registry Authentication

Before building the Docker image, authenticate with Google Cloud to access the marketplace container registry:

```bash
gcloud auth login
gcloud auth configure-docker marketplace.gcr.io
```

This is required because the `rdk/Dockerfile` pulls base images from Google's marketplace registry.

## 1. Build Docker Image

The Dockerfile is located at `chromium/src/cobalt/docker/rdk/Dockerfile`.

```bash
cd chromium/src/cobalt/docker/rdk
docker build -t rdk .
```

## 2. Fetch Cobalt Sources

**Note:** For detailed source setup instructions, see `cobalt/starboard/doc/eap/getting_started.md`.

Follow the standard Cobalt setup process:

1. **Install required packages and depot_tools** (see starboard documentation)
2. **Clone Cobalt repository and configure gclient** (see starboard documentation)  
3. **Install build dependencies:** `./build/install-build-deps.sh` (see starboard documentation)

**Optional:** For faster builds, configure ccache as described in the starboard documentation.

**Important Platform Difference:** The starboard documentation on `cobalt/starboard/doc/eap/getting_started.md` demonstrates building for `evergreen-x64` (Linux desktop). Skip the starboard build examples and proceed to the RDK-specific build section below.

**For RDK boards, you will use the ARM platform instead:** `evergreen-arm-hardfp-rdk`. 

**RDK-specific directory setup:**
```bash
COBALT_BUILD_DIR="$HOME/cobalt-build"
mkdir -p "$COBALT_BUILD_DIR"
cd "$COBALT_BUILD_DIR"
# Continue with standard setup from starboard docs, then return here for RDK-specific build
```

## 3. Build Cobalt

The Docker container bind-mounts `$HOME/cobalt-build` to access sources and write build artifacts:

```bash
COBALT_BUILD_DIR="$HOME/cobalt-build"

docker run --rm -v $COBALT_BUILD_DIR:$COBALT_BUILD_DIR -t rdk bash -c "
    git config --global --add safe.directory '*'
    export PATH=$COBALT_BUILD_DIR/depot_tools:\$PATH
    cd $COBALT_BUILD_DIR/chromium/src
    cobalt/build/gn.py -p evergreen-arm-hardfp-rdk -c devel --no-rbe
    ninja -j 4 -C out/evergreen-arm-hardfp-rdk_devel/ cobalt_loader nplb_loader
"
```

**Build Output Location:** `$HOME/cobalt-build/chromium/src/out/evergreen-arm-hardfp-rdk_devel/`

The bind mount ensures all build artifacts are directly accessible on your host machine after the build completes.

## 4. Deploy to Target

```bash
TARGET="root@<TARGET_IP>"
DEST="/data/cobalt/evergreen-arm-hardfp-rdk_devel"
OUT="$HOME/cobalt-build/chromium/src/out/evergreen-arm-hardfp-rdk_devel"

# Create destination directory
ssh "$TARGET" "mkdir -p $DEST"

# Configure environment variables
ssh "$TARGET" "
grep -q 'Cobalt environment' /etc/default/westeros-env || cat >> /etc/default/westeros-env << 'EOF'

# Cobalt environment variables
WAYLAND_DISPLAY=ResidentApp/wst-ResidentApp
EOF
"

# Sync build artifacts using GN-generated runtime dependencies
# This ensures all required files are included automatically
cd "$OUT"
tar czf - -T cobalt_loader.runtime_deps | ssh "$TARGET" "cd $DEST && tar xzf -"

# Alternative: For NPLB (Non-Platform Loader Binary) testing
# tar czf - -T nplb_loader.runtime_deps | ssh "$TARGET" "cd $DEST && tar xzf -"
```

### Alternative: Using ADB for File Transfer

If you have a USB type-C cable connected, you can use `adb` to update the library file:

```sh
adb push libloader_app.so /usr/lib/libloader_app.so
```

## 5. Run on Target

```bash
ssh "$TARGET" "
    cd $DEST
    killall -9 elf_loader_sandbox loader_app 2>/dev/null || true
    # Note: westeros and wayland are assumed to be always running
    # No need to stop/start wpeframework or other display services
    sleep 1

    # Set environment variables directly and import systemd environment
    export LD_PRELOAD=/usr/lib/libwesteros_gl.so.0.0.0
    export WESTEROS_SINK_USE_ESSRMGR=1
    export WAYLAND_DISPLAY=ResidentApp/wst-ResidentApp
    eval \$(systemctl show-environment | sed 's/^/export /')

    # To run Cobalt
    ./elf_loader_sandbox --evergreen_content=. --evergreen_library=libcobalt.so

    # To run NPLB:
    # ./elf_loader_sandbox --evergreen_content=. --evergreen_library=libnplb.so
"
```

## Display Configuration

**Wayland Display Setup:** Cobalt must connect to the same Wayland compositor used by the RDK UI. The correct wayland display is typically `ResidentApp/wst-ResidentApp`, not `wayland-0`.

**Verification:** To check which wayland display is active:
```bash
ssh "$TARGET" "ps aux | awk '{print \$2}' | while read pid; do if [ -f /proc/\$pid/environ ]; then env_vars=\$(cat /proc/\$pid/environ 2>/dev/null | tr '\0' '\n' | grep WAYLAND_DISPLAY); if [ -n \"\$env_vars\" ]; then echo \"PID \$pid: \$env_vars\"; fi; fi; done"
```

**Socket Location:** The wayland socket is typically found at `/run/ResidentApp/wst-ResidentApp`.

## Runtime Dependencies

**Available Targets:**
- `cobalt_loader.runtime_deps` - Main Cobalt application
- `nplb_loader.runtime_deps` - Non-Platform Loader Binary for testing

## Notes

- **Deployment Directory:** Uses `/data`  for storage due to RDK storage restrictions. The `/data/` partition typically has more space and is designed for application deployments.
- **Environment Variables:** Cobalt requires specific environment variables for display functionality. The script inside the `ssh` command above should set these variables on the target accordingly.
- **Westeros/Wayland Services:** These services are assumed to be always running. The script no longer attempts to stop/start them.
- **Build Configuration:** This guide uses the `devel` configuration for faster iteration. For production builds, use `-c qa` or `-c gold`.
- **Display Issues:** If Cobalt runs but doesn't display, verify the wayland display setting matches the active compositor.

