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
use crate::{BareItem, Date, Dictionary, InnerList, Item, List, Parameters, Version};

#[test]
#[cfg(feature = "parsed-types")]
fn parse() -> Result<(), Error> {
    let input = r#""some_value""#;
    let parsed_item = Item::new(string_ref("some_value"));
    let expected = parsed_item;
    assert_eq!(expected, Parser::new(input).parse::<Item>()?);

    let input = "12.35;a ";
    let params = Parameters::from_iter(vec![(key_ref("a").to_owned(), BareItem::Boolean(true))]);
    let expected = Item::with_params(Decimal::try_from(12.35)?, params);

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
    let item1 = Item::new(1);
    let item2 = Item::new(42);
    let expected_list: List = vec![item1.into(), item2.into()];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_with_multiple_spaces() -> Result<(), Error> {
    let input = "1  ,  42";
    let item1 = Item::new(1);
    let item2 = Item::new(42);
    let expected_list: List = vec![item1.into(), item2.into()];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_of_lists() -> Result<(), Error> {
    let input = "(1 2), (42 43)";
    let item1 = Item::new(1);
    let item2 = Item::new(2);
    let item3 = Item::new(42);
    let item4 = Item::new(43);
    let inner_list_1 = InnerList::new(vec![item1, item2]);
    let inner_list_2 = InnerList::new(vec![item3, item4]);
    let expected_list: List = vec![inner_list_1.into(), inner_list_2.into()];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_empty_inner_list() -> Result<(), Error> {
    let input = "()";
    let inner_list = InnerList::new(vec![]);
    let expected_list: List = vec![inner_list.into()];
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
    let item1 = Item::new(1);
    let item2 = Item::new(42);
    let inner_list_param = Parameters::from_iter(vec![(
        key_ref("k").to_owned(),
        BareItem::Token(token_ref("*").to_owned()),
    )]);
    let inner_list = InnerList::with_params(vec![item1, item2], inner_list_param);
    let expected_list: List = vec![inner_list.into()];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_list_of_items_and_lists_with_param() -> Result<(), Error> {
    let input = r#"12, 14, (a  b); param="param_value_1", ()"#;
    let item1 = Item::new(12);
    let item2 = Item::new(14);
    let item3 = Item::new(token_ref("a"));
    let item4 = Item::new(token_ref("b"));
    let inner_list_param = Parameters::from_iter(vec![(
        key_ref("param").to_owned(),
        BareItem::String(string_ref("param_value_1").to_owned()),
    )]);
    let inner_list = InnerList::with_params(vec![item3, item4], inner_list_param);
    let empty_inner_list = InnerList::new(vec![]);
    let expected_list: List = vec![
        item1.into(),
        item2.into(),
        inner_list.into(),
        empty_inner_list.into(),
    ];
    assert_eq!(expected_list, Parser::new(input).parse::<List>()?);
    Ok(())
}

#[test]
fn parse_list_errors() {
    let input = ",";
    assert_eq!(
        Err(error::Repr::ExpectedStartOfBareItem(0).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
    );

    let input = "a, b c";
    assert_eq!(
        Err(error::Repr::TrailingCharactersAfterMember(5).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
    );

    let input = "a,";
    assert_eq!(
        Err(error::Repr::TrailingComma(1).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
    );

    let input = "a     ,    ";
    assert_eq!(
        Err(error::Repr::TrailingComma(6).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
    );

    let input = "a\t \t ,\t ";
    assert_eq!(
        Err(error::Repr::TrailingComma(5).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
    );

    let input = "a\t\t,\t\t\t";
    assert_eq!(
        Err(error::Repr::TrailingComma(3).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
    );

    let input = "(a b),";
    assert_eq!(
        Err(error::Repr::TrailingComma(5).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
    );

    let input = "(1, 2, (a b)";
    assert_eq!(
        Err(error::Repr::ExpectedInnerListDelimiter(2).into()),
        Parser::new(input).parse_list_with_visitor(&mut Ignored)
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
    let inner_list_param = Parameters::from_iter(vec![(key_ref("a").to_owned(), 1.into())]);

    let item1 = Item::new(token_ref("c"));
    let item2 = Item::new(token_ref("b"));
    let expected = InnerList::with_params(vec![item1, item2], inner_list_param);
    let mut inner_list = InnerList::default();
    Parser::new(input).parse_inner_list(&mut inner_list)?;
    assert_eq!(expected, inner_list);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_item_int_with_space() -> Result<(), Error> {
    let input = "12 ";
    assert_eq!(Item::new(12), Parser::new(input).parse::<Item>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_item_decimal_with_bool_param_and_space() -> Result<(), Error> {
    let input = "12.35;a ";
    let param = Parameters::from_iter(vec![(key_ref("a").to_owned(), BareItem::Boolean(true))]);
    assert_eq!(
        Item::with_params(Decimal::try_from(12.35)?, param),
        Parser::new(input).parse::<Item>()?
    );
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_item_number_with_param() -> Result<(), Error> {
    let param = Parameters::from_iter(vec![(
        key_ref("a1").to_owned(),
        BareItem::Token(token_ref("*").to_owned()),
    )]);
    assert_eq!(
        Item::with_params(string_ref("12.35"), param),
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
        Parser::new(input).parse_dictionary_with_visitor(&mut Ignored)
    );
    let input = "abc=123;a=1,";
    assert_eq!(
        Err(error::Repr::TrailingComma(11).into()),
        Parser::new(input).parse_dictionary_with_visitor(&mut Ignored)
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_with_spaces_and_params() -> Result<(), Error> {
    let input = r#"abc=123;a=1;b=2, def=456, ghi=789;q=9;r="+w""#;
    let item1_params = Parameters::from_iter(vec![
        (key_ref("a").to_owned(), 1.into()),
        (key_ref("b").to_owned(), 2.into()),
    ]);
    let item3_params = Parameters::from_iter(vec![
        (key_ref("q").to_owned(), 9.into()),
        (
            key_ref("r").to_owned(),
            BareItem::String(string_ref("+w").to_owned()),
        ),
    ]);

    let item1 = Item::with_params(123, item1_params);
    let item2 = Item::new(456);
    let item3 = Item::with_params(789, item3_params);

    let expected_dict = Dictionary::from_iter(vec![
        (key_ref("abc").to_owned(), item1.into()),
        (key_ref("def").to_owned(), item2.into()),
        (key_ref("ghi").to_owned(), item3.into()),
    ]);
    assert_eq!(expected_dict, Parser::new(input).parse::<Dictionary>()?);

    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_empty_value() -> Result<(), Error> {
    let input = "a=()";
    let inner_list = InnerList::new(vec![]);
    let expected_dict = Dictionary::from_iter(vec![(key_ref("a").to_owned(), inner_list.into())]);
    assert_eq!(expected_dict, Parser::new(input).parse::<Dictionary>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_with_token_param() -> Result<(), Error> {
    let input = "a=1, b;foo=*, c=3";
    let item2_params = Parameters::from_iter(vec![(
        key_ref("foo").to_owned(),
        BareItem::Token(token_ref("*").to_owned()),
    )]);
    let item1 = Item::new(1);
    let item2 = Item::with_params(true, item2_params);
    let item3 = Item::new(3);
    let expected_dict = Dictionary::from_iter(vec![
        (key_ref("a").to_owned(), item1.into()),
        (key_ref("b").to_owned(), item2.into()),
        (key_ref("c").to_owned(), item3.into()),
    ]);
    assert_eq!(expected_dict, Parser::new(input).parse::<Dictionary>()?);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_dict_multiple_spaces() -> Result<(), Error> {
    // input1, input2, input3 must be parsed into the same structure
    let item1 = Item::new(1);
    let item2 = Item::new(2);
    let expected_dict = Dictionary::from_iter(vec![
        (key_ref("a").to_owned(), item1.into()),
        (key_ref("b").to_owned(), item2.into()),
    ]);

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
        RefBareItem::Decimal(Decimal::try_from(-3.55)?),
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
        Num::Decimal(Decimal::try_from(0.42)?),
        parser.parse_number()?
    );
    assert_eq!(parser.remaining(), b" test string");

    assert_eq!(
        Num::Decimal(Decimal::try_from(1.5)?),
        Parser::new("1.5.4.").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::try_from(1.8)?),
        Parser::new("1.8.").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::try_from(1.7)?),
        Parser::new("1.7.0").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::try_from(2.14)?),
        Parser::new("2.14").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::try_from(-2.14)?),
        Parser::new("-2.14").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::try_from(123_456_789_012.1)?),
        Parser::new("123456789012.1").parse_number()?
    );
    assert_eq!(
        Num::Decimal(Decimal::try_from(1_234_567_890.112)?),
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
    let expected = Parameters::from_iter(vec![(
        key_ref("b").to_owned(),
        BareItem::String(string_ref("param_val").to_owned()),
    )]);
    let mut params = Parameters::new();
    Parser::new(input).parse_parameters(&mut params)?;
    assert_eq!(expected, params);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_bool() -> Result<(), Error> {
    let input = ";b;a";
    let expected = Parameters::from_iter(vec![
        (key_ref("b").to_owned(), BareItem::Boolean(true)),
        (key_ref("a").to_owned(), BareItem::Boolean(true)),
    ]);
    let mut params = Parameters::new();
    Parser::new(input).parse_parameters(&mut params)?;
    assert_eq!(expected, params);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_mixed_types() -> Result<(), Error> {
    let input = ";key1=?0;key2=746.15";
    let expected = Parameters::from_iter(vec![
        (key_ref("key1").to_owned(), BareItem::Boolean(false)),
        (
            key_ref("key2").to_owned(),
            Decimal::try_from(746.15)?.into(),
        ),
    ]);
    let mut params = Parameters::new();
    Parser::new(input).parse_parameters(&mut params)?;
    assert_eq!(expected, params);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_params_with_spaces() -> Result<(), Error> {
    let input = "; key1=?0; key2=11111";
    let expected = Parameters::from_iter(vec![
        (key_ref("key1").to_owned(), BareItem::Boolean(false)),
        (key_ref("key2").to_owned(), 11111.into()),
    ]);
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
    let item1 = Item::new(1);
    let item2 = Item::new(2);
    let item3 = Item::new(42);
    let inner_list_1 = InnerList::new(vec![item1, item2]);
    let expected_list: List = vec![inner_list_1.into(), item3.into()];

    let mut parsed_header: List = Parser::new("(1 2)").parse()?;
    Parser::new("42").parse_list_with_visitor(&mut parsed_header)?;
    assert_eq!(expected_list, parsed_header);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_more_dict() -> Result<(), Error> {
    let item2_params = Parameters::from_iter(vec![(
        key_ref("foo").to_owned(),
        BareItem::Token(token_ref("*").to_owned()),
    )]);
    let item1 = Item::new(1);
    let item2 = Item::with_params(true, item2_params);
    let item3 = Item::new(3);
    let expected_dict = Dictionary::from_iter(vec![
        (key_ref("a").to_owned(), item1.into()),
        (key_ref("b").to_owned(), item2.into()),
        (key_ref("c").to_owned(), item3.into()),
    ]);

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

    assert_eq!(
        Parser::new(input).parse::<Item>()?,
        Item::new(Date::UNIX_EPOCH)
    );

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
        Item::new(BareItem::DisplayString(
            "This is intended for display to üsers.".to_owned()
        ))
    );

    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_display_string_errors() {
    assert_eq!(
        Parser::new(" %").parse::<Item>(),
        Err(error::Repr::ExpectedQuote(2).into())
    );

    assert_eq!(
        Parser::new(r#" %""#).parse::<Item>(),
        Err(error::Repr::UnterminatedDisplayString(3).into())
    );

    assert_eq!(
        Parser::new(r#" %"%"#).parse::<Item>(),
        Err(error::Repr::UnterminatedEscapeSequence(4).into())
    );

    assert_eq!(
        Parser::new(r#" %"%a"#).parse::<Item>(),
        Err(error::Repr::UnterminatedEscapeSequence(5).into())
    );

    assert_eq!(
        Parser::new(r#" %"%A"#).parse::<Item>(),
        Err(error::Repr::InvalidEscapeSequence(4).into())
    );

    assert_eq!(
        Parser::new(r#" %"%aA"#).parse::<Item>(),
        Err(error::Repr::InvalidEscapeSequence(5).into())
    );

    assert_eq!(
        Parser::new(r#" %"x%aa""#).parse::<Item>(),
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

struct PointVisitor<'a> {
    point: &'a mut Point,
}

/// For when a `Point` is a parameter somewhere.
impl<'de> ParameterVisitor<'de> for PointVisitor<'_> {
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
            "x" => &mut self.point.x,
            "y" => &mut self.point.y,
            _ => return Ok(()),
        };
        *ptr = i64::from(v);
        Ok(())
    }
}

impl<'de> DictionaryVisitor<'de> for PointVisitor<'_> {
    type Error = Infallible;

    fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
        let coord = match key.as_str() {
            "x" => &mut self.point.x,
            "y" => &mut self.point.y,
            _ => return Ok(None),
        };
        Ok(Some(CoordVisitor { coord }))
    }
}

struct CoordVisitor<'a> {
    coord: &'a mut i64,
}

impl<'de> ItemVisitor<'de> for CoordVisitor<'_> {
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        if let Some(v) = bare_item.as_integer() {
            *self.coord = i64::from(v);
        }
        Ok(None::<Ignored>)
    }
}

impl<'de> EntryVisitor<'de> for CoordVisitor<'_> {
    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

#[test]
fn complex_dict_visitor() {
    let mut point = Point::default();
    let parser = Parser::new("x=10, y=3");
    let mut visit = PointVisitor { point: &mut point };
    parser
        .parse_dictionary_with_visitor(&mut visit)
        .expect("successful parse");
    assert_eq!(point, Point::new(10, 3));
}

/// An item with parameters.
#[derive(Default, Debug, PartialEq)]
struct Holder {
    v: i64,
    point: Point,
}

/// A visitor for an item that is an integer, with optional `x` and `y` parameters.
struct HolderVisitor<'a> {
    holder: &'a mut Holder,
}

impl<'de> ItemVisitor<'de> for HolderVisitor<'_> {
    type Error = Infallible;
    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        Ok(if let Some(v) = bare_item.as_integer() {
            self.holder.v = i64::from(v);
            Some(PointVisitor {
                point: &mut self.holder.point,
            })
        } else {
            None
        })
    }
}

#[test]
fn complex_item_visitor() {
    let mut holder = Holder::default();
    let parser = Parser::new("12;x=7;y=-5");
    let visit = HolderVisitor {
        holder: &mut holder,
    };
    parser
        .parse_item_with_visitor(visit)
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

    struct OuterListVisitor<'a> {
        list: &'a mut Vec<ListHolder>,
    }

    impl<'de> ListVisitor<'de> for OuterListVisitor<'_> {
        type Error = Infallible;
        fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error> {
            self.list.push(ListHolder::default());
            let list = self.list.last_mut().unwrap(); // cannot fail
            Ok(HolderListVisitor { list })
        }
    }

    struct HolderListVisitor<'a> {
        list: &'a mut ListHolder,
    }

    impl<'de> ItemVisitor<'de> for HolderListVisitor<'_> {
        type Error = Infallible;
        fn bare_item(
            self,
            _bare_item: BareItemFromInput<'de>,
        ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
            Ok(None::<Ignored>)
        }
    }

    impl<'de> EntryVisitor<'de> for HolderListVisitor<'_> {
        fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
            Ok(self)
        }
    }

    impl<'de> InnerListVisitor<'de> for HolderListVisitor<'_> {
        type Error = Infallible;

        fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
            self.list.list.push(Holder::default());
            let holder = self.list.list.last_mut().unwrap(); // cannot fail
            Ok(HolderVisitor { holder })
        }

        fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
            let point = &mut self.list.point;
            Ok(Some(PointVisitor { point }))
        }
    }

    let mut list = Vec::default();
    let parser = Parser::new("(1;x=4 2;y=5 3);x=1;y=2,(4;x=12;y=33), ()");
    let mut visit = OuterListVisitor { list: &mut list };
    parser
        .parse_list_with_visitor(&mut visit)
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
        type Error = Infallible;

        fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
            self.0 = Some(key);
            Ok(Ignored)
        }
    }

    let mut visitor = Visitor(None);
    Parser::new("a=1").parse_dictionary_with_visitor(&mut visitor)?;
    assert_eq!(visitor.0, Some(key_ref("a")));
    Ok(())
}
