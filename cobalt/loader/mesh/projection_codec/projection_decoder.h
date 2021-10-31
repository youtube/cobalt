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

#ifndef COBALT_LOADER_MESH_PROJECTION_CODEC_PROJECTION_DECODER_H_
#define COBALT_LOADER_MESH_PROJECTION_CODEC_PROJECTION_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/loader/mesh/projection_codec/constants.h"
#include "cobalt/loader/mesh/projection_codec/indexed_vert.h"

namespace cobalt {
namespace loader {
namespace mesh {
namespace projection_codec {

class ProjectionDecoder {
 public:
  // We need limits to keep us from having crazy large meshes that eat up all of
  // our RAM and take down services. The values below are chosen to be large
  // enough for everything we see on the horizon, but if future valid meshes
  // need bigger numbers they can certainly be adjusted.
  static const uint32_t kMaxCoordinateCount = 10 * 1000;
  static const uint32_t kMaxVertexCount = 32 * 1000;
  static const uint32_t kMaxMeshCount = 32;
  static const uint32_t kMaxTriangleIndexCount = 128 * 1000;

  // This is a callback handler interface for a ProjectionDecoder.
  class Sink {
   public:
    virtual ~Sink();

    // These are the callbacks, with calling pattern like:
    //
    // BeginMeshCollection
    //   AddCoordinate
    //   ...
    //   AddVertex
    //   ...
    //   BeginMeshInstance
    //     SetMeshGeometryType
    //     AddVertexIndex
    //     ...
    //   EndMeshInstance
    // EndMeshCollection

    // Give the Sink an opportunity to check the CRC and decide whether it
    // is known already and skip the decoding. This helps with memory, cpu, and
    // streaming cost.
    virtual bool IsCached(uint32_t crc) = 0;

    virtual void BeginMeshCollection() = 0;
    virtual void AddCoordinate(float value) = 0;
    virtual void AddVertex(const IndexedVert& v) = 0;

    virtual void BeginMeshInstance() = 0;
    virtual void SetTextureId(uint8_t texture_id) = 0;
    virtual void SetMeshGeometryType(MeshGeometryType type) = 0;
    virtual void AddVertexIndex(size_t v_index) = 0;
    virtual void EndMeshInstance() = 0;

    virtual void EndMeshCollection() = 0;
  };

  // Decodes a full projection box that includes the full box header.
  static bool DecodeToSink(const uint8_t* data, size_t data_size, Sink* sink);

  // Decodes the contents of a projection box without the full box header.
  // |version| and |flags| are the values from the full box header provided by
  // an external parser. |data| contains all box contents after the full box
  // header.
  static bool DecodeBoxContentsToSink(uint8_t version, uint32_t flags,
                                      const uint8_t* data, size_t data_size,
                                      Sink* sink);

  // Decodes the contents of a projection box without the full box header or
  // CRC-32 or compression FourCC. |version|, |flags|, |crc|, and |compression|
  // are the values the partially parsed MeshProjectionBox. |data| contains all
  // the remaining box contents after compression FourCC.
  static bool DecodeMeshesToSink(uint8_t version, uint32_t flags, uint32_t crc,
                                 const std::string& compression,
                                 const uint8_t* data, size_t data_size,
                                 Sink* sink);

 private:
  class BitReader {
   public:
    BitReader(const uint8_t* data, size_t data_size);

    // Reads the requested number of big-endian bits. Sets ok() to false if an
    // error occurred.
    uint32_t GetBits(int32_t num_bits);

    // Aligns the reader to an 8-bit boundary, potentially discarding up to 7
    // bits.
    void Align();

    // Discards the requested number of bits and advances the reader forward.
    void SkipBits(int64_t num_bits);

    // Gets a pointer to the underlying raw buffer of aligned bytes. Exactly
    // bits_remaining() / 8 bytes are available and may be accessed.
    const uint8_t* aligned_raw_bytes() const;

    // Gets the number of bits available in the buffer. If ok() is false, this
    // will be zero.
    int64_t bits_remaining() const;

    // Returns true if the reader is aligned on an 8-bit boundary.
    bool is_aligned() const;

    // Returns true if the reader is in a good state and everything is okay.
    bool ok() const;

   private:
    const uint8_t* data_begin_;  // Unowned.
    const uint8_t* data_end_;    // Unowned.

    // Cached bits, stored as big-endian. 7 or fewer bits may be cached between
    // method calls.
    uint64_t cached_bits_;

    // The number of cached bits in cached_bits_. Only fewer than 8 bits should
    // ever be cached between method calls.
    int32_t cached_bit_count_;

    bool ok_;
  };

  bool error_;
  Sink* sink_;  // Unowned.
  BitReader reader_;
  uint8_t version_;
  uint32_t flags_;
  uint32_t crc_;
  std::string compression_;
  std::vector<uint8_t> decompress_buffer_;

  ProjectionDecoder(const uint8_t* data, size_t data_size, Sink* sink);

  bool VerifyGreater(uint32_t a, uint32_t b);
  bool VerifyEqual(uint32_t a, uint32_t b);
  bool VerifyEqual(const std::string& a, const std::string& b);
  bool VerifyAligned();

  uint32_t GetBits(int32_t num_bits);
  uint8_t GetUInt8();
  uint32_t GetUInt32();
  float GetFloat();
  std::string GetFourCC();
  void SkipBits(int64_t num_bits);
  void Align();
  void CheckOK();

  void DecodeProjectionBox();
  void DecodeProjectionBoxContents(uint8_t version, uint32_t flags);
  void DecodeCompressedProjectionBoxContents(uint8_t version, uint32_t flags,
                                             uint32_t crc,
                                             const std::string& compression);
  void DecodeMeshData();
  void DeflateDecompress();

  DISALLOW_COPY_AND_ASSIGN(ProjectionDecoder);
};

}  // namespace projection_codec
}  // namespace mesh
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_MESH_PROJECTION_CODEC_PROJECTION_DECODER_H_
