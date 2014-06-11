// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/state_machine_shell.h"

#include <list>
#include <iostream>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

// Constant to represent no version for TestHsm::Expect.
static const uint64_t kNoVersion = static_cast<uint64_t>(-1);

// --- Test Subclass ---

// StateMachineShell is an abstract class, so we must subclass it to test it.
// This class uses the sample test state machine specified by Miro Samek in his
// Practical Statecharts book. It covers the interesting transitions and
// topologies, so if it's fully exercised, it should represent a
// close-to-canonical test of the state machine's facilities.
//
// The diagram of this statechart is reproduced here:
// http://www.state-machine.com/resources/Heinzmann04.pdf
//
// This version has an extra event, I, to test reentrant event handling.
class TestHsm : public base::StateMachineShell {
 public:
  // --- States for this HSM ---

  // The predefined state TOP and the specified state S0 are the same.
  static const State kStateS0 = kStateTop;
  static const State kStateS1 = 0;
  static const State kStateS11 = 1;
  static const State kStateS2 = 2;
  static const State kStateS21 = 3;
  static const State kStateS211 = 4;

  // --- Events for this HSM ---

  static const Event kEventA = 0;
  static const Event kEventB = 1;
  static const Event kEventC = 2;
  static const Event kEventD = 3;
  static const Event kEventE = 4;
  static const Event kEventF = 5;
  static const Event kEventG = 6;
  static const Event kEventH = 7;
  static const Event kEventI = 8;

  // --- Some types to aid sensing ---

  // An enumeration of things that the HSM does that we can sense and then
  // assert about.
  enum HsmEvent {
    kEnter,
    kExit,
    kHandled
  };

  struct ResultEvent {
    // The state the HSM was in when it handled the event.
    State state;

    // The data passed into the event, if any.
    void *data;

    // The state that actually handled the event (could be an ancestor of the
    // current state.
    State event_state;

    // The event that was handled.
    Event event;

    // The "HSM Event" that occurred causing this to be recorded.
    HsmEvent hsm_event;

    // The state version at the time the HsmEvent occured.
    uint64_t version;
  };

  // --- Extra state to aid sensing ---

  // Foo is used as extended state and in guard conditions in the test HSM (see
  // link above).
  bool foo_;

  // A counter for how many times the S11::kEventI handler is invoked.
  int event_i_count_;

  // A collection of interesting things that has happened to this state
  // machine. Used to validate that the HSM behaved as expected.
  std::list<ResultEvent> results;

  TestHsm()
      : base::StateMachineShell("TestHsm"),
        foo_(true),
        event_i_count_(0) { }
  virtual ~TestHsm() { }

  // Clears the results list, so nothing bleeds between test cases.
  void ClearResults() {
    results.clear();
  }

  // Consumes and validates a ResultEvent from the results list.
  void Expect(TestHsm::HsmEvent hsmEvent, State eventState = kStateNone,
              Event event = kEventNone, State currentState = kStateNone,
              void *data = NULL, uint64_t version = kNoVersion) {
    DLOG(INFO) << __FUNCTION__ << ": hsmEvent=" << hsmEvent
               << ", eventState=" << GetStateString(eventState)
               << ", event=" << GetEventString(event)
               << ", currentState=" << GetStateString(currentState)
               << ", data=0x" << std::hex << data
               << ", version=" << version;
    EXPECT_FALSE(results.empty());
    TestHsm::ResultEvent result = results.front();
    results.pop_front();
    EXPECT_EQ(hsmEvent, result.hsm_event);
    if (eventState != kStateNone) {
      EXPECT_EQ(eventState, result.event_state);
      if (currentState == kStateNone) {
        EXPECT_EQ(eventState, result.state);
      }
    }

    if (currentState != kStateNone) {
      EXPECT_EQ(currentState, result.state);
    }

    if (event != kEventNone) {
      EXPECT_EQ(event, result.event);
    }
    EXPECT_EQ(data, result.data);

    if (version != kNoVersion) {
      EXPECT_EQ(version, result.version);
    }
  }


  // --- StateMachineShell Implementation ---
 protected:
  virtual State GetUserParentState(State state) const OVERRIDE {
    switch (state) {
      case kStateS1:
      case kStateS2:
        return kStateS0;
      case kStateS11:
        return kStateS1;
      case kStateS21:
        return kStateS2;
      case kStateS211:
        return kStateS21;
      default:
        return kStateNone;
    }
  }

  virtual State GetUserInitialSubstate(State state) const OVERRIDE {
    switch (state) {
      case kStateS0:
        return kStateS1;
      case kStateS1:
        return kStateS11;
      case kStateS2:
        return kStateS21;
      case kStateS21:
        return kStateS211;
      default:
        return kStateNone;
    }
  }

  virtual const char *GetUserStateString(State state) const OVERRIDE {
    switch (state) {
      case kStateS1:
        return "S1";
      case kStateS11:
        return "S11";
      case kStateS2:
        return "S2";
      case kStateS21:
        return "S21";
      case kStateS211:
        return "S211";
      default:
        return NULL;
    }
  }

  virtual const char *GetUserEventString(Event event) const OVERRIDE {
    switch (event) {
      case kEventA:
        return "A";
      case kEventB:
        return "B";
      case kEventC:
        return "C";
      case kEventD:
        return "D";
      case kEventE:
        return "E";
      case kEventF:
        return "F";
      case kEventG:
        return "G";
      case kEventH:
        return "H";
      case kEventI:
        return "I";
      default:
        return NULL;
    }
  }

  virtual State HandleUserStateEvent(State state, Event event, void *data)
      OVERRIDE {
    DLOG(INFO) << __FUNCTION__ << "(" << GetStateString(state) << ", "
               << GetEventString(event) << ", 0x" << std::hex << data << ");";

    if (event == kEventEnter) {
      AddEvent(state, event, data, kEnter);
      return kStateNone;
    }

    if (event == kEventExit) {
      AddEvent(state, event, data, kExit);
      return kStateNone;
    }

    State result = kNotHandled;
    switch (state) {
      case kStateS0:
        switch (event) {
          case kEventE:
            result = kStateS211;
            break;
          default:
            // Not Handled
            break;
        }
        break;

      case kStateS1:
        switch (event) {
          case kEventA:
            MakeExternalTransition();
            result = kStateS1;
            break;
          case kEventB:
            result = kStateS11;
            break;
          case kEventC:
            result = kStateS2;
            break;
          case kEventD:
            result = kStateS0;
            break;
          case kEventF:
            result = kStateS211;
            break;
          default:
            // Not Handled
            break;
        }
        break;

      case kStateS11:
        switch (event) {
          case kEventG:
            result = kStateS211;
            break;
          case kEventH:
            if (foo_) {
              foo_ = false;
              result = kStateNone;
              break;
            }
            break;
          case kEventI:
            // Inject another I every other time I is handled so every I should
            // ultimately increase event_i_count_ by 2.
            ++event_i_count_;
            if (event_i_count_ % 2 == 1) {
              // This should queue and be run before Handle() returns.
              Handle(kEventI);
              result = kStateS1;
              break;
            } else {
              result = kStateNone;
              break;
            }
            break;
          default:
            // Not Handled
            break;
        }
        break;

      case kStateS2:
        switch (event) {
          case kEventC:
            result = kStateS1;
            break;
          case kEventF:
            result = kStateS11;
            break;
          default:
            // Not Handled
            break;
        }
        break;

      case kStateS21:
        switch (event) {
          case kEventB:
            result = kStateS211;
            break;
          case kEventH:
            if (!foo_) {
              foo_ = true;
              MakeExternalTransition();
              result = kStateS21;
              break;
            }
            break;
          default:
            // Not Handled
            break;
        }
        break;

      case kStateS211:
        switch (event) {
          case kEventD:
            result = kStateS21;
            break;
          case kEventG:
            result = kStateS0;
            break;
          default:
            // Not Handled
            break;
        }
        break;

      default:
        // Not Handled
        break;
    }

    if (result != kNotHandled) {
      AddEvent(state, event, data, kHandled);
    }

    return result;
  }

 private:
  // Adds a new record to the result list.
  void AddEvent(State state, Event event, void *data, HsmEvent hsm_event) {
    ResultEvent result_event = { this->state(), data, state, event, hsm_event,
                                 this->version() };
    results.push_back(result_event);
  }
};

#if !defined(_MSC_VER)
const base::StateMachineShell::State TestHsm::kStateS0;
const base::StateMachineShell::State TestHsm::kStateS1;
const base::StateMachineShell::State TestHsm::kStateS11;
const base::StateMachineShell::State TestHsm::kStateS2;
const base::StateMachineShell::State TestHsm::kStateS21;
const base::StateMachineShell::State TestHsm::kStateS211;
const base::StateMachineShell::Event TestHsm::kEventA;
const base::StateMachineShell::Event TestHsm::kEventB;
const base::StateMachineShell::Event TestHsm::kEventC;
const base::StateMachineShell::Event TestHsm::kEventD;
const base::StateMachineShell::Event TestHsm::kEventE;
const base::StateMachineShell::Event TestHsm::kEventF;
const base::StateMachineShell::Event TestHsm::kEventG;
const base::StateMachineShell::Event TestHsm::kEventH;
const base::StateMachineShell::Event TestHsm::kEventI;
#endif

// --- Test Definitions ---

// This test validates that a state machine will initialize itself when it
// handles its first event, even if the user has not explicitly called
// initialize.
TEST(StateMachineShellTest, AutoInit) {
  TestHsm hsm;

  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventA);

  // The HSM should Auto-Initialize
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS0);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);

  // Then it should handle the event
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS1, TestHsm::kEventA,
             TestHsm::kStateS11, 0);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(1, hsm.version());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());
}


// This test validates that if Handle() is called from within an event handler,
// it queues the event rather than trying to Handle the next event
// reentrantly. This behavior, or something like it, is required to make the
// state machine truly run-to-completion.
TEST(StateMachineShellTest, ReentrantHandle) {
  TestHsm hsm;
  hsm.Initialize();
  EXPECT_EQ(0, hsm.version());
  hsm.ClearResults();

  // Test a Handle() inside Handle()
  EXPECT_EQ(0, hsm.event_i_count_);
  hsm.Handle(TestHsm::kEventI);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS11, TestHsm::kEventI,
             TestHsm::kStateS11, NULL, 0);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS11, TestHsm::kEventI,
             TestHsm::kStateS11, NULL, 1);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(1, hsm.version());
  EXPECT_EQ(2, hsm.event_i_count_);
}

// This test validates that every meaningful event in every state in the test
// state machine behaves as expected. This should cover all normal operation of
// the state machine framework, except what is extracted into their own test
// cases above.
TEST(StateMachineShellTest, KitNKaboodle) {
  TestHsm hsm;

  // Test the initial state
  EXPECT_EQ(0, hsm.version());
  hsm.Initialize();
  EXPECT_EQ(0, hsm.version());
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS0);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // Test IsIn
  EXPECT_TRUE(hsm.IsIn(TestHsm::kStateS11));
  EXPECT_TRUE(hsm.IsIn(TestHsm::kStateS1));
  EXPECT_TRUE(hsm.IsIn(TestHsm::kStateS0));
  EXPECT_FALSE(hsm.IsIn(TestHsm::kStateS2));
  EXPECT_FALSE(hsm.IsIn(TestHsm::kStateS21));
  EXPECT_FALSE(hsm.IsIn(TestHsm::kStateS211));

  // State: S11, Event: A
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventA);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS1, TestHsm::kEventA,
             TestHsm::kStateS11, 0);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(1, hsm.version());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // State: S11, Event: B
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventB);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS1, TestHsm::kEventB,
             TestHsm::kStateS11, NULL, 1);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(2, hsm.version());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // State: S11, Event: D
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventD);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS1, TestHsm::kEventD,
             TestHsm::kStateS11, NULL, 2);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(3, hsm.version());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // State: S11, Event: H (foo == true)
  hsm.ClearResults();
  EXPECT_TRUE(hsm.foo_);
  hsm.Handle(TestHsm::kEventH);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS11, TestHsm::kEventH,
             TestHsm::kStateS11, NULL, 3);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(3, hsm.version());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());
  EXPECT_FALSE(hsm.foo_);

  // State: S11, Event: H (foo == false)
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventH);
  EXPECT_FALSE(hsm.foo_);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(3, hsm.version());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // State: S11, Event: C
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventC);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS1, TestHsm::kEventC,
             TestHsm::kStateS11, NULL, 3);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(4, hsm.version());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: A
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventA);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: B
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventB);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS21, TestHsm::kEventB,
             TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS211);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: D
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventD);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS211, TestHsm::kEventD,
             TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS211);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: E
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventE);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS0, TestHsm::kEventE,
             TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: F
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventF);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS2, TestHsm::kEventF,
             TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // State: S11, Event: E
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventE);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS0, TestHsm::kEventE,
             TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: G
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventG);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS211, TestHsm::kEventG,
             TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // State: S11, Event: F
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventF);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS1, TestHsm::kEventF,
             TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: H (foo == false)
  EXPECT_FALSE(hsm.foo_);
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventH);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS21, TestHsm::kEventH,
             TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_TRUE(hsm.foo_);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: H (foo == true)
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventH);
  EXPECT_TRUE(hsm.foo_);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // State: S211, Event: C
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventC);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS2, TestHsm::kEventC,
             TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS211);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS11);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS11, hsm.state());

  // State: S11, Event: G
  hsm.ClearResults();
  hsm.Handle(TestHsm::kEventG);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS11, TestHsm::kEventG,
             TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS11);
  hsm.Expect(TestHsm::kExit, TestHsm::kStateS1);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS2);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS21);
  hsm.Expect(TestHsm::kEnter, TestHsm::kStateS211);
  EXPECT_EQ(0, hsm.results.size());
  EXPECT_EQ(TestHsm::kStateS211, hsm.state());

  // Test that event data gets passed through to the handler
  hsm.ClearResults();
  void *data = reinterpret_cast<void *>(7);
  hsm.Handle(TestHsm::kEventC, data);
  hsm.Expect(TestHsm::kHandled, TestHsm::kStateS2, TestHsm::kEventC,
             TestHsm::kStateS211, data);

  EXPECT_EQ(TestHsm::kStateS11, hsm.state());
}
