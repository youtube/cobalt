---
layout: doc
title: "Set up your environment - Docker"
---

We provide <a
href="https://cobalt.googlesource.com/cobalt/+/refs/heads/22.lts.stable/src/docker/linux/">Docker image definitions</a> to simplify managing build environments.

The instructions below assume Docker is installed and is able to run basic
hello-world verification. `docker-compose` command is expected to be available as well.

## Set up your workstation

Clone the Cobalt code repository. The following `git` command creates a
`cobalt` directory that contains the repository:

```
$ git clone https://cobalt.googlesource.com/cobalt
$ cd cobalt
```

### Usage

The simplest usage is:

```
docker-compose run <platform>
```

By default, a `debug` build will be built, with `cobalt` as a target.
You can override this with an environment variable, e.g.

```
docker-compose run -e CONFIG=devel -e TARGET=nplb <platform>
```

where config is one of the four optimization levels, `debug`, `devel`,
`qa` and `gold`, and target is the build target passed to ninja

See <a
href="https://cobalt.googlesource.com/cobalt/+/refs/heads/22.lts.stable/src/README.md#build-types">Cobalt README</a> for full details.

Builds will be available in your `${COBALT_SRC}/out` directory.

<aside class="note">
Note that Docker runs processes as root user by default, hence
output files in `src/out/<platform>` directory have `root` as file owner.
</aside>

### Customization

To parametrize base operating system images used for the build, pass
`BASE_OS` as an argument to `docker-compose` as follows:

```
docker-compose build --build-arg BASE_OS="ubuntu:bionic" base
```

This parameter is defined in `docker/linux/base/Dockerfile` and is passed
to Docker `FROM ...` statement.

Available parameters for customizing container execution are:

**BASE_OS**: passed to `base` image at build time to select a Debian-based
   base os image and version. Defaults to Debian 10. `ubuntu:bionic` and
   `ubuntu:xenial` are other tested examples.

**PLATFORM**: Cobalt build platform, passed to GN

**CONFIG**: Cobalt build config, passed to GN. Defaults to `debug`

**TARGET**: Build target, passed to `ninja`

The `docker-compose.yml` contains the currently defined experimental build
configurations. Edit or add new `service` entries as needed, to build custom
configurations.


### Pre-built images

Note: Pre-built images from a public container registry are not yet available.

### Troubleshooting

To debug build issues, enter the shell of the corresponding build container
by launching the bash shell, i.e.

```
docker-compose run linux-x64x11 /bin/bash
```

and try to build Cobalt with the <a
href="https://cobalt.googlesource.com/cobalt/+/refs/heads/22.lts.stable/src/README.md#building-and-running-the-code">usual GN / ninja flow.</a>
