// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a bunch of recurring problems in the Chromium C++ code.
//
// Checks that are implemented:
// - Constructors/Destructors should not be inlined if they are of a complex
//   class type.
// - Missing "virtual" keywords on methods that should be virtual.
// - Non-annotated overriding virtual methods.
// - Virtual methods with nonempty implementations in their headers.
//
// Things that are still TODO:
// - Deriving from base::RefCounted and friends should mandate non-public
//   destructors.

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

#include "ChromeClassTester.h"

using namespace clang;

namespace {

bool TypeHasNonTrivialDtor(const Type* type) {
  if (const CXXRecordDecl* cxx_r = type->getCXXRecordDeclForPointerType())
    return cxx_r->hasTrivialDestructor();

  return false;
}

// Searches for constructs that we know we don't want in the Chromium code base.
class FindBadConstructsConsumer : public ChromeClassTester {
 public:
  FindBadConstructsConsumer(CompilerInstance& instance,
                            bool skip_override)
      : ChromeClassTester(instance),
        skip_override_(skip_override) {}

  virtual void CheckChromeClass(const SourceLocation& record_location,
                                CXXRecordDecl* record) {
    CheckCtorDtorWeight(record_location, record);
    CheckVirtualMethods(record_location, record);
  }

  // Prints errors if the constructor/destructor weight is too heavy.
  void CheckCtorDtorWeight(const SourceLocation& record_location,
                           CXXRecordDecl* record) {
    // We don't handle anonymous structs. If this record doesn't have a
    // name, it's of the form:
    //
    // struct {
    //   ...
    // } name_;
    if (record->getIdentifier() == NULL)
      return;

    // Count the number of templated base classes as a feature of whether the
    // destructor can be inlined.
    int templated_base_classes = 0;
    for (CXXRecordDecl::base_class_const_iterator it = record->bases_begin();
         it != record->bases_end(); ++it) {
      if (it->getTypeSourceInfo()->getTypeLoc().getTypeLocClass() ==
          TypeLoc::TemplateSpecialization) {
        ++templated_base_classes;
      }
    }

    // Count the number of trivial and non-trivial member variables.
    int trivial_member = 0;
    int non_trivial_member = 0;
    int templated_non_trivial_member = 0;
    for (RecordDecl::field_iterator it = record->field_begin();
         it != record->field_end(); ++it) {
      CountType(it->getType().getTypePtr(),
                &trivial_member,
                &non_trivial_member,
                &templated_non_trivial_member);
    }

    // Check to see if we need to ban inlined/synthesized constructors. Note
    // that the cutoffs here are kind of arbitrary. Scores over 10 break.
    int dtor_score = 0;
    // Deriving from a templated base class shouldn't be enough to trigger
    // the ctor warning, but if you do *anything* else, it should.
    //
    // TODO(erg): This is motivated by templated base classes that don't have
    // any data members. Somehow detect when templated base classes have data
    // members and treat them differently.
    dtor_score += templated_base_classes * 9;
    // Instantiating a template is an insta-hit.
    dtor_score += templated_non_trivial_member * 10;
    // The fourth normal class member should trigger the warning.
    dtor_score += non_trivial_member * 3;

    int ctor_score = dtor_score;
    // You should be able to have 9 ints before we warn you.
    ctor_score += trivial_member;

    if (ctor_score >= 10) {
      if (!record->hasUserDeclaredConstructor()) {
        emitWarning(record_location,
                    "Complex class/struct needs an explicit out-of-line "
                    "constructor.");
      } else {
        // Iterate across all the constructors in this file and yell if we
        // find one that tries to be inline.
        for (CXXRecordDecl::ctor_iterator it = record->ctor_begin();
             it != record->ctor_end(); ++it) {
          if (it->hasInlineBody()) {
            emitWarning(it->getInnerLocStart(),
                        "Complex constructor has an inlined body.");
          }
        }
      }
    }

    // The destructor side is equivalent except that we don't check for
    // trivial members; 20 ints don't need a destructor.
    if (dtor_score >= 10 && !record->hasTrivialDestructor()) {
      if (!record->hasUserDeclaredDestructor()) {
        emitWarning(
            record_location,
            "Complex class/struct needs an explicit out-of-line "
            "destructor.");
      } else if (CXXDestructorDecl* dtor = record->getDestructor()) {
        if (dtor->hasInlineBody()) {
          emitWarning(dtor->getInnerLocStart(),
                      "Complex destructor has an inline body.");
        }
      }
    }
  }

  void CheckVirtualMethod(const CXXMethodDecl* method) {
    if (!method->isVirtual())
      return;

    if (!method->isVirtualAsWritten()) {
      SourceLocation loc = method->getTypeSpecStartLoc();
      if (isa<CXXDestructorDecl>(method))
        loc = method->getInnerLocStart();
      emitWarning(loc, "Overriding method must have \"virtual\" keyword.");
    }

    // Virtual methods should not have inline definitions beyond "{}".
    if (method->hasBody() && method->hasInlineBody()) {
      if (CompoundStmt* cs = dyn_cast<CompoundStmt>(method->getBody())) {
        if (cs->size()) {
          emitWarning(
              cs->getLBracLoc(),
              "virtual methods with non-empty bodies shouldn't be "
              "declared inline.");
        }
      }
    }
  }

  bool IsMethodInBannedNamespace(const CXXMethodDecl* method) {
    if (InBannedNamespace(method))
      return true;
    for (CXXMethodDecl::method_iterator i = method->begin_overridden_methods();
         i != method->end_overridden_methods();
         ++i) {
      const CXXMethodDecl* overridden = *i;
      if (IsMethodInBannedNamespace(overridden))
        return true;
    }

    return false;
  }

  void CheckOverriddenMethod(const CXXMethodDecl* method) {
    if (skip_override_)
      return;

    if (!method->size_overridden_methods() || method->getAttr<OverrideAttr>())
      return;

    if (isa<CXXDestructorDecl>(method) || method->isPure())
      return;

    if (IsMethodInBannedNamespace(method))
      return;

    SourceLocation loc = method->getTypeSpecStartLoc();
    emitWarning(loc, "Overriding method must be marked with OVERRIDE.");
  }

  // Makes sure there is a "virtual" keyword on virtual methods and that there
  // are no inline function bodies on them (but "{}" is allowed).
  //
  // Gmock objects trigger these for each MOCK_BLAH() macro used. So we have a
  // trick to get around that. If a class has member variables whose types are
  // in the "testing" namespace (which is how gmock works behind the scenes),
  // there's a really high chance we won't care about these errors
  void CheckVirtualMethods(const SourceLocation& record_location,
                           CXXRecordDecl* record) {
    for (CXXRecordDecl::field_iterator it = record->field_begin();
         it != record->field_end(); ++it) {
      CXXRecordDecl* record_type =
          it->getTypeSourceInfo()->getTypeLoc().getTypePtr()->
          getAsCXXRecordDecl();
      if (record_type) {
        if (InTestingNamespace(record_type)) {
          return;
        }
      }
    }

    for (CXXRecordDecl::method_iterator it = record->method_begin();
         it != record->method_end(); ++it) {
      if (it->isCopyAssignmentOperator() || isa<CXXConstructorDecl>(*it)) {
        // Ignore constructors and assignment operators.
      } else if (isa<CXXDestructorDecl>(*it) &&
          !record->hasUserDeclaredDestructor()) {
        // Ignore non-user-declared destructors.
      } else {
        CheckVirtualMethod(*it);
        CheckOverriddenMethod(*it);
      }
    }
  }

  void CountType(const Type* type,
                 int* trivial_member,
                 int* non_trivial_member,
                 int* templated_non_trivial_member) {
    switch (type->getTypeClass()) {
      case Type::Record: {
        // Simplifying; the whole class isn't trivial if the dtor is, but
        // we use this as a signal about complexity.
        if (TypeHasNonTrivialDtor(type))
          (*trivial_member)++;
        else
          (*non_trivial_member)++;
        break;
      }
      case Type::TemplateSpecialization: {
        TemplateName name =
            dyn_cast<TemplateSpecializationType>(type)->getTemplateName();
        bool whitelisted_template = false;

        // HACK: I'm at a loss about how to get the syntax checker to get
        // whether a template is exterened or not. For the first pass here,
        // just do retarded string comparisons.
        if (TemplateDecl* decl = name.getAsTemplateDecl()) {
          std::string base_name = decl->getNameAsString();
          if (base_name == "basic_string")
            whitelisted_template = true;
        }

        if (whitelisted_template)
          (*non_trivial_member)++;
        else
          (*templated_non_trivial_member)++;
        break;
      }
      case Type::Elaborated: {
        CountType(
            dyn_cast<ElaboratedType>(type)->getNamedType().getTypePtr(),
            trivial_member, non_trivial_member, templated_non_trivial_member);
        break;
      }
      case Type::Typedef: {
        while (const TypedefType* TT = dyn_cast<TypedefType>(type)) {
          type = TT->getDecl()->getUnderlyingType().getTypePtr();
        }
        CountType(type, trivial_member, non_trivial_member,
                  templated_non_trivial_member);
        break;
      }
      default: {
        // Stupid assumption: anything we see that isn't the above is one of
        // the 20 integer types.
        (*trivial_member)++;
        break;
      }
    }
  }

 private:
  // TODO(avi): Remove this (and all related code) once the override warning is
  // enabled on all bots.
  bool skip_override_;
};

class FindBadConstructsAction : public PluginASTAction {
 public:
  FindBadConstructsAction() : skip_override_(false) {
  }

 protected:
  ASTConsumer* CreateASTConsumer(CompilerInstance &CI, llvm::StringRef ref) {
    return new FindBadConstructsConsumer(CI, skip_override_);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string>& args) {
    bool parsed = true;

    for (size_t i = 0; i < args.size() && parsed; ++i) {
      if (args[i] == "skip-override") {
        skip_override_ = true;
      } else {
        parsed = false;
        llvm::errs() << "Unknown arg: " << args[i] << "\n";
      }
    }

    return parsed;
  }

 private:
  bool skip_override_;
};

}  // namespace

static FrontendPluginRegistry::Add<FindBadConstructsAction>
X("find-bad-constructs", "Finds bad C++ constructs");
