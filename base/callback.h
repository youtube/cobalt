#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#include "base/functional/callback.h"

namespace base {
template <typename Signature>
using Callback = RepeatingCallback<Signature>;

using Closure = Callback<void()>;
}

#endif
