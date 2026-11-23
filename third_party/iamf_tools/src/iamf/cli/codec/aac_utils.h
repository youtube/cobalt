#ifndef CLI_CODEC_AAC_UTILS_H_
#define CLI_CODEC_AAC_UTILS_H_

#include <cstddef>

#include "libSYS/include/FDK_audio.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

/*!\brief IAMF requires raw AAC frames with no ADTS header. */
constexpr TRANSPORT_TYPE GetAacTransportationType() { return TT_MP4_RAW; }

/*!\brief The FDK AAC encoder uses 16-bit PCM. */
constexpr size_t GetFdkAacBytesPerSample() { return sizeof(INT_PCM); }

/*!\brief Convenience method for getting bit depth. */
constexpr size_t GetFdkAacBitDepth() { return GetFdkAacBytesPerSample() * 8; }

}  // namespace iamf_tools

#endif  // CLI_CODEC_AAC_UTILS_H_
