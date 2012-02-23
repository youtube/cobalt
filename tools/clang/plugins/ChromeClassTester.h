// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_CHROMECLASSTESTER_H_
#define TOOLS_CLANG_PLUGINS_CHROMECLASSTESTER_H_

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"

#include <set>
#include <vector>

// A class on top of ASTConsumer that forwards classes defined in Chromium
// headers to subclasses which implement CheckChromeClass().
class ChromeClassTester : public clang::ASTConsumer {
 public:
  explicit ChromeClassTester(clang::CompilerInstance& instance);
  virtual ~ChromeClassTester();

  void BuildBannedLists();

  // ASTConsumer:
  virtual void HandleTagDeclDefinition(clang::TagDecl* tag);

 protected:
  clang::CompilerInstance& instance() { return instance_; }
  clang::DiagnosticsEngine& diagnostic() { return diagnostic_; }

  // Emits a simple warning; this shouldn't be used if you require printf-style
  // printing.
  void emitWarning(clang::SourceLocation loc, const char* error);

  // Utility method for subclasses to check if testing details are in this
  // class. Some tests won't care if a class has a ::testing member and others
  // will.
  bool InTestingNamespace(const clang::Decl* record);

  // Utility method for subclasses to check if this class is in a banned
  // namespace.
  bool InBannedNamespace(const clang::Decl* record);

 private:
  // Filtered versions of tags that are only called with things defined in
  // chrome header files.
  virtual void CheckChromeClass(const clang::SourceLocation& record_location,
                                clang::CXXRecordDecl* record) = 0;

  // Utility methods used for filtering out non-chrome classes (and ones we
  // deliberately ignore) in HandleTagDeclDefinition().
  std::string GetNamespace(const clang::Decl* record);
  std::string GetNamespaceImpl(const clang::DeclContext* context,
                               std::string candidate);
  bool InBannedDirectory(clang::SourceLocation loc);
  bool IsIgnoredType(const std::string& base_name);

  clang::CompilerInstance& instance_;
  clang::DiagnosticsEngine& diagnostic_;

  // List of banned namespaces.
  std::vector<std::string> banned_namespaces_;

  // List of banned directories.
  std::vector<std::string> banned_directories_;

  // List of types that we don't check.
  std::set<std::string> ignored_record_names_;
};

#endif  // TOOLS_CLANG_PLUGINS_CHROMECLASSTESTER_H_
