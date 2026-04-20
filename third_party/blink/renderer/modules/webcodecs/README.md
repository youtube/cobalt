# WebCodecs API

This directory will contain the implementation of
https://github.com/WICG/web-codecs/, which is a low-level API for encode and
decode of audio and video.

It will use the existing codec implementations in src/media used by the video
stack, WebRTC, and MediaRecorder, such as media::DecoderFactory,
media::VideoEncodeAccelerator, and media::VideoFrame.