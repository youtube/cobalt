#ifndef temporal_rs_Duration_D_HPP
#define temporal_rs_Duration_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"

namespace temporal_rs {
namespace capi { struct DateDuration; }
class DateDuration;
namespace capi { struct Duration; }
class Duration;
namespace capi { struct TimeDuration; }
class TimeDuration;
struct PartialDuration;
struct TemporalError;
class Sign;
}


namespace temporal_rs {
namespace capi {
    struct Duration;
} // namespace capi
} // namespace

namespace temporal_rs {
class Duration {
public:

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> create(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_day_and_time(int64_t day, const temporal_rs::TimeDuration& time);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_partial_duration(temporal_rs::PartialDuration partial);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline bool is_time_within_range() const;

  inline const temporal_rs::TimeDuration& time() const;

  inline const temporal_rs::DateDuration& date() const;

  inline int64_t years() const;

  inline int64_t months() const;

  inline int64_t weeks() const;

  inline int64_t days() const;

  inline int64_t hours() const;

  inline int64_t minutes() const;

  inline int64_t seconds() const;

  inline int64_t milliseconds() const;

  inline std::optional<double> microseconds() const;

  inline std::optional<double> nanoseconds() const;

  inline temporal_rs::Sign sign() const;

  inline bool is_zero() const;

  inline std::unique_ptr<temporal_rs::Duration> abs() const;

  inline std::unique_ptr<temporal_rs::Duration> negated() const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> add(const temporal_rs::Duration& other) const;

  inline diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& other) const;

  inline const temporal_rs::capi::Duration* AsFFI() const;
  inline temporal_rs::capi::Duration* AsFFI();
  inline static const temporal_rs::Duration* FromFFI(const temporal_rs::capi::Duration* ptr);
  inline static temporal_rs::Duration* FromFFI(temporal_rs::capi::Duration* ptr);
  inline static void operator delete(void* ptr);
private:
  Duration() = delete;
  Duration(const temporal_rs::Duration&) = delete;
  Duration(temporal_rs::Duration&&) noexcept = delete;
  Duration operator=(const temporal_rs::Duration&) = delete;
  Duration operator=(temporal_rs::Duration&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // temporal_rs_Duration_D_HPP
