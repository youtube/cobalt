// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"

namespace blink {

// To get pretty messages out of the CHECK_OP-type macros, we must teach it
// how to print its operand.
std::ostream& operator<<(std::ostream& stream, const SanitizerNameSet& names) {
  stream << "{";
  const char* separator = "";
  for (const auto& name : names) {
    stream << separator;
    stream << name;
    separator = ", ";
  }
  stream << "}";
  return stream;
}

// Below, we'll do an awful lot of checks on HashSet<QualifiedName>.
// These macros are trivial wrappers around .Contains and .empty, but with
// nicer error messages since they'll also print out the set contents.
#define CHECK_CONTAINS(set, name) \
  CHECK((set).Contains(name)) << #set "=" << set << " should contain " << name
#define CHECK_NOT_CONTAINS(set, name) \
  CHECK(!(set).Contains(name)) << #set "=" << set << " contains " << name
#define CHECK_EMPTY(set) \
  CHECK((set).empty()) << #set "=" << set << " should be empty."
#define CHECK_NOT_EMPTY(set) CHECK(!(set).empty()) << #set " is empty."

TEST(SanitizerBuiltinsTest, DefaultUnsafeIsReallyEmpty) {
  // Sanity check: The default "unsafe" config needs to allow anything. It's
  // equivalent to an empty dictionary.
  const Sanitizer* sanitizer = SanitizerBuiltins::GetDefaultUnsafe();
  CHECK_EMPTY(sanitizer->allow_elements());
  CHECK_EMPTY(sanitizer->remove_elements());
  CHECK_EMPTY(sanitizer->replace_elements());
  CHECK_EMPTY(sanitizer->allow_attrs());
  CHECK_EMPTY(sanitizer->remove_attrs());
  CHECK(sanitizer->allow_attrs_per_element().empty());
  CHECK(sanitizer->remove_attrs_per_element().empty());
  CHECK(sanitizer->allow_data_attrs());
  CHECK(sanitizer->allow_comments());
}

TEST(SanitizerBuiltinsTest, DefaultSafeIsAllowList) {
  // The default safe config should be written as an allow list.
  const Sanitizer* sanitizer = SanitizerBuiltins::GetDefaultSafe();
  CHECK_NOT_EMPTY(sanitizer->allow_elements());
  CHECK_NOT_EMPTY(sanitizer->allow_attrs());
  CHECK_EMPTY(sanitizer->remove_elements());
  CHECK_EMPTY(sanitizer->replace_elements());
  CHECK_EMPTY(sanitizer->remove_attrs());
  CHECK(sanitizer->allow_attrs_per_element().empty());
  CHECK(sanitizer->remove_attrs_per_element().empty());
}

TEST(SanitizerBuiltinsTest, BaselineIsRemoveList) {
  // The baseline should be written as a remove lists.
  const Sanitizer* sanitizer = SanitizerBuiltins::GetBaseline();
  CHECK_NOT_EMPTY(sanitizer->remove_elements());
  CHECK_NOT_EMPTY(sanitizer->remove_attrs());
  CHECK_EMPTY(sanitizer->allow_elements());
  CHECK_EMPTY(sanitizer->replace_elements());
  CHECK_EMPTY(sanitizer->allow_attrs());
  CHECK(sanitizer->allow_attrs_per_element().empty());
  CHECK(sanitizer->remove_attrs_per_element().empty());
}

TEST(SanitizerBuiltinsTest, DefaultSafeContainsOnlyKnownNames) {
  const Sanitizer* safe = SanitizerBuiltins::GetDefaultSafe();
  const Sanitizer* all =
      sanitizer_generated_builtins::BuildAllKnownConfig_ForTesting();

  SanitizerNameSet elements(safe->allow_elements().begin(),
                            safe->allow_elements().end());
  elements.RemoveAll(all->allow_elements());
  CHECK_EMPTY(elements);

  SanitizerNameSet attrs(safe->allow_attrs().begin(),
                         safe->allow_attrs().end());
  attrs.RemoveAll(all->allow_attrs());
  CHECK_EMPTY(attrs);
}

void CheckForScriptyStuff(const Sanitizer* sanitizer) {
  // Spot checks of whether sanitizer contains "obviously" script-y stuff.
  CHECK_NOT_CONTAINS(sanitizer->allow_elements(), html_names::kScriptTag);
  CHECK_NOT_CONTAINS(sanitizer->allow_elements(), svg_names::kScriptTag);
  for (const QualifiedName& name : sanitizer->allow_attrs()) {
    CHECK(!name.LocalName().StartsWith("on")) << "found on*: " << name;
  }
}

TEST(SanitizerBuiltinsTest, DefaultsContainNoScriptyBullshit) {
  // "Safe" defaults shouldn't contain "obviously" script-y stuff.
  CheckForScriptyStuff(SanitizerBuiltins::GetDefaultSafe());
}

TEST(SanitizerBuiltinsTest, SafeDefaultsShouldNotContainBaselineStuff) {
  const Sanitizer* defaults = SanitizerBuiltins::GetDefaultSafe();
  const Sanitizer* baseline = SanitizerBuiltins::GetBaseline();
  for (const QualifiedName& name : defaults->allow_elements()) {
    CHECK(!baseline->remove_elements().Contains(name));
  }
  for (const QualifiedName& name : defaults->allow_attrs()) {
    CHECK(!baseline->remove_attrs().Contains(name));
  }
}

TEST(SanitizerBuiltinsTest, RemovingBaselineShouldNotContainScriptyStuff) {
  const Sanitizer* all =
      sanitizer_generated_builtins::BuildAllKnownConfig_ForTesting();
  const Sanitizer* baseline = SanitizerBuiltins::GetBaseline();

  // TODO(vogelheim): Once the Sanitizer API is more completely implemented,
  // the logic below should probably be re-implemented with Sanitizer methods
  // like removeUnsafe();
  Sanitizer* all_without_baseline = MakeGarbageCollected<Sanitizer>();

  SanitizerNameSet elements = all->allow_elements();
  elements.RemoveAll(baseline->remove_elements());
  for (const auto& element : elements) {
    all_without_baseline->AllowElement(element);
  }

  SanitizerNameSet attrs = all->allow_attrs();
  attrs.RemoveAll(baseline->remove_attrs());
  for (const auto& attr : attrs) {
    all_without_baseline->AllowAttribute(attr);
  }

  CheckForScriptyStuff(all_without_baseline);
}

}  // namespace blink
