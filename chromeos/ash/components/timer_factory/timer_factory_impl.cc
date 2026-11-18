// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/timer_factory/timer_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"

namespace ash::timer_factory {

namespace {

std::unique_ptr<TimerFactoryImpl::Factory>& GetTestFactory() {
  static base::NoDestructor<std::unique_ptr<TimerFactoryImpl::Factory>>
      test_factory;
  return *test_factory;
}

}  // namespace

// static
std::unique_ptr<TimerFactory> TimerFactoryImpl::Factory::Create() {
  if (GetTestFactory()) {
    return GetTestFactory()->CreateInstance();
  }

  return base::WrapUnique(new TimerFactoryImpl());
}

// static
void TimerFactoryImpl::Factory::SetFactoryForTesting(
    std::unique_ptr<Factory> test_factory) {
  GetTestFactory() = std::move(test_factory);
}

TimerFactoryImpl::Factory::~Factory() = default;

TimerFactoryImpl::TimerFactoryImpl() = default;

TimerFactoryImpl::~TimerFactoryImpl() = default;

std::unique_ptr<base::OneShotTimer> TimerFactoryImpl::CreateOneShotTimer() {
  return std::make_unique<base::OneShotTimer>();
}

}  // namespace ash::timer_factory
