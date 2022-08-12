// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_WORKER_TESTING_TEST_WITH_JAVASCRIPT_H_
#define COBALT_WORKER_TESTING_TEST_WITH_JAVASCRIPT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "cobalt/base/type_id.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/context.h"
#include "cobalt/web/testing/stub_web_context.h"
#include "cobalt/worker/dedicated_worker_global_scope.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/worker_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {
namespace testing {

// Helper class for running tests in a WorkerGlobalScope JavaScript context.
template <class TypeIdProvider>
class TestWithJavaScriptBase : public TypeIdProvider {
 public:
  TestWithJavaScriptBase() {
    web_context_.reset(new web::testing::StubWebContext());
    web_context_->setup_environment_settings(new WorkerSettings());
    web_context_->environment_settings()->set_creation_url(GURL("about:blank"));

    if (TypeIdProvider::GetGlobalScopeTypeId() ==
        base::GetTypeId<DedicatedWorkerGlobalScope>()) {
      dedicated_worker_global_scope_ =
          new DedicatedWorkerGlobalScope(web_context_->environment_settings());
      dedicated_worker_global_scope_->set_name("TestWithJavaScriptBase");
      web_context_->global_environment()->CreateGlobalObject(
          dedicated_worker_global_scope_, web_context_->environment_settings());
      worker_global_scope_ = dedicated_worker_global_scope_.get();
    } else if (TypeIdProvider::GetGlobalScopeTypeId() ==
               base::GetTypeId<ServiceWorkerGlobalScope>()) {
      containing_service_worker_registration_ =
          new ServiceWorkerRegistrationObject(url::Origin(), GURL(),
                                              kServiceWorkerUpdateViaCacheNone);
      service_worker_object_ =
          new ServiceWorkerObject(ServiceWorkerObject::Options(
              "TestServiceWorkerObject", web_context_->web_settings(),
              web_context_->network_module(),
              containing_service_worker_registration_));
      service_worker_global_scope_ = new ServiceWorkerGlobalScope(
          web_context_->environment_settings(), service_worker_object_);
      web_context_->global_environment()->CreateGlobalObject(
          service_worker_global_scope_, web_context_->environment_settings());
      worker_global_scope_ = service_worker_global_scope_.get();
    }
  }

  ~TestWithJavaScriptBase() { ClearWebContext(); }

  void ClearWebContext() {
    dedicated_worker_global_scope_ = nullptr;
    service_worker_object_ = nullptr;
    containing_service_worker_registration_ = nullptr;
    service_worker_global_scope_ = nullptr;
    worker_global_scope_ = nullptr;
    web_context_.reset();
  }

  WorkerGlobalScope* worker_global_scope() const {
    return worker_global_scope_;
  }

  virtual web::Context* web_context() const {
    DCHECK(web_context_);
    return web_context_.get();
  }

  scoped_refptr<script::GlobalEnvironment> global_environment() const {
    DCHECK(this->web_context());
    return this->web_context()->global_environment();
  }

  bool EvaluateScript(const std::string& js_code,
                      std::string* result = nullptr) {
    DCHECK(global_environment());
    return global_environment()->EvaluateScript(
        CreateSourceCodeAndPrepareEval(js_code), result);
  }

  bool EvaluateScript(
      const std::string& js_code,
      base::Optional<script::ValueHandleHolder::Reference>* result) {
    DCHECK(global_environment());
    return global_environment()->EvaluateScript(
        CreateSourceCodeAndPrepareEval(js_code),
        this->web_context()->GetWindowOrWorkerGlobalScope(), result);
  }

  ::testing::StrictMock<script::testing::MockExceptionState>*
  exception_state() {
    return &exception_state_;
  }

 private:
  scoped_refptr<script::SourceCode> CreateSourceCodeAndPrepareEval(
      const std::string& js_code) {
    DCHECK(global_environment());
    global_environment()->EnableEval();
    global_environment()->SetReportEvalCallback(base::Closure());
    return script::SourceCode::CreateSourceCode(
        js_code, base::SourceLocation(__FILE__, __LINE__, 1));
  }

  std::unique_ptr<web::testing::StubWebContext> web_context_;
  WorkerGlobalScope* worker_global_scope_ = nullptr;
  scoped_refptr<DedicatedWorkerGlobalScope> dedicated_worker_global_scope_;
  scoped_refptr<ServiceWorkerObject> service_worker_object_;
  scoped_refptr<ServiceWorkerRegistrationObject>
      containing_service_worker_registration_;
  scoped_refptr<ServiceWorkerGlobalScope> service_worker_global_scope_;
  ::testing::StrictMock<script::testing::MockExceptionState> exception_state_;
};

template <class GlobalScope>
class GetGlobalScopeTypeIdBase : public ::testing::Test {
 public:
  base::TypeId GetGlobalScopeTypeId() const {
    return base::GetTypeId<GlobalScope>();
  }
};

class GetGlobalScopeTypeIdWithParam
    : public ::testing::TestWithParam<base::TypeId> {
 public:
  base::TypeId GetGlobalScopeTypeId() const { return GetParam(); }
  static void SetUpTestCase() {}
  static void TearDownTestCase() {}
};

// Helper class for running TEST_P() tests on all known worker types.
class TestWorkersWithJavaScript
    : public TestWithJavaScriptBase<GetGlobalScopeTypeIdWithParam> {
 public:
  // Return a vector of values for all known worker types, to be used in the
  // INSTANTIATE_TEST_CASE_P() declaration.
  static std::vector<base::TypeId> GetWorkerTypes() {
    std::vector<base::TypeId> worker_types = {
        base::GetTypeId<DedicatedWorkerGlobalScope>(),
        base::GetTypeId<ServiceWorkerGlobalScope>()};
    return worker_types;
  }
  static std::string GetTypeName(::testing::TestParamInfo<base::TypeId> info) {
    if (info.param == base::GetTypeId<DedicatedWorkerGlobalScope>()) {
      return "DedicatedWorkerGlobalScope";
    }
    if (info.param == base::GetTypeId<ServiceWorkerGlobalScope>()) {
      return "ServiceWorkerGlobalScope";
    }
    return "Unknown";
  }
};

// Helper classes for running TEST_F() tests on only one type of worker.
class TestDedicatedWorkerWithJavaScript
    : public TestWithJavaScriptBase<
          GetGlobalScopeTypeIdBase<DedicatedWorkerGlobalScope>> {};

// Helper classes for running TEST_F() tests on only one type of worker.
class TestServiceWorkerWithJavaScript
    : public TestWithJavaScriptBase<
          GetGlobalScopeTypeIdBase<ServiceWorkerGlobalScope>> {};

}  // namespace testing
}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_TESTING_TEST_WITH_JAVASCRIPT_H_
