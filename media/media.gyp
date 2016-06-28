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
      ['OS == "android" or OS == "ios" or OS == "lb_shell" or OS == "starboard"', {
        # Android and iOS don't use ffmpeg.
        'use_ffmpeg%': 0,
      }, {  # 'OS != "android" and OS != "ios"' and OS != "lb_shell" and OS != "starboard"
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
        'audio/mac/audio_synchronized_mac.cc',
        'audio/mac/audio_synchronized_mac.h',
        'audio/mac/audio_unified_mac.cc',
        'audio/mac/audio_unified_mac.h',
        'audio/null_audio_sink.cc',
        'audio/null_audio_sink.h',
        'audio/null_audio_streamer.cc',
        'audio/null_audio_streamer.h',
        'audio/openbsd/audio_manager_openbsd.cc',
        'audio/openbsd/audio_manager_openbsd.h',
        'audio/pulse/pulse_output.cc',
        'audio/pulse/pulse_output.h',
        'audio/sample_rates.cc',
        'audio/sample_rates.h',
        'audio/scoped_loop_observer.cc',
        'audio/scoped_loop_observer.h',
        'audio/shell_wav_test_probe.cc',
        'audio/shell_wav_test_probe.h',
        'audio/shell_audio_sink.cc',
        'audio/shell_audio_sink.h',
        'audio/simple_sources.cc',
        'audio/simple_sources.h',
        'audio/virtual_audio_input_stream.cc',
        'audio/virtual_audio_input_stream.h',
        'audio/virtual_audio_output_stream.cc',
        'audio/virtual_audio_output_stream.h',
        'audio/win/audio_device_listener_win.cc',
        'audio/win/audio_device_listener_win.h',
        'audio/win/audio_low_latency_input_win.cc',
        'audio/win/audio_low_latency_input_win.h',
        'audio/win/audio_low_latency_output_win.cc',
        'audio/win/audio_low_latency_output_win.h',
        'audio/win/audio_manager_win.cc',
        'audio/win/audio_manager_win.h',
        'audio/win/audio_unified_win.cc',
        'audio/win/audio_unified_win.h',
        'audio/win/avrt_wrapper_win.cc',
        'audio/win/avrt_wrapper_win.h',
        'audio/win/device_enumeration_win.cc',
        'audio/win/device_enumeration_win.h',
        'audio/win/core_audio_util_win.cc',
        'audio/win/core_audio_util_win.h',
        'audio/win/wavein_input_win.cc',
        'audio/win/wavein_input_win.h',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'base/android/cookie_getter.cc',
        'base/android/cookie_getter.h',
        'base/android/media_player_bridge_manager.cc',
        'base/android/media_player_bridge_manager.h',
        'base/audio_capturer_source.h',
        'base/audio_converter.cc',
        'base/audio_converter.h',
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
        'base/audio_renderer_sink.h',
        'base/audio_renderer_mixer.cc',
        'base/audio_renderer_mixer.h',
        'base/audio_renderer_mixer_input.cc',
        'base/audio_renderer_mixer_input.h',
        'base/audio_splicer.cc',
        'base/audio_splicer.h',
        'base/audio_timestamp_helper.cc',
        'base/audio_timestamp_helper.h',
        'base/bind_to_loop.h',
        'base/bitstream_buffer.h',
        'base/bit_reader.cc',
        'base/bit_reader.h',
        'base/buffers.cc',
        'base/buffers.h',
        'base/byte_queue.cc',
        'base/byte_queue.h',
        'base/channel_mixer.cc',
        'base/channel_mixer.h',
        'base/clock.cc',
        'base/clock.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/data_source.cc',
        'base/data_source.h',
        'base/decoder_buffer.cc',
        'base/decoder_buffer.h',
        'base/decoder_buffer_pool.cc',
        'base/decoder_buffer_pool.h',
        'base/decryptor.cc',
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
        'base/interleaved_sinc_resampler.cc',
        'base/interleaved_sinc_resampler.h',
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
        'base/pipeline.h',
        'base/pipeline_impl.cc',
        'base/pipeline_impl.h',
        'base/pipeline_status.cc',
        'base/pipeline_status.h',
        'base/ranges.cc',
        'base/ranges.h',
        'base/sample_format.cc',
        'base/sample_format.h',
        'base/seekable_buffer.cc',
        'base/seekable_buffer.h',
        'base/serial_runner.cc',
        'base/serial_runner.h',
        'base/shell_audio_bus.cc',
        'base/shell_audio_bus.h',
        'base/shell_buffer_factory.cc',
        'base/shell_buffer_factory.h',
        'base/shell_data_source_reader.cc',
        'base/shell_data_source_reader.h',
        'base/shell_media_platform.cc',
        'base/shell_media_platform.h',
        'base/shell_media_statistics.cc',
        'base/shell_media_statistics.h',
        'base/shell_video_data_allocator.cc',
        'base/shell_video_data_allocator.h',
        'base/shell_video_frame_provider.cc',
        'base/shell_video_frame_provider.h',
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
        'crypto/shell_decryptor_factory.cc',
        'crypto/shell_decryptor_factory.h',
        'ffmpeg/ffmpeg_common.cc',
        'ffmpeg/ffmpeg_common.h',
        'filters/audio_decoder_selector.cc',
        'filters/audio_decoder_selector.h',
        'filters/audio_file_reader.cc',
        'filters/audio_file_reader.h',
        'filters/audio_renderer_algorithm.cc',
        'filters/audio_renderer_algorithm.h',
        'filters/audio_renderer_impl.cc',
        'filters/audio_renderer_impl.h',
        'filters/blocking_url_protocol.cc',
        'filters/blocking_url_protocol.h',
        'filters/chunk_demuxer.cc',
        'filters/chunk_demuxer.h',
        'filters/decrypting_audio_decoder.cc',
        'filters/decrypting_audio_decoder.h',
        'filters/decrypting_demuxer_stream.cc',
        'filters/decrypting_demuxer_stream.h',
        'filters/decrypting_video_decoder.cc',
        'filters/decrypting_video_decoder.h',
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
        'filters/opus_audio_decoder.cc',
        'filters/opus_audio_decoder.h',
        'filters/skcanvas_video_renderer.cc',
        'filters/skcanvas_video_renderer.h',
        'filters/shell_au.cc',
        'filters/shell_au.h',
        'filters/shell_audio_decoder_impl.cc',
        'filters/shell_audio_decoder_impl.h',
        'filters/shell_audio_renderer.h',
        'filters/shell_audio_renderer_impl.cc',
        'filters/shell_audio_renderer_impl.h',
        'filters/shell_avc_parser.cc',
        'filters/shell_avc_parser.h',
        'filters/shell_demuxer.cc',
        'filters/shell_demuxer.h',
        'filters/shell_flv_parser.cc',
        'filters/shell_flv_parser.h',
        'filters/shell_mp4_map.cc',
        'filters/shell_mp4_map.h',
        'filters/shell_mp4_parser.cc',
        'filters/shell_mp4_parser.h',
        'filters/shell_parser.cc',
        'filters/shell_parser.h',
        'filters/shell_raw_audio_decoder_stub.cc',
        'filters/shell_raw_audio_decoder_stub.h',
        'filters/shell_raw_video_decoder_stub.cc',
        'filters/shell_raw_video_decoder_stub.h',
        'filters/shell_rbsp_stream.cc',
        'filters/shell_rbsp_stream.h',
        'filters/shell_video_decoder_impl.cc',
        'filters/shell_video_decoder_impl.h',
        'filters/source_buffer_stream.cc',
        'filters/source_buffer_stream.h',
        'filters/video_decoder_selector.cc',
        'filters/video_decoder_selector.h',
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
        'video/capture/win/capability_list_win.cc',
        'video/capture/win/capability_list_win.h',
        'video/capture/win/filter_base_win.cc',
        'video/capture/win/filter_base_win.h',
        'video/capture/win/pin_base_win.cc',
        'video/capture/win/pin_base_win.h',
        'video/capture/win/sink_filter_observer_win.h',
        'video/capture/win/sink_filter_win.cc',
        'video/capture/win/sink_filter_win.h',
        'video/capture/win/sink_input_pin_win.cc',
        'video/capture/win/sink_input_pin_win.h',
        'video/capture/win/video_capture_device_mf_win.cc',
        'video/capture/win/video_capture_device_mf_win.h',
        'video/capture/win/video_capture_device_win.cc',
        'video/capture/win/video_capture_device_win.h',
        'video/picture.cc',
        'video/picture.h',
        'video/video_decode_accelerator.cc',
        'video/video_decode_accelerator.h',
        'webm/webm_audio_client.cc',
        'webm/webm_audio_client.h',
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
        'webm/webm_video_client.cc',
        'webm/webm_video_client.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['arm_neon == 1', {
          'defines': [
            'USE_NEON'
          ],
        }],
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
            'filters/audio_file_reader.cc',
            'filters/audio_file_reader.h',
            'filters/chunk_demuxer.cc',
            'filters/chunk_demuxer.h',
            'filters/blocking_url_protocol.cc',
            'filters/blocking_url_protocol.h',
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
          ],
        }],
        ['OS == "lb_shell" or OS == "starboard"', {
          'include_dirs': [
            '<(DEPTH)/../..', # for our explicit external/ inclusion of headers
          ],
          'sources/': [
            ['exclude', 'android'],
            # media is not excluding windows sources automatically.
            # forcibly exclude them.
            ['exclude', 'win'],
            ['exclude', 'audio/audio_device_thread'],
            ['exclude', 'audio/audio_input_stream_impl'],
            ['exclude', 'audio/audio_io.h'],
            ['exclude', 'audio/audio_input_controller'],
            ['exclude', 'audio/audio_input_device'],
            ['exclude', 'audio/audio_manager'],
            ['exclude', 'audio/audio_manager_base'],
            ['exclude', 'audio/audio_output_controller'],
            ['exclude', 'audio/audio_output_device'],
            ['exclude', 'audio/audio_output_dispatcher'],
            ['exclude', 'audio/audio_output_dispatcher_impl'],
            ['exclude', 'audio/audio_output_ipc'],
            ['exclude', 'audio/audio_output_proxy'],
            ['exclude', 'audio/audio_output_resampler'],
            ['exclude', 'audio/cross_process_notification'],
            ['exclude', 'audio/cross_process_notification_posix'],
            ['exclude', 'audio/cross_process_notification_win'],
            ['exclude', 'audio/fake_audio_input_stream'],
            ['exclude', 'audio/fake_audio_output_stream'],
            ['exclude', 'audio/ios/audio_manager_ios'],
            ['exclude', 'audio/linux/alsa_input'],
            ['exclude', 'audio/linux/alsa_output'],
            ['exclude', 'audio/linux/alsa_util'],
            ['exclude', 'audio/linux/alsa_wrapper'],
            ['exclude', 'audio/linux/audio_manager_linux'],
            ['exclude', 'audio/linux/cras_input'],
            ['exclude', 'audio/linux/cras_output'],
            ['exclude', 'audio/mac/audio_input_mac'],
            ['exclude', 'audio/mac/audio_low_latency_input_mac'],
            ['exclude', 'audio/mac/audio_low_latency_output_mac'],
            ['exclude', 'audio/mac/audio_manager_mac'],
            ['exclude', 'audio/mac/audio_output_mac'],
            ['exclude', 'audio/mac/audio_synchronized_mac'],
            ['exclude', 'audio/mac/audio_unified_mac'],
            ['exclude', 'audio/openbsd/audio_manager_openbsd'],
            ['exclude', 'audio/pulse/pulse_output'],
            ['exclude', 'audio/scoped_loop_observer'],
            ['exclude', 'audio/simple_sources'],
            ['exclude', 'audio/virtual_audio_input_stream'],
            ['exclude', 'audio/virtual_audio_output_stream'],
            ['exclude', 'audio/win/audio_device_listener_win'],
            ['exclude', 'audio/win/audio_low_latency_input_win'],
            ['exclude', 'audio/win/audio_low_latency_output_win'],
            ['exclude', 'audio/win/audio_manager_win'],
            ['exclude', 'audio/win/audio_unified_win'],
            ['exclude', 'audio/win/avrt_wrapper_win'],
            ['exclude', 'audio/win/device_enumeration_win'],
            ['exclude', 'audio/win/core_audio_util_win'],
            ['exclude', 'audio/win/wavein_input_win'],
            ['exclude', 'audio/win/waveout_output_win'],
            # we use hardware mixers
            ['exclude', 'base/audio_converter'],
            ['exclude', 'base/audio_renderer_mixer'],
            ['exclude', 'base/channel_mixer'],
            # we use our own encryption
            ['exclude', 'crypto/aes'],
            # we use our own AudioRendererAlgorithm and Impl
            ['exclude', 'filters/audio_renderer'],
            # but re-use various other parts of the stack
            ['include', 'filters/chunk_demuxer'],
            ['exclude', 'filters/decrypting_audio_decoder'],
            ['exclude', 'filters/decrypting_video_decoder'],
            # we stream from network only
            ['exclude', 'filters/file_data_source'],
            # gpu-based decoding is interesting, perhaps explore further
            ['exclude', 'filters/gpu'],
            # avc/aac only (for now)
            ['exclude', 'filters/opus'],
            ['exclude', 'video/capture/fake_video_capture_device'],
          ],
          'conditions': [
            ['target_arch=="xb1" and actual_target_arch!="win"', {
              'dependencies' : [
                '../third_party/modp_b64/modp_b64.gyp:modp_b64',
                '<(lbshell_root)/build/projects/shell_scheme_handler.gyp:shell_scheme_handler',
              ],
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'ComponentExtensions': 'true'
                },
              },
            }],
            ['target_arch=="linux"', {
              'sources': [
                'audio/shell_audio_streamer_linux.cc',
                'audio/shell_audio_streamer_linux.h',
                'audio/shell_pulse_audio.cc',
                'audio/shell_pulse_audio.h',
                'filters/shell_ffmpeg.cc',
                'filters/shell_ffmpeg.h',
                'filters/shell_raw_audio_decoder_linux.cc',
                'filters/shell_raw_audio_decoder_linux.h',
                'filters/shell_raw_video_decoder_linux.cc',
                'filters/shell_raw_video_decoder_linux.h',
              ],
            }],
            ['target_arch=="ps3"', {
              'sources': [
                'base/shell_cached_decoder_buffer.cc',
                'base/shell_cached_decoder_buffer.h',
                'audio/shell_audio_streamer_ps3.cc',
                'audio/shell_audio_streamer_ps3.h',
                'audio/shell_audio_streamer_v2_ps3.cc',
                'audio/shell_audio_streamer_v2_ps3.h',
                'audio/shell_spurs_ps3.cc',
                'audio/shell_spurs_ps3.h',
                'filters/shell_audio_decoder_ps3.cc',
                'filters/shell_audio_decoder_ps3.h',
                'filters/shell_audio_renderer_ps3.cc',
                'filters/shell_audio_renderer_ps3.h',
                'filters/shell_audio_resampler_ps3.cc',
                'filters/shell_audio_resampler_ps3.h',
                'filters/shell_raw_audio_decoder_ps3.cc',
                'filters/shell_raw_audio_decoder_ps3.h',
                'filters/shell_raw_video_decoder_ps3.cc',
                'filters/shell_raw_video_decoder_ps3.h',
                'filters/shell_vdec_helper_ps3.cc',
                'filters/shell_vdec_helper_ps3.h',
              ],
              'sources/': [
                # PS3 has its own implementations.
                ['exclude', 'filters/shell_audio_renderer_impl'],
              ],
              'dependencies' : [
                'spurs_tasks',
              ],
            }],
            ['target_arch=="ps4"', {
              'sources': [
                'audio/shell_audio_streamer_ps4.cc',
                'filters/shell_raw_audio_decoder_ps4.cc',
                'filters/shell_raw_audio_decoder_ps4.h',
                'filters/shell_raw_avc_decoder_ps4.cc',
                'filters/shell_raw_avc_decoder_ps4.h',
                'filters/shell_raw_video_decoder_ps4.cc',
                'filters/shell_raw_video_decoder_ps4.h',
                'filters/shell_raw_vp9_decoder_ps4.cc',
                'filters/shell_raw_vp9_decoder_ps4.h',
              ],
              'dependencies' : [
                '<(DEPTH)/third_party/libvpx_gpu/libvpx_gpu.gyp:libvpx_gpu',
              ],
            }],
            ['target_arch=="xb1" or target_arch=="xb360"', {
              'sources/': [
                # These platforms have their own implementations.
                ['exclude', 'filters/shell_audio_decoder_impl'],
                ['exclude', 'filters/shell_audio_renderer_impl'],
                ['exclude', 'filters/shell_video_decoder_impl'],
              ]
            }],
            ['use_widevine==1', {
              'sources': [
                'crypto/shell_widevine_decryptor.cc',
                'crypto/shell_widevine_decryptor.h',
              ],
              'conditions': [
                ['cobalt==1', {
                  'include_dirs': [
                    '../third_party/cdm',
                  ],
                  'dependencies': [
                    'crypto/widevine_cobalt.gyp:wvcdm_static',
                  ],
                }, {  #cobalt!=1
                  'include_dirs': [
                    '../../cdm',
                  ],
                  'dependencies': [
                    'crypto/widevine_steel.gyp:wvcdm_static',
                  ],
                }],
              ],
            }],
            ['cobalt==1', {
              'dependencies': [
                '../googleurl/googleurl.gyp:googleurl',
              ],
              'sources': [
                'player/buffered_data_source.h',
                'player/can_play_type.cc',
                'player/can_play_type.h',
                'player/crypto/key_systems.h',
                'player/crypto/key_systems.cc',
                'player/crypto/proxy_decryptor.cc',
                'player/crypto/proxy_decryptor.h',
                'player/mime_util.cc',
                'player/mime_util.h',
                'player/mime_util_certificate_type_list.h',
                'player/web_media_player.h',
                'player/web_media_player_delegate.h',
                'player/web_media_player_impl.cc',
                'player/web_media_player_impl.h',
                'player/web_media_player_proxy.cc',
                'player/web_media_player_proxy.h',
              ],
              'sources!': [
                'filters/decrypting_audio_decoder.cc',
                'filters/decrypting_audio_decoder.h',
                'filters/decrypting_video_decoder.cc',
                'filters/decrypting_video_decoder.h',
              ],
            }],
          ],
        }, {  # OS != "lb_shell" and OS != "starboard"
          'dependencies': [
            'yuv_convert',
            '../third_party/openmax/openmax.gyp:il',
            '../third_party/opus/opus.gyp:opus',
          ],
          'sources/': [
            ['exclude', 'shell_'],
          ],
        }],  # OS != "lb_shell" and OS != "starboard"
        ['OS == "lb_shell"', {
          'include_dirs': [
            '<(lbshell_root)/src/platform/<(target_arch)/chromium',
            '<(lbshell_root)/src/platform/<(target_arch)/lb_shell',
          ],
          'dependencies': [
            '<(lbshell_root)/build/projects/posix_emulation.gyp:posix_emulation',
          ],
          'conditions': [
            ['(target_arch=="xb1" and actual_target_arch!="win") or target_arch=="xb360"', {
              'sources': [
                '<!@(find <(lbshell_root)/src/platform/<(target_arch)/chromium/media -type f)',
              ],
            }],
          ],
        }],  # OS == "lb_shell"
        ['OS == "starboard"', {
          'dependencies': [
            '<(DEPTH)/starboard/starboard.gyp:starboard',
          ],
          'sources': [
            'audio/shell_audio_streamer_starboard.cc',
            'base/sbplayer_pipeline.cc',
            'base/shell_cached_decoder_buffer.cc',
            'base/shell_cached_decoder_buffer.h',
            'crypto/starboard_decryptor.cc',
            'crypto/starboard_decryptor.h',
          ],
          'sources/': [
            ['exclude', '^base/pipeline_impl.cc'],
          ],
        }],  # OS == "starboard"
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
              '$(SDKROOT)/System/Library/Frameworks/AVFoundation.framework',
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
          'link_settings':  {
            'libraries': [
              '-lmf.lib',
              '-lmfplat.lib',
              '-lmfreadwrite.lib',
              '-lmfuuid.lib',
            ],
          },
          # Specify delayload for media.dll.
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'mf.dll',
                'mfplat.dll',
                'mfreadwrite.dll',
              ],
            },
          },
          # Specify delayload for components that link with media.lib.
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  'mf.dll',
                  'mfplat.dll',
                  'mfreadwrite.dll',
                ],
              },
            },
          },
        }],
        ['proprietary_codecs==1 or branding=="Chrome" or OS=="lb_shell" or OS=="starboard"', {
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
        'audio/fake_audio_output_stream_unittest.cc',
        'audio/ios/audio_manager_ios_unittest.cc',
        'audio/linux/alsa_output_unittest.cc',
        'audio/mac/audio_low_latency_input_mac_unittest.cc',
        'audio/mac/audio_output_mac_unittest.cc',
        'audio/simple_sources_unittest.cc',
        'audio/virtual_audio_input_stream_unittest.cc',
        'audio/virtual_audio_output_stream_unittest.cc',
        'audio/win/audio_device_listener_win_unittest.cc',
        'audio/win/audio_low_latency_input_win_unittest.cc',
        'audio/win/audio_low_latency_output_win_unittest.cc',
        'audio/win/audio_output_win_unittest.cc',
        'audio/win/audio_unified_win_unittest.cc',
        'audio/win/core_audio_util_win_unittest.cc',
        'base/audio_bus_unittest.cc',
        'base/audio_converter_unittest.cc',
        'base/audio_fifo_unittest.cc',
        'base/audio_pull_fifo_unittest.cc',
        'base/audio_renderer_mixer_input_unittest.cc',
        'base/audio_renderer_mixer_unittest.cc',
        'base/audio_splicer_unittest.cc',
        'base/audio_timestamp_helper_unittest.cc',
        'base/bit_reader_unittest.cc',
        'base/bind_to_loop_unittest.cc',
        'base/buffers_unittest.cc',
        'base/channel_mixer_unittest.cc',
        'base/clock_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/decoder_buffer_unittest.cc',
        'base/djb2_unittest.cc',
        'base/filter_collection_unittest.cc',
        'base/gmock_callback_support_unittest.cc',
        'base/interleaved_sinc_resampler_unittest.cc',
        'base/multi_channel_resampler_unittest.cc',
        'base/pipeline_impl_unittest.cc',
        'base/ranges_unittest.cc',
        'base/run_all_unittests.cc',
        'base/seekable_buffer_unittest.cc',
        'base/shell_audio_bus_test.cc',
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
        'filters/audio_decoder_selector_unittest.cc',
        'filters/audio_file_reader_unittest.cc',
        'filters/audio_renderer_algorithm_unittest.cc',
        'filters/audio_renderer_impl_unittest.cc',
        'filters/blocking_url_protocol_unittest.cc',
        'filters/chunk_demuxer_unittest.cc',
        'filters/decrypting_audio_decoder_unittest.cc',
        'filters/decrypting_demuxer_stream_unittest.cc',
        'filters/decrypting_video_decoder_unittest.cc',
        'filters/ffmpeg_audio_decoder_unittest.cc',
        'filters/ffmpeg_demuxer_unittest.cc',
        'filters/ffmpeg_glue_unittest.cc',
        'filters/ffmpeg_h264_to_annex_b_bitstream_converter_unittest.cc',
        'filters/ffmpeg_video_decoder_unittest.cc',
        'filters/file_data_source_unittest.cc',
        'filters/h264_to_annex_b_bitstream_converter_unittest.cc',
        'filters/pipeline_integration_test.cc',
        'filters/pipeline_integration_test_base.cc',
        'filters/skcanvas_video_renderer_unittest.cc',
        'filters/source_buffer_stream_unittest.cc',
        'filters/video_decoder_selector_unittest.cc',
        'filters/video_renderer_base_unittest.cc',
        'video/capture/video_capture_device_unittest.cc',
        'webm/cluster_builder.cc',
        'webm/cluster_builder.h',
        'webm/webm_cluster_parser_unittest.cc',
        'webm/webm_content_encodings_client_unittest.cc',
        'webm/webm_parser_unittest.cc',
      ],
      'conditions': [
        ['arm_neon == 1', {
          'defines': [
            'USE_NEON'
          ],
        }],
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
        ['OS=="android" or OS=="lb_shell" or OS=="starboard"', {
          'sources!': [
            'audio/audio_input_volume_unittest.cc',
            'base/test_data_util.cc',
            'base/test_data_util.h',
            'ffmpeg/ffmpeg_common_unittest.cc',
            'filters/audio_file_reader_unittest.cc',
            'filters/blocking_url_protocol_unittest.cc',
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
        ['proprietary_codecs==1 or branding=="Chrome" or OS=="lb_shell" or OS=="starboard"', {
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
        ['OS == "lb_shell" or OS=="starboard"', {
          'sources': [
            'audio/mock_shell_audio_streamer.h',
            'audio/shell_audio_sink_unittest.cc',
            'base/mock_shell_data_source_reader.h',
            'base/shell_buffer_factory_unittest.cc',
            'filters/shell_audio_renderer_unittest.cc',
            'filters/shell_mp4_map_unittest.cc',
            'filters/shell_rbsp_stream_unittest.cc',
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
        'base/gmock_callback_support.h',
        'base/mock_audio_renderer_sink.cc',
        'base/mock_audio_renderer_sink.h',
        'base/mock_data_source_host.cc',
        'base/mock_data_source_host.h',
        'base/mock_demuxer_host.cc',
        'base/mock_demuxer_host.h',
        'base/mock_filters.cc',
        'base/mock_filters.h',
        'base/test_helpers.cc',
        'base/test_helpers.h',
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
          'conditions': [
            ['OS == "lb_shell"', {
              'type': 'static_library',
            }],
          ],
        },
        {
          'target_name': 'yuv_convert',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'conditions': [
            [ 'target_arch == "ia32" or target_arch == "x64"', {
              'dependencies': [
                'yuv_convert_simd_x86',
              ],
            }],
            [ 'target_arch == "arm" or target_arch == "mipsel"', {
              'dependencies': [
                'yuv_convert_simd_c',
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
                'conditions': [
                  [ 'target_arch=="ia32"', {
                    'yasm_flags': [
                      '-DPREFIX',
                      '-DMACHO',
                      '-DCHROMIUM',
                      '-Isimd',
                    ],
                  }, {
                    'yasm_flags': [
                      '-DPREFIX',
                      '-DARCH_X86_64',
                      '-DMACHO',
                      '-DCHROMIUM',
                      '-Isimd',
                    ],
                  }],
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
          # Comment this out so we no longer depends on yasm_compile.gypi.
          # Choose to comment instead of removing so we wouldn't get lost if we
          # ever want to get it back.
          # 'includes': [
          #  '../third_party/yasm/yasm_compile.gypi',
          # ],
        },
        {
          'target_name': 'yuv_convert_simd_c',
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
        {
          'target_name': 'demuxer_bench',
          'type': 'executable',
          'dependencies': [
            'media',
            '../base/base.gyp:base',
          ],
          'sources': [
            'tools/demuxer_bench/demuxer_bench.cc',
          ],
        },
      ],
    }],
    ['(OS == "win" or toolkit_uses_gtk == 1) and use_aura != 1', {
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
    ['OS == "linux" and target_arch != "arm" and target_arch != "mipsel"', {
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
    ['(OS == "lb_shell" or OS == "starboard") and target_arch == "ps3"', {
      'targets': [
        {
          # this target builds SPU task objects into a PPU-linkable static library
          # that is then linked with the main executable.
          #
          # HORRIBLENESS WARNING:
          # the python script in the rule action contains compiler and linker
          # flags and determines build configuration based on the presence of
          # the string 'Debug' in the output directory. I'm very sorry.
          # TODO: refactor script to take more defines from the command line..
          'target_name': 'spurs_tasks',
          'type': 'static_library',
          'sources': [
            '<(INTERMEDIATE_DIR)/sinc_resampler.o',
          ],
          'actions': [
            {
              'action_name' : 'spurs_tasks_builder',
              'variables': {
                'spu_source': [
                  'audio/spurs/tasks/sinc_resampler.c',
                ],
                'spu_includes': [
                  'audio/spurs/tasks/spu_log.h',
                ],
              },
              'inputs' : [
                '<@(spu_source)',
                '<@(spu_includes)',
              ],
              'outputs' : [
                '<(INTERMEDIATE_DIR)/sinc_resampler.o',
              ],
              'action': [
                'python',
                'audio/spurs/scripts/spu_task_to_ppu_obj.py',
                '<(INTERMEDIATE_DIR)',
                '<(SHARED_INTERMEDIATE_DIR)/spu',
                '<@(spu_source)',
              ],
              'msvs_cygwin_shell' : 1,
            },
          ],
          'dependencies': [
            'sinc_resampler_table'
          ],
        },
        {
          'target_name': 'sinc_resampler_table',
          'type': 'none',
          'hard_dependency': 1,
          'actions': [
            {
              'action_name': 'generate_sinc_resampler_table',
              'inputs': [
                'audio/spurs/scripts/build_sinc_table.py',
              ],
              'outputs': [
                # please keep this list to just this one file
                '<(SHARED_INTERMEDIATE_DIR)/spu/sinc_table.c',
              ],
              'action': ['python', 'audio/spurs/scripts/build_sinc_table.py', '<@(_outputs)'],
              'message': 'generating sinc resampler table in <@(_outputs)',
              'msvs_cygwin_shell' : 1,
            },
          ],
        },
      ],
    }],
    ['os_posix == 1 and OS != "mac" and OS != "android" and OS != "lb_shell"', {
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
          'target_name': 'media_player_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_dir': 'media',
            'input_java_class': 'android/media/MediaPlayer.class',
            'input_jar_file': '<(android_sdk)/android.jar',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'player_android_jni_headers',
          'type': 'none',
          'dependencies': [
            'media_player_jni_headers',
          ],
          'sources': [
            'base/android/java/src/org/chromium/media/AudioFocusBridge.java',
            'base/android/java/src/org/chromium/media/AudioTrackBridge.java',
            'base/android/java/src/org/chromium/media/MediaCodecBridge.java',
            'base/android/java/src/org/chromium/media/MediaDrmBridge.java',
            'base/android/java/src/org/chromium/media/MediaPlayerBridge.java',
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
            'base/android/audio_focus_bridge.cc',
            'base/android/audio_focus_bridge.h',
            'base/android/audio_track_bridge.cc',
            'base/android/audio_track_bridge.h',
            'base/android/media_codec_bridge.cc',
            'base/android/media_codec_bridge.h',
            'base/android/media_drm_bridge.cc',
            'base/android/media_drm_bridge.h',
            'base/android/media_jni_registrar.cc',
            'base/android/media_jni_registrar.h',
            'base/android/media_player_bridge.cc',
            'base/android/media_player_bridge.h',
            'base/android/media_player_listener.cc',
            'base/android/media_player_listener.h',
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
    ['OS != "android" and OS != "ios" and OS != "lb_shell" and OS != "starboard"', {
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
