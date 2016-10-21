## Introduction

cobalt tizen is a Cobalt engine port to tizen platform. The port
implements starboard/tizen platform APIs.

## Precondition

1. Install gbs package
  - Add Source list
    $ sudo vi /etc/apt/sources.list
      deb http://download.tizen.org/tools/latest-release/Ubuntu_14.04/ /
    $ sudo apt-get update
    $ sudo apt-get install gbs
    $ gbs --version
## Building

0. Download ruby package(temporally)
    $ wget http://download.tizen.org/snapshots/2.4-base/common/latest/repos/standard/packages/armv7l/ruby-1.9.2-1.6.armv7l.rpm
    $ mv ruby-1.9.2-1.6.armv7l.rpm $HOME/GBS-ROOT-COBALT/local/repos/cobalt/armv7l/RPMS/
      You should create directory or build is generated automatically this directory.

1. Build commands:
  * armv7l
    $ gbs -c packaging/gbs.conf -v build -A armv7l -P profile.cobalt [options]
    Detail option see "gbs --help" and "gbs -v build --help"

cobalt tizen code uses the BSD license, see our `LICENSE` file.