/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_TOKENIZER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_TOKENIZER_H_

<<<<<<< HEAD
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/protozero/field.h"
#include "perfetto/protozero/proto_utils.h"
#include "perfetto/public/compiler.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "src/trace_processor/util/gzip_utils.h"

#include "protos/perfetto/trace/trace.pbzero.h"
#include "src/trace_processor/util/status_macros.h"
#include "src/trace_processor/util/trace_blob_view_reader.h"

namespace perfetto::trace_processor {

// Reads a protobuf trace in chunks and extracts boundaries of trace packets
// with their timestamps.
=======
#include <cstdint>
#include <vector>

#include "perfetto/base/status.h"
#include "perfetto/protozero/proto_utils.h"
#include "perfetto/public/compiler.h"
#include "perfetto/trace_processor/status.h"
#include "perfetto/trace_processor/trace_blob.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "src/trace_processor/util/gzip_utils.h"
#include "src/trace_processor/util/status_macros.h"

#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

// Reads a protobuf trace in chunks and extracts boundaries of trace packets
// (or subfields, for the case of ftrace) with their timestamps.
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
class ProtoTraceTokenizer {
 public:
  ProtoTraceTokenizer();

<<<<<<< HEAD
  template <typename Callback = base::Status(TraceBlobView)>
  base::Status Tokenize(TraceBlobView tbv, Callback callback) {
    reader_.PushBack(std::move(tbv));

    for (;;) {
      size_t start_offset = reader_.start_offset();
      size_t avail = reader_.avail();

      // The header must be at least 2 bytes (1 byte for tag, 1 byte for
      // size/varint) and can be at most 20 bytes (10 bytes for tag + 10 bytes
      // for size/varint).
      const size_t kMinHeaderBytes = 2;
      const size_t kMaxHeaderBytes = 20;
      std::optional<TraceBlobView> header = reader_.SliceOff(
          start_offset,
          std::min(std::max(avail, kMinHeaderBytes), kMaxHeaderBytes));

      // This means that kMinHeaderBytes was not available. Just wait for the
      // next round.
      if (PERFETTO_UNLIKELY(!header)) {
        return base::OkStatus();
      }

      uint64_t tag;
      const uint8_t* tag_start = header->data();
      const uint8_t* tag_end = protozero::proto_utils::ParseVarInt(
          tag_start, header->data() + header->size(), &tag);

      if (PERFETTO_UNLIKELY(tag_end == tag_start)) {
        return header->size() < kMaxHeaderBytes
                   ? base::OkStatus()
                   : base::ErrStatus("Failed to parse tag");
      }

      if (PERFETTO_UNLIKELY(tag != kTracePacketTag)) {
        // Other field. Skip.
        auto field_type = static_cast<uint8_t>(tag & 0b111);
        switch (field_type) {
          case static_cast<uint8_t>(
              protozero::proto_utils::ProtoWireType::kVarInt): {
            uint64_t varint;
            const uint8_t* varint_start = tag_end;
            const uint8_t* varint_end = protozero::proto_utils::ParseVarInt(
                tag_end, header->data() + header->size(), &varint);
            if (PERFETTO_UNLIKELY(varint_end == varint_start)) {
              return header->size() < kMaxHeaderBytes
                         ? base::OkStatus()
                         : base::ErrStatus("Failed to skip varint");
            }
            PERFETTO_CHECK(reader_.PopFrontBytes(
                static_cast<size_t>(varint_end - tag_start)));
            continue;
          }
          case static_cast<uint8_t>(
              protozero::proto_utils::ProtoWireType::kLengthDelimited): {
            uint64_t varint;
            const uint8_t* varint_start = tag_end;
            const uint8_t* varint_end = protozero::proto_utils::ParseVarInt(
                tag_end, header->data() + header->size(), &varint);
            if (PERFETTO_UNLIKELY(varint_end == varint_start)) {
              return header->size() < kMaxHeaderBytes
                         ? base::OkStatus()
                         : base::ErrStatus("Failed to skip delimited");
            }

            size_t size_incl_header =
                static_cast<size_t>(varint_end - tag_start) + varint;
            if (size_incl_header > avail) {
              return base::OkStatus();
            }
            PERFETTO_CHECK(reader_.PopFrontBytes(size_incl_header));
            continue;
          }
          case static_cast<uint8_t>(
              protozero::proto_utils::ProtoWireType::kFixed64): {
            size_t size_incl_header =
                static_cast<size_t>(tag_end - tag_start) + 8;
            if (size_incl_header > avail) {
              return base::OkStatus();
            }
            PERFETTO_CHECK(reader_.PopFrontBytes(size_incl_header));
            continue;
          }
          case static_cast<uint8_t>(
              protozero::proto_utils::ProtoWireType::kFixed32): {
            size_t size_incl_header =
                static_cast<size_t>(tag_end - tag_start) + 4;
            if (size_incl_header > avail) {
              return base::OkStatus();
            }
            PERFETTO_CHECK(reader_.PopFrontBytes(size_incl_header));
            continue;
          }
          default:
            return base::ErrStatus("Unknown field type");
        }
      }

      uint64_t field_size;
      const uint8_t* size_start = tag_end;
      const uint8_t* size_end = protozero::proto_utils::ParseVarInt(
          size_start, header->data() + header->size(), &field_size);

      // If we had less than the maximum number of header bytes, it's possible
      // that we just need more to actually parse. Otherwise, this is an error.
      if (PERFETTO_UNLIKELY(size_start == size_end)) {
        return header->size() < kMaxHeaderBytes
                   ? base::OkStatus()
                   : base::ErrStatus("Failed to parse TracePacket size");
      }

      // Empty packets can legitimately happen if the producer ends up emitting
      // no data: just ignore them.
      auto hdr_size = static_cast<size_t>(size_end - header->data());
      if (PERFETTO_UNLIKELY(field_size == 0)) {
        PERFETTO_CHECK(reader_.PopFrontBytes(hdr_size));
        continue;
      }

      // If there's not enough bytes in the reader, then we cannot do anymore.
      size_t size_incl_header = hdr_size + field_size;
      if (size_incl_header > avail) {
        return base::OkStatus();
      }

      auto packet = reader_.SliceOff(start_offset + hdr_size, field_size);
      PERFETTO_CHECK(packet);
      PERFETTO_CHECK(reader_.PopFrontBytes(hdr_size + field_size));
      protos::pbzero::TracePacket::Decoder decoder(packet->data(),
                                                   packet->length());
      if (!decoder.has_compressed_packets()) {
        RETURN_IF_ERROR(callback(std::move(*packet)));
        continue;
      }

      if (!util::IsGzipSupported()) {
        return base::ErrStatus(
            "Cannot decode compressed packets. Zlib not enabled");
      }

      protozero::ConstBytes field = decoder.compressed_packets();
      TraceBlobView compressed_packets = packet->slice(field.data, field.size);
      TraceBlobView packets;
      RETURN_IF_ERROR(Decompress(std::move(compressed_packets), &packets));

      const uint8_t* start = packets.data();
      const uint8_t* end = packets.data() + packets.length();
      const uint8_t* ptr = start;
      while ((end - ptr) > 2) {
        const uint8_t* packet_outer = ptr;
        if (PERFETTO_UNLIKELY(*ptr != kTracePacketTag)) {
          return base::ErrStatus("Expected TracePacket tag");
        }
        uint64_t packet_size = 0;
        ptr = protozero::proto_utils::ParseVarInt(++ptr, end, &packet_size);
        const uint8_t* packet_start = ptr;
        ptr += packet_size;
        if (PERFETTO_UNLIKELY((ptr - packet_outer) < 2 || ptr > end)) {
          return base::ErrStatus("Invalid packet size");
        }
        TraceBlobView sliced =
            packets.slice(packet_start, static_cast<size_t>(packet_size));
        RETURN_IF_ERROR(callback(std::move(sliced)));
      }
    }
=======
  template <typename Callback = util::Status(TraceBlobView)>
  util::Status Tokenize(TraceBlobView blob, Callback callback) {
    const uint8_t* data = blob.data();
    size_t size = blob.size();
    if (!partial_buf_.empty()) {
      // It takes ~5 bytes for a proto preamble + the varint size.
      const size_t kHeaderBytes = 5;
      if (PERFETTO_UNLIKELY(partial_buf_.size() < kHeaderBytes)) {
        size_t missing_len = std::min(kHeaderBytes - partial_buf_.size(), size);
        partial_buf_.insert(partial_buf_.end(), &data[0], &data[missing_len]);
        if (partial_buf_.size() < kHeaderBytes)
          return util::OkStatus();
        data += missing_len;
        size -= missing_len;
      }

      // At this point we have enough data in |partial_buf_| to read at least
      // the field header and know the size of the next TracePacket.
      const uint8_t* pos = &partial_buf_[0];
      uint8_t proto_field_tag = *pos;
      uint64_t field_size = 0;
      // We cannot do &partial_buf_[partial_buf_.size()] because that crashes
      // on MSVC STL debug builds, so does &*partial_buf_.end().
      const uint8_t* next = protozero::proto_utils::ParseVarInt(
          ++pos, &partial_buf_.front() + partial_buf_.size(), &field_size);
      bool parse_failed = next == pos;
      pos = next;
      if (proto_field_tag != kTracePacketTag || field_size == 0 ||
          parse_failed) {
        return util::ErrStatus(
            "Failed parsing a TracePacket from the partial buffer");
      }

      // At this point we know how big the TracePacket is.
      size_t hdr_size = static_cast<size_t>(pos - &partial_buf_[0]);
      size_t size_incl_header = static_cast<size_t>(field_size + hdr_size);
      PERFETTO_DCHECK(size_incl_header > partial_buf_.size());

      // There is a good chance that between the |partial_buf_| and the new
      // |data| of the current call we have enough bytes to parse a TracePacket.
      if (partial_buf_.size() + size >= size_incl_header) {
        // Create a new buffer for the whole TracePacket and copy into that:
        // 1) The beginning of the TracePacket (including the proto header) from
        //    the partial buffer.
        // 2) The rest of the TracePacket from the current |data| buffer (note
        //    that we might have consumed already a few bytes form |data|
        //    earlier in this function, hence we need to keep |off| into
        //    account).
        TraceBlob glued = TraceBlob::Allocate(size_incl_header);
        memcpy(glued.data(), partial_buf_.data(), partial_buf_.size());
        // |size_missing| is the number of bytes for the rest of the TracePacket
        // in |data|.
        size_t size_missing = size_incl_header - partial_buf_.size();
        memcpy(glued.data() + partial_buf_.size(), &data[0], size_missing);
        data += size_missing;
        size -= size_missing;
        partial_buf_.clear();
        RETURN_IF_ERROR(
            ParseInternal(TraceBlobView(std::move(glued)), callback));
      } else {
        partial_buf_.insert(partial_buf_.end(), data, &data[size]);
        return util::OkStatus();
      }
    }
    return ParseInternal(blob.slice(data, size), callback);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

 private:
  static constexpr uint8_t kTracePacketTag =
      protozero::proto_utils::MakeTagLengthDelimited(
          protos::pbzero::Trace::kPacketFieldNumber);

<<<<<<< HEAD
  base::Status Decompress(TraceBlobView input, TraceBlobView* output);

  // Used to glue together trace packets that span across two (or more)
  // Parse() boundaries.
  util::TraceBlobViewReader reader_;
=======
  template <typename Callback = util::Status(TraceBlobView)>
  util::Status ParseInternal(TraceBlobView whole_buf, Callback callback) {
    static constexpr auto kLengthDelimited =
        protozero::proto_utils::ProtoWireType::kLengthDelimited;
    const uint8_t* const start = whole_buf.data();
    protos::pbzero::Trace::Decoder decoder(whole_buf.data(), whole_buf.size());
    for (auto it = decoder.packet(); it; ++it) {
      if (PERFETTO_UNLIKELY(it->type() != kLengthDelimited)) {
        return base::ErrStatus("Failed to parse TracePacket bounds");
      }
      protozero::ConstBytes packet = *it;
      TraceBlobView sliced = whole_buf.slice(packet.data, packet.size);
      RETURN_IF_ERROR(ParsePacket(std::move(sliced), callback));
    }

    const size_t bytes_left = decoder.bytes_left();
    if (bytes_left > 0) {
      PERFETTO_DCHECK(partial_buf_.empty());
      partial_buf_.insert(partial_buf_.end(), &start[decoder.read_offset()],
                          &start[decoder.read_offset() + bytes_left]);
    }
    return util::OkStatus();
  }

  template <typename Callback = util::Status(TraceBlobView)>
  util::Status ParsePacket(TraceBlobView packet, Callback callback) {
    protos::pbzero::TracePacket::Decoder decoder(packet.data(),
                                                 packet.length());
    if (decoder.has_compressed_packets()) {
      if (!util::IsGzipSupported()) {
        return util::Status(
            "Cannot decode compressed packets. Zlib not enabled");
      }

      protozero::ConstBytes field = decoder.compressed_packets();
      TraceBlobView compressed_packets = packet.slice(field.data, field.size);
      TraceBlobView packets;

      RETURN_IF_ERROR(Decompress(std::move(compressed_packets), &packets));

      const uint8_t* start = packets.data();
      const uint8_t* end = packets.data() + packets.length();
      const uint8_t* ptr = start;
      while ((end - ptr) > 2) {
        const uint8_t* packet_outer = ptr;
        if (PERFETTO_UNLIKELY(*ptr != kTracePacketTag))
          return util::ErrStatus("Expected TracePacket tag");
        uint64_t packet_size = 0;
        ptr = protozero::proto_utils::ParseVarInt(++ptr, end, &packet_size);
        const uint8_t* packet_start = ptr;
        ptr += packet_size;
        if (PERFETTO_UNLIKELY((ptr - packet_outer) < 2 || ptr > end))
          return util::ErrStatus("Invalid packet size");

        TraceBlobView sliced =
            packets.slice(packet_start, static_cast<size_t>(packet_size));
        RETURN_IF_ERROR(ParsePacket(std::move(sliced), callback));
      }
      return util::OkStatus();
    }
    return callback(std::move(packet));
  }

  util::Status Decompress(TraceBlobView input, TraceBlobView* output);

  // Used to glue together trace packets that span across two (or more)
  // Parse() boundaries.
  std::vector<uint8_t> partial_buf_;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Allows support for compressed trace packets.
  util::GzipDecompressor decompressor_;
};

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_TRACE_TOKENIZER_H_
