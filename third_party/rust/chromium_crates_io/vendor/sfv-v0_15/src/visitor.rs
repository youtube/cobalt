/*!
Contains traits for parsing structured-field values incrementally.

These can be used to borrow data from the input without copies in some cases.

The various visitor methods are invoked *during* parsing, i.e. before validation
of the entire input is complete. Therefore, users of these traits should
carefully consider whether they want to induce side effects or perform expensive
operations *before* knowing whether the entire input is valid.

For example, it may make sense to defer storage of these values in a database
until after validation is complete, in order to avoid the need for rollbacks in
the event that a later error occurs. In this case, the visitor could retain the
relevant state in its fields, before using that state to perform the operation
*after* parsing is complete:

```
# use sfv::visitor::{Ignored, ItemVisitor, ParameterVisitor};
# use sfv::{BareItemFromInput, TokenRef, token_ref};
# fn main() -> Result<(), sfv::Error> {
struct Visitor<'de> {
    token: Option<&'de TokenRef>,
}

impl<'de> ItemVisitor<'de> for &mut Visitor<'de> {
  type Out = ();
  type Error = std::convert::Infallible;

  fn bare_item(self, bare_item: BareItemFromInput<'de>) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
      self.token =
          if let BareItemFromInput::Token(token) = bare_item {
              Some(token)
          } else {
              None
          };

      Ok(Ignored)
  }
}

let mut visitor = Visitor { token: None };

sfv::Parser::new("abc").parse_item_with_visitor(&mut visitor)?;

assert_eq!(visitor.token, Some(token_ref("abc")));

// Use `visitor.token` to do something expensive or with side effects now that
// we know the entire input is valid.
# Ok(())
# }
```

# Returning a value from `ItemVisitor`

If a top-level item is being parsed, the visitor can return the value directly.
The previous example can be written more concisely as:

```
# use sfv::visitor::{Ignored, ItemVisitor, ParameterVisitor, parameter_visitor_with};
# use sfv::{BareItemFromInput, TokenRef, token_ref};
# fn main() -> Result<(), sfv::Error> {
struct Visitor;

impl<'de> ItemVisitor<'de> for Visitor {
  type Out = Option<&'de TokenRef>;
  type Error = std::convert::Infallible;

  fn bare_item(self, bare_item: BareItemFromInput<'de>) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
      Ok(parameter_visitor_with(Ignored, move |_| {
        Ok(if let BareItemFromInput::Token(token) = bare_item {
            Some(token)
        } else {
            None
        })
     }))
  }
}

assert_eq!(
  Some(token_ref("abc")),
  sfv::Parser::new("abc").parse_item_with_visitor(Visitor)?,
);
# Ok(())
# }
```

Or without the `Option` at all:

```
# use sfv::visitor::{Ignored, ItemVisitor, ParameterVisitor, parameter_visitor_with};
# use sfv::{BareItemFromInput, TokenRef, token_ref};
# fn main() -> Result<(), sfv::Error> {
struct Visitor;

#[derive(Debug)]
struct ExpectedToken;

impl std::fmt::Display for ExpectedToken {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str("expected token")
    }
}

impl std::error::Error for ExpectedToken {}

impl<'de> ItemVisitor<'de> for Visitor {
  type Out = &'de TokenRef;
  type Error = ExpectedToken;

  fn bare_item(self, bare_item: BareItemFromInput<'de>) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
      if let BareItemFromInput::Token(token) = bare_item {
          Ok(parameter_visitor_with(Ignored, move |_| Ok(token)))
      } else {
          Err(ExpectedToken)
      }
  }
}

assert_eq!(
  token_ref("abc"),
  sfv::Parser::new("abc").parse_item_with_visitor(Visitor)?,
);

assert!(sfv::Parser::new("123").parse_item_with_visitor(Visitor).is_err());
# Ok(())
# }
```

Or using a function, given the blanket implementation of [`ItemVisitor`] for [`FnOnce`]:

```
# use sfv::visitor::{Ignored, ParameterVisitor, parameter_visitor_with};
# use sfv::{BareItemFromInput, TokenRef, token_ref};
# fn main() -> Result<(), sfv::Error> {
# #[derive(Debug)]
# struct ExpectedToken;
#
# impl std::fmt::Display for ExpectedToken {
#    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
#        f.write_str("expected token")
#    }
# }
#
# impl std::error::Error for ExpectedToken {}
#
fn as_token<'de>(bare_item: BareItemFromInput<'de>) -> Result<impl ParameterVisitor<'de, Out = &'de TokenRef>, ExpectedToken> {
  if let BareItemFromInput::Token(token) = bare_item {
      Ok(parameter_visitor_with(Ignored, move |_| Ok(token)))
  } else {
      Err(ExpectedToken)
  }
}

assert_eq!(
  token_ref("abc"),
  sfv::Parser::new("abc").parse_item_with_visitor(as_token)?,
);
# Ok(())
# }
```

Or even:

```
# use sfv::{TokenRef, token_ref};
# fn main() -> Result<(), sfv::Error> {
assert_eq!(
  token_ref("abc"),
  sfv::Parser::new("abc").parse_item::<&TokenRef>()?,
);
# Ok(())
# }
```

# Discarding irrelevant parts

Two kinds of helpers are provided for silently discarding structured-field
parts:

- [`Ignored`]: This type implements all of the visitor traits as no-ops, and can
  be used when a visitor implementation would unconditionally do nothing. An
  example of this is when an item's bare item needs to be validated, but its
  parameters do not (e.g. because the relevant field definition prescribes
  none and permits unknown ones).

- Blanket implementations of [`ParameterVisitor`], [`ItemVisitor`],
  [`EntryVisitor`], and [`InnerListVisitor`] for [`Option<V>`] where `V`
  implements that trait: These implementations act like `Ignored` when `self` is
  [`None`], and forward to `V`'s implementation when `self` is [`Some`]. These
  can be used when the visitor dynamically handles or ignores field parts. An
  example of this is when a field definition prescribes the format of certain
  dictionary keys, but ignores unknown ones.

Note that the discarded parts are still validated during parsing: syntactic
errors in the input still cause parsing to fail even when these helpers are
used, [as required by RFC 9651](https://httpwg.org/specs/rfc9651.html#strict).

The following example demonstrates usage of both kinds of helpers:

```
# use sfv::{BareItemFromInput, KeyRef, Parser, visitor::*};
#[derive(Debug, Default, PartialEq)]
struct Point {
    x: i64,
    y: i64,
}

struct CoordVisitor<'a> {
    coord: &'a mut i64,
}

impl<'de> DictionaryVisitor<'de> for Point {
    type Out = Self;
    type Error = std::convert::Infallible;

    fn entry(
        &mut self,
        key: &'de KeyRef,
    ) -> Result<impl EntryVisitor<'de>, Self::Error>
    {
        let coord = match key.as_str() {
            "x" => &mut self.x,
            "y" => &mut self.y,
            // Ignore this key by returning `None`. Its value will still be
            // validated syntactically during parsing, but we don't need to
            // visit it.
            _ => return Ok(None),
        };
        // Visit this key's value by returning `Some`.
        Ok(Some(CoordVisitor { coord }))
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(self)
    }
}

#[derive(Debug)]
struct NotAnInteger;

impl std::fmt::Display for NotAnInteger {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str("must be an integer")
    }
}

impl std::error::Error for NotAnInteger {}

impl<'de> EntryVisitor<'de> for CoordVisitor<'_> {
    type Error = NotAnInteger;

    fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(|bare_item: BareItemFromInput<'de>| {
            if let BareItemFromInput::Integer(v) = bare_item {
                *self.coord = i64::from(v);
                // Ignore the item's parameters by returning `Ignored`. The
                // parameters will still be validated syntactically during parsing,
                // but we don't need to visit them.
                //
                // We could return `None` instead to ignore the parameters only
                // some of the time, returning `Some(visitor)` otherwise.
                Ok(Ignored)
            } else {
                Err(NotAnInteger)
            }
        })
    }

    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        // Use `Never` to enforce at the type level that this method will only
        // return `Err`, as our coordinate must be a single integer, not an
        // inner list.
        Err::<Never, _>(NotAnInteger)
    }
}

# fn main() -> Result<(), sfv::Error> {
assert_eq!(
    Parser::new("x=10, z=abc, y=3").parse_dictionary_with_visitor(Point::default())?,
    Point { x: 10, y: 3 });
# Ok(())
# }
```
*/

use std::{convert::Infallible, error::Error};

use crate::{BareItemFromInput, KeyRef};

/// A visitor whose methods are called during parameter parsing.
///
/// The lifetime `'de` is the lifetime of the input.
pub trait ParameterVisitor<'de> {
    /// The successful return type of the [`ParameterVisitor::finish`] method.
    ///
    /// Many implementations will set this to `()`. See
    /// [the module documentation](crate::visitor#returning-a-value-from-itemvisitor)
    /// for an example that does not.
    type Out;

    /// The error type that can be returned if some error occurs during parsing.
    type Error: Error;

    /// Called after a parameter has been parsed.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// Note: Per [RFC 9651], when duplicate parameter keys are encountered in
    /// the same scope, all but the last instance are ignored. Implementations
    /// of this trait must respect that requirement in order to comply with the
    /// specification. For example, if parameters are stored in a map, earlier
    /// values for a given parameter key must be overwritten by later ones.
    ///
    /// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html#parse-param>
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error>;

    /// Called after all parameters have been parsed.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn finish(self) -> Result<Self::Out, Self::Error>;
}

/// A visitor whose methods are called during item parsing.
///
/// The lifetime `'de` is the lifetime of the input.
///
/// Use this trait with
/// [`Parser::parse_item_with_visitor`][crate::Parser::parse_item_with_visitor].
pub trait ItemVisitor<'de> {
    /// The successful return type of the returned [`ParameterVisitor::finish`]
    /// method.
    ///
    /// Many implementations will set this to `()`. See
    /// [the module documentation](crate::visitor#returning-a-value-from-itemvisitor)
    /// for an example that does not.
    type Out;

    /// The error type that can be returned if some error occurs during parsing.
    type Error: Error;

    /// Called after a bare item has been parsed.
    ///
    /// The returned visitor is used to handle the bare item's parameters.
    /// See [the module documentation](crate::visitor#discarding-irrelevant-parts)
    /// for guidance on discarding parameters.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error>;
}

impl<'de, F, V, E> ItemVisitor<'de> for F
where
    F: FnOnce(BareItemFromInput<'de>) -> Result<V, E>,
    V: ParameterVisitor<'de>,
    E: Error,
{
    type Out = V::Out;
    type Error = E;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        self(bare_item)
    }
}

/// A visitor whose methods are called during inner-list parsing.
///
/// The lifetime `'de` is the lifetime of the input.
pub trait InnerListVisitor<'de> {
    /// The error type that can be returned if some error occurs during parsing.
    type Error: Error;

    /// Called before an item has been parsed.
    ///
    /// The returned visitor is used to handle the bare item.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error>;

    /// Called after all inner-list items have been parsed.
    ///
    /// The returned visitor is used to handle the inner list's parameters.
    /// See [the module documentation](crate::visitor#discarding-irrelevant-parts)
    /// for guidance on discarding parameters.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error>;
}

/// A visitor whose methods are called during entry parsing.
///
/// The lifetime `'de` is the lifetime of the input.
pub trait EntryVisitor<'de> {
    /// The error type that can be returned if some error occurs during parsing.
    type Error: Error;

    /// Called before an item has been parsed.
    ///
    /// The returned visitor is used to handle the item.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error>;

    /// Called before an inner list has been parsed.
    ///
    /// The returned visitor is used to handle the inner list.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error>;
}

/// A visitor whose methods are called during dictionary parsing.
///
/// The lifetime `'de` is the lifetime of the input.
///
/// Use this trait with
/// [`Parser::parse_dictionary_with_visitor`][crate::Parser::parse_dictionary_with_visitor].
pub trait DictionaryVisitor<'de> {
    /// The successful return type of the [`DictionaryVisitor::finish`] method.
    type Out;

    /// The error type that can be returned if some error occurs during parsing.
    type Error: Error;

    /// Called after a dictionary key has been parsed.
    ///
    /// The returned visitor is used to handle the associated value.
    /// See [the module documentation](crate::visitor#discarding-irrelevant-parts)
    /// for guidance on discarding entries.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// Note: Per [RFC 9651], when duplicate dictionary keys are encountered in
    /// the same scope, all but the last instance are ignored. Implementations
    /// of this trait must respect that requirement in order to comply with the
    /// specification. For example, if dictionary entries are stored in a map,
    /// earlier values for a given dictionary key must be overwritten by later
    /// ones.
    ///
    /// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html#parse-dictionary>
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error>;

    /// Called after all dictionary keys have been parsed.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn finish(self) -> Result<Self::Out, Self::Error>;
}

/// A visitor whose methods are called during list parsing.
///
/// The lifetime `'de` is the lifetime of the input.
///
/// Use this trait with
/// [`Parser::parse_list_with_visitor`][crate::Parser::parse_list_with_visitor].
pub trait ListVisitor<'de> {
    /// The successful return type of the [`ListVisitor::finish`] method.
    type Out;

    /// The error type that can be returned if some error occurs during parsing.
    type Error: Error;

    /// Called before a list entry has been parsed.
    ///
    /// The returned visitor is used to handle the entry.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error>;

    /// Called after all list entries have been parsed.
    ///
    /// Parsing will be terminated early if an error is returned.
    ///
    /// # Errors
    /// The error result should report the reason for any failed validation.
    fn finish(self) -> Result<Self::Out, Self::Error>;
}

/// A visitor that can be used to silently discard structured-field parts.
///
/// Note that the discarded parts are still validated during parsing: syntactic
/// errors in the input still cause parsing to fail even when this type is used,
/// [as required by RFC 9651](https://httpwg.org/specs/rfc9651.html#strict).
///
/// See [the module documentation](crate::visitor#discarding-irrelevant-parts)
/// for example usage.
#[derive(Clone, Copy, Debug, Default)]
pub struct Ignored;

impl<'de> ParameterVisitor<'de> for Ignored {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        _key: &'de KeyRef,
        _value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        Ok(())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

impl<'de> ItemVisitor<'de> for Ignored {
    type Out = ();
    type Error = Infallible;

    fn bare_item(
        self,
        _bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        Ok(Ignored)
    }
}

impl<'de> EntryVisitor<'de> for Ignored {
    type Error = Infallible;

    fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }

    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

impl<'de> InnerListVisitor<'de> for Ignored {
    type Error = Infallible;

    fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }

    fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

impl<'de> DictionaryVisitor<'de> for Ignored {
    type Out = ();
    type Error = Infallible;

    fn entry(&mut self, _key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

impl<'de> ListVisitor<'de> for Ignored {
    type Out = ();
    type Error = Infallible;

    fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

fn map_visitor<V, T, E>(
    visitor: Option<V>,
    f: impl FnOnce(V) -> Result<T, E>,
) -> Result<Option<T>, E> {
    match visitor {
        None => Ok(None),
        Some(visitor) => f(visitor).map(Some),
    }
}

impl<'de, V: ParameterVisitor<'de>> ParameterVisitor<'de> for Option<V> {
    type Out = Option<V::Out>;
    type Error = V::Error;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        match *self {
            None => Ok(()),
            Some(ref mut visitor) => visitor.parameter(key, value),
        }
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        map_visitor(self, V::finish)
    }
}

impl<'de, V: ItemVisitor<'de>> ItemVisitor<'de> for Option<V> {
    type Out = Option<V::Out>;
    type Error = V::Error;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        map_visitor(self, |visitor| visitor.bare_item(bare_item))
    }
}

impl<'de, V: EntryVisitor<'de>> EntryVisitor<'de> for Option<V> {
    type Error = V::Error;

    fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        map_visitor(self, V::item)
    }

    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        map_visitor(self, V::inner_list)
    }
}

impl<'de, V: InnerListVisitor<'de>> InnerListVisitor<'de> for Option<V> {
    type Error = V::Error;

    fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        map_visitor(self.as_mut(), V::item)
    }

    fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        map_visitor(self, V::finish)
    }
}

/// A visitor that cannot be instantiated, but can be used as a type in
/// situations guaranteed to return an error `Result`, analogous to
/// [`std::convert::Infallible`].
///
/// When [`!`] is stabilized, this type will be replaced with an alias for it.
#[derive(Clone, Copy, Debug)]
pub enum Never {}

impl<'de> ParameterVisitor<'de> for Never {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        _key: &'de KeyRef,
        _value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        match *self {}
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        match self {}
    }
}

impl<'de> ItemVisitor<'de> for Never {
    type Out = ();
    type Error = Infallible;

    fn bare_item(
        self,
        _bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        Ok(self)
    }
}

impl<'de> EntryVisitor<'de> for Never {
    type Error = Infallible;

    fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(self)
    }

    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        Ok(self)
    }
}

impl<'de> InnerListVisitor<'de> for Never {
    type Error = Infallible;

    fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(*self)
    }

    fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        Ok(self)
    }
}

/// Returns a `ParameterVisitor` that delegates to another visitor but invokes
/// a function to return its value.
///
/// The returned visitor behaves as follows:
///
/// - [`ParameterVisitor::parameter`] forwards directly to `visitor.parameter`
/// - [`ParameterVisitor::finish`] returns `finish(visitor.finish()?)`
///
/// This can be used to propagate a value of type `T` produced within an
/// [`ItemVisitor::bare_item`] call to a parameter visitor `V` that will return
/// it, and even delay an expensive operation producing `T` until the parameters
/// have been parsed successfully.
///
/// See [the module documentation](crate::visitor#returning-a-value-from-itemvisitor)
/// for an example.
pub fn parameter_visitor_with<'de, V, T>(
    visitor: V,
    finish: impl FnOnce(V::Out) -> Result<T, V::Error>,
) -> impl ParameterVisitor<'de, Out = T, Error = V::Error>
where
    V: ParameterVisitor<'de>,
{
    ParameterVisitorWith { visitor, finish }
}

struct ParameterVisitorWith<V, F> {
    visitor: V,
    finish: F,
}

impl<'de, V, F, T> ParameterVisitor<'de> for ParameterVisitorWith<V, F>
where
    V: ParameterVisitor<'de>,
    F: FnOnce(V::Out) -> Result<T, V::Error>,
{
    type Out = T;
    type Error = V::Error;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        self.visitor.parameter(key, value)
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        (self.finish)(self.visitor.finish()?)
    }
}

/// A type that can be produced from an [`ItemVisitor`].
///
/// Use this with [`crate::Parser::parse_item`].
pub trait MakeItemVisitor<'de> {
    /// Returns an item visitor that produces `Self` on success.
    fn make_item_visitor() -> impl ItemVisitor<'de, Out = Self>;
}

/// A type that can be produced from a [`ListVisitor`].
///
/// Use this with [`crate::Parser::parse_list`].
pub trait MakeListVisitor<'de> {
    /// Returns a list visitor that produces `Self` on success.
    fn make_list_visitor() -> impl ListVisitor<'de, Out = Self>;
}

/// A type that can be produced from a [`DictionaryVisitor`].
///
/// Use this with [`crate::Parser::parse_dictionary`].
pub trait MakeDictionaryVisitor<'de> {
    /// Returns a dictionary visitor that produces `Self` on success.
    fn make_dictionary_visitor() -> impl DictionaryVisitor<'de, Out = Self>;
}

impl<'de, V> MakeItemVisitor<'de> for V
where
    V: ItemVisitor<'de, Out = Self> + Default,
{
    fn make_item_visitor() -> impl ItemVisitor<'de, Out = Self> {
        V::default()
    }
}

impl<'de, V> MakeListVisitor<'de> for V
where
    V: ListVisitor<'de, Out = Self> + Default,
{
    fn make_list_visitor() -> impl ListVisitor<'de, Out = Self> {
        V::default()
    }
}

impl<'de, V> MakeDictionaryVisitor<'de> for V
where
    V: DictionaryVisitor<'de, Out = Self> + Default,
{
    fn make_dictionary_visitor() -> impl DictionaryVisitor<'de, Out = Self> {
        V::default()
    }
}

#[allow(clippy::unnecessary_wraps)]
fn infallible_bare_item_visitor<'de, T>(
    bare_item: BareItemFromInput<'de>,
) -> Result<impl ParameterVisitor<'de, Out = T>, Infallible>
where
    T: From<BareItemFromInput<'de>>,
{
    Ok(parameter_visitor_with(Ignored, |()| Ok(T::from(bare_item))))
}

/// Makes an item visitor expecting any bare item and ignoring parameters.
impl<'de> MakeItemVisitor<'de> for BareItemFromInput<'de> {
    fn make_item_visitor() -> impl ItemVisitor<'de, Out = Self> {
        infallible_bare_item_visitor
    }
}

/// Makes an item visitor expecting any bare item and ignoring parameters.
impl<'de> MakeItemVisitor<'de> for super::BareItem {
    fn make_item_visitor() -> impl ItemVisitor<'de, Out = Self> {
        infallible_bare_item_visitor
    }
}

#[derive(Debug)]
enum BareItemType {
    Decimal,
    Integer,
    String,
    ByteSequence,
    Boolean,
    Token,
    Date,
    DisplayString,
}

impl BareItemFromInput<'_> {
    fn ty(&self) -> BareItemType {
        match *self {
            Self::Decimal(_) => BareItemType::Decimal,
            Self::Integer(_) => BareItemType::Integer,
            Self::String(_) => BareItemType::String,
            Self::ByteSequence(_) => BareItemType::ByteSequence,
            Self::Boolean(_) => BareItemType::Boolean,
            Self::Token(_) => BareItemType::Token,
            Self::Date(_) => BareItemType::Date,
            Self::DisplayString(_) => BareItemType::DisplayString,
        }
    }
}

impl std::fmt::Display for BareItemType {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str(match *self {
            Self::Decimal => "decimal",
            Self::Integer => "integer",
            Self::String => "string",
            Self::ByteSequence => "byte sequence",
            Self::Boolean => "boolean",
            Self::Token => "token",
            Self::Date => "date",
            Self::DisplayString => "display string",
        })
    }
}

#[derive(Debug)]
struct UnexpectedBareItemType {
    expected: BareItemType,
    got: BareItemType,
}

impl std::fmt::Display for UnexpectedBareItemType {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "unexpected bare item type: expected {}, got {}",
            self.expected, self.got
        )
    }
}

impl Error for UnexpectedBareItemType {}

macro_rules! impl_make_item_visitor_ignoring_params {
    ($($var: ident($t: ty) $doc: literal,)+) => {
        $(
            /// Makes an item visitor expecting
            #[doc = $doc]
            /// bare item and ignoring parameters.
            impl<'de> MakeItemVisitor<'de> for $t {
                fn make_item_visitor() -> impl ItemVisitor<'de, Out = Self> {
                    |v| {
                        if let BareItemFromInput::$var(v) = v {
                            Ok(parameter_visitor_with(Ignored, move |()| Ok(v)))
                        } else {
                            Err(UnexpectedBareItemType {
                                expected: BareItemType::$var,
                                got: v.ty(),
                            })
                        }
                    }
                }
            }
        )+
    };
}

impl_make_item_visitor_ignoring_params! {
    Decimal(super::Decimal) "a decimal",
    Integer(super::Integer) "an integer",
    String(std::borrow::Cow<'de, super::StringRef>) "a string",
    ByteSequence(Vec<u8>) "a byte sequence",
    Boolean(bool) "a boolean",
    Token(&'de super::TokenRef) "a token",
    Date(super::Date) "a date",
    // deliberately omitted: DisplayString(Cow<'de, str>) "display string",
}
