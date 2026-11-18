use std::fmt::Write as _;

use crate::{utils, Date, Decimal, Integer, KeyRef, RefBareItem, StringRef, TokenRef};

pub(crate) struct Serializer;

impl Serializer {
    pub(crate) fn serialize_bare_item<'b>(value: impl Into<RefBareItem<'b>>, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-bare-item

        match value.into() {
            RefBareItem::Boolean(value) => Self::serialize_bool(value, output),
            RefBareItem::String(value) => Self::serialize_string(value, output),
            RefBareItem::ByteSequence(value) => Self::serialize_byte_sequence(value, output),
            RefBareItem::Token(value) => Self::serialize_token(value, output),
            RefBareItem::Integer(value) => Self::serialize_integer(value, output),
            RefBareItem::Decimal(value) => Self::serialize_decimal(value, output),
            RefBareItem::Date(value) => Self::serialize_date(value, output),
            RefBareItem::DisplayString(value) => Self::serialize_display_string(value, output),
        }
    }

    pub(crate) fn serialize_parameter<'b>(
        name: &KeyRef,
        value: impl Into<RefBareItem<'b>>,
        output: &mut String,
    ) {
        // https://httpwg.org/specs/rfc9651.html#ser-params
        output.push(';');
        Self::serialize_key(name, output);

        let value = value.into();
        if value != RefBareItem::Boolean(true) {
            output.push('=');
            Self::serialize_bare_item(value, output);
        }
    }

    pub(crate) fn serialize_key(input_key: &KeyRef, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-key

        output.push_str(input_key.as_str());
    }

    pub(crate) fn serialize_integer(value: Integer, output: &mut String) {
        //https://httpwg.org/specs/rfc9651.html#ser-integer

        write!(output, "{value}").unwrap();
    }

    pub(crate) fn serialize_decimal(value: Decimal, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-decimal

        write!(output, "{value}").unwrap();
    }

    pub(crate) fn serialize_string(value: &StringRef, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-string

        output.push('"');
        for char in value.as_str().chars() {
            if char == '\\' || char == '"' {
                output.push('\\');
            }
            output.push(char);
        }
        output.push('"');
    }

    pub(crate) fn serialize_token(value: &TokenRef, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-token

        output.push_str(value.as_str());
    }

    pub(crate) fn serialize_byte_sequence(value: &[u8], output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-binary

        output.push(':');
        base64::Engine::encode_string(&utils::BASE64, value, output);
        output.push(':');
    }

    pub(crate) fn serialize_bool(value: bool, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-boolean

        output.push_str(if value { "?1" } else { "?0" });
    }

    pub(crate) fn serialize_date(value: Date, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-date

        write!(output, "{value}").unwrap();
    }

    pub(crate) fn serialize_display_string(value: &str, output: &mut String) {
        // https://httpwg.org/specs/rfc9651.html#ser-display

        output.push_str(r#"%""#);
        for c in value.bytes() {
            match c {
                b'%' | b'"' | 0x00..=0x1f | 0x7f..=0xff => {
                    output.push('%');
                    output.push(char::from_digit((u32::from(c) >> 4) & 0xf, 16).unwrap());
                    output.push(char::from_digit(u32::from(c) & 0xf, 16).unwrap());
                }
                _ => output.push(c as char),
            }
        }
        output.push('"');
    }
}
