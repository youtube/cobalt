#ifndef CLI_CODEC_OPUS_UTILS_H_
#define CLI_CODEC_OPUS_UTILS_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace iamf_tools {

/*!\brief Converts a `libopus` error code to an `absl::Status`.
 *
 * \param opus_error_code Error code from `libopus`.
 * \param message Message to include in the returned `absl::Status`.
 * \return `absl::Status` corresponding to input arguments.
 */
absl::Status OpusErrorCodeToAbslStatus(int opus_error_code,
                                       absl::string_view message);

}  // namespace iamf_tools

#endif  // CLI_CODEC_OPUS_UTILS_H_
