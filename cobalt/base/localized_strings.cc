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

#include "cobalt/base/localized_strings.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace base {

namespace {
// Does a DFS over the tree starting at |node| and ending at the first element
// whose name is |name|.
xmlNode* FindXmlNode(xmlNode* node, const char* name) {
  xmlNode* child = node->children;
  while (child) {
    if (child->type == XML_ELEMENT_NODE) {
      if (strcmp((const char*)child->name, name) == 0) {
        return child;
      }
      xmlNode* descendant = FindXmlNode(child, name);
      if (descendant) {
        return descendant;
      }
    }
    child = child->next;
  }
  return NULL;
}
}  // namespace

LocalizedStrings* LocalizedStrings::GetInstance() {
  return Singleton<LocalizedStrings>::get();
}

LocalizedStrings::LocalizedStrings() {
  // Initialize to US English on creation. A subsequent call to Initialize
  // will overwrite all available strings in the specified language.
  LoadStrings("en-US");
}

LocalizedStrings::~LocalizedStrings() {}

void LocalizedStrings::Initialize(const std::string& language) {
  LoadStrings(language);
}

std::string LocalizedStrings::GetString(const std::string& id,
                                        const std::string& fallback) {
  StringContainer::iterator iter = strings_.find(id);
  if (iter == strings_.end()) {
    return fallback;
  }
  return iter->second;
}

void LocalizedStrings::LoadStrings(const std::string& language) {
  // Construct the XLB filename.
  FilePath xlb_path;
  PathService::Get(base::DIR_EXE, &xlb_path);
  xlb_path = xlb_path.Append("i18n").Append(language).AddExtension("xlb");

  // Try to open the XLB file.
  std::string content;
  if (!file_util::ReadFileToString(xlb_path, &content)) {
    DLOG(WARNING) << "Cannot open XLB file: " << xlb_path.value();

    // Fall back to a generic version of the same language.
    size_t dash = language.find('-');
    if (dash != std::string::npos) {
      // Chop off the country part of the language string and try again.
      std::string generic_lang(language.c_str(), dash);
      LoadStrings(generic_lang);
    }
    return;
  }

  // Parse the XML document.
  xmlDoc* doc = xmlParseDoc(reinterpret_cast<const xmlChar*>(content.c_str()));
  if (!doc) {
    DLOG(WARNING) << "Cannot parse XLB file: " << xlb_path.value();
    return;
  }

  // Find the first <msg> node by DFS.
  xmlNode* msg = FindXmlNode(xmlDocGetRootElement(doc), "msg");
  // Iterate through the sibling <msg> elements.
  while (msg) {
    if (!strcmp((const char*)msg->name, "msg")) {
      // Add the data from this element to the strings list.
      char* name =
          reinterpret_cast<char*>(xmlGetProp(msg, (const xmlChar*)"name"));
      char* value =
          reinterpret_cast<char*>(xmlNodeListGetString(doc, msg->children, 1));
      strings_[name] = value;
      xmlFree(name);
      xmlFree(value);
    }
    msg = msg->next;
  }

  xmlFreeDoc(doc);
}

}  // namespace base
