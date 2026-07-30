use std::fmt;

use crate::{error, Error, Integer};

/// A structured field value [decimal].
///
/// Decimals have 12 digits of integer precision and 3 digits of fractional precision.
///
/// [decimal]: <https://httpwg.org/specs/rfc9651.html#decimal>
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub struct Decimal(Integer);

impl Decimal {
    /// The minimum value for a parsed or serialized decimal: `-999_999_999_999.999`.
    pub const MIN: Self = Self::from_integer_scaled_1000(Integer::MIN);

    /// The maximum value for a parsed or serialized decimal: `999_999_999_999.999`.
    pub const MAX: Self = Self::from_integer_scaled_1000(Integer::MAX);

    /// `0.0`.
    pub const ZERO: Self = Self(Integer::ZERO);

    /// Returns the decimal as an integer multiplied by 1000.
    ///
    /// The conversion is guaranteed to be precise.
    ///
    /// # Example
    ///
    /// ```
    /// let decimal = sfv::Decimal::try_from(1.234).unwrap();
    /// assert_eq!(i64::try_from(decimal.as_integer_scaled_1000()).unwrap(), 1234);
    /// ```
    #[must_use]
    pub fn as_integer_scaled_1000(&self) -> Integer {
        self.0
    }

    /// Creates a decimal from an integer multiplied by 1000.
    ///
    /// The conversion is guaranteed to be precise.
    ///
    /// # Example
    ///
    /// ```
    /// let decimal = sfv::Decimal::from_integer_scaled_1000(sfv::integer(1234));
    /// #[allow(clippy::float_cmp)]
    /// assert_eq!(f64::try_from(decimal).unwrap(), 1.234);
    /// ```
    #[must_use]
    pub const fn from_integer_scaled_1000(v: Integer) -> Self {
        Self(v)
    }
}

impl fmt::Display for Decimal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let v = i64::from(self.as_integer_scaled_1000());

        if v == 0 {
            return f.write_str("0.0");
        }

        let sign = if v < 0 { "-" } else { "" };
        let v = v.abs();
        let i_part = v / 1000;
        let f_part = v % 1000;

        if f_part % 100 == 0 {
            write!(f, "{}{}.{}", sign, i_part, f_part / 100)
        } else if f_part % 10 == 0 {
            write!(f, "{}{}.{:02}", sign, i_part, f_part / 10)
        } else {
            write!(f, "{sign}{i_part}.{f_part:03}")
        }
    }
}

impl From<i8> for Decimal {
    fn from(v: i8) -> Decimal {
        Self(Integer::from(i16::from(v) * 1000))
    }
}

impl From<i16> for Decimal {
    fn from(v: i16) -> Decimal {
        Self(Integer::from(i32::from(v) * 1000))
    }
}

impl From<i32> for Decimal {
    fn from(v: i32) -> Decimal {
        Self(Integer::try_from(i64::from(v) * 1000).unwrap())
    }
}

macro_rules! impl_try_from_integer {
    ($($from: ty,)+) => {
        $(
            impl TryFrom<$from> for Decimal {
                type Error = Error;

                fn try_from(v: $from) -> Result<Decimal, Error> {
                    match v.checked_mul(1000) {
                        None => Err(error::Repr::OutOfRange.into()),
                        Some(v) => Integer::try_from(v).map(Decimal),
                    }
                }
            }
        )+
    }
}

impl_try_from_integer! {
    i64,
    i128,
    isize,
    u64,
    u128,
    usize,
}

impl From<u8> for Decimal {
    fn from(v: u8) -> Decimal {
        Self(Integer::from(u16::from(v) * 1000))
    }
}

impl From<u16> for Decimal {
    fn from(v: u16) -> Decimal {
        Self(Integer::from(u32::from(v) * 1000))
    }
}

impl From<u32> for Decimal {
    fn from(v: u32) -> Decimal {
        Self(Integer::try_from(u64::from(v) * 1000).unwrap())
    }
}

impl From<Decimal> for f64 {
    #[allow(clippy::cast_precision_loss)]
    fn from(v: Decimal) -> Self {
        let v = i64::from(v.as_integer_scaled_1000());
        (v as f64) / 1000.0
    }
}

impl TryFrom<f32> for Decimal {
    type Error = Error;

    fn try_from(v: f32) -> Result<Decimal, Self::Error> {
        Self::try_from(f64::from(v))
    }
}

impl TryFrom<f64> for Decimal {
    type Error = Error;

    fn try_from(v: f64) -> Result<Decimal, Error> {
        if v.is_nan() {
            return Err(error::Repr::NaN.into());
        }

        let v = (v * 1000.0).round_ties_even();
        // Only excessively clever options exist for this conversion, so use "as"
        // Note that this relies on saturating casts for values > i64::MAX
        // See https://github.com/rust-lang/rust/issues/10184
        #[allow(clippy::cast_possible_truncation)]
        Integer::try_from(v as i64).map(Decimal)
    }
}

impl TryFrom<Integer> for Decimal {
    type Error = Error;

    fn try_from(v: Integer) -> Result<Decimal, Error> {
        i64::from(v).try_into()
    }
}
