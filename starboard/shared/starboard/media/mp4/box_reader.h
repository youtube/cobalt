// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MP4_BOX_READER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MP4_BOX_READER_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/span.h"

// Copied from media/formats/mp4/box_reader.h, with the parts of
// media/formats/mp4/{fourccs,parse_result,rcheck}.h that it uses, reduced to
// what ValidatePsshInput() needs and ported to C++17 without //base and
// //media.

namespace starboard {

enum FourCC {
  FOURCC_NULL = 0,
  FOURCC_PSSH = 0x70737368,
};

enum class ParseResult {
  kOk,            // Parsing was successful.
  kError,         // The data is invalid (usually unrecoverable).
  kNeedMoreData,  // More data is required to parse successfully.
};

// Evaluate |condition| once. If the result is false, (early) return false from
// the containing function.
#define RCHECK(condition)                                         \
  do {                                                            \
    if (!(condition)) {                                           \
      SB_DLOG(ERROR) << "Failure while parsing MP4: " #condition; \
      return false;                                               \
    }                                                             \
  } while (0)

class BoxReader;

struct Box {
  virtual ~Box();

  virtual bool Parse(BoxReader* reader) = 0;

  virtual FourCC BoxType() const = 0;
};

class BufferReader {
 public:
  BufferReader(const uint8_t* buf, const size_t buf_size);
  BufferReader(const BufferReader& other);
  virtual ~BufferReader();
  bool HasBytes(size_t count) {
    // As the size of a box is implementation limited to 2^31, fail if
    // attempting to check for too many bytes.
    constexpr size_t kImplLimit =
        static_cast<size_t>(std::numeric_limits<int32_t>::max());
    return pos_ <= buf_.size() && count <= kImplLimit &&
           count <= buf_.size() - pos_;
  }

  // Read a value from the stream, performing endian correction, and advance the
  // stream pointer.
  //
  // MPEG-4 uses big endian byte order, so these convert from big endian order.
  [[nodiscard]] bool Read4(uint32_t* v);
  [[nodiscard]] bool Read8(uint64_t* v);
  [[nodiscard]] bool ReadFourCC(FourCC* v);

  // This variant reads a 4-byte integer and stores it in the 8-byte return
  // type.
  [[nodiscard]] bool Read4Into8(uint64_t* v);

  // Reads a sequence of bytes verbatim from the buffer into `t` after clearing
  // `t`, and advances the stream pointer.
  [[nodiscard]] bool ReadVec(std::vector<uint8_t>* t, size_t count);

  // Returns the full buffer. This size may not match the size specified in the
  // mp4 box header and could be less than the box size when the full box has
  // not been appended.
  const span<const uint8_t> buffer() const { return buf_; }

  // The current position in the buffer given by `buffer()`.
  size_t pos() const { return pos_; }

 protected:
  span<const uint8_t> buf_;
  // The position in `buf_` where the next read is from.
  size_t pos_ = 0u;
};

class BoxReader : public BufferReader {
 public:
  BoxReader(const BoxReader& other);
  ~BoxReader() override;

  // Create a BoxReader from a buffer. |buf| must be the complete buffer, as
  // errors are returned when sufficient data is not available. |buf| can start
  // with any type of box.
  //
  // |buf| is retained but not owned, and must outlive the BoxReader instance.
  static BoxReader* ReadConcatentatedBoxes(const uint8_t* buf,
                                           const size_t buf_size);

  // Read all children and verify that the FourCC matches what is expected.
  // Returns true if all children are successfully parsed and have the correct
  // box type for |T|.
  template <typename T>
  [[nodiscard]] bool ReadAllChildrenAndCheckFourCC(std::vector<T>* children);

  // Populate the values of 'version()' and 'flags()' from a full box header.
  // Many boxes, but not all, use these values. This call should happen after
  // the box has been initialized, and does not re-read the main box header.
  [[nodiscard]] bool ReadFullBoxHeader();

  size_t box_size() const {
    SB_DCHECK(box_size_known_);
    return box_size_;
  }

  FourCC type() const { return type_; }
  uint8_t version() const { return version_; }
  uint32_t flags() const { return flags_; }

 private:
  // Create a BoxReader from |buf|. |is_EOS| should be true if |buf| is
  // complete stream (i.e. no additional data is expected to be appended).
  BoxReader(const uint8_t* buf, const size_t buf_size, bool is_EOS);

  // Must be called immediately after init.
  [[nodiscard]] ParseResult ReadHeader();

  // Read all children, optionally checking FourCC. Returns true if all
  // children are successfully parsed and, if |check_box_type|, have the
  // correct box type for |T|.
  template <typename T>
  bool ReadAllChildrenInternal(std::vector<T>* children, bool check_box_type);

  size_t box_size_;
  bool box_size_known_;
  FourCC type_;
  uint8_t version_;
  uint32_t flags_;
  bool scanned_;

  // True if the buffer provided to the reader is the complete stream.
  const bool is_EOS_;
};

// Template definitions
template <typename T>
bool BoxReader::ReadAllChildrenAndCheckFourCC(std::vector<T>* children) {
  return ReadAllChildrenInternal(children, true);
}

template <typename T>
bool BoxReader::ReadAllChildrenInternal(std::vector<T>* children,
                                        bool check_box_type) {
  SB_DCHECK(!scanned_);
  scanned_ = true;

  // Must know our box size before attempting to parse child boxes.
  RCHECK(box_size_known_);

  SB_DCHECK_LE(pos_, box_size_);
  while (pos_ < box_size_) {
    BoxReader child_reader(&buf_[pos_], box_size_ - pos_, is_EOS_);

    if (child_reader.ReadHeader() != ParseResult::kOk) {
      return false;
    }

    T child;
    RCHECK(!check_box_type || child_reader.type() == child.BoxType());
    RCHECK(child.Parse(&child_reader));
    children->push_back(child);
    pos_ += child_reader.box_size();
  }
  SB_DCHECK_EQ(pos_, box_size_);
  return true;
}

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MP4_BOX_READER_H_
