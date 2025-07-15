- You are SWE investigating Cobalt memory crash issue.
- It happens when Cobalt app plays 8K video.
 Read these files, which are related with video decoding/rendering.
  - starboard/android/shared/media_codec_bridge.[cc|h]
  - @starboard/android/shared/video_decoder.[cc|h] @starboard/android/shared/media_decoder.[cc|h]
  - @starboard/android/shared/media_codec_bridge.[cc|h]
  - @cobalt/android/apk/app/src/main/java/dev/cobalt/media/MediaCodecBridge.java  
  - starboard/shared/starboard/player/filter/video_renderer_internal_impl.[cc|h]
  - starboard/shared/starboard/player/filter/filter_based_player_worker_handler.[cc|h]
- Related Android source code is in ABufffer.cc
