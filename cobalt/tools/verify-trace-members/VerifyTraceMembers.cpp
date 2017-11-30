// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Note: This file is here for reference only, and is *NOT* part of Cobalt's
// build system.  See adjacent README.md for build instructions.

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang;

// My"Cool"String -> My\"Cool\"String
std::string EscapeJson(const std::string& string) {
  std::ostringstream o;
  for (auto c : string) {
    switch (c) {
      case '"':
        o << "\\\"";
        break;
      case '\\':
        o << "\\\\";
        break;
      case '\b':
        o << "\\b";
        break;
      case '\f':
        o << "\\f";
        break;
      case '\n':
        o << "\\n";
        break;
      case '\r':
        o << "\\r";
        break;
      case '\t':
        o << "\\t";
        break;
      default:
        if ('\x00' <= c && c <= '\x1f') {
          o << "\\u" << std::hex << std::setw(4) << std::setfill('0')
            << static_cast<int>(c);
        } else {
          o << c;
        }
    }
  }
  return o.str();
}

enum JsonConversionType {
  kNeedsEscape,
  kAlreadyJson,
};

// Helper struct to make call sites of |ToJson| nice and fresh.
struct KeyValue {
  KeyValue(const std::string& key, const std::string& value,
           JsonConversionType conversion_type = kNeedsEscape) {
    this->key = '"' + EscapeJson(key) + '"';
    if (conversion_type == kNeedsEscape) {
      this->value = '"' + EscapeJson(value) + '"';
    } else if (conversion_type == kAlreadyJson) {
      this->value = value;
    } else {
      assert(false);
    }
  }
  KeyValue(const std::string& key, int value) {
    this->key = '"' + EscapeJson(key) + '"';
    this->value = std::to_string(value);
  }
  KeyValue(const std::string& key, unsigned value) {
    this->key = '"' + EscapeJson(key) + '"';
    this->value = std::to_string(value);
  }

  std::string key;
  std::string value;
};

// {{"foo", "bar"}, {"baz", 5}} -> {"foo": "bar", "baz": 5}
std::string ToJson(std::vector<KeyValue> key_values) {
  std::string result = "{";
  bool needs_comma = false;
  for (auto& key_value : key_values) {
    if (needs_comma) result += ", ";
    needs_comma = true;
    result += key_value.key + ": " + key_value.value;
  }
  result += "}";
  return result;
}

// Get an unsugared and unqualified string representation of
// |qual_type|.
std::string ToString(QualType qual_type) {
  const Type* type = qual_type.getTypePtrOrNull();
  if (!type) {
    return "<INTERNALLY NULL>";
  }
  const Type* unqualified_desugared_type = type->getUnqualifiedDesugaredType();
  CanQualType can_qual_type =
      unqualified_desugared_type->getCanonicalTypeUnqualified();
  return static_cast<QualType>(can_qual_type).getAsString();
}

// Return whether |base_decl| is a base class of |derived|.
bool IsBaseOf(CXXRecordDecl* base_decl, QualType derived) {
  const Type* derived_type = derived.getTypePtrOrNull();
  CXXRecordDecl* derived_decl = derived_type->getAsCXXRecordDecl();
  if (!derived_decl) {
    return false;
  }
  if (base_decl == derived_decl) {
    return true;
  }
  for (auto& base : derived_decl->bases()) {
    if (IsBaseOf(base_decl, base.getType())) {
      return true;
    }
  }
  return false;
}

// We need to keep track of types we've already visisted due to type cycles in
// templates. Consider, for example, "class Foo : public MyCrtp<Foo> {};".
using Visited = std::unordered_set<const Type*>;

// Utility functions to check if a type is roughly Traceable.
// "Is roughly Traceable" is defined as follows:
//   1. Traceable is roughly Traceable.
//   2. A type is roughly Traceable if any of its parent classes are roughly
//   Traceable.
//   3. A type is roughly Traceable if any of its template arguments are roughly
//   Traceable.
//   4. T* is roughly Traceable if T is roughly Traceable.
//
// Note that this definition is intentionally liberal in what it considers
// Traceable.  False positives (such as the T* field in scoped_refptr) are
// expected to be handled by a white list that is implemented in a wrapper
// script (in order to facilitate modification of the white list without
// recompilation).
bool IsTraceable(QualType qual_type, Visited& visited);
bool IsTraceable(const RecordDecl* record_decl, Visited& visited);
bool IsTraceable(QualType qual_type) {
  Visited visited;
  return IsTraceable(qual_type, visited);
}

bool IsTraceable(QualType qual_type, Visited& visited) {
  const Type* type = qual_type.getTypePtrOrNull();
  if (!type) {
    return false;
  }
  if (visited.count(type) == 1) {
    return false;
  }
  visited.insert(type);

  // 1. Traceable is roughly Traceable.
  const Type* unqualified_desugared_type = type->getUnqualifiedDesugaredType();
  CanQualType can_qual_type =
      unqualified_desugared_type->getCanonicalTypeUnqualified();
  if (static_cast<QualType>(can_qual_type).getAsString() ==
      "class cobalt::script::Traceable") {
    return true;
  }

  // 4. T* is roughly Traceable if T is roughly Traceable.
  if (type->isPointerType()) {
    if (IsTraceable(type->getPointeeType(), visited)) {
      return true;
    }
  }

  // 3. A type is roughly Traceable if any of its template arguments are
  // roughly Traceable.
  if (auto* tst = qual_type.getNonReferenceType()
                      ->getAs<TemplateSpecializationType>()) {
    for (auto& arg : *tst) {
      if (arg.getKind() == TemplateArgument::Type) {
        if (IsTraceable(arg.getAsType(), visited)) {
          return true;
        }
      }
    }
  }

  return IsTraceable(type->getAsCXXRecordDecl(), visited);
}

bool IsTraceable(const RecordDecl* record_decl,
                 std::unordered_set<const Type*>& visited) {
  if (record_decl == nullptr) {
    return false;
  }
  const CXXRecordDecl* cxx_record_decl =
      dyn_cast<const CXXRecordDecl>(record_decl);
  if (!cxx_record_decl || !cxx_record_decl->hasDefinition()) {
    return false;
  }
  // 2. A type is roughly Traceable if any of its parent classes are roughly
  // Traceable.
  for (auto& base : cxx_record_decl->bases()) {
    if (IsTraceable(base.getType(), visited)) {
      return true;
    }
  }
  return false;
}

// Search |stmt| (and its children) for a reference to a member
// expression named |target|.
// clang-format off
// Example tree:
// tracer->Trace(wrappable_);
//   |-CXXMemberCallExpr 0x98b2b80 'void'
//   | |-MemberExpr 0x98b2af8 '<bound member function type>' ->Trace 0x5d490b0
//   | | `-ImplicitCastExpr 0x98b2ae0 'script::Tracer *' <LValueToRValue>
//   | |   `-DeclRefExpr 0x98b2ab8 'script::Tracer *' lvalue ParmVar 0x98b27e0 'tracer' 'script::Tracer *'
//   | `-ImplicitCastExpr 0x98b2c78 'class cobalt::script::Traceable *' <DerivedToBase (Traceable)>
//   |   `-ImplicitCastExpr 0x98b2c60 'class cobalt::dom::DOMImplementation *' <UserDefinedConversion>
//   |     `-CXXMemberCallExpr 0x98b2c38 'class cobalt::dom::DOMImplementation *'
//   |       `-MemberExpr 0x98b2c00 '<bound member function type>' .operator cobalt::dom::DOMImplementation * 0x90f1ad8
//   |         `-ImplicitCastExpr 0x98b2be8 'const class scoped_refptr<class cobalt::dom::DOMImplementation>' lvalue <NoOp>
//   |           `-MemberExpr 0x98b2b48 'scoped_refptr<class cobalt::dom::DOMImplementation>':'class scoped_refptr<class cobalt::dom::DOMImplementation>' lvalue ->implementation_ 0x90f2be8
//   |             `-CXXThisExpr 0x98b2b30 'class cobalt::dom::Document *' this
// clang-format on
bool SearchForMemberExpr(Stmt* stmt, const std::string& target) {
  if (MemberExpr* member_expr = dyn_cast<MemberExpr>(stmt)) {
    if (member_expr->getMemberDecl()->getNameAsString() == target) {
      return true;
    }
  }
  for (auto& child : stmt->children()) {
    if (SearchForMemberExpr(child, target)) {
      return true;
    }
  }
  return false;
}

// Attempt to get the |n|th child of |body| as a |T|.  Returns nullptr if
// there are not enough children, or the child was not |dyn_cast|able to |T|.
template <typename T>
T* MaybeGetNthChildAs(Stmt* body, int n) {
  auto children = body->children();
  if (std::distance(children.begin(), children.end()) < n) {
    return nullptr;
  }
  auto it = children.begin();
  std::advance(it, n);
  return dyn_cast<T>(*it);
}

// Check if |stmt| matches something like:
// clang-format off
// (this example is from inside void Document::TraceMembers(script::Tracer* tracer))
//   Node::TraceMembers(tracer);
// Example tree:
//   |-CXXMemberCallExpr 0x98b2a50 'void'
//   | |-MemberExpr 0x98b29d8 '<bound member function type>' ->TraceMembers 0x7b979a0
//   | | `-ImplicitCastExpr 0x98b2a80 'class cobalt::dom::Node *' <UncheckedDerivedToBase (Node)>
//   | |   `-CXXThisExpr 0x98b29c0 'class cobalt::dom::Document *' this
//   | `-ImplicitCastExpr 0x98b2aa0 'script::Tracer *' <LValueToRValue>
//   |   `-DeclRefExpr 0x98b2a28 'script::Tracer *' lvalue ParmVar 0x98b27e0 'tracer' 'script::Tracer *'
// clang-format on
bool IsBaseClassTracerCall(Stmt* stmt) {
  CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<CXXMemberCallExpr>(stmt);
  if (!cxx_member_call_expr) {
    return false;
  }

  {
    MemberExpr* member_expr =
        MaybeGetNthChildAs<MemberExpr>(cxx_member_call_expr, 0);
    if (!member_expr) {
      return false;
    }
    {
      ImplicitCastExpr* implicit_cast_expr =
          MaybeGetNthChildAs<ImplicitCastExpr>(member_expr, 0);
      if (!implicit_cast_expr) {
        return false;
      }
      {
        CXXThisExpr* cxx_this_expr =
            MaybeGetNthChildAs<CXXThisExpr>(implicit_cast_expr, 0);
        if (!cxx_this_expr) {
          return false;
        }

        auto GetPointeeType = [](QualType qual_type) -> const Type* {
          const Type* type = qual_type.getTypePtrOrNull();
          if (!type) {
            return nullptr;
          }

          if (!type->isPointerType()) {
            return nullptr;
          }

          QualType pointee_qual_type = type->getPointeeType();
          return pointee_qual_type.getTypePtrOrNull();
        };

        // Ensure that we are calling our base's TraceMembers.
        const Type* this_type = GetPointeeType(cxx_this_expr->getType());
        const Type* cast_type = GetPointeeType(implicit_cast_expr->getType());
        if (!this_type || !cast_type) {
          return false;
        }

        CXXRecordDecl* cast_decl = cast_type->getAsCXXRecordDecl();
        CXXRecordDecl* this_decl = cast_type->getAsCXXRecordDecl();

        if (!cast_decl || !this_decl) {
          return false;
        }

        if (!IsBaseOf(cast_decl, cxx_this_expr->getType())) {
          return false;
        }
      }
    }
  }

  {
    ImplicitCastExpr* implicit_cast_expr =
        MaybeGetNthChildAs<ImplicitCastExpr>(cxx_member_call_expr, 1);
    if (!implicit_cast_expr) {
      return false;
    }
    // TODO: Check if this is of type script::Tracer*
  }

  return true;
}

class FieldPrinter : public MatchFinder::MatchCallback {
 public:
  // Iterate over each field declaration (FieldDecl) of each class.  Check
  // that the owning class's |TraceMembers| traces it.
  void run(const MatchFinder::MatchResult& result) override {
    const FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("fieldDecl");
    if (!field_decl) {
      return;
    }

    QualType qual_type = field_decl->getType();
    if (!IsTraceable(qual_type)) {
      return;
    }

    const RecordDecl* parent = field_decl->getParent();
    const CXXRecordDecl* cxx_parent = dyn_cast<const CXXRecordDecl>(parent);
    if (!cxx_parent) {
      return;
    }

    bool is_traced = false;
    bool had_trace_members_declaration = false;
    bool had_trace_members_body = false;
    bool calls_base_trace_members = false;

    auto methods = cxx_parent->methods();
    auto trace_members_it =
        std::find_if(methods.begin(), methods.end(), [](CXXMethodDecl* method) {
          return method->getNameInfo().getName().getAsString() ==
                 "TraceMembers";
        });

    if (trace_members_it != methods.end()) {
      auto method = *trace_members_it;
      had_trace_members_declaration = true;
      Stmt* body = method->getBody();
      if (body) {
        had_trace_members_body = true;
        bool first_statement = true;
        for (Stmt* stmt : body->children()) {
          if (first_statement) {
            first_statement = false;
            calls_base_trace_members |= IsBaseClassTracerCall(stmt);
          }

          if (CXXMemberCallExpr* call = dyn_cast<CXXMemberCallExpr>(stmt)) {
            // TODO: Check that this is actually a tracer call
            int nargs = call->getNumArgs();
            if (nargs == 0) {
              continue;
            }
            Expr* arg = call->getArg(0);
            is_traced |=
                SearchForMemberExpr(arg, field_decl->getNameAsString());
          }
        }
      }
    }

    std::string field_name = field_decl->getNameAsString();
    std::string field_class = ToString(qual_type);
    std::string field_class_friendly = qual_type.getAsString();
    std::string parent_class =
        cxx_parent->getCanonicalDecl()->getQualifiedNameAsString();
    std::string parent_class_friendly = cxx_parent->getNameAsString();
    // TODO: Maybe get some line/column numbers and a filename too?

    if (!had_trace_members_declaration) {
      std::cout << ToJson({
                       {"messageType", "needsTraceMembersDeclaration"},
                       {"fieldName", field_name},
                       {"fieldClass", field_class},
                       {"fieldClassFriendly", field_class_friendly},
                       {"parentClass", parent_class},
                       {"parentClassFriendly", parent_class_friendly},
                   })
                << '\n';
    } else {
      if (had_trace_members_body) {
        if (!is_traced) {
          std::cout << ToJson({
                           {"messageType", "needsTracerTraceField"},
                           {"fieldName", field_name},
                           {"fieldClass", field_class},
                           {"fieldClassFriendly", field_class_friendly},
                           {"parentClass", parent_class},
                           {"parentClassFriendly", parent_class_friendly},
                       })
                    << '\n';
        }
        // Note that for a class that fails to call its base class's
        // |TraceMembers|, this message will be printed for each field
        // declaration it has.  We expect the script wrapping us to de-dupe
        // these messages.
        if (!calls_base_trace_members) {
          // We need to dump the base classes so our wrapper script can filter
          // out classes with direct bases of script::Wrappable and
          // script::Traceable.
          std::string bases = "[";
          bool first = true;
          for (auto base : cxx_parent->bases()) {
            if (!first) bases += ", ";
            first = false;
            bases += '"' + EscapeJson(ToString(base.getType())) + '"';
          }
          bases += "]";
          std::cout << ToJson({
                           {"messageType", "needsCallBaseTraceMembers"},
                           {"parentClass", parent_class},
                           {"parentClassFriendly", parent_class_friendly},
                           {"baseNames", bases, kAlreadyJson},
                       })
                    << '\n';
        }
      }
      // Don't handle the case where there isn't a body.  We will find it
      // later in another translation unit, or else the code would not be
      // able to link successfully.
    }
  }
};

int main(int argc, const char** argv) {
  llvm::cl::OptionCategory my_tool_category("verify-trace-members options");
  llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);
  llvm::cl::extrahelp more_help("\nMore help...");

  CommonOptionsParser options_parser(argc, argv, my_tool_category);
  ClangTool tool(options_parser.getCompilations(),
                 options_parser.getSourcePathList());

  FieldPrinter field_printer;
  MatchFinder match_finder;
  DeclarationMatcher field_matcher = fieldDecl().bind("fieldDecl");
  match_finder.addMatcher(field_matcher, &field_printer);

  return tool.run(newFrontendActionFactory(&match_finder).get());
}
