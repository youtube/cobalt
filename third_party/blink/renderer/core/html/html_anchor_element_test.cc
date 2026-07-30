// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_anchor_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

using HTMLAnchorElementTest = PageTestBase;

TEST_F(HTMLAnchorElementTest, UnchangedHrefDoesNotInvalidateStyle) {
  SetBodyInnerHTML("<a href=\"https://www.chromium.org/\">Chromium</a>");
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  auto* anchor =
      To<HTMLAnchorElement>(GetDocument().QuerySelector(AtomicString("a")));
  anchor->setAttribute(html_names::kHrefAttr,
                       AtomicString("https://www.chromium.org/"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

// This tests whether `rel=privacy-policy` is properly counted.
TEST_F(HTMLAnchorElementTest, PrivacyPolicyCounter) {
  // <a rel="privacy-policy"> is not counted when absent
  SetBodyInnerHTML(R"HTML(
    <a rel="not-privacy-policy" href="/">Test</a>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));

  // <a rel="privacy-policy"> is counted when present.
  SetBodyInnerHTML(R"HTML(
    <a rel="privacy-policy" href="/">Test</a>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));
}

// This tests whether `rel=terms-of-service` is properly counted.
TEST_F(HTMLAnchorElementTest, TermsOfServiceCounter) {
  // <a rel="terms-of-service"> is not counted when absent
  SetBodyInnerHTML(R"HTML(
    <a rel="not-terms-of-service" href="/">Test</a>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));

  // <a rel="terms-of-service"> is counted when present.
  SetBodyInnerHTML(R"HTML(
    <a rel="terms-of-service" href="/">Test</a>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));
}

TEST_F(HTMLAnchorElementTest,
       DisconnectedOrHiddenAnchorHasNoScrollTargetObserver) {
  const AtomicString target_id("target");

  // Set up a scroll-target-group in the document so the tree is created.
  SetBodyInnerHTML(R"HTML(
    <style>
      .group {
        scroll-target-group: auto;
      }
    </style>
    <div class="group"></div>
  )HTML");

  auto& registry = GetDocument().EnsureIdTargetObserverRegistry();

  // 1. Disconnected anchor element should not register an observer.
  auto* anchor = MakeGarbageCollected<HTMLAnchorElement>(GetDocument());
  anchor->setAttribute(html_names::kHrefAttr, AtomicString("#target"));
  EXPECT_FALSE(registry.HasObservers(target_id));

  // 2. Connected anchor with display: none should not register an observer.
  Element* group = GetDocument().QuerySelector(AtomicString(".group"));
  ASSERT_TRUE(group);
  group->AppendChild(anchor);
  anchor->setAttribute(html_names::kStyleAttr, AtomicString("display: none;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(registry.HasObservers(target_id));

  // 3. Connected anchor with display: block should register an observer.
  anchor->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(registry.HasObservers(target_id));

  // 4. Detaching or setting back to display: none should unregister the
  // observer.
  anchor->setAttribute(html_names::kStyleAttr, AtomicString("display: none;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(registry.HasObservers(target_id));

  // Clean up by removing from group.
  anchor->remove();
}

}  // namespace
}  // namespace blink
