// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/kombucha_in_process_fuzzer.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
// The following includes are used to enable ui_controls only.
#include "ui/base/test/ui_controls.h"
#if BUILDFLAG(IS_OZONE)
#include "ui/views/test/test_desktop_screen_ozone.h"
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/test/ui_controls_ash.h"
#elif BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "ui/aura/test/ui_controls_aurawin.h"
#endif
#if defined(USE_AURA) && BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#endif  // defined(USE_AURA) && BUILDFLAG(IS_OZONE)

namespace {
ui::ElementTracker::ElementList GetTargetableEvents() {
  // Only views can be targeted by clicks or mouse drag.
  // See ui/views/interaction/interactive_views_test.cc:330.
  auto elements =
      ui::ElementTracker::GetElementTracker()->GetAllElementsForTesting();
  elements.erase(std::remove_if(elements.begin(), elements.end(), [](auto* e1) {
    // We must ensure to never select `kInteractiveTestPivotElementId` because
    // it is an internal element of the interactive test framework. Selecting it
    // will likely result in asserting in the framework.
    return !e1->template IsA<views::TrackedElementViews>() ||
           e1->identifier() ==
               ui::test::internal::kInteractiveTestPivotElementId;
  }));
  return elements;
}
}  // namespace

KombuchaInProcessFuzzer::KombuchaInProcessFuzzer() = default;
KombuchaInProcessFuzzer::~KombuchaInProcessFuzzer() = default;

#if BUILDFLAG(IS_WIN)
void KombuchaInProcessFuzzer::TearDown() {
  InteractiveBrowserTestT::TearDown();
  com_initializer_.reset();
}
#endif

void KombuchaInProcessFuzzer::SetUp() {
  scoped_feature_list_.InitWithFeatures(
      {features::kTabGroupsSave, features::kExtensionsMenuInAppMenu}, {});

  // Mouse movements require enabling ui_controls manually for tests
  // that live outside the ui_interaction_test directory.
  // The following is copied from
  // chrome/test/base/interactive_ui_tests_main.cc
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::test::EnableUIControlsAsh();
#elif BUILDFLAG(IS_WIN)
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
  aura::test::EnableUIControlsAuraWin();
#elif BUILDFLAG(IS_OZONE)
  // Notifies the platform that test config is needed. For Wayland, for
  // example, makes its possible to use emulated input.
  ui::test::EnableTestConfigForPlatformWindows();
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForUI(params);
  ui_controls::EnableUIControls();
#else
  ui_controls::EnableUIControls();
#endif

  InteractiveBrowserTestT::SetUp();
}

void KombuchaInProcessFuzzer::SetUpOnMainThread() {
  InteractiveBrowserTestT::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&KombuchaInProcessFuzzer::HandleHTTPRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  ASSERT_TRUE(embedded_test_server()->Start());
}

std::unique_ptr<net::test_server::HttpResponse>
KombuchaInProcessFuzzer::HandleHTTPRequest(
    base::WeakPtr<KombuchaInProcessFuzzer> fuzzer_weak,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  FuzzCase response_body;
  // We are running on the embedded test server's thread.
  // We want to ask the fuzzer thread for the latest payload,
  // but there's a risk of UaF if it's being destroyed.
  // We use a weak pointer, but we have to dereference that on the originating
  // thread.
  base::RunLoop run_loop;
  base::RepeatingCallback<void()> get_payload_lambda =
      base::BindLambdaForTesting([&]() {
        KombuchaInProcessFuzzer* fuzzer = fuzzer_weak.get();
        if (fuzzer) {
          response_body = fuzzer->current_fuzz_case_;
        }
        run_loop.Quit();
      });
  content::GetUIThreadTaskRunner()->PostTask(FROM_HERE, get_payload_lambda);
  run_loop.Run();
  std::string proto_debug_str = response_body.DebugString();

  response->set_content(base::StringPrintf(
      "<html><body><h1>hello world</h1><p>%s</p></body></html>",
      proto_debug_str.c_str()));
  response->set_code(net::HTTP_OK);
  return response;
}

int KombuchaInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  FuzzCase fuzz_case;
  fuzz_case.ParseFromArray(data, size);

  // Used to reassign target with NameElement
  constexpr char TargetName[] = "name";
  constexpr char OtherTargetName[] = "otherName";

  // Should only be defined on first run, as ElementIdentifiers persist when
  // batching
  // Redefining hits CHECK
  current_fuzz_case_ = fuzz_case;
  GURL test_url = embedded_test_server()->GetURL("/test.html");

  // Base input always used in fuzzer
  auto ui_input = Steps(Log("[KOMB] First Log Step!"));
  auto input_buffer = Steps(Log("[KOMB] Began procedurally generated inputs"));

  // Action can have arbitrary number of steps
  // Translate and append each step to run at once
  test::fuzzing::ui_fuzzing::Action action = fuzz_case.action();

  AddStep(input_buffer,
          Log("[KOMB] Count of Steps generated: ", action.steps_size()));

  // TODO(xrosado) Condense calls to NameElement
  for (int j = 0; j < action.steps_size(); j++) {
    test::fuzzing::ui_fuzzing::Step step = action.steps(j);
    switch (step.step_choice_case()) {
      case test::fuzzing::ui_fuzzing::Step::kClickAt: {
        int target = step.click_at().target();
        AddStep(input_buffer,
                NameElement(TargetName, base::BindLambdaForTesting([target]() {
                              auto elements = GetTargetableEvents();
                              auto* choice = elements[target % elements.size()];
                              return choice;
                            })));

        AddStep(input_buffer,
                Steps(step.click_at().right_click() ? ClickRight(TargetName)
                                                    : ClickLeft(TargetName)));

        AddStep(input_buffer, Log("[KOMB] Added ClickAt"));
        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kDragFromTo: {
        int source = step.drag_from_to().source();
        int dest = step.drag_from_to().dest();
        AddStep(input_buffer,
                NameElement(TargetName, base::BindLambdaForTesting([source]() {
                              auto elements = GetTargetableEvents();
                              auto* choice = elements[source % elements.size()];
                              return choice;
                            })));
        AddStep(
            input_buffer,
            NameElement(OtherTargetName, base::BindLambdaForTesting([dest]() {
                          auto elements = GetTargetableEvents();
                          auto* choice = elements[dest % elements.size()];
                          return choice;
                        })));
        AddStep(input_buffer, DragFromTo(TargetName, OtherTargetName));
        AddStep(input_buffer, Log("[KOMB] Added DragFromTo"));
        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kSendAccelerator: {
        int chosen_id = step.send_accelerator().target();
        AddStep(input_buffer, Log("[KOMB] Adding Accelerator: ", chosen_id));

        // Set current_accelerator_ to chosen id's accelerator then add it to
        // input
        chrome::AcceleratorProviderForBrowser(browser())
            ->GetAcceleratorForCommandId(chosen_id, &current_accelerator_);
        AddStep(input_buffer,
                SendAccelerator(kBrowserViewElementId, current_accelerator_));
        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kClickTab: {
        int target = step.click_tab().target();
        bool right_click = step.click_tab().right_click();

        AddStep(input_buffer,
                If([]() { return true; },
                   [&, target]() {
                     auto index =
                         target % browser()->tab_strip_model()->count();
                     return Steps(ClickTab(index, right_click),
                                  Log("[KOMB] Added ClickTab index:", index,
                                      " target: ", target, " tab_count: ",
                                      browser()->tab_strip_model()->count()));
                   }()));
        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kClickTabGroupHeader: {
        int target = step.click_tab_group_header().target();
        int right_click = step.click_tab_group_header().right_click();
        AddStep(input_buffer,
                If(
                    [this]() {
                      return browser()->tab_strip_model()->SupportsTabGroups();
                    },
                    [this, target, right_click]() {
                      auto groups = browser()
                                        ->tab_strip_model()
                                        ->group_model()
                                        ->ListTabGroups();
                      int size = groups.size();
                      if (size == 0) {
                        return Steps(
                            Log("[KOMB] Attempted ClickTabGroupHeader. "
                                "Couldn't select tab group. Empty list!"));
                      }
                      auto tab_group = groups[target % size];
                      return ClickTabGroupHeader(tab_group, right_click);
                    }()));
        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kSaveTabGroup: {
        int target = step.save_tab_group().target();
        bool close_editor = step.save_tab_group().close_editor();

        AddStep(input_buffer,
                If(
                    [this]() {
                      return browser()->tab_strip_model()->SupportsTabGroups();
                    },
                    [this, target, close_editor]() {
                      TabStripModel* tab_strip = browser()->tab_strip_model();
                      auto groups = tab_strip->group_model()->ListTabGroups();
                      int size = groups.size();
                      if (size == 0) {
                        return Steps(
                            Log("[KOMB] Attempted SaveTabGroup. Couldn't save "
                                "tab group. Empty list!"));
                      }
                      auto tab_group = groups[target % size];
                      return close_editor
                                 ? SaveGroupAndCloseEditorBubble(tab_group)
                                 : SaveGroupLeaveEditorBubbleOpen(tab_group);
                    }()));
        break;
      }

      case test::fuzzing::ui_fuzzing::Step::kAddNewTabGroup: {
        int target = step.add_new_tab_group().target();

        AddStep(
            input_buffer,
            If(
                [this]() {
                  return browser()->tab_strip_model()->SupportsTabGroups();
                },
                [this, target]() {
                  return Steps(
                      Do([this, target]() {
                        int actual_target =
                            target % browser()->tab_strip_model()->count();
                        browser()->tab_strip_model()->AddToNewGroup(
                            {actual_target});
                      }),
                      Log("[KOMB] Added New Tab Group with Target: ", target));
                }()));
        break;
      }
      default:  // Unspecified Value
        break;
    }
  }
  AddStep(ui_input, std::move(input_buffer));
  RunTestSequence(std::move(ui_input));
  return 0;
}

REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(KombuchaInProcessFuzzer)
