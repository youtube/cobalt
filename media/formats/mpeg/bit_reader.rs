// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// A high-performance, bounds-checked bit reader that reads bits from a byte
/// slice. Modeled after the C++ media::BitReader.
///
/// It buffers bits in an internal 64-bit cache to minimize overhead for
/// consecutive reads. Unlike the C++ reader, reads are currently limited to
/// 32-bits for simplicity. Like the C++ reader, a two register u64 cache would
/// be more efficient to support larger reads (u128 ends up being slower).
pub struct SimpleBitReader<'a> {
    data: &'a [u8],
    data_byte_index: usize,
    cache: u64,
    bits_in_cache: usize,
}

impl<'a> SimpleBitReader<'a> {
    /// Creates a new `SimpleBitReader` reading from the given byte slice.
    pub fn new(data: &'a [u8]) -> Self {
        // MediaSource will parse at most 128kB of data in a single chunk.
        assert!(data.len() <= usize::MAX / 8);
        return Self { data, data_byte_index: 0, cache: 0, bits_in_cache: 0 };
    }

    /// Reads up to 32 bits from the buffer and returns them as a `usize`.
    /// Returns `None` if `num_bits` > 32 or if there are not enough bits
    /// remaining in the buffer.
    pub fn read_bits(&mut self, num_bits: usize) -> Option<usize> {
        assert!(num_bits <= 32);

        if num_bits == 0 {
            return Some(0);
        }

        if num_bits > self.bits_in_cache && !self.refill(num_bits) {
            self.mark_eos();
            return None;
        }

        let result = self.cache >> (64 - num_bits);
        self.cache <<= num_bits;
        self.bits_in_cache -= num_bits;
        return Some(result as usize);
    }

    /// Skips the specified number of bits.
    /// Returns `Some(())` if the skip was successful, or `None` if the skip
    /// would exceed the buffer's bounds.
    pub fn skip_bits(&mut self, num_bits: usize) -> Option<()> {
        if num_bits <= self.bits_in_cache {
            self.cache <<= num_bits;
            self.bits_in_cache -= num_bits;
            return Some(());
        }

        let to_skip = num_bits - self.bits_in_cache;
        let bytes_needed = to_skip.div_ceil(8);

        // Note: This construction avoids overflow if `num_bits` is too large.
        if bytes_needed > self.data.len() - self.data_byte_index {
            self.mark_eos();
            return None;
        }

        let bytes_to_skip = to_skip / 8;
        let bits_left_to_skip = to_skip % 8;

        self.cache = 0;
        self.bits_in_cache = 0;
        self.data_byte_index += bytes_to_skip;

        if bits_left_to_skip > 0 {
            let ok = self.refill(bits_left_to_skip);
            debug_assert!(ok);
            self.cache <<= bits_left_to_skip;
            self.bits_in_cache -= bits_left_to_skip;
        }

        return Some(());
    }

    /// Returns the total number of bits read or skipped so far.
    pub fn bits_read(&self) -> usize {
        return self.data_byte_index * 8 - self.bits_in_cache;
    }

    /// Returns the number of bits available to read.
    pub fn bits_available(&self) -> usize {
        return (self.data.len() - self.data_byte_index) * 8 + self.bits_in_cache;
    }

    /// Marks internal state as end of stream to preclude future reads/skips.
    fn mark_eos(&mut self) {
        self.bits_in_cache = 0;
        self.cache = 0;
        self.data_byte_index = self.data.len();
    }

    /// Refills the internal cache with bits from the input buffer.
    ///
    /// It attempts to load at least `min_bits` into `self.cache`.
    /// - Fast Path: If at least 4 bytes remain in the buffer, it loads 32 bits
    ///   instantly.
    /// - Fallback Path: Loads all remaining bytes into the cache.
    ///
    /// Returns `true` if at least `min_bits` are now available in the cache, or
    /// `false` if the buffer doesn't contain enough data.
    fn refill(&mut self, min_bits: usize) -> bool {
        // read_bits() will refill at most 32 bits and skip_bits() at most 7.
        assert!(min_bits <= 32);

        let remaining = &self.data[self.data_byte_index..];
        if remaining.len() >= 4 {
            let bytes: [u8; 4] = remaining[..4].try_into().unwrap();
            let val = u32::from_be_bytes(bytes) as u64;
            let shift = 64 - self.bits_in_cache - 32;
            self.cache |= val << shift;
            self.bits_in_cache += 32;
            self.data_byte_index += 4;
            return true;
        }

        // We're at the end of `data`, so fill the last few bits.
        let mut val: u64 = 0;
        for &b in remaining {
            val = (val << 8) | (b as u64);
        }
        let loaded_bits = remaining.len() * 8;
        if loaded_bits + self.bits_in_cache < min_bits {
            return false;
        }

        let shift = 64 - self.bits_in_cache - loaded_bits;
        self.cache |= val << shift;
        self.bits_in_cache += loaded_bits;
        self.data_byte_index = self.data.len();
        return true;
    }
}
