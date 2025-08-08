#!/usr/bin/python3

# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import itertools
import sys

PAGE_SIZE = 0x1000  # System page size.
HASH_SIZE = 8       # Size of the hash in bytes.


def hash_pages(data):
    out = bytearray()

    offset = 0
    while offset < len(data):
        # Extract a page and pad it with zeroes in case it's not a full page.
        # This is to emulate the behavior of mmap in Linux (and thus ChromeOS)
        # environments.
        #
        #     "POSIX specifies that the system shall always zero fill any
        #      partial page at the end of the object [...]".
        #
        # Reference: https://man7.org/linux/man-pages/man2/mmap.2.html
        page = bytearray(data[offset : offset + PAGE_SIZE])
        page.extend(itertools.repeat(0x00, PAGE_SIZE - len(page)))

        # Calculate the hash of the page.
        digest = hashlib.blake2b(page, digest_size=HASH_SIZE).digest()
        out.extend(digest)

        offset += PAGE_SIZE

    return out


if __name__ == "__main__":
    # Check arguments.
    if len(sys.argv) != 3:
        error_str = "icuhash: wrong number of arguments\n\n"
        help_str = "usage: icuhash <infilename> <outfilename>\n\n"
        sys.exit(error_str + help_str)

    # Extract arguments.
    in_filename = sys.argv[1]
    out_filename = sys.argv[2]

    # Read the input file.
    with open(in_filename, "rb") as in_file:
        data = in_file.read()
        # Calculate hashes for each page of the file.
        out_data = hash_pages(data)
        # Write the output file.
        with open(out_filename, "wb") as out_file:
            out_file.write(out_data)
