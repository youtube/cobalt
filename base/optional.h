#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
template <typename T>
using Optional = absl::optional<T>;

using nullopt = absl::nullopt_t;
}

#endif  // BASE_OPTIONAL_H_
