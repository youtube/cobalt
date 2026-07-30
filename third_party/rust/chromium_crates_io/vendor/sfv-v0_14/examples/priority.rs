// This example demonstrates both dictionary- and visitor-based parsing of the
// Priority header according to https://httpwg.org/specs/rfc9218.html.

const KEY_U: &str = "u";
const KEY_I: &str = "i";

const DEFAULT_URGENCY: u8 = 3;
const DEFAULT_INCREMENTAL: bool = false;

#[derive(Debug, PartialEq)]
struct Priority {
    urgency: u8,
    incremental: bool,
}

impl Default for Priority {
    fn default() -> Self {
        Self {
            urgency: DEFAULT_URGENCY,
            incremental: DEFAULT_INCREMENTAL,
        }
    }
}

fn parse_urgency(v: Option<sfv::Integer>) -> u8 {
    v.and_then(|v| u8::try_from(v).ok())
        .filter(|v| *v <= 7)
        .unwrap_or(DEFAULT_URGENCY)
}

fn parse_incremental(v: Option<bool>) -> bool {
    v.unwrap_or(DEFAULT_INCREMENTAL)
}

impl<'de> sfv::visitor::DictionaryVisitor<'de> for Priority {
    type Error = std::convert::Infallible;

    fn entry(
        &mut self,
        key: &'de sfv::KeyRef,
    ) -> Result<impl sfv::visitor::EntryVisitor<'de>, Self::Error> {
        Ok(match key.as_str() {
            KEY_U => Some(PriorityParameter::U(&mut self.urgency)),
            KEY_I => Some(PriorityParameter::I(&mut self.incremental)),
            // Per https://httpwg.org/specs/rfc9218.html#parameters unknown
            // dictionary keys are ignored.
            _ => None,
        })
    }
}

enum PriorityParameter<'a> {
    U(&'a mut u8),
    I(&'a mut bool),
}

impl<'de> sfv::visitor::ItemVisitor<'de> for PriorityParameter<'_> {
    type Error = std::convert::Infallible;

    fn bare_item(
        self,
        bare_item: sfv::BareItemFromInput<'de>,
    ) -> Result<impl sfv::visitor::ParameterVisitor<'de>, Self::Error> {
        // Per https://httpwg.org/specs/rfc9218.html#parameters values of
        // unexpected types and out-of-range values are ignored. Since the same
        // dictionary key can appear multiple times in the input, and only the
        // last value may be considered per Structured Field semantics, we
        // overwrite any existing value with the default on error.
        match self {
            Self::U(urgency) => *urgency = parse_urgency(bare_item.as_integer()),
            Self::I(incremental) => *incremental = parse_incremental(bare_item.as_boolean()),
        }
        // Neither https://httpwg.org/specs/rfc9218.html#urgency nor
        // https://httpwg.org/specs/rfc9218.html#incremental defines parameters.
        Ok(sfv::visitor::Ignored)
    }
}

impl<'de> sfv::visitor::EntryVisitor<'de> for PriorityParameter<'_> {
    fn inner_list(self) -> Result<impl sfv::visitor::InnerListVisitor<'de>, Self::Error> {
        // Per https://httpwg.org/specs/rfc9218.html#parameters values of
        // unexpected types are ignored. Since the same dictionary key can
        // appear multiple times in the input, and only the last value may be
        // considered per Structured Field semantics, we overwrite any existing
        // value with the default.
        match self {
            Self::U(urgency) => *urgency = DEFAULT_URGENCY,
            Self::I(incremental) => *incremental = DEFAULT_INCREMENTAL,
        }
        // Per https://httpwg.org/specs/rfc9218.html#parameters values of
        // unexpected types are ignored.
        Ok(sfv::visitor::Ignored)
    }
}

impl From<&sfv::Dictionary> for Priority {
    fn from(dict: &sfv::Dictionary) -> Self {
        Self {
            urgency: parse_urgency(match dict.get(KEY_U) {
                Some(sfv::ListEntry::Item(sfv::Item { bare_item, .. })) => bare_item.as_integer(),
                _ => None,
            }),
            incremental: parse_incremental(match dict.get(KEY_I) {
                Some(sfv::ListEntry::Item(sfv::Item { bare_item, .. })) => bare_item.as_boolean(),
                _ => None,
            }),
        }
    }
}

#[allow(clippy::too_many_lines)]
fn main() -> Result<(), sfv::Error> {
    let examples = [
        // From https://httpwg.org/specs/rfc9218.html#parameters: "When
        // receiving an HTTP request that does not carry these priority
        // parameters, a server SHOULD act as if their default values were
        // specified."
        (
            "",
            Priority {
                urgency: 3,
                incremental: false,
            },
        ),
        // https://httpwg.org/specs/rfc9218.html#incremental
        (
            "u=5, i",
            Priority {
                urgency: 5,
                incremental: true,
            },
        ),
        // Unknown key
        (
            "x;a, u=5, i",
            Priority {
                urgency: 5,
                incremental: true,
            },
        ),
        // Unexpected type for urgency
        (
            "u=(), i",
            Priority {
                urgency: 3,
                incremental: true,
            },
        ),
        // Unexpected type for urgency
        (
            "u=6.5, i",
            Priority {
                urgency: 3,
                incremental: true,
            },
        ),
        // Urgency below minimum
        (
            "u=-1, i",
            Priority {
                urgency: 3,
                incremental: true,
            },
        ),
        // Urgency above maximum
        (
            "u=8, i",
            Priority {
                urgency: 3,
                incremental: true,
            },
        ),
        // Unexpected type for incremental
        (
            "i=(), u=5",
            Priority {
                urgency: 5,
                incremental: false,
            },
        ),
        // Unexpected type for incremental
        (
            "i=1, u=5",
            Priority {
                urgency: 5,
                incremental: false,
            },
        ),
        // Parameters are ignored
        (
            "u=5;x, i;y",
            Priority {
                urgency: 5,
                incremental: true,
            },
        ),
        // When duplicate keys are encountered, only last's value is used
        (
            "u=6, i, u=5, i=?0",
            Priority {
                urgency: 5,
                incremental: false,
            },
        ),
        // When duplicate keys are encountered, only last's value is used
        (
            "u=6, i, u=(), i=()",
            Priority {
                urgency: 3,
                incremental: false,
            },
        ),
    ];

    for (input, expected) in examples {
        assert_eq!(
            Priority::from(
                &sfv::Parser::new(input)
                    .with_version(sfv::Version::Rfc8941)
                    .parse()?
            ),
            expected,
            "{input}"
        );

        assert_eq!(
            {
                let mut priority = Priority::default();
                sfv::Parser::new(input)
                    .with_version(sfv::Version::Rfc8941)
                    .parse_dictionary_with_visitor(&mut priority)?;
                priority
            },
            expected,
            "{input}"
        );
    }

    Ok(())
}
