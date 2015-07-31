// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_H_

#if defined(__LB_SHELL__)
#include "media/base/shell_buffer_factory.h"
#else
#include "media/base/decoder_buffer.h"
#endif
#include "media/base/media_export.h"

namespace media {

#if defined(__LB_SHELL__)
class ShellFilterGraphLog;

class MEDIA_EXPORT StreamParserBuffer : public ShellBuffer {
#else
class MEDIA_EXPORT StreamParserBuffer : public DecoderBuffer {
#endif
 public:
  // Value used to signal an invalid decoder config ID.
  enum { kInvalidConfigId = -1 };

#if defined(__LB_SHELL__)
  static scoped_refptr<StreamParserBuffer> CreateEOSBuffer(
      scoped_refptr<ShellFilterGraphLog> filter_graph_log);
  static scoped_refptr<StreamParserBuffer> CopyFrom(
      const uint8* data, int data_size, bool is_keyframe,
      scoped_refptr<ShellFilterGraphLog> filter_graph_log);
#else
  static scoped_refptr<StreamParserBuffer> CreateEOSBuffer();
  static scoped_refptr<StreamParserBuffer> CopyFrom(
      const uint8* data, int data_size, bool is_keyframe);
#endif
  bool IsKeyframe() const { return is_keyframe_; }

  // Decode timestamp. If not explicitly set, or set to kNoTimestamp(), the
  // value will be taken from the normal timestamp.
  base::TimeDelta GetDecodeTimestamp() const;
  void SetDecodeTimestamp(const base::TimeDelta& timestamp);

  // Gets/sets the ID of the decoder config associated with this
  // buffer.
  int GetConfigId() const;
  void SetConfigId(int config_id);

 private:
#if defined(__LB_SHELL__)
  StreamParserBuffer(uint8* data, int data_size, bool is_keyframe,
      scoped_refptr<ShellFilterGraphLog> filter_graph_log);
#else
  StreamParserBuffer(const uint8* data, int data_size, bool is_keyframe);
#endif
  virtual ~StreamParserBuffer();

  bool is_keyframe_;
  base::TimeDelta decode_timestamp_;
  int config_id_;
  DISALLOW_COPY_AND_ASSIGN(StreamParserBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_H_
