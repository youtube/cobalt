/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/selector.h"

#include "cobalt/cssom/child_combinator.h"
#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/descendant_combinator.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/type_selector.h"
#include "cobalt/cssom/universal_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(SelectorTest, UniversalSelectorSpecificity) {
  scoped_ptr<SimpleSelector> universal_selector(new UniversalSelector());
  EXPECT_EQ(Specificity(0, 0, 0), universal_selector->GetSpecificity());
}

TEST(SelectorTest, TypeSelectorSpecificity) {
  scoped_ptr<SimpleSelector> type_selector(new TypeSelector("div"));
  EXPECT_EQ(Specificity(0, 0, 1), type_selector->GetSpecificity());
}

TEST(SelectorTest, ClassSelectorSpecificity) {
  scoped_ptr<SimpleSelector> class_selector(new ClassSelector("my-class"));
  EXPECT_EQ(Specificity(0, 1, 0), class_selector->GetSpecificity());
}

TEST(SelectorTest, IdSelectorSpecificity) {
  scoped_ptr<SimpleSelector> id_selector(new IdSelector("my-id"));
  EXPECT_EQ(Specificity(1, 0, 0), id_selector->GetSpecificity());
}

TEST(SelectorTest, EmptyPseudoClassSpecificity) {
  scoped_ptr<SimpleSelector> empty_pseudo_class(new EmptyPseudoClass());
  EXPECT_EQ(Specificity(0, 1, 0), empty_pseudo_class->GetSpecificity());
}

TEST(SelectorTest, CompoundSelectorSpecificity) {
  // The following compound selector is "div.my-class#my-id:empty".
  //
  scoped_ptr<CompoundSelector> compound_selector(new CompoundSelector());

  scoped_ptr<SimpleSelector> class_selector(new ClassSelector("my-class"));
  compound_selector->AppendSelector(class_selector.Pass());

  scoped_ptr<SimpleSelector> empty_pseudo_class(new EmptyPseudoClass());
  compound_selector->AppendSelector(empty_pseudo_class.Pass());

  scoped_ptr<SimpleSelector> id_selector(new IdSelector("my-id"));
  compound_selector->AppendSelector(id_selector.Pass());

  scoped_ptr<SimpleSelector> type_selector(new TypeSelector("div"));
  compound_selector->AppendSelector(type_selector.Pass());

  EXPECT_EQ(Specificity(1, 2, 1), compound_selector->GetSpecificity());
}

TEST(SelectorTest, ComplexSelectorSpecificity) {
  // The following complex selector is "div div > div.my-class#my-id:empty".
  //
  scoped_ptr<ComplexSelector> complex_selector(new ComplexSelector());

  scoped_ptr<CompoundSelector> compound_selector(new CompoundSelector());

  scoped_ptr<SimpleSelector> class_selector(new ClassSelector("my-class"));
  compound_selector->AppendSelector(class_selector.Pass());

  scoped_ptr<SimpleSelector> empty_pseudo_class(new EmptyPseudoClass());
  compound_selector->AppendSelector(empty_pseudo_class.Pass());

  scoped_ptr<SimpleSelector> id_selector(new IdSelector("my-id"));
  compound_selector->AppendSelector(id_selector.Pass());

  scoped_ptr<SimpleSelector> type_selector(new TypeSelector("div"));
  compound_selector->AppendSelector(type_selector.Pass());

  scoped_ptr<CompoundSelector> compound_selector2(new CompoundSelector());
  compound_selector2->AppendSelector(
      make_scoped_ptr<SimpleSelector>(new TypeSelector("div")));
  complex_selector->AppendSelector(compound_selector2.Pass());

  scoped_ptr<CompoundSelector> compound_selector3(new CompoundSelector());
  scoped_ptr<ChildCombinator> child_combinator(new ChildCombinator());
  compound_selector3->AppendSelector(
      make_scoped_ptr<SimpleSelector>(new TypeSelector("div")));
  complex_selector->AppendCombinatorAndSelector(
      child_combinator.PassAs<Combinator>(), compound_selector3.Pass());

  scoped_ptr<DescendantCombinator> descendant_combinator(
      new DescendantCombinator());
  complex_selector->AppendCombinatorAndSelector(
      descendant_combinator.PassAs<Combinator>(), compound_selector.Pass());

  EXPECT_EQ(Specificity(1, 2, 3), complex_selector->GetSpecificity());
}

}  // namespace cssom
}  // namespace cobalt
