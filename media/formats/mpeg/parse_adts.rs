// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::bit_reader::SimpleBitReader;
use crate::ffi::MpegAudioHeaderInfo;
use crate::ParserError;

const K_HEADER_SIZE: usize = 7;
const K_SAMPLES_PER_AAC_FRAME: u32 = 1024;

const K_ADTS_FREQUENCY_TABLE: &[u32] =
    &[96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350];

const K_ADTS_CHANNELS_TABLE: &[u32] = &[
    0, // index 0 (unsupported/discrete)
    1, // Mono
    2, // Stereo
    3, // Surround
    4, // 4.0
    5, // 5.0 Back
    6, // 5.1 Back
    8, // 7.1
];

pub fn parse_adts_frame_header_internal(data: &[u8]) -> Result<MpegAudioHeaderInfo, ParserError> {
    if data.len() < K_HEADER_SIZE {
        return Err(ParserError::NeedMoreData);
    }

    let mut reader = SimpleBitReader::new(data);

    debug_assert!(reader.bits_available() >= K_HEADER_SIZE * 8);
    let sync = reader.read_bits(12).unwrap();
    reader.skip_bits(1).unwrap(); // MPEG ID
    let layer = reader.read_bits(2).unwrap();
    let protection_absent = reader.read_bits(1).unwrap();
    let profile = reader.read_bits(2).unwrap();
    let sample_rate_index = reader.read_bits(4).unwrap();
    reader.skip_bits(1).unwrap(); // private bit
    let channel_layout_index = reader.read_bits(3).unwrap();
    // original/copy (1), home (1), copyright id (1), copyright id start (1)
    reader.skip_bits(4).unwrap();
    let frame_length = reader.read_bits(13).unwrap();
    reader.skip_bits(11).unwrap(); // adts_buffer_fullness
    let num_data_blocks = reader.read_bits(2).unwrap();

    if protection_absent == 0 {
        reader.skip_bits(16).ok_or(ParserError::NeedMoreData)?; // CRC
    }

    let bytes_read = reader.bits_read() / 8;

    if sync != 0xfff
        || layer != 0
        || frame_length < bytes_read
        || sample_rate_index >= K_ADTS_FREQUENCY_TABLE.len()
        || channel_layout_index >= K_ADTS_CHANNELS_TABLE.len()
    {
        return Err(ParserError::InvalidHeader);
    }

    debug_assert_ne!(sample_rate_index, 15);
    let esds = (((((profile + 1) << 4) + sample_rate_index) << 4) + channel_layout_index) << 3;

    return Ok(MpegAudioHeaderInfo {
        frame_size: frame_length,
        sample_rate: K_ADTS_FREQUENCY_TABLE[sample_rate_index],
        channels: K_ADTS_CHANNELS_TABLE[channel_layout_index],
        sample_count: (num_data_blocks as u32 + 1) * K_SAMPLES_PER_AAC_FRAME,
        metadata_frame: false,
        esds: esds as u16,
    });
}
