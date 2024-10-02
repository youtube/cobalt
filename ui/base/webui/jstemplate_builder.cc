// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper function for using JsTemplate. See jstemplate_builder.h for more
// info.

#include "ui/base/webui/jstemplate_builder.h"

#include "base/check.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/resources/grit/webui_resources.h"

namespace webui {

namespace {

// Appends a script tag with a variable name |templateData| that has the JSON
// assigned to it.
void AppendJsonHtml(const base::Value::Dict& json, std::string* output) {
  std::string javascript_string;
  AppendJsonJS(json, &javascript_string, /*from_js_module=*/false);

  // </ confuses the HTML parser because it could be a </script> tag.  So we
  // replace </ with <\/.  The extra \ will be ignored by the JS engine.
  base::ReplaceSubstringsAfterOffset(&javascript_string, 0, "</", "<\\/");

  output->append("<script>");
  output->append(javascript_string);
  output->append("</script>");
}

// Appends the source for load_time_data.js in a script tag.
void AppendLoadTimeData(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  std::string load_time_data_src =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEBUI_JS_LOAD_TIME_DATA_DEPRECATED_JS);

  if (load_time_data_src.empty()) {
    NOTREACHED() << "Unable to get loadTimeData src";
    return;
  }

  output->append("<script>");
  output->append(load_time_data_src);
  output->append("</script>");
}

// Appends the source for JsTemplates in a script tag.
void AppendJsTemplateSourceHtml(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  std::string jstemplate_src =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_JSTEMPLATE_JSTEMPLATE_COMPILED_JS);

  if (jstemplate_src.empty()) {
    NOTREACHED() << "Unable to get jstemplate src";
    return;
  }

  output->append("<script>");
  output->append(jstemplate_src);
  output->append("</script>");
}

// Appends the code that processes the JsTemplate with the JSON. You should
// call AppendJsTemplateSourceHtml and AppendLoadTimeData before calling this.
void AppendJsTemplateProcessHtml(const base::Value::Dict& json,
                                 base::StringPiece template_id,
                                 std::string* output) {
  std::string jstext;
  JSONStringValueSerializer serializer(&jstext);
  serializer.Serialize(json);

  output->append("<script>");
  output->append("const pageData = ");
  output->append(jstext);
  output->append(";");
  output->append("loadTimeData.data = pageData;");
  output->append("var tp = document.getElementById('");
  output->append(template_id.data(), template_id.size());
  output->append("');");
  output->append("jstProcess(new JsEvalContext(pageData), tp);");
  output->append("</script>");
}

}  // namespace

std::string GetI18nTemplateHtml(base::StringPiece html_template,
                                const base::Value::Dict& json) {
  ui::TemplateReplacements replacements;
  ui::TemplateReplacementsFromDictionaryValue(json, &replacements);
  std::string output =
      ui::ReplaceTemplateExpressions(html_template, replacements);

  AppendLoadTimeData(&output);
  AppendJsonHtml(json, &output);

  return output;
}

std::string GetTemplatesHtml(base::StringPiece html_template,
                             const base::Value::Dict& json,
                             base::StringPiece template_id) {
  ui::TemplateReplacements replacements;
  ui::TemplateReplacementsFromDictionaryValue(json, &replacements);
  std::string output =
      ui::ReplaceTemplateExpressions(html_template, replacements);

  AppendJsTemplateSourceHtml(&output);
  AppendLoadTimeData(&output);
  AppendJsTemplateProcessHtml(json, template_id, &output);
  return output;
}

void AppendJsonJS(const base::Value::Dict& json,
                  std::string* output,
                  bool from_js_module) {
  if (from_js_module) {
    // If the script is being imported as a module, import |loadTimeData| in
    // order to allow assigning the localized strings to loadTimeData.data.
    output->append("import {loadTimeData} from ");
    output->append("'//resources/js/load_time_data.js';\n");

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Imported for the side effect of setting the |window.loadTimeData| global,
    // which is relied on by ChromeOS Ash Tast Tests and some browser tests.
    // See https://www.crbug.com/1395148.
    output->append("import '//resources/ash/common/load_time_data.m.js';\n");
#endif
  }

  std::string jstext;
  JSONStringValueSerializer serializer(&jstext);
  serializer.Serialize(json);
  output->append("loadTimeData.data = ");
  output->append(jstext);
  output->append(";");
}

}  // namespace webui
