use std::convert::Infallible;

use indexmap::IndexMap;

use crate::{
    private::Sealed,
    visitor::{
        DictionaryVisitor, EntryVisitor, InnerListVisitor, ItemVisitor, ListVisitor,
        ParameterVisitor,
    },
    BareItem, BareItemFromInput, Error, Key, KeyRef, Parser,
};

/// An [item]-type structured field value.
///
/// Can be used as a member of `List` or `Dictionary`.
///
/// [item]: <https://httpwg.org/specs/rfc9651.html#item>
// sf-item   = bare-item parameters
// bare-item = sf-integer / sf-decimal / sf-string / sf-token
//             / sf-binary / sf-boolean
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub struct Item {
    /// The item's value.
    pub bare_item: BareItem,
    /// The item's parameters, which can be empty.
    pub params: Parameters,
}

impl Item {
    /// Returns a new `Item` with empty `Parameters`.
    #[must_use]
    pub fn new(bare_item: impl Into<BareItem>) -> Self {
        Self {
            bare_item: bare_item.into(),
            params: Parameters::new(),
        }
    }

    /// Returns a new `Item` with the given `Parameters`.
    #[must_use]
    pub fn with_params(bare_item: impl Into<BareItem>, params: Parameters) -> Self {
        Self {
            bare_item: bare_item.into(),
            params,
        }
    }
}

/// A [dictionary]-type structured field value.
///
/// [dictionary]: <https://httpwg.org/specs/rfc9651.html#dictionary>
// sf-dictionary  = dict-member *( OWS "," OWS dict-member )
// dict-member    = member-name [ "=" member-value ]
// member-name    = key
// member-value   = sf-item / inner-list
pub type Dictionary = IndexMap<Key, ListEntry>;

/// A [list]-type structured field value.
///
/// [list]: <https://httpwg.org/specs/rfc9651.html#list>
// sf-list       = list-member *( OWS "," OWS list-member )
// list-member   = sf-item / inner-list
pub type List = Vec<ListEntry>;

/// [Parameters] of an [`Item`] or [`InnerList`].
///
/// [parameters]: <https://httpwg.org/specs/rfc9651.html#param>
// parameters    = *( ";" *SP parameter )
// parameter     = param-name [ "=" param-value ]
// param-name    = key
// key           = ( lcalpha / "*" )
//                 *( lcalpha / DIGIT / "_" / "-" / "." / "*" )
// lcalpha       = %x61-7A ; a-z
// param-value   = bare-item
pub type Parameters = IndexMap<Key, BareItem>;

/// A member of a [`List`] or [`Dictionary`].
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub enum ListEntry {
    /// An item.
    Item(Item),
    /// An inner list.
    InnerList(InnerList),
}

impl From<Item> for ListEntry {
    fn from(item: Item) -> Self {
        ListEntry::Item(item)
    }
}

impl From<InnerList> for ListEntry {
    fn from(inner_list: InnerList) -> Self {
        ListEntry::InnerList(inner_list)
    }
}

/// An [array] of [`Item`]s with associated [`Parameters`].
///
/// [array]: <https://httpwg.org/specs/rfc9651.html#inner-list>
// inner-list    = "(" *SP [ sf-item *( 1*SP sf-item ) *SP ] ")"
//                 parameters
#[derive(Debug, Default, PartialEq, Clone)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub struct InnerList {
    /// The inner list's items, which can be empty.
    pub items: Vec<Item>,
    /// The inner list's parameters, which can be empty.
    pub params: Parameters,
}

impl InnerList {
    /// Returns a new `InnerList` with empty `Parameters`.
    #[must_use]
    pub fn new(items: Vec<Item>) -> Self {
        Self {
            items,
            params: Parameters::new(),
        }
    }

    /// Returns a new `InnerList` with the given `Parameters`.
    #[must_use]
    pub fn with_params(items: Vec<Item>, params: Parameters) -> Self {
        Self { items, params }
    }
}

impl<'de> ParameterVisitor<'de> for &mut Parameters {
    type Error = Infallible;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        self.insert(key.to_owned(), value.into());
        Ok(())
    }
}

impl<'de> ItemVisitor<'de> for &mut Item {
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        self.bare_item = bare_item.into();
        Ok(&mut self.params)
    }
}

impl<'de> ItemVisitor<'de> for &mut InnerList {
    type Error = Infallible;
    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        self.items.push(Item::new(bare_item));
        match self.items.last_mut() {
            Some(item) => Ok(&mut item.params),
            None => unreachable!(),
        }
    }
}

impl<'de> InnerListVisitor<'de> for &mut InnerList {
    type Error = Infallible;

    fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(&mut **self)
    }

    fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        Ok(&mut self.params)
    }
}

impl<'de> DictionaryVisitor<'de> for Dictionary {
    type Error = Infallible;

    fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(self.entry(key.to_owned()))
    }
}

type Entry<'a> = indexmap::map::Entry<'a, Key, ListEntry>;

impl<'de> ItemVisitor<'de> for Entry<'_> {
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        match self.insert_entry(Item::new(bare_item).into()).into_mut() {
            ListEntry::Item(item) => Ok(&mut item.params),
            ListEntry::InnerList(_) => unreachable!(),
        }
    }
}

impl<'de> EntryVisitor<'de> for Entry<'_> {
    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        match self.insert_entry(InnerList::default().into()).into_mut() {
            ListEntry::InnerList(inner_list) => Ok(inner_list),
            ListEntry::Item(_) => unreachable!(),
        }
    }
}

impl<'de> ItemVisitor<'de> for &mut List {
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de>, Self::Error> {
        self.push(Item::new(bare_item).into());
        match self.last_mut() {
            Some(ListEntry::Item(item)) => Ok(&mut item.params),
            _ => unreachable!(),
        }
    }
}

impl<'de> EntryVisitor<'de> for &mut List {
    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        self.push(InnerList::default().into());
        match self.last_mut() {
            Some(ListEntry::InnerList(inner_list)) => Ok(inner_list),
            _ => unreachable!(),
        }
    }
}

impl<'de> ListVisitor<'de> for List {
    type Error = Infallible;

    fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error> {
        Ok(self)
    }
}

/// A structured-field type, supporting parsing and serialization.
pub trait FieldType: Sealed {
    /// The result of serializing the value into a string.
    ///
    /// [`Item`] serialization is infallible; [`List`] and [`Dictionary`]
    /// serialization is not.
    type SerializeResult: Into<Option<String>>;

    /// Serializes a structured field value into a string.
    ///
    /// Note: The serialization conforms to [RFC 9651], meaning that
    /// [`Dates`][crate::Date] and [`Display Strings`][RefBareItem::DisplayString],
    /// which cause parsing errors under [RFC 8941], will be serialized
    /// unconditionally. The consumer of this API is responsible for determining
    /// whether it is valid to serialize these bare items for any specific field.
    ///
    /// [RFC 8941]: <https://httpwg.org/specs/rfc8941.html>
    /// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html>
    ///
    /// Use [`crate::ItemSerializer`], [`crate::ListSerializer`], or
    /// [`crate::DictSerializer`] to serialize components incrementally without
    /// having to create an [`Item`], [`List`], or [`Dictionary`].
    #[must_use]
    fn serialize(&self) -> Self::SerializeResult;

    /// Parses a structured-field value from the given parser.
    ///
    /// # Errors
    /// When the parsing process is unsuccessful.
    fn parse(parser: Parser<'_>) -> Result<Self, Error>
    where
        Self: Sized;
}

impl Sealed for Item {}

impl FieldType for Item {
    type SerializeResult = String;

    fn serialize(&self) -> String {
        crate::ItemSerializer::new()
            .bare_item(&self.bare_item)
            .parameters(&self.params)
            .finish()
    }

    fn parse(parser: Parser<'_>) -> Result<Self, Error> {
        let mut item = Self::new(false);
        parser.parse_item_with_visitor(&mut item)?;
        Ok(item)
    }
}

impl Sealed for List {}

impl FieldType for List {
    type SerializeResult = Option<String>;

    fn serialize(&self) -> Option<String> {
        let mut ser = crate::ListSerializer::new();
        ser.members(self);
        ser.finish()
    }

    fn parse(parser: Parser<'_>) -> Result<Self, Error> {
        let mut list = Self::new();
        parser.parse_list_with_visitor(&mut list)?;
        Ok(list)
    }
}

impl Sealed for Dictionary {}

impl FieldType for Dictionary {
    type SerializeResult = Option<String>;

    fn serialize(&self) -> Option<String> {
        let mut ser = crate::DictSerializer::new();
        ser.members(self);
        ser.finish()
    }

    fn parse(parser: Parser<'_>) -> Result<Self, Error> {
        let mut dict = Self::new();
        parser.parse_dictionary_with_visitor(&mut dict)?;
        Ok(dict)
    }
}
