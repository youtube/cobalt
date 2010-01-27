# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'target_defaults': {
    'conditions': [
      ['OS!="linux"', {'sources/': [['exclude', '/linux/']]}],
      ['OS!="freebsd"', {'sources/': [['exclude', '/freebsd/']]}],
      ['OS!="mac"', {'sources/': [['exclude', '/mac/']]}],
      ['OS!="win"', {'sources/': [['exclude', '/win/']]}],
    ],
  },
  'targets': [
    {
      'target_name': 'media',
      'type': '<(library)',
      'dependencies': [
        'omx_wrapper',
        '../base/base.gyp:base',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
      ],
      'include_dirs': [
        '..',
      ],
      'msvs_guid': '6AE76406-B03B-11DD-94B1-80B556D89593',
      'sources': [
        'audio/audio_output.h',
        'audio/audio_util.cc',
        'audio/audio_util.h',
        'audio/fake_audio_output_stream.cc',
        'audio/fake_audio_output_stream.h',
        'audio/linux/audio_manager_linux.cc',
        'audio/linux/audio_manager_linux.h',
        'audio/linux/alsa_output.cc',
        'audio/linux/alsa_output.h',
        'audio/linux/alsa_wrapper.cc',
        'audio/linux/alsa_wrapper.h',
        'audio/mac/audio_manager_mac.cc',
        'audio/mac/audio_manager_mac.h',
        'audio/mac/audio_output_mac.cc',
        'audio/mac/audio_output_mac.h',
        'audio/simple_sources.cc',
        'audio/simple_sources.h',
        'audio/win/audio_manager_win.h',
        'audio/win/audio_output_win.cc',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'base/buffer_queue.cc',
        'base/buffer_queue.h',
        'base/buffers.cc',
        'base/buffers.h',
        'base/callback.h',
        'base/clock.h',
        'base/clock_impl.cc',
        'base/clock_impl.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/djb2.cc',
        'base/djb2.h',
        'base/factory.h',
        'base/filter_host.h',
        'base/filters.h',
        'base/media.h',
        'base/media_format.cc',
        'base/media_format.h',
        'base/media_posix.cc',
        'base/media_switches.cc',
        'base/media_switches.h',
        'base/media_win.cc',
        'base/pipeline.h',
        'base/pipeline_impl.cc',
        'base/pipeline_impl.h',
        'base/pts_heap.h',
        'base/seekable_buffer.cc',
        'base/seekable_buffer.h',
        'base/synchronizer.cc',
        'base/synchronizer.h',
        'base/video_frame_impl.cc',
        'base/video_frame_impl.h',
        'base/yuv_convert.cc',
        'base/yuv_convert.h',
        'base/yuv_row_win.cc',
        'base/yuv_row_mac.cc',
        'base/yuv_row_linux.cc',
        'base/yuv_row.h',
        'ffmpeg/ffmpeg_common.cc',
        'ffmpeg/ffmpeg_common.h',
        'ffmpeg/ffmpeg_util.cc',
        'ffmpeg/ffmpeg_util.h',
        'ffmpeg/file_protocol.cc',
        'ffmpeg/file_protocol.h',
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
        'filters/ffmpeg_glue.cc',
        'filters/ffmpeg_glue.h',
        'filters/ffmpeg_interfaces.cc',
        'filters/ffmpeg_interfaces.h',
        'filters/ffmpeg_video_decode_engine.cc',
        'filters/ffmpeg_video_decode_engine.h',
        'filters/ffmpeg_video_decoder.cc',
        'filters/ffmpeg_video_decoder.h',
        'filters/file_data_source.cc',
        'filters/file_data_source.h',
        'filters/null_audio_renderer.cc',
        'filters/null_audio_renderer.h',
        'filters/omx_video_decode_engine.cc',
        'filters/omx_video_decode_engine.h',
        'filters/omx_video_decoder.cc',
        'filters/omx_video_decoder.h',
        'filters/video_decoder_impl.cc',
        'filters/video_decoder_impl.h',
        'filters/video_decode_engine.h',
        'filters/video_renderer_base.cc',
        'filters/video_renderer_base.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS =="linux"', {
          'sources/': [ ['exclude', '_(mac|win)\\.cc$'],
                        ['exclude', '\\.mm?$' ] ],
          'link_settings': {
            'libraries': [
              '-lasound',
            ],
          },
        }],
        ['OS =="freebsd"', {
          'sources/': [ ['exclude', '_(mac|win)\\.cc$'],
                        ['exclude', '\\.mm?$' ] ],
          'link_settings': {
            'libraries': [
            ],
          },
        }],
        ['OS =="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
            ],
          },
          'sources/': [ ['exclude', '_(linux|win)\\.cc$'],
          ],
        }],
        [ 'OS == "win"', {
          'sources/': [ ['exclude', '_(linux|mac|posix)\\.cc$'],
                        ['exclude', '\\.mm?$' ] ],
        }],
      ],
    },
    {
      'target_name': 'media_unittests',
      'type': 'executable',
      'msvs_guid': 'C8C6183C-B03C-11DD-B471-DFD256D89593',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
      ],
      'sources': [
        'audio/audio_util_unittest.cc',
        'audio/linux/alsa_output_unittest.cc',
        'audio/mac/audio_output_mac_unittest.cc',
        'audio/simple_sources_unittest.cc',
        'audio/win/audio_output_win_unittest.cc',
        'base/buffer_queue_unittest.cc',
        'base/clock_impl_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/djb2_unittest.cc',
        'base/mock_ffmpeg.cc',
        'base/mock_ffmpeg.h',
        'base/mock_filter_host.h',
        'base/mock_filters.cc',
        'base/mock_filters.h',
        'base/mock_reader.h',
        'base/mock_task.h',
        'base/pipeline_impl_unittest.cc',
        'base/pts_heap_unittest.cc',
        'base/run_all_unittests.cc',
        'base/seekable_buffer_unittest.cc',
        'base/video_frame_impl_unittest.cc',
        'base/yuv_convert_unittest.cc',
        'filters/audio_renderer_algorithm_ola_unittest.cc',
        'filters/audio_renderer_base_unittest.cc',
        'filters/bitstream_converter_unittest.cc',
        'filters/ffmpeg_demuxer_unittest.cc',
        'filters/ffmpeg_glue_unittest.cc',
        'filters/ffmpeg_video_decode_engine_unittest.cc',
        'filters/file_data_source_unittest.cc',
        'filters/video_decoder_impl_unittest.cc',
        'filters/video_renderer_base_unittest.cc',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
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
    {
      'target_name': 'omx_test',
      'type': 'executable',
      'dependencies': [
        'omx_wrapper',
        '../base/base.gyp:base',
      ],
      'sources': [
        'tools/omx_test/omx_test.cc',
      ],
    },
    {
      'target_name': 'omx_unittests',
      'type': 'executable',
      'dependencies': [
        'omx_wrapper',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../testing/gtest.gyp:gtest',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
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
      ],
      'sources': [
        'omx/input_buffer.cc',
        'omx/input_buffer.h',
        'omx/omx_codec.cc',
        'omx/omx_codec.h',
      ],
      'export_dependent_settings': [
        '../third_party/openmax/openmax.gyp:il',
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
            '../chrome/third_party/wtl/include',
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
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'player_x11',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
            '../gpu/gpu.gyp:gl_libs',
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
            'tools/player_x11/x11_video_renderer.cc',
            'tools/player_x11/x11_video_renderer.h',
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
