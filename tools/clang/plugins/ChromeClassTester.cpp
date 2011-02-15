// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A general interface for filtering and only acting on classes in Chromium C++
// code.

#include "ChromeClassTester.h"

using namespace clang;

namespace {

bool starts_with(const std::string& one, const std::string& two) {
  return one.substr(0, two.size()) == two;
}

bool ends_with(const std::string& one, const std::string& two) {
  if (two.size() > one.size())
    return false;

  return one.substr(one.size() - two.size(), two.size()) == two;
}

}  // namespace

ChromeClassTester::ChromeClassTester(CompilerInstance& instance)
    : instance_(instance),
      diagnostic_(instance.getDiagnostics()) {
  banned_namespaces_.push_back("std");
  banned_namespaces_.push_back("__gnu_cxx");

  banned_directories_.push_back("third_party");
  banned_directories_.push_back("native_client");
  banned_directories_.push_back("breakpad");
  banned_directories_.push_back("courgette");
  banned_directories_.push_back("ppapi");
  banned_directories_.push_back("/usr");
  banned_directories_.push_back("testing");
  banned_directories_.push_back("googleurl");
  banned_directories_.push_back("v8");
  banned_directories_.push_back("sdch");

  // You are standing in a mazy of twisty dependencies, all resolved by
  // putting everything in the header.
  banned_directories_.push_back("chrome/test/automation");

  // Used in really low level threading code that probably shouldn't be out of
  // lined.
  ignored_record_names_.push_back("ThreadLocalBoolean");

  // A complicated pickle derived struct that is all packed integers.
  ignored_record_names_.push_back("Header");

  // Part of the GPU system that uses multiple included header
  // weirdness. Never getting this right.
  ignored_record_names_.push_back("Validators");

  // RAII class that's simple enough (media/base/callback.h).
  ignored_record_names_.push_back("AutoTaskRunner");
  ignored_record_names_.push_back("AutoCallbackRunner");

  // Has a UNIT_TEST only constructor. Isn't *terribly* complex...
  ignored_record_names_.push_back("AutocompleteController");
  ignored_record_names_.push_back("HistoryURLProvider");

  // Because of chrome frame
  ignored_record_names_.push_back("ReliabilityTestSuite");

  // Part of our public interface that nacl and friends use. (Arguably, this
  // should mean that this is a higher priority but fixing this looks hard.)
  ignored_record_names_.push_back("PluginVersionInfo");
}

ChromeClassTester::~ChromeClassTester() {}

void ChromeClassTester::HandleTagDeclDefinition(TagDecl* tag) {
  if (CXXRecordDecl* record = dyn_cast<CXXRecordDecl>(tag)) {
    // If this is a POD or a class template or a type dependent on a
    // templated class, assume there's no ctor/dtor/virtual method
    // optimization that we can do.
    if (record->isPOD() ||
        record->getDescribedClassTemplate() ||
        record->getTemplateSpecializationKind() ||
        record->isDependentType())
      return;

    if (InBannedNamespace(record))
      return;

    SourceLocation record_location = record->getInnerLocStart();
    if (InBannedDirectory(record_location))
      return;

    // We sadly need to maintain a blacklist of types that violate these
    // rules, but do so for good reason or due to limitations of this
    // checker (i.e., we don't handle extern templates very well).
    std::string base_name = record->getNameAsString();
    if (IsIgnoredType(base_name))
      return;

    // We ignore all classes that end with "Matcher" because they're probably
    // GMock artifacts.
    if (ends_with(base_name, "Matcher"))
        return;

    CheckChromeClass(record_location, record);
  }
}

void ChromeClassTester::emitWarning(SourceLocation loc, const char* raw_error) {
  FullSourceLoc full(loc, instance().getSourceManager());
  std::string err;
  err = "[chromium-style] ";
  err += raw_error;
  unsigned id = diagnostic().getCustomDiagID(Diagnostic::Warning, err);
  DiagnosticBuilder B = diagnostic().Report(full, id);
}

bool ChromeClassTester::InTestingNamespace(Decl* record) {
  return GetNamespace(record).find("testing") != std::string::npos;
}

bool ChromeClassTester::InBannedNamespace(Decl* record) {
  std::string n = GetNamespace(record);
  if (n != "") {
    return std::find(banned_namespaces_.begin(), banned_namespaces_.end(), n)
        != banned_namespaces_.end();
  }

  return false;
}

std::string ChromeClassTester::GetNamespace(Decl* record) {
  return GetNamespaceImpl(record->getDeclContext(), "");
}

std::string ChromeClassTester::GetNamespaceImpl(const DeclContext* context,
                                                std::string candidate) {
  switch (context->getDeclKind()) {
    case Decl::TranslationUnit: {
      return candidate;
    }
    case Decl::Namespace: {
      const NamespaceDecl* decl = dyn_cast<NamespaceDecl>(context);
      std::string name_str;
      llvm::raw_string_ostream OS(name_str);
      if (decl->isAnonymousNamespace())
        OS << "<anonymous namespace>";
      else
        OS << decl;
      return GetNamespaceImpl(context->getParent(),
                              OS.str());
    }
    default: {
      return GetNamespaceImpl(context->getParent(), candidate);
    }
  }
}

bool ChromeClassTester::InBannedDirectory(const SourceLocation& loc) {
  if (loc.isFileID() && loc.isValid()) {
    bool invalid = false;
    const char* buffer_name = instance_.getSourceManager().getBufferName(
        loc, &invalid);
    if (!invalid && buffer_name) {
      std::string b(buffer_name);

      // Don't complain about these things in implementation files.
      if (ends_with(b, ".cc") || ends_with(b, ".cpp")) {
        return true;
      }

      // Don't complain about autogenerated protobuf files.
      if (ends_with(b, ".pb.h")) {
        return true;
      }

      // Strip out all preceding path garbage. Linux and mac builds have
      // different path garbage, but after doing this, the path should be
      // relative to the root of the source tree. (If we didn't require
      // relative paths, we could have just used realpath().)
      if (!b.empty() && b[0] != '/') {
        size_t i = 0;
        for (; i < b.size() && (b[i] == '.' || b[i] == '/'); ++i) {}
        b = b.substr(i);
      }

      for (std::vector<std::string>::const_iterator it =
               banned_directories_.begin();
           it != banned_directories_.end(); ++it) {
        if (starts_with(b, *it))
          return true;
      }
    }
  }

  return false;
}

bool ChromeClassTester::IsIgnoredType(const std::string& base_name) {
  for (std::vector<std::string>::const_iterator it =
           ignored_record_names_.begin();
       it != ignored_record_names_.end(); ++it) {
    if (base_name == *it)
      return true;
  }

  return false;
}
