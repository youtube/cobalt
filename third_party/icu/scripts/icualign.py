#!/usr/bin/python3

# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import struct
import sys

PAGE_SIZE = 0x1000  # System page size.
THRESHOLD = 0x2000  # Minimum size of the file to be aligned.


# Read 2 bytes.
def read16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


# Read 4 bytes.
def read32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


# Write 4 bytes.
def write32(data, offset, value):
    return struct.pack_into("<I", data, offset, value)


################################################################################
# (Adapted from `source/tools/toolutil/pkg_gencmn.cpp`)
#
# A .dat package file contains a simple Table of Contents of item names,
# followed by the items themselves:
#
# 1. ToC table
#
# uint32_t count; - number of items
# UDataOffsetTOCEntry entry[count]; - pair of uint32_t values per item:
#     uint32_t nameOffset; - offset of the item name
#     uint32_t dataOffset; - offset of the item data
# both are byte offsets from the beginning of the data
#
# 2. item name strings
#
# All item names are stored as char * strings in one block between the ToC table
# and the data items.
#
# 3. data items
#
# The data items are stored following the item names block.
# The data items are stored in the sorted order of their names.
################################################################################


def pad_data(data):
    out = bytearray()

    header_size = read16(data, 0)           # Size of the ICU header.
    item_count = read32(data, header_size)  # Number of files inside icudtl.dat
    toc_offset = header_size + 4            # Offset of the Table of Contents.

    # Copy everything until the beginning of the data.
    out_offset = read32(data, toc_offset + 4) + header_size
    out += data[:out_offset]

    # Iterate over the files.
    for i in range(item_count):
        # Offset inside the ToC for this file.
        offset = toc_offset + (i * 8)

        # Offset of the name and data, relative to the beginning of the data section.
        name_offset = read32(data, offset)
        data_offset = read32(data, offset + 4)

        # Offset of the name and the data, relative to the beginning of the file.
        name_file_offset = name_offset + header_size
        data_file_offset = data_offset + header_size

        # Calculate the size of this file.
        if i + 1 < item_count:
            next_offset = toc_offset + ((i + 1) * 8)
            next_data_offset = read32(data, next_offset + 4)
            size = next_data_offset - data_offset
        else:
            size = len(data) - (data_offset + header_size)

        # Insert padding to align files bigger than the threshold.
        page_offset = out_offset & (PAGE_SIZE - 1)
        if size >= THRESHOLD and page_offset != 0:
            padding = PAGE_SIZE - page_offset
            out.extend(itertools.repeat(0x00, padding))
            out_offset += padding

        # Put the new offset into the Table of Contents.
        write32(out, offset + 4, out_offset - header_size)

        # Copy the content of the file.
        out += data[data_file_offset : data_file_offset + size]
        out_offset += size

    return out


if __name__ == "__main__":
    # Check arguments.
    if len(sys.argv) != 3:
        error_str = "icualign: wrong number of arguments\n\n"
        help_str = "usage: icualign <infilename> <outfilename>\n\n"
        sys.exit(error_str + help_str)

    # Extract arguments.
    in_filename = sys.argv[1]
    out_filename = sys.argv[2]

    # Read the input file.
    with open(in_filename, "rb") as in_file:
        data = in_file.read()
        # Apply padding to the file to achieve the desired alignment.
        out_data = pad_data(data)
        # Write the output file.
        with open(out_filename, "wb") as out_file:
            out_file.write(out_data)
