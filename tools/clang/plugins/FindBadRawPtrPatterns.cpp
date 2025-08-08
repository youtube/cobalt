// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "FindBadRawPtrPatterns.h"
#include <memory>

#include "RawPtrHelpers.h"
#include "RawPtrManualPathsToIgnore.h"
#include "SeparateRepositoryPaths.h"
#include "StackAllocatedChecker.h"
#include "TypePredicateUtil.h"
#include "Util.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/TypeLoc.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace chrome_checker {

constexpr char kBadCastDiagnostic[] =
    "[chromium-style] casting '%0' to '%1 is not allowed.";
constexpr char kBadCastDiagnosticNoteExplanation[] =
    "[chromium-style] '%0' manages BackupRefPtr refcounts; bypassing its C++ "
    "interface or treating it as a POD will lead to memory safety errors.";
constexpr char kBadCastDiagnosticNoteType[] =
    "[chromium-style] '%0' manages BackupRefPtr or its container here.";

class BadCastMatcher : public MatchFinder::MatchCallback {
 public:
  explicit BadCastMatcher(clang::CompilerInstance& compiler,
                          const FilterFile& exclude_files,
                          const FilterFile& exclude_functions)
      : compiler_(compiler),
        exclude_files_(exclude_files),
        exclude_functions_(exclude_functions) {
    error_bad_cast_signature_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kBadCastDiagnostic);
    note_bad_cast_signature_explanation_ =
        compiler_.getDiagnostics().getCustomDiagID(
            clang::DiagnosticsEngine::Note, kBadCastDiagnosticNoteExplanation);
    note_bad_cast_signature_type_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Note, kBadCastDiagnosticNoteType);
  }

  void Register(MatchFinder& match_finder) {
    auto cast_matcher = BadRawPtrCastExpr(casting_unsafe_predicate_,
                                          exclude_files_, exclude_functions_);
    match_finder.addMatcher(cast_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const clang::CastExpr* cast_expr =
        result.Nodes.getNodeAs<clang::CastExpr>("castExpr");
    assert(cast_expr && "matcher should bind 'castExpr'");

    const clang::SourceManager& source_manager = *result.SourceManager;
    clang::SourceLocation loc = cast_expr->getSourceRange().getBegin();
    std::string file_path = GetFilename(source_manager, loc);

    clang::PrintingPolicy printing_policy(result.Context->getLangOpts());
    const std::string src_name =
        cast_expr->getSubExpr()->getType().getAsString(printing_policy);
    const std::string dst_name =
        cast_expr->getType().getAsString(printing_policy);

    const auto* src_type = result.Nodes.getNodeAs<clang::Type>("srcType");
    const auto* dst_type = result.Nodes.getNodeAs<clang::Type>("dstType");
    assert((src_type || dst_type) &&
           "matcher should bind 'srcType' or 'dstType'");

    const auto* enclosing_cast_expr =
        result.Nodes.getNodeAs<clang::ExplicitCastExpr>("enclosingCastExpr");
    const auto* cast_expr_for_display =
        enclosing_cast_expr ? enclosing_cast_expr : cast_expr;

    compiler_.getDiagnostics().Report(cast_expr_for_display->getEndLoc(),
                                      error_bad_cast_signature_)
        << src_name << dst_name;

    std::shared_ptr<MatchResult> type_note;
    if (src_type != nullptr) {
      compiler_.getDiagnostics().Report(cast_expr_for_display->getEndLoc(),
                                        note_bad_cast_signature_explanation_)
          << src_name;
      type_note = casting_unsafe_predicate_.GetMatchResult(src_type);
    } else {
      compiler_.getDiagnostics().Report(cast_expr_for_display->getEndLoc(),
                                        note_bad_cast_signature_explanation_)
          << dst_name;
      type_note = casting_unsafe_predicate_.GetMatchResult(dst_type);
    }

    while (type_note) {
      if (type_note->source_loc()) {
        const auto& type_name = clang::QualType::getAsString(
            type_note->type(), {}, printing_policy);
        compiler_.getDiagnostics().Report(*type_note->source_loc(),
                                          note_bad_cast_signature_type_)
            << type_name;
      }
      type_note = type_note->source();
    }
  }

 private:
  clang::CompilerInstance& compiler_;
  const FilterFile& exclude_files_;
  const FilterFile& exclude_functions_;
  CastingUnsafePredicate casting_unsafe_predicate_;
  unsigned error_bad_cast_signature_;
  unsigned note_bad_cast_signature_explanation_;
  unsigned note_bad_cast_signature_type_;
};

const char kNeedRawPtrSignature[] =
    "[chromium-rawptr] Use raw_ptr<T> instead of a raw pointer.";

class RawPtrFieldMatcher : public MatchFinder::MatchCallback {
 public:
  explicit RawPtrFieldMatcher(
      clang::CompilerInstance& compiler,
      const RawPtrAndRefExclusionsOptions& exclusion_options)
      : compiler_(compiler), exclusion_options_(exclusion_options) {
    error_need_raw_ptr_signature_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kNeedRawPtrSignature);
  }

  void Register(MatchFinder& match_finder) {
    auto field_decl_matcher = AffectedRawPtrFieldDecl(exclusion_options_);
    match_finder.addMatcher(field_decl_matcher, this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const clang::FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("affectedFieldDecl");
    assert(field_decl && "matcher should bind 'fieldDecl'");

    const clang::TypeSourceInfo* type_source_info =
        field_decl->getTypeSourceInfo();
    assert(type_source_info && "assuming |type_source_info| is always present");

    assert(type_source_info->getType()->isPointerType() &&
           "matcher should only match pointer types");

    compiler_.getDiagnostics().Report(field_decl->getLocation(),
                                      error_need_raw_ptr_signature_);
  }

 private:
  clang::CompilerInstance& compiler_;
  unsigned error_need_raw_ptr_signature_;
  const RawPtrAndRefExclusionsOptions& exclusion_options_;
};

const char kNeedRawRefSignature[] =
    "[chromium-rawref] Use raw_ref<T> instead of a native reference.";

class RawRefFieldMatcher : public MatchFinder::MatchCallback {
 public:
  explicit RawRefFieldMatcher(
      clang::CompilerInstance& compiler,
      const RawPtrAndRefExclusionsOptions& exclusion_options)
      : compiler_(compiler), exclusion_options_(exclusion_options) {
    error_need_raw_ref_signature_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kNeedRawRefSignature);
  }

  void Register(MatchFinder& match_finder) {
    auto field_decl_matcher = AffectedRawRefFieldDecl(exclusion_options_);
    match_finder.addMatcher(field_decl_matcher, this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const clang::FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("affectedFieldDecl");
    assert(field_decl && "matcher should bind 'fieldDecl'");

    const clang::TypeSourceInfo* type_source_info =
        field_decl->getTypeSourceInfo();
    assert(type_source_info && "assuming |type_source_info| is always present");

    assert(type_source_info->getType()->isReferenceType() &&
           "matcher should only match reference types");

    compiler_.getDiagnostics().Report(field_decl->getEndLoc(),
                                      error_need_raw_ref_signature_);
  }

 private:
  clang::CompilerInstance& compiler_;
  unsigned error_need_raw_ref_signature_;
  const RawPtrAndRefExclusionsOptions exclusion_options_;
};

const char kNoRawPtrToStackAllocatedSignature[] =
    "[chromium-raw-ptr-to-stack-allocated] Do not use '%0<T>' on a "
    "`STACK_ALLOCATED` object '%1'.";

class RawPtrToStackAllocatedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit RawPtrToStackAllocatedMatcher(clang::CompilerInstance& compiler)
      : compiler_(compiler), stack_allocated_predicate_() {
    error_no_raw_ptr_to_stack_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kNoRawPtrToStackAllocatedSignature);
  }

  void Register(MatchFinder& match_finder) {
    auto value_decl_matcher =
        RawPtrToStackAllocatedTypeLoc(&stack_allocated_predicate_);
    match_finder.addMatcher(value_decl_matcher, this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const auto* pointer =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("pointerRecordDecl");
    assert(pointer && "matcher should bind 'pointerRecordDecl'");

    const auto* pointee =
        result.Nodes.getNodeAs<clang::QualType>("pointeeQualType");
    assert(pointee && "matcher should bind 'pointeeQualType'");
    clang::PrintingPolicy printing_policy(result.Context->getLangOpts());
    const std::string pointee_name = pointee->getAsString(printing_policy);

    const auto* type_loc =
        result.Nodes.getNodeAs<clang::TypeLoc>("stackAllocatedRawPtrTypeLoc");
    assert(type_loc && "matcher should bind 'stackAllocatedRawPtrTypeLoc'");

    compiler_.getDiagnostics().Report(type_loc->getEndLoc(),
                                      error_no_raw_ptr_to_stack_)
        << pointer->getNameAsString() << pointee_name;
  }

 private:
  clang::CompilerInstance& compiler_;
  StackAllocatedPredicate stack_allocated_predicate_;
  unsigned error_no_raw_ptr_to_stack_;
};

void FindBadRawPtrPatterns(Options options,
                           clang::ASTContext& ast_context,
                           clang::CompilerInstance& compiler) {
  MatchFinder match_finder;

  std::vector<std::string> paths_to_exclude_lines;
  std::vector<std::string> check_bad_raw_ptr_cast_exclude_paths;
  for (auto* const line : kRawPtrManualPathsToIgnore) {
    paths_to_exclude_lines.push_back(line);
  }
  for (auto* const line : kSeparateRepositoryPaths) {
    paths_to_exclude_lines.push_back(line);
    check_bad_raw_ptr_cast_exclude_paths.push_back(line);
  }
  paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                options.raw_ptr_paths_to_exclude_lines.begin(),
                                options.raw_ptr_paths_to_exclude_lines.end());
  check_bad_raw_ptr_cast_exclude_paths.insert(
      check_bad_raw_ptr_cast_exclude_paths.end(),
      options.check_bad_raw_ptr_cast_exclude_paths.begin(),
      options.check_bad_raw_ptr_cast_exclude_paths.end());

  FilterFile exclude_fields(options.exclude_fields_file, "exclude-fields");
  FilterFile exclude_lines(paths_to_exclude_lines);

  StackAllocatedPredicate stack_allocated_predicate;
  RawPtrAndRefExclusionsOptions exclusion_options{
      &exclude_fields, &exclude_lines, options.check_raw_ptr_to_stack_allocated,
      &stack_allocated_predicate};

  FilterFile filter_check_bad_raw_ptr_cast_exclude_paths(
      check_bad_raw_ptr_cast_exclude_paths);
  FilterFile filter_check_bad_raw_ptr_cast_exclude_funcs(
      options.check_bad_raw_ptr_cast_exclude_funcs);
  BadCastMatcher bad_cast_matcher(compiler,
                                  filter_check_bad_raw_ptr_cast_exclude_paths,
                                  filter_check_bad_raw_ptr_cast_exclude_funcs);
  if (options.check_bad_raw_ptr_cast) {
    bad_cast_matcher.Register(match_finder);
  }

  RawPtrFieldMatcher field_matcher(compiler, exclusion_options);
  if (options.check_raw_ptr_fields) {
    field_matcher.Register(match_finder);
  }

  RawRefFieldMatcher ref_field_matcher(compiler, exclusion_options);
  if (options.check_raw_ref_fields) {
    ref_field_matcher.Register(match_finder);
  }

  RawPtrToStackAllocatedMatcher raw_ptr_to_stack(compiler);
  if (options.check_raw_ptr_to_stack_allocated) {
    raw_ptr_to_stack.Register(match_finder);
  }

  match_finder.matchAST(ast_context);
}

}  // namespace chrome_checker
