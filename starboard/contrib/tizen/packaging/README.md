## Introduction

cobalt tizen is a Cobalt engine port to tizen platform. The port
implements starboard/contrib/tizen platform APIs.

## Precondition

1. Install gbs package
  - Add Source list
    $ sudo vi /etc/apt/sources.list
      deb http://download.tizen.org/tools/latest-release/Ubuntu_14.04/ /
    $ sudo apt-get update
    $ sudo apt-get install gbs
    $ gbs --version

## Building

1. Build commands:
  - Need to add packaging-dir option
    --packaging-dir src/starboard/contrib/tizen/packaging/
  - armv7l
    $ gbs -c packaging/gbs.conf -v build -A armv7l -P profile.cobalt [options]
    Detail option see "gbs --help" and "gbs -v build --help"
    ex) gbs -c src/starboard/contrib/tizen/packaging/gbs.conf -v build -A armv7l -P profile.cobalt --packaging-dir src/starboard/contrib/tizen/packaging/

cobalt tizen code uses the BSD license, see our `LICENSE` file.
