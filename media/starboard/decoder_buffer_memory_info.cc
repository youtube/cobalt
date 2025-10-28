#include "media/starboard/decoder_buffer_memory_info.h"

#include "media/base/video_codecs.h"
#include "media/starboard/starboard_utils.h"
#include "starboard/media.h"
#include "ui/gfx/geometry/size.h"

namespace media {

int GetAudioDecoderBufferLimitBytes() {
  return SbMediaGetAudioBufferBudget();
}

int GetVideoDecoderBufferLimitBytes(VideoCodec codec,
                                    const gfx::Size& resolution,
                                    int bits_per_pixel) {
  return SbMediaGetVideoBufferBudget(MediaVideoCodecToSbMediaVideoCodec(codec),
                                     resolution.width(), resolution.height(),
                                     bits_per_pixel);
}

}  // namespace media
