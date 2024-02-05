#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#ifndef USE_HACKY_COBALT_CHANGES
#error "TODO: Remove these"
#endif

#include "base/functional/callback.h"

namespace base {
template <typename Signature>
using Callback = RepeatingCallback<Signature>;

using Closure = Callback<void()>;
}

#endif
