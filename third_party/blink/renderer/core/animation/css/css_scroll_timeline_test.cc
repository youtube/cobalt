// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSScrollTimelineTest : public PageTestBase,
                              private ScopedScrollTimelineForTest {
 public:
  CSSScrollTimelineTest() : ScopedScrollTimelineForTest(true) {}

  DocumentAnimations& GetDocumentAnimations() const {
    return GetDocument().GetDocumentAnimations();
  }

  const HeapHashSet<WeakMember<ScrollSnapshotClient>>&
  GetUnvalidatedTimelines() {
    return GetFrame().GetUnvalidatedScrollSnapshotClientsForTesting();
  }
};

TEST_F(CSSScrollTimelineTest, SharedTimelines) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim1 { to { top: 200px; } }
      @keyframes anim2 { to { left: 200px; } }
      @keyframes anim3 { to { right: 200px; } }
      .scroller {
        height: 100px;
        overflow: scroll;
      }
      .scroller > div {
        height: 200px;
      }
      #scroller1 {
        scroll-timeline: timeline1;
      }
      #scroller2 {
        scroll-timeline: timeline2;
      }
    </style>
    <div id=scroller1 class=scroller><div></div></div>
    <div id=scroller2 class=scroller><div></div></div>
    <main id=main></main>
  )HTML");
  // #scroller[1,2] etc is created in a separate lifecycle phase to ensure that
  // we get a layout box for #scroller[1,2] before the animations are started.

  Element* main = GetDocument().getElementById("main");
  ASSERT_TRUE(main);
  main->setInnerHTML(R"HTML(
    <style>
      #element1, #element2 {
        animation-name: anim1, anim2, anim3;
        animation-duration: 10s;
        animation-timeline: timeline1, timeline1, timeline2;
      }
    </style>
    <div id=element1></div>
    <div id=element2></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element1 = GetDocument().getElementById("element1");
  Element* element2 = GetDocument().getElementById("element2");
  ASSERT_TRUE(element1);
  ASSERT_TRUE(element2);
  HeapVector<Member<Animation>> animations1 = element1->getAnimations();
  HeapVector<Member<Animation>> animations2 = element2->getAnimations();
  EXPECT_EQ(3u, animations1.size());
  EXPECT_EQ(3u, animations2.size());

  // The animations associated with anim1 and anim2 should share the same
  // timeline instance, also across elements.
  EXPECT_EQ(animations1[0]->timeline(), animations1[1]->timeline());
  EXPECT_EQ(animations1[1]->timeline(), animations2[0]->timeline());
  EXPECT_EQ(animations2[0]->timeline(), animations2[1]->timeline());

  // The animation associated with anim3 uses a different timeline
  // from anim1/2.
  EXPECT_EQ(animations1[2]->timeline(), animations2[2]->timeline());

  EXPECT_NE(animations2[2]->timeline(), animations1[0]->timeline());
  EXPECT_NE(animations2[2]->timeline(), animations1[1]->timeline());
}

TEST_F(CSSScrollTimelineTest, MultipleLifecyclePasses) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { color: green; }
        to { color: green; }
      }
      #scroller {
        height: 100px;
        overflow: scroll;
        scroll-timeline: timeline;
      }
      #scroller > div {
        height: 200px;
      }
      #element {
        color: red;
        animation: anim 10s;
        animation-timeline: timeline;
      }
    </style>
    <div id=scroller>
      <div id=contents></div>
    </div>
    <div id=element></div>
  )HTML");

  Element* element = GetDocument().getElementById("element");
  ASSERT_TRUE(element);

  // According to the rules of the spec [1], the timeline is now inactive,
  // because #scroller did not have a layout box at the time style recalc
  // for #element happened.
  //
  // However, we do an additional style/layout pass if we detect new
  // CSSScrollTimelines in this situation, hence we ultimately do expect
  // the animation to apply [2].
  //
  // See also DocumentAnimations::ValidateTimelines.
  //
  // [1] https://drafts.csswg.org/scroll-animations-1/#avoiding-cycles
  // [2] https://github.com/w3c/csswg-drafts/issues/5261
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            element->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

namespace {

class AnimationTriggeringDelegate : public ResizeObserver::Delegate {
 public:
  explicit AnimationTriggeringDelegate(Element* scroller_element)
      : scroller_element_(scroller_element) {}

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    scroller_element_->SetInlineStyleProperty(CSSPropertyID::kScrollTimeline,
                                              "timeline");
  }

  void Trace(Visitor* visitor) const override {
    ResizeObserver::Delegate::Trace(visitor);
    visitor->Trace(scroller_element_);
  }

 private:
  Member<Element> scroller_element_;
};

}  // namespace

TEST_F(CSSScrollTimelineTest, ResizeObserverTriggeredTimelines) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { width: 100px; }
        to { width: 100px; }
      }
      #scroller {
        height: 100px;
        overflow: scroll;
      }
      #scroller > div {
        height: 200px;
      }
      #element {
        width: 1px;
        animation: anim 10s timeline;
      }
    </style>
    <div id=main></div>
  )HTML");

  ASSERT_TRUE(GetUnvalidatedTimelines().empty());

  Element* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  element->setAttribute(blink::html_names::kIdAttr, "element");

  Element* scroller = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  scroller->setAttribute(blink::html_names::kIdAttr, "scroller");
  scroller->AppendChild(MakeGarbageCollected<HTMLDivElement>(GetDocument()));

  Element* main = GetDocument().getElementById("main");
  ASSERT_TRUE(main);
  main->AppendChild(scroller);
  main->AppendChild(element);

  auto* delegate = MakeGarbageCollected<AnimationTriggeringDelegate>(scroller);
  ResizeObserver* observer =
      ResizeObserver::Create(GetDocument().domWindow(), delegate);
  observer->observe(element);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, GetUnvalidatedTimelines().size());
}

namespace {

absl::optional<ScrollTimeline::ScrollAxis> GetTimelineAxis(
    const Animation& animation) {
  if (auto* scroll_timeline = DynamicTo<ScrollTimeline>(animation.timeline())) {
    return scroll_timeline->GetAxis();
  }
  return absl::nullopt;
}

}  // namespace

TEST_F(CSSScrollTimelineTest, ViewTimelineHost) {
  ScopedCSSTreeScopedTimelinesForTest scoped_feature(true);
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      @keyframes anim {
        from { z-index: 100; }
        to { z-index: 100; }
      }
      .target {
        animation: anim 10s linear;
        animation-timeline: timeline;
      }
      .scroller > div {
        view-timeline: timeline horizontal;
      }
    </style>
    <div class=scroller>
      <div>
        <div class=target>
          <template shadowroot=open>
            <style>
              :host {
                view-timeline: timeline vertical;
              }
            </style>
          </template>
        </div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().QuerySelector(".target");
  ASSERT_TRUE(target);
  HeapVector<Member<Animation>> animations = target->getAnimations();
  ASSERT_EQ(1u, animations.size());
  ASSERT_EQ(ScrollTimeline::ScrollAxis::kHorizontal,
            GetTimelineAxis(*animations[0]))
      << "Outer animation can not see view timeline defined by :host";
}

TEST_F(CSSScrollTimelineTest, ViewTimelineSlotted) {
  ScopedCSSTreeScopedTimelinesForTest scoped_feature(true);
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      @keyframes anim {
        from { z-index: 100; }
        to { z-index: 100; }
      }
      .target {
        animation: anim 10s linear;
        animation-timeline: timeline;
      }
      .host {
        view-timeline: timeline horizontal;
      }
    </style>
    <div class=scroller>
      <div class=host>
        <template shadowroot=open>
          <style>
            ::slotted(.target) {
              view-timeline: timeline vertical;
            }
          </style>
          <slot></slot>
        </template>
        <div class=target></div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().QuerySelector(".target");
  ASSERT_TRUE(target);
  HeapVector<Member<Animation>> animations = target->getAnimations();
  ASSERT_EQ(1u, animations.size());
  ASSERT_EQ(ScrollTimeline::ScrollAxis::kHorizontal,
            GetTimelineAxis(*animations[0]))
      << "Outer animation can not see view timeline defined by ::slotted";
}

TEST_F(CSSScrollTimelineTest, ViewTimelinePart) {
  ScopedCSSTreeScopedTimelinesForTest scoped_feature(true);
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      .host {
        view-timeline: timeline vertical;
      }
      .host::part(foo) {
        view-timeline: timeline horizontal;
      }
    </style>
    <div class=host>
      <template shadowroot=open>
        <style>
            /* Not placing 'anim2' at document scope, due to
               https://crbug.com/1334534 */
            @keyframes anim2 {
              from { z-index: 100; }
              to { z-index: 100; }
            }
          .target {
            animation: anim2 10s linear;
            animation-timeline: timeline;
          }
        </style>
        <div part=foo>
          <div class=target></div>
        </div>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* host = GetDocument().QuerySelector(".host");
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->GetShadowRoot());
  Element* target = host->GetShadowRoot()->QuerySelector(".target");
  ASSERT_TRUE(target);
  HeapVector<Member<Animation>> animations = target->getAnimations();
  ASSERT_EQ(1u, animations.size());
  ASSERT_EQ(ScrollTimeline::ScrollAxis::kHorizontal,
            GetTimelineAxis(*animations[0]))
      << "Inner animation can see view timeline defined by ::part";
}

TEST_F(CSSScrollTimelineTest, ScrollTimelineHost) {
  ScopedCSSTreeScopedTimelinesForTest scoped_feature(true);
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      @keyframes anim {
        from { z-index: 100; }
        to { z-index: 100; }
      }
      .target {
        animation: anim 10s linear;
        animation-timeline: timeline;
      }
      main > .scroller {
        scroll-timeline: timeline horizontal;
      }
    </style>
    <main>
      <div class=scroller>
        <div class=scroller>
          <template shadowroot=open>
            <style>
              :host {
                scroll-timeline: timeline vertical;
              }
            </style>
            <slot></slot>
          </template>
          <div class=target></div>
        </div>
      </div>
    </main>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().QuerySelector(".target");
  ASSERT_TRUE(target);
  HeapVector<Member<Animation>> animations = target->getAnimations();
  ASSERT_EQ(1u, animations.size());
  ASSERT_EQ(ScrollTimeline::ScrollAxis::kHorizontal,
            GetTimelineAxis(*animations[0]))
      << "Outer animation can not see scroll timeline defined by :host";
}

TEST_F(CSSScrollTimelineTest, ScrollTimelineSlotted) {
  ScopedCSSTreeScopedTimelinesForTest scoped_feature(true);
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      @keyframes anim {
        from { z-index: 100; }
        to { z-index: 100; }
      }
      .target {
        animation: anim 10s linear;
        animation-timeline: timeline;
      }
      .host {
        scroll-timeline: timeline horizontal;
      }
    </style>
    <div class=host>
      <template shadowroot=open>
        <style>
          ::slotted(.scroller) {
            scroll-timeline: timeline vertical;
          }
        </style>
        <slot></slot>
      </template>
      <div class=scroller>
        <div class=target></div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().QuerySelector(".target");
  ASSERT_TRUE(target);
  HeapVector<Member<Animation>> animations = target->getAnimations();
  ASSERT_EQ(1u, animations.size());
  ASSERT_EQ(ScrollTimeline::ScrollAxis::kHorizontal,
            GetTimelineAxis(*animations[0]))
      << "Outer animation can not see scroll timeline defined by ::slotted";
}

TEST_F(CSSScrollTimelineTest, ScrollTimelinePart) {
  ScopedCSSTreeScopedTimelinesForTest scoped_feature(true);
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      .host {
        scroll-timeline: timeline vertical;
      }
      .host::part(foo) {
        scroll-timeline: timeline horizontal;
      }
    </style>
    <div class=host>
      <template shadowroot=open>
        <style>
            /* Not placing 'anim2' at document scope, due to
               https://crbug.com/1334534 */
            @keyframes anim2 {
              from { z-index: 100; background-color: green; }
              to { z-index: 100; background-color: green; }
            }
          .target {
            animation: anim2 10s linear;
            animation-timeline: timeline;
          }
        </style>
        <div part=foo>
          <div class=target></div>
        </div>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* host = GetDocument().QuerySelector(".host");
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->GetShadowRoot());
  Element* target = host->GetShadowRoot()->QuerySelector(".target");
  ASSERT_TRUE(target);
  HeapVector<Member<Animation>> animations = target->getAnimations();
  ASSERT_EQ(1u, animations.size());
  ASSERT_EQ(ScrollTimeline::ScrollAxis::kHorizontal,
            GetTimelineAxis(*animations[0]))
      << "Inner animation can see scroll timeline defined by ::part";
}

}  // namespace blink
