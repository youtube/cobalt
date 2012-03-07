#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install everything needed to build chromium on android that
# requires sudo privileges.
# See http://code.google.com/p/chromium/wiki/AndroidBuildInstructions

DOWNLOAD_URL="http://ftp.us.debian.org/debian/pool/non-free/s/sun-java6"

BIN_FILE_NAME="sun-java6-bin_6.26-0squeeze1_amd64.deb"
JRE_FILE_NAME="sun-java6-jre_6.26-0squeeze1_all.deb"
JDK_FILE_NAME="sun-java6-jdk_6.26-0squeeze1_amd64.deb"

if ! uname -m | egrep -q "i686|x86_64"; then
  echo "Only x86 architectures are currently supported" >&2
  exit
fi

if [ "x$(id -u)" != x0 ]; then
  echo "Running as non-root user."
  echo "You might have to enter your password one or more times for 'sudo'."
  echo
fi

sudo apt-get update

# The temporary directory used to store the downloaded file.
TEMPDIR=$(mktemp -d)
cleanup() {
  local status=${?}
  trap - EXIT
  rm -rf "${TEMPDIR}"
  exit ${status}
}
trap cleanup EXIT

##########################################################
# Download (i.e. wget) and install debian package.
# The current directory is changed in this function.
# Arguments:
#   file_name
# Returns:
#   None
##########################################################
install_deb_pkg() {
  local file_name="${1}"
  local download_url="${DOWNLOAD_URL}/${file_name}"

  cd "${TEMPDIR}"
  wget "${download_url}"

  echo "Install ${file_name}"
  sudo dpkg -i "${file_name}"
}


# Install ant
sudo apt-get install python-pexpect ant

# Install sun-java6-bin
install_deb_pkg "${BIN_FILE_NAME}"

# Install sun-java6-jre
install_deb_pkg "${JRE_FILE_NAME}"

# Install sun-java6-jdk
install_deb_pkg "${JDK_FILE_NAME}"

# Switch version of Java to java-6-sun
sudo update-java-alternatives -s java-6-sun

echo "install-build-deps-android.sh complete."
