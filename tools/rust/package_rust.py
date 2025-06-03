#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import lzma
import os
import platform
import shutil
import subprocess
import sys
import tarfile

THIS_DIR = os.path.dirname(__file__)
sys.path.append(
    os.path.join(os.path.dirname(THIS_DIR), '..', 'clang', 'scripts'))

from build_rust import (RUST_TOOLCHAIN_OUT_DIR, THIRD_PARTY_DIR)
from update_rust import (GetRustClangRevision)
from package import (MaybeUpload, TeeCmd, DEFAULT_GCS_BUCKET)
from update import (CHROMIUM_DIR)

PACKAGE_VERSION = GetRustClangRevision()
BUILDLOG_NAME = f'rust-buildlog-{PACKAGE_VERSION}.txt'
RUST_TOOLCHAIN_PACKAGE_NAME = f'rust-toolchain-{PACKAGE_VERSION}.tar.xz'


def BuildCrubit(build_mac_arm):
    with open(os.path.join(THIRD_PARTY_DIR, BUILDLOG_NAME), 'w') as log:
        build_cmd = [sys.executable, os.path.join(THIS_DIR, 'build_crubit.py')]
        if build_mac_arm:
            build_cmd.append('--build-mac-arm')
        # TODO(lukasza): Default to `fail_hard` once we actually depend on the
        # build step (i.e. once we start packaging Crubit).
        TeeCmd(build_cmd, log, fail_hard=False)

    # TODO(lukasza): Rename this function to BuildAndInstallCrubit and actually
    # install Crubit binaries into RUST_TOOLCHAIN_OUT_DIR/bin (once we gain
    # confidence that Crubit continues to build uneventfully on the bots).


def main():
    parser = argparse.ArgumentParser(description='build and package Rust')
    parser.add_argument('--upload',
                        action='store_true',
                        help='upload package to GCS')
    parser.add_argument(
        '--bucket',
        default=DEFAULT_GCS_BUCKET,
        help='Google Cloud Storage bucket where the target archive is uploaded'
    )
    parser.add_argument('--build-mac-arm',
                        action='store_true',
                        help='Build arm binaries. Only valid on macOS.')
    args = parser.parse_args()

    if args.build_mac_arm and sys.platform != 'darwin':
        print('--build-mac-arm only valid on macOS')
        return 1
    if args.build_mac_arm and platform.machine() == 'arm64':
        print('--build-mac-arm only valid on intel to cross-build arm')
        return 1

    # The gcs_platform logic copied from `//tools/clang/scripts/upload.sh`.
    if sys.platform == 'darwin':
        # The --build-mac-intel switch can be used to force the Mac build to
        # target arm64.
        if args.build_mac_arm or platform.machine() == 'arm64':
            gcs_platform = 'Mac_arm64'
        else:
            gcs_platform = 'Mac'
    elif sys.platform == 'win32':
        gcs_platform = 'Win'
    else:
        gcs_platform = 'Linux_x64'

    # Clean build output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    with open(os.path.join(THIRD_PARTY_DIR, BUILDLOG_NAME), 'w') as log:
        # Build the Rust toolchain.
        build_cmd = [sys.executable, os.path.join(THIS_DIR, 'build_rust.py')]
        if args.build_mac_arm:
            build_cmd.append('--build-mac-arm')
        TeeCmd(build_cmd, log)

        # Build bindgen.
        build_cmd = [
            sys.executable,
            os.path.join(THIS_DIR, 'build_bindgen.py')
        ]
        if args.build_mac_arm:
            build_cmd.append('--build-mac-arm')
        TeeCmd(build_cmd, log)

    BuildCrubit(args.build_mac_arm)

    # Strip everything in bin/ to reduce the package size.
    bin_dir_path = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin')
    if sys.platform != 'win32' and os.path.exists(bin_dir_path):
        for f in os.listdir(bin_dir_path):
            file_path = os.path.join(bin_dir_path, f)
            if not os.path.islink(file_path):
                subprocess.call(['strip', file_path])

    with tarfile.open(os.path.join(THIRD_PARTY_DIR,
                                   RUST_TOOLCHAIN_PACKAGE_NAME),
                      'w:xz',
                      preset=9 | lzma.PRESET_EXTREME) as tar:
        tar.add(RUST_TOOLCHAIN_OUT_DIR, arcname='rust-toolchain')

    os.chdir(THIRD_PARTY_DIR)
    MaybeUpload(args.upload, args.bucket, RUST_TOOLCHAIN_PACKAGE_NAME,
                gcs_platform)
    MaybeUpload(args.upload, args.bucket, BUILDLOG_NAME, gcs_platform)

    return 0


if __name__ == '__main__':
    sys.exit(main())
