// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/page_form_analyser_logger.h"

#include <utility>

#include "base/strings/string_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_node.h"

namespace autofill {

struct PageFormAnalyserLogger::LogEntry {
  const std::string message;
  const std::vector<blink::WebNode> nodes;
};

PageFormAnalyserLogger::PageFormAnalyserLogger(blink::WebLocalFrame* frame)
    : frame_(frame) {}
PageFormAnalyserLogger::~PageFormAnalyserLogger() {}

void PageFormAnalyserLogger::Send(std::string message,
                                  ConsoleLevel level,
                                  blink::WebNode node) {
  Send(std::move(message), level, std::vector<blink::WebNode>{std::move(node)});
}

void PageFormAnalyserLogger::Send(std::string message,
                                  ConsoleLevel level,
                                  std::vector<blink::WebNode> nodes) {
  node_buffer_[level].push_back(LogEntry{std::move(message), std::move(nodes)});
}

void PageFormAnalyserLogger::Flush() {
  std::string text;
  for (ConsoleLevel level : {kError, kWarning, kVerbose}) {
    for (LogEntry& entry : node_buffer_[level]) {
      text.clear();
      text += "[DOM] ";
      text += entry.message;

      std::vector<blink::WebNode> nodes_to_log;
      for (unsigned i = 0; i < entry.nodes.size(); ++i) {
        if (entry.nodes[i].IsElementNode()) {
          const blink::WebElement element =
              entry.nodes[i].To<blink::WebElement>();
          const blink::WebInputElement input_element =
              element.DynamicTo<blink::WebInputElement>();

          // Filter out password inputs with values from being logged, as their
          // values are also logged.
          const bool should_obfuscate =
              !input_element.IsNull() &&
              input_element.IsPasswordFieldForAutofill() &&
              !input_element.Value().IsEmpty();

          if (!should_obfuscate) {
            text += " %o";
            nodes_to_log.push_back(element);
          }
        }
      }

      blink::WebConsoleMessage message(level, blink::WebString::FromUTF8(text));
      message.nodes = std::move(nodes_to_log);  // avoids copying node vectors.
      frame_->AddMessageToConsole(message);
    }
  }
  node_buffer_.clear();
}

}  // namespace autofill
