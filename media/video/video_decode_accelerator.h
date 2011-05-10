// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "media/base/bitstream_buffer.h"
#include "ui/gfx/size.h"

namespace media {

typedef Callback0::Type VideoDecodeAcceleratorCallback;

// Enumeration defining global dictionary ranges for various purposes that are
// used to handle the configurations of the video decoder.
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

  VIDEOATTRIBUTEKEY_COLORFORMAT_BASE = 0x1000,
  // Keys for definining attributes of a color buffer. Using these attributes
  // users can define color spaces in terms of red, green, blue and alpha
  // components as well as with combination of luma and chroma values with
  // different subsampling schemes. Also planar, semiplanar and interleaved
  // formats can be described by using the provided keys as instructed.
  //
  // Rules for describing the color planes (1 or more) that constitute the whole
  // picture are:
  //   1. Each plane starts with VIDEOATTRIBUTEKEY_COLORFORMAT_PLANE_PIXEL_SIZE
  //   attribute telling how many bits per pixel the plane contains.
  //   2. VIDEOATTRIBUTEKEY_COLORFORMAT_PLANE_PIXEL_SIZE attribute must be
  //   followed either by
  //     a. Red, green and blue components followed optionally by alpha size
  //     attribute.
  //     OR
  //     b. Luma, blue difference chroma and red difference chroma components as
  //     well as three sampling reference factors that tell how the chroma may
  //     have been subsampled with respect to luma.
  //   3. Description must be terminated with VIDEOATTRIBUTEKEY_COLORFORMAT_NONE
  //   key with no value for attribute.
  //
  // For example, semiplanar YCbCr 4:2:2 (2 planes, one containing 8-bit luma,
  // the other containing two interleaved chroma data components) may be
  // described with the following attributes:
  // {
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_PLANE_PIXEL_SIZE, 8,
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_LUMA_SIZE, 8,
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_PLANE_PIXEL_SIZE, 16,
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_BLUE_SIZE, 8,
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_RED_SIZE, 8,
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_HORIZONTAL_SAMPLING_FACTOR_REFERENCE, 4,
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_HORIZONTAL_SUBSAMPLING_FACTOR, 2,
  //    VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_VERTICAL_SUBSAMPLING_FACTOR, 2
  //    VIDEOATTRIBUTEKEY_TERMINATOR
  // }
  //
  // Another example, commonly known 16-bit RGB 565 color format may be
  // specified as follows:
  // {
  //   VIDEOATTRIBUTEKEY_COLORFORMAT_PLANE_PIXEL_SIZE, 16,
  //   VIDEOATTRIBUTEKEY_COLORFORMAT_RED_SIZE, 5,
  //   VIDEOATTRIBUTEKEY_COLORFORMAT_GREEN_SIZE, 6,
  //   VIDEOATTRIBUTEKEY_COLORFORMAT_BLUE_SIZE, 5,
  //   VIDEOATTRIBUTEKEY_TERMINATOR
  // }
  // Total color component bits per pixel in the picture buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_PLANE_PIXEL_SIZE,
  // Bits of red per pixel in picture buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_RED_SIZE,
  // Bits of green per pixel in picture buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_GREEN_SIZE,
  // Bits of blue per pixel in picture buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_BLUE_SIZE,
  // Bits of alpha in color buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_ALPHA_SIZE,
  // Bits of luma per pixel in color buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_LUMA_SIZE,
  // Bits of blue difference chroma (Cb) data in color buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_BLUE_SIZE,
  // Bits of blue difference chroma (Cr) data in color buffer.
  VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_RED_SIZE,
  // Three keys to describe the subsampling of YCbCr sampled digital video
  // signal. For example, 4:2:2 sampling could be defined by setting:
  // VIDEOATTRIBUTEKEY_COLORFORMAT_HORIZONTAL_SAMPLING_FACTOR_REFERENCE = 4
  // VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMINANCE_HORIZONTAL_SUBSAMPLING_FACTOR = 2
  // VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMINANCE_VERTICAL_SUBSAMPLING_FACTOR = 2
  VIDEOATTRIBUTEKEY_COLORFORMAT_HORIZONTAL_SAMPLING_FACTOR_REFERENCE,
  VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_HORIZONTAL_SUBSAMPLING_FACTOR,
  VIDEOATTRIBUTEKEY_COLORFORMAT_CHROMA_VERTICAL_SUBSAMPLING_FACTOR,
  // Base for telling implementation specific information about the optimal
  // number of picture buffers to be provided to the implementation.
  VIDEOATTRIBUTEKEY_PICTUREBUFFER_REQUIREMENTS_BASE = 0x10000,
  // Following two keys are used to signal how many buffers are needed by the
  // implementation as a function of the maximum number of reference frames set
  // by the stream. Number of required buffers is
  //   MAX_REF_FRAMES * REFERENCE_PIC_MULTIPLIER + ADDITIONAL_BUFFERS
  VIDEOATTRIBUTEKEY_PICTUREBUFFER_REQUIREMENTS_ADDITIONAL_BUFFERS,
  VIDEOATTRIBUTEKEY_PICTUREBUFFER_REQUIREMENTS_REFERENCE_PIC_MULTIPLIER,
  // If decoder does not support pixel accurate strides for picture buffer, this
  // parameter tells the stride multiple that is needed by the decoder. Plugin
  // must obey the given stride in its picture buffer allocations.
  VIDEOATTRIBUTEKEY_PICTUREBUFFER_REQUIREMENTS_STRIDE_MULTIPLE,
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

// Video decoder interface.
// TODO(vmr): Move much of the inner classes to media namespace to simplify code
//            that's using it.
class VideoDecodeAccelerator {
 public:
  virtual ~VideoDecodeAccelerator();

  // Enumeration of potential errors generated by the API.
  enum Error {
    VIDEODECODERERROR_NONE = 0,
    VIDEODECODERERROR_UNINITIALIZED,
    VIDEODECODERERROR_UNSUPPORTED,
    VIDEODECODERERROR_INVALIDINPUT,
    VIDEODECODERERROR_MEMFAILURE,
    VIDEODECODERERROR_INSUFFICIENT_BUFFERS,
    VIDEODECODERERROR_INSUFFICIENT_RESOURCES,
    VIDEODECODERERROR_HARDWARE,
    VIDEODECODERERROR_UNEXPECTED_FLUSH,
  };

  // Interface expected from PictureBuffers where pictures are stored.
  class PictureBuffer {
   public:
    enum MemoryType {
      PICTUREBUFFER_MEMORYTYPE_NONE = 0,
      PICTUREBUFFER_MEMORYTYPE_SYSTEM,
      PICTUREBUFFER_MEMORYTYPE_GL_TEXTURE,
    };
    // Union to represent one data plane in picture buffer.
    union DataPlaneHandle {
      struct {
        uint32 context_id;  // GLES context id.
        uint32 texture_id;  // GLES texture id.
      };
      void* sysmem;  // Simply a pointer to system memory.
    };

    virtual ~PictureBuffer();
    virtual int32 GetId() = 0;
    virtual gfx::Size GetSize() = 0;
    virtual const std::vector<uint32>& GetColorFormat() = 0;
    virtual MemoryType GetMemoryType() = 0;
    virtual std::vector<DataPlaneHandle>& GetPlaneHandles() = 0;
  };

  class Picture {
   public:
    virtual ~Picture();

    // Picture size related functions.
    // There are three types of logical picture sizes that applications using
    // video decoder should be aware of:
    //   - Visible picture size,
    //   - Decoded picture size, and
    //   - Picture buffer size.
    //
    // Visible picture size means the actual picture size that is intended to be
    // displayed from the decoded output.
    //
    // Decoded picture size might vary from the visible size of the picture,
    // because of the underlying properties of the codec. Vast majority of
    // modern video compression algorithms are based on (macro)block-based
    // transforms and therefore process the picture in small windows (usually
    // of size 16x16 pixels) one by one. However, if the native picture size
    // does not happen to match the block-size of the algorithm, there may be
    // redundant data left on the sides of the output picture, which are not
    // intended for display. For example, this happens to video of size 854x480
    // and H.264 codec. Since the width (854 pixels) is not multiple of the
    // block size of the coding format (16 pixels), pixel columns 854-863
    // contain garbage data which is notintended for display.
    //
    // Plugin is providing the buffers for output decoding and it should know
    // the picture buffer size it has provided to the decoder. Thus, there is
    // no function to query the buffer size from this class.

    // Returns the picture buffer where this picture is contained.
    virtual PictureBuffer* picture_buffer() = 0;

    // Returns the decoded size of the decoded picture in pixels.
    virtual gfx::Size GetDecodedSize() const = 0;

    // Returns the visible size of the decoded picture in pixels.
    virtual gfx::Size GetVisibleSize() const = 0;

    // Returns metadata associated with the picture.
    virtual void* GetUserHandle() = 0;
  };

  // Interface for collaborating with picture interface to provide memory for
  // output picture and blitting them.
  class Client {
   public:
    virtual ~Client() {}

    // Callback to tell the information needed by the client to provide decoding
    // buffer to the decoder.
    virtual void ProvidePictureBuffers(
        uint32 requested_num_of_buffers,
        const std::vector<uint32>& buffer_properties) = 0;

    // Callback to dismiss picture buffer that was assigned earlier.
    virtual void DismissPictureBuffer(PictureBuffer* picture_buffer) = 0;

    // Callback to deliver decoded pictures ready to be displayed.
    virtual void PictureReady(Picture* picture) = 0;

    // Callback to notify that decoder has decoded end of stream marker and has
    // outputted all displayable pictures.
    virtual void NotifyEndOfStream() = 0;

    // Callback to notify about decoding errors.
    virtual void NotifyError(Error error) = 0;
  };

  // Video decoder functions.
  // GetConfig returns supported configurations that are subsets of given
  // |prototype_config|.
  // Parameters:
  //  |instance| is the pointer to the plug-in instance.
  //  |prototype_config| is the prototypical configuration.
  //
  // Returns std::vector containing all the configurations that match the
  // prototypical configuration.
  virtual const std::vector<uint32>& GetConfig(
      const std::vector<uint32>& prototype_config) = 0;

  // Initializes the video decoder with specific configuration.
  // Parameters:
  //  |config| is the configuration on which the decoder should be initialized.
  //
  // Returns true when command successfully accepted. Otherwise false.
  virtual bool Initialize(const std::vector<uint32>& config) = 0;

  // Decodes given bitstream buffer. Once decoder is done with processing
  // |bitstream_buffer| is will call |callback|.
  // Parameters:
  //  |bitstream_buffer| is the input bitstream that is sent for decoding.
  //  |callback| contains the callback function pointer for informing about
  //  finished processing for |bitstream_buffer|.
  //
  // Returns true when command successfully accepted. Otherwise false.
  virtual bool Decode(BitstreamBuffer* bitstream_buffer,
                      VideoDecodeAcceleratorCallback* callback) = 0;

  // Assigns picture buffer to the video decoder. This function must be called
  // at latest when decoder has made a ProvidePictureBuffers callback to the
  // client. Ownership of the picture buffer remains with the client, but it is
  // not allowed to deallocate picture buffer before DismissPictureBuffer
  // callback has been initiated for a given buffer.
  //
  // Parameters:
  //  |picture_buffers| contains the allocated picture buffers for the output.
  virtual void AssignPictureBuffer(
      std::vector<PictureBuffer*> picture_buffers) = 0;

  // Sends picture buffers to be reused by the decoder. This needs to be called
  // for each buffer that has been processed so that decoder may know onto which
  // picture buffers it can write the output to.
  //
  // Parameters:
  //  |picture_buffer| points to the picture buffer that is to be reused.
  virtual void ReusePictureBuffer(PictureBuffer* picture_buffer) = 0;

  // Flushes the decoder. Flushing will result in output of the
  // pictures and buffers held inside the decoder and returning of bitstream
  // buffers using the callbacks implemented by the plug-in. Once done with
  // flushing, the decode will call the |callback|.
  //
  // Parameters:
  //  |callback| contains the callback function pointer.
  //
  // Returns true when command successfully accepted. Otherwise false.
  virtual bool Flush(VideoDecodeAcceleratorCallback* callback) = 0;

  // Aborts the decoder. Decode will abort the decoding as soon as possible and
  // will not output anything. |callback| will be called as soon as abort has
  // been finished. After abort all buffers can be considered dismissed, even
  // when there has not been callbacks to dismiss them.
  //
  // Parameters:
  //  |callback| contains the callback function pointer.
  //
  // Returns true when command successfully accepted. Otherwise false.
  virtual bool Abort(VideoDecodeAcceleratorCallback* callback) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_
