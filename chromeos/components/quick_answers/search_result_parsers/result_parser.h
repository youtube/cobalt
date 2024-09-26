// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_RESULT_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_RESULT_PARSER_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace quick_answers {

// Parser interface.
class ResultParser {
 public:
  virtual ~ResultParser() = default;

  // Parse the result into |quick_answer|.
  virtual bool Parse(const base::Value::Dict& result,
                     QuickAnswer* quick_answer) = 0;

 protected:
  // Helper function to get the first element in a value list, which is expected
  // to be a dictionary.
  const base::Value::Dict* GetFirstListElement(const base::Value::Dict& dict,
                                               const std::string& path);
};

// A factory class for creating ResultParser based on the |one_namespace_type|.
class ResultParserFactory {
 public:
  // Creates ResultParser based on the |one_namespace_type|.
  static std::unique_ptr<ResultParser> Create(int one_namespace_type);

  virtual ~ResultParserFactory() = default;
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_RESULT_PARSER_H_
