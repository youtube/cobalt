#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates an array of images to be preloaded as a ES6 Module."""

import argparse
import json
import os
import shlex
import sys


def main():
    argument_parser = argparse.ArgumentParser()
    argument_parser.add_argument(
        '--output_file',
        help='The output js file exporting preload images array')
    argument_parser.add_argument(
        '--images_list_file',
        help='File contains a list of images to be appended')
    args = argument_parser.parse_args()
    with open(args.images_list_file) as f:
        files = shlex.split(f.read())

    images = {}
    for image in files:
        with open(image, 'r', encoding='utf-8') as f:
            images[os.path.basename(image)] = f.read()

    with open(args.output_file, 'w', encoding='utf-8') as f:
        filenames = [os.path.basename(f) for f in files]
        f.write('export const preloadImagesList = %s;' %
                json.dumps(filenames, indent=2))
        f.write('export const preloadedImages = %s;' % json.dumps(images))

    return 0


if __name__ == '__main__':
    sys.exit(main())
