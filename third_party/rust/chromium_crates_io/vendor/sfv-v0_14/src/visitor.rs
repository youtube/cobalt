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
# use sfv::{BareItemFromInput, TokenRef};
# fn main() -> Result<(), sfv::Error> {
struct Visitor<'de> {
    token: Option<&'de TokenRef>,
}

impl<'de> ItemVisitor<'de> for &mut Visitor<'de> {
  type Error = std::convert::Infallible;

  fn bare_item(self, bare_item: BareItemFromInput<'de>) -> Result<impl ParameterVisitor<'de>, Self::Error> {
      self.token =
          if let BareItemFromInput::Token(token) = bare_item {
              Some(token)
          } else {
              None
          };

      Ok(Ignored)
  }
}

let input = "abc";

let mut visitor = Visitor { token: None };

sfv::Parser::new(input).parse_item_with_visitor(&mut visitor)?;

// Use `visitor.token` to do something expensive or with side effects now that
// we know the entire input is valid.

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
}

#[derive(Debug)]
struct NotAnInteger;

impl std::fmt::Display for NotAnInteger {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str("must be an integer")
    }
}

impl std::error::Error for NotAnInteger {}

impl<'de> ItemVisitor<'de> for CoordVisitor<'_> {
    type Error = NotAnInteger;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
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
    }
}

impl<'de> EntryVisitor<'de> for CoordVisitor<'_> {
    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        // Use `Never` to enforce at the type level that this method will only
        // return `Err`, as our coordinate must be a single integer, not an
        // inner list.
        Err::<Never, _>(NotAnInteger)
    }
}

# fn main() -> Result<(), sfv::Error> {
let mut point = Point::default();
Parser::new("x=10, z=abc, y=3").parse_dictionary_with_visitor(&mut point)?;
assert_eq!(point, Point { x: 10, y: 3 });
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
    fn finish(self) -> Result<(), Self::Error>
    where
        Self: Sized,
    {
        Ok(())
    }
}

/// A visitor whose methods are called during item parsing.
///
/// The lifetime `'de` is the lifetime of the input.
///
/// Use this trait with
/// [`Parser::parse_item_with_visitor`][crate::Parser::parse_item_with_visitor].
pub trait ItemVisitor<'de> {
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
    ) -> Result<impl ParameterVisitor<'de>, Self::Error>;
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
pub trait EntryVisitor<'de>: ItemVisitor<'de> {
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
}

/// A visitor whose methods are called during list parsing.
///
/// The lifetime `'de` is the lifetime of the input.
///
/// Use this trait with
/// [`Parser::parse_list_with_visitor`][crate::Parser::parse_list_with_visitor].
pub trait ListVisitor<'de> {
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
    type Error = Infallible;

    fn parameter(
        &mut self,
        _key: &'de KeyRef,
        _value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        Ok(())
    }
}

impl<'de> ItemVisitor<'de> for Ignored {
    type Error = Infallible;

    fn bare_item(
        self,
        _bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

impl<'de> EntryVisitor<'de> for Ignored {
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
    type Error = Infallible;

    fn entry(&mut self, _key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

impl<'de> ListVisitor<'de> for Ignored {
    type Error = Infallible;

    fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

impl<'de, V: ParameterVisitor<'de>> ParameterVisitor<'de> for Option<V> {
    type Error = V::Error;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        match self {
            None => Ok(()),
            Some(visitor) => visitor.parameter(key, value),
        }
    }
}

impl<'de, V: ItemVisitor<'de>> ItemVisitor<'de> for Option<V> {
    type Error = V::Error;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        match self {
            None => Ok(None),
            Some(visitor) => visitor.bare_item(bare_item).map(Some),
        }
    }
}

impl<'de, V: EntryVisitor<'de>> EntryVisitor<'de> for Option<V> {
    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        match self {
            None => Ok(None),
            Some(visitor) => visitor.inner_list().map(Some),
        }
    }
}

impl<'de, V: InnerListVisitor<'de>> InnerListVisitor<'de> for Option<V> {
    type Error = V::Error;

    fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        match self {
            None => Ok(None),
            Some(visitor) => visitor.item().map(Some),
        }
    }

    fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        match self {
            None => Ok(None),
            Some(visitor) => visitor.finish().map(Some),
        }
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
    type Error = Infallible;

    fn parameter(
        &mut self,
        _key: &'de KeyRef,
        _value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        match *self {}
    }
}

impl<'de> ItemVisitor<'de> for Never {
    type Error = Infallible;

    fn bare_item(
        self,
        _bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        Ok(self)
    }
}

impl<'de> EntryVisitor<'de> for Never {
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

impl<'de> DictionaryVisitor<'de> for Never {
    type Error = Infallible;

    fn entry(&mut self, _key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(*self)
    }
}

impl<'de> ListVisitor<'de> for Never {
    type Error = Infallible;

    fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(*self)
    }
}
