# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'player_x11_renderer%': 'x11',
  },
  'targets': [
    {
      'target_name': 'media',
      'type': '<(library)',
      'dependencies': [
        'yuv_convert',
        '../base/base.gyp:base',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
      ],
      'include_dirs': [
        '..',
      ],
      'msvs_guid': '6AE76406-B03B-11DD-94B1-80B556D89593',
      'sources': [
        'audio/audio_buffers_state.cc',
        'audio/audio_buffers_state.h',
        'audio/audio_io.h',
        'audio/audio_input_controller.cc',
        'audio/audio_input_controller.h',
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
        'audio/openbsd/audio_manager_openbsd.cc',
        'audio/openbsd/audio_manager_openbsd.h',
        'audio/mac/audio_input_mac.cc',
        'audio/mac/audio_input_mac.h',
        'audio/mac/audio_low_latency_output_mac.cc',
        'audio/mac/audio_low_latency_output_mac.h',
        'audio/mac/audio_manager_mac.cc',
        'audio/mac/audio_manager_mac.h',
        'audio/mac/audio_output_mac.cc',
        'audio/mac/audio_output_mac.h',
        'audio/simple_sources.cc',
        'audio/simple_sources.h',
        'audio/win/audio_manager_win.h',
        'audio/win/audio_manager_win.cc',
        'audio/win/wavein_input_win.cc',
        'audio/win/wavein_input_win.h',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'base/buffers.cc',
        'base/buffers.h',
        'base/callback.cc',
        'base/callback.h',
        'base/clock.h',
        'base/clock_impl.cc',
        'base/clock_impl.h',
        'base/composite_filter.cc',
        'base/composite_filter.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/djb2.cc',
        'base/djb2.h',
        'base/filter_collection.cc',
        'base/filter_collection.h',
        'base/filter_host.h',
        'base/filters.cc',
        'base/filters.h',
        'base/h264_bitstream_converter.cc',
        'base/h264_bitstream_converter.h',
        'base/media.h',
        'base/media_format.cc',
        'base/media_format.h',
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
        'base/pts_heap.cc',
        'base/pts_heap.h',
        'base/seekable_buffer.cc',
        'base/seekable_buffer.h',
        'base/state_matrix.cc',
        'base/state_matrix.h',
        'base/video_frame.cc',
        'base/video_frame.h',
        'ffmpeg/ffmpeg_common.cc',
        'ffmpeg/ffmpeg_common.h',
        'ffmpeg/ffmpeg_util.cc',
        'ffmpeg/ffmpeg_util.h',
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
        'filters/audio_renderer_impl.cc',
        'filters/audio_renderer_impl.h',
        'filters/bitstream_converter.cc',
        'filters/bitstream_converter.h',
        'filters/decoder_base.h',
        'filters/ffmpeg_audio_decoder.cc',
        'filters/ffmpeg_audio_decoder.h',
        'filters/ffmpeg_demuxer.cc',
        'filters/ffmpeg_demuxer.h',
        'filters/ffmpeg_h264_bitstream_converter.cc',
        'filters/ffmpeg_h264_bitstream_converter.h',
        'filters/ffmpeg_glue.cc',
        'filters/ffmpeg_glue.h',
        'filters/ffmpeg_interfaces.cc',
        'filters/ffmpeg_interfaces.h',
        'filters/ffmpeg_video_decoder.cc',
        'filters/ffmpeg_video_decoder.h',
        'filters/file_data_source.cc',
        'filters/file_data_source.h',
        'filters/null_audio_renderer.cc',
        'filters/null_audio_renderer.h',
        'filters/null_video_renderer.cc',
        'filters/null_video_renderer.h',
        'filters/video_renderer_base.cc',
        'filters/video_renderer_base.h',
        'video/ffmpeg_video_allocator.cc',
        'video/ffmpeg_video_allocator.h',
        'video/ffmpeg_video_decode_engine.cc',
        'video/ffmpeg_video_decode_engine.h',
        'video/video_decode_engine.cc',
        'video/video_decode_engine.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'video/mft_h264_decode_engine.cc',
            'video/mft_h264_decode_engine.h',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'link_settings': {
            'libraries': [
              '-lasound',
            ],
          },
        }],
        ['OS=="openbsd"', {
          'sources/': [ ['exclude', 'alsa_' ],
                        ['exclude', 'audio_manager_linux' ],
                        ['exclude', '\\.mm?$' ] ],
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
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'sources': [
            'filters/omx_video_decoder.cc',
            'filters/omx_video_decoder.h',
          ],
          'dependencies': [
            'omx_wrapper',
          ]
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AudioUnit.framework',
              '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'cpu_features',
      'type': '<(library)',
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
      'type': '<(library)',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'cpu_features',
      ],
      'conditions': [
        [ 'target_arch == "ia32" or target_arch == "x64"', {
          'dependencies': [
            'yuv_convert_sse2',
          ],
        }],
      ],
      'sources': [
        'base/yuv_convert.cc',
        'base/yuv_convert.h',
        'base/yuv_convert_internal.h',
        'base/yuv_convert_c.cc',
        'base/yuv_row_win.cc',
        'base/yuv_row_posix.cc',
        'base/yuv_row_table.cc',
        'base/yuv_row.h',
      ],
    },
    {
      'target_name': 'yuv_convert_sse2',
      'type': '<(library)',
      'include_dirs': [
        '..',
      ],
      'conditions': [
        [ 'OS == "linux" or OS == "freebsd" or OS == "openbsd"', {
          'cflags': [
            '-msse2',
          ],
        }],
      ],
      'sources': [
        'base/yuv_convert_sse2.cc',
      ],
    },
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
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
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
      'target_name': 'media_unittests',
      'type': 'executable',
      'msvs_guid': 'C8C6183C-B03C-11DD-B471-DFD256D89593',
      'dependencies': [
        'media',
        'media_test_support',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '../third_party/openmax/openmax.gyp:il',
      ],
      'sources!': [
        '../third_party/openmax/omx_stub.cc',
      ],
      'sources': [
        'audio/audio_input_controller_unittest.cc',
        'audio/audio_input_unittest.cc',
        'audio/audio_output_controller_unittest.cc',
        'audio/audio_output_proxy_unittest.cc',
        'audio/audio_parameters_unittest.cc',
        'audio/audio_util_unittest.cc',
        'audio/fake_audio_input_stream_unittest.cc',
        'audio/linux/alsa_output_unittest.cc',
        'audio/mac/audio_output_mac_unittest.cc',
        'audio/simple_sources_unittest.cc',
        'audio/win/audio_output_win_unittest.cc',
        'base/composite_filter_unittest.cc',
        'base/clock_impl_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/djb2_unittest.cc',
        'base/filter_collection_unittest.cc',
        'base/h264_bitstream_converter_unittest.cc',
        'base/mock_ffmpeg.cc',
        'base/mock_ffmpeg.h',
        'base/mock_reader.h',
        'base/mock_task.cc',
        'base/mock_task.h',
        'base/pipeline_impl_unittest.cc',
        'base/pts_heap_unittest.cc',
        'base/run_all_unittests.cc',
        'base/seekable_buffer_unittest.cc',
        'base/state_matrix_unittest.cc',
        'base/video_frame_unittest.cc',
        'base/yuv_convert_unittest.cc',
        'filters/audio_renderer_algorithm_ola_unittest.cc',
        'filters/audio_renderer_base_unittest.cc',
        'filters/bitstream_converter_unittest.cc',
        'filters/decoder_base_unittest.cc',
        'filters/ffmpeg_demuxer_unittest.cc',
        'filters/ffmpeg_glue_unittest.cc',
        'filters/ffmpeg_h264_bitstream_converter_unittest.cc',
        'filters/ffmpeg_video_decoder_unittest.cc',
        'filters/file_data_source_unittest.cc',
        'filters/video_renderer_base_unittest.cc',
        'omx/mock_omx.cc',
        'omx/mock_omx.h',
        'video/ffmpeg_video_decode_engine_unittest.cc',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'dependencies': [
            # Needed for the following #include chain:
            #   base/run_all_unittests.cc
            #   ../base/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            'omx/omx_codec_unittest.cc',
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
      'target_name': 'media_test_support',
      'type': '<(library)',
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
      'target_name': 'media_bench',
      'type': 'executable',
      'msvs_guid': '45BC4F87-4604-4962-A751-7C7B29A080BF',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
      ],
      'sources': [
        'tools/media_bench/media_bench.cc',
      ],
    },
    {
      'target_name': 'scaler_bench',
      'type': 'executable',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'tools/scaler_bench/scaler_bench.cc',
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
      'target_name': 'wav_ola_test',
      'type': 'executable',
      'dependencies': [
        'media',
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
            '../base/base.gyp:base',
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
        {
          'target_name': 'mfplayer',
          'type': 'executable',
          'dependencies': [
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/mfplayer/mfplayer.h',
            'tools/mfplayer/mfplayer.cc',
            'tools/mfplayer/mf_playback_main.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
            },
          },
        },
        {
          'target_name': 'mfdecoder',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/mfdecoder/main.cc',
            'tools/mfdecoder/mfdecoder.h',
            'tools/mfdecoder/mfdecoder.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
            },
          },
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
            '../app/app.gyp:app_base',
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
            ['OS=="linux"', {
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
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
      'targets': [
        {
          'target_name': 'omx_test',
          'type': 'executable',
          'dependencies': [
            'media',
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
            '../third_party/openmax/openmax.gyp:il',
          ],
          'sources': [
            'tools/omx_test/color_space_util.cc',
            'tools/omx_test/color_space_util.h',
            'tools/omx_test/file_reader_util.cc',
            'tools/omx_test/file_reader_util.h',
            'tools/omx_test/file_sink.cc',
            'tools/omx_test/file_sink.h',
            'tools/omx_test/omx_test.cc',
          ],
        },
        {
          'target_name': 'omx_unittests',
          'type': 'executable',
          'dependencies': [
            'media',
            'omx_wrapper',
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
          ],
          'conditions': [
            ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
          ],
          'sources': [
            'omx/omx_unittest.cc',
            'omx/run_all_unittests.cc',
          ],
        },
        {
          'target_name': 'omx_wrapper',
          'type': '<(library)',
          'dependencies': [
            '../base/base.gyp:base',
            '../third_party/openmax/openmax.gyp:il',
            # TODO(wjia): remove ffmpeg
            '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources': [
            'omx/omx_configurator.cc',
            'omx/omx_configurator.h',
            'video/omx_video_decode_engine.cc',
            'video/omx_video_decode_engine.cc',
          ],
          'hard_dependency': 1,
          'export_dependent_settings': [
            '../third_party/openmax/openmax.gyp:il',
          ],
        },
        {
          'target_name': 'player_x11',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
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
            'tools/player_x11/player_x11.cc',
          ],
          'conditions' : [
            ['player_x11_renderer == "x11"', {
              'sources': [
                'tools/player_x11/x11_video_renderer.cc',
                'tools/player_x11/x11_video_renderer.h',
              ],
              'defines': [
                'RENDERER_X11',
              ],
            }],
            ['player_x11_renderer == "gles"', {
              'libraries': [
                '-lEGL',
                '-lGLESv2',
              ],
              'sources': [
                'tools/player_x11/gles_video_renderer.cc',
                'tools/player_x11/gles_video_renderer.h',
              ],
              'defines': [
                'RENDERER_GLES',
              ],
            }],
            ['player_x11_renderer == "gl"', {
              'dependencies': [
                '../app/app.gyp:app_base',
              ],
              'sources': [
                'tools/player_x11/gl_video_renderer.cc',
                'tools/player_x11/gl_video_renderer.h',
              ],
              'defines': [
                'RENDERER_GL',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
