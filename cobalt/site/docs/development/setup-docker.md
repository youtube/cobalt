Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Set up Docker for Building Cobalt

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

To run build commands in the container environment, first launch the container:

```bash
docker run --rm \
  --user $(id -u):$(id -g) \
  -e HOME=/tmp \
  -v /path/to/workspace:/cobalt \
  -w /cobalt/src \
  -e PYTHONPATH="/cobalt/src" \
  -it linux /bin/bash
```

> Warning: The `/path/to/workspace` must contain both the repository checkout in `src/` and the `depot_tools` checkout.


## 3. Building Cobalt inside the Container

Once inside the interactive container shell (at `/cobalt/src`), run the following commands sequentially to configure and build the Cobalt target:

1. **Set the environment PATH variable**:
   Configure the search path to locate `depot_tools` and `ninja` correctly:

   ```bash
   export PATH="/cobalt/tools/depot_tools:/cobalt/src/third_party/ninja:/cobalt/src:$PATH"
   ```

2. **(Optional) Clean stale build locks**:
   If a previous compilation attempt was interrupted, a stale lock file might block siso. Clean it by running:

   ```bash
   rm -f out/linux-x64x11_devel/.siso_lock
   ```

3. **Generate the build configuration files** for the `linux-x64x11` platform in the `devel` configuration with Remote Build Execution (RBE) disabled:

   ```bash
   python3 cobalt/build/gn.py -p linux-x64x11 -C devel --no-rbe
   ```

4. **Compile the Cobalt target** using `autoninja`:

   ```bash
   autoninja -C out/linux-x64x11_devel cobalt
   ```

Completed builds are generated in the `out/linux-x64x11_devel` directory of your workspace.
