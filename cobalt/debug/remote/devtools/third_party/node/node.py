#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import path as os_path
import os
import platform
import subprocess
import sys


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
            'Darwin': ('mac', 'node-darwin-x64', 'bin', 'node'),
            'Linux': ('linux', 'node-linux-x64', 'bin', 'node'),
            'Windows': ('win', 'node.exe'),
        }[platform.system()])


def RunNode(cmd_parts, stdout=None):
    cmd = " ".join([GetBinaryPath()] + cmd_parts)
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    stdout, stderr = process.communicate()

    if stderr:
        raise RuntimeError('%s failed: %s' % (cmd, stderr))

    return stdout
