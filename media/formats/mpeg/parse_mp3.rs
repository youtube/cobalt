// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::bit_reader::SimpleBitReader;
use crate::ffi::MpegAudioHeaderInfo;
use crate::ParserError;

// Versions and layers as defined in ISO/IEC 11172-3.
const K_VERSION_2: usize = 2;
const K_VERSION_RESERVED: usize = 1;
const K_VERSION_2_5: usize = 0;

const K_LAYER_1: usize = 3;
const K_LAYER_2: usize = 2;
const K_LAYER_3: usize = 1;
const K_LAYER_RESERVED: usize = 0;

const K_HEADER_SIZE: usize = 4;

// Maps version and layer information in the frame header into an index for the
// |kBitrateMap|.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
const K_VERSION_LAYER_MAP: [[usize; 4]; 4] = [
    // { reserved, L3, L2, L1 }
    [5, 4, 4, 3], // MPEG 2.5
    [5, 5, 5, 5], // reserved
    [5, 4, 4, 3], // MPEG 2
    [5, 2, 1, 0], // MPEG 1
];

// Maps the bitrate index field in the header and an index from
// |kVersionLayerMap| to a frame bitrate.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
const K_BITRATE_MAP: [[u32; 6]; 16] = [
    // { V1L1, V1L2, V1L3, V2L1, V2L2 & V2L3, reserved }
    [0, 0, 0, 0, 0, 0],
    [32, 32, 32, 32, 8, 0],
    [64, 48, 40, 48, 16, 0],
    [96, 56, 48, 56, 24, 0],
    [128, 64, 56, 64, 32, 0],
    [160, 80, 64, 80, 40, 0],
    [192, 96, 80, 96, 48, 0],
    [224, 112, 96, 112, 56, 0],
    [256, 128, 112, 128, 64, 0],
    [288, 160, 128, 144, 80, 0],
    [320, 192, 160, 160, 96, 0],
    [352, 224, 192, 176, 112, 0],
    [384, 256, 224, 192, 128, 0],
    [416, 320, 256, 224, 144, 0],
    [448, 384, 320, 256, 160, 0],
    [0, 0, 0, 0, 0, 0],
];

// Maps the sample rate index and version fields from the frame header to a
// sample rate.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
const K_SAMPLE_RATE_MAP: [[u32; 4]; 4] = [
    // { V2.5, reserved, V2, V1 }
    [11025, 0, 22050, 44100],
    [12000, 0, 24000, 48000],
    [8000, 0, 16000, 32000],
    [0, 0, 0, 0],
];

const K_BITRATE_FREE: usize = 0;
const K_BITRATE_BAD: usize = 0xf;
const K_SAMPLE_RATE_RESERVED: usize = 3;

// Offset in bytes from the end of the MP3 header to "Xing" or "Info" tags which
// indicate a frame is silent metadata frame.  Values taken from FFmpeg.
const K_XING_HEADER_MAP: [[usize; 2]; 2] = [[32, 17], [17, 9]];

pub fn parse_mp3_header_internal(data: &[u8]) -> Result<MpegAudioHeaderInfo, ParserError> {
    if data.len() < K_HEADER_SIZE {
        return Err(ParserError::NeedMoreData);
    }

    let mut reader = SimpleBitReader::new(data);

    debug_assert!(reader.bits_available() >= K_HEADER_SIZE * 8);
    let sync = reader.read_bits(11).unwrap();
    let version = reader.read_bits(2).unwrap();
    let layer = reader.read_bits(2).unwrap();
    reader.skip_bits(1).unwrap(); // protection_absent
    let bitrate_index = reader.read_bits(4).unwrap();
    let sample_rate_index = reader.read_bits(2).unwrap();
    let has_padding = reader.read_bits(1).unwrap();
    reader.skip_bits(1).unwrap(); // private_bit
    let channel_mode = reader.read_bits(2).unwrap();
    // mode_extension (2), copyright (1), original (1), emphasis (2)
    reader.skip_bits(6).unwrap();

    // Note: For MPEG2 we don't check if a given bitrate or channel layout is
    // allowed per spec since all tested decoders don't seem to care.
    if sync != 0x7ff
        || version == K_VERSION_RESERVED
        || layer == K_LAYER_RESERVED
        || bitrate_index == K_BITRATE_FREE
        || bitrate_index == K_BITRATE_BAD
        || sample_rate_index == K_SAMPLE_RATE_RESERVED
    {
        return Err(ParserError::InvalidHeader);
    }

    let bitrate = K_BITRATE_MAP[bitrate_index][K_VERSION_LAYER_MAP[version][layer]];
    if bitrate == 0 {
        return Err(ParserError::InvalidHeader);
    }

    let frame_sample_rate = K_SAMPLE_RATE_MAP[sample_rate_index][version];
    if frame_sample_rate == 0 {
        return Err(ParserError::InvalidHeader);
    }

    // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
    // Table 2.1.5
    let samples_per_frame = match layer {
        K_LAYER_1 => 384,
        K_LAYER_2 => 1152,
        K_LAYER_3 => {
            if version == K_VERSION_2 || version == K_VERSION_2_5 {
                576
            } else {
                1152
            }
        }
        _ => return Err(ParserError::InvalidHeader),
    };

    // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
    // Text just below Table 2.1.5.
    let mut frame_size = if layer == K_LAYER_1 {
        // This formulation is a slight variation on the equation below,
        // but has slightly different truncation characteristics to deal
        // with the fact that Layer 1 has 4 byte "slots" instead of single
        // byte ones.
        4 * (12 * bitrate * 1000 / frame_sample_rate)
    } else {
        ((samples_per_frame / 8) * bitrate * 1000) / frame_sample_rate
    };

    if has_padding != 0 {
        frame_size += if layer == K_LAYER_1 { 4 } else { 1 };
    }

    let frame_size = frame_size as usize;

    // Check for XING/Info metadata frame (only for Layer 3)
    let mut metadata_frame = false;
    if layer == K_LAYER_3 {
        let xing_header_index = K_XING_HEADER_MAP
            [if version == K_VERSION_2 || version == K_VERSION_2_5 { 1 } else { 0 }]
            [if channel_mode == 3 { 1 } else { 0 }];
        let tag_size = 4; // sizeof(u32)
        let needed_data_for_xing = K_HEADER_SIZE + xing_header_index + tag_size;

        if frame_size >= needed_data_for_xing && data.len() >= needed_data_for_xing {
            let mut xing_reader = SimpleBitReader::new(&data[K_HEADER_SIZE..]);
            if xing_reader.skip_bits(xing_header_index * 8).is_some()
                && matches!(xing_reader.read_bits(tag_size * 8), Some(0x496e666f | 0x58696e67))
            {
                metadata_frame = true;
            }
        }
    }

    return Ok(MpegAudioHeaderInfo {
        frame_size,
        sample_rate: frame_sample_rate,
        // Map Stereo(0), Joint Stereo(1), and Dual Channel (2) to
        // CHANNEL_LAYOUT_STEREO and Single Channel (3) to CHANNEL_LAYOUT_MONO.
        channels: if channel_mode == 3 { 1 } else { 2 },
        sample_count: samples_per_frame,
        metadata_frame,
        esds: 0,
    });
}
