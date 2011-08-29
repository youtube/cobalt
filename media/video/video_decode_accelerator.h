// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/picture.h"
#include "ui/gfx/size.h"

namespace media {

// Enumeration defining global dictionary ranges for various purposes that are
// used to handle the configurations of the video decoder.
//
// IMPORTANT! Dictionary keys and corresponding values MUST match the ones found
// in Pepper API dictionary for video (ppapi/c/dev/pp_video_dev.h)!
enum VideoAttributeKey {
  VIDEOATTRIBUTEKEY_TERMINATOR = 0,

  VIDEOATTRIBUTEKEY_BITSTREAM_FORMAT_BASE = 0x100,
  // Array of key/value pairs describing video configuration.
  // It could include any keys from PP_VideoKey. Its last element shall be
  // VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_NONE with no corresponding value.
  // An example:
  // {
  //   VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_FOURCC,      PP_VIDEODECODECID_VP8,
  //   VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_VP8_PROFILE,  (VP8PROFILE_1 |
  //                                                    VP8PROFILE_2 |
  //                                                    VP8PROFILE_3),
  //   VIDEOATTRIBUTEKEY_TERMINATOR
  // };
  // Keys for defining video bitstream format.
  // Value is type of PP_VideoCodecFourcc. Commonly known attributes values are
  // defined in PP_VideoCodecFourcc enumeration.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_FOURCC,
  // Bitrate in bits/s. Attribute value is 32-bit unsigned integer.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_BITRATE,
  // Width and height of the input video bitstream, if known by the application.
  // Decoder will expect the bitstream to match these values and does memory
  // considerations accordingly.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_WIDTH,
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_HEIGHT,
  // Following attributes are applicable only in case of VP8.
  // Key for VP8 profile attribute. Attribute value is bitmask of flags defined
  // in PP_VP8Profile_Dev enumeration.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_VP8_PROFILE,
  // Number of partitions per picture. Attribute value is unsigned 32-bit
  // integer.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_VP8_NUM_OF_PARTITIONS,
  // Following attributes are applicable only in case of H.264.
  // Value is bitmask collection from the flags defined in PP_H264Profile.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_PROFILE,
  // Value is type of PP_H264Level.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_LEVEL,
  // Value is type of PP_H264PayloadFormat_Dev.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_PAYLOADFORMAT,
  // Subset for H.264 features, attribute value 0 signifies unsupported.
  // This is needed in case decoder has partial support for certain profile.
  // Default for features are enabled if they're part of supported profile.
  // H264 tool called Flexible Macroblock Ordering.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_FEATURE_FMO,
  // H264 tool called Arbitrary Slice Ordering.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_FEATURE_ASO,
  // H264 tool called Interlacing.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_FEATURE_INTERLACE,
  // H264 tool called Context-Adaptive Binary Arithmetic Coding.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_FEATURE_CABAC,
  // H264 tool called Weighted Prediction.
  VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_FEATURE_WEIGHTEDPREDICTION,

  VIDEOATTRIBUTEKEY_COLOR_FORMAT_BASE = 0x1000,
  // This specifies the output color format for a decoded frame. Value is one
  // of the values in VideoColorFormat enumeration.
  VIDEOATTRIBUTEKEY_VIDEOCOLORFORMAT,
};

enum VideoCodecFourcc {
  VIDEOCODECFOURCC_NONE = 0,
  VIDEOCODECFOURCC_VP8 = 0x00385056,  // a.k.a. Fourcc 'VP8\0'.
  VIDEOCODECFOURCC_H264 = 0x31637661,  // a.k.a. Fourcc 'avc1'.
};

// VP8 specific information to be carried over the APIs.
// Enumeration for flags defining supported VP8 profiles.
enum VP8Profile {
  VP8PROFILE_NONE = 0,
  VP8PROFILE_0 = 1,
  VP8PROFILE_1 = 1 << 1,
  VP8PROFILE_2 = 1 << 2,
  VP8PROFILE_3 = 1 << 3,
};

// H.264 specific information to be carried over the APIs.
// Enumeration for flags defining supported H.264 profiles.
enum H264Profile {
  H264PROFILE_NONE = 0,
  H264PROFILE_BASELINE = 1,
  H264PROFILE_MAIN = 1 << 2,
  H264PROFILE_EXTENDED = 1 << 3,
  H264PROFILE_HIGH = 1 << 4,
  H264PROFILE_HIGH10PROFILE = 1 << 5,
  H264PROFILE_HIGH422PROFILE = 1 << 6,
  H264PROFILE_HIGH444PREDICTIVEPROFILE = 1 << 7,
  H264PROFILE_SCALABLEBASELINE = 1 << 8,
  H264PROFILE_SCALABLEHIGH = 1 << 9,
  H264PROFILE_STEREOHIGH = 1 << 10,
  H264PROFILE_MULTIVIEWHIGH = 1 << 11,
};

// Enumeration for defining H.264 level of decoder implementation.
enum H264Level {
  H264LEVEL_NONE = 0,
  H264LEVEL_10 = 1,
  H264LEVEL_1B = H264LEVEL_10 | 1 << 1,
  H264LEVEL_11 = H264LEVEL_1B | 1 << 2,
  H264LEVEL_12 = H264LEVEL_11 | 1 << 3,
  H264LEVEL_13 = H264LEVEL_12 | 1 << 4,
  H264LEVEL_20 = H264LEVEL_13 | 1 << 5,
  H264LEVEL_21 = H264LEVEL_20 | 1 << 6,
  H264LEVEL_22 = H264LEVEL_21 | 1 << 7,
  H264LEVEL_30 = H264LEVEL_22 | 1 << 8,
  H264LEVEL_31 = H264LEVEL_30 | 1 << 9,
  H264LEVEL_32 = H264LEVEL_31 | 1 << 10,
  H264LEVEL_40 = H264LEVEL_32 | 1 << 11,
  H264LEVEL_41 = H264LEVEL_40 | 1 << 12,
  H264LEVEL_42 = H264LEVEL_41 | 1 << 13,
  H264LEVEL_50 = H264LEVEL_42 | 1 << 14,
  H264LEVEL_51 = H264LEVEL_50 | 1 << 15,
};

// Enumeration to describe which payload format is used within the exchanged
// bitstream buffers.
enum H264PayloadFormat {
  H264PAYLOADFORMAT_NONE = 0,
  // NALUs separated by Start Code.
  H264PAYLOADFORMAT_BYTESTREAM = 1,
  // Exactly one raw NALU per buffer.
  H264PAYLOADFORMAT_ONE_NALU_PER_BUFFER = 1 << 1,
  // NALU separated by 1-byte interleaved length field.
  H264PAYLOADFORMAT_ONE_BYTE_INTERLEAVED_LENGTH = 1 << 2,
  // NALU separated by 2-byte interleaved length field.
  H264PAYLOADFORMAT_TWO_BYTE_INTERLEAVED_LENGTH = 1 << 3,
  // NALU separated by 4-byte interleaved length field.
  H264PAYLOADFORMAT_FOUR_BYTE_INTERLEAVED_LENGTH = 1 << 4,
};

// Enumeration for various color formats.
enum VideoColorFormat {
  // Value represents 32-bit RGBA format where each component is 8-bit in order
  // R-G-B-A. Regardless of endianness of the architecture color components are
  // stored in this order in the memory.
  VIDEOCOLORFORMAT_RGBA = 0,
};

// Video decoder interface.
// This interface is extended by the various components that ultimately
// implement the backend of PPB_VideoDecode_Dev.
//
// No thread-safety guarantees are implied by the use of RefCountedThreadSafe
// below.
class MEDIA_EXPORT VideoDecodeAccelerator
    : public base::RefCountedThreadSafe<VideoDecodeAccelerator> {
 public:
  // Enumeration of potential errors generated by the API.
  // Note: Keep these in sync with PP_VideoDecodeError_Dev.
  enum Error {
    // An operation was attempted during an incompatible decoder state.
    ILLEGAL_STATE = 1,
    // Invalid argument was passed to an API method.
    INVALID_ARGUMENT,
    // Encoded input is unreadable.
    UNREADABLE_INPUT,
    // A failure occurred at the browser layer or one of its dependencies.
    // Examples of such failures include GPU hardware failures, GPU driver
    // failures, GPU library failures, browser programming errors, and so on.
    PLATFORM_FAILURE,
  };

  // Interface for collaborating with picture interface to provide memory for
  // output picture and blitting them.
  // This interface is extended by the various layers that relay messages back
  // to the plugin, through the PPP_VideoDecode_Dev interface the plugin
  // implements.
  class Client {
   public:
    virtual ~Client() {}

    // Callback to notify client that decoder has been initialized.
    virtual void NotifyInitializeDone() = 0;

    // Callback to tell client how many and what size of buffers to provide.
    virtual void ProvidePictureBuffers(
        uint32 requested_num_of_buffers, const gfx::Size& dimensions) = 0;

    // Callback to dismiss picture buffer that was assigned earlier.
    virtual void DismissPictureBuffer(int32 picture_buffer_id) = 0;

    // Callback to deliver decoded pictures ready to be displayed.
    virtual void PictureReady(const Picture& picture) = 0;

    // Callback to notify that decoder has decoded end of stream marker and has
    // outputted all displayable pictures.
    virtual void NotifyEndOfStream() = 0;

    // Callback to notify that decoded has decoded the end of the current
    // bitstream buffer.
    virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) = 0;

    // Flush completion callback.
    virtual void NotifyFlushDone() = 0;

    // Reset completion callback.
    virtual void NotifyResetDone() = 0;

    // Callback to notify about decoding errors.
    virtual void NotifyError(Error error) = 0;
  };

  // Video decoder functions.

  // Initializes the video decoder with specific configuration.
  // Parameters:
  //  |config| is the configuration on which the decoder should be initialized.
  //
  // Returns true when command successfully accepted. Otherwise false.
  virtual bool Initialize(const std::vector<int32>& config) = 0;

  // Decodes given bitstream buffer. Once decoder is done with processing
  // |bitstream_buffer| it will call NotifyEndOfBitstreamBuffer() with the
  // bitstream buffer id.
  // Parameters:
  //  |bitstream_buffer| is the input bitstream that is sent for decoding.
  virtual void Decode(const BitstreamBuffer& bitstream_buffer) = 0;

  // Assigns a set of texture-backed picture buffers to the video decoder.
  //
  // Ownership of each picture buffer remains with the client, but the client
  // is not allowed to deallocate the buffer before the DismissPictureBuffer
  // callback has been initiated for a given buffer.
  //
  // Parameters:
  //  |buffers| contains the allocated picture buffers for the output.
  virtual void AssignPictureBuffers(
      const std::vector<PictureBuffer>& buffers) = 0;

  // Sends picture buffers to be reused by the decoder. This needs to be called
  // for each buffer that has been processed so that decoder may know onto which
  // picture buffers it can write the output to.
  //
  // Parameters:
  //  |picture_buffer_id| id of the picture buffer that is to be reused.
  virtual void ReusePictureBuffer(int32 picture_buffer_id) = 0;

  // Flushes the decoder: all pending inputs will be decoded and pictures handed
  // back to the client, followed by NotifyFlushDone() being called on the
  // client.  Can be used to implement "end of stream" notification.
  virtual void Flush() = 0;

  // Resets the decoder: all pending inputs are dropped immediately and the
  // decoder returned to a state ready for further Decode()s, followed by
  // NotifyResetDone() being called on the client.  Can be used to implement
  // "seek".
  virtual void Reset() = 0;

  // Destroys the decoder: all pending inputs are dropped immediately and the
  // component is freed.  This call may asynchornously free system resources,
  // but its client-visible effects are synchronous.  After this method returns
  // no more callbacks will be made on the client.
  virtual void Destroy() = 0;

 protected:
  friend class base::RefCountedThreadSafe<VideoDecodeAccelerator>;
  virtual ~VideoDecodeAccelerator();
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_
