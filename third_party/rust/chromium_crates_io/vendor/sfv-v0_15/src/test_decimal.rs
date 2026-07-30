use crate::{error, Decimal, Error, Integer};

// Expect a floating point error less than the smallest allowed value of 0.001
const ABSERR: f64 = 0.000_1_f64;

#[test]
fn test_display() {
    for (expected, input) in [
        ("0.1", 100),
        ("-0.1", -100),
        ("0.01", 10),
        ("-0.01", -10),
        ("0.001", 1),
        ("-0.001", -1),
        ("0.12", 120),
        ("-0.12", -120),
        ("0.124", 124),
        ("-0.124", -124),
        ("0.125", 125),
        ("-0.125", -125),
        ("0.126", 126),
        ("-0.126", -126),
    ] {
        let decimal = Decimal::from_integer_scaled_1000(Integer::constant(input));
        assert_eq!(expected, decimal.to_string());
    }

    assert_eq!("0.0", Decimal::ZERO.to_string());
    assert_eq!("-999999999999.999", Decimal::MIN.to_string());
    assert_eq!("999999999999.999", Decimal::MAX.to_string());
}

#[test]
fn test_into_f64() {
    for (expected, input) in [
        (0.0, 0),
        (0.001, 1),
        (0.01, 10),
        (0.1, 100),
        (1.0, 1000),
        (10.0, 10000),
        (0.123, 123),
        (-0.001, -1),
        (-0.01, -10),
        (-0.1, -100),
        (-1.0, -1000),
        (-10.0, -10000),
        (-0.123, -123),
    ] {
        assert!(
            (expected - f64::from(Decimal::from_integer_scaled_1000(input.into()))).abs() < ABSERR
        );
    }

    assert!((-999_999_999_999.999 - f64::from(Decimal::MIN)).abs() < ABSERR);
    assert!((999_999_999_999.999 - f64::from(Decimal::MAX)).abs() < ABSERR);
}

#[test]
fn test_try_from_f64() {
    for (expected, input) in [
        (Err(error::Repr::NaN), f64::NAN),
        (Err(error::Repr::OutOfRange), f64::INFINITY),
        (Err(error::Repr::OutOfRange), f64::NEG_INFINITY),
        (Err(error::Repr::OutOfRange), 2_f64.powi(65)),
        (Err(error::Repr::OutOfRange), -(2_f64.powi(65))),
        (Err(error::Repr::OutOfRange), -1_000_000_000_000.0),
        (Err(error::Repr::OutOfRange), 1_000_000_000_000.0),
        (Ok(Decimal::MIN), -999_999_999_999.999),
        (Ok(Decimal::MIN), -999_999_999_999.999_1),
        (Err(error::Repr::OutOfRange), -999_999_999_999.999_5),
        (Ok(Decimal::MAX), 999_999_999_999.999),
        (Ok(Decimal::MAX), 999_999_999_999.999_1),
        (Err(error::Repr::OutOfRange), 999_999_999_999.999_5),
        (Ok(Decimal::ZERO), 0.0),
        (Ok(Decimal::ZERO), -0.0),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(123))),
            0.1234,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(124))),
            0.1235,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(124))),
            0.1236,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-123))),
            -0.1234,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-124))),
            -0.1235,
        ),
        (
            Ok(Decimal::from_integer_scaled_1000(Integer::constant(-124))),
            -0.1236,
        ),
    ] {
        assert_eq!(expected.map_err(Error::from), Decimal::try_from(input));
    }
}
