#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import path as os_path
import os
import platform
import subprocess
import sys
import os


def GetBinaryPath():
    """Searches $PATH for the binary returned by GetLocalBinary() if missing.

    Look for the system version of Node in the $PATH if it's not found in this
    subdirectory, where the Chromium DEPS would drop it, but the Cobalt DEPS
    does not.
    """
    local_path, binary_name = os.path.split(GetLocalBinary())
    search_paths = [local_path] + os.getenv('PATH').split(os.path.pathsep)
    for binary_path in [os.path.join(p, binary_name) for p in search_paths]:
        if os.access(binary_path, os.X_OK):
            return binary_path
    raise RuntimeError('%s not found in PATH' % binary_name)


def GetLocalBinary():
    return os_path.join(
        os_path.dirname(__file__), *{
            'Darwin': ('mac', 'node-darwin-arm64' if platform.machine()
                       == 'arm64' else 'node-darwin-x64', 'bin', 'node'),
            'Linux': ('linux', 'node-linux-x64', 'bin', 'node'),
            'Windows': ('win', 'node.exe'),
        }[platform.system()])


def RunNode(cmd_parts, output=subprocess.PIPE):
    cmd = [GetBinaryPath()] + cmd_parts
    process = subprocess.Popen(cmd,
                               cwd=os.getcwd(),
                               stdout=output,
                               stderr=output,
                               universal_newlines=True)
    stdout, stderr = process.communicate()

    if process.returncode != 0:
        print('%s failed:\n%s\n%s' % (cmd, stdout, stderr))
        exit(process.returncode)

    return stdout


if __name__ == '__main__':
    args = sys.argv[1:]
    # Accept --output as the first argument, and then remove
    # it from the args entirely if present.
    if len(args) > 0 and args[0] == '--output':
        output = None
        args = sys.argv[2:]
    else:
        output = subprocess.PIPE
    RunNode(args, output)
