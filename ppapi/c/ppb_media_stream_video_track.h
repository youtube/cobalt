/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_media_stream_video_track.idl modified Mon Apr  7 15:25:56 2014. */

#ifndef PPAPI_C_PPB_MEDIA_STREAM_VIDEO_TRACK_H_
#define PPAPI_C_PPB_MEDIA_STREAM_VIDEO_TRACK_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_0_1 "PPB_MediaStreamVideoTrack;0.1"
#define PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_1_0 \
    "PPB_MediaStreamVideoTrack;1.0" /* dev */
#define PPB_MEDIASTREAMVIDEOTRACK_INTERFACE \
    PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_0_1

/**
 * @file
 * Defines the <code>PPB_MediaStreamVideoTrack</code> interface. Used for
 * receiving video frames from a MediaStream video track in the browser.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains video track attributes which are used by
 * <code>Configure()</code>.
 */
typedef enum {
  /**
   * Attribute list terminator.
   */
  PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE = 0,
  /**
   * The maximum number of frames to hold in the input buffer.
   * Note: this is only used as advisory; the browser may allocate more or fewer
   * based on available resources. How many frames to buffer depends on usage -
   * request at least 2 to make sure latency doesn't cause lost frames. If
   * the plugin expects to hold on to more than one frame at a time (e.g. to do
   * multi-frame processing), it should request that many more.
   * If this attribute is not specified or value 0 is specified for this
   * attribute, the default value will be used.
   */
  PP_MEDIASTREAMVIDEOTRACK_ATTRIB_BUFFERED_FRAMES = 1,
  /**
   * The width of video frames in pixels. It should be a multiple of 4.
   * If the specified size is different from the video source (webcam),
   * frames will be scaled to specified size.
   * If this attribute is not specified or value 0 is specified, the original
   * frame size of the video track will be used.
   *
   * Maximum value: 4096 (4K resolution).
   */
  PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH = 2,
  /**
   * The height of video frames in pixels. It should be a multiple of 4.
   * If the specified size is different from the video source (webcam),
   * frames will be scaled to specified size.
   * If this attribute is not specified or value 0 is specified, the original
   * frame size of the video track will be used.
   *
   * Maximum value: 4096 (4K resolution).
   */
  PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT = 3,
  /**
   * The format of video frames. The attribute value is
   * a <code>PP_VideoFrame_Format</code>. If the specified format is different
   * from the video source (webcam), frames will be converted to specified
   * format.
   * If this attribute is not specified or value
   * <code>PP_VIDEOFRAME_FORMAT_UNKNOWN</code> is specified, the orignal frame
   * format of the video track will be used.
   */
  PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT = 4
} PP_MediaStreamVideoTrack_Attrib;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_MediaStreamVideoTrack_1_0 { /* dev */
  /**
   * Creates a PPB_MediaStreamVideoTrack resource for video output. Call this
   * when you will be creating frames and putting them to the track.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   *
   * @return A <code>PP_Resource</code> corresponding to a
   * PPB_MediaStreamVideoTrack resource if successful, 0 if failed.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a resource is a MediaStream video track resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is a Mediastream video track resource or <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsMediaStreamVideoTrack)(PP_Resource resource);
  /**
   * Configures underlying frame buffers for incoming frames.
   * If the application doesn't want to drop frames, then the
   * <code>PP_MEDIASTREAMVIDEOTRACK_ATTRIB_BUFFERED_FRAMES</code> should be
   * chosen such that inter-frame processing time variability won't overrun the
   * input buffer. If the buffer is overfilled, then frames will be dropped.
   * The application can detect this by examining the timestamp on returned
   * frames. If some attributes are not specified, default values will be used
   * for those unspecified attributes. If <code>Configure()</code> is not
   * called, default settings will be used.
   * Example usage from plugin code:
   * @code
   * int32_t attribs[] = {
   *     PP_MEDIASTREAMVIDEOTRACK_ATTRIB_BUFFERED_FRAMES, 4,
   *     PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE};
   * track_if->Configure(track, attribs, callback);
   * @endcode
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a video
   * resource.
   * @param[in] attrib_list A list of attribute name-value pairs in which each
   * attribute is immediately followed by the corresponding desired value.
   * The list is terminated by
   * <code>PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE</code>.
   * @param[in] callback <code>PP_CompletionCallback</code> to be called upon
   * completion of <code>Configure()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   * Returns <code>PP_ERROR_INPROGRESS</code> if there is a pending call of
   * <code>Configure()</code> or <code>GetFrame()</code>, or the plugin
   * holds some frames which are not recycled with <code>RecycleFrame()</code>.
   * If an error is returned, all attributes and the underlying buffer will not
   * be changed.
   */
  int32_t (*Configure)(PP_Resource video_track,
                       const int32_t attrib_list[],
                       struct PP_CompletionCallback callback);
  /**
   * Gets attribute value for a given attribute name.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a video
   * resource.
   * @param[in] attrib A <code>PP_MediaStreamVideoTrack_Attrib</code> for
   * querying.
   * @param[out] value A int32_t for storing the attribute value on success.
   * Otherwise, the value will not be changed.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*GetAttrib)(PP_Resource video_track,
                       PP_MediaStreamVideoTrack_Attrib attrib,
                       int32_t* value);
  /**
   * Returns the track ID of the underlying MediaStream video track.
   *
   * @param[in] video_track The <code>PP_Resource</code> to check.
   *
   * @return A <code>PP_Var</code> containing the MediaStream track ID as
   * a string.
   */
  struct PP_Var (*GetId)(PP_Resource video_track);
  /**
   * Checks whether the underlying MediaStream track has ended.
   * Calls to GetFrame while the track has ended are safe to make and will
   * complete, but will fail.
   *
   * @param[in] video_track The <code>PP_Resource</code> to check.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * MediaStream track has ended or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*HasEnded)(PP_Resource video_track);
  /**
   * Gets the next video frame from the MediaStream track.
   * If internal processing is slower than the incoming frame rate, new frames
   * will be dropped from the incoming stream. Once the input buffer is full,
   * frames will be dropped until <code>RecycleFrame()</code> is called to free
   * a spot for another frame to be buffered.
   * If there are no frames in the input buffer,
   * <code>PP_OK_COMPLETIONPENDING</code> will be returned immediately and the
   * <code>callback</code> will be called when a new frame is received or an
   * error happens.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a video
   * resource.
   * @param[out] frame A <code>PP_Resource</code> corresponding to a VideoFrame
   * resource.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of GetFrame().
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_NOMEMORY if <code>max_buffered_frames</code> frames buffer
   * was not allocated successfully.
   */
  int32_t (*GetFrame)(PP_Resource video_track,
                      PP_Resource* frame,
                      struct PP_CompletionCallback callback);
  /**
   * Recycles a frame returned by <code>GetFrame()</code>, so the track can
   * reuse the underlying buffer of this frame. And the frame will become
   * invalid. The caller should release all references it holds to
   * <code>frame</code> and not use it anymore.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a video
   * resource.
   * @param[in] frame A <code>PP_Resource</code> corresponding to a VideoFrame
   * resource returned by <code>GetFrame()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*RecycleFrame)(PP_Resource video_track, PP_Resource frame);
  /**
   * Closes the MediaStream video track and disconnects it from video source.
   * After calling <code>Close()</code>, no new frames will be received.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a
   * MediaStream video track resource.
   */
  void (*Close)(PP_Resource video_track);
  /**
   * Gets a free frame for output. The frame is allocated by
   * <code>Configure()</code>. The caller should fill it with frame data, and
   * then use |PutFrame()| to send the frame back.
   */
  int32_t (*GetEmptyFrame)(PP_Resource video_track,
                           PP_Resource* frame,
                           struct PP_CompletionCallback callback);
  /**
   * Sends a frame returned by |GetEmptyFrame()| to the output track.
   * After this function, the |frame| should not be used anymore and the
   * caller should release the reference that it holds.
   */
  int32_t (*PutFrame)(PP_Resource video_track, PP_Resource frame);
};

struct PPB_MediaStreamVideoTrack_0_1 {
  PP_Bool (*IsMediaStreamVideoTrack)(PP_Resource resource);
  int32_t (*Configure)(PP_Resource video_track,
                       const int32_t attrib_list[],
                       struct PP_CompletionCallback callback);
  int32_t (*GetAttrib)(PP_Resource video_track,
                       PP_MediaStreamVideoTrack_Attrib attrib,
                       int32_t* value);
  struct PP_Var (*GetId)(PP_Resource video_track);
  PP_Bool (*HasEnded)(PP_Resource video_track);
  int32_t (*GetFrame)(PP_Resource video_track,
                      PP_Resource* frame,
                      struct PP_CompletionCallback callback);
  int32_t (*RecycleFrame)(PP_Resource video_track, PP_Resource frame);
  void (*Close)(PP_Resource video_track);
};

typedef struct PPB_MediaStreamVideoTrack_0_1 PPB_MediaStreamVideoTrack;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MEDIA_STREAM_VIDEO_TRACK_H_ */

