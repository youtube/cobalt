// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/mesh/projection_codec/projection_decoder.h"

#include <zlib.h>

#include <cmath>
#include <cstring>
#include <ios>
#include <limits>
#include <memory>
#include <string>

#if defined(ENABLE_LOGGING)
#include "base/logging.h"
#endif  // #if defined(ENABLE_LOGGING)

#include "cobalt/loader/mesh/projection_codec/constants.h"
#include "cobalt/loader/mesh/projection_codec/indexed_vert.h"

namespace {  // anonymous namespace

inline int32_t ZigZagDecode32(uint32_t n) {
  return (n >> 1) ^ -static_cast<int32_t>(n & 1);
}

// This is a simple class that overloads operator<< to consume any parameter and
// do nothing.
class NopLogger {};
template <typename T>
NopLogger operator<<(NopLogger, const T&) {
  return NopLogger();
}

#if !defined(LOG)
#define LOG(level) NopLogger()
#endif  // #if !defined(LOG)

#if !defined(VLOG)
#define VLOG(level) NopLogger()
#endif  // #if !defined(VLOG)

#if !defined(DVLOG)
#define DVLOG(level) NopLogger()
#endif  // #if !defined(DVLOG)

}  // anonymous namespace

namespace cobalt {
namespace loader {
namespace mesh {
namespace projection_codec {

const uint32_t kErrorBits = 0xFFFFFFFF;
const uint8_t kErrorUint8 = 0xFF;
const uint32_t kErrorUint32 = 0xFFFFFFFF;
const float kErrorFloat = FP_NAN;
const char kError4cc[] = "ERR_";

const uint32_t ProjectionDecoder::kMaxCoordinateCount;
const uint32_t ProjectionDecoder::kMaxVertexCount;
const uint32_t ProjectionDecoder::kMaxMeshCount;
const uint32_t ProjectionDecoder::kMaxTriangleIndexCount;

ProjectionDecoder::Sink::~Sink() {}

ProjectionDecoder::BitReader::BitReader(const uint8_t* data, size_t data_size)
    : data_begin_(data),
      data_end_(data + data_size),
      cached_bits_(0),
      cached_bit_count_(0),
      ok_(true) {}

uint32_t ProjectionDecoder::BitReader::GetBits(int32_t num_bits) {
  if (!ok()) return kErrorBits;

  if (num_bits <= 0) {
    LOG(ERROR) << "Tried to read " << num_bits << " bits";
    ok_ = false;
    return kErrorBits;
  }
  if (num_bits > 32) {
    LOG(ERROR) << "Tried to read " << num_bits
               << " bits, only <= 32 is allowed";
    ok_ = false;
    return kErrorBits;
  }

  if (num_bits > bits_remaining()) {
    LOG(ERROR) << "Tried to read " << num_bits
               << " bits, but only <= " << bits_remaining() << " remain";
    ok_ = false;
    return kErrorBits;
  }

  while (cached_bit_count_ < num_bits) {
    uint8_t byte = *data_begin_;
    ++data_begin_;
    cached_bits_ |= static_cast<uint64_t>(byte) << (7 * 8 - cached_bit_count_);
    cached_bit_count_ += 8;
  }

  uint32_t value = static_cast<uint32_t>(cached_bits_ >> (64 - num_bits));

  cached_bits_ <<= num_bits;
  cached_bit_count_ -= num_bits;

  DVLOG(5) << "  Got " << num_bits << " bits as " << value << ",0x" << std::hex
           << value;

  return value;
}

void ProjectionDecoder::BitReader::Align() {
  int32_t discard_count = cached_bit_count_;

  cached_bits_ = 0;
  cached_bit_count_ = 0;

  DVLOG(5) << "  Aligned, discarded " << discard_count << " bits";
}

void ProjectionDecoder::BitReader::SkipBits(int64_t num_bits) {
  if (!ok()) return;

  if (num_bits < 0) {
    LOG(ERROR) << "Tried to skip " << num_bits << " bits";
    ok_ = false;
    return;
  }

  if (num_bits > bits_remaining()) {
    LOG(ERROR) << "Tried to skip more bits than remain";
    ok_ = false;
    return;
  }

  if (num_bits <= cached_bit_count_) {
    cached_bits_ <<= static_cast<uint64_t>(num_bits);
    cached_bit_count_ -= static_cast<int32_t>(num_bits);
    return;
  }

  num_bits -= cached_bit_count_;
  cached_bits_ = 0;
  cached_bit_count_ = 0;

  int64_t num_bytes = num_bits / 8;
  int32_t remainder_bits = static_cast<int32_t>(num_bits % 8);

  data_begin_ += num_bytes;
  if (remainder_bits > 0) {
    uint64_t byte = *data_begin_;
    ++data_begin_;
    cached_bits_ |= byte << static_cast<uint64_t>(7 * 8 + remainder_bits);
    cached_bit_count_ = 8 - remainder_bits;
  }
}

const uint8_t* ProjectionDecoder::BitReader::aligned_raw_bytes() const {
  return data_begin_;
}

int64_t ProjectionDecoder::BitReader::bits_remaining() const {
  if (!ok()) return 0;
  return static_cast<int64_t>(data_end_ - data_begin_) * 8 + cached_bit_count_;
}

bool ProjectionDecoder::BitReader::is_aligned() const {
  return cached_bit_count_ == 0;
}

bool ProjectionDecoder::BitReader::ok() const { return ok_; }

bool ProjectionDecoder::DecodeToSink(const uint8_t* data, size_t data_size,
                                     Sink* sink) {
  ProjectionDecoder mesh_decoder(data, data_size, sink);
  mesh_decoder.DecodeProjectionBox();
  return !mesh_decoder.error_;
}

bool ProjectionDecoder::DecodeBoxContentsToSink(uint8_t version, uint32_t flags,
                                                const uint8_t* data,
                                                size_t data_size, Sink* sink) {
  ProjectionDecoder mesh_decoder(data, data_size, sink);
  mesh_decoder.DecodeProjectionBoxContents(version, flags);
  return !mesh_decoder.error_;
}

bool ProjectionDecoder::DecodeMeshesToSink(uint8_t version, uint32_t flags,
                                           uint32_t crc,
                                           const std::string& compression,
                                           const uint8_t* data,
                                           size_t data_size, Sink* sink) {
  ProjectionDecoder mesh_decoder(data, data_size, sink);
  mesh_decoder.DecodeCompressedProjectionBoxContents(version, flags, crc,
                                                     compression);
  return !mesh_decoder.error_;
}

ProjectionDecoder::ProjectionDecoder(const uint8_t* data, size_t data_size,
                                     Sink* sink)
    : error_(false),
      sink_(sink),
      reader_(data, data_size),
      version_(0),
      flags_(0),
      crc_(0) {}

bool ProjectionDecoder::VerifyGreater(uint32_t a, uint32_t b) {
  if (a > b) {
    return true;
  }
  LOG(ERROR) << "Expected " << a << " > " << b;
  error_ = true;
  return false;
}

bool ProjectionDecoder::VerifyEqual(uint32_t a, uint32_t b) {
  if (a == b) {
    return true;
  }
  LOG(ERROR) << "Expected " << a << " == " << b;
  error_ = true;
  return false;
}

bool ProjectionDecoder::VerifyEqual(const std::string& a,
                                    const std::string& b) {
  if (a == b) {
    return true;
  }
  LOG(ERROR) << "Expected " << a << " == " << b;
  error_ = true;
  return false;
}

bool ProjectionDecoder::VerifyAligned() {
  if (!reader_.is_aligned()) {
    LOG(ERROR) << "Alignment error";
    error_ = true;
    return false;
  }
  return true;
}

uint32_t ProjectionDecoder::GetBits(int32_t num_bits) {
  if (error_) return kErrorBits;

  const uint32_t res = reader_.GetBits(num_bits);
  if (!reader_.ok()) {
    LOG(ERROR) << "Read error";
    error_ = true;
    return kErrorBits;
  }

  DVLOG(5) << "  Got " << num_bits << " bits as " << res << ",0x" << std::hex
           << res;

  return res;
}

uint8_t ProjectionDecoder::GetUInt8() {
  VerifyAligned();
  if (error_) return kErrorUint8;

  const uint8_t res = static_cast<uint8_t>(reader_.GetBits(8));
  if (!reader_.ok()) {
    LOG(ERROR) << "Read error";
    error_ = true;
    return kErrorUint8;
  }

  DVLOG(5) << "  Got uint8 as " << res << ",0x" << std::hex << res;

  return res;
}

uint32_t ProjectionDecoder::GetUInt32() {
  VerifyAligned();
  if (error_) return kErrorUint32;

  const uint32_t res = reader_.GetBits(32);
  if (!reader_.ok()) {
    LOG(ERROR) << "Read error";
    error_ = true;
    return kErrorUint32;
  }

  DVLOG(5) << "  Got uint32 as " << res << ",0x" << std::hex << res;

  return res;
}

float ProjectionDecoder::GetFloat() {
  VerifyAligned();
  if (error_) return kErrorFloat;

  // Read as a 32 bit uint, then reinterpret the memory as a float.
  const uint32_t res_as_bytes = reader_.GetBits(32);
  if (!reader_.ok()) {
    LOG(ERROR) << "Read error";
    error_ = true;
    return kErrorFloat;
  }

  float res;
  std::memcpy(&res, &res_as_bytes, 4);

  DVLOG(5) << "  Got float " << res;

  return res;
}

std::string ProjectionDecoder::GetFourCC() {
  VerifyAligned();
  if (error_) return kError4cc;

  // Read 4 individual bytes. Note that reading many is OK before checking for
  // errors, nothing bad will happen, though the bytes will be 0xFF.
  const char buf[] = {static_cast<char>(reader_.GetBits(8)),
                      static_cast<char>(reader_.GetBits(8)),
                      static_cast<char>(reader_.GetBits(8)),
                      static_cast<char>(reader_.GetBits(8)), 0};
  if (!reader_.ok()) {
    LOG(ERROR) << "Read error";
    error_ = true;
    return kError4cc;
  }

  std::string res(reinterpret_cast<const char*>(buf));

  DVLOG(5) << "  Got fourcc " << res;

  return res;
}

void ProjectionDecoder::SkipBits(int64_t num_bits) {
  if (error_) return;

  reader_.SkipBits(num_bits);
  if (!reader_.ok()) {
    LOG(ERROR) << "Failed to skip " << num_bits << " bits";
    error_ = true;
  }
}

void ProjectionDecoder::Align() {
  if (error_) return;
  reader_.Align();
}

void ProjectionDecoder::DecodeProjectionBox() {
  DVLOG(2) << "Getting projection box";

  // Read base box() header.
  uint32_t projection_box_size = GetUInt32();
  if (!VerifyGreater(projection_box_size, 20)) return;

  std::string box_name_4cc = GetFourCC();
  if (box_name_4cc != codec_strings::kMeshProjBoxType4cc &&
      box_name_4cc != codec_strings::kMeshProjAlternateBoxType4cc) {
    LOG(ERROR) << "Box name " << box_name_4cc << " is not "
               << codec_strings::kMeshProjBoxType4cc << " or "
               << codec_strings::kMeshProjAlternateBoxType4cc;
    error_ = true;
    return;
  }

  // Read full_box() header.
  uint8_t version = GetUInt8();
  uint32_t flags = GetBits(24);

  DecodeProjectionBoxContents(version, flags);
}

void ProjectionDecoder::DecodeProjectionBoxContents(uint8_t version,
                                                    uint32_t flags) {
  // mesh_projection_box() header.
  uint32_t crc = GetUInt32();
  std::string compression = GetFourCC();
  if (error_) return;

  DecodeCompressedProjectionBoxContents(version, flags, crc, compression);
}

void ProjectionDecoder::DecodeCompressedProjectionBoxContents(
    uint8_t version, uint32_t flags, uint32_t crc,
    const std::string& compression) {
  if (!VerifyEqual(version, 0)) return;
  if (!VerifyEqual(flags, 0)) return;

  version_ = version;
  flags_ = flags;

  crc_ = crc;
  if (sink_->IsCached(crc_)) return;

  compression_ = compression;
  if (compression_ == codec_strings::kCompressionRaw4cc) {
    // All good.
  } else if (compression_ == codec_strings::kCompressionDeflate4cc) {
    DeflateDecompress();
  } else {
    LOG(ERROR) << "Invalid compression format " << compression_;
    error_ = true;
  }

  while (!error_ && reader_.bits_remaining() > 0) {
    // Read the box.
    int64_t beginning_bits_remaining = reader_.bits_remaining();
    uint32_t box_size = GetUInt32();
    std::string box_name_4cc = GetFourCC();

    if (box_size < 8 ||
        static_cast<int64_t>(box_size) > beginning_bits_remaining / 8) {
      LOG(ERROR) << "Invalid box size " << box_size;
      error_ = true;
      return;
    }

    if (box_name_4cc == codec_strings::kMeshBoxType4cc) {
      DecodeMeshData();
    }

    // Make sure we didn't overflow the box's bounds.
    int64_t bits_read = beginning_bits_remaining - reader_.bits_remaining();
    if (static_cast<int64_t>(box_size) * 8 < bits_read) {
      LOG(ERROR) << "Box overflowed its bounds";
      error_ = true;
      return;
    } else if (!reader_.is_aligned()) {
      LOG(ERROR) << "Reader finished box in a misaligned state";
      error_ = true;
      return;
    }

    SkipBits(static_cast<int64_t>(box_size) * 8 - bits_read);
  }
}

void ProjectionDecoder::DeflateDecompress() {
  VLOG(1) << "Decompressing DEFLATE";
  if (!reader_.is_aligned()) {
    LOG(ERROR) << "Data is not 8-bit aligned";
    error_ = true;
    return;
  }

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
  size_t size = static_cast<size_t>(reader_.bits_remaining() / 8);
  if (size > std::numeric_limits<uInt>::max()) {
    LOG(ERROR) << "Data is bigger than zlib's uInt max";
    error_ = true;
    return;
  }
#if __clang__
#pragma clang diagnostic pop
#endif

  z_stream stream = {0};
  int status = inflateInit2(&stream, -MAX_WBITS);
  if (status != Z_OK) {
    LOG(ERROR) << "zlib inflateInit2 error: " << status;
    error_ = true;
    return;
  }

  stream.avail_in = static_cast<uInt>(size);
  stream.next_in = const_cast<Bytef*>(reader_.aligned_raw_bytes());

  do {
    Bytef buf[1024];
    stream.avail_out = sizeof(buf);
    stream.next_out = buf;
    status = inflate(&stream, Z_NO_FLUSH);
    if (status != Z_STREAM_END && (status != Z_OK || stream.avail_out != 0)) {
      LOG(ERROR) << "zlib inflate error: " << status;
      error_ = true;
      inflateEnd(&stream);
      return;
    }
    decompress_buffer_.insert(decompress_buffer_.end(), buf,
                              buf + (sizeof(buf) - stream.avail_out));
  } while (status != Z_STREAM_END);

  status = inflateEnd(&stream);
  if (status != Z_OK) {
    LOG(ERROR) << "zlib inflateEnd error: " << status;
    error_ = true;
    return;
  }

  // Note that resetting the bit reader resets its position. This is okay here,
  // because we aren't tracking the reader's position or bit count before/after
  // decompression.
  size_t buffer_size = decompress_buffer_.size();
  reader_ = BitReader(buffer_size ? &decompress_buffer_[0] : NULL,
                      decompress_buffer_.size());
}

void ProjectionDecoder::DecodeMeshData() {
  if (error_) return;

  sink_->BeginMeshCollection();

  // Read coordinate list.
  DVLOG(2) << "Getting coordinates";
  uint32_t num_coordinates = GetUInt32();
  if (num_coordinates > kMaxCoordinateCount) {
    LOG(ERROR) << "Coordinate count of " << num_coordinates
               << " exceeds maximum of " << kMaxCoordinateCount;
    error_ = true;
    return;
  }
  if (num_coordinates == 0) {
    LOG(ERROR) << "Too few coordinates";
    error_ = true;
    return;
  }

  for (uint32_t i = 0; i < num_coordinates; ++i) {
    sink_->AddCoordinate(GetFloat());
  }
  if (error_) return;

  // Read vertex list. Note that the verts are recorded as zig-zag deltas
  // so we need to track the previous vertex's indices.
  DVLOG(2) << "Getting vertex indices";
  uint32_t num_verts = GetUInt32();
  if (num_verts > kMaxVertexCount) {
    LOG(ERROR) << "Vertex count of " << num_verts << " exceeds maximum of "
               << kMaxVertexCount;
    error_ = true;
    return;
  }
  if (num_verts == 0) {
    LOG(ERROR) << "Too few vertices";
    error_ = true;
    return;
  }

  int bits_per_coordinate_index =
      static_cast<int>(ceil(log2(static_cast<double>(num_coordinates * 2))));
  IndexedVert vert;  // Initialized to 0's.
  for (uint32_t i = 0; i < num_verts; ++i) {
    IndexedVert delta;
    delta.x = ZigZagDecode32(GetBits(bits_per_coordinate_index));
    delta.y = ZigZagDecode32(GetBits(bits_per_coordinate_index));
    delta.z = ZigZagDecode32(GetBits(bits_per_coordinate_index));
    delta.u = ZigZagDecode32(GetBits(bits_per_coordinate_index));
    delta.v = ZigZagDecode32(GetBits(bits_per_coordinate_index));

    vert += delta;

    // Silence compiler warning of signed/unsigned mismatch.
    int32_t signed_num_coordinates = static_cast<int32_t>(num_coordinates);
    // Check that the vertex refers to valid coordinates.
    if (vert.x < 0 || vert.x >= signed_num_coordinates ||  //
        vert.y < 0 || vert.y >= signed_num_coordinates ||  //
        vert.z < 0 || vert.z >= signed_num_coordinates ||  //
        vert.u < 0 || vert.u >= signed_num_coordinates ||  //
        vert.v < 0 || vert.v >= signed_num_coordinates) {
      LOG(ERROR) << "Coordinate index OOB";
      error_ = true;
      return;
    }
    sink_->AddVertex(vert);
  }
  Align();  // Pad.
  if (error_) return;

  // Read mesh instance count.
  DVLOG(2) << "Getting mesh instance count";
  uint32_t mesh_instance_count = GetUInt32();
  if (mesh_instance_count > kMaxMeshCount) {
    LOG(ERROR) << "Mesh instance count of " << mesh_instance_count
               << " exceeds maximum of " << kMaxMeshCount;
    error_ = true;
    return;
  }

  for (uint32_t i = 0; i < mesh_instance_count; ++i) {
    sink_->BeginMeshInstance();

    sink_->SetTextureId(GetUInt8());

    // Read mesh geometry type.
    DVLOG(2) << "Getting mesh geometry type";
    uint8_t geometryType = GetUInt8();
    switch (geometryType) {
      case 0:
        sink_->SetMeshGeometryType(kTriangles);
        break;
      case 1:
        sink_->SetMeshGeometryType(kTriangleStrip);
        break;
      case 2:
        sink_->SetMeshGeometryType(kTriangleFan);
        break;
      default:
        LOG(ERROR) << "Unknown mesh geometry type " << geometryType;
        error_ = true;
        return;
    }

    // Read triangle vertex indices.
    DVLOG(2) << "Getting triangle vertex indices";
    uint32_t num_indices = GetUInt32();
    if (num_indices > kMaxTriangleIndexCount) {
      LOG(ERROR) << "Vertex indices count of " << num_indices
                 << " exceeds the maximum of " << kMaxTriangleIndexCount;
      error_ = true;
      return;
    }

    int v_index = 0;
    int bits_per_vertex_index =
        static_cast<int>(ceil(log2(static_cast<double>(num_verts * 2))));
    for (uint32_t j = 0; j < num_indices; ++j) {
      v_index += ZigZagDecode32(GetBits(bits_per_vertex_index));

      // Check that this vertex index is in range before sending it off.
      if (v_index < 0 || v_index >= static_cast<int32_t>(num_verts)) {
        LOG(ERROR) << "Triangle vertex index " << v_index << " is OOB";
        error_ = true;
        return;
      }

      sink_->AddVertexIndex(v_index);
    }
    Align();  // Pad.
    sink_->EndMeshInstance();

    if (error_) return;
  }
  sink_->EndMeshCollection();
}

}  // namespace projection_codec
}  // namespace mesh
}  // namespace loader
}  // namespace cobalt
