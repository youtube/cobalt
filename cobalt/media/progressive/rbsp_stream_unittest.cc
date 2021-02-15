// Copyright 2012 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/progressive/rbsp_stream.h"

#include <list>
#include <memory>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {

class RBSPStreamTest : public testing::Test {
 protected:
  RBSPStreamTest() {}

  virtual ~RBSPStreamTest() {}

  // Given num encode the value in signed exp-golomb syntax and push
  // the value on the provided bitlist
  void EncodeSEV(int32 num, std::list<bool>& bits) {
    bool is_negative = (num < 0);
    uint32 unum = 0;
    if (is_negative) {
      unum = (uint32)(num * -1);
    } else {
      unum = (uint32)num;
    }
    // multiply unsigned value by 2
    unum = unum << 1;
    // subtract one from the positive values
    if (!is_negative) {
      --unum;
    }
    // encode the resulting uev
    EncodeUEV(unum, bits);
  }

  // Given num encode the value in unsigned exp-golomb syntax and push
  // the value on to the provided bitlist
  void EncodeUEV(uint32 num, std::list<bool>& bits) {
    // find largest (2^pow) - 1 smaller than num
    uint32 pow = 31;
    uint32 base = 0x7fffffff;
    while (base > num) {
      pow--;
      base = base >> 1;
    }
    // encoding calls for pow leading zeros, followed by a 1, followed
    // by pow digits of the input number - ((2^pow) - 1).
    // we move from MSb to LSb, so start by pushing back the leading 0s
    for (int i = 0; i < pow; i++) {
      bits.push_back(false);
    }
    // now push the separating one
    bits.push_back(true);
    // and now pow bits of the remainder bitfield MSb to LSb
    uint32 remainder = num - base;
    for (int i = pow - 1; i >= 0; --i) {
      bits.push_back((remainder >> i) & 0x01);
    }
  }

  // after building a bitlist in various fun ways call this method to
  // create a buffer on the heap that can be passed to RBSPStream
  // for deserialization.
  std::unique_ptr<uint8[]> SerializeToBuffer(const std::list<bool>& bitlist,
                                             bool add_sequence_bytes,
                                             size_t& buffer_size_out) {
    // start by building a list of bytes, so we can add the
    // 00 00 => 00 00 03 sequence bytes
    std::list<uint8> bytelist;
    uint8 push_byte = 0;
    uint32 bit_counter = 0;
    for (std::list<bool>::const_iterator it = bitlist.begin();
         it != bitlist.end(); ++it) {
      bit_counter++;
      push_byte = push_byte << 1;
      if (*it) {
        push_byte |= 1;
      }
      if (!(bit_counter % 8)) {
        bytelist.push_back(push_byte);
        push_byte = 0;
      }
    }
    // push any remaining bits on as the final byte
    if (bit_counter % 8) {
      bytelist.push_back(push_byte << (8 - (bit_counter % 8)));
    }
    // if we should add sequence bytes we iterate through the new
    // byte list looking for 00 00 and inserting a 03 after each.
    if (add_sequence_bytes) {
      int num_zeros = 0;
      for (std::list<uint8>::iterator it = bytelist.begin();
           it != bytelist.end(); ++it) {
        // if we just passed two sequential zeros insert a 03
        if (num_zeros == 2) {
          bytelist.insert(it, 0x03);
          // reset the counter
          num_zeros = 0;
        }
        if (*it == 0) {
          ++num_zeros;
        } else {
          num_zeros = 0;
        }
      }
    } else {
      // we will need to detect any naturally occurring 00 00 03s
      // and protect them from removal of the 03, by inserting a
      // second 03
      int num_zeros = 0;
      for (std::list<uint8>::iterator it = bytelist.begin();
           it != bytelist.end(); ++it) {
        if ((num_zeros >= 2) && (*it == 0x03)) {
          bytelist.insert(it, 0x03);
        }
        if (*it == 0) {
          ++num_zeros;
        } else {
          num_zeros = 0;
        }
      }
    }
    // alright we can make the final output buffer
    std::unique_ptr<uint8[]> buf(new uint8[bytelist.size()]);
    int index = 0;
    for (std::list<uint8>::iterator it = bytelist.begin(); it != bytelist.end();
         it++) {
      buf[index] = *it;
      index++;
    }
    buffer_size_out = bytelist.size();
    return std::move(buf);
  }
};

TEST_F(RBSPStreamTest, ReadUEV) {
  std::list<bool> fibbits;
  // encode first 47 Fibonacci numbers
  uint32 f_n_minus_2 = 0;
  EncodeUEV(f_n_minus_2, fibbits);
  uint32 f_n_minus_1 = 1;
  EncodeUEV(f_n_minus_1, fibbits);
  for (int i = 2; i < 47; i++) {
    uint32 f_n = f_n_minus_1 + f_n_minus_2;
    EncodeUEV(f_n, fibbits);
    // update values
    f_n_minus_2 = f_n_minus_1;
    f_n_minus_1 = f_n;
  }
  // convert to buffer
  size_t fib_buffer_size = 0;
  std::unique_ptr<uint8[]> fib_buffer =
      SerializeToBuffer(fibbits, true, fib_buffer_size);
  size_t fib_buffer_no_sequence_size;
  std::unique_ptr<uint8[]> fib_buffer_no_sequence =
      SerializeToBuffer(fibbits, false, fib_buffer_no_sequence_size);
  RBSPStream fib_stream(fib_buffer.get(), fib_buffer_size);
  RBSPStream fib_stream_no_sequence(fib_buffer_no_sequence.get(),
                                    fib_buffer_no_sequence_size);
  // deserialize the same sequence from both buffers
  uint32 uev = 0;
  uint32 uev_n = 0;
  f_n_minus_2 = 0;
  ASSERT_TRUE(fib_stream.ReadUEV(&uev));
  ASSERT_EQ(uev, f_n_minus_2);
  ASSERT_TRUE(fib_stream_no_sequence.ReadUEV(&uev_n));
  ASSERT_EQ(uev_n, f_n_minus_2);

  f_n_minus_1 = 1;
  ASSERT_TRUE(fib_stream.ReadUEV(&uev));
  ASSERT_EQ(uev, f_n_minus_1);
  ASSERT_TRUE(fib_stream_no_sequence.ReadUEV(&uev_n));
  ASSERT_EQ(uev_n, f_n_minus_1);

  for (int i = 2; i < 47; i++) {
    uint32 f_n = f_n_minus_1 + f_n_minus_2;
    ASSERT_TRUE(fib_stream.ReadUEV(&uev));
    ASSERT_EQ(uev, f_n);
    ASSERT_TRUE(fib_stream_no_sequence.ReadUEV(&uev_n));
    ASSERT_EQ(uev_n, f_n);
    f_n_minus_2 = f_n_minus_1;
    f_n_minus_1 = f_n;
  }
  // subsequent call to ReadUEV should fail
  ASSERT_FALSE(fib_stream.ReadUEV(&uev));
  ASSERT_FALSE(fib_stream_no_sequence.ReadUEV(&uev_n));
}

TEST_F(RBSPStreamTest, ReadSEV) {
  std::list<bool> lucasbits;
  // encode first 44 Lucas numbers with alternating sign
  int32 l_n_minus_2 = 1;
  EncodeSEV(l_n_minus_2, lucasbits);
  int32 l_n_minus_1 = 2;
  EncodeSEV(-l_n_minus_1, lucasbits);
  for (int i = 2; i < 44; ++i) {
    int32 l_n = l_n_minus_1 + l_n_minus_2;
    if (i % 2) {
      EncodeSEV(-l_n, lucasbits);
    } else {
      EncodeSEV(l_n, lucasbits);
    }
    l_n_minus_2 = l_n_minus_1;
    l_n_minus_1 = l_n;
  }
  // convert to buffers
  size_t lucas_seq_buffer_size = 0;
  std::unique_ptr<uint8[]> lucas_seq_buffer =
      SerializeToBuffer(lucasbits, true, lucas_seq_buffer_size);
  size_t lucas_deseq_buffer_size = 0;
  std::unique_ptr<uint8[]> lucas_deseq_buffer =
      SerializeToBuffer(lucasbits, false, lucas_deseq_buffer_size);
  RBSPStream lucas_seq_stream(lucas_seq_buffer.get(), lucas_seq_buffer_size);
  RBSPStream lucas_deseq_stream(lucas_deseq_buffer.get(),
                                lucas_deseq_buffer_size);
  l_n_minus_2 = 1;
  l_n_minus_1 = 2;
  int32 sev = 0;
  int32 sev_n = 0;
  ASSERT_TRUE(lucas_seq_stream.ReadSEV(&sev));
  ASSERT_EQ(sev, 1);
  ASSERT_TRUE(lucas_deseq_stream.ReadSEV(&sev_n));
  ASSERT_EQ(sev_n, 1);
  ASSERT_TRUE(lucas_seq_stream.ReadSEV(&sev));
  ASSERT_EQ(sev, -2);
  ASSERT_TRUE(lucas_deseq_stream.ReadSEV(&sev_n));
  ASSERT_EQ(sev_n, -2);
  for (int i = 2; i < 44; ++i) {
    int32 l_n = l_n_minus_1 + l_n_minus_2;
    ASSERT_TRUE(lucas_seq_stream.ReadSEV(&sev));
    ASSERT_TRUE(lucas_deseq_stream.ReadSEV(&sev_n));
    if (i % 2) {
      ASSERT_EQ(-sev, l_n);
      ASSERT_EQ(-sev_n, l_n);
    } else {
      ASSERT_EQ(sev, l_n);
      ASSERT_EQ(sev_n, l_n);
    }
    l_n_minus_2 = l_n_minus_1;
    l_n_minus_1 = l_n;
  }
  // subsequent calls to ReadSEV should fail
  ASSERT_FALSE(lucas_seq_stream.ReadSEV(&sev));
  ASSERT_FALSE(lucas_deseq_stream.ReadSEV(&sev_n));
}

static const uint8 kTestRBSPExpGolombTooBig[] = {
    // 15 leading zeros, should be fine
    // 0000000000000001010101010101010
    //  = 2^15 - 1  + read_bits(010101010101010)
    //  = 32768 - 1 + 10922 = 43689 unsigned, 21845 signed
    // 0000 0000 0000 0001 0101 0101 0101 010+0 (first 0 of next number)
    0x00, 0x01, 0x55, 0x54,
    // 31 leading zeros, should be fine
    // 000000000000000000000000000000010000000000000000000000000000001
    //  = 2^31 - 1 + 1 = 2147483648 unsigned, -1073741824 signed
    // 0 appended on to last byte
    // 0000 0000 0000 0000 0000 0000 0000 0010 0000 0000 0000 0000 0000 0000
    0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
    // 0000 01+00 (first 2 zeros of next number)
    0x04,
    // 32 leading zeros, should not be ok
    // 00000000000000000000000000000000111111111111111111111111111111111
    // = 2^32 - 1 + 2^32 = 2^33 - 1 = 8589934591
    // 00 appended on to last byte
    // 0000 0000 0000 0000 0000 0000 0000 0011 1111 1111 1111 1111 1111 1111
    0x00, 0x00, 0x00, 0x03, 0x03, 0xff, 0xff, 0xff,
    // 1111 111+0 (to complete the byte)
    0xfe};

TEST_F(RBSPStreamTest, ReadUEVTooLarge) {
  // construct a stream from the supplied test data
  RBSPStream uev_too_big(kTestRBSPExpGolombTooBig,
                         sizeof(kTestRBSPExpGolombTooBig));
  // first call should succeed
  uint32 uev = 0;
  ASSERT_TRUE(uev_too_big.ReadUEV(&uev));
  ASSERT_EQ(uev, 43689);
  // as should the second call
  ASSERT_TRUE(uev_too_big.ReadUEV(&uev));
  ASSERT_EQ(uev, 2147483648u);
  // third should fail
  ASSERT_FALSE(uev_too_big.ReadUEV(&uev));
}

TEST_F(RBSPStreamTest, ReadSEVTooLarge) {
  // construct a stream from the supplied test data
  RBSPStream sev_too_big(kTestRBSPExpGolombTooBig,
                         sizeof(kTestRBSPExpGolombTooBig));
  // first call should succeed
  int32 sev = 0;
  ASSERT_TRUE(sev_too_big.ReadSEV(&sev));
  ASSERT_EQ(sev, 21845);
  // as should the second call
  ASSERT_TRUE(sev_too_big.ReadSEV(&sev));
  ASSERT_EQ(sev, -1073741824);
  // third should fail
  ASSERT_FALSE(sev_too_big.ReadSEV(&sev));
}

TEST_F(RBSPStreamTest, ReadBit) {
  std::list<bool> padded_ones;
  // build a bitfield of 1 padded by n zeros, for n in range[0, 1024]
  for (int i = 0; i < 1024; i++) {
    for (int j = 0; j < i; j++) {
      padded_ones.push_back(false);
    }
    padded_ones.push_back(true);
  }
  // build the buffer with sequence bits and without
  size_t sequence_buff_size = 0;
  std::unique_ptr<uint8[]> sequence_buff =
      SerializeToBuffer(padded_ones, true, sequence_buff_size);
  RBSPStream seq_stream(sequence_buff.get(), sequence_buff_size);

  size_t desequence_buff_size = 0;
  std::unique_ptr<uint8[]> desequence_buff =
      SerializeToBuffer(padded_ones, false, desequence_buff_size);
  RBSPStream deseq_stream(desequence_buff.get(), desequence_buff_size);
  for (std::list<bool>::iterator it = padded_ones.begin();
       it != padded_ones.end(); ++it) {
    uint8 bit = 0;
    ASSERT_TRUE(seq_stream.ReadBit(&bit));
    ASSERT_EQ(*it, static_cast<bool>(bit));
    uint8 deseq_bit = 0;
    ASSERT_TRUE(deseq_stream.ReadBit(&deseq_bit));
    ASSERT_EQ(*it, static_cast<bool>(deseq_bit));
  }

  // there should be less than a byte in the either stream
  uint8 fail_byte = 0;
  ASSERT_FALSE(seq_stream.ReadByte(&fail_byte));
  ASSERT_FALSE(deseq_stream.ReadByte(&fail_byte));
}

TEST_F(RBSPStreamTest, ReadByte) {
  // build a field of 16 x (0xaa byte followed by 0 bit)
  std::list<bool> aa_field;
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 8; ++j) {
      aa_field.push_back(!(j % 2));
    }
    aa_field.push_back(false);
  }
  // deseqbuff will be identical due to dense packing of 01 pattern
  size_t aabuff_size = 0;
  std::unique_ptr<uint8[]> aabuff =
      SerializeToBuffer(aa_field, true, aabuff_size);
  RBSPStream aa_stream(aabuff.get(), aabuff_size);
  for (int i = 0; i < 16; ++i) {
    uint8 aa = 0;
    ASSERT_TRUE(aa_stream.ReadByte(&aa));
    ASSERT_EQ(aa, 0xaa);
    // read the zero separator bit
    uint8 zero = 0;
    ASSERT_TRUE(aa_stream.ReadBit(&zero));
    ASSERT_EQ(zero, 0);
  }

  // build a field of 24 x (1 bit, 4 bytes of 0, one 03 byte, 4 bytes of 0)
  std::list<bool> zero_field;
  for (int i = 0; i < 24; ++i) {
    zero_field.push_back(true);
    for (int j = 0; j < 32; ++j) {
      zero_field.push_back(false);
    }
    zero_field.push_back(false);
    zero_field.push_back(false);
    zero_field.push_back(false);
    zero_field.push_back(false);
    zero_field.push_back(false);
    zero_field.push_back(false);
    zero_field.push_back(true);
    zero_field.push_back(true);
    for (int j = 0; j < 32; ++j) {
      zero_field.push_back(false);
    }
  }
  size_t zseqbuff_size = 0;
  std::unique_ptr<uint8[]> zseqbuff =
      SerializeToBuffer(zero_field, true, zseqbuff_size);
  RBSPStream zseq_stream(zseqbuff.get(), zseqbuff_size);
  size_t zdseqbuff_size = 0;
  std::unique_ptr<uint8[]> zdseqbuff =
      SerializeToBuffer(zero_field, false, zdseqbuff_size);
  RBSPStream zdseq_stream(zdseqbuff.get(), zdseqbuff_size);
  for (int i = 0; i < 24; ++i) {
    // read the leading 1 bit
    uint8 seq_bit = 0;
    ASSERT_TRUE(zseq_stream.ReadBit(&seq_bit));
    ASSERT_EQ(seq_bit, 1);
    uint8 dseq_bit = 0;
    ASSERT_TRUE(zdseq_stream.ReadBit(&dseq_bit));
    ASSERT_EQ(dseq_bit, 1);
    // read 4 zeros
    uint8 seq_byte = 0;
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    uint8 dseq_byte = 0;
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
    // read the 3
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0x03);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0x03);
    // read the remaining 4 zeros
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    ASSERT_TRUE(zseq_stream.ReadByte(&seq_byte));
    ASSERT_EQ(seq_byte, 0);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
    ASSERT_TRUE(zdseq_stream.ReadByte(&dseq_byte));
    ASSERT_EQ(dseq_byte, 0);
  }
}

TEST_F(RBSPStreamTest, ReadBits) {
  // test the assertion in the ReadBits comment, as it had a bug :)
  std::list<bool> seventeen_ones;
  for (int i = 0; i < 17; ++i) {
    seventeen_ones.push_back(true);
  }
  size_t seventeen_ones_size = 0;
  std::unique_ptr<uint8[]> seventeen_ones_buff =
      SerializeToBuffer(seventeen_ones, false, seventeen_ones_size);
  RBSPStream seventeen_ones_stream(seventeen_ones_buff.get(),
                                   seventeen_ones_size);
  uint32 seventeen_ones_word = 0;
  ASSERT_TRUE(seventeen_ones_stream.ReadBits(17, &seventeen_ones_word));
  ASSERT_EQ(seventeen_ones_word, 0x0001ffff);

  // serialize all powers of two from 2^0 to 2^31
  std::list<bool> pows;
  for (int i = 0; i < 32; ++i) {
    pows.push_back(true);
    for (int j = 0; j < i; ++j) {
      pows.push_back(false);
    }
  }
  size_t pows_size = 0;
  std::unique_ptr<uint8[]> pows_buff = SerializeToBuffer(pows, true, pows_size);
  RBSPStream pows_stream(pows_buff.get(), pows_size);
  // ReadBits(0) should succeed and not modify the value of the ref output or
  // internal bit iterator
  uint32 dont_touch = 0xfeedfeed;
  ASSERT_TRUE(pows_stream.ReadBits(0, &dont_touch));
  ASSERT_EQ(dont_touch, 0xfeedfeed);
  // compare deserializations
  for (int i = 0; i < 32; ++i) {
    uint32 bits = 0;
    ASSERT_TRUE(pows_stream.ReadBits(i + 1, &bits));
    ASSERT_EQ(bits, (uint32)(1 << i));
  }
}

TEST_F(RBSPStreamTest, SkipBytes) {
  // serialize all nine-bit values from zero to 512
  std::list<bool> nines;
  for (int i = 0; i < 512; ++i) {
    for (int j = 8; j >= 0; --j) {
      nines.push_back((i >> j) & 1);
    }
  }
  size_t nines_size = 0;
  std::unique_ptr<uint8[]> nines_buff =
      SerializeToBuffer(nines, true, nines_size);
  size_t nines_deseq_size = 0;
  std::unique_ptr<uint8[]> nines_deseq_buff =
      SerializeToBuffer(nines, false, nines_deseq_size);
  RBSPStream nines_stream(nines_buff.get(), nines_size);
  RBSPStream nines_deseq_stream(nines_deseq_buff.get(), nines_deseq_size);
  // iterate through streams, skipping in one and reading in the other, always
  // comparing values.
  for (int i = 0; i < 512; ++i) {
    if (i % 2) {
      ASSERT_TRUE(nines_stream.SkipBytes(1));
      uint8 bit = 0;
      ASSERT_TRUE(nines_stream.ReadBit(&bit));
      uint32 ninebits = 0;
      ASSERT_TRUE(nines_deseq_stream.ReadBits(9, &ninebits));
      ASSERT_EQ(ninebits, i);
      ASSERT_EQ(ninebits & 1, bit);
    } else {
      ASSERT_TRUE(nines_deseq_stream.SkipBytes(1));
      uint8 bit = 0;
      ASSERT_TRUE(nines_deseq_stream.ReadBit(&bit));
      uint32 ninebits = 0;
      ASSERT_TRUE(nines_stream.ReadBits(9, &ninebits));
      ASSERT_EQ(ninebits, i);
      ASSERT_EQ(ninebits & 1, bit);
    }
  }
  // 1 true bit followed by 1 byte with 1, followed by 1 true bit, then 2 bytes
  // with 2, followed by 1 bit, then 3 bytes with 3, etc up to 256
  std::list<bool> run_length;
  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < i; ++j) {
      for (int k = 7; k >= 0; --k) {
        run_length.push_back((i >> k) & 1);
      }
    }
    run_length.push_back(true);
  }
  size_t run_length_size = 0;
  std::unique_ptr<uint8[]> run_length_buff =
      SerializeToBuffer(run_length, true, run_length_size);
  size_t run_length_deseq_size = 0;
  std::unique_ptr<uint8[]> run_length_deseq_buff =
      SerializeToBuffer(run_length, false, run_length_deseq_size);
  RBSPStream run_length_stream(run_length_buff.get(), run_length_size);
  RBSPStream run_length_deseq_stream(run_length_deseq_buff.get(),
                                     run_length_deseq_size);
  // read first bit, skip first byte from each stream, read next bit
  uint8 bit = 0;
  ASSERT_TRUE(run_length_stream.ReadBit(&bit));
  ASSERT_EQ(bit, 1);
  bit = 0;
  ASSERT_TRUE(run_length_deseq_stream.ReadBit(&bit));
  ASSERT_EQ(bit, 1);
  ASSERT_TRUE(run_length_stream.SkipBytes(1));
  ASSERT_TRUE(run_length_deseq_stream.SkipBytes(1));
  bit = 0;
  ASSERT_TRUE(run_length_stream.ReadBit(&bit));
  ASSERT_EQ(bit, 1);
  bit = 0;
  ASSERT_TRUE(run_length_deseq_stream.ReadBit(&bit));
  ASSERT_EQ(bit, 1);

  for (int i = 2; i < 256; ++i) {
    // read first byte in seq stream, make sure it matches value
    uint8 byte = 0;
    ASSERT_TRUE(run_length_stream.ReadByte(&byte));
    ASSERT_EQ(byte, i);
    // skip the rest of the byte field
    ASSERT_TRUE(run_length_stream.SkipBytes(i - 1));
    bit = 0;
    // read the separating one bit
    ASSERT_TRUE(run_length_stream.ReadBit(&bit));
    ASSERT_EQ(bit, 1);
    // read last byte in deseq stream, so skip bytes first
    ASSERT_TRUE(run_length_deseq_stream.SkipBytes(i - 1));
    byte = 0;
    ASSERT_TRUE(run_length_deseq_stream.ReadByte(&byte));
    ASSERT_EQ(byte, i);
    // read the separating one bit
    bit = 0;
    ASSERT_TRUE(run_length_deseq_stream.ReadBit(&bit));
    ASSERT_EQ(bit, 1);
  }

  // further skips should fail
  ASSERT_FALSE(run_length_stream.SkipBytes(1));
  ASSERT_FALSE(run_length_deseq_stream.SkipBytes(1));
}

TEST_F(RBSPStreamTest, SkipBits) {
  std::list<bool> one_ohs;
  // encode one 1, followed by one zero, followed by 2 1s, followed by 2 zeros,
  // etc
  for (int i = 1; i <= 64; ++i) {
    for (int j = 0; j < i; ++j) {
      one_ohs.push_back(true);
    }
    for (int j = 0; j < i; ++j) {
      one_ohs.push_back(false);
    }
  }
  size_t skip_ones_size = 0;
  std::unique_ptr<uint8[]> skip_ones_buff =
      SerializeToBuffer(one_ohs, true, skip_ones_size);
  size_t skip_ohs_size = 0;
  std::unique_ptr<uint8[]> skip_ohs_buff =
      SerializeToBuffer(one_ohs, false, skip_ohs_size);
  RBSPStream skip_ones(skip_ones_buff.get(), skip_ones_size);
  RBSPStream skip_ohs(skip_ohs_buff.get(), skip_ohs_size);
  for (int i = 1; i < 64; ++i) {
    // skip the ones
    ASSERT_TRUE(skip_ones.SkipBits(i));
    // read the ones from the zeros stream
    for (int j = 0; j < i; ++j) {
      uint8 bit = 0;
      ASSERT_TRUE(skip_ohs.ReadBit(&bit));
      ASSERT_EQ(bit, 1);
    }
    // skip the ohs
    ASSERT_TRUE(skip_ohs.SkipBits(i));
    // read the ohs from the ones stream
    for (int j = 0; j < i; ++j) {
      uint8 bit = 0;
      ASSERT_TRUE(skip_ones.ReadBit(&bit));
      ASSERT_EQ(bit, 0);
    }
  }
}

}  // namespace media
}  // namespace cobalt
