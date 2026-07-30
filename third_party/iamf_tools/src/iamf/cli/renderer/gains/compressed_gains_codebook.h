#ifndef CLI_RENDERER_GAINS_COMPRESSED_GAINS_CODEBOOK_H_
#define CLI_RENDERER_GAINS_COMPRESSED_GAINS_CODEBOOK_H_

#include <cmath>
#include <vector>

#include "absl/base/no_destructor.h"

namespace iamf_tools {

static inline const std::vector<double>& GetCodebook() {
  using std::sqrt;
  static const absl::NoDestructor<std::vector<double>> kCodebook(
      {0, 0.5, 1 / sqrt(3), 1 / sqrt(2), sqrt(2.0 / 3.0),
       (1 + (1 / sqrt(2))) / 2, 1.0, 2 / sqrt(3), 1 / (sqrt(2)) + 0.5,
       sqrt(3.0 / 2.0)});
  return *kCodebook;
}

}  // namespace iamf_tools

#endif  // CLI_RENDERER_GAINS_COMPRESSED_GAINS_CODEBOOK_H_
