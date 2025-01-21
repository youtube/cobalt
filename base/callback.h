#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#ifndef COBALT_PENDING_CLEAN_UP
#error "TODO: Remove these"
#endif

#include "base/functional/callback.h"

namespace base {
template <typename Signature>
using Callback = RepeatingCallback<Signature>;

using Closure = Callback<void()>;
}

#endif
