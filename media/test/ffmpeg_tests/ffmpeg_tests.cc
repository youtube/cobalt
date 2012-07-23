// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Software qualification test for FFmpeg.  This test is used to certify that
// software decoding quality and performance of FFmpeg meets a mimimum
// standard.

#include <iomanip>
#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "media/base/djb2.h"
#include "media/base/media.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/filters/ffmpeg_video_decoder.h"

#ifdef DEBUG
#define SHOW_VERBOSE 1
#else
#define SHOW_VERBOSE 0
#endif

#if defined(OS_WIN)

// Enable to build with exception handler
//#define ENABLE_WINDOWS_EXCEPTIONS 1

#ifdef ENABLE_WINDOWS_EXCEPTIONS
// warning: disable warning about exception handler.
#pragma warning(disable:4509)
#endif

// Thread priorities to make benchmark more stable.

void EnterTimingSection() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

void LeaveTimingSection() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}
#else
void EnterTimingSection() {
  pthread_attr_t pta;
  struct sched_param param;

  pthread_attr_init(&pta);
  memset(&param, 0, sizeof(param));
  param.sched_priority = 78;
  pthread_attr_setschedparam(&pta, &param);
  pthread_attr_destroy(&pta);
}

void LeaveTimingSection() {
}
#endif

int main(int argc, const char** argv) {
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  const CommandLine::StringVector& filenames = cmd_line->GetArgs();

  if (filenames.empty()) {
    std::cerr << "Usage: " << argv[0] << " MEDIAFILE" << std::endl;
    return 1;
  }

  // Initialize our media library (try loading DLLs, etc.) before continuing.
  // We use an empty file path as the parameter to force searching of the
  // default locations for necessary DLLs and DSOs.
  if (media::InitializeMediaLibrary(FilePath()) == false) {
    std::cerr << "Unable to initialize the media library.";
    return 1;
  }

  // Retrieve command line options.
  FilePath in_path(filenames[0]);
  FilePath out_path;
  if (filenames.size() > 1)
    out_path = FilePath(filenames[1]);

  // Default flags that match Chrome defaults.
  int video_threads = 2;
  int verbose_level = AV_LOG_FATAL;
  int max_frames = 0;
  int max_loops = 0;
  bool flush = false;

  unsigned int hash_value = 5381u;  // Seed for DJB2.
  bool hash_djb2 = false;

  base::MD5Context ctx;  // Intermediate MD5 data: do not use
  base::MD5Init(&ctx);
  bool hash_md5 = false;

  std::ostream* log_out = &std::cout;
#if defined(ENABLE_WINDOWS_EXCEPTIONS)
  // Catch exceptions so this tool can be used in automated testing.
  __try {
#endif

  // Register FFmpeg and attempt to open file.
  av_log_set_level(verbose_level);
  av_register_all();
  av_register_protocol2(&kFFmpegFileProtocol, sizeof(kFFmpegFileProtocol));
  AVFormatContext* format_context = NULL;
  // avformat_open_input() wants a char*, which can't work with wide paths.
  // So we assume ASCII on Windows.  On other platforms we can pass the
  // path bytes through verbatim.
#if defined(OS_WIN)
  std::string string_path = WideToASCII(in_path.value());
#else
  const std::string& string_path = in_path.value();
#endif
  int result = avformat_open_input(&format_context, string_path.c_str(),
                                   NULL, NULL);
  if (result < 0) {
    switch (result) {
      case AVERROR(EINVAL):
        std::cerr << "Error: File format not supported "
                  << in_path.value() << std::endl;
        break;
      default:
        std::cerr << "Error: Could not open input for "
                  << in_path.value() << std::endl;
        break;
    }
    return 1;
  }

  // Open output file.
  FILE *output = NULL;
  if (!out_path.empty()) {
    output = file_util::OpenFile(out_path, "wb");
    if (!output) {
      std::cerr << "Error: Could not open output "
                << out_path.value() << std::endl;
      return 1;
    }
  }

  // Parse a little bit of the stream to fill out the format context.
  if (avformat_find_stream_info(format_context, NULL) < 0) {
    std::cerr << "Error: Could not find stream info for "
              << in_path.value() << std::endl;
    return 1;
  }

  // Find our target stream(s)
  int video_stream = -1;
  int audio_stream = -1;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;

    if (codec_context->codec_type == AVMEDIA_TYPE_VIDEO && video_stream < 0) {
#if SHOW_VERBOSE
      *log_out << "V ";
#endif
      video_stream = i;
    } else {
      if (codec_context->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream < 0) {
#if SHOW_VERBOSE
        *log_out << "A ";
#endif
        audio_stream = i;
      } else {
#if SHOW_VERBOSE
      *log_out << "  ";
#endif
      }
    }

#if SHOW_VERBOSE
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
    if (!codec || (codec_context->codec_type == AVMEDIA_TYPE_UNKNOWN)) {
      *log_out << "Stream #" << i << ": Unknown" << std::endl;
    } else {
      // Print out stream information
      *log_out << "Stream #" << i << ": " << codec->name << " ("
               << codec->long_name << ")" << std::endl;
    }
#endif
  }
  int target_stream = video_stream;
  AVMediaType target_codec = AVMEDIA_TYPE_VIDEO;
  if (target_stream < 0) {
    target_stream = audio_stream;
    target_codec = AVMEDIA_TYPE_AUDIO;
  }

  // Only continue if we found our target stream.
  if (target_stream < 0) {
    std::cerr << "Error: Could not find target stream "
              << target_stream << " for " << in_path.value() << std::endl;
    return 1;
  }

  // Prepare FFmpeg structures.
  AVPacket packet;
  AVCodecContext* codec_context = format_context->streams[target_stream]->codec;
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

  // Only continue if we found our codec.
  if (!codec) {
    std::cerr << "Error: Could not find codec for "
              << in_path.value() << std::endl;
    return 1;
  }

  codec_context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context->err_recognition = AV_EF_CAREFUL;

  // Initialize threaded decode.
  if (target_codec == AVMEDIA_TYPE_VIDEO && video_threads > 0) {
    codec_context->thread_count = video_threads;
  }

  // Initialize our codec.
  if (avcodec_open2(codec_context, codec, NULL) < 0) {
    std::cerr << "Error: Could not open codec "
              << codec_context->codec->name << " for "
              << in_path.value() << std::endl;
    return 1;
  }

  // Buffer used for audio decoding.
  scoped_ptr_malloc<AVFrame, media::ScopedPtrAVFree> audio_frame(
      avcodec_alloc_frame());
  if (!audio_frame.get()) {
    std::cerr << "Error: avcodec_alloc_frame for "
              << in_path.value() << std::endl;
    return 1;
  }

  // Buffer used for video decoding.
  scoped_ptr_malloc<AVFrame, media::ScopedPtrAVFree> video_frame(
      avcodec_alloc_frame());
  if (!video_frame.get()) {
    std::cerr << "Error: avcodec_alloc_frame for "
              << in_path.value() << std::endl;
    return 1;
  }

  // Stats collector.
  EnterTimingSection();
  std::vector<double> decode_times;
  decode_times.reserve(4096);
  // Parse through the entire stream until we hit EOF.
#if SHOW_VERBOSE
  base::TimeTicks start = base::TimeTicks::HighResNow();
#endif
  int frames = 0;
  int read_result = 0;
  do {
    read_result = av_read_frame(format_context, &packet);

    if (read_result < 0) {
      if (max_loops) {
        --max_loops;
      }
      if (max_loops > 0) {
        av_seek_frame(format_context, -1, 0, AVSEEK_FLAG_BACKWARD);
        read_result = 0;
        continue;
      }
      if (flush) {
        packet.stream_index = target_stream;
        packet.size = 0;
      } else {
        break;
      }
    }

    // Only decode packets from our target stream.
    if (packet.stream_index == target_stream) {
      int result = -1;
      if (target_codec == AVMEDIA_TYPE_AUDIO) {
        int size_out = 0;
        int got_audio = 0;

        avcodec_get_frame_defaults(audio_frame.get());

        base::TimeTicks decode_start = base::TimeTicks::HighResNow();
        result = avcodec_decode_audio4(codec_context, audio_frame.get(),
                                       &got_audio, &packet);
        base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;

        if (got_audio) {
          size_out = av_samples_get_buffer_size(
              NULL, codec_context->channels, audio_frame->nb_samples,
              codec_context->sample_fmt, 1);
        }

        if (got_audio && size_out) {
          decode_times.push_back(delta.InMillisecondsF());
          ++frames;
          read_result = 0;  // Force continuation.

          if (output) {
            if (fwrite(audio_frame->data[0], 1, size_out, output) !=
                static_cast<size_t>(size_out)) {
              std::cerr << "Error: Could not write "
                        << size_out << " bytes for " << in_path.value()
                        << std::endl;
              return 1;
            }
          }

          const uint8* u8_samples =
              reinterpret_cast<const uint8*>(audio_frame->data[0]);
          if (hash_djb2) {
            hash_value = DJB2Hash(u8_samples, size_out, hash_value);
          }
          if (hash_md5) {
            base::MD5Update(
                &ctx,
                base::StringPiece(reinterpret_cast<const char*>(u8_samples),
                                                                size_out));
          }
        }
      } else if (target_codec == AVMEDIA_TYPE_VIDEO) {
        int got_picture = 0;

        avcodec_get_frame_defaults(video_frame.get());

        base::TimeTicks decode_start = base::TimeTicks::HighResNow();
        result = avcodec_decode_video2(codec_context, video_frame.get(),
                                       &got_picture, &packet);
        base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;

        if (got_picture) {
          decode_times.push_back(delta.InMillisecondsF());
          ++frames;
          read_result = 0;  // Force continuation.

          for (int plane = 0; plane < 3; ++plane) {
            const uint8* source = video_frame->data[plane];
            const size_t source_stride = video_frame->linesize[plane];
            size_t bytes_per_line = codec_context->width;
            size_t copy_lines = codec_context->height;
            if (plane != 0) {
              switch (codec_context->pix_fmt) {
                case PIX_FMT_YUV420P:
                case PIX_FMT_YUVJ420P:
                  bytes_per_line /= 2;
                  copy_lines = (copy_lines + 1) / 2;
                  break;
                case PIX_FMT_YUV422P:
                case PIX_FMT_YUVJ422P:
                  bytes_per_line /= 2;
                  break;
                case PIX_FMT_YUV444P:
                case PIX_FMT_YUVJ444P:
                  break;
                default:
                  std::cerr << "Error: Unknown video format "
                            << codec_context->pix_fmt;
                  return 1;
              }
            }
            if (output) {
              for (size_t i = 0; i < copy_lines; ++i) {
                if (fwrite(source, 1, bytes_per_line, output) !=
                           bytes_per_line) {
                  std::cerr << "Error: Could not write data after "
                            << copy_lines << " lines for "
                            << in_path.value() << std::endl;
                  return 1;
                }
                source += source_stride;
              }
            }
            if (hash_djb2) {
              for (size_t i = 0; i < copy_lines; ++i) {
                hash_value = DJB2Hash(source, bytes_per_line, hash_value);
                source += source_stride;
              }
            }
            if (hash_md5) {
              for (size_t i = 0; i < copy_lines; ++i) {
                base::MD5Update(
                    &ctx,
                    base::StringPiece(reinterpret_cast<const char*>(source),
                                bytes_per_line));
                source += source_stride;
              }
            }
          }
        }
      } else {
        NOTREACHED();
      }

      // Make sure our decoding went OK.
      if (result < 0) {
        std::cerr << "Error: avcodec_decode returned "
                  << result << " for " << in_path.value() << std::endl;
        return 1;
      }
    }
    // Free our packet.
    av_free_packet(&packet);

    if (max_frames && (frames >= max_frames))
      break;
  } while (read_result >= 0);
#if SHOW_VERBOSE
  base::TimeDelta total = base::TimeTicks::HighResNow() - start;
#endif
  LeaveTimingSection();

  // Clean up.
  if (output)
    file_util::CloseFile(output);
  if (codec_context)
    avcodec_close(codec_context);
  if (format_context)
    avformat_close_input(&format_context);

  // Calculate the sum of times.  Note that some of these may be zero.
  double sum = 0;
  for (size_t i = 0; i < decode_times.size(); ++i) {
    sum += decode_times[i];
  }

  if (sum > 0) {
    if (target_codec == AVMEDIA_TYPE_AUDIO) {
      // Calculate the average milliseconds per frame.
      // Audio decoding is usually in the millisecond or range, and
      // best expressed in time (ms) rather than FPS, which can approach
      // infinity.
      double ms = sum / frames;
      // Print our results.
      log_out->setf(std::ios::fixed);
      log_out->precision(2);
      *log_out << "TIME PER FRAME (MS):" << std::setw(11) << ms << std::endl;
    } else if (target_codec == AVMEDIA_TYPE_VIDEO) {
      // Calculate the average frames per second.
      // Video decoding is expressed in Frames Per Second - a term easily
      // understood and should exceed a typical target of 30 fps.
      double fps = frames * 1000.0 / sum;
      // Print our results.
      log_out->setf(std::ios::fixed);
      log_out->precision(2);
      *log_out << "FPS:" << std::setw(11) << fps << std::endl;
    }
  }

#if SHOW_VERBOSE
  // Print our results.
  log_out->setf(std::ios::fixed);
  log_out->precision(2);
  *log_out << std::endl;
  *log_out << "     Frames:" << std::setw(11) << frames
           << std::endl;
  *log_out << "      Total:" << std::setw(11) << total.InMillisecondsF()
           << " ms" << std::endl;
  *log_out << "  Summation:" << std::setw(11) << sum
           << " ms" << std::endl;

  if (frames > 0) {
    // Calculate the average time per frame.
    double average = sum / frames;

    // Calculate the sum of the squared differences.
    // Standard deviation will only be accurate if no threads are used.
    // TODO(fbarchard): Rethink standard deviation calculation.
    double squared_sum = 0;
    for (int i = 0; i < frames; ++i) {
      double difference = decode_times[i] - average;
      squared_sum += difference * difference;
    }

    // Calculate the standard deviation (jitter).
    double stddev = sqrt(squared_sum / frames);

    *log_out << "    Average:" << std::setw(11) << average
             << " ms" << std::endl;
    *log_out << "     StdDev:" << std::setw(11) << stddev
             << " ms" << std::endl;
  }
  if (hash_djb2) {
    *log_out << "  DJB2 Hash:" << std::setw(11) << hash_value
             << "  " << in_path.value() << std::endl;
  }
  if (hash_md5) {
    base::MD5Digest digest;  // The result of the computation.
    base::MD5Final(&digest, &ctx);
    *log_out << "   MD5 Hash: " << base::MD5DigestToBase16(digest)
             << " " << in_path.value() << std::endl;
  }
#endif  // SHOW_VERBOSE
#if defined(ENABLE_WINDOWS_EXCEPTIONS)
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    *log_out << "  Exception:" << std::setw(11) << GetExceptionCode()
             << " " << in_path.value() << std::endl;
    return 1;
  }
#endif
  CommandLine::Reset();
  return 0;
}
