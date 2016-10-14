## Introduction

cobalt tizen is a Cobalt engine port to tizen platform. The port
implements starboard/tizen platform APIs.

## Building

1. Set gbs conf file
   $ cp -f package/sample.gbs.conf $HOME/.gbs.conf

2. Download ruby package(temporally)
   $ wget http://download.tizen.org/snapshots/2.4-base/common/latest/repos/standard/packages/armv7l/ruby-1.9.2-1.6.armv7l.rpm
   $ mv ruby-1.9.2-1.6.armv7l.rpm $HOME/GBS-ROOT-COBALT/local/repos/cobalt/armv7l/RPMS/
   You should create directory or build is generated automatically this directory.

2. Build commands:
* armv7l
   $ gbs -v build -A armv7l [options] -P profile.cobalt
   Detail option see "gbs --help" and "gbs -v build --help"

cobalt tizen code uses the BSD license, see our `LICENSE` file.