// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/request_matcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/updater/test/http_request.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"

namespace updater::test::request {

FormExpectations::FormExpectations(const std::string& name,
                                   std::vector<std::string> regexes)
    : name(name), regex_sequence(std::move(regexes)) {}

FormExpectations::FormExpectations(const FormExpectations&) = default;
FormExpectations& FormExpectations::operator=(const FormExpectations& other) =
    default;
FormExpectations::~FormExpectations() = default;

Matcher GetPathMatcher(const std::string& expected_path_regex) {
  return base::BindLambdaForTesting(
      [expected_path_regex](const HttpRequest& request) {
        if (!re2::RE2::FullMatch(request.relative_url, expected_path_regex)) {
          ADD_FAILURE() << "Request path [" << request.relative_url
                        << "], did not match expected path regex ["
                        << expected_path_regex << "].";
          return false;
        }
        return true;
      });
}

Matcher GetHeaderMatcher(const std::string& header_name,
                         const std::string& expected_header_regex) {
  return base::BindLambdaForTesting(
      [header_name, expected_header_regex](const HttpRequest& request) {
        re2::RE2::Options opt;
        opt.set_case_sensitive(false);
        HttpRequest::HeaderMap::const_iterator it =
            request.headers.find(header_name);
        if (it == request.headers.end()) {
          ADD_FAILURE() << "Request header '" << header_name
                        << "' not found, expected regex "
                        << expected_header_regex;
          return false;
        } else if (!re2::RE2::FullMatch(it->second,
                                        re2::RE2(expected_header_regex, opt))) {
          ADD_FAILURE() << "Request header [" << it->first << " = '"
                        << it->second << "], did not match expected regex ["
                        << expected_header_regex << "]";
          return false;
        }
        return true;
      });
}

Matcher GetContentMatcher(
    const std::vector<std::string>& expected_content_regex_sequence) {
  return base::BindLambdaForTesting(
      [expected_content_regex_sequence](const HttpRequest& request) {
        re2::StringPiece input(request.decoded_content);
        for (const std::string& regex : expected_content_regex_sequence) {
          re2::RE2::Options opt;
          opt.set_case_sensitive(false);
          if (re2::RE2::FindAndConsume(&input, re2::RE2(regex, opt))) {
            VLOG(3) << "Found regex: [" << regex << "]";
          } else {
            ADD_FAILURE() << "Request content match failed. Expected regex: ["
                          << regex << "] not found in content: ["
                          << GetPrintableContent(request) << "]";
            return false;
          }
        }
        return true;
      });
}

Matcher GetScopeMatcher(UpdaterScope scope) {
  return base::BindLambdaForTesting([scope](const HttpRequest& request) {
    const bool is_match = [&scope, &request]() {
      const absl::optional<base::Value> doc =
          base::JSONReader::Read(request.decoded_content);
      if (!doc || !doc->is_dict()) {
        return false;
      }
      const base::Value::Dict* object_request =
          doc->GetDict().FindDict("request");
      if (!object_request) {
        return false;
      }
      absl::optional<bool> ismachine = object_request->FindBool("ismachine");
      if (!ismachine.has_value()) {
        return false;
      }
      switch (scope) {
        case UpdaterScope::kSystem:
          return *ismachine;
        case UpdaterScope::kUser:
          return !*ismachine;
      }
    }();
    if (!is_match) {
      ADD_FAILURE() << R"(Request does not match "ismachine": )"
                    << GetPrintableContent(request);
    }
    return is_match;
  });
}

Matcher GetAppPriorityMatcher(const std::string& app_id,
                              UpdateService::Priority priority) {
  return base::BindLambdaForTesting(
      [app_id, priority](const HttpRequest& request) {
        const bool is_match = [&app_id, priority, &request]() {
          const absl::optional<base::Value> doc =
              base::JSONReader::Read(request.decoded_content);
          if (!doc || !doc->is_dict()) {
            return false;
          }
          const base::Value::List* app_list =
              doc->GetDict().FindListByDottedPath("request.app");
          if (!app_list) {
            return false;
          }
          for (const base::Value& app : *app_list) {
            if (const auto* dict = app.GetIfDict()) {
              if (const auto* appid = dict->FindString("appid");
                  *appid == app_id) {
                if (const auto* install_source =
                        dict->FindString("installsource")) {
                  return (*install_source == "ondemand") ==
                         (priority == UpdateService::Priority::kForeground);
                }
              }
            }
          }
          return priority != UpdateService::Priority::kForeground;
        }();
        if (!is_match) {
          ADD_FAILURE() << R"(Request does not match "appid", "priority: )"
                        << GetPrintableContent(request);
        }
        return is_match;
      });
}

Matcher GetMultipartContentMatcher(
    const std::vector<FormExpectations>& form_expections) {
  return base::BindLambdaForTesting([form_expections](
                                        const HttpRequest& request) {
    constexpr char kMultifpartBoundaryPrefix[] =
        "multipart/form-data; boundary=";
    if (request.headers.count("Content-Type") == 0) {
      ADD_FAILURE() << "Content-Type header not found, which is expected "
                    << "for multipart content.";
      return false;
    }

    const std::string content_type = request.headers.at("Content-Type");
    if (!base::StartsWith(content_type, kMultifpartBoundaryPrefix)) {
      ADD_FAILURE() << "Content-Type value is not the expected "
                    << "[multipart/form-data].";
      return false;
    }

    const std::string form_data_boundary = content_type.substr(
        base::StringPiece(kMultifpartBoundaryPrefix).length());

    re2::RE2::Options opt;
    opt.set_case_sensitive(false);
    re2::StringPiece input(request.decoded_content);
    for (std::vector<FormExpectations>::const_iterator form_expection =
             form_expections.begin();
         form_expection < form_expections.end(); ++form_expection) {
      if (re2::RE2::FindAndConsume(&input, form_data_boundary)) {
        VLOG(3) << "Advancing to next form in the multipart content.";
      } else {
        ADD_FAILURE() << "No boundary separator found between multipart forms.";
        return false;
      }

      const std::string& form_name = form_expection->name;
      if (re2::RE2::FindAndConsume(
              &input,
              base::StringPrintf(R"(Content-Disposition: form-data; name="%s")",
                                 form_name.c_str()))) {
        VLOG(3) << "Found form with name [" << form_name << "]";
      } else {
        ADD_FAILURE() << "Form [" << form_name << "] not found.";
        return false;
      }

      for (std::vector<std::string>::const_iterator regex =
               form_expection->regex_sequence.begin();
           regex < form_expection->regex_sequence.end(); ++regex) {
        if (re2::RE2::FindAndConsume(&input, re2::RE2(*regex, opt))) {
          VLOG(3) << "Found regex: [" << *regex << "]";
        } else {
          ADD_FAILURE() << "Form [" << form_name << "] match failed. "
                        << "Expected regex: [" << *regex << "] not found in "
                        << "content: [" << GetPrintableContent(request) << "]";
          return false;
        }
      }
    }

    if (!re2::RE2::FindAndConsume(&input, form_data_boundary)) {
      ADD_FAILURE() << "Multipart data should end with boundary separator.";
      return false;
    }

    return true;
  });
}

}  // namespace updater::test::request
