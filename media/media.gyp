# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # Override to dynamically link the PulseAudio library.
    'use_pulseaudio%': 0,
    # Override to dynamically link the cras (ChromeOS audio) library.
    'use_cras%': 0,
    'conditions': [
      ['OS == "android" or OS == "ios"', {
        # Android and iOS don't use ffmpeg.
        'use_ffmpeg%': 0,
      }, {  # 'OS != "android" and OS != "ios"'
        'use_ffmpeg%': 1,
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'media',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../crypto/crypto.gyp:crypto',
        '../ui/ui.gyp:ui',
      ],
      'defines': [
        'MEDIA_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'audio/android/audio_manager_android.cc',
        'audio/android/audio_manager_android.h',
        'audio/android/opensles_input.cc',
        'audio/android/opensles_input.h',
        'audio/android/opensles_output.cc',
        'audio/android/opensles_output.h',
        'audio/async_socket_io_handler.h',
        'audio/async_socket_io_handler_posix.cc',
        'audio/async_socket_io_handler_win.cc',
        'audio/audio_buffers_state.cc',
        'audio/audio_buffers_state.h',
        'audio/audio_device_name.cc',
        'audio/audio_device_name.h',
        'audio/audio_device_thread.cc',
        'audio/audio_device_thread.h',
        'audio/audio_input_controller.cc',
        'audio/audio_input_controller.h',
        'audio/audio_input_device.cc',
        'audio/audio_input_device.h',
        'audio/audio_input_ipc.cc',
        'audio/audio_input_ipc.h',
        # TODO(dalecurtis): Temporarily disabled while switching pipeline to use
        # float, http://crbug.com/114700
        # 'audio/audio_output_mixer.cc',
        # 'audio/audio_output_mixer.h',
        'audio/audio_input_stream_impl.cc',
        'audio/audio_input_stream_impl.h',
        'audio/audio_io.h',
        'audio/audio_manager.cc',
        'audio/audio_manager.h',
        'audio/audio_manager_base.cc',
        'audio/audio_manager_base.h',
        'audio/audio_output_controller.cc',
        'audio/audio_output_controller.h',
        'audio/audio_output_device.cc',
        'audio/audio_output_device.h',
        'audio/audio_output_dispatcher.cc',
        'audio/audio_output_dispatcher.h',
        'audio/audio_output_dispatcher_impl.cc',
        'audio/audio_output_dispatcher_impl.h',
        'audio/audio_output_ipc.cc',
        'audio/audio_output_ipc.h',
        'audio/audio_output_proxy.cc',
        'audio/audio_output_proxy.h',
        'audio/audio_output_resampler.cc',
        'audio/audio_output_resampler.h',
        'audio/audio_util.cc',
        'audio/audio_util.h',
        'audio/cross_process_notification.cc',
        'audio/cross_process_notification.h',
        'audio/cross_process_notification_posix.cc',
        'audio/cross_process_notification_win.cc',
        'audio/fake_audio_input_stream.cc',
        'audio/fake_audio_input_stream.h',
        'audio/fake_audio_output_stream.cc',
        'audio/fake_audio_output_stream.h',
        'audio/ios/audio_manager_ios.h',
        'audio/ios/audio_manager_ios.mm',
        'audio/linux/alsa_input.cc',
        'audio/linux/alsa_input.h',
        'audio/linux/alsa_output.cc',
        'audio/linux/alsa_output.h',
        'audio/linux/alsa_util.cc',
        'audio/linux/alsa_util.h',
        'audio/linux/alsa_wrapper.cc',
        'audio/linux/alsa_wrapper.h',
        'audio/linux/audio_manager_linux.cc',
        'audio/linux/audio_manager_linux.h',
        'audio/linux/cras_input.cc',
        'audio/linux/cras_input.h',
        'audio/linux/cras_output.cc',
        'audio/linux/cras_output.h',
        'audio/mac/audio_input_mac.cc',
        'audio/mac/audio_input_mac.h',
        'audio/mac/audio_low_latency_input_mac.cc',
        'audio/mac/audio_low_latency_input_mac.h',
        'audio/mac/audio_low_latency_output_mac.cc',
        'audio/mac/audio_low_latency_output_mac.h',
        'audio/mac/audio_manager_mac.cc',
        'audio/mac/audio_manager_mac.h',
        'audio/mac/audio_output_mac.cc',
        'audio/mac/audio_output_mac.h',
        'audio/null_audio_sink.cc',
        'audio/null_audio_sink.h',
        'audio/openbsd/audio_manager_openbsd.cc',
        'audio/openbsd/audio_manager_openbsd.h',
        'audio/pulse/pulse_output.cc',
        'audio/pulse/pulse_output.h',
        'audio/sample_rates.cc',
        'audio/sample_rates.h',
        'audio/scoped_loop_observer.cc',
        'audio/scoped_loop_observer.h',
        'audio/simple_sources.cc',
        'audio/simple_sources.h',
        'audio/win/audio_low_latency_input_win.cc',
        'audio/win/audio_low_latency_input_win.h',
        'audio/win/audio_low_latency_output_win.cc',
        'audio/win/audio_low_latency_output_win.h',
        'audio/win/audio_manager_win.cc',
        'audio/win/audio_manager_win.h',
        'audio/win/avrt_wrapper_win.cc',
        'audio/win/avrt_wrapper_win.h',
        'audio/win/device_enumeration_win.cc',
        'audio/win/device_enumeration_win.h',
        'audio/win/wavein_input_win.cc',
        'audio/win/wavein_input_win.h',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'base/android/media_jni_registrar.cc',
        'base/android/media_jni_registrar.h',
        'base/audio_decoder.cc',
        'base/audio_decoder.h',
        'base/audio_decoder_config.cc',
        'base/audio_decoder_config.h',
        'base/audio_fifo.cc',
        'base/audio_fifo.h',
        'base/audio_pull_fifo.cc',
        'base/audio_pull_fifo.h',
        'base/audio_renderer.cc',
        'base/audio_renderer.h',
        'base/audio_renderer_mixer.cc',
        'base/audio_renderer_mixer.h',
        'base/audio_renderer_mixer_input.cc',
        'base/audio_renderer_mixer_input.h',
        'base/bind_to_loop.h',
        'base/bitstream_buffer.h',
        'base/bit_reader.cc',
        'base/bit_reader.h',
        'base/buffers.cc',
        'base/buffers.h',
        'base/byte_queue.cc',
        'base/byte_queue.h',
        'base/clock.cc',
        'base/clock.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/data_source.cc',
        'base/data_source.h',
        'base/decoder_buffer.cc',
        'base/decoder_buffer.h',
        'base/decryptor.h',
        'base/decryptor_client.h',
        'base/decrypt_config.cc',
        'base/decrypt_config.h',
        'base/demuxer.cc',
        'base/demuxer.h',
        'base/demuxer_stream.cc',
        'base/demuxer_stream.h',
        'base/djb2.cc',
        'base/djb2.h',
        'base/filter_collection.cc',
        'base/filter_collection.h',
        'base/media.h',
        'base/media_log.cc',
        'base/media_log.h',
        'base/media_log_event.h',
        'base/media_posix.cc',
        'base/media_switches.cc',
        'base/media_switches.h',
        'base/media_win.cc',
        'base/message_loop_factory.cc',
        'base/message_loop_factory.h',
        'base/multi_channel_resampler.cc',
        'base/multi_channel_resampler.h',
        'base/pipeline.cc',
        'base/pipeline.h',
        'base/pipeline_status.cc',
        'base/pipeline_status.h',
        'base/ranges.cc',
        'base/ranges.h',
        'base/seekable_buffer.cc',
        'base/seekable_buffer.h',
        'base/serial_runner.cc',
        'base/serial_runner.h',
        'base/sinc_resampler.cc',
        'base/sinc_resampler.h',
        'base/stream_parser.cc',
        'base/stream_parser.h',
        'base/stream_parser_buffer.cc',
        'base/stream_parser_buffer.h',
        'base/vector_math.cc',
        'base/vector_math.h',
        'base/video_decoder.cc',
        'base/video_decoder.h',
        'base/video_decoder_config.cc',
        'base/video_decoder_config.h',
        'base/video_frame.cc',
        'base/video_frame.h',
        'base/video_renderer.cc',
        'base/video_renderer.h',
        'base/video_util.cc',
        'base/video_util.h',
        'crypto/aes_decryptor.cc',
        'crypto/aes_decryptor.h',
        'ffmpeg/ffmpeg_common.cc',
        'ffmpeg/ffmpeg_common.h',
        'ffmpeg/file_protocol.cc',
        'ffmpeg/file_protocol.h',
        'filters/audio_file_reader.cc',
        'filters/audio_file_reader.h',
        'filters/audio_renderer_algorithm.cc',
        'filters/audio_renderer_algorithm.h',
        'filters/audio_renderer_impl.cc',
        'filters/audio_renderer_impl.h',
        'filters/chunk_demuxer.cc',
        'filters/chunk_demuxer.h',
        'filters/dummy_demuxer.cc',
        'filters/dummy_demuxer.h',
        'filters/ffmpeg_audio_decoder.cc',
        'filters/ffmpeg_audio_decoder.h',
        'filters/ffmpeg_demuxer.cc',
        'filters/ffmpeg_demuxer.h',
        'filters/ffmpeg_glue.cc',
        'filters/ffmpeg_glue.h',
        'filters/ffmpeg_h264_to_annex_b_bitstream_converter.cc',
        'filters/ffmpeg_h264_to_annex_b_bitstream_converter.h',
        'filters/ffmpeg_video_decoder.cc',
        'filters/ffmpeg_video_decoder.h',
        'filters/file_data_source.cc',
        'filters/file_data_source.h',
        'filters/gpu_video_decoder.cc',
        'filters/gpu_video_decoder.h',
        'filters/h264_to_annex_b_bitstream_converter.cc',
        'filters/h264_to_annex_b_bitstream_converter.h',
        'filters/in_memory_url_protocol.cc',
        'filters/in_memory_url_protocol.h',
        'filters/source_buffer_stream.cc',
        'filters/source_buffer_stream.h',
        'filters/video_frame_generator.cc',
        'filters/video_frame_generator.h',
        'filters/video_renderer_base.cc',
        'filters/video_renderer_base.h',
        'video/capture/fake_video_capture_device.cc',
        'video/capture/fake_video_capture_device.h',
        'video/capture/linux/video_capture_device_linux.cc',
        'video/capture/linux/video_capture_device_linux.h',
        'video/capture/mac/video_capture_device_mac.h',
        'video/capture/mac/video_capture_device_mac.mm',
        'video/capture/mac/video_capture_device_qtkit_mac.h',
        'video/capture/mac/video_capture_device_qtkit_mac.mm',
        'video/capture/video_capture.h',
        'video/capture/video_capture_device.h',
        'video/capture/video_capture_device_dummy.cc',
        'video/capture/video_capture_device_dummy.h',
        'video/capture/video_capture_proxy.cc',
        'video/capture/video_capture_proxy.h',
        'video/capture/video_capture_types.h',
        'video/capture/win/filter_base_win.cc',
        'video/capture/win/filter_base_win.h',
        'video/capture/win/pin_base_win.cc',
        'video/capture/win/pin_base_win.h',
        'video/capture/win/sink_filter_observer_win.h',
        'video/capture/win/sink_filter_win.cc',
        'video/capture/win/sink_filter_win.h',
        'video/capture/win/sink_input_pin_win.cc',
        'video/capture/win/sink_input_pin_win.h',
        'video/capture/win/video_capture_device_win.cc',
        'video/capture/win/video_capture_device_win.h',
        'video/picture.cc',
        'video/picture.h',
        'video/video_decode_accelerator.cc',
        'video/video_decode_accelerator.h',
        'webm/webm_cluster_parser.cc',
        'webm/webm_cluster_parser.h',
        'webm/webm_constants.h',
        'webm/webm_content_encodings.cc',
        'webm/webm_content_encodings.h',
        'webm/webm_content_encodings_client.cc',
        'webm/webm_content_encodings_client.h',
        'webm/webm_info_parser.cc',
        'webm/webm_info_parser.h',
        'webm/webm_parser.cc',
        'webm/webm_parser.h',
        'webm/webm_stream_parser.cc',
        'webm/webm_stream_parser.h',
        'webm/webm_tracks_parser.cc',
        'webm/webm_tracks_parser.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            'shared_memory_support',
            'yuv_convert',
          ],
        }],
        ['use_ffmpeg == 1', {
          'dependencies': [
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }, {  # use_ffmpeg == 0
          # Exclude the sources that depend on ffmpeg.
          'sources!': [
            'base/media_posix.cc',
            'ffmpeg/ffmpeg_common.cc',
            'ffmpeg/ffmpeg_common.h',
            'ffmpeg/file_protocol.cc',
            'ffmpeg/file_protocol.h',
            'filters/audio_file_reader.cc',
            'filters/audio_file_reader.h',
            'filters/chunk_demuxer.cc',
            'filters/chunk_demuxer.h',
            'filters/ffmpeg_audio_decoder.cc',
            'filters/ffmpeg_audio_decoder.h',
            'filters/ffmpeg_demuxer.cc',
            'filters/ffmpeg_demuxer.h',
            'filters/ffmpeg_glue.cc',
            'filters/ffmpeg_glue.h',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter.cc',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter.h',
            'filters/ffmpeg_video_decoder.cc',
            'filters/ffmpeg_video_decoder.h',
            'filters/gpu_video_decoder.cc',
            'filters/gpu_video_decoder.h',
            'webm/webm_cluster_parser.cc',
            'webm/webm_cluster_parser.h',
            'webm/webm_stream_parser.cc',
            'webm/webm_stream_parser.h',
          ],
        }],
        ['OS == "ios"', {
          'includes': [
            # For shared_memory_support_sources variable.
            'shared_memory_support.gypi',
          ],
          'sources': [
            'base/media_stub.cc',
            # These sources are normally built via a dependency on the
            # shared_memory_support target, but that target is not built on iOS.
            # Instead, directly build only the files that are needed for iOS.
            '<@(shared_memory_support_sources)',
          ],
          'sources/': [
            # iOS support is limited to audio input only.
            ['exclude', '.*'],
            ['include', '^audio/audio_buffers_state\\.'],
            ['include', '^audio/audio_input_controller\\.'],
            ['include', '^audio/audio_io\\.h$'],
            ['include', '^audio/audio_manager\\.'],
            ['include', '^audio/audio_manager_base\\.'],
            ['include', '^audio/audio_parameters\\.'],
            ['include', '^audio/fake_audio_input_stream\\.'],
            ['include', '^audio/fake_audio_output_stream\\.'],
            ['include', '^audio/ios/audio_manager_ios\\.'],
            ['include', '^base/audio_bus\\.'],
            ['include', '^base/channel_layout\\.'],
            ['include', '^base/media\\.h$'],
            ['include', '^base/media_stub\\.cc$'],
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
            ],
          },
        }],
        ['OS == "android"', {
          'sources': [
            'base/media_stub.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lOpenSLES',
            ],
          },
        }],
        # A simple WebM encoder for animated avatars on ChromeOS.
        ['chromeos==1', {
          'dependencies': [
            '../skia/skia.gyp:skia',
            '../third_party/libvpx/libvpx.gyp:libvpx',
            '../third_party/libyuv/libyuv.gyp:libyuv',
          ],
          'sources': [
            'webm/chromeos/ebml_writer.cc',
            'webm/chromeos/ebml_writer.h',
            'webm/chromeos/webm_encoder.cc',
            'webm/chromeos/webm_encoder.h',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="solaris"', {
          'link_settings': {
            'libraries': [
              '-lasound',
            ],
          },
        }],
        ['OS=="openbsd"', {
          'sources/': [ ['exclude', '/alsa_' ],
                        ['exclude', '/audio_manager_linux' ] ],
          'link_settings': {
            'libraries': [
            ],
          },
        }],
        ['OS!="openbsd"', {
          'sources!': [
            'audio/openbsd/audio_manager_openbsd.cc',
            'audio/openbsd/audio_manager_openbsd.h',
          ],
        }],
        ['OS=="linux"', {
          'variables': {
            'conditions': [
              ['sysroot!=""', {
                'pkg-config': '../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
              }, {
                'pkg-config': 'pkg-config'
              }],
            ],
          },
          'conditions': [
            ['use_cras == 1', {
              'cflags': [
                '<!@(<(pkg-config) --cflags libcras)',
              ],
              'link_settings': {
                'libraries': [
                  '<!@(<(pkg-config) --libs libcras)',
                ],
              },
              'defines': [
                'USE_CRAS',
              ],
            }, {  # else: use_cras == 0
              'sources!': [
                'audio/linux/cras_input.cc',
                'audio/linux/cras_input.h',
                'audio/linux/cras_output.cc',
                'audio/linux/cras_output.h',
              ],
            }],
          ],
        }],
        ['os_posix == 1', {
          'conditions': [
            ['use_pulseaudio == 1', {
              'cflags': [
                '<!@(pkg-config --cflags libpulse)',
              ],
              'link_settings': {
                'libraries': [
                  '<!@(pkg-config --libs-only-l libpulse)',
                ],
              },
              'defines': [
                'USE_PULSEAUDIO',
              ],
            }, {  # else: use_pulseaudio == 0
              'sources!': [
                'audio/pulse/pulse_output.cc',
                'audio/pulse/pulse_output.h',
              ],
            }],
          ],
        }],
        ['os_posix == 1 and OS != "android"', {
          # Video capture isn't supported in Android yet.
          'sources!': [
            'video/capture/video_capture_device_dummy.cc',
            'video/capture/video_capture_device_dummy.h',
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AudioUnit.framework',
              '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreVideo.framework',
              '$(SDKROOT)/System/Library/Frameworks/QTKit.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'sources!': [
            'audio/pulse/pulse_output.cc',
            'audio/pulse/pulse_output.h',
            'video/capture/video_capture_device_dummy.cc',
            'video/capture/video_capture_device_dummy.h',
          ],
        }],
        ['proprietary_codecs==1 or branding=="Chrome"', {
          'sources': [
            'mp4/aac.cc',
            'mp4/aac.h',
            'mp4/avc.cc',
            'mp4/avc.h',
            'mp4/box_definitions.cc',
            'mp4/box_definitions.h',
            'mp4/box_reader.cc',
            'mp4/box_reader.h',
            'mp4/cenc.cc',
            'mp4/cenc.h',
            'mp4/es_descriptor.cc',
            'mp4/es_descriptor.h',
            'mp4/mp4_stream_parser.cc',
            'mp4/mp4_stream_parser.h',
            'mp4/offset_byte_queue.cc',
            'mp4/offset_byte_queue.h',
            'mp4/track_run_iterator.cc',
            'mp4/track_run_iterator.h',
          ],
        }],
      ],
      'target_conditions': [
        ['OS == "ios"', {
          'sources/': [
            # Pull in specific Mac files for iOS (which have been filtered out
            # by file name rules).
            ['include', '^audio/mac/audio_input_mac\\.'],
          ],
        }],
      ],
    },
    {
      'target_name': 'media_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'media',
        'media_test_support',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/ui.gyp:ui',
      ],
      'sources': [
        'audio/async_socket_io_handler_unittest.cc',
        'audio/audio_input_controller_unittest.cc',
        'audio/audio_input_device_unittest.cc',
        'audio/audio_input_unittest.cc',
        'audio/audio_input_volume_unittest.cc',
        'audio/audio_low_latency_input_output_unittest.cc',
        'audio/audio_output_controller_unittest.cc',
        'audio/audio_output_device_unittest.cc',
        'audio/audio_output_proxy_unittest.cc',
        'audio/audio_parameters_unittest.cc',
        'audio/audio_util_unittest.cc',
        'audio/cross_process_notification_unittest.cc',
        'audio/ios/audio_manager_ios_unittest.cc',
        'audio/linux/alsa_output_unittest.cc',
        'audio/mac/audio_low_latency_input_mac_unittest.cc',
        'audio/mac/audio_output_mac_unittest.cc',
        'audio/simple_sources_unittest.cc',
        'audio/win/audio_low_latency_input_win_unittest.cc',
        'audio/win/audio_low_latency_output_win_unittest.cc',
        'audio/win/audio_output_win_unittest.cc',
        'base/audio_bus_unittest.cc',
        'base/audio_fifo_unittest.cc',
        'base/audio_pull_fifo_unittest.cc',
        'base/audio_renderer_mixer_input_unittest.cc',
        'base/audio_renderer_mixer_unittest.cc',
        'base/bit_reader_unittest.cc',
        'base/bind_to_loop_unittest.cc',
        'base/buffers_unittest.cc',
        'base/clock_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/decoder_buffer_unittest.cc',
        'base/djb2_unittest.cc',
        'base/filter_collection_unittest.cc',
        'base/multi_channel_resampler_unittest.cc',
        'base/pipeline_unittest.cc',
        'base/ranges_unittest.cc',
        'base/run_all_unittests.cc',
        'base/seekable_buffer_unittest.cc',
        'base/sinc_resampler_unittest.cc',
        'base/test_data_util.cc',
        'base/test_data_util.h',
        'base/vector_math_testing.h',
        'base/vector_math_unittest.cc',
        'base/video_frame_unittest.cc',
        'base/video_util_unittest.cc',
        'base/yuv_convert_unittest.cc',
        'crypto/aes_decryptor_unittest.cc',
        'ffmpeg/ffmpeg_common_unittest.cc',
        'filters/audio_renderer_algorithm_unittest.cc',
        'filters/audio_renderer_impl_unittest.cc',
        'filters/chunk_demuxer_unittest.cc',
        'filters/ffmpeg_audio_decoder_unittest.cc',
        'filters/ffmpeg_decoder_unittest.h',
        'filters/ffmpeg_demuxer_unittest.cc',
        'filters/ffmpeg_glue_unittest.cc',
        'filters/ffmpeg_h264_to_annex_b_bitstream_converter_unittest.cc',
        'filters/ffmpeg_video_decoder_unittest.cc',
        'filters/file_data_source_unittest.cc',
        'filters/h264_to_annex_b_bitstream_converter_unittest.cc',
        'filters/pipeline_integration_test.cc',
        'filters/pipeline_integration_test_base.cc',
        'filters/source_buffer_stream_unittest.cc',
        'filters/video_renderer_base_unittest.cc',
        'video/capture/video_capture_device_unittest.cc',
        'webm/cluster_builder.cc',
        'webm/cluster_builder.h',
        'webm/webm_cluster_parser_unittest.cc',
        'webm/webm_content_encodings_client_unittest.cc',
        'webm/webm_parser_unittest.cc',
      ],
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            'shared_memory_support',
            'yuv_convert',
          ],
        }],
        ['use_ffmpeg == 1', {
          'dependencies': [
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }],
        ['os_posix==1 and OS!="mac" and OS!="ios"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['OS == "ios"', {
          'sources/': [
            ['exclude', '.*'],
            ['include', '^audio/audio_input_controller_unittest\\.cc$'],
            ['include', '^audio/audio_input_unittest\\.cc$'],
            ['include', '^audio/audio_parameters_unittest\\.cc$'],
            ['include', '^audio/ios/audio_manager_ios_unittest\\.cc$'],
            ['include', '^base/mock_reader\\.h$'],
            ['include', '^base/run_all_unittests\\.cc$'],
          ],
        }],
        ['OS=="android"', {
          'sources!': [
            'audio/audio_input_volume_unittest.cc',
            'base/test_data_util.cc',
            'base/test_data_util.h',
            'ffmpeg/ffmpeg_common_unittest.cc',
            'filters/chunk_demuxer_unittest.cc',
            'filters/ffmpeg_audio_decoder_unittest.cc',
            'filters/ffmpeg_demuxer_unittest.cc',
            'filters/ffmpeg_glue_unittest.cc',
            'filters/ffmpeg_h264_to_annex_b_bitstream_converter_unittest.cc',
            'filters/ffmpeg_video_decoder_unittest.cc',
            'filters/pipeline_integration_test.cc',
            'filters/pipeline_integration_test_base.cc',
            'mp4/mp4_stream_parser_unittest.cc',
            'webm/webm_cluster_parser_unittest.cc',
          ],
          'conditions': [
            ['gtest_target_type == "shared_library"', {
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        }],
        ['OS == "linux"', {
          'conditions': [
            ['use_cras == 1', {
              'sources': [
                'audio/linux/cras_input_unittest.cc',
                'audio/linux/cras_output_unittest.cc',
              ],
              'defines': [
                'USE_CRAS',
              ],
            }],
          ],
        }],
        [ 'target_arch=="ia32" or target_arch=="x64"', {
          'sources': [
            'base/simd/convert_rgb_to_yuv_unittest.cc',
          ],
        }],
        ['proprietary_codecs==1 or branding=="Chrome"', {
          'sources': [
            'mp4/aac_unittest.cc',
            'mp4/avc_unittest.cc',
            'mp4/box_reader_unittest.cc',
            'mp4/es_descriptor_unittest.cc',
            'mp4/mp4_stream_parser_unittest.cc',
            'mp4/offset_byte_queue_unittest.cc',
            'mp4/track_run_iterator_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'media_test_support',
      'type': 'static_library',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'audio/mock_audio_manager.cc',
        'audio/mock_audio_manager.h',
        'audio/test_audio_input_controller_factory.cc',
        'audio/test_audio_input_controller_factory.h',
        'base/fake_audio_render_callback.cc',
        'base/fake_audio_render_callback.h',
        'base/mock_audio_renderer_sink.cc',
        'base/mock_audio_renderer_sink.h',
        'base/mock_callback.cc',
        'base/mock_callback.h',
        'base/mock_data_source_host.cc',
        'base/mock_data_source_host.h',
        'base/mock_demuxer_host.cc',
        'base/mock_demuxer_host.h',
        'base/mock_filters.cc',
        'base/mock_filters.h',
      ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          # Minimal target for NaCl and other renderer side media clients which
          # only need to send audio data across the shared memory to the browser
          # process.
          'target_name': 'shared_memory_support',
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'defines': [
            'MEDIA_IMPLEMENTATION',
          ],
          'include_dirs': [
            '..',
          ],
          'includes': [
            'shared_memory_support.gypi',
          ],
          'sources': [
            '<@(shared_memory_support_sources)',
          ],
        },
        {
          'target_name': 'yuv_convert',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'conditions': [
            ['order_profiling != 0', {
              'target_conditions' : [
                ['_toolset=="target"', {
                  'cflags!': [ '-finstrument-functions' ],
                }],
              ],
            }],
            [ 'target_arch == "ia32" or target_arch == "x64"', {
              'dependencies': [
                'yuv_convert_simd_x86',
              ],
            }],
            [ 'target_arch == "arm"', {
              'dependencies': [
                'yuv_convert_simd_arm',
              ],
            }],
          ],
          'sources': [
            'base/yuv_convert.cc',
            'base/yuv_convert.h',
          ],
        },
        {
          'target_name': 'yuv_convert_simd_x86',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'base/simd/convert_rgb_to_yuv_c.cc',
            'base/simd/convert_rgb_to_yuv_sse2.cc',
            'base/simd/convert_rgb_to_yuv_ssse3.asm',
            'base/simd/convert_rgb_to_yuv_ssse3.cc',
            'base/simd/convert_rgb_to_yuv_ssse3.inc',
            'base/simd/convert_yuv_to_rgb_c.cc',
            'base/simd/convert_yuv_to_rgb_mmx.asm',
            'base/simd/convert_yuv_to_rgb_mmx.inc',
            'base/simd/convert_yuv_to_rgb_sse.asm',
            'base/simd/convert_yuv_to_rgb_x86.cc',
            'base/simd/filter_yuv.h',
            'base/simd/filter_yuv_c.cc',
            'base/simd/filter_yuv_mmx.cc',
            'base/simd/filter_yuv_sse2.cc',
            'base/simd/linear_scale_yuv_to_rgb_mmx.asm',
            'base/simd/linear_scale_yuv_to_rgb_mmx.inc',
            'base/simd/linear_scale_yuv_to_rgb_sse.asm',
            'base/simd/scale_yuv_to_rgb_mmx.asm',
            'base/simd/scale_yuv_to_rgb_mmx.inc',
            'base/simd/scale_yuv_to_rgb_sse.asm',
            'base/simd/yuv_to_rgb_table.cc',
            'base/simd/yuv_to_rgb_table.h',
          ],
          'conditions': [
            ['order_profiling != 0', {
              'target_conditions' : [
                ['_toolset=="target"', {
                  'cflags!': [ '-finstrument-functions' ],
                }],
              ],
            }],
            [ 'target_arch == "x64"', {
              # Source files optimized for X64 systems.
              'sources': [
                'base/simd/linear_scale_yuv_to_rgb_mmx_x64.asm',
                'base/simd/scale_yuv_to_rgb_sse2_x64.asm',
              ],
            }],
            [ 'os_posix == 1 and OS != "mac" and OS != "android"', {
              'cflags': [
                '-msse2',
              ],
            }],
            [ 'OS == "mac"', {
              'configurations': {
                'Debug': {
                  'xcode_settings': {
                    # gcc on the mac builds horribly unoptimized sse code in
                    # debug mode. Since this is rarely going to be debugged,
                    # run with full optimizations in Debug as well as Release.
                    'GCC_OPTIMIZATION_LEVEL': '3',  # -O3
                   },
                 },
              },
            }],
            [ 'OS=="win"', {
              'variables': {
                'yasm_flags': [
                  '-DWIN32',
                  '-DMSVC',
                  '-DCHROMIUM',
                  '-Isimd',
                ],
              },
            }],
            [ 'OS=="mac"', {
              'variables': {
                'yasm_flags': [
                  '-DPREFIX',
                  '-DMACHO',
                  '-DCHROMIUM',
                  '-Isimd',
                ],
              },
            }],
            [ 'os_posix==1 and OS!="mac"', {
              'variables': {
                'conditions': [
                  [ 'target_arch=="ia32"', {
                    'yasm_flags': [
                      '-DX86_32',
                      '-DELF',
                      '-DCHROMIUM',
                      '-Isimd',
                    ],
                  }, {
                    'yasm_flags': [
                      '-DARCH_X86_64',
                      '-DELF',
                      '-DPIC',
                      '-DCHROMIUM',
                      '-Isimd',
                    ],
                  }],
                ],
              },
            }],
          ],
          'variables': {
            'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/media',
          },
          'msvs_2010_disable_uldi_when_referenced': 1,
          'includes': [
            '../third_party/yasm/yasm_compile.gypi',
          ],
        },
        {
          'target_name': 'yuv_convert_simd_arm',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'base/simd/convert_rgb_to_yuv.h',
            'base/simd/convert_rgb_to_yuv_c.cc',
            'base/simd/convert_yuv_to_rgb.h',
            'base/simd/convert_yuv_to_rgb_c.cc',
            'base/simd/filter_yuv.h',
            'base/simd/filter_yuv_c.cc',
            'base/simd/yuv_to_rgb_table.cc',
            'base/simd/yuv_to_rgb_table.h',
          ],
        },
        {
          'target_name': 'scaler_bench',
          'type': 'executable',
          'dependencies': [
            'media',
            'yuv_convert',
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
            '../ui/ui.gyp:ui',
          ],
          'sources': [
            'tools/scaler_bench/scaler_bench.cc',
          ],
        },
        {
          'target_name': 'qt_faststart',
          'type': 'executable',
          'sources': [
            'tools/qt_faststart/qt_faststart.c'
          ],
        },
        {
          'target_name': 'seek_tester',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
          ],
          'sources': [
            'tools/seek_tester/seek_tester.cc',
          ],
        },
      ],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'player_wtl',
          'type': 'executable',
          'dependencies': [
            'media',
            'yuv_convert',
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../ui/ui.gyp:ui',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources': [
            'tools/player_wtl/list.h',
            'tools/player_wtl/mainfrm.h',
            'tools/player_wtl/movie.cc',
            'tools/player_wtl/movie.h',
            'tools/player_wtl/player_wtl.cc',
            'tools/player_wtl/player_wtl.rc',
            'tools/player_wtl/props.h',
            'tools/player_wtl/resource.h',
            'tools/player_wtl/seek.h',
            'tools/player_wtl/view.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
          'defines': [
            '_CRT_SECURE_NO_WARNINGS=1',
          ],
        },
      ],
    }],
    ['OS == "win" or toolkit_uses_gtk == 1', {
      'targets': [
        {
          'target_name': 'shader_bench',
          'type': 'executable',
          'dependencies': [
            'media',
            'yuv_convert',
            '../base/base.gyp:base',
            '../ui/gl/gl.gyp:gl',
            '../ui/ui.gyp:ui',
          ],
          'sources': [
            'tools/shader_bench/cpu_color_painter.cc',
            'tools/shader_bench/cpu_color_painter.h',
            'tools/shader_bench/gpu_color_painter.cc',
            'tools/shader_bench/gpu_color_painter.h',
            'tools/shader_bench/gpu_painter.cc',
            'tools/shader_bench/gpu_painter.h',
            'tools/shader_bench/painter.cc',
            'tools/shader_bench/painter.h',
            'tools/shader_bench/shader_bench.cc',
            'tools/shader_bench/window.cc',
            'tools/shader_bench/window.h',
          ],
          'conditions': [
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
              'sources': [
                'tools/shader_bench/window_linux.cc',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../third_party/angle/src/build_angle.gyp:libEGL',
                '../third_party/angle/src/build_angle.gyp:libGLESv2',
              ],
              'sources': [
                'tools/shader_bench/window_win.cc',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS == "linux" and target_arch != "arm"', {
      'targets': [
        {
          'target_name': 'tile_render_bench',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/gl/gl.gyp:gl',
          ],
          'libraries': [
            '-lGL',
            '-ldl',
          ],
          'sources': [
            'tools/tile_render_bench/tile_render_bench.cc',
          ],
        },
      ],
    }],
    ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
      'targets': [
        {
          'target_name': 'player_x11',
          'type': 'executable',
          'dependencies': [
            'media',
            'yuv_convert',
            '../base/base.gyp:base',
            '../ui/gl/gl.gyp:gl',
            '../ui/ui.gyp:ui',
          ],
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lX11',
              '-lXrender',
              '-lXext',
            ],
          },
          'sources': [
            'tools/player_x11/data_source_logger.cc',
            'tools/player_x11/data_source_logger.h',
            'tools/player_x11/gl_video_renderer.cc',
            'tools/player_x11/gl_video_renderer.h',
            'tools/player_x11/player_x11.cc',
            'tools/player_x11/x11_video_renderer.cc',
            'tools/player_x11/x11_video_renderer.h',
          ],
        },
      ],
    }],
    # Special target to wrap a gtest_target_type==shared_library
    # media_unittests into an android apk for execution.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'media_unittests_apk',
          'type': 'none',
          'dependencies': [
            'media_java',
            'media_unittests',
          ],
          'variables': {
            'test_suite_name': 'media_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)media_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'player_android_jni_headers',
          'type': 'none',
          'sources': [
            'base/android/java/src/org/chromium/media/MediaPlayerListener.java',
          ],
          'variables': {
            'jni_gen_dir': 'media',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'player_android',
          'type': 'static_library',
          'sources': [
            'base/android/media_player_bridge.cc',
            'base/android/media_player_bridge.h',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'player_android_jni_headers',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/media',
          ],
        },
        {
          'target_name': 'media_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'export_dependent_settings': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'package_name': 'media',
            'java_in_dir': 'base/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },

      ],
    }],
    ['OS != "android" and OS != "ios"', {
      # Android and iOS do not use ffmpeg, so disable the targets which require
      # it.
      'targets': [
        {
          'target_name': 'ffmpeg_unittests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../base/base.gyp:test_support_base',
            '../base/base.gyp:test_support_perf',
            '../testing/gtest.gyp:gtest',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
            'media',
            'media_test_support',
          ],
          'sources': [
            'ffmpeg/ffmpeg_unittest.cc',
          ],
          'conditions': [
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                # Needed for the following #include chain:
                #   base/run_all_unittests.cc
                #   ../base/test_suite.h
                #   gtk/gtk.h
                '../build/linux/system.gyp:gtk',
              ],
              'conditions': [
                ['linux_use_tcmalloc==1', {
                  'dependencies': [
                    '../base/allocator/allocator.gyp:allocator',
                  ],
                }],
              ],
            }],
          ],
        },
        {
          'target_name': 'ffmpeg_regression_tests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
            'media',
            'media_test_support',
          ],
          'sources': [
            'base/run_all_unittests.cc',
            'base/test_data_util.cc',
            'ffmpeg/ffmpeg_regression_tests.cc',
            'filters/pipeline_integration_test_base.cc',
          ],
          'conditions': [
            ['os_posix==1 and OS!="mac"', {
              'conditions': [
                ['linux_use_tcmalloc==1', {
                  'dependencies': [
                    '../base/allocator/allocator.gyp:allocator',
                  ],
                }],
              ],
            }],
          ],
        },
        {
          'target_name': 'ffmpeg_tests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
            'media',
          ],
          'sources': [
            'test/ffmpeg_tests/ffmpeg_tests.cc',
          ],
        },
        {
          'target_name': 'media_bench',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
            'media',
          ],
          'sources': [
            'tools/media_bench/media_bench.cc',
          ],
        },
      ],
    }]
  ],
}
