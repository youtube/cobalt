Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up your environment - Docker

We provide Docker image definitions to simplify managing Cobalt build environments on Linux.

## Prerequisites

These instructions assume:
- You have already set up your Cobalt repository workspace on the host machine.
- Docker is installed and running on your host Linux machine. If you need to install Docker, refer to the [official Docker installation guide for Linux](https://docs.docker.com/engine/install/).

## 1. Building the Linux Docker Image

To build the `linux` builder Docker image, run one of the following commands from your workspace root (the directory containing `docker-compose.yaml`):

Using `docker compose`:
```bash
docker compose build linux
```

Or using Docker directly:
```bash
docker build -t linux cobalt/docker/linux
```

## 2. Launching the Linux Container

To run build commands in the container environment as your host user (matching your host user's UID and GID to prevent files created in the mounts from being owned by root), launch it using:

```bash
docker run --rm \
  --user $(id -u):$(id -g) \
  -e HOME=/tmp \
  -v /path/to/parent/directory:/chromium \
  -w /chromium/src \
  -e PYTHONPATH="/chromium/src" \
  -it linux /bin/bash
```

> [!NOTE]
> * Replace `/path/to/parent/directory` with the absolute path of the parent directory on your host machine (e.g. the directory containing `.gclient`, `tools/`, and `src/`). Mounting the parent directory ensures all relative path checks for gclient and depot_tools succeed.
> * Setting `-e HOME=/tmp` redirects the home directory inside the container to a writeable directory, avoiding permission errors when build tools (like `autoninja`) try to write to telemetry or cache configurations under the default home directory (`/` when running as a non-root user).

## 3. Building Cobalt inside the Container

Once inside the interactive container shell (at `/chromium/src`), run the following commands sequentially to configure and build the Cobalt target:

1. **Set the environment PATH variable**:
   Set the path to locate `depot_tools` and `ninja` correctly:
   ```bash
   export PATH="/chromium/tools/depot_tools:/chromium/src/third_party/ninja:/chromium/src:$PATH"
   ```

2. **(Optional) Clean stale build locks**:
   If a previous compilation attempt was interrupted, a stale lock file might block siso. You can clean it by running:
   ```bash
   rm -f out/linux-x64x11_devel/.siso_lock
   ```

3. **Generate the build configuration files** for the `linux-x64x11` platform, using the `devel` config, and disabling Remote Build Execution (RBE):
   ```bash
   python3 cobalt/build/gn.py -p linux-x64x11 -C devel --no-rbe
   ```

4. **Compile the Cobalt target** using `autoninja`:
   ```bash
   autoninja -C out/linux-x64x11_devel cobalt
   ```

Completed builds will be generated in the `out/linux-x64x11_devel` directory in your workspace.
