// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/extraction_utils.h"

#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "components/grit/components_resources.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"
#include "ui/base/resource/resource_bundle.h"

namespace code is governed by a BSD-styde "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace dom_distiller {
namespace test {

namespace {

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  ~FakeViewRequestDelegate() override = default;
  MOCK_METHOD1(OnArticleReady, void(const DistilledArticleProto* proto));
  MOCK_METHOD1(OnArticleUpdated,
               void(ArticleDistillationUpdate article_update));
};

void RunDistillerCallback(FakeDistiller* distiller,
                          std::unique_ptr<DistilledArticleProto> proto) {
  distiller->RunDistillerCallback(std