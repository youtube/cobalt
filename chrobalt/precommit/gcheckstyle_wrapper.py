#!/usr/bin/env python3
#
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""Wrapper to run internal gcheckstyle tool."""

import subprocess
import sys

if __name__ == '__main__':
    gcheckstyle_args = sys.argv[1:]

    try:
        sys.exit(
            subprocess.call([
                '/home/build/nonconf/google3/tools/java/checkstyle/gcheckstyle.sh'
            ] + gcheckstyle_args))
    except FileNotFoundError:
        print('gcheckstyle not found, skipping.')
        sys.exit(0)
    except OSError as e:
        print('You may need to run gcert.')
        raise e
