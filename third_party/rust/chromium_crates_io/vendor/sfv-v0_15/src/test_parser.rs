use std::convert::Infallible;

use crate::{
    error, integer, key_ref, string_ref, token_ref,
    visitor::{
        DictionaryVisitor, EntryVisitor, Ignored, InnerListVisitor, ItemVisitor, ListVisitor,
        ParameterVisitor,
    },
    BareItemFromInput, Decimal, Error, KeyRef, Num, Parser, RefBareItem,
};
#[cfg(feature = "parsed-types")]
use crate::{BareItem, Date, Dictionary, InnerList, Item, List, ListEntry, Parameters, Version};

#[cfg(feature = "parsed-types")]
macro_rules! item {
    ($val:expr) => {
        Item::from($val)
    };
    ($val:expr; {$($k:expr => $v:expr),* $(,)?}) => {
        Item::with_params($val, params!({$($k => $v),*}))
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! params {
    ({$($k:expr => $v:expr),* $(,)?}) => {
        Parameters::from([
            $( (key_ref($k).to_owned(), BareItem::from($v)) ),*
        ])
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! inner_list {
    ($($item:expr),* $(,)?) => {
        InnerList::new(vec![$( Item::from($item) ),*])
    };
    ([$($item:expr),* $(,)?]; {$($k:expr => $v:expr),* $(,)?}) => {
        InnerList::with_params(
            vec![$( Item::from($item) ),*],
            params!({$($k => $v),*})
        )
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! list {
    ($($v:expr),* $(,)?) => {
        vec![$( ListEntry::from($v) ),*]
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! dict {
    ($($k:expr => $v:expr),* $(,)?) => {
        Dictionary::from([
            $( (key_ref($k).to_owned(), ListEntry::from($v)) ),*
        ])
    };
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse() -> Result<(), Error> {
    let input = r#""some_value""#;
    let expected = item!(string_ref("some_value"));
    assert_eq!(expected, Parser::new(input).parse::<Item>()?);

    let input = "12.35;a ";
    let expected = item!(
        Decimal::from_integer_scaled_1000(integer(12_350));
        {"a" => true}
    );

    assert_eq!(expected, Parser::new(input).parse::<Item>()?);
    Ok(())
}

#[test]
fn parse_errors() {
    let input = r#""some_value¢""#;
    assert_eq!(
        Err(error::Repr::InvalidStringCharacter(11).into()),
        Parser::new(input).parse_item_with_visitor(Ignored)
    );
    let input = r#""some_value" trailing_text""#;
    assert_eq!(
        Err(error::Repr::TrailingCharactersAfterParsedValue(13).into()),
        Parser::new(input).parse_item_with_visitor(Ignored)
    );
    assert_eq!(
        Err(error::Repr::ExpectedStartOfBareItem(0).into()),
        Parser::new("").parse_item_with_visitor(Ignored)
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_of_numbers() -> Result<(), Error> {
    let input = "1,42";
    let expected_list = list![1, 42];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_with_multiple_spaces() -> Result<(), Error> {
    let input = "1  ,  42";
    let expected_list = list![1, 42];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_of_lists() -> Result<(), Error> {
    let input = "(1 2), (42 43)";
    let expected_list = list![inner_list![1, 2], inner_list![42, 43]];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_empty_inner_list() -> Result<(), Error> {
    let input = "()";
    let expected_list = list![inner_list![]];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_empty() -> Result<(), Error> {
    let input = "";
    let expected_list: List = vec![];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_of_lists_with_param_and_spaces() -> Result<(), Error> {
    let input = "(  1  42  ); k=*";
    let expected_list = list![inner_list!([1, 42]; {"k" => token_ref("*")})];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_of_items_and_lists_with_param() -> Result<(), Error> {
    let input = r#"12, 14, (a  b); param="param_value_1", ()"#;
    let expected_list = list![
        12,
        14,
        inner_list!([token_ref("a"), token_ref("b")]; {"param" => string_ref("param_value_1")}),
        inner_list![]
    ];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
fn parse_list_errors() {
    let input = ",";
    assert_eq!(
        Err(error::Repr::ExpectedStartOfBareItem(0).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );

    let input = "a, b c";
    assert_eq!(
        Err(error::Repr::TrailingCharactersAfterMember(5).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );

    let input = "a,";
    assert_eq!(
        Err(error::Repr::TrailingComma(1).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );

    let input = "a     ,    ";
    assert_eq!(
        Err(error::Repr::TrailingComma(6).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );

    let input = "a\t \t ,\t ";
    assert_eq!(
        Err(error::Repr::TrailingComma(5).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );

    let input = "a\t\t,\t\t\t";
    assert_eq!(
        Err(error::Repr::TrailingComma(3).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );

    let input = "(a b),";
    assert_eq!(
        Err(error::Repr::TrailingComma(5).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );

    let input = "(1, 2, (a b)";
    assert_eq!(
        Err(error::Repr::ExpectedInnerListDelimiter(2).into()),
        Parser::new(input).parse_list_with_visitor(Ignored)
    );
}

#[test]
fn parse_inner_list_errors() {
    let input = "c b); a=1";
    assert_eq!(
        Err(error::Repr::ExpectedStartOfInnerList(0)),
        Parser::new(input).parse_inner_list(Ignored)
    );

    let input = "(";
    assert_eq!(
        Err(error::Repr::UnterminatedInnerList(1)),
        Parser::new(input).parse_inner_list(Ignored)
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_inner_list_with_param_and_spaces() -> Result<(), Error> {
    let input = "(c b); a=1";
    let expected = inner_list!([token_ref("c"), token_ref("b")]; {"a" => 1});
    let mut inner_list = InnerList::default();
    Parser::new(input).parse_inner_list(&mut inner_list)?;
    assert_eq!(expected, inner_list);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_item_int_with_space() -> Result<(), Error> {
    let input = "12 ";
    assert_eq!(item!(12), Parser::new(input).parse::<Item>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_item_decimal_with_bool_param_and_space() -> Result<(), Error> {
    let input = "12.35;a ";
    assert_eq!(
        item!(Decimal::from_integer_scaled_1000(integer(12_350)); {"a" => true}),
        Parser::new(input).parse::<Item>()?
    );
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_item_number_with_param() -> Result<(), Error> {
    assert_eq!(
        item!(string_ref("12.35"); {"a1" => token_ref("*")}),
        Parser::new(r#""12.35";a1=*"#).parse::<Item>()?
    );
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_empty() -> Result<(), Error> {
    assert_eq!(Dictionary::new(), Parser::new("").parse::<Dictionary>()?);
    Ok(())
}

#[test]
fn parse_dict_errors() {
    let input = "abc=123;a=1;b=2 def";
    assert_eq!(
        Err(error::Repr::TrailingCharactersAfterMember(16).into()),
        Parser::new(input).parse_dictionary_with_visitor(Ignored)
    );
    let input = "abc=123;a=1,";
    assert_eq!(
        Err(error::Repr::TrailingComma(11).into()),
        Parser::new(input).parse_dictionary_with_visitor(Ignored)
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_with_spaces_and_params() -> Result<(), Error> {
    let input = r#"abc=123;a=1;b=2, def=456, ghi=789;q=9;r="+w""#;

    let expected_dict = dict! {
        "abc" => item!(123; {"a" => 1, "b" => 2}),
        "def" => 456,
        "ghi" => item!(789; {"q" => 9, "r" => string_ref("+w")})
    };
    assert_eq!(expected_dict, Parser::new(input).parse::<Dictionary>()?);

    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_empty_value() -> Result<(), Error> {
    let input = "a=()";
    let expected_dict = dict! {"a" => inner_list![]};
    assert_eq!(expected_dict, Parser::new(input).parse::<Dictionary>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_with_token_param() -> Result<(), Error> {
    let input = "a=1, b;foo=*, c=3";
    let expected_dict = dict! {
        "a" => 1,
        "b" => item!(true; {"foo" => token_ref("*")}),
        "c" => 3
    };
    assert_eq!(expected_dict, Parser::new(input).parse::<Dictionary>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_multiple_spaces() -> Result<(), Error> {
    // input1, input2, input3 must be parsed into the same structure
    let expected_dict = dict! {"a" => 1, "b" => 2};

    let input1 = "a=1 ,  b=2";
    let input2 = "a=1\t,\tb=2";
    let input3 = "a=1, b=2";
    assert_eq!(expected_dict, Parser::new(input1).parse::<Dictionary>()?);
    assert_eq!(expected_dict, Parser::new(input2).parse::<Dictionary>()?);
    assert_eq!(expected_dict, Parser::new(input3).parse::<Dictionary>()?);

    Ok(())
}

#[test]
fn parse_bare_item() -> Result<(), Error> {
    assert_eq!(
        RefBareItem::Boolean(false),
        Parser::new("?0").parse_bare_item()?
    );
    assert_eq!(
        RefBareItem::String(string_ref("test string")),
        Parser::new(r#""test string""#).parse_bare_item()?
    );
    assert_eq!(
        RefBareItem::Token(token_ref("*token")),
        Parser::new("*token").parse_bare_item()?
    );
    assert_eq!(
        RefBareItem::ByteSequence(b"base_64 encoding test"),
        Parser::new(":YmFzZV82NCBlbmNvZGluZyB0ZXN0:").parse_bare_item()?
    );
    assert_eq!(
        RefBareItem::Decimal(Decimal::from_integer_scaled_1000(integer(-3_550))),
        Parser::new("-3.55").parse_bare_item()?
    );
    Ok(())
}

#[test]
fn parse_bare_item_errors() {
    assert_eq!(
        Err(error::Repr::ExpectedStartOfBareItem(0)),
        Parser::new("!?0").parse_bare_item()
    );
    assert_eq!(
        Err(error::Repr::ExpectedStartOfBareItem(0)),
        Parser::new("_11abc").parse_bare_item()
    );
    assert_eq!(
        Err(error::Repr::ExpectedStartOfBareItem(0)),
        Parser::new("   ").parse_bare_item()
    );
}

#[test]
#[allow(clippy::bool_assert_comparison)]
fn parse_bool() -> Result<(), Error> {
    let mut parser = Parser::new("?0gk");
    assert_eq!(false, parser.parse_bool()?);
    assert_eq!(parser.remaining(), b"gk");

    assert_eq!(false, Parser::new("?0").parse_bool()?);
    assert_eq!(true, Parser::new("?1").parse_bool()?);
    Ok(())
}

#[test]
fn parse_bool_errors() {
    assert_eq!(
        Err(error::Repr::ExpectedStartOfBoolean(0)),
        Parser::new("").parse_bool()
    );
    assert_eq!(
        Err(error::Repr::ExpectedBoolean(1)),
        Parser::new("?").parse_bool()
    );
}

#[test]
fn parse_string() -> Result<(), Error> {
    let mut parser = Parser::new(r#""some string" ;not string"#);
    assert_eq!(string_ref("some string"), parser.parse_string()?);
    assert_eq!(parser.remaining(), " ;not string".as_bytes());

    assert_eq!(string_ref("test"), Parser::new(r#""test""#).parse_string()?);
    assert_eq!(
        string_ref("te\\st"),
        Parser::new(r#""te\\st""#).parse_string()?
    );
    assert_eq!(string_ref(""), Parser::new(r#""""#).parse_string()?);
    assert_eq!(
        string_ref("some string"),
        Parser::new(r#""some string""#).parse_string()?
    );
    Ok(())
}

#[test]
fn parse_string_errors() {
    assert_eq!(
        Err(error::Repr::ExpectedStartOfString(0)),
        Parser::new("test").parse_string()
    );
    assert_eq!(
        Err(error::Repr::UnterminatedEscapeSequence(2)),
        Parser::new(r#""\"#).parse_string()
    );
    assert_eq!(
        Err(error::Repr::InvalidEscapeSequence(2)),
        Parser::new(r#""\l""#).parse_string()
    );
    assert_eq!(
        Err(error::Repr::InvalidStringCharacter(1)),
        Parser::new("\"\u{1f}\"").parse_string()
    );
    assert_eq!(
        Err(error::Repr::UnterminatedString(5)),
        Parser::new(r#""smth"#).parse_string()
    );
}

#[test]
fn parse_token() -> Result<(), Error> {
    let mut parser = Parser::new("*some:token}not token");
    assert_eq!(token_ref("*some:token"), parser.parse_token()?);
    assert_eq!(parser.remaining(), b"}not token");

    assert_eq!(token_ref("token"), Parser::new("token").parse_token()?);
    assert_eq!(
        token_ref("a_b-c.d3:f%00/*"),
        Parser::new("a_b-c.d3:f%00/*").parse_token()?
    );
    assert_eq!(
        token_ref("TestToken"),
        Parser::new("TestToken").parse_token()?
    );
    assert_eq!(token_ref("some"), Parser::new("some@token").parse_token()?);
    assert_eq!(
        token_ref("*TestToken*"),
        Parser::new("*TestToken*").parse_token()?
    );
    assert_eq!(token_ref("*"), Parser::new("*[@:token").parse_token()?);
    assert_eq!(token_ref("test"), Parser::new("test token").parse_token()?);

    Ok(())
}

#[test]
fn parse_token_errors() {
    let mut parser = Parser::new("765token");
    assert_eq!(
        Err(error::Repr::ExpectedStartOfToken(0)),
        parser.parse_token()
    );
    assert_eq!(parser.remaining(), b"765token");

    assert_eq!(
        Err(error::Repr::ExpectedStartOfToken(0)),
        Parser::new("7token").parse_token()
    );
    assert_eq!(
        Err(error::Repr::ExpectedStartOfToken(0)),
        Parser::new("").parse_token()
    );
}

#[test]
fn parse_byte_sequence() -> Result<(), Error> {
    let mut parser = Parser::new(":aGVsbG8:rest_of_str");
    assert_eq!("hello".as_bytes(), parser.parse_byte_sequence()?);
    assert_eq!(parser.remaining(), b"rest_of_str");

    assert_eq!(
        "hello".as_bytes(),
        Parser::new(":aGVsbG8:").parse_byte_sequence()?
    );
    assert_eq!(
        "test_encode".as_bytes(),
        Parser::new(":dGVzdF9lbmNvZGU:").parse_byte_sequence()?
    );
    assert_eq!(
        "new:year tree".as_bytes(),
        Parser::new(":bmV3OnllYXIgdHJlZQ==:").parse_byte_sequence()?
    );
    assert_eq!("".as_bytes(), Parser::new("::").parse_byte_sequence()?);
    Ok(())
}

#[test]
fn parse_byte_sequence_errors() {
    assert_eq!(
        Err(error::Repr::ExpectedStartOfByteSequence(0)),
        Parser::new("aGVsbG8").parse_byte_sequence()
    );
    assert_eq!(
        Err(error::Repr::InvalidByteSequence(6)),
        Parser::new(":aGVsb G8=:").parse_byte_sequence()
    );
    assert_eq!(
        Err(error::Repr::UnterminatedByteSequence(9)),
        Parser::new(":aGVsbG8=").parse_byte_sequence()
    );
}

#[test]
fn parse_number_int() -> Result<(), Error> {
    let mut parser = Parser::new("-733333333332d.14");
    assert_eq!(
        Num::Integer(integer(-733_333_333_332)),
        parser.parse_number()?
    );
    assert_eq!(parser.remaining(), b"d.14");

    assert_eq!(Num::Integer(integer(42)), Parser::new("42").parse_number()?);
    assert_eq!(
        Num::Integer(integer(-42)),
        Parser::new("-42").parse_number()?
    );
    assert_eq!(
        Num::Integer(integer(-42)),
        Parser::new("-042").parse_number()?
    );
    assert_eq!(Num::Integer(integer(0)), Parser::new("0").parse_number()?);
    assert_eq!(Num::Integer(integer(0)), Parser::new("00").parse_number()?);
    assert_eq!(
        Num::Integer(integer(123_456_789_012_345)),
        Parser::new("123456789012345").parse_number()?
    );
    assert_eq!(
        Num::Integer(integer(-123_456_789_012_345)),
        Parser::new("-123456789012345").parse_number()?
    );
    assert_eq!(Num::Integer(integer(2)), Parser::new("2,3").parse_number()?);
    assert_eq!(Num::Integer(integer(4)), Parser::new("4-2").parse_number()?);
    assert_eq!(
        Num::Integer(integer(-999_999_999_999_999)),
        Parser::new("-999999999999999").parse_number()?
    );
    assert_eq!(
        Num::Integer(integer(999_999_999_999_999)),
        Parser::new("999999999999999").parse_number()?
    );

    Ok(())
}

#[test]
fn parse_number_decimal() -> Result<(), Error> {
    let mut parser = Parser::new("00.42 test string");
    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(420))),
        parser.parse_number()?
    );
    assert_eq!(parser.remaining(), b" test string");

    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(1_500))),
        Parser::new("1.5.4.").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(1_800))),
        Parser::new("1.8.").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(1_700))),
        Parser::new("1.7.0").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(2_140))),
        Parser::new("2.14").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(-2_140))),
        Parser::new("-2.14").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(
            123_456_789_012_100
        ))),
        Parser::new("123456789012.1").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_integer_scaled_1000(integer(
            1_234_567_890_112
        ))),
        Parser::new("1234567890.112").parse_number()?
    );

    Ok(())
}

#[test]
fn parse_number_errors() {
    let mut parser = Parser::new(":aGVsbG8:rest");
    assert_eq!(Err(error::Repr::ExpectedDigit(0)), parser.parse_number());
    assert_eq!(parser.remaining(), b":aGVsbG8:rest");

    let mut parser = Parser::new("-11.5555 test string");
    assert_eq!(
        Err(error::Repr::TooManyDigitsAfterDecimalPoint(7)),
        parser.parse_number()
    );
    assert_eq!(parser.remaining(), b"5 test string");

    assert_eq!(
        Err(error::Repr::ExpectedDigit(1)),
        Parser::new("--0").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TooManyDigitsBeforeDecimalPoint(13)),
        Parser::new("1999999999999.1").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TrailingDecimalPoint(11)),
        Parser::new("19888899999.").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TooManyDigits(15)),
        Parser::new("1999999999999999").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TooManyDigitsAfterDecimalPoint(15)),
        Parser::new("19999999999.99991").parse_number()
    );
    assert_eq!(
        Err(error::Repr::ExpectedDigit(1)),
        Parser::new("- 42").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TrailingDecimalPoint(1)),
        Parser::new("1..4").parse_number()
    );
    assert_eq!(
        Err(error::Repr::ExpectedDigit(1)),
        Parser::new("-").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TrailingDecimalPoint(2)),
        Parser::new("-5. 14").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TrailingDecimalPoint(1)),
        Parser::new("7. 1").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TooManyDigitsAfterDecimalPoint(6)),
        Parser::new("-7.3333333333").parse_number()
    );
    assert_eq!(
        Err(error::Repr::TooManyDigitsBeforeDecimalPoint(14)),
        Parser::new("-7333333333323.12").parse_number()
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_string() -> Result<(), Error> {
    let input = r#";b="param_val""#;
    let expected = params!({"b" => string_ref("param_val")});
    let mut params = Parameters::new();
    Parser::new(input).parse_parameters(&mut params)?;
    assert_eq!(expected, params);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_bool() -> Result<(), Error> {
    let input = ";b;a";
    let expected = params!({"b" => true, "a" => true});
    let mut params = Parameters::new();
    Parser::new(input).parse_parameters(&mut params)?;
    assert_eq!(expected, params);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_mixed_types() -> Result<(), Error> {
    let input = ";key1=?0;key2=746.15";
    let expected = params!({
        "key1" => false,
        "key2" => Decimal::from_integer_scaled_1000(integer(746_150))
    });
    let mut params = Parameters::new();
    Parser::new(input).parse_parameters(&mut params)?;
    assert_eq!(expected, params);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_with_spaces() -> Result<(), Error> {
    let input = "; key1=?0; key2=11111";
    let expected = params!({"key1" => false, "key2" => 11111});
    let mut params = Parameters::new();
    Parser::new(input).parse_parameters(&mut params)?;
    assert_eq!(expected, params);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_empty() -> Result<(), Error> {
    let mut params = Parameters::new();
    Parser::new(" key1=?0; key2=11111").parse_parameters(&mut params)?;
    assert_eq!(Parameters::new(), params);
    Parser::new("").parse_parameters(&mut params)?;
    assert_eq!(Parameters::new(), params);
    Parser::new("[;a=1").parse_parameters(&mut params)?;
    assert_eq!(Parameters::new(), params);
    Parser::new("").parse_parameters(&mut params)?;
    assert_eq!(Parameters::new(), params);
    Ok(())
}

#[test]
fn parse_key() -> Result<(), Error> {
    assert_eq!(key_ref("a"), Parser::new("a=1").parse_key()?);
    assert_eq!(key_ref("a1"), Parser::new("a1=10").parse_key()?);
    assert_eq!(key_ref("*1"), Parser::new("*1=10").parse_key()?);
    assert_eq!(key_ref("f"), Parser::new("f[f=10").parse_key()?);
    Ok(())
}

#[test]
fn parse_key_errors() {
    assert_eq!(
        Err(error::Repr::ExpectedStartOfKey(0)),
        Parser::new("[*f=10").parse_key()
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_more_list() -> Result<(), Error> {
    let expected_list = list![inner_list![1, 2], 42];

    let mut parsed_header: List = Parser::new("(1 2)").parse()?;
    Parser::new("42").parse_list_with_visitor(&mut parsed_header)?;
    assert_eq!(expected_list, parsed_header);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_more_dict() -> Result<(), Error> {
    let expected_dict = dict! {
        "a" => 1,
        "b" => item!(true; {"foo" => token_ref("*")}),
        "c" => 3
    };

    let mut parsed_header: Dictionary = Parser::new("a=1, b;foo=*\t\t").parse()?;
    Parser::new(" c=3").parse_dictionary_with_visitor(&mut parsed_header)?;
    assert_eq!(expected_dict, parsed_header);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_more_errors() -> Result<(), Error> {
    let mut parsed_dict_header: Dictionary = Parser::new("a=1, b;foo=*").parse()?;
    assert!(Parser::new(",a")
        .parse_dictionary_with_visitor(&mut parsed_dict_header)
        .is_err());

    let mut parsed_list_header: List = Parser::new("a, b;foo=*").parse()?;
    assert!(Parser::new("(a, 2)")
        .parse_list_with_visitor(&mut parsed_list_header)
        .is_err());
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_date() -> Result<(), Error> {
    let input = "@0";

    assert!(Parser::new(input)
        .with_version(Version::Rfc8941)
        .parse::<Item>()
        .is_err());

    assert_eq!(Parser::new(input).parse::<Item>()?, item!(Date::UNIX_EPOCH));

    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_display_string() -> Result<(), Error> {
    let input = r#"%"This is intended for display to %c3%bcsers.""#;

    assert!(Parser::new(input)
        .with_version(Version::Rfc8941)
        .parse::<Item>()
        .is_err());

    assert_eq!(
        Parser::new(input).parse::<Item>()?,
        item!(BareItem::DisplayString(
            "This is intended for display to üsers.".to_owned()
        ))
    );

    Ok(())
}

#[test]
fn parse_display_string_errors() {
    assert_eq!(
        Parser::new(" %").parse_item_with_visitor(Ignored),
        Err(error::Repr::ExpectedQuote(2).into())
    );

    assert_eq!(
        Parser::new(r#" %""#).parse_item_with_visitor(Ignored),
        Err(error::Repr::UnterminatedDisplayString(3).into())
    );

    assert_eq!(
        Parser::new(r#" %"%"#).parse_item_with_visitor(Ignored),
        Err(error::Repr::UnterminatedEscapeSequence(4).into())
    );

    assert_eq!(
        Parser::new(r#" %"%a"#).parse_item_with_visitor(Ignored),
        Err(error::Repr::UnterminatedEscapeSequence(5).into())
    );

    assert_eq!(
        Parser::new(r#" %"%A"#).parse_item_with_visitor(Ignored),
        Err(error::Repr::InvalidEscapeSequence(4).into())
    );

    assert_eq!(
        Parser::new(r#" %"%aA"#).parse_item_with_visitor(Ignored),
        Err(error::Repr::InvalidEscapeSequence(5).into())
    );

    assert_eq!(
        Parser::new(r#" %"x%aa""#).parse_item_with_visitor(Ignored),
        Err(error::Repr::InvalidUtf8InDisplayString(4).into())
    );
}

/// A simple struct used for the complex tests.
#[derive(Default, Debug, PartialEq)]
struct Point {
    x: i64,
    y: i64,
}

impl Point {
    fn new(x: i64, y: i64) -> Self {
        Self { x, y }
    }
}

// For when a `Point` is a parameter somewhere.
impl<'de> ParameterVisitor<'de> for &mut Point {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        let Some(v) = value.as_integer() else {
            return Ok(());
        };
        let ptr = match key.as_str() {
            "x" => &mut self.x,
            "y" => &mut self.y,
            _ => return Ok(()),
        };
        *ptr = i64::from(v);
        Ok(())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

impl<'de> DictionaryVisitor<'de> for &mut Point {
    type Out = ();
    type Error = Infallible;

    fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
        let coord = match key.as_str() {
            "x" => &mut self.x,
            "y" => &mut self.y,
            _ => return Ok(None),
        };
        Ok(Some(CoordVisitor { coord }))
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

struct CoordVisitor<'a> {
    coord: &'a mut i64,
}

impl<'de> ItemVisitor<'de> for CoordVisitor<'_> {
    type Out = ();
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        if let Some(v) = bare_item.as_integer() {
            *self.coord = i64::from(v);
        }
        Ok(Ignored)
    }
}

impl<'de> EntryVisitor<'de> for CoordVisitor<'_> {
    type Error = Infallible;

    fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(self)
    }

    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

#[test]
fn complex_dict_visitor() {
    let mut point = Point::default();
    Parser::new("x=10, y=3")
        .parse_dictionary_with_visitor(&mut point)
        .expect("successful parse");
    assert_eq!(point, Point::new(10, 3));
}

// An item that is an integer, with optional `x` and `y` parameters.
#[derive(Default, Debug, PartialEq)]
struct Holder {
    v: i64,
    point: Point,
}

impl<'de> ItemVisitor<'de> for &mut Holder {
    type Out = Option<()>;
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        Ok(if let Some(v) = bare_item.as_integer() {
            self.v = i64::from(v);
            Some(&mut self.point)
        } else {
            None
        })
    }
}

#[test]
fn complex_item_visitor() {
    let mut holder = Holder::default();
    Parser::new("12;x=7;y=-5")
        .parse_item_with_visitor(&mut holder)
        .expect("successful parse");
    assert_eq!(holder.point, Point::new(7, -5));
}

#[test]
fn complex_list_visitor() {
    #[derive(Default, Debug, PartialEq)]
    struct ListHolder {
        list: Vec<Holder>,
        point: Point,
    }

    impl<'de> ListVisitor<'de> for Vec<ListHolder> {
        type Out = Self;
        type Error = Infallible;

        fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error> {
            self.push(ListHolder::default());
            Ok(self.last_mut().unwrap()) // cannot fail
        }

        fn finish(self) -> Result<Self::Out, Self::Error> {
            Ok(self)
        }
    }

    impl<'de> EntryVisitor<'de> for &mut ListHolder {
        type Error = Infallible;

        fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
            Ok(Ignored)
        }

        fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
            Ok(self)
        }
    }

    impl<'de> InnerListVisitor<'de> for &mut ListHolder {
        type Error = Infallible;

        fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
            self.list.push(Holder::default());
            Ok(self.list.last_mut().unwrap()) // cannot fail
        }

        fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
            Ok(&mut self.point)
        }
    }

    let list: Vec<ListHolder> = Parser::new("(1;x=4 2;y=5 3);x=1;y=2,(4;x=12;y=33), ()")
        .parse_list()
        .expect("successful parse");

    let expected = vec![
        ListHolder {
            list: vec![
                Holder {
                    v: 1,
                    point: Point::new(4, 0),
                },
                Holder {
                    v: 2,
                    point: Point::new(0, 5),
                },
                Holder {
                    v: 3,
                    point: Point::default(),
                },
            ],
            point: Point::new(1, 2),
        },
        ListHolder {
            list: vec![Holder {
                v: 4,
                point: Point::new(12, 33),
            }],
            point: Point::default(),
        },
        ListHolder {
            list: Vec::new(),
            point: Point::default(),
        },
    ];

    assert_eq!(list, expected);
}

// Regression test for https://github.com/undef1nd/sfv/issues/194.
// This test does not compile without the associated fix.
#[test]
fn parse_dictionary_lifetime() -> Result<(), Error> {
    struct Visitor<'de>(Option<&'de KeyRef>);

    impl<'de> DictionaryVisitor<'de> for Visitor<'de> {
        type Out = Option<&'de KeyRef>;
        type Error = Infallible;

        fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
            self.0 = Some(key);
            Ok(Ignored)
        }

        fn finish(self) -> Result<Self::Out, Self::Error> {
            Ok(self.0)
        }
    }

    assert_eq!(
        Parser::new("a=1").parse_dictionary_with_visitor(Visitor(None))?,
        Some(key_ref("a"))
    );
    Ok(())
}
