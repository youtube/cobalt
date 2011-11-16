# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # Override to dynamically link the PulseAudio library.
    'use_pulseaudio%': 0,
  },
  'targets': [
    {
      'target_name': 'media',
      'type': '<(component)',
      'dependencies': [
        'yuv_convert',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '../third_party/openmax/openmax.gyp:il',
        '../ui/ui.gyp:ui',
      ],
      'defines': [
        'MEDIA_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'audio/audio_buffers_state.cc',
        'audio/audio_buffers_state.h',
        'audio/audio_io.h',
        'audio/audio_input_controller.cc',
        'audio/audio_input_controller.h',
        'audio/audio_device_name.cc',
        'audio/audio_device_name.h',
        'audio/audio_manager.cc',
        'audio/audio_manager.h',
        'audio/audio_manager_base.cc',
        'audio/audio_manager_base.h',
        'audio/audio_output_controller.cc',
        'audio/audio_output_controller.h',
        'audio/audio_output_dispatcher.cc',
        'audio/audio_output_dispatcher.h',
        'audio/audio_output_proxy.cc',
        'audio/audio_output_proxy.h',
        'audio/audio_parameters.cc',
        'audio/audio_parameters.h',
        'audio/audio_util.cc',
        'audio/audio_util.h',
        'audio/fake_audio_input_stream.cc',
        'audio/fake_audio_input_stream.h',
        'audio/fake_audio_output_stream.cc',
        'audio/fake_audio_output_stream.h',
        'audio/linux/audio_manager_linux.cc',
        'audio/linux/audio_manager_linux.h',
        'audio/linux/alsa_input.cc',
        'audio/linux/alsa_input.h',
        'audio/linux/alsa_output.cc',
        'audio/linux/alsa_output.h',
        'audio/linux/alsa_util.cc',
        'audio/linux/alsa_util.h',
        'audio/linux/alsa_wrapper.cc',
        'audio/linux/alsa_wrapper.h',
        'audio/linux/pulse_output.cc',
        'audio/linux/pulse_output.h',
        'audio/openbsd/audio_manager_openbsd.cc',
        'audio/openbsd/audio_manager_openbsd.h',
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
        'audio/win/wavein_input_win.cc',
        'audio/win/wavein_input_win.h',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'base/async_filter_factory_base.cc',
        'base/async_filter_factory_base.h',
        'base/audio_decoder_config.cc',
        'base/audio_decoder_config.h',
        'base/bitstream_buffer.h',
        'base/buffers.cc',
        'base/buffers.h',
        'base/byte_queue.cc',
        'base/byte_queue.h',
        'base/channel_layout.cc',
        'base/channel_layout.h',
        'base/clock.cc',
        'base/clock.h',
        'base/composite_data_source_factory.cc',
        'base/composite_data_source_factory.h',
        'base/composite_filter.cc',
        'base/composite_filter.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/demuxer.cc',
        'base/demuxer.h',
        'base/demuxer_stream.cc',
        'base/demuxer_stream.h',
        'base/djb2.cc',
        'base/djb2.h',
        'base/filter_collection.cc',
        'base/filter_collection.h',
        'base/filter_factories.cc',
        'base/filter_factories.h',
        'base/filter_host.h',
        'base/filters.cc',
        'base/filters.h',
        'base/h264_bitstream_converter.cc',
        'base/h264_bitstream_converter.h',
        'base/media.h',
        'base/media_export.h',
        'base/media_log.cc',
        'base/media_log.h',
        'base/media_log_event.h',
        'base/media_posix.cc',
        'base/media_switches.cc',
        'base/media_switches.h',
        'base/media_win.cc',
        'base/message_loop_factory.cc',
        'base/message_loop_factory.h',
        'base/message_loop_factory_impl.cc',
        'base/message_loop_factory_impl.h',
        'base/pipeline.h',
        'base/pipeline_impl.cc',
        'base/pipeline_impl.h',
        'base/pipeline_status.h',
        'base/pts_heap.cc',
        'base/pts_heap.h',
        'base/pts_stream.cc',
        'base/pts_stream.h',
        'base/seekable_buffer.cc',
        'base/seekable_buffer.h',
        'base/state_matrix.cc',
        'base/state_matrix.h',
        'base/video_decoder_config.cc',
        'base/video_decoder_config.h',
        'base/video_frame.cc',
        'base/video_frame.h',
        'base/video_util.cc',
        'base/video_util.h',
        'ffmpeg/ffmpeg_common.cc',
        'ffmpeg/ffmpeg_common.h',
        'ffmpeg/file_protocol.cc',
        'ffmpeg/file_protocol.h',
        'filters/audio_file_reader.cc',
        'filters/audio_file_reader.h',
        'filters/audio_renderer_algorithm_base.cc',
        'filters/audio_renderer_algorithm_base.h',
        'filters/audio_renderer_algorithm_default.cc',
        'filters/audio_renderer_algorithm_default.h',
        'filters/audio_renderer_algorithm_ola.cc',
        'filters/audio_renderer_algorithm_ola.h',
        'filters/audio_renderer_base.cc',
        'filters/audio_renderer_base.h',
        'filters/bitstream_converter.cc',
        'filters/bitstream_converter.h',
        'filters/chunk_demuxer.cc',
        'filters/chunk_demuxer.h',
        'filters/chunk_demuxer_client.h',
        'filters/chunk_demuxer_factory.cc',
        'filters/chunk_demuxer_factory.h',
        'filters/dummy_demuxer.cc',
        'filters/dummy_demuxer.h',
        'filters/dummy_demuxer_factory.cc',
        'filters/dummy_demuxer_factory.h',
        'filters/ffmpeg_audio_decoder.cc',
        'filters/ffmpeg_audio_decoder.h',
        'filters/ffmpeg_demuxer.cc',
        'filters/ffmpeg_demuxer.h',
        'filters/ffmpeg_demuxer_factory.cc',
        'filters/ffmpeg_demuxer_factory.h',
        'filters/ffmpeg_h264_bitstream_converter.cc',
        'filters/ffmpeg_h264_bitstream_converter.h',
        'filters/ffmpeg_glue.cc',
        'filters/ffmpeg_glue.h',
        'filters/ffmpeg_video_decoder.cc',
        'filters/ffmpeg_video_decoder.h',
        'filters/file_data_source.cc',
        'filters/file_data_source.h',
        'filters/file_data_source_factory.cc',
        'filters/file_data_source_factory.h',
        'filters/in_memory_url_protocol.cc',
        'filters/in_memory_url_protocol.h',
        'filters/null_audio_renderer.cc',
        'filters/null_audio_renderer.h',
        'filters/null_video_renderer.cc',
        'filters/null_video_renderer.h',
        'filters/reference_audio_renderer.cc',
        'filters/reference_audio_renderer.h',
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
        'video/ffmpeg_video_decode_engine.cc',
        'video/ffmpeg_video_decode_engine.h',
        'video/picture.cc',
        'video/picture.h',
        'video/video_decode_accelerator.cc',
        'video/video_decode_accelerator.h',
        'video/video_decode_engine.h',
        'webm/webm_constants.h',
        'webm/webm_cluster_parser.cc',
        'webm/webm_cluster_parser.h',
        'webm/webm_info_parser.cc',
        'webm/webm_info_parser.h',
        'webm/webm_parser.cc',
        'webm/webm_parser.h',
        'webm/webm_tracks_parser.cc',
        'webm/webm_tracks_parser.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        # Android doesn't use ffmpeg, so make the dependency conditional
        # and exclude the sources which depend on ffmpeg.
        ['OS=="android"', {
          'dependencies!': [
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources!': [
            'base/media_posix.cc',
            'ffmpeg/ffmpeg_common.cc',
            'ffmpeg/ffmpeg_common.h',
            'ffmpeg/file_protocol.cc',
            'ffmpeg/file_protocol.h',
            'filters/audio_file_reader.cc',
            'filters/audio_file_reader.h',
            'filters/bitstream_converter.cc',
            'filters/bitstream_converter.h',
            'filters/chunk_demuxer.cc',
            'filters/chunk_demuxer.h',
            'filters/chunk_demuxer_client.h',
            'filters/chunk_demuxer_factory.cc',
            'filters/chunk_demuxer_factory.h',
            'filters/ffmpeg_audio_decoder.cc',
            'filters/ffmpeg_audio_decoder.h',
            'filters/ffmpeg_demuxer.cc',
            'filters/ffmpeg_demuxer.h',
            'filters/ffmpeg_demuxer_factory.cc',
            'filters/ffmpeg_demuxer_factory.h',
            'filters/ffmpeg_h264_bitstream_converter.cc',
            'filters/ffmpeg_h264_bitstream_converter.h',
            'filters/ffmpeg_glue.cc',
            'filters/ffmpeg_glue.h',
            'filters/ffmpeg_video_decoder.cc',
            'filters/ffmpeg_video_decoder.h',
            'video/ffmpeg_video_decode_engine.cc',
            'video/ffmpeg_video_decode_engine.h',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="solaris"', {
          'link_settings': {
            'libraries': [
              '-lasound',
            ],
          },
          'conditions': [
            ['OS=="linux"', {
              'conditions': [
                ['use_pulseaudio == 1', {
                  'link_settings': {
                    'libraries': [
                      '-lpulse',
                    ],
                  },
                  'defines': [
                    'USE_PULSEAUDIO',
                  ],
                }, {  # else: use_pulseaudio == 0
                  'sources!': [
                    'audio/linux/pulse_output.cc',
                    'audio/linux/pulse_output.h',
                  ],
                }],
              ],
            }],
          ],
        }],
        ['OS=="openbsd"', {
          'sources/': [ ['exclude', '/alsa_' ],
                        ['exclude', '/audio_manager_linux' ],
                        ['exclude', '/pulse_' ] ],
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
        ['os_posix == 1', {
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
            'video/capture/video_capture_device_dummy.cc',
            'video/capture/video_capture_device_dummy.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'cpu_features',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'conditions': [
        [ 'target_arch == "ia32" or target_arch == "x64"', {
          'sources': [
            'base/cpu_features_x86.cc',
          ],
        }],
        [ 'target_arch == "arm"', {
          'sources': [
            'base/cpu_features_arm.cc',
          ],
        }],
      ],
      'sources': [
        'base/cpu_features.h',
      ],
    },
    {
      'target_name': 'yuv_convert',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'cpu_features',
      ],
      'conditions': [
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
        'base/simd/convert_yuv_to_rgb_x86.cc',
        'base/simd/convert_yuv_to_rgb_mmx.asm',
        'base/simd/convert_yuv_to_rgb_mmx.inc',
        'base/simd/convert_yuv_to_rgb_sse.asm',
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
        [ 'target_arch == "x64"', {
          # Source files optimized for X64 systems.
          'sources': [
            'base/simd/linear_scale_yuv_to_rgb_mmx_x64.asm',
            'base/simd/scale_yuv_to_rgb_sse2_x64.asm',
          ],
        }],
        [ 'os_posix == 1 and OS != "mac"', {
          'cflags': [
            '-msse2',
            '-msse3',
            '-mssse3',
          ],
        }],
        [ 'OS == "openbsd"', {
          # OpenBSD's gcc (4.2.1) does not support -mssse3
          'cflags!': [
            '-mssse3',
          ],
        }],
        [ 'OS == "mac"', {
          'configurations': {
            'Debug': {
              'xcode_settings': {
                # gcc on the mac builds horribly unoptimized sse code in debug
                # mode. Since this is rarely going to be debugged, run with full
                # optimizations in Debug as well as Release.
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
        'base/simd/convert_rgb_to_yuv_c.cc',
        'base/simd/convert_rgb_to_yuv.h',
        'base/simd/convert_yuv_to_rgb_c.cc',
        'base/simd/convert_yuv_to_rgb.h',
        'base/simd/filter_yuv.h',
        'base/simd/filter_yuv_c.cc',
        'base/simd/yuv_to_rgb_table.cc',
        'base/simd/yuv_to_rgb_table.h',
      ],
    },
    {
      'target_name': 'media_unittests',
      'type': 'executable',
      'dependencies': [
        'media',
        'media_test_support',
        'yuv_convert',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '../ui/ui.gyp:ui',
      ],
      'sources': [
        'audio/audio_input_controller_unittest.cc',
        'audio/audio_input_device_unittest.cc',
        'audio/audio_input_unittest.cc',
        'audio/audio_output_controller_unittest.cc',
        'audio/audio_output_proxy_unittest.cc',
        'audio/audio_parameters_unittest.cc',
        'audio/audio_util_unittest.cc',
        'audio/linux/alsa_output_unittest.cc',
        'audio/mac/audio_low_latency_input_mac_unittest.cc',
        'audio/mac/audio_output_mac_unittest.cc',
        'audio/simple_sources_unittest.cc',
        'audio/win/audio_low_latency_input_win_unittest.cc',
        'audio/win/audio_low_latency_output_win_unittest.cc',
        'audio/win/audio_output_win_unittest.cc',
        'base/clock_unittest.cc',
        'base/composite_filter_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/djb2_unittest.cc',
        'base/filter_collection_unittest.cc',
        'base/h264_bitstream_converter_unittest.cc',
        'base/mock_reader.h',
        'base/pipeline_impl_unittest.cc',
        'base/pts_heap_unittest.cc',
        'base/pts_stream_unittest.cc',
        'base/run_all_unittests.cc',
        'base/seekable_buffer_unittest.cc',
        'base/state_matrix_unittest.cc',
        'base/test_data_util.cc',
        'base/test_data_util.h',
        'base/video_frame_unittest.cc',
        'base/video_util_unittest.cc',
        'base/yuv_convert_unittest.cc',
        'ffmpeg/ffmpeg_common_unittest.cc',
        'filters/audio_renderer_algorithm_ola_unittest.cc',
        'filters/audio_renderer_base_unittest.cc',
        'filters/bitstream_converter_unittest.cc',
        'filters/chunk_demuxer_unittest.cc',
        'filters/ffmpeg_audio_decoder_unittest.cc',
        'filters/ffmpeg_demuxer_unittest.cc',
        'filters/ffmpeg_glue_unittest.cc',
        'filters/ffmpeg_h264_bitstream_converter_unittest.cc',
        'filters/ffmpeg_video_decoder_unittest.cc',
        'filters/file_data_source_unittest.cc',
        'filters/video_renderer_base_unittest.cc',
        'video/capture/video_capture_device_unittest.cc',
        'webm/cluster_builder.cc',
        'webm/cluster_builder.h',
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
        ['OS=="android"', {
          'dependencies!': [
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources!': [
            'ffmpeg/ffmpeg_common_unittest.cc',
            'filters/ffmpeg_audio_decoder_unittest.cc',
            'filters/bitstream_converter_unittest.cc',
            'filters/chunk_demuxer_unittest.cc',
            'filters/ffmpeg_demuxer_unittest.cc',
            'filters/ffmpeg_glue_unittest.cc',
            'filters/ffmpeg_h264_bitstream_converter_unittest.cc',
            'filters/ffmpeg_video_decoder_unittest.cc',
          ],
        }],
        [ 'target_arch=="ia32" or target_arch=="x64"', {
          'sources': [
            'base/simd/convert_rgb_to_yuv_unittest.cc',
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
        'audio/test_audio_input_controller_factory.cc',
        'audio/test_audio_input_controller_factory.h',
        'base/mock_callback.cc',
        'base/mock_callback.h',
        'base/mock_filter_host.cc',
        'base/mock_filter_host.h',
        'base/mock_filters.cc',
        'base/mock_filters.h',
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
      ],
      'sources': [
        'tools/scaler_bench/scaler_bench.cc',
      ],
    },
    {
      'target_name': 'wav_ola_test',
      'type': 'executable',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
      ],
      'sources': [
        'tools/wav_ola_test/wav_ola_test.cc'
      ],
    },
    {
      'target_name': 'qt_faststart',
      'type': 'executable',
      'sources': [
        'tools/qt_faststart/qt_faststart.c'
      ],
    },
  ],
  'conditions': [
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
            'tools/player_wtl/seek.h',
            'tools/player_wtl/resource.h',
            'tools/player_wtl/view.h',
            'tools/player_wtl/wtl_renderer.cc',
            'tools/player_wtl/wtl_renderer.h',
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
    ['OS!="mac"', {
      'targets': [
        {
          'target_name': 'shader_bench',
          'type': 'executable',
          'dependencies': [
            'media',
            'yuv_convert',
            '../base/base.gyp:base',
            '../ui/gfx/gl/gl.gyp:gl',
          ],
          'sources': [
            'tools/shader_bench/shader_bench.cc',
            'tools/shader_bench/cpu_color_painter.cc',
            'tools/shader_bench/cpu_color_painter.h',
            'tools/shader_bench/gpu_color_painter.cc',
            'tools/shader_bench/gpu_color_painter.h',
            'tools/shader_bench/gpu_color_painter_exp.cc',
            'tools/shader_bench/gpu_color_painter_exp.h',
            'tools/shader_bench/gpu_painter.cc',
            'tools/shader_bench/gpu_painter.h',
            'tools/shader_bench/painter.cc',
            'tools/shader_bench/painter.h',
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
            '../ui/gfx/gl/gl.gyp:gl',
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
    ['os_posix == 1 and OS != "mac"', {
      'targets': [
        {
          'target_name': 'player_x11',
          'type': 'executable',
          'dependencies': [
            'media',
            'yuv_convert',
            '../base/base.gyp:base',
            '../ui/gfx/gl/gl.gyp:gl',
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
            'tools/player_x11/gl_video_renderer.cc',
            'tools/player_x11/gl_video_renderer.h',
            'tools/player_x11/player_x11.cc',
            'tools/player_x11/x11_video_renderer.cc',
            'tools/player_x11/x11_video_renderer.h',
          ],
        },
      ],
    }],
    # Android does not use ffmpeg, so disable the targets which require it.
    ['OS!="android"', {
      'targets': [
        {
          'target_name': 'ffmpeg_unittests',
          'type': 'executable',
          'dependencies': [
            'media',
            'media_test_support',
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../base/base.gyp:test_support_base',
            '../base/base.gyp:test_support_perf',
            '../testing/gtest.gyp:gtest',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
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
          'target_name': 'ffmpeg_tests',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources': [
            'test/ffmpeg_tests/ffmpeg_tests.cc',
          ],
        },
        {
          'target_name': 'media_bench',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources': [
            'tools/media_bench/media_bench.cc',
          ],
        },
      ],
    }]
  ],
}
