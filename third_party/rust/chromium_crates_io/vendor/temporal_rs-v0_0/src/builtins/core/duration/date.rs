//! Implementation of a `DateDuration`

use crate::{
    iso::iso_date_to_epoch_days, options::ArithmeticOverflow, Duration, PlainDate, Sign,
    TemporalError, TemporalResult,
};

use super::duration_sign;

/// `DateDuration` represents the [date duration record][spec] of the `Duration.`
///
/// These fields are laid out in the [Temporal Proposal][field spec] as 64-bit floating point numbers.
///
/// [spec]: https://tc39.es/proposal-temporal/#sec-temporal-date-duration-records
/// [field spec]: https://tc39.es/proposal-temporal/#sec-properties-of-temporal-duration-instances
#[non_exhaustive]
#[derive(Debug, Default, Clone, Copy, PartialEq, PartialOrd)]
pub struct DateDuration {
    /// `DateDuration`'s internal year value.
    pub years: i64,
    /// `DateDuration`'s internal month value.
    pub months: i64,
    /// `DateDuration`'s internal week value.
    pub weeks: i64,
    /// `DateDuration`'s internal day value.
    pub days: i64,
}

impl DateDuration {
    /// Creates a new, non-validated `DateDuration`.
    #[inline]
    #[must_use]
    pub(crate) const fn new_unchecked(years: i64, months: i64, weeks: i64, days: i64) -> Self {
        Self {
            years,
            months,
            weeks,
            days,
        }
    }

    /// Returns the iterator for `DateDuration`
    #[inline]
    #[must_use]
    pub(crate) fn fields(&self) -> [i64; 4] {
        [self.years, self.months, self.weeks, self.days]
    }
}

impl DateDuration {
    /// Creates a new `DateDuration` with provided values.
    #[inline]
    pub fn new(years: i64, months: i64, weeks: i64, days: i64) -> TemporalResult<Self> {
        let result = Self::new_unchecked(years, months, weeks, days);
        if !super::is_valid_duration(years, months, weeks, days, 0, 0, 0, 0, 0, 0) {
            return Err(TemporalError::range().with_message("Invalid DateDuration."));
        }
        Ok(result)
    }

    /// Returns a negated `DateDuration`.
    #[inline]
    #[must_use]
    pub fn negated(&self) -> Self {
        Self {
            years: self.years.saturating_neg(),
            months: self.months.saturating_neg(),
            weeks: self.weeks.saturating_neg(),
            days: self.days.saturating_neg(),
        }
    }

    /// Returns a new `DateDuration` representing the absolute value of the current.
    #[inline]
    #[must_use]
    pub fn abs(&self) -> Self {
        Self {
            years: self.years.abs(),
            months: self.months.abs(),
            weeks: self.weeks.abs(),
            days: self.days.abs(),
        }
    }

    /// Returns the sign for the current `DateDuration`.
    #[inline]
    #[must_use]
    pub fn sign(&self) -> Sign {
        duration_sign(self.fields().as_slice())
    }

    /// DateDurationDays
    pub(crate) fn days(&self, relative_to: &PlainDate) -> TemporalResult<i64> {
        // 1. Let yearsMonthsWeeksDuration be ! AdjustDateDurationRecord(dateDuration, 0).
        let ymw_duration = self.adjust(0, None, None)?;
        // 2. If DateDurationSign(yearsMonthsWeeksDuration) = 0, return dateDuration.[[Days]].
        if ymw_duration.sign() == Sign::Zero {
            return Ok(self.days);
        }
        // 3. Let later be ? CalendarDateAdd(plainRelativeTo.[[Calendar]], plainRelativeTo.[[ISODate]], yearsMonthsWeeksDuration, constrain).
        let later = relative_to.add(
            &Duration {
                date: *self,
                time: Default::default(),
            },
            Some(ArithmeticOverflow::Constrain),
        )?;
        // 4. Let epochDays1 be ISODateToEpochDays(plainRelativeTo.[[ISODate]].[[Year]], plainRelativeTo.[[ISODate]].[[Month]] - 1, plainRelativeTo.[[ISODate]].[[Day]]).
        let epoch_days_1 = iso_date_to_epoch_days(
            relative_to.iso_year(),
            i32::from(relative_to.iso_month()), // this function takes 1 based month number
            i32::from(relative_to.iso_day()),
        );
        // 5. Let epochDays2 be ISODateToEpochDays(later.[[Year]], later.[[Month]] - 1, later.[[Day]]).
        let epoch_days_2 = iso_date_to_epoch_days(
            later.iso_year(),
            i32::from(later.iso_month()), // this function takes 1 based month number
            i32::from(later.iso_day()),
        );
        // 6. Let yearsMonthsWeeksInDays be epochDays2 - epochDays1.
        let ymd_in_days = epoch_days_2 - epoch_days_1;
        // 7. Return dateDuration.[[Days]] + yearsMonthsWeeksInDays.
        Ok(self.days + ymd_in_days)
    }

    /// AdjustDateDurationRecord
    pub(crate) fn adjust(
        &self,
        days: i64,
        weeks: Option<i64>,
        months: Option<i64>,
    ) -> TemporalResult<Self> {
        // 1. If weeks is not present, set weeks to dateDuration.[[Weeks]].
        // 2. If months is not present, set months to dateDuration.[[Months]].
        // 3. Return ? CreateDateDurationRecord(dateDuration.[[Years]], months, weeks, days).
        let weeks = weeks.unwrap_or(self.weeks);
        let months = months.unwrap_or(self.months);
        Ok(Self {
            years: self.years,
            months,
            weeks,
            days,
        })
    }
}
