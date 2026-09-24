// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_STREAM_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_STREAM_PARSER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/byte_queue.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include "media/base/segmented_byte_queue.h"
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

namespace media {

class WebMClusterParser;

class MEDIA_EXPORT WebMStreamParser : public StreamParser {
 public:
  WebMStreamParser();

  WebMStreamParser(const WebMStreamParser&) = delete;
  WebMStreamParser& operator=(const WebMStreamParser&) = delete;

  ~WebMStreamParser() override;

  // StreamParser implementation.
  void Init(InitCB init_cb,
            NewConfigCB config_cb,
            NewBuffersCB new_buffers_cb,
            EncryptedMediaInitDataCB encrypted_media_init_data_cb,
            NewMediaSegmentCB new_segment_cb,
            EndMediaSegmentCB end_of_segment_cb,
            MediaLog* media_log) override;
  void Flush() override;
  bool GetGenerateTimestampsFlag() const override;
  [[nodiscard]] bool AppendToParseBuffer(
      base::span<const uint8_t> buf) override;
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  [[nodiscard]] bool AppendToParseBuffer(
      base::span<const uint8_t> buf,
      base::ScopedClosureRunner release_runner) override;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
  [[nodiscard]] ParseStatus Parse(int max_pending_bytes_to_inspect) override;
#if BUILDFLAG(USE_STARBOARD_MEDIA)
 private:
  // The following function and variable should technically be moved down.  They
  // are kept here to make the new feature easy to review and maintain.
  [[nodiscard]] ParseStatus ParseInternal(int max_pending_bytes_to_inspect);
  [[nodiscard]] ParseStatus ParseInternalSegmented(
      int max_pending_bytes_to_inspect);

  bool buffers_parsed_ = false;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

 private:
  enum State {
    kWaitingForInit,
    kParsingHeaders,
    kParsingClusters,
    kError
  };

  void ChangeState(State new_state);

  // Parses WebM Header, Info, Tracks elements. It also skips other level 1
  // elements that are not used right now. Once the Info & Tracks elements have
  // been parsed, this method will transition the parser from PARSING_HEADERS to
  // PARSING_CLUSTERS.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseInfoAndTracks(const uint8_t* data, int size);
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  int ParseInfoAndTracksSegmented(int offset, int size);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

  // Incrementally parses WebM cluster elements. This method also skips
  // CUES elements if they are encountered since we currently don't use the
  // data in these elements.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseCluster(const uint8_t* data, int size);
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  int ParseClusterSegmented(int offset, int size);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

  // Fire the encrypted event through the |encrypted_media_init_data_cb_|.
  void OnEncryptedMediaInitData(const std::string& key_id);

  State state_;
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  EncryptedMediaInitDataCB encrypted_media_init_data_cb_;

  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;
  raw_ptr<MediaLog> media_log_;

  bool unknown_segment_size_;

  std::unique_ptr<WebMClusterParser> cluster_parser_;

  // Tracks how much data has not yet been attempted to be parsed from
  // `byte_queue_` between calls to Parse(). AppendToParseBuffer() increases
  // this from 0 as more data is added. Parse() incrementally reduces this and
  // Flush() zeroes this. Note that Parse() may have inspected some data at the
  // front of `byte_queue_` but not yet been able to pop it from the queue. So
  // this value may be lower than the actual amount of bytes in `byte_queue_`,
  // since more data is needed to complete the parse.
  int uninspected_pending_bytes_ = 0;
  ByteQueue byte_queue_;

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  enum class QueueMode {
    kUndetermined,  // No append yet; either queue may still be chosen.
    kCopying,       // `byte_queue_` holds the stream.
    kBorrowing,     // `segmented_queue_` holds the stream.
  };
  QueueMode queue_mode_ = QueueMode::kUndetermined;

  // Zero-copy counterpart of `byte_queue_`, holding each appended buffer by
  // reference instead of copying it. When it holds the stream, `byte_queue_`
  // is only scratch for ranges straddling a segment boundary.
  SegmentedByteQueue segmented_queue_;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_STREAM_PARSER_H_
