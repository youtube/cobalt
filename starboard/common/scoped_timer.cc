#include "starboard/common/scoped_timer.h"

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"

namespace starboard {

ScopedTimer::ScopedTimer(std::string_view message, SourceLocation location)
    : message_(message),
      location_(location),
      start_time_us_(CurrentMonotonicTime()) {}

ScopedTimer::~ScopedTimer() {
  if (!stopped_) {
    Stop();
  }
}

int64_t ScopedTimer::Stop() {
  stopped_ = true;
  const int64_t duration_us = CurrentMonotonicTime() - start_time_us_;
  if (!message_.empty()) {
    LogMessage(location_.file(), location_.line(), SB_LOG_INFO).stream()
        << message_ << " completed: elapsed(usec)="
        << FormatWithDigitSeparators(duration_us);
  }
  return duration_us;
}

}  // namespace starboard
