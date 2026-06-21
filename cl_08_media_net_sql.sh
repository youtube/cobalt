#!/bin/bash
git restore --source=add-thread-based-memory-allocation-hooks \
  media/base/pipeline_impl.cc \
  media/ffmpeg/BUILD.gn \
  media/ffmpeg/ffmpeg_decoding_loop.cc \
  media/filters/BUILD.gn \
  media/filters/chunk_demuxer.cc \
  media/filters/ffmpeg_audio_decoder.cc \
  media/filters/ffmpeg_video_decoder.cc \
  media/starboard/BUILD.gn \
  media/starboard/decoder_buffer_allocator.cc \
  media/starboard/sbplayer_bridge.cc \
  media/starboard/starboard_renderer.cc \
  net/BUILD.gn \
  net/http/http_cache_transaction.cc \
  net/url_request/url_request.cc \
  sql/BUILD.gn \
  sql/database.cc \
  sql/statement.cc \
  starboard/android/shared/audio_track_bridge.cc
