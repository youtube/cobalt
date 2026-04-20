#!/usr/bin/env python
# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""Simple helper to unzip a given archive to a specified directory."""

import argparse
import sys
import zipfile


def main():
  argument_parser = argparse.ArgumentParser()
  argument_parser.add_argument('--zip_file', required=True)
  argument_parser.add_argument('--output_dir', required=True)
  args = argument_parser.parse_args()

  with zipfile.ZipFile(args.zip_file, 'r') as zip_ref:
    zip_ref.extractall(args.output_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
