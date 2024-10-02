use crate::syntax::symbol::Segment;
use crate::syntax::{Lifetimes, NamedType, Pair, Symbol};
use proc_macro2::{Ident, Span};
use std::fmt::{self, Display};
use std::iter;
use syn::parse::{Error, Result};
use syn::punctuated::Punctuated;

#[derive(Clone)]
pub struct ForeignName {
    text: String,
}

impl Pair {
    pub fn to_symbol(&self) -> Symbol {
        let segments = self
            .namespace
            .iter()
            .map(|ident| ident as &dyn Segment)
            .chain(iter::once(&self.cxx as &dyn Segment));
        Symbol::from_idents(segments)
    }
}

impl NamedType {
    pub fn new(rust: Ident) -> Self {
        let generics = Lifetimes {
            lt_token: None,
            lifetimes: Punctuated::new(),
            gt_token: None,
        };
        NamedType { rust, generics }
    }

    pub fn span(&self) -> Span {
        self.rust.span()
    }
}

impl ForeignName {
    pub fn parse(text: &str, span: Span) -> Result<Self> {
        // TODO: support C++ names containing whitespace (`unsigned int`) or
        // non-alphanumeric characters (`operator++`).
        match syn::parse_str::<Ident>(text) {
            Ok(ident) => {
                let text = ident.to_string();
                Ok(ForeignName { text })
            }
            Err(err) => Err(Error::new(span, err)),
        }
    }
}

impl Display for ForeignName {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str(&self.text)
    }
}

impl PartialEq<str> for ForeignName {
    fn eq(&self, rhs: &str) -> bool {
        self.text == rhs
    }
}
