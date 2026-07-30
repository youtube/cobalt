// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//media/formats:safe_mpeg_audio_parsers";
}

use rust_gtest_interop::prelude::*;
use safe_mpeg_audio_parsers::bit_reader::SimpleBitReader;

#[gtest(BitReaderTest, NormalOperation)]
fn test_normal_operation() {
    // 0101 0101 1001 1001 repeats 4 times
    let buffer = [0x55, 0x99, 0x55, 0x99, 0x55, 0x99, 0x55, 0x99];
    let mut reader1 = SimpleBitReader::new(&buffer[..6]); // 6 bytes only

    expect_eq!(reader1.read_bits(1), Some(0));
    expect_eq!(reader1.read_bits(8), Some(0xab)); // 1010 1011
    expect_true!(reader1.read_bits(7).is_some());
    expect_eq!(reader1.read_bits(32), Some(0x55995599));
    expect_eq!(reader1.read_bits(1), None); // out of bounds!
    expect_eq!(reader1.read_bits(0), Some(0));

    let mut reader2 = SimpleBitReader::new(&buffer);
    expect_eq!(reader2.read_bits(32), Some(0x55995599));
    expect_eq!(reader2.read_bits(32), Some(0x55995599));
    expect_eq!(reader2.read_bits(1), None);
    expect_eq!(reader2.read_bits(0), Some(0));
}

#[gtest(BitReaderTest, ReadBeyondEnd)]
fn test_read_beyond_end() {
    let buffer = [0x12];
    let mut reader = SimpleBitReader::new(&buffer);

    expect_eq!(reader.read_bits(4), Some(0x01)); // 0001
    expect_eq!(reader.read_bits(5), None);
    expect_eq!(reader.read_bits(1), None);
    expect_eq!(reader.read_bits(0), Some(0));
}

#[gtest(BitReaderTest, SkipBits)]
fn test_skip_bits() {
    let buffer = [0x0a, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
    let mut reader = SimpleBitReader::new(&buffer);

    expect_true!(reader.skip_bits(2).is_some());
    expect_eq!(reader.read_bits(3), Some(1));
    expect_true!(reader.skip_bits(11).is_some());
    expect_eq!(reader.read_bits(8), Some(3));
    expect_true!(reader.skip_bits(76).is_some());
    expect_eq!(reader.read_bits(4), Some(13));
    expect_true!(reader.skip_bits(100).is_none());
    expect_true!(reader.skip_bits(usize::MAX).is_none());
    expect_true!(reader.skip_bits(0).is_some());
    expect_true!(reader.skip_bits(1).is_none());
}

#[gtest(BitReaderTest, BitsRead)]
fn test_bits_read() {
    let buffer = [0x0a, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
    let mut reader = SimpleBitReader::new(&buffer);

    expect_true!(reader.skip_bits(2).is_some());
    expect_eq!(reader.bits_read(), 2);
    expect_eq!(reader.read_bits(3), Some(1));
    expect_eq!(reader.bits_read(), 5);
    expect_eq!(reader.read_bits(1), Some(0)); // Read Flag
    expect_eq!(reader.bits_read(), 6);
    expect_true!(reader.skip_bits(76).is_some());
    expect_eq!(reader.bits_read(), 82);
}

#[gtest(BitReaderTest, SlowPathRefill)]
fn test_slow_path_refill() {
    // 1-byte buffer (< 4 bytes)
    let mut reader1 = SimpleBitReader::new(&[0xab]);
    expect_eq!(reader1.read_bits(4), Some(0xa));
    expect_eq!(reader1.read_bits(4), Some(0xb));
    expect_eq!(reader1.read_bits(1), None);

    // 2-byte buffer (< 4 bytes)
    let mut reader2 = SimpleBitReader::new(&[0x12, 0x34]);
    expect_eq!(reader2.read_bits(16), Some(0x1234));
    expect_eq!(reader2.read_bits(1), None);

    // 3-byte buffer (< 4 bytes)
    let mut reader3 = SimpleBitReader::new(&[0x55, 0x66, 0x77]);
    expect_eq!(reader3.read_bits(24), Some(0x556677));
    expect_eq!(reader3.read_bits(1), None);
}
