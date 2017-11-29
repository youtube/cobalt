#!/bin/bash
# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script configures, verifies, and prepares toolchain paths.
# This script is sourced from download_gcc.sh and download_clang.sh

set -e

script_dir="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
root="$(readlink -f "${script_dir}/../../..")"

canonical_path="$(python "${root}/starboard/tools/get_toolchains_path.py")"
base_path="${COBALT_TOOLCHAINS_DIR:=${canonical_path}}"
toolchain_path="${base_path}/${toolchain_folder}"
toolchain_binary="${toolchain_path}/${binary_path}"

if [ -x "${toolchain_binary}" ]; then
  # The toolchain binary already exist.
  1>&2 echo "${toolchain_name} ${version} already available."
  exit 0
fi

if [ -d "${toolchain_path}" ]; then
  cat <<EOF 1>&2
  WARNING: ${toolchain_name} ${version} folder ${toolchain_path}
  already exists, but it does not contain a binary.  Perhaps a previous download
  was interrupted or failed?
EOF
  rm -rf "${toolchain_path}"
fi

mkdir -p "${toolchain_path}"
cd "${toolchain_path}"

logfile="${toolchain_path}/build_log.txt"

cat <<EOF 1>&2
Downloading and compiling ${toolchain_name} version ${version} into ${toolchain_path}
Log file can be found at ${logfile}
This may take ${build_duration}
EOF
